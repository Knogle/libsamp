#include "samp_re/rpc178_logic.h"

int samp_re_decode_rpc178(const uint8_t *input, size_t input_bytes,
                          uint32_t number_of_bits,
                          samp_re_rpc178_payload *out_payload) {
  if (input == NULL || out_payload == NULL || input_bytes < 6u ||
      number_of_bits != 48u) {
    return 0;
  }

  out_payload->actor_id =
      (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8u));
  out_payload->health_bits =
      (uint32_t)input[2] | ((uint32_t)input[3] << 8u) |
      ((uint32_t)input[4] << 16u) | ((uint32_t)input[5] << 24u);
  return 1;
}

int samp_re_actor_pool_offsets(uint16_t actor_id,
                               samp_re_actor_pool_slot_offsets *out_offsets) {
  uint32_t slot_offset;

  if (out_offsets == NULL ||
      (uint32_t)actor_id >= SAMP_RE_ACTOR_POOL_CAPACITY) {
    return 0;
  }

  slot_offset = (uint32_t)actor_id * 4u;
  out_offsets->actor_pointer = SAMP_RE_ACTOR_POOL_ACTOR_BASE + slot_offset;
  out_offsets->active_state = SAMP_RE_ACTOR_POOL_ACTIVE_BASE + slot_offset;
  return 1;
}
