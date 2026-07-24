#include "samp_re/rpc175_logic.h"

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

#define SAMP_RE_FLOAT_ABS_MASK UINT32_C(0x7fffffff)
#define SAMP_RE_FLOAT_EXP_MASK UINT32_C(0x7f800000)
#define SAMP_RE_FLOAT_MANTISSA_MASK UINT32_C(0x007fffff)
#define SAMP_RE_FLOAT_180_BITS UINT32_C(0x43340000)
#define SAMP_RE_FLOAT_360_BITS UINT32_C(0x43b40000)
#define SAMP_RE_FLOAT_PI_BITS UINT32_C(0x40490fdb)
#define SAMP_RE_FLOAT_INVERSE_180_BITS UINT32_C(0x3bb60b61)

static float float_from_bits(uint32_t bits) {
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static uint32_t bits_from_float(float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

int samp_re_decode_rpc175(const uint8_t *input, size_t input_bytes,
                          uint32_t number_of_bits,
                          samp_re_rpc175_payload *out_payload) {
  /*
   * STATIC_037: samp.dll+0x0001D9F0 consumes one 16-bit and one 32-bit
   * field from the bounded BitStream but has no exact-total-length guard.
   */
  if (input == NULL || out_payload == NULL || input_bytes < 6u ||
      number_of_bits < 48u) {
    return 0;
  }

  out_payload->actor_id =
      (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8u));
  out_payload->angle_bits =
      (uint32_t)input[2] | ((uint32_t)input[3] << 8u) |
      ((uint32_t)input[4] << 16u) | ((uint32_t)input[5] << 24u);
  return 1;
}

/*
 * STATIC_037:
 * samp.dll+0x000B5970, R5 SHA256
 * b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
 *
 * OBSERVED_037 + PROBE_TRACE:
 * run=20260723_222459_rpc175_raw_original_c1610b7a established the nine
 * input-to-CPed+0x55C mappings, including 360 -> negative zero.
 *
 * The original keeps ST(0) live through the arithmetic sequence and inherits
 * the active x87 control word.  The i686 path below emits that exact
 * FSUB/FMUL/FMUL/FSUBR/FCHS or FMUL/FMUL sequence without intermediate
 * stores. OBSERVED_037 shows GTA's active precision produces binary32-like
 * checkpoints (135 degrees -> 0x4016CBE5 and 360 -> negative zero). Tests set
 * x87 precision-control to that observed PC24 state explicitly. Bit
 * classification preserves the original unordered-NaN branches without
 * finite-value filtering.
 */
uint32_t samp_re_rpc175_heading_bits(uint32_t angle_bits) {
  uint32_t magnitude = angle_bits & SAMP_RE_FLOAT_ABS_MASK;
  int is_nan =
      (magnitude & SAMP_RE_FLOAT_EXP_MASK) == SAMP_RE_FLOAT_EXP_MASK &&
      (magnitude & SAMP_RE_FLOAT_MANTISSA_MASK) != 0u;
  float angle;
  float pi_binary32;
  float inverse_180_binary32;
  float stored;

  if (!is_nan &&
      (((angle_bits & ~SAMP_RE_FLOAT_ABS_MASK) != 0u &&
        magnitude != 0u) ||
       magnitude > SAMP_RE_FLOAT_360_BITS)) {
    return UINT32_C(0x00000000);
  }

  angle = float_from_bits(angle_bits);
  pi_binary32 = float_from_bits(SAMP_RE_FLOAT_PI_BITS);
  inverse_180_binary32 =
      float_from_bits(SAMP_RE_FLOAT_INVERSE_180_BITS);
#if defined(__GNUC__) && defined(__i386__)
  if (!is_nan && magnitude > SAMP_RE_FLOAT_180_BITS) {
    const float angle_180 = 180.0f;
    __asm__ volatile("flds %[angle]\n\t"
                     "fsubs %[angle_180]\n\t"
                     "fmuls %[pi]\n\t"
                     "fmuls %[inverse_180]\n\t"
                     "fsubrs %[pi]\n\t"
                     "fchs\n\t"
                     "fstps %[stored]"
                     : [stored] "=m"(stored)
                     : [angle] "m"(angle), [angle_180] "m"(angle_180),
                       [pi] "m"(pi_binary32),
                       [inverse_180] "m"(inverse_180_binary32));
  } else {
    __asm__ volatile("flds %[angle]\n\t"
                     "fmuls %[pi]\n\t"
                     "fmuls %[inverse_180]\n\t"
                     "fstps %[stored]"
                     : [stored] "=m"(stored)
                     : [angle] "m"(angle), [pi] "m"(pi_binary32),
                       [inverse_180] "m"(inverse_180_binary32));
  }
#else
  {
    long double result;
    if (!is_nan && magnitude > SAMP_RE_FLOAT_180_BITS) {
      result =
          ((long double)angle - 180.0L) * (long double)pi_binary32;
      result *= (long double)inverse_180_binary32;
      result = -((long double)pi_binary32 - result);
    } else {
      result = (long double)angle * (long double)pi_binary32;
      result *= (long double)inverse_180_binary32;
    }
    stored = (float)result;
  }
#endif
  return bits_from_float(stored);
}
