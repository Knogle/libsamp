#ifndef SAMPDLL_NET_RAKNET_BRIDGE_H
#define SAMPDLL_NET_RAKNET_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 when bridge is compiled with vendored Knogle RakNet. */
int samp_raknet_bridge_is_available(void);

/* Basic allocation/destruction smoke test against RakNetworkFactory. */
int samp_raknet_bridge_self_test(void);

const char *samp_raknet_bridge_variant_name(void);

#ifdef __cplusplus
}
#endif

#endif
