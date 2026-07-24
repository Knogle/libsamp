#include "actor_facing_compat.h"

#include <stdint.h>
#include <stdio.h>

typedef struct actor_facing_case {
  const char *name;
  uint32_t degrees_bits;
  uint32_t expected_radians_bits;
} actor_facing_case;

#if (defined(__GNUC__) || defined(__clang__)) && \
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
#error "RPC175 parity tests require an x86 x87 control word"
#endif

static int assert_bits(const actor_facing_case *test_case) {
  const uint32_t actual = samp_actor_facing_r5_radians_bits(test_case->degrees_bits);
  if (actual == test_case->expected_radians_bits) {
    return 0;
  }
  fprintf(stderr, "FAIL: %s input=%08x expected=%08x actual=%08x\n", test_case->name,
          (unsigned)test_case->degrees_bits, (unsigned)test_case->expected_radians_bits,
          (unsigned)actual);
  return 1;
}

static int assert_finite_classification(void) {
  static const uint32_t finite_bits[] = {
      0x00000000u, 0x80000000u, 0x00000001u, 0x007fffffu,
      0x00800000u, 0x7f7fffffu, 0xff7fffffu,
  };
  static const uint32_t nonfinite_bits[] = {
      0x7f800000u, 0xff800000u, 0x7fc01234u, 0x7f801234u,
  };
  unsigned int i = 0u;
  int failed = 0;

  for (i = 0u; i < sizeof(finite_bits) / sizeof(finite_bits[0]); ++i) {
    if (!samp_actor_facing_binary32_is_finite(finite_bits[i])) {
      fprintf(stderr, "FAIL: finite classification input=%08x\n",
              (unsigned)finite_bits[i]);
      ++failed;
    }
  }
  for (i = 0u; i < sizeof(nonfinite_bits) / sizeof(nonfinite_bits[0]); ++i) {
    if (samp_actor_facing_binary32_is_finite(nonfinite_bits[i])) {
      fprintf(stderr, "FAIL: nonfinite classification input=%08x\n",
              (unsigned)nonfinite_bits[i]);
      ++failed;
    }
  }
  return failed;
}

static int assert_pending_state_transitions(void) {
  samp_actor_facing_pending_state state = {0};
  int failed = 0;

  samp_actor_facing_pending_on_create(&state, 10u, 0x43070000u);
  if (state.create_revision != 10u ||
      state.create_degrees_bits != 0x43070000u ||
      state.target_revision != 10u ||
      state.target_degrees_bits != 0x43070000u ||
      samp_actor_facing_pending_needs_post_create_apply(&state)) {
    fprintf(stderr, "FAIL: RPC171 pending-state initialization\n");
    ++failed;
  }

  samp_actor_facing_pending_on_target(&state, 11u, 0x7fc01234u);
  if (state.create_revision != 10u ||
      state.create_degrees_bits != 0x43070000u ||
      state.target_revision != 11u ||
      state.target_degrees_bits != 0x7fc01234u ||
      !samp_actor_facing_pending_needs_post_create_apply(&state) ||
      !samp_actor_facing_binary32_is_finite(state.create_degrees_bits)) {
    fprintf(stderr, "FAIL: deferred CREATE + qNaN RPC175 separation\n");
    ++failed;
  }

  /*
   * The actor's aggregate state revision may advance for RPC176/RPC178, but
   * authoritative resync must retain the last actual facing revision from the
   * private sidecar and therefore must not synthesize another RPC175 apply.
   */
  samp_actor_facing_pending_on_resync(&state, 21u, 0x43340000u, 21u,
                                      0x43340000u);
  if (state.target_revision != 21u ||
      samp_actor_facing_pending_needs_post_create_apply(&state)) {
    fprintf(stderr, "FAIL: unrelated actor RPC changed facing revision\n");
    ++failed;
  }

  samp_actor_facing_pending_on_resync(&state, 30u, 0x00000000u, 30u,
                                      0x7f800000u);
  if (!samp_actor_facing_pending_needs_post_create_apply(&state)) {
    fprintf(stderr, "FAIL: fallback target bits were not scheduled after create\n");
    ++failed;
  }

  samp_actor_facing_pending_on_resync(&state, 21u, 0x43340000u, 25u,
                                      0x7f800000u);
  if (state.create_revision != 21u ||
      state.create_degrees_bits != 0x43340000u ||
      state.target_revision != 25u ||
      state.target_degrees_bits != 0x7f800000u ||
      !samp_actor_facing_pending_needs_post_create_apply(&state) ||
      !samp_actor_facing_binary32_is_finite(state.create_degrees_bits)) {
    fprintf(stderr, "FAIL: authoritative RPC171/RPC175 sidecar resync\n");
    ++failed;
  }

  return failed;
}

static int assert_pending_target_history(void) {
  static const uint32_t raw_cases[] = {
      0x00000000u, 0x80000000u, 0x43340000u,
      0x43b40000u, 0xc2340000u, 0x44070000u,
      0x7f800000u, 0xff800000u, 0x7fc01234u,
  };
  samp_actor_facing_pending_state state = {0};
  uint32_t consumed = 0u;
  uint32_t i = 0u;
  int failed = 0;

  samp_actor_facing_pending_on_create(&state, 100u, 0x43070000u);
  for (i = 0u; i < sizeof(raw_cases) / sizeof(raw_cases[0]); ++i) {
    samp_actor_facing_pending_on_target(&state, 101u + i, raw_cases[i]);
  }
  if (state.target_event_count != 9u || state.target_revision != 109u ||
      state.target_degrees_bits != 0x7fc01234u ||
      state.create_revision != 100u ||
      state.create_degrees_bits != 0x43070000u) {
    fprintf(stderr, "FAIL: RPC175 burst history aggregate state\n");
    ++failed;
  }

  for (i = 0u; i < sizeof(raw_cases) / sizeof(raw_cases[0]); ++i) {
    uint32_t next_consumed = 0u;
    uint32_t revision = 0u;
    uint32_t degrees_bits = 0u;
    const int result = samp_actor_facing_pending_next_target(
        &state, consumed, &next_consumed, &revision, &degrees_bits);
    if (result != 1 || next_consumed != consumed + 1u ||
        revision != 101u + i || degrees_bits != raw_cases[i]) {
      fprintf(stderr,
              "FAIL: RPC175 burst history index=%u result=%d count=%u "
              "revision=%u bits=%08x\n",
              (unsigned)i, result, (unsigned)next_consumed,
              (unsigned)revision, (unsigned)degrees_bits);
      ++failed;
      break;
    }
    consumed = next_consumed;
  }
  {
    uint32_t next_consumed = 0u;
    uint32_t revision = 0u;
    uint32_t degrees_bits = 0u;
    if (samp_actor_facing_pending_next_target(
            &state, consumed, &next_consumed, &revision,
            &degrees_bits) != 0) {
      fprintf(stderr, "FAIL: RPC175 burst history did not reach tail\n");
      ++failed;
    }
  }

  samp_actor_facing_pending_on_resync(&state, 200u, 0x43340000u, 205u,
                                      0x7f800000u);
  if (state.target_event_count != 0u) {
    fprintf(stderr, "FAIL: authoritative resync retained synthetic history\n");
    ++failed;
  }

  for (i = 0u;
       i < SAMP_ACTOR_FACING_TARGET_HISTORY_CAPACITY + 1u; ++i) {
    samp_actor_facing_pending_on_target(&state, 300u + i, i);
  }
  {
    uint32_t next_consumed = 0u;
    uint32_t revision = 0u;
    uint32_t degrees_bits = 0u;
    if (samp_actor_facing_pending_next_target(
            &state, 0u, &next_consumed, &revision,
            &degrees_bits) != -1) {
      fprintf(stderr, "FAIL: RPC175 burst history overflow not detected\n");
      ++failed;
    }
  }

  return failed;
}

int main(void) {
  static const actor_facing_case cases[] = {
      {"+0", 0x00000000u, 0x00000000u},
      {"-0", 0x80000000u, 0x80000000u},
      {"180", 0x43340000u, 0x40490fdbu},
      {"360", 0x43b40000u, 0x80000000u},
      {"135 (GTA PC24 discriminator)", 0x43070000u, 0x4016cbe5u},
      {"-45", 0xc2340000u, 0x00000000u},
      {"540", 0x44070000u, 0x00000000u},
      {"+inf", 0x7f800000u, 0x00000000u},
      {"-inf", 0xff800000u, 0x00000000u},
      {"qNaN payload", 0x7fc01234u, 0x7fc01234u},
  };
  int failed = 0;
  unsigned int i = 0u;
  const uint16_t original_x87_control =
      begin_observed_gta_x87_precision();

  for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    failed += assert_bits(&cases[i]);
  }
  failed += assert_finite_classification();
  failed += assert_pending_state_transitions();
  failed += assert_pending_target_history();
  restore_x87_control_word(original_x87_control);
  if (failed != 0) {
    fprintf(stderr, "%d actor-facing parity checks failed\n", failed);
    return 1;
  }

  fprintf(stdout, "All RPC175 actor-facing parity checks passed\n");
  return 0;
}
