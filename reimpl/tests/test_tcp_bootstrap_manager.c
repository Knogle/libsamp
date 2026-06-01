#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#endif

#include "sampdll/net/tcp_bootstrap_manager.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

static int assert_true(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

int main(void) {
  int failed = 0;
  samp_socket_t probe = SAMP_INVALID_SOCKET;
  samp_tcp_bootstrap_manager manager;
  samp_socket_t client = SAMP_INVALID_SOCKET;
  samp_socket_t accepted = SAMP_INVALID_SOCKET;

  memset(&manager, 0, sizeof(manager));
  manager.listen_socket = SAMP_INVALID_SOCKET;

  probe = (samp_socket_t)socket(AF_INET, SOCK_STREAM, 0);
  if (probe == SAMP_INVALID_SOCKET) {
    fprintf(stdout, "SKIP: AF_INET stream sockets unavailable in this environment\n");
    return 0;
  }
  samp_net_close_socket(probe);

  failed += assert_true(samp_net_mgr_init(&manager) == 0, "manager init");
  failed += assert_true(manager.initialized == 1, "manager initialized");
  failed += assert_true(samp_net_mgr_uses_raknet(&manager) == (manager.use_raknet_client ? 1 : 0), "raknet use flag");

  if (samp_net_mgr_uses_raknet(&manager)) {
    failed += assert_true(manager.raknet_client != NULL, "raknet client created");
    failed += assert_true(samp_net_mgr_raknet_pump(&manager, 0) == 0, "raknet pump zero");
    failed += assert_true(samp_net_mgr_connect_raknet(&manager, NULL, 7777, 0, 5) != 0, "raknet invalid connect");
    failed += assert_true(samp_net_mgr_raknet_is_connected(&manager) == 0, "raknet not connected by default");
  } else {
    failed += assert_true(manager.raknet_client == NULL, "raknet client absent");
  }

  failed += assert_true(samp_net_mgr_start_listener(&manager, 0, 1) == 0, "start listener");
  failed += assert_true(manager.running == 1, "manager running");
  failed += assert_true(manager.listen_port > 0, "ephemeral listener port assigned");

  failed += assert_true(samp_net_mgr_start_listener(&manager, 0, 1) != 0, "double start denied");

  failed +=
      assert_true(samp_net_mgr_connect(&manager, "127.0.0.1", manager.listen_port, &client) == 0, "connect loopback");
  if (client != SAMP_INVALID_SOCKET && manager.listen_socket != SAMP_INVALID_SOCKET) {
    struct sockaddr_in peer;
#ifdef _WIN32
    int peer_len = (int)sizeof(peer);
#else
    socklen_t peer_len = (socklen_t)sizeof(peer);
#endif
    memset(&peer, 0, sizeof(peer));
    accepted = (samp_socket_t)accept(manager.listen_socket, (struct sockaddr *)&peer, &peer_len);
    failed += assert_true(accepted != SAMP_INVALID_SOCKET, "accept connected client");
  }

  failed += assert_true(samp_net_mgr_stop(&manager) == 0, "manager stop");
  failed += assert_true(manager.running == 0, "manager stopped");
  failed += assert_true(samp_net_mgr_stop(&manager) == 0, "manager stop idempotent");
  failed += assert_true(samp_net_mgr_destroy(&manager) == 0, "manager destroy");
  failed += assert_true(manager.initialized == 0, "manager destroyed");

  if (accepted != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(accepted);
  }
  if (client != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(client);
  }

  if (failed != 0) {
    fprintf(stderr, "%d checks failed\n", failed);
    return 1;
  }
  fprintf(stdout, "All tcp bootstrap manager checks passed\n");
  return 0;
}
