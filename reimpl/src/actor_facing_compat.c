#include "actor_facing_compat.h"

#include <float.h>
#include <string.h>

_Static_assert(sizeof(float) == sizeof(uint32_t),
               "RPC175 requires an IEEE-754 binary32 float");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "RPC175 requires an IEEE-754 binary32 float");
#if !(defined(__GNUC__) && defined(__i386__))
_Static_assert(LDBL_MANT_DIG >= 64,
               "RPC175 fallback requires x87-width intermediates");
#endif

#define SAMP_ACTOR_FACING_SIGN_BIT 0x80000000u
#define SAMP_ACTOR_FACING_EXPONENT_MASK 0x7f800000u
#define SAMP_ACTOR_FACING_FRACTION_MASK 0x007fffffu
#define SAMP_ACTOR_FACING_180_BITS 0x43340000u
#define SAMP_ACTOR_FACING_360_BITS 0x43b40000u
#define SAMP_ACTOR_FACING_PI_BITS 0x40490fdbu
#define SAMP_ACTOR_FACING_INV_180_BITS 0x3bb60b61u

static float actor_facing_float_from_bits(uint32_t bits) {
  float value = 0.0f;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static uint32_t actor_facing_float_bits(const float *value) {
  uint32_t bits = 0u;
  memcpy(&bits, value, sizeof(bits));
  return bits;
}

int samp_actor_facing_binary32_is_finite(uint32_t bits) {
  return (bits & SAMP_ACTOR_FACING_EXPONENT_MASK) !=
         SAMP_ACTOR_FACING_EXPONENT_MASK;
}

void samp_actor_facing_pending_on_create(
    samp_actor_facing_pending_state *state, uint32_t revision,
    uint32_t degrees_bits) {
  if (state == NULL) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->create_revision = revision;
  state->create_degrees_bits = degrees_bits;
  state->target_revision = revision;
  state->target_degrees_bits = degrees_bits;
}

void samp_actor_facing_pending_on_target(
    samp_actor_facing_pending_state *state, uint32_t revision,
    uint32_t degrees_bits) {
  uint32_t event_index = 0u;

  if (state == NULL) {
    return;
  }
  event_index =
      state->target_event_count %
      SAMP_ACTOR_FACING_TARGET_HISTORY_CAPACITY;
  state->target_events[event_index].revision = revision;
  state->target_events[event_index].degrees_bits = degrees_bits;
  ++state->target_event_count;
  state->target_revision = revision;
  state->target_degrees_bits = degrees_bits;
}

void samp_actor_facing_pending_on_resync(
    samp_actor_facing_pending_state *state, uint32_t create_revision,
    uint32_t create_degrees_bits, uint32_t target_revision,
    uint32_t target_degrees_bits) {
  if (state == NULL) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->create_revision = create_revision;
  state->create_degrees_bits = create_degrees_bits;
  state->target_revision = target_revision;
  state->target_degrees_bits = target_degrees_bits;
}

int samp_actor_facing_pending_needs_post_create_apply(
    const samp_actor_facing_pending_state *state) {
  return state != NULL &&
         (state->target_revision != state->create_revision ||
          state->target_degrees_bits != state->create_degrees_bits);
}

int samp_actor_facing_pending_next_target(
    const samp_actor_facing_pending_state *state, uint32_t consumed_count,
    uint32_t *out_consumed_count, uint32_t *out_revision,
    uint32_t *out_degrees_bits) {
  uint32_t available = 0u;
  uint32_t event_index = 0u;
  const samp_actor_facing_target_event *event = NULL;

  if (out_consumed_count != NULL) {
    *out_consumed_count = consumed_count;
  }
  if (out_revision != NULL) {
    *out_revision = 0u;
  }
  if (out_degrees_bits != NULL) {
    *out_degrees_bits = 0u;
  }
  if (state == NULL || out_consumed_count == NULL || out_revision == NULL ||
      out_degrees_bits == NULL) {
    return -1;
  }

  available = state->target_event_count - consumed_count;
  if (available == 0u) {
    return 0;
  }
  if (available > SAMP_ACTOR_FACING_TARGET_HISTORY_CAPACITY) {
    return -1;
  }

  event_index =
      consumed_count % SAMP_ACTOR_FACING_TARGET_HISTORY_CAPACITY;
  event = &state->target_events[event_index];
  *out_consumed_count = consumed_count + 1u;
  *out_revision = event->revision;
  *out_degrees_bits = event->degrees_bits;
  return 1;
}

uint32_t samp_actor_facing_r5_radians_bits(uint32_t degrees_bits) {
  const uint32_t magnitude_bits = degrees_bits & ~SAMP_ACTOR_FACING_SIGN_BIT;
  const int is_nan =
      (magnitude_bits & SAMP_ACTOR_FACING_EXPONENT_MASK) ==
          SAMP_ACTOR_FACING_EXPONENT_MASK &&
      (magnitude_bits & SAMP_ACTOR_FACING_FRACTION_MASK) != 0u;
  float degrees;
  float pi;
  float inverse_180;
  float radians;

  /*
   * OBSERVED_037 + PROBE_TRACE + STATIC_037:
   * samp.dll+0x000b5970, SHA256=
   * b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   * The helper returns literal +0 for infinities and ordered values outside
   * [0, 360]. NaNs take the direct arithmetic path. The golden qNaN keeps
   * every bit; the sNaN quieting/payload result remains TODO_VERIFY.
   */
  if (!is_nan &&
      (((degrees_bits & SAMP_ACTOR_FACING_SIGN_BIT) != 0u &&
        magnitude_bits != 0u) ||
       magnitude_bits > SAMP_ACTOR_FACING_360_BITS)) {
    return 0u;
  }

  /*
   * STATIC_037:
   * The original keeps ST(0) live through the entire sequence and inherits
   * the active x87 control word. The i686 path emits the same register-only
   * operations. OBSERVED_037 shows the GTA runtime uses PC24 here:
   * 135 degrees -> 0x4016cbe5 and 360 degrees -> negative zero.
   */
  degrees = actor_facing_float_from_bits(degrees_bits);
  pi = actor_facing_float_from_bits(SAMP_ACTOR_FACING_PI_BITS);
  inverse_180 =
      actor_facing_float_from_bits(SAMP_ACTOR_FACING_INV_180_BITS);
#if defined(__GNUC__) && defined(__i386__)
  if (!is_nan && magnitude_bits > SAMP_ACTOR_FACING_180_BITS) {
    const float degrees_180 =
        actor_facing_float_from_bits(SAMP_ACTOR_FACING_180_BITS);
    __asm__ volatile("flds %[degrees]\n\t"
                     "fsubs %[degrees_180]\n\t"
                     "fmuls %[pi]\n\t"
                     "fmuls %[inverse_180]\n\t"
                     "fsubrs %[pi]\n\t"
                     "fchs\n\t"
                     "fstps %[radians]"
                     : [radians] "=m"(radians)
                     : [degrees] "m"(degrees),
                       [degrees_180] "m"(degrees_180), [pi] "m"(pi),
                       [inverse_180] "m"(inverse_180));
  } else {
    __asm__ volatile("flds %[degrees]\n\t"
                     "fmuls %[pi]\n\t"
                     "fmuls %[inverse_180]\n\t"
                     "fstps %[radians]"
                     : [radians] "=m"(radians)
                     : [degrees] "m"(degrees), [pi] "m"(pi),
                       [inverse_180] "m"(inverse_180));
  }
#else
  {
    long double result;
    if (!is_nan && magnitude_bits > SAMP_ACTOR_FACING_180_BITS) {
      result = ((long double)degrees - 180.0L) * (long double)pi;
      result *= (long double)inverse_180;
      result = -((long double)pi - result);
    } else {
      result = (long double)degrees * (long double)pi;
      result *= (long double)inverse_180;
    }
    radians = (float)result;
  }
#endif
  return actor_facing_float_bits(&radians);
}
