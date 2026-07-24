#ifndef SAMP_RE_RPC178_LOGIC_H
#define SAMP_RE_RPC178_LOGIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMP_RE_ACTOR_POOL_CAPACITY 1000u
#define SAMP_RE_ACTOR_POOL_ACTOR_BASE 0x0004u
#define SAMP_RE_ACTOR_POOL_ACTIVE_BASE 0x0fa4u

typedef struct samp_re_rpc178_payload {
  uint16_t actor_id;
  uint32_t health_bits;
} samp_re_rpc178_payload;

typedef struct samp_re_actor_pool_slot_offsets {
  uint32_t actor_pointer;
  uint32_t active_state;
} samp_re_actor_pool_slot_offsets;

int samp_re_decode_rpc178(const uint8_t *input, size_t input_bytes,
                          uint32_t number_of_bits,
                          samp_re_rpc178_payload *out_payload);

int samp_re_actor_pool_offsets(uint16_t actor_id,
                               samp_re_actor_pool_slot_offsets *out_offsets);

#ifdef __cplusplus
}
#endif

#endif
