#include "sampdll/net/tcp_bootstrap_manager.h"

#include <string.h>

#include "sampdll/net/ipv4_socket_compat.h"
#include "sampdll/net/raknet_client_adapter.h"

int samp_net_mgr_init(samp_tcp_bootstrap_manager *manager) {
  if (manager == NULL) {
    return -1;
  }

  if (manager->initialized) {
    return 0;
  }

  memset(manager, 0, sizeof(*manager));
  manager->listen_socket = SAMP_INVALID_SOCKET;
  if (samp_net_runtime_init() != 0) {
    return -1;
  }

  if (samp_raknet_client_available()) {
    if (samp_raknet_client_create(&manager->raknet_client) == 0 && manager->raknet_client != NULL) {
      manager->use_raknet_client = 1;
    }
  }

  manager->initialized = 1;
  manager->init_count = 1;
  return 0;
}

int samp_net_mgr_start_listener(samp_tcp_bootstrap_manager *manager, uint16_t port, uint16_t backlog) {
  samp_socket_t socket_fd = SAMP_INVALID_SOCKET;
  uint16_t bound_port = 0;

  if (manager == NULL || !manager->initialized) {
    return -1;
  }
  if (manager->running) {
    return -1;
  }

  if (samp_net_ipv4_tcp_listen(port, backlog, &socket_fd) != 0) {
    return -1;
  }
  if (samp_net_ipv4_getsockname_port(socket_fd, &bound_port) != 0) {
    samp_net_close_socket(socket_fd);
    return -1;
  }

  manager->listen_socket = socket_fd;
  manager->listen_port = bound_port;
  manager->running = 1;
  manager->start_count += 1U;
  return 0;
}

int samp_net_mgr_connect(samp_tcp_bootstrap_manager *manager, const char *host, uint16_t port, samp_socket_t *out_socket) {
  if (manager == NULL || !manager->initialized || out_socket == NULL) {
    return -1;
  }
  return samp_net_ipv4_tcp_connect(host, port, out_socket);
}

int samp_net_mgr_connect_raknet(samp_tcp_bootstrap_manager *manager, const char *host, uint16_t port, uint16_t client_port,
                                int thread_sleep_timer) {
  if (manager == NULL || !manager->initialized || !manager->use_raknet_client || manager->raknet_client == NULL) {
    return -1;
  }

  manager->raknet_client_port = client_port;
  if (samp_raknet_client_connect(manager->raknet_client, host, port, client_port, thread_sleep_timer) != 0) {
    return -1;
  }

  return 0;
}

int samp_net_mgr_raknet_is_connected(const samp_tcp_bootstrap_manager *manager) {
  if (manager == NULL || !manager->initialized || !manager->use_raknet_client || manager->raknet_client == NULL) {
    return 0;
  }
  return samp_raknet_client_is_connected(manager->raknet_client);
}

int samp_net_mgr_raknet_pump(const samp_tcp_bootstrap_manager *manager, int max_packets) {
  if (manager == NULL || !manager->initialized || !manager->use_raknet_client || manager->raknet_client == NULL) {
    return -1;
  }
  return samp_raknet_client_drain_packets(manager->raknet_client, max_packets);
}

int samp_net_mgr_uses_raknet(const samp_tcp_bootstrap_manager *manager) {
  if (manager == NULL || !manager->initialized) {
    return 0;
  }
  return manager->use_raknet_client ? 1 : 0;
}

int samp_net_mgr_stop(samp_tcp_bootstrap_manager *manager) {
  if (manager == NULL || !manager->initialized) {
    return -1;
  }

  if (manager->listen_socket != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(manager->listen_socket);
    manager->listen_socket = SAMP_INVALID_SOCKET;
  }

  if (manager->raknet_client != NULL) {
    samp_raknet_client_disconnect(manager->raknet_client, 0U, 0U);
  }

  manager->running = 0;
  manager->listen_port = 0;
  manager->stop_count += 1U;
  return 0;
}

int samp_net_mgr_destroy(samp_tcp_bootstrap_manager *manager) {
  if (manager == NULL) {
    return -1;
  }
  if (!manager->initialized) {
    return 0;
  }

  (void)samp_net_mgr_stop(manager);
  if (manager->raknet_client != NULL) {
    (void)samp_raknet_client_destroy(manager->raknet_client);
    manager->raknet_client = NULL;
  }
  samp_net_runtime_shutdown();
  memset(manager, 0, sizeof(*manager));
  manager->listen_socket = SAMP_INVALID_SOCKET;
  return 0;
}
