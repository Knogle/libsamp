#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#endif

#include "sampdll/net/dual_stack.h"

#include <stdio.h>
#include <string.h>

/* SPEC-NET-001 and SPEC-NET-002 implementation foundation. */

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#ifdef _WIN32
typedef INT(WSAAPI *samp_getaddrinfo_a_fn)(PCSTR, PCSTR, const ADDRINFOA *, PADDRINFOA *);
typedef VOID(WSAAPI *samp_freeaddrinfo_fn)(PADDRINFOA);

static LONG g_win32_resolver_state = 0;
static HMODULE g_win32_resolver_module = NULL;
static samp_getaddrinfo_a_fn g_win32_getaddrinfo_a = NULL;
static samp_freeaddrinfo_fn g_win32_freeaddrinfo = NULL;

static int ensure_win32_resolver(void) {
  LONG state = InterlockedCompareExchange(&g_win32_resolver_state, 0, 0);
  HMODULE module_handle = NULL;

  if (state == 2) {
    return 0;
  }
  if (state == -1) {
    return -1;
  }

  if (InterlockedCompareExchange(&g_win32_resolver_state, 1, 0) == 0) {
    module_handle = GetModuleHandleA("ws2_32.dll");
    if (module_handle == NULL) {
      module_handle = LoadLibraryA("ws2_32.dll");
    }

    if (module_handle != NULL) {
      g_win32_getaddrinfo_a = (samp_getaddrinfo_a_fn)(uintptr_t)GetProcAddress(module_handle, "getaddrinfo");
      g_win32_freeaddrinfo = (samp_freeaddrinfo_fn)(uintptr_t)GetProcAddress(module_handle, "freeaddrinfo");
    }

    g_win32_resolver_module = module_handle;
    if (g_win32_resolver_module != NULL && g_win32_getaddrinfo_a != NULL && g_win32_freeaddrinfo != NULL) {
      InterlockedExchange(&g_win32_resolver_state, 2);
      return 0;
    }

    g_win32_resolver_module = NULL;
    g_win32_getaddrinfo_a = NULL;
    g_win32_freeaddrinfo = NULL;
    InterlockedExchange(&g_win32_resolver_state, -1);
    return -1;
  }

  do {
    Sleep(0);
    state = InterlockedCompareExchange(&g_win32_resolver_state, 0, 0);
  } while (state == 1);

  return (state == 2) ? 0 : -1;
}

static int samp_getaddrinfo_compat(const char *host, const char *service, const struct addrinfo *hints,
                                   struct addrinfo **results) {
  if (ensure_win32_resolver() != 0 || g_win32_getaddrinfo_a == NULL) {
    return -1;
  }
  return (int)g_win32_getaddrinfo_a(host, service, (const ADDRINFOA *)hints, (PADDRINFOA *)results);
}

static void samp_freeaddrinfo_compat(struct addrinfo *results) {
  if (results == NULL) {
    return;
  }
  if (ensure_win32_resolver() != 0 || g_win32_freeaddrinfo == NULL) {
    return;
  }
  g_win32_freeaddrinfo((PADDRINFOA)results);
}
#else
static int samp_getaddrinfo_compat(const char *host, const char *service, const struct addrinfo *hints,
                                   struct addrinfo **results) {
  return getaddrinfo(host, service, hints, results);
}

static void samp_freeaddrinfo_compat(struct addrinfo *results) {
  if (results == NULL) {
    return;
  }
  freeaddrinfo(results);
}
#endif

static int parse_u16_port(const char *text, uint16_t *out) {
  unsigned long value = 0;
  size_t i = 0;

  if (text == NULL || *text == '\0' || out == NULL) {
    return -1;
  }

  for (i = 0; text[i] != '\0'; ++i) {
    char c = text[i];
    if (c < '0' || c > '9') {
      return -1;
    }
    value = (value * 10UL) + (unsigned long)(c - '0');
    if (value > 65535UL) {
      return -1;
    }
  }

  *out = (uint16_t)value;
  return 0;
}

static int copy_host(char *dst, size_t dst_size, const char *src, size_t src_len) {
  if (dst == NULL || src == NULL || dst_size == 0 || src_len == 0 || src_len >= dst_size) {
    return -1;
  }
  memcpy(dst, src, src_len);
  dst[src_len] = '\0';
  return 0;
}

int samp_net_parse_hostport(const char *input, uint16_t default_port, samp_endpoint *out) {
  const char *port_text = NULL;
  const char *end = NULL;
  const char *single_colon = NULL;
  size_t colon_count = 0;
  size_t i = 0;

  if (input == NULL || out == NULL || *input == '\0') {
    return -1;
  }

  memset(out, 0, sizeof(*out));
  out->port = default_port;

  if (input[0] == '[') {
    end = strchr(input + 1, ']');
    if (end == NULL) {
      return -1;
    }
    if (copy_host(out->host, sizeof(out->host), input + 1, (size_t)(end - (input + 1))) != 0) {
      return -1;
    }
    if (end[1] == '\0') {
      return 0;
    }
    if (end[1] != ':') {
      return -1;
    }
    port_text = end + 2;
    if (*port_text == '\0') {
      return -1;
    }
    return parse_u16_port(port_text, &out->port);
  }

  for (i = 0; input[i] != '\0'; ++i) {
    if (input[i] == ':') {
      ++colon_count;
      single_colon = input + i;
    }
  }

  if (colon_count == 0) {
    if (copy_host(out->host, sizeof(out->host), input, strlen(input)) != 0) {
      return -1;
    }
    return 0;
  }

  if (colon_count == 1 && single_colon != NULL) {
    size_t host_len = (size_t)(single_colon - input);
    port_text = single_colon + 1;
    if (host_len == 0 || *port_text == '\0') {
      return -1;
    }
    if (copy_host(out->host, sizeof(out->host), input, host_len) != 0) {
      return -1;
    }
    return parse_u16_port(port_text, &out->port);
  }

  /* Multiple ':' without brackets is treated as plain IPv6 literal, no explicit port. */
  if (copy_host(out->host, sizeof(out->host), input, strlen(input)) != 0) {
    return -1;
  }
  return 0;
}

int samp_net_runtime_init(void) {
#ifdef _WIN32
  static int initialized = 0;
  WSADATA wsa_data;
  if (!initialized) {
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (rc != 0) {
      return -1;
    }
    initialized = 1;
  }
#endif
  return 0;
}

void samp_net_runtime_shutdown(void) {
#ifdef _WIN32
  WSACleanup();
#endif
}

static int set_nonblocking(samp_socket_t s) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(s, FIONBIO, &mode);
#else
  int flags = fcntl(s, F_GETFL, 0);
  if (flags < 0) {
    return -1;
  }
  return fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int is_wouldblock_error(void) {
#ifdef _WIN32
  int err = WSAGetLastError();
  return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY;
#else
  return errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN;
#endif
}

void samp_net_close_socket(samp_socket_t s) {
  if (s == SAMP_INVALID_SOCKET) {
    return;
  }
#ifdef _WIN32
  closesocket(s);
#else
  close(s);
#endif
}

int samp_net_connect_dual_stack(const char *host, uint16_t port, int timeout_ms, samp_socket_t *out_socket) {
  char port_text[8];
  struct addrinfo hints;
  struct addrinfo *results = NULL;
  struct addrinfo *it = NULL;

  if (host == NULL || *host == '\0' || out_socket == NULL) {
    return -1;
  }
  *out_socket = SAMP_INVALID_SOCKET;

  if (samp_net_runtime_init() != 0) {
    return -1;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  (void)snprintf(port_text, sizeof(port_text), "%u", (unsigned int)port);
  if (samp_getaddrinfo_compat(host, port_text, &hints, &results) != 0) {
    return -1;
  }

  for (it = results; it != NULL; it = it->ai_next) {
    samp_socket_t s = (samp_socket_t)socket(it->ai_family, it->ai_socktype, it->ai_protocol);
    if (s == SAMP_INVALID_SOCKET) {
      continue;
    }

    if (timeout_ms > 0) {
      if (set_nonblocking(s) != 0) {
        samp_net_close_socket(s);
        continue;
      }
    }

    if (connect(s, it->ai_addr, (int)it->ai_addrlen) == 0) {
      *out_socket = s;
      samp_freeaddrinfo_compat(results);
      return 0;
    }

    if (timeout_ms > 0 && is_wouldblock_error()) {
      fd_set wfds;
      struct timeval tv;
      int sel_rc = 0;
      int so_error = 0;
#ifdef _WIN32
      int so_error_len = (int)sizeof(so_error);
#else
      socklen_t so_error_len = (socklen_t)sizeof(so_error);
#endif

      FD_ZERO(&wfds);
      FD_SET(s, &wfds);
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      sel_rc = select((int)(s + 1), NULL, &wfds, NULL, &tv);

      if (sel_rc > 0 && getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_error, &so_error_len) == 0 &&
          so_error == 0) {
        *out_socket = s;
        samp_freeaddrinfo_compat(results);
        return 0;
      }
    }

    samp_net_close_socket(s);
  }

  samp_freeaddrinfo_compat(results);
  return -1;
}
