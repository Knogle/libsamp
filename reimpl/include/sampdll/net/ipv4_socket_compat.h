#ifndef SAMPDLL_NET_IPV4_SOCKET_COMPAT_H
#define SAMPDLL_NET_IPV4_SOCKET_COMPAT_H

#include <stddef.h>
#include <stdint.h>

#include "sampdll/net/dual_stack.h"

/* SPEC-NET-003: IPv4-era socket primitives used by bootstrap flows. */

#define SAMP_NET_IPV4_ADDR_STR_MAX 16U

int samp_net_ipv4_create_udp_bound(uint16_t port, const char *bind_ip, int nonblocking, samp_socket_t *out_socket);

int samp_net_ipv4_sendto(samp_socket_t socket_fd, const void *data, size_t length, uint32_t ipv4_addr_be, uint16_t port,
                         int *out_error);

/*
 * Return values:
 *   1: packet received
 *   0: no packet available (would-block)
 *  -1: hard error
 */
int samp_net_ipv4_recvfrom(samp_socket_t socket_fd, void *buffer, size_t buffer_size, size_t *out_length,
                           uint32_t *out_ipv4_addr_be, uint16_t *out_port);

int samp_net_ipv4_getsockname_port(samp_socket_t socket_fd, uint16_t *out_port);

int samp_net_ipv4_collect_local_addrs(char (*out_addrs)[SAMP_NET_IPV4_ADDR_STR_MAX], size_t max_addrs,
                                      size_t *out_count);

int samp_net_ipv4_resolve_first(const char *host, uint32_t *out_ipv4_addr_be);

int samp_net_ipv4_tcp_connect(const char *host, uint16_t port, samp_socket_t *out_socket);

int samp_net_ipv4_tcp_listen(uint16_t port, uint16_t backlog, samp_socket_t *out_socket);

#endif
