#ifndef SAMPDLL_NET_DUAL_STACK_H
#define SAMPDLL_NET_DUAL_STACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET samp_socket_t;
#define SAMP_INVALID_SOCKET INVALID_SOCKET
#else
typedef int samp_socket_t;
#define SAMP_INVALID_SOCKET (-1)
#endif

typedef struct samp_endpoint {
  char host[256];
  uint16_t port;
} samp_endpoint;

int samp_net_runtime_init(void);
void samp_net_runtime_shutdown(void);

int samp_net_parse_hostport(const char *input, uint16_t default_port, samp_endpoint *out);

int samp_net_connect_dual_stack(const char *host, uint16_t port, int timeout_ms, samp_socket_t *out_socket);

void samp_net_close_socket(samp_socket_t s);

#endif
