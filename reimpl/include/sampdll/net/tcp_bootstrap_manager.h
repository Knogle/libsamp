#ifndef SAMPDLL_NET_TCP_BOOTSTRAP_MANAGER_H
#define SAMPDLL_NET_TCP_BOOTSTRAP_MANAGER_H

#include <stdint.h>

#include "sampdll/net/dual_stack.h"

typedef struct samp_tcp_bootstrap_manager {
  int initialized;
  int running;
  int use_raknet_client;
  uint32_t init_count;
  uint32_t start_count;
  uint32_t stop_count;
  samp_socket_t listen_socket;
  uint16_t listen_port;
  void *raknet_client;
  uint16_t raknet_client_port;
} samp_tcp_bootstrap_manager;

/* SPEC-NET-004 implementation foundation. */
int samp_net_mgr_init(samp_tcp_bootstrap_manager *manager);

int samp_net_mgr_start_listener(samp_tcp_bootstrap_manager *manager, uint16_t port, uint16_t backlog);

int samp_net_mgr_connect(samp_tcp_bootstrap_manager *manager, const char *host, uint16_t port, samp_socket_t *out_socket);

int samp_net_mgr_connect_raknet(samp_tcp_bootstrap_manager *manager, const char *host, uint16_t port, uint16_t client_port,
                                int thread_sleep_timer);

int samp_net_mgr_raknet_is_connected(const samp_tcp_bootstrap_manager *manager);

int samp_net_mgr_raknet_pump(const samp_tcp_bootstrap_manager *manager, int max_packets);

int samp_net_mgr_uses_raknet(const samp_tcp_bootstrap_manager *manager);

int samp_net_mgr_stop(samp_tcp_bootstrap_manager *manager);

int samp_net_mgr_destroy(samp_tcp_bootstrap_manager *manager);

#endif
