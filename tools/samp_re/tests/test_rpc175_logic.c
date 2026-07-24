#include "samp_re/rpc175_logic.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(expression)                                                       \
  do {                                                                          \
    if (!(expression)) {                                                        \
      (void)fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expression,         \
                    __FILE__, __LINE__);                                        \
      return 0;                                                                 \
    }                                                                           \
  } while (0)

#if (defined(__GNUC__) || defined(__clang__)) &&                                \
    (defined(__i386__) || defined(__x86_64__))
static uint16_t begin_observed_gta_x87_precision(void) {
  uint16_t original;
  uint16_t pc24;

  __asm__ volatile("fnstcw %0" : "=m"(original) : : "memory");
  pc24 = (uint16_t)(original & (uint16_t)~UINT16_C(0x0300));
  __asm__ volatile("fldcw %0" : : "m"(pc24) : "memory");
  return original;
}

static void restore_x87_control_word(uint16_t control_word) {
  __asm__ volatile("fldcw %0" : : "m"(control_word) : "memory");
}
#else
#error "RPC175 conversion tests require an x86 x87 control word"
#endif

static const struct rpc175_edge_case {
  uint8_t input[6];
  uint16_t actor_id;
  uint32_t angle_bits;
  uint32_t heading_bits;
} g_rpc175_edge_cases[] = {
    {{0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u}, 0u,
     UINT32_C(0x00000000), UINT32_C(0x00000000)},
    {{0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u}, 0u,
     UINT32_C(0x80000000), UINT32_C(0x80000000)},
    {{0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0x43u}, 0u,
     UINT32_C(0x43340000), UINT32_C(0x40490fdb)},
    {{0x00u, 0x00u, 0x00u, 0x00u, 0xb4u, 0x43u}, 0u,
     UINT32_C(0x43b40000), UINT32_C(0x80000000)},
    {{0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0xc2u}, 0u,
     UINT32_C(0xc2340000), UINT32_C(0x00000000)},
    {{0x00u, 0x00u, 0x00u, 0x00u, 0x07u, 0x44u}, 0u,
     UINT32_C(0x44070000), UINT32_C(0x00000000)},
    {{0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x7fu}, 0u,
     UINT32_C(0x7f800000), UINT32_C(0x00000000)},
    {{0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0xffu}, 0u,
     UINT32_C(0xff800000), UINT32_C(0x00000000)},
    {{0x00u, 0x00u, 0x34u, 0x12u, 0xc0u, 0x7fu}, 0u,
     UINT32_C(0x7fc01234), UINT32_C(0x7fc01234)},
};

static int test_decode_preserves_fixture_raw_bits(void) {
  size_t index;

  for (index = 0u;
       index < sizeof(g_rpc175_edge_cases) / sizeof(g_rpc175_edge_cases[0]);
       ++index) {
    samp_re_rpc175_payload payload = {0};

    CHECK(samp_re_decode_rpc175(g_rpc175_edge_cases[index].input,
                                sizeof(g_rpc175_edge_cases[index].input), 48u,
                                &payload) == 1);
    CHECK(payload.actor_id == g_rpc175_edge_cases[index].actor_id);
    CHECK(payload.angle_bits == g_rpc175_edge_cases[index].angle_bits);
  }
  return 1;
}

static int test_decode_preserves_actor_id(void) {
  static const uint8_t input[6] = {0x34u, 0x12u, 0x00u,
                                   0x00u, 0x80u, 0x3fu};
  samp_re_rpc175_payload payload = {0};

  CHECK(samp_re_decode_rpc175(input, sizeof(input), 48u, &payload) == 1);
  CHECK(payload.actor_id == UINT16_C(0x1234));
  CHECK(payload.angle_bits == UINT32_C(0x3f800000));
  return 1;
}

static int test_conversion_matches_static_r5_x87_sequence(void) {
  size_t index;

  /*
   * OBSERVED_037 + PROBE_TRACE + STATIC_037:
   * run=20260723_222459_rpc175_raw_original_c1610b7a
   * samp.dll+0x000B5970 and constants at +0xE6124/+0xE5940/
   * +0xED468/+0xED47C/+0xED480.
   */
  for (index = 0u;
       index < sizeof(g_rpc175_edge_cases) / sizeof(g_rpc175_edge_cases[0]);
       ++index) {
    CHECK(samp_re_rpc175_heading_bits(
              g_rpc175_edge_cases[index].angle_bits) ==
          g_rpc175_edge_cases[index].heading_bits);
  }
  /*
   * OBSERVED_037 + PROBE_TRACE:
   * The same raw original run recorded 135 degrees three times as
   * 0x43070000 -> 0x4016CBE5. This distinguishes GTA's observed PC24 result
   * from the hypothetical PC53 result 0x4016CBE4.
   */
  CHECK(samp_re_rpc175_heading_bits(UINT32_C(0x43070000)) ==
        UINT32_C(0x4016cbe5));
  return 1;
}

static int test_decode_rejects_truncated_payload(void) {
  static const uint8_t input[6] = {0};
  samp_re_rpc175_payload payload = {UINT16_C(0x1234),
                                    UINT32_C(0x89abcdef)};
  uint32_t bit_count;
  size_t byte_count;

  for (bit_count = 0u; bit_count < 48u; ++bit_count) {
    CHECK(samp_re_decode_rpc175(input, sizeof(input), bit_count, &payload) == 0);
    CHECK(payload.actor_id == UINT16_C(0x1234));
    CHECK(payload.angle_bits == UINT32_C(0x89abcdef));
  }

  for (byte_count = 0u; byte_count < sizeof(input); ++byte_count) {
    CHECK(samp_re_decode_rpc175(input, byte_count, 48u, &payload) == 0);
    CHECK(payload.actor_id == UINT16_C(0x1234));
    CHECK(payload.angle_bits == UINT32_C(0x89abcdef));
  }

  CHECK(samp_re_decode_rpc175(input, sizeof(input), 49u, &payload) == 1);
  CHECK(payload.actor_id == 0u);
  CHECK(payload.angle_bits == 0u);
  CHECK(samp_re_decode_rpc175(input, sizeof(input), UINT32_MAX, &payload) == 1);
  CHECK(payload.actor_id == 0u);
  CHECK(payload.angle_bits == 0u);
  CHECK(samp_re_decode_rpc175(NULL, sizeof(input), 48u, &payload) == 0);
  CHECK(samp_re_decode_rpc175(input, sizeof(input), 48u, NULL) == 0);
  return 1;
}

int main(void) {
  uint16_t original_x87_control = begin_observed_gta_x87_precision();
  int passed = test_decode_preserves_fixture_raw_bits() &&
               test_decode_preserves_actor_id() &&
               test_conversion_matches_static_r5_x87_sequence() &&
               test_decode_rejects_truncated_payload();

  restore_x87_control_word(original_x87_control);
  return passed ? 0 : 1;
}
