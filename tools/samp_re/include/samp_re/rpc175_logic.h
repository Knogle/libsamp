#ifndef SAMP_RE_RPC175_LOGIC_H
#define SAMP_RE_RPC175_LOGIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct samp_re_rpc175_payload {
  uint16_t actor_id;
  uint32_t angle_bits;
} samp_re_rpc175_payload;

int samp_re_decode_rpc175(const uint8_t *input, size_t input_bytes,
                          uint32_t number_of_bits,
                          samp_re_rpc175_payload *out_payload);

uint32_t samp_re_rpc175_heading_bits(uint32_t angle_bits);

#ifdef __cplusplus
}
#endif

#endif
