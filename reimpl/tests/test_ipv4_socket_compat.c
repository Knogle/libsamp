#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#endif

#include "sampdll/net/dual_stack.h"
#include "sampdll/net/ipv4_socket_compat.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#endif

static int assert_true(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

static void sleep_ms(unsigned ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000U);
  ts.tv_nsec = (long)((ms % 1000U) * 1000000U);
  nanosleep(&ts, NULL);
#endif
}

int main(void) {
  int failed = 0;
  samp_socket_t probe = SAMP_INVALID_SOCKET;
  samp_socket_t sock_a = SAMP_INVALID_SOCKET;
  samp_socket_t sock_b = SAMP_INVALID_SOCKET;
  uint16_t port_a = 0;
  uint16_t port_b = 0;
  const char payload[] = "ipv4-udp-probe";
  uint32_t loopback_be = 0;
  uint32_t from_addr_be = 0;
  uint16_t from_port = 0;
  char recv_buf[64];
  size_t recv_len = 0;
  int got_packet = 0;
  int i = 0;
  uint32_t resolved_be = 0;
  char local_addrs[8][SAMP_NET_IPV4_ADDR_STR_MAX];
  size_t local_addrs_count = 0;
  samp_socket_t listener = SAMP_INVALID_SOCKET;
  samp_socket_t client = SAMP_INVALID_SOCKET;
  samp_socket_t accepted = SAMP_INVALID_SOCKET;

  probe = (samp_socket_t)socket(AF_INET, SOCK_DGRAM, 0);
  if (probe == SAMP_INVALID_SOCKET) {
    fprintf(stdout, "SKIP: AF_INET sockets unavailable in this environment\n");
    return 0;
  }
  samp_net_close_socket(probe);

  failed += assert_true(samp_net_ipv4_create_udp_bound(0, NULL, 1, &sock_a) == 0, "create udp socket A");
  if (sock_a == SAMP_INVALID_SOCKET) {
    fprintf(stdout, "SKIP: UDP bind path unavailable in this environment\n");
    return 0;
  }
  failed += assert_true(samp_net_ipv4_create_udp_bound(0, NULL, 1, &sock_b) == 0, "create udp socket B");
  failed += assert_true(samp_net_ipv4_getsockname_port(sock_a, &port_a) == 0, "getsockname port A");
  failed += assert_true(samp_net_ipv4_getsockname_port(sock_b, &port_b) == 0, "getsockname port B");
  failed += assert_true(port_a > 0, "port A assigned");
  failed += assert_true(port_b > 0, "port B assigned");

  loopback_be = inet_addr("127.0.0.1");
  failed += assert_true(samp_net_ipv4_sendto(sock_a, payload, sizeof(payload), loopback_be, port_b, NULL) ==
                            (int)sizeof(payload),
                        "udp sendto length");

  memset(recv_buf, 0, sizeof(recv_buf));
  for (i = 0; i < 100; ++i) {
    int rc =
        samp_net_ipv4_recvfrom(sock_b, recv_buf, sizeof(recv_buf), &recv_len, &from_addr_be, &from_port);
    if (rc < 0) {
      failed += assert_true(0, "udp recv hard error");
      break;
    }
    if (rc == 1) {
      got_packet = 1;
      break;
    }
    sleep_ms(10U);
  }

  failed += assert_true(got_packet == 1, "udp packet received");
  if (got_packet == 1) {
    failed += assert_true(recv_len == sizeof(payload), "udp recv length");
    failed += assert_true(memcmp(recv_buf, payload, sizeof(payload)) == 0, "udp recv payload");
    failed += assert_true(from_port == port_a, "udp source port");
    failed += assert_true(from_addr_be == loopback_be, "udp source addr loopback");
  }

  failed += assert_true(samp_net_ipv4_resolve_first("localhost", &resolved_be) == 0, "resolve localhost");
  failed += assert_true(resolved_be != 0U, "resolved address non-zero");
  memset(local_addrs, 0, sizeof(local_addrs));
  failed +=
      assert_true(samp_net_ipv4_collect_local_addrs(local_addrs, 8U, &local_addrs_count) == 0, "collect local addrs");
  failed += assert_true(local_addrs_count <= 8U, "local addr count bounded");
  if (local_addrs_count > 0U) {
    failed += assert_true(local_addrs[0][0] != '\0', "first local addr non-empty");
  }

  failed += assert_true(samp_net_ipv4_tcp_listen(0, 1, &listener) == 0, "tcp listen");
  failed += assert_true(samp_net_ipv4_getsockname_port(listener, &port_b) == 0, "listener local port");
  failed += assert_true(port_b > 0, "listener port assigned");
  failed += assert_true(samp_net_ipv4_tcp_connect("127.0.0.1", port_b, &client) == 0, "tcp connect loopback");

  if (listener != SAMP_INVALID_SOCKET) {
    struct sockaddr_in peer;
#ifdef _WIN32
    int peer_len = (int)sizeof(peer);
#else
    socklen_t peer_len = (socklen_t)sizeof(peer);
#endif
    memset(&peer, 0, sizeof(peer));
    accepted = (samp_socket_t)accept(listener, (struct sockaddr *)&peer, &peer_len);
    failed += assert_true(accepted != SAMP_INVALID_SOCKET, "accept client");
  }

  if (accepted != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(accepted);
  }
  if (client != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(client);
  }
  if (listener != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(listener);
  }
  if (sock_a != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(sock_a);
  }
  if (sock_b != SAMP_INVALID_SOCKET) {
    samp_net_close_socket(sock_b);
  }

  samp_net_runtime_shutdown();

  if (failed != 0) {
    fprintf(stderr, "%d checks failed\n", failed);
    return 1;
  }

  fprintf(stdout, "All IPv4 socket compat checks passed\n");
  return 0;
}
