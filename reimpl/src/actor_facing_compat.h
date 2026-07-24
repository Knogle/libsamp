#ifndef SAMPDLL_ACTOR_FACING_COMPAT_H
#define SAMPDLL_ACTOR_FACING_COMPAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAMP_ACTOR_FACING_TARGET_HISTORY_CAPACITY 128u

typedef struct samp_actor_facing_target_event {
  uint32_t revision;
  uint32_t degrees_bits;
} samp_actor_facing_target_event;

typedef struct samp_actor_facing_pending_state {
  uint32_t create_revision;
  uint32_t create_degrees_bits;
  uint32_t target_revision;
  uint32_t target_degrees_bits;
  /*
   * The adapter exposes up to 128 actor events per snapshot.  Keep the same
   * bounded history in the launch-monitor -> graphics-thread seqlock so a
   * burst of RPC175 calls cannot collapse to only the final target.
   */
  uint32_t target_event_count;
  samp_actor_facing_target_event
      target_events[SAMP_ACTOR_FACING_TARGET_HISTORY_CAPACITY];
} samp_actor_facing_pending_state;

/*
 * Converts the raw RPC 175 degree bits to the raw CPed+0x55c radian bits
 * produced by the original SA-MP 0.3.7-R5 conversion helper.
 */
uint32_t samp_actor_facing_r5_radians_bits(uint32_t degrees_bits);

/*
 * Integer-only IEEE-754 classification used while a raw RPC 175 value is
 * pending.  Avoiding a float load here preserves signaling-NaN payload bits
 * and does not perturb the x87 status word.
 */
int samp_actor_facing_binary32_is_finite(uint32_t bits);

/*
 * Pure logical state transitions shared by the receive/event/resync paths.
 * RPC171 initializes both values; a later RPC175 changes only the target.
 */
void samp_actor_facing_pending_on_create(
    samp_actor_facing_pending_state *state, uint32_t revision,
    uint32_t degrees_bits);
void samp_actor_facing_pending_on_target(
    samp_actor_facing_pending_state *state, uint32_t revision,
    uint32_t degrees_bits);
void samp_actor_facing_pending_on_resync(
    samp_actor_facing_pending_state *state, uint32_t create_revision,
    uint32_t create_degrees_bits, uint32_t target_revision,
    uint32_t target_degrees_bits);
int samp_actor_facing_pending_needs_post_create_apply(
    const samp_actor_facing_pending_state *state);

/*
 * Returns 1 and the next ordered RPC175 event, 0 when caught up, and -1 when
 * the consumer fell more than one bounded history window behind.
 */
int samp_actor_facing_pending_next_target(
    const samp_actor_facing_pending_state *state, uint32_t consumed_count,
    uint32_t *out_consumed_count, uint32_t *out_revision,
    uint32_t *out_degrees_bits);

#ifdef __cplusplus
}
#endif

#endif
