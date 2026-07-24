#include "samp_re/rpc178_logic.h"

#include <stdio.h>
#include <stdint.h>

#define CHECK(expression)                                                       \
  do {                                                                          \
    if (!(expression)) {                                                        \
      (void)fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expression,         \
                    __FILE__, __LINE__);                                        \
      return 0;                                                                 \
    }                                                                           \
  } while (0)

static int test_decode_preserves_observed_raw_bits(void) {
  /*
   * OBSERVED_037 + PROBE_TRACE:
   * run=20260723_213551_rpc178_edge_original_8ba9d540
   * samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2
   */
  static const struct {
    uint8_t input[6];
    uint16_t actor_id;
    uint32_t health_bits;
  } cases[] = {
      {{0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x7fu}, 0u,
       UINT32_C(0x7f800000)},
      {{0x01u, 0x00u, 0x34u, 0x12u, 0xc0u, 0x7fu}, 1u,
       UINT32_C(0x7fc01234)},
      {{0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u}, 2u,
       UINT32_C(0x00000000)},
      {{0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u}, 3u,
       UINT32_C(0x80000000)},
      {{0x04u, 0x00u, 0x00u, 0x00u, 0xc8u, 0xc1u}, 4u,
       UINT32_C(0xc1c80000)},
  };
  size_t index;

  for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
    samp_re_rpc178_payload payload = {0};

    CHECK(samp_re_decode_rpc178(cases[index].input,
                                sizeof(cases[index].input), 48u,
                                &payload) == 1);
    CHECK(payload.actor_id == cases[index].actor_id);
    CHECK(payload.health_bits == cases[index].health_bits);
  }
  return 1;
}

static int test_decode_rejects_non_exact_payload(void) {
  static const uint8_t input[6] = {0};
  samp_re_rpc178_payload payload = {UINT16_C(0x1234),
                                    UINT32_C(0x89abcdef)};
  uint32_t bit_count;
  size_t byte_count;

  for (bit_count = 0u; bit_count < 48u; ++bit_count) {
    CHECK(samp_re_decode_rpc178(input, sizeof(input), bit_count, &payload) == 0);
    CHECK(payload.actor_id == UINT16_C(0x1234));
    CHECK(payload.health_bits == UINT32_C(0x89abcdef));
  }

  for (byte_count = 0u; byte_count < sizeof(input); ++byte_count) {
    CHECK(samp_re_decode_rpc178(input, byte_count, 48u, &payload) == 0);
    CHECK(payload.actor_id == UINT16_C(0x1234));
    CHECK(payload.health_bits == UINT32_C(0x89abcdef));
  }

  CHECK(samp_re_decode_rpc178(input, sizeof(input), 49u, &payload) == 0);
  CHECK(samp_re_decode_rpc178(NULL, sizeof(input), 48u, &payload) == 0);
  CHECK(samp_re_decode_rpc178(input, sizeof(input), 48u, NULL) == 0);
  return 1;
}

static int test_actor_pool_boundaries(void) {
  samp_re_actor_pool_slot_offsets offsets = {0};

  CHECK(samp_re_actor_pool_offsets(0u, &offsets) == 1);
  CHECK(offsets.actor_pointer == UINT32_C(0x0004));
  CHECK(offsets.active_state == UINT32_C(0x0fa4));

  CHECK(samp_re_actor_pool_offsets(999u, &offsets) == 1);
  CHECK(offsets.actor_pointer == UINT32_C(0x0fa0));
  CHECK(offsets.active_state == UINT32_C(0x1f40));

  CHECK(samp_re_actor_pool_offsets(1000u, &offsets) == 0);
  CHECK(samp_re_actor_pool_offsets(0u, NULL) == 0);
  return 1;
}

int main(void) {
  if (!test_decode_preserves_observed_raw_bits() ||
      !test_decode_rejects_non_exact_payload() ||
      !test_actor_pool_boundaries()) {
    return 1;
  }
  return 0;
}
