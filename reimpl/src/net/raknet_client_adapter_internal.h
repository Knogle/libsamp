#ifndef SAMPDLL_NET_RAKNET_CLIENT_ADAPTER_INTERNAL_H
#define SAMPDLL_NET_RAKNET_CLIENT_ADAPTER_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal authoritative-resync sidecar. RPC 175 replaces the public actor
 * state's current rotation, so the original finite RPC 171 creation heading
 * and the last actual facing revision must be retained separately without
 * changing any public snapshot layout.
 */
int samp_raknet_client_get_actor_create_rotation_bits(
    void *client, uint16_t actor_id, uint32_t *out_create_revision,
    uint32_t *out_rotation_bits, uint32_t *out_facing_revision);

#ifdef __cplusplus
}
#endif

#endif
