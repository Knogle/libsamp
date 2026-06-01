#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#endif

#include "sampdll/net/legacy_ipv4_compat.h"

#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static int last_socket_error(void) {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

static int is_wouldblock_error(int err) {
#ifdef _WIN32
  return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY;
#else
  return err == EWOULDBLOCK || err == EAGAIN || err == EINPROGRESS;
#endif
}

static int set_nonblocking_socket(samp_socket_t socket_fd, int enabled) {
#ifdef _WIN32
  u_long mode = enabled ? 1UL : 0UL;
  return ioctlsocket(socket_fd, FIONBIO, &mode);
#else
  int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags < 0) {
    return -1;
  }
  if (enabled) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }
  return fcntl(socket_fd, F_SETFL, flags);
#endif
}

int samp_net_ipv4_create_udp_bound(uint16_t port, const char *bind_ip, int nonblocking, samp_socket_t *out_socket) {
  struct sockaddr_in addr;
  samp_socket_t socket_fd = SAMP_INVALID_SOCKET;
  int one = 1;
  int rcvbuf = 0x40000;
  int sndbuf = 0x4000;

  if (out_socket == NULL) {
    return -1;
  }
  *out_socket = SAMP_INVALID_SOCKET;

  if (samp_net_runtime_init() != 0) {
    return -1;
  }

  socket_fd = (samp_socket_t)socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd == SAMP_INVALID_SOCKET) {
    return -1;
  }

  /* Legacy behavior mirrors best-effort setsockopt tuning from mapped flow. */
  (void)setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, (int)sizeof(one));
  (void)setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, (int)sizeof(rcvbuf));
  (void)setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbuf, (int)sizeof(sndbuf));
  (void)setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, (const char *)&one, (int)sizeof(one));

  if (nonblocking) {
    if (set_nonblocking_socket(socket_fd, 1) != 0) {
      samp_net_close_socket(socket_fd);
      return -1;
    }
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind_ip != NULL && bind_ip[0] != '\0') {
    addr.sin_addr.s_addr = inet_addr(bind_ip);
  }

  if (bind(socket_fd, (const struct sockaddr *)&addr, (int)sizeof(addr)) != 0) {
    samp_net_close_socket(socket_fd);
    return -1;
  }

  *out_socket = socket_fd;
  return 0;
}

int samp_net_ipv4_sendto(samp_socket_t socket_fd, const void *data, size_t length, uint32_t ipv4_addr_be, uint16_t port,
                         int *out_error) {
  struct sockaddr_in addr;
  int rc = -1;
  int attempts = 0;

  if (socket_fd == SAMP_INVALID_SOCKET || data == NULL || length == 0 || length > (size_t)INT32_MAX) {
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ipv4_addr_be;
  addr.sin_port = htons(port);

  do {
    rc = (int)sendto(socket_fd, (const char *)data, (int)length, 0, (const struct sockaddr *)&addr, (int)sizeof(addr));
    ++attempts;
  } while (rc == 0 && attempts < 16);

  if (rc < 0 && out_error != NULL) {
    *out_error = last_socket_error();
  }
  return rc;
}

int samp_net_ipv4_recvfrom(samp_socket_t socket_fd, void *buffer, size_t buffer_size, size_t *out_length,
                           uint32_t *out_ipv4_addr_be, uint16_t *out_port) {
  struct sockaddr_in from;
#ifdef _WIN32
  int from_len = (int)sizeof(from);
#else
  socklen_t from_len = (socklen_t)sizeof(from);
#endif
  int rc = -1;

  if (socket_fd == SAMP_INVALID_SOCKET || buffer == NULL || buffer_size == 0 || out_length == NULL) {
    return -1;
  }

  memset(&from, 0, sizeof(from));
  rc = (int)recvfrom(socket_fd, (char *)buffer, (int)buffer_size, 0, (struct sockaddr *)&from, &from_len);
  if (rc < 0) {
    if (is_wouldblock_error(last_socket_error())) {
      *out_length = 0;
      return 0;
    }
    *out_length = 0;
    return -1;
  }

  *out_length = (size_t)rc;
  if (out_ipv4_addr_be != NULL) {
    *out_ipv4_addr_be = from.sin_addr.s_addr;
  }
  if (out_port != NULL) {
    *out_port = ntohs(from.sin_port);
  }
  return 1;
}

int samp_net_ipv4_getsockname_port(samp_socket_t socket_fd, uint16_t *out_port) {
  struct sockaddr_in addr;
#ifdef _WIN32
  int addr_len = (int)sizeof(addr);
#else
  socklen_t addr_len = (socklen_t)sizeof(addr);
#endif

  if (socket_fd == SAMP_INVALID_SOCKET || out_port == NULL) {
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  if (getsockname(socket_fd, (struct sockaddr *)&addr, &addr_len) != 0) {
    return -1;
  }
  *out_port = ntohs(addr.sin_port);
  return 0;
}

int samp_net_ipv4_collect_local_addrs(char (*out_addrs)[SAMP_NET_IPV4_ADDR_STR_MAX], size_t max_addrs,
                                      size_t *out_count) {
  char host_name[80];
  struct hostent *ent = NULL;
  size_t count = 0;

  if (out_count == NULL) {
    return -1;
  }
  *out_count = 0;

  if (max_addrs > 0U && out_addrs == NULL) {
    return -1;
  }

  memset(host_name, 0, sizeof(host_name));
  if (gethostname(host_name, (int)sizeof(host_name)) != 0) {
    return -1;
  }

  ent = gethostbyname(host_name);
  if (ent == NULL || ent->h_addr_list == NULL) {
    return -1;
  }

  while (ent->h_addr_list[count] != NULL && count < max_addrs && count < 40U) {
    struct in_addr addr;
    const char *as_text = NULL;
    memcpy(&addr, ent->h_addr_list[count], sizeof(addr));
    as_text = inet_ntoa(addr);
    if (as_text != NULL) {
      strncpy(out_addrs[count], as_text, SAMP_NET_IPV4_ADDR_STR_MAX - 1U);
      out_addrs[count][SAMP_NET_IPV4_ADDR_STR_MAX - 1U] = '\0';
    } else {
      out_addrs[count][0] = '\0';
    }
    ++count;
  }

  *out_count = count;
  return 0;
}

int samp_net_ipv4_resolve_first(const char *host, uint32_t *out_ipv4_addr_be) {
  struct hostent *ent = NULL;

  if (host == NULL || host[0] == '\0' || out_ipv4_addr_be == NULL) {
    return -1;
  }

  ent = gethostbyname(host);
  if (ent == NULL || ent->h_addr_list == NULL || ent->h_addr_list[0] == NULL) {
    return -1;
  }

  memcpy(out_ipv4_addr_be, ent->h_addr_list[0], sizeof(*out_ipv4_addr_be));
  return 0;
}

int samp_net_ipv4_tcp_connect(const char *host, uint16_t port, samp_socket_t *out_socket) {
  struct sockaddr_in addr;
  samp_socket_t socket_fd = SAMP_INVALID_SOCKET;
  uint32_t ip_be = INADDR_NONE;

  if (host == NULL || host[0] == '\0' || out_socket == NULL) {
    return -1;
  }
  *out_socket = SAMP_INVALID_SOCKET;

  if (samp_net_runtime_init() != 0) {
    return -1;
  }

  ip_be = inet_addr(host);
  if (ip_be == INADDR_NONE) {
    if (samp_net_ipv4_resolve_first(host, &ip_be) != 0) {
      return -1;
    }
  }

  socket_fd = (samp_socket_t)socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == SAMP_INVALID_SOCKET) {
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = ip_be;

  if (connect(socket_fd, (const struct sockaddr *)&addr, (int)sizeof(addr)) != 0) {
    samp_net_close_socket(socket_fd);
    return -1;
  }

  *out_socket = socket_fd;
  return 0;
}

int samp_net_ipv4_tcp_listen(uint16_t port, uint16_t backlog, samp_socket_t *out_socket) {
  struct sockaddr_in addr;
  samp_socket_t socket_fd = SAMP_INVALID_SOCKET;
  int one = 1;
  int listen_backlog = (backlog == 0U) ? 1 : (int)backlog;

  if (out_socket == NULL) {
    return -1;
  }
  *out_socket = SAMP_INVALID_SOCKET;

  if (samp_net_runtime_init() != 0) {
    return -1;
  }

  socket_fd = (samp_socket_t)socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == SAMP_INVALID_SOCKET) {
    return -1;
  }

  (void)setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, (int)sizeof(one));

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(socket_fd, (const struct sockaddr *)&addr, (int)sizeof(addr)) != 0) {
    samp_net_close_socket(socket_fd);
    return -1;
  }
  if (listen(socket_fd, listen_backlog) != 0) {
    samp_net_close_socket(socket_fd);
    return -1;
  }

  *out_socket = socket_fd;
  return 0;
}
