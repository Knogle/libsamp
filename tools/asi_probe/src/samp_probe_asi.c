#include <windows.h>
#include <winsock.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROBE_LOG_NAME "samp_probe.log"
#define PROBE_NO_HOOKS_FLAG "samp_probe_no_hooks.flag"
#define PROBE_MAX_IMPORT_LOGS 4096
#define PROBE_WATCH_INTERVAL_MS 250
#define PROBE_WAIT_FOR_SAMP_MS 30000
#define PROBE_INLINE_HOOK_MAX_COPY 16
#define PROBE_PAYLOAD_PREVIEW_BYTES 256
#define PROBE_STATE_HEARTBEAT_MS 5000

#if defined(__GNUC__)
#define PROBE_ALWAYS_INLINE static __inline__ __attribute__((always_inline))
#else
#define PROBE_ALWAYS_INLINE static __inline
#endif

typedef int(WINAPI *probe_WSAStartup_fn)(WORD, LPWSADATA);
typedef int(WINAPI *probe_WSACleanup_fn)(void);
typedef SOCKET(WINAPI *probe_socket_fn)(int, int, int);
typedef int(WINAPI *probe_bind_fn)(SOCKET, const struct sockaddr *, int);
typedef int(WINAPI *probe_connect_fn)(SOCKET, const struct sockaddr *, int);
typedef int(WINAPI *probe_closesocket_fn)(SOCKET);
typedef struct hostent *(WINAPI *probe_gethostbyname_fn)(const char *);
typedef int(WINAPI *probe_gethostname_fn)(char *, int);
typedef unsigned long(WINAPI *probe_inet_addr_fn)(const char *);
typedef char *(WINAPI *probe_inet_ntoa_fn)(struct in_addr);
typedef int(WINAPI *probe_send_fn)(SOCKET, const char *, int, int);
typedef int(WINAPI *probe_recv_fn)(SOCKET, char *, int, int);
typedef int(WINAPI *probe_sendto_fn)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int(WINAPI *probe_recvfrom_fn)(SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int(WINAPI *probe_select_fn)(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
typedef int(WINAPI *probe_WSAGetLastError_fn)(void);
typedef BOOL(WINAPI *probe_VirtualProtect_fn)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef HMODULE(WINAPI *probe_LoadLibraryA_fn)(LPCSTR);
typedef HMODULE(WINAPI *probe_LoadLibraryW_fn)(LPCWSTR);
typedef FARPROC(WINAPI *probe_GetProcAddress_fn)(HMODULE, LPCSTR);
typedef HANDLE(WINAPI *probe_CreateThread_fn)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef int(WINAPI *probe_samp_socketlayer_sendto_fn)(SOCKET, const char *, int, unsigned long, unsigned short);
typedef void(WINAPI *probe_samp_process_network_packet_fn)(unsigned long, unsigned short, const char *, int, void *);

typedef struct probe_iat_hook {
  const char *dll_name;
  const char *symbol_name;
  WORD ordinal;
  void *replacement;
  void **original_slot;
  LONG install_count;
} probe_iat_hook;

typedef struct probe_watchpoint {
  const char *name;
  uintptr_t address;
  size_t size;
  BYTE previous[16];
  int has_previous;
} probe_watchpoint;

typedef struct probe_transition_state {
  DWORD entry_gate;
  DWORD hwnd;
  DWORD id3d9;
  DWORD id3d9_device;
  DWORD game_process_storage;
  DWORD render2d_storage;
  DWORD ped_table;
  DWORD vehicle_table;
  DWORD script_call_target;
  DWORD graphics_call_target;
  DWORD samp_base;
  DWORD samp_size;
  BYTE start_game;
  BYTE game_started;
  BYTE menu;
  BYTE menu2;
  BYTE menu3;
  BYTE hud;
  BYTE time_patch;
  BYTE script_gate0;
  BYTE script_gate1;
  BYTE script_call_opcode;
  BYTE graphics_call_opcode;
  BYTE current_player;
  BYTE camera_mode;
  WORD camera_mode2;
} probe_transition_state;

typedef struct probe_inline_hook {
  const char *dll_name;
  const char *symbol_name;
  void *replacement;
  void **original_slot;
  void *target;
  void *patch_target;
  void *trampoline;
  BYTE saved[PROBE_INLINE_HOOK_MAX_COPY];
  size_t saved_len;
  LONG installed;
} probe_inline_hook;

typedef struct probe_code_hook {
  const char *name;
  DWORD rva;
  void *replacement;
  void **original_slot;
  BYTE expected[8];
  size_t expected_len;
  void *patch_target;
  void *trampoline;
  BYTE saved[PROBE_INLINE_HOOK_MAX_COPY];
  size_t saved_len;
  LONG installed;
} probe_code_hook;

static HMODULE g_self_module;
static HANDLE g_worker_thread;
static HANDLE g_stop_event;
static CRITICAL_SECTION g_log_lock;
static LONG g_log_ready;
static char g_log_path[MAX_PATH];
static HMODULE g_samp_module;
static uintptr_t g_samp_base;
static DWORD g_samp_size;
static LONG g_import_log_count;
static PVOID g_exception_handler;
static LONG g_exception_logged;
static probe_transition_state g_previous_state;
static LONG g_previous_state_valid;
static DWORD g_previous_state_log_ms;

static void *g_orig_WSAStartup;
static void *g_orig_WSACleanup;
static void *g_orig_socket;
static void *g_orig_bind;
static void *g_orig_connect;
static void *g_orig_closesocket;
static void *g_orig_gethostbyname;
static void *g_orig_gethostname;
static void *g_orig_inet_addr;
static void *g_orig_inet_ntoa;
static void *g_orig_send;
static void *g_orig_recv;
static void *g_orig_sendto;
static void *g_orig_recvfrom;
static void *g_orig_select;
static void *g_orig_VirtualProtect;
static void *g_orig_LoadLibraryA;
static void *g_orig_LoadLibraryW;
static void *g_orig_GetProcAddress;
static void *g_orig_CreateThread;
static void *g_orig_samp_socketlayer_sendto;
static void *g_orig_samp_process_network_packet;
static probe_WSAGetLastError_fn g_WSAGetLastError_ptr;

static LONG CALLBACK probe_exception_handler(PEXCEPTION_POINTERS info);
static int WINAPI hook_WSAStartup(WORD version, LPWSADATA data);
static int WINAPI hook_WSACleanup(void);
static SOCKET WINAPI hook_socket(int af, int type, int protocol);
static int WINAPI hook_bind(SOCKET s, const struct sockaddr *name, int namelen);
static int WINAPI hook_connect(SOCKET s, const struct sockaddr *name, int namelen);
static int WINAPI hook_closesocket(SOCKET s);
static struct hostent *WINAPI hook_gethostbyname(const char *name);
static int WINAPI hook_gethostname(char *name, int namelen);
static unsigned long WINAPI hook_inet_addr(const char *cp);
static char *WINAPI hook_inet_ntoa(struct in_addr in);
static int WINAPI hook_send(SOCKET s, const char *buf, int len, int flags);
static int WINAPI hook_recv(SOCKET s, char *buf, int len, int flags);
static int WINAPI hook_sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
static int WINAPI hook_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
static int WINAPI hook_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout);
static BOOL WINAPI hook_VirtualProtect(LPVOID address, SIZE_T size, DWORD new_protect, PDWORD old_protect);
static HMODULE WINAPI hook_LoadLibraryA(LPCSTR file_name);
static HMODULE WINAPI hook_LoadLibraryW(LPCWSTR file_name);
static FARPROC WINAPI hook_GetProcAddress(HMODULE module, LPCSTR proc_name);
static HANDLE WINAPI hook_CreateThread(LPSECURITY_ATTRIBUTES attr, SIZE_T stack_size, LPTHREAD_START_ROUTINE start, LPVOID param,
                                       DWORD flags, LPDWORD thread_id);
static int WINAPI hook_samp_socketlayer_sendto(SOCKET s, const char *buf, int len, unsigned long binary_address,
                                               unsigned short port);
static void WINAPI hook_samp_process_network_packet(unsigned long binary_address, unsigned short port, const char *data, int len,
                                                    void *rak_peer);

static probe_iat_hook g_hooks[] = {
    {"WSOCK32.dll", "WSAStartup", 115, (void *)hook_WSAStartup, &g_orig_WSAStartup, 0},
    {"WSOCK32.dll", "WSACleanup", 116, (void *)hook_WSACleanup, &g_orig_WSACleanup, 0},
    {"WSOCK32.dll", "socket", 23, (void *)hook_socket, &g_orig_socket, 0},
    {"WSOCK32.dll", "bind", 2, (void *)hook_bind, &g_orig_bind, 0},
    {"WSOCK32.dll", "connect", 4, (void *)hook_connect, &g_orig_connect, 0},
    {"WSOCK32.dll", "closesocket", 3, (void *)hook_closesocket, &g_orig_closesocket, 0},
    {"WSOCK32.dll", "gethostbyname", 52, (void *)hook_gethostbyname, &g_orig_gethostbyname, 0},
    {"WSOCK32.dll", "gethostname", 57, (void *)hook_gethostname, &g_orig_gethostname, 0},
    {"WSOCK32.dll", "inet_addr", 10, (void *)hook_inet_addr, &g_orig_inet_addr, 0},
    {"WSOCK32.dll", "inet_ntoa", 11, (void *)hook_inet_ntoa, &g_orig_inet_ntoa, 0},
    {"WSOCK32.dll", "send", 19, (void *)hook_send, &g_orig_send, 0},
    {"WSOCK32.dll", "recv", 16, (void *)hook_recv, &g_orig_recv, 0},
    {"WSOCK32.dll", "sendto", 20, (void *)hook_sendto, &g_orig_sendto, 0},
    {"WSOCK32.dll", "recvfrom", 17, (void *)hook_recvfrom, &g_orig_recvfrom, 0},
    {"WSOCK32.dll", "select", 18, (void *)hook_select, &g_orig_select, 0},
    {"WS2_32.dll", "WSAStartup", 0, (void *)hook_WSAStartup, &g_orig_WSAStartup, 0},
    {"WS2_32.dll", "WSACleanup", 0, (void *)hook_WSACleanup, &g_orig_WSACleanup, 0},
    {"WS2_32.dll", "socket", 0, (void *)hook_socket, &g_orig_socket, 0},
    {"WS2_32.dll", "bind", 0, (void *)hook_bind, &g_orig_bind, 0},
    {"WS2_32.dll", "connect", 0, (void *)hook_connect, &g_orig_connect, 0},
    {"WS2_32.dll", "closesocket", 0, (void *)hook_closesocket, &g_orig_closesocket, 0},
    {"WS2_32.dll", "gethostbyname", 0, (void *)hook_gethostbyname, &g_orig_gethostbyname, 0},
    {"WS2_32.dll", "gethostname", 0, (void *)hook_gethostname, &g_orig_gethostname, 0},
    {"WS2_32.dll", "inet_addr", 0, (void *)hook_inet_addr, &g_orig_inet_addr, 0},
    {"WS2_32.dll", "inet_ntoa", 0, (void *)hook_inet_ntoa, &g_orig_inet_ntoa, 0},
    {"WS2_32.dll", "send", 0, (void *)hook_send, &g_orig_send, 0},
    {"WS2_32.dll", "recv", 0, (void *)hook_recv, &g_orig_recv, 0},
    {"WS2_32.dll", "sendto", 0, (void *)hook_sendto, &g_orig_sendto, 0},
    {"WS2_32.dll", "recvfrom", 0, (void *)hook_recvfrom, &g_orig_recvfrom, 0},
    {"WS2_32.dll", "select", 0, (void *)hook_select, &g_orig_select, 0},
    {"KERNEL32.dll", "VirtualProtect", 0, (void *)hook_VirtualProtect, &g_orig_VirtualProtect, 0},
    {"KERNEL32.dll", "LoadLibraryA", 0, (void *)hook_LoadLibraryA, &g_orig_LoadLibraryA, 0},
    {"KERNEL32.dll", "LoadLibraryW", 0, (void *)hook_LoadLibraryW, &g_orig_LoadLibraryW, 0},
    {"KERNEL32.dll", "GetProcAddress", 0, (void *)hook_GetProcAddress, &g_orig_GetProcAddress, 0},
    {"KERNEL32.dll", "CreateThread", 0, (void *)hook_CreateThread, &g_orig_CreateThread, 0},
};

static probe_watchpoint g_watchpoints[] = {
    {"gta_entry_gate", 0x00C8D4C0u, 4, {0}, 0},
    {"start_game_flag", 0x00BA677Bu, 1, {0}, 0},
    {"game_started_flag", 0x00BA6831u, 1, {0}, 0},
    {"menu_flag", 0x00BA67A4u, 1, {0}, 0},
    {"menu2_flag", 0x00BA67A5u, 1, {0}, 0},
    {"menu3_flag", 0x00BA67A6u, 1, {0}, 0},
    {"hud_flag", 0x00BA6769u, 1, {0}, 0},
    {"time_pass_patch", 0x0052CF10u, 1, {0}, 0},
    {"hwnd_ptr", 0x00C97C1Cu, 4, {0}, 0},
    {"id3d9_ptr", 0x00C97C20u, 4, {0}, 0},
    {"id3d9device_ptr", 0x00C97C28u, 4, {0}, 0},
    {"script_process_gate", 0x00469EF5u, 2, {0}, 0},
    {"script_process_call", 0x0053BFC7u, 5, {0}, 0},
    {"graphics_loop_call", 0x0053EB12u, 5, {0}, 0},
    {"game_process_hook_site", 0x0058C246u, 8, {0}, 0},
    {"game_process_hook_storage", 0x0053BED1u, 4, {0}, 0},
    {"render2dstuff_storage", 0x0053E22Cu, 4, {0}, 0},
    {"ped_table_ptr", 0x00B74490u, 4, {0}, 0},
    {"vehicle_table_ptr", 0x00B74494u, 4, {0}, 0},
    {"current_player", 0x00B7CD74u, 1, {0}, 0},
    {"camera_mode", 0x00B6F1A8u, 1, {0}, 0},
    {"camera_mode2", 0x00B6F858u, 2, {0}, 0},
    {"bypass_vids_usa10", 0x00747483u, 6, {0}, 0},
    {"bypass_vids_eu10", 0x007474D3u, 6, {0}, 0},
};

static probe_inline_hook g_inline_hooks[] = {
    {"WSOCK32.dll", "WSAStartup", (void *)hook_WSAStartup, &g_orig_WSAStartup, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "WSACleanup", (void *)hook_WSACleanup, &g_orig_WSACleanup, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "socket", (void *)hook_socket, &g_orig_socket, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "bind", (void *)hook_bind, &g_orig_bind, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "connect", (void *)hook_connect, &g_orig_connect, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "closesocket", (void *)hook_closesocket, &g_orig_closesocket, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "gethostbyname", (void *)hook_gethostbyname, &g_orig_gethostbyname, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "gethostname", (void *)hook_gethostname, &g_orig_gethostname, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "inet_addr", (void *)hook_inet_addr, &g_orig_inet_addr, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "inet_ntoa", (void *)hook_inet_ntoa, &g_orig_inet_ntoa, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "send", (void *)hook_send, &g_orig_send, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "recv", (void *)hook_recv, &g_orig_recv, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "sendto", (void *)hook_sendto, &g_orig_sendto, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "recvfrom", (void *)hook_recvfrom, &g_orig_recvfrom, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "select", (void *)hook_select, &g_orig_select, NULL, NULL, NULL, {0}, 0, 0},
};

static probe_code_hook g_samp_code_hooks[] = {
    {"samp.SocketLayer.SendTo", 0x0004ffc0u, (void *)hook_samp_socketlayer_sendto, &g_orig_samp_socketlayer_sendto,
     {0x83, 0xec, 0x10, 0x57}, 4, NULL, NULL, {0}, 0, 0},
    {"samp.ProcessNetworkPacket", 0x0003b950u, (void *)hook_samp_process_network_packet, &g_orig_samp_process_network_packet,
     {0x6a, 0xff, 0x68}, 3, NULL, NULL, {0}, 0, 0},
};

static void probe_log(const char *fmt, ...) {
  char payload[2048];
  char line[2300];
  DWORD now_ms;
  FILE *fp;
  va_list args;

  if (InterlockedCompareExchange(&g_log_ready, 0, 0) == 0 || fmt == NULL) {
    return;
  }

  va_start(args, fmt);
  vsnprintf(payload, sizeof(payload), fmt, args);
  va_end(args);

  now_ms = GetTickCount();
  snprintf(line, sizeof(line), "[%10lu] %s\r\n", (unsigned long)now_ms, payload);

  EnterCriticalSection(&g_log_lock);
  fp = fopen(g_log_path, "ab");
  if (fp != NULL) {
    fputs(line, fp);
    fclose(fp);
  }
  LeaveCriticalSection(&g_log_lock);

  OutputDebugStringA("[samp_probe] ");
  OutputDebugStringA(payload);
  OutputDebugStringA("\n");
}

static int should_log_call(LONG count) {
  return count <= 64 || (count & 0xff) == 0;
}

static LONG next_call_count(LONG *counter) {
  return InterlockedIncrement(counter);
}

static int env_flag_enabled(const char *name) {
  char value[16];
  DWORD len;

  if (name == NULL) {
    return 0;
  }

  len = GetEnvironmentVariableA(name, value, sizeof(value));
  if (len == 0 || len >= sizeof(value)) {
    return 0;
  }

  return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
}

PROBE_ALWAYS_INLINE void *probe_return_address(void) {
  return __builtin_return_address(0);
}

static int address_in_samp(void *address) {
  uintptr_t value = (uintptr_t)address;
  if (g_samp_base == 0 || g_samp_size == 0 || value == 0) {
    return 0;
  }
  return value >= g_samp_base && value < (g_samp_base + (uintptr_t)g_samp_size);
}

static int should_log_api_from_caller(void *caller) {
  return address_in_samp(caller) || env_flag_enabled("SAMP_PROBE_LOG_ALL_API");
}

static unsigned long samp_rva_from_address(void *address) {
  if (!address_in_samp(address)) {
    return 0;
  }
  return (unsigned long)((uintptr_t)address - g_samp_base);
}

static void path_dirname(char *path) {
  size_t len;

  if (path == NULL) {
    return;
  }

  len = strlen(path);
  while (len > 0) {
    char ch = path[len - 1];
    if (ch == '\\' || ch == '/') {
      path[len - 1] = '\0';
      return;
    }
    path[len - 1] = '\0';
    --len;
  }
}

static void join_path(char *out, size_t out_size, const char *dir, const char *file) {
  size_t len;

  if (out == NULL || out_size == 0) {
    return;
  }

  if (dir == NULL || *dir == '\0') {
    snprintf(out, out_size, "%s", file);
    return;
  }

  len = strlen(dir);
  snprintf(out, out_size, "%s%s%s", dir, (len > 0 && dir[len - 1] == '\\') ? "" : "\\", file);
}

static int file_exists(const char *path) {
  DWORD attr;

  if (path == NULL || *path == '\0') {
    return 0;
  }

  attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void init_log_path(void) {
  char module_path[MAX_PATH];
  char module_dir[MAX_PATH];

  module_path[0] = '\0';
  module_dir[0] = '\0';
  g_log_path[0] = '\0';

  if (GetModuleFileNameA(g_self_module, module_path, sizeof(module_path)) > 0) {
    snprintf(module_dir, sizeof(module_dir), "%s", module_path);
    path_dirname(module_dir);
    join_path(g_log_path, sizeof(g_log_path), module_dir, PROBE_LOG_NAME);
  } else {
    snprintf(g_log_path, sizeof(g_log_path), "%s", PROBE_LOG_NAME);
  }

  InitializeCriticalSection(&g_log_lock);
  InterlockedExchange(&g_log_ready, 1);
}

static int hooks_disabled_by_flag(void) {
  char module_path[MAX_PATH];
  char module_dir[MAX_PATH];
  char flag_path[MAX_PATH];

  if (env_flag_enabled("SAMP_PROBE_NO_HOOKS")) {
    return 1;
  }

  module_path[0] = '\0';
  module_dir[0] = '\0';
  flag_path[0] = '\0';

  if (GetModuleFileNameA(g_self_module, module_path, sizeof(module_path)) == 0) {
    return 0;
  }

  snprintf(module_dir, sizeof(module_dir), "%s", module_path);
  path_dirname(module_dir);
  join_path(flag_path, sizeof(flag_path), module_dir, PROBE_NO_HOOKS_FLAG);
  return file_exists(flag_path);
}

static void format_sockaddr(const struct sockaddr *addr, int addr_len, char *out, size_t out_size) {
  const struct sockaddr_in *sin;
  const unsigned char *ip;
  const unsigned char *port_bytes;
  unsigned int port;

  if (out == NULL || out_size == 0) {
    return;
  }

  if (addr == NULL) {
    snprintf(out, out_size, "null");
    return;
  }

  if (addr_len < (int)sizeof(struct sockaddr_in) || addr->sa_family != AF_INET) {
    snprintf(out, out_size, "family=%d len=%d", (int)addr->sa_family, addr_len);
    return;
  }

  sin = (const struct sockaddr_in *)addr;
  ip = (const unsigned char *)&sin->sin_addr.s_addr;
  port_bytes = (const unsigned char *)&sin->sin_port;
  port = ((unsigned int)port_bytes[0] << 8u) | (unsigned int)port_bytes[1];
  snprintf(out, out_size, "%u.%u.%u.%u:%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3], port);
}

static void format_binary_address(unsigned long binary_address, unsigned short port, char *out, size_t out_size) {
  const unsigned char *ip = (const unsigned char *)&binary_address;

  if (out == NULL || out_size == 0) {
    return;
  }

  snprintf(out, out_size, "%u.%u.%u.%u:%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
           (unsigned)port);
}

static const char *samp_packet_name(unsigned char packet_id) {
  switch (packet_id) {
    case 6:
      return "ID_INTERNAL_PING";
    case 7:
      return "ID_PING";
    case 8:
      return "ID_PING_OPEN_CONNECTIONS";
    case 9:
      return "ID_CONNECTED_PONG";
    case 10:
      return "ID_REQUEST_STATIC_DATA";
    case 11:
      return "ID_CONNECTION_REQUEST";
    case 12:
      return "ID_AUTH_KEY";
    case 14:
      return "ID_BROADCAST_PINGS";
    case 15:
      return "ID_SECURED_CONNECTION_RESPONSE";
    case 16:
      return "ID_SECURED_CONNECTION_CONFIRMATION";
    case 17:
      return "ID_RPC_MAPPING";
    case 19:
      return "ID_SET_RANDOM_NUMBER_SEED";
    case 20:
      return "ID_RPC";
    case 21:
      return "ID_RPC_REPLY";
    case 23:
      return "ID_DETECT_LOST_CONNECTIONS";
    case 24:
      return "ID_OPEN_CONNECTION_REQUEST";
    case 25:
      return "ID_OPEN_CONNECTION_REPLY";
    case 26:
      return "ID_OPEN_CONNECTION_COOKIE";
    case 28:
      return "ID_RSA_PUBLIC_KEY_MISMATCH";
    case 29:
      return "ID_CONNECTION_ATTEMPT_FAILED";
    case 30:
      return "ID_NEW_INCOMING_CONNECTION";
    case 31:
      return "ID_NO_FREE_INCOMING_CONNECTIONS";
    case 32:
      return "ID_DISCONNECTION_NOTIFICATION";
    case 33:
      return "ID_CONNECTION_LOST";
    case 34:
      return "ID_CONNECTION_REQUEST_ACCEPTED";
    case 36:
      return "ID_CONNECTION_BANNED";
    case 37:
      return "ID_INVALID_PASSWORD";
    case 38:
      return "ID_MODIFIED_PACKET";
    case 39:
      return "ID_PONG";
    case 40:
      return "ID_TIMESTAMP";
    default:
      return "";
  }
}

static const char *known_ordinal_name(const char *dll_name, WORD ordinal) {
  if (dll_name == NULL) {
    return NULL;
  }

  if (lstrcmpiA(dll_name, "WSOCK32.dll") == 0) {
    switch (ordinal) {
      case 2:
        return "bind";
      case 3:
        return "closesocket";
      case 4:
        return "connect";
      case 10:
        return "inet_addr";
      case 11:
        return "inet_ntoa";
      case 16:
        return "recv";
      case 17:
        return "recvfrom";
      case 18:
        return "select";
      case 19:
        return "send";
      case 20:
        return "sendto";
      case 23:
        return "socket";
      case 52:
        return "gethostbyname";
      case 57:
        return "gethostname";
      case 115:
        return "WSAStartup";
      case 116:
        return "WSACleanup";
      default:
        return NULL;
    }
  }

  return NULL;
}

static int last_wsa_error(void) {
  if (g_WSAGetLastError_ptr != NULL) {
    return g_WSAGetLastError_ptr();
  }
  return (int)GetLastError();
}

static int memory_is_readable(uintptr_t address, size_t size) {
  MEMORY_BASIC_INFORMATION mbi;
  DWORD protect;

  if (address == 0 || size == 0) {
    return 0;
  }

  if (VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi)) != sizeof(mbi)) {
    return 0;
  }

  if (mbi.State != MEM_COMMIT) {
    return 0;
  }

  if ((uintptr_t)mbi.BaseAddress + mbi.RegionSize < address + size) {
    return 0;
  }

  protect = mbi.Protect & 0xffu;
  if ((mbi.Protect & PAGE_GUARD) != 0 || protect == PAGE_NOACCESS) {
    return 0;
  }

  return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
         protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

static void bytes_to_hex(const BYTE *bytes, size_t size, char *out, size_t out_size) {
  size_t i;
  size_t used = 0;

  if (out == NULL || out_size == 0) {
    return;
  }

  out[0] = '\0';
  if (bytes == NULL) {
    return;
  }

  for (i = 0; i < size && used + 4 < out_size; ++i) {
    int written = snprintf(out + used, out_size - used, "%s%02x", (i == 0) ? "" : " ", (unsigned)bytes[i]);
    if (written < 0) {
      return;
    }
    used += (size_t)written;
  }
}

static void payload_to_hex(const void *data, int len, char *out, size_t out_size) {
  size_t preview_len;

  if (out == NULL || out_size == 0) {
    return;
  }

  out[0] = '\0';
  if (data == NULL || len <= 0) {
    return;
  }

  preview_len = (size_t)len;
  if (preview_len > PROBE_PAYLOAD_PREVIEW_BYTES) {
    preview_len = PROBE_PAYLOAD_PREVIEW_BYTES;
  }

  if (!memory_is_readable((uintptr_t)data, preview_len)) {
    snprintf(out, out_size, "unreadable");
    return;
  }

  bytes_to_hex((const BYTE *)data, preview_len, out, out_size);
}

static BYTE read_u8_or(uintptr_t address, BYTE fallback) {
  BYTE value;

  if (!memory_is_readable(address, sizeof(value))) {
    return fallback;
  }

  memcpy(&value, (const void *)address, sizeof(value));
  return value;
}

static WORD read_u16_or(uintptr_t address, WORD fallback) {
  WORD value;

  if (!memory_is_readable(address, sizeof(value))) {
    return fallback;
  }

  memcpy(&value, (const void *)address, sizeof(value));
  return value;
}

static DWORD read_u32_or(uintptr_t address, DWORD fallback) {
  DWORD value;

  if (!memory_is_readable(address, sizeof(value))) {
    return fallback;
  }

  memcpy(&value, (const void *)address, sizeof(value));
  return value;
}

static DWORD decode_rel32_target_or(uintptr_t instruction_address, BYTE opcode, DWORD fallback) {
  int32_t rel;
  BYTE actual_opcode;

  if (!memory_is_readable(instruction_address, 5)) {
    return fallback;
  }

  actual_opcode = *(const BYTE *)instruction_address;
  if (actual_opcode != opcode) {
    return fallback;
  }

  memcpy(&rel, (const void *)(instruction_address + 1), sizeof(rel));
  return (DWORD)(instruction_address + 5u + (intptr_t)rel);
}

static void capture_transition_state(probe_transition_state *state) {
  if (state == NULL) {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->entry_gate = read_u32_or(0x00C8D4C0u, 0xffffffffu);
  state->hwnd = read_u32_or(0x00C97C1Cu, 0xffffffffu);
  state->id3d9 = read_u32_or(0x00C97C20u, 0xffffffffu);
  state->id3d9_device = read_u32_or(0x00C97C28u, 0xffffffffu);
  state->game_process_storage = read_u32_or(0x0053BED1u, 0xffffffffu);
  state->render2d_storage = read_u32_or(0x0053E22Cu, 0xffffffffu);
  state->ped_table = read_u32_or(0x00B74490u, 0xffffffffu);
  state->vehicle_table = read_u32_or(0x00B74494u, 0xffffffffu);
  state->script_call_target = decode_rel32_target_or(0x0053BFC7u, 0xe8u, 0xffffffffu);
  state->graphics_call_target = decode_rel32_target_or(0x0053EB12u, 0xe8u, 0xffffffffu);
  state->samp_base = (DWORD)g_samp_base;
  state->samp_size = g_samp_size;
  state->start_game = read_u8_or(0x00BA677Bu, 0xffu);
  state->game_started = read_u8_or(0x00BA6831u, 0xffu);
  state->menu = read_u8_or(0x00BA67A4u, 0xffu);
  state->menu2 = read_u8_or(0x00BA67A5u, 0xffu);
  state->menu3 = read_u8_or(0x00BA67A6u, 0xffu);
  state->hud = read_u8_or(0x00BA6769u, 0xffu);
  state->time_patch = read_u8_or(0x0052CF10u, 0xffu);
  state->script_gate0 = read_u8_or(0x00469EF5u, 0xffu);
  state->script_gate1 = read_u8_or(0x00469EF6u, 0xffu);
  state->script_call_opcode = read_u8_or(0x0053BFC7u, 0xffu);
  state->graphics_call_opcode = read_u8_or(0x0053EB12u, 0xffu);
  state->current_player = read_u8_or(0x00B7CD74u, 0xffu);
  state->camera_mode = read_u8_or(0x00B6F1A8u, 0xffu);
  state->camera_mode2 = read_u16_or(0x00B6F858u, 0xffffu);
}

static void sample_transition_state(const char *reason) {
  probe_transition_state state;
  DWORD now_ms;
  int previous_valid;
  int changed;
  int force_log;

  if (env_flag_enabled("SAMP_PROBE_NO_STATE")) {
    return;
  }

  capture_transition_state(&state);
  now_ms = GetTickCount();
  previous_valid = InterlockedCompareExchange(&g_previous_state_valid, 0, 0) != 0;
  changed = !previous_valid || memcmp(&state, &g_previous_state, sizeof(state)) != 0;
  force_log = env_flag_enabled("SAMP_PROBE_STATE_ALWAYS") ||
              (DWORD)(now_ms - g_previous_state_log_ms) >= PROBE_STATE_HEARTBEAT_MS;

  if (!changed && !force_log) {
    return;
  }

  probe_log("state: reason=%s entry=%lu start=%u game_started=%u menu=%u/%u/%u hud=%u time_patch=0x%02x "
            "script_gate=%02x%02x script_call_op=0x%02x script_target=0x%08lx graphics_call_op=0x%02x "
            "graphics_target=0x%08lx gp_storage=0x%08lx render2d_storage=0x%08lx hwnd=0x%08lx d3d=0x%08lx "
            "d3ddev=0x%08lx ped_table=0x%08lx vehicle_table=0x%08lx current_player=%u camera=%u/%u "
            "samp=0x%08lx+0x%08lx",
            changed ? (reason ? reason : "changed") : "heartbeat", (unsigned long)state.entry_gate,
            (unsigned)state.start_game, (unsigned)state.game_started, (unsigned)state.menu, (unsigned)state.menu2,
            (unsigned)state.menu3, (unsigned)state.hud, (unsigned)state.time_patch, (unsigned)state.script_gate0,
            (unsigned)state.script_gate1, (unsigned)state.script_call_opcode, (unsigned long)state.script_call_target,
            (unsigned)state.graphics_call_opcode, (unsigned long)state.graphics_call_target,
            (unsigned long)state.game_process_storage, (unsigned long)state.render2d_storage, (unsigned long)state.hwnd,
            (unsigned long)state.id3d9, (unsigned long)state.id3d9_device, (unsigned long)state.ped_table,
            (unsigned long)state.vehicle_table, (unsigned)state.current_player, (unsigned)state.camera_mode,
            (unsigned)state.camera_mode2, (unsigned long)state.samp_base, (unsigned long)state.samp_size);

  if (changed) {
    memcpy(&g_previous_state, &state, sizeof(g_previous_state));
    InterlockedExchange(&g_previous_state_valid, 1);
  }
  g_previous_state_log_ms = now_ms;
}

static void sample_watchpoints(void) {
  size_t i;

  if (env_flag_enabled("SAMP_PROBE_NO_WATCH")) {
    return;
  }

  for (i = 0; i < sizeof(g_watchpoints) / sizeof(g_watchpoints[0]); ++i) {
    probe_watchpoint *wp = &g_watchpoints[i];
    BYTE current[16];
    char hex[80];

    if (wp->size > sizeof(current)) {
      continue;
    }

    if (!memory_is_readable(wp->address, wp->size)) {
      if (wp->has_previous) {
        probe_log("watch: name=%s addr=0x%08lx unreadable", wp->name, (unsigned long)wp->address);
        wp->has_previous = 0;
      }
      continue;
    }

    memcpy(current, (const void *)wp->address, wp->size);
    if (!wp->has_previous || memcmp(current, wp->previous, wp->size) != 0) {
      bytes_to_hex(current, wp->size, hex, sizeof(hex));
      probe_log("watch: name=%s addr=0x%08lx bytes=%s", wp->name, (unsigned long)wp->address, hex);
      memcpy(wp->previous, current, wp->size);
      wp->has_previous = 1;
    }
  }
}

static size_t modrm_extra_length(const BYTE *code, size_t pos) {
  BYTE modrm;
  BYTE mod;
  BYTE rm;
  BYTE base;
  size_t extra = 1;

  modrm = code[pos];
  mod = (BYTE)((modrm >> 6u) & 3u);
  rm = (BYTE)(modrm & 7u);

  if (mod != 3 && rm == 4) {
    BYTE sib = code[pos + extra];
    ++extra;
    base = (BYTE)(sib & 7u);
    if (mod == 0 && base == 5) {
      extra += 4;
    }
  } else if (mod == 0 && rm == 5) {
    extra += 4;
  }

  if (mod == 1) {
    extra += 1;
  } else if (mod == 2) {
    extra += 4;
  }

  return extra;
}

static size_t x86_instruction_length(const BYTE *code) {
  size_t pos = 0;
  BYTE op;

  while (code[pos] == 0xf0 || code[pos] == 0xf2 || code[pos] == 0xf3 || code[pos] == 0x2e || code[pos] == 0x36 ||
         code[pos] == 0x3e || code[pos] == 0x26 || code[pos] == 0x64 || code[pos] == 0x65 || code[pos] == 0x66 ||
         code[pos] == 0x67) {
    ++pos;
  }

  op = code[pos++];

  if ((op >= 0x50 && op <= 0x5f) || op == 0x90 || op == 0xc3 || op == 0xcc || op == 0x9c || op == 0x9d) {
    return pos;
  }
  if (op == 0xc2) {
    return pos + 2;
  }
  if (op == 0x6a) {
    return pos + 1;
  }
  if (op == 0x68 || (op >= 0xb8 && op <= 0xbf) || op == 0xa1 || op == 0xa3) {
    return pos + 4;
  }
  if (op == 0xe8 || op == 0xe9 || op == 0xeb) {
    return 0;
  }
  if (op == 0x8b || op == 0x89 || op == 0x8d || op == 0x85 || op == 0x3b || op == 0x33 || op == 0x31 ||
      op == 0x39 || op == 0x8f || op == 0xff) {
    return pos + modrm_extra_length(code, pos);
  }
  if (op == 0x80 || op == 0x82 || op == 0x83) {
    return pos + modrm_extra_length(code, pos) + 1;
  }
  if (op == 0x81 || op == 0xc7) {
    return pos + modrm_extra_length(code, pos) + 4;
  }
  if (op == 0x0f) {
    BYTE op2 = code[pos++];
    if (op2 >= 0x80 && op2 <= 0x8f) {
      return 0;
    }
    return pos + modrm_extra_length(code, pos);
  }

  return 0;
}

static void *resolve_inline_hook_target(void *target) {
  BYTE *cursor = (BYTE *)target;
  int depth;

  for (depth = 0; depth < 4 && cursor != NULL && memory_is_readable((uintptr_t)cursor, 8); ++depth) {
    if (cursor[0] == 0xe9) {
      int32_t rel = 0;
      memcpy(&rel, cursor + 1, sizeof(rel));
      cursor = cursor + 5 + rel;
      continue;
    }
    if (cursor[0] == 0xff && cursor[1] == 0x25) {
      uint32_t ptr_addr = 0;
      memcpy(&ptr_addr, cursor + 2, sizeof(ptr_addr));
      if (memory_is_readable(ptr_addr, sizeof(void *))) {
        cursor = *(BYTE **)(uintptr_t)ptr_addr;
        continue;
      }
    }
    break;
  }

  return cursor;
}

static size_t calculate_patch_length(void *target) {
  BYTE *cursor = (BYTE *)target;
  size_t total = 0;

  while (total < 5 && total < PROBE_INLINE_HOOK_MAX_COPY) {
    size_t len;
    if (!memory_is_readable((uintptr_t)(cursor + total), 8)) {
      return 0;
    }
    len = x86_instruction_length(cursor + total);
    if (len == 0 || total + len > PROBE_INLINE_HOOK_MAX_COPY) {
      return 0;
    }
    total += len;
  }

  return total >= 5 ? total : 0;
}

static void *create_trampoline(void *target, const BYTE *saved, size_t saved_len) {
  BYTE *trampoline;
  int32_t rel_back;
  DWORD old_protect = 0;

  trampoline = (BYTE *)VirtualAlloc(NULL, saved_len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (trampoline == NULL) {
    return NULL;
  }

  memcpy(trampoline, saved, saved_len);
  trampoline[saved_len] = 0xe9;
  rel_back = (int32_t)((intptr_t)((BYTE *)target + saved_len) - (intptr_t)(trampoline + saved_len + 5));
  memcpy(trampoline + saved_len + 1, &rel_back, sizeof(rel_back));
  FlushInstructionCache(GetCurrentProcess(), trampoline, saved_len + 5);
  (void)VirtualProtect(trampoline, saved_len + 5, PAGE_EXECUTE_READ, &old_protect);
  return trampoline;
}

static int install_inline_hook(probe_inline_hook *hook) {
  HMODULE module;
  FARPROC export_addr;
  void *patch_target;
  size_t patch_len;
  BYTE *target_bytes;
  DWORD old_protect = 0;
  DWORD restore_protect = 0;
  int32_t rel;
  size_t i;

  if (hook == NULL || InterlockedCompareExchange(&hook->installed, 0, 0) != 0) {
    return 0;
  }

  module = GetModuleHandleA(hook->dll_name);
  if (module == NULL) {
    module = LoadLibraryA(hook->dll_name);
  }
  if (module == NULL) {
    return 0;
  }

  export_addr = GetProcAddress(module, hook->symbol_name);
  if (export_addr == NULL) {
    return 0;
  }

  patch_target = resolve_inline_hook_target((void *)export_addr);
  if (patch_target == NULL || patch_target == hook->replacement || !memory_is_readable((uintptr_t)patch_target, 8)) {
    return 0;
  }

  target_bytes = (BYTE *)patch_target;
  if (target_bytes[0] == 0xe9) {
    int32_t existing_rel = 0;
    void *existing_target;
    memcpy(&existing_rel, target_bytes + 1, sizeof(existing_rel));
    existing_target = target_bytes + 5 + existing_rel;
    if (existing_target == hook->replacement) {
      InterlockedExchange(&hook->installed, 1);
      return 1;
    }
  }

  patch_len = calculate_patch_length(patch_target);
  if (patch_len == 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("inline_hook: skip dll=%s name=%s target=%p prologue=%s", hook->dll_name, hook->symbol_name, patch_target, hex);
    return 0;
  }

  memcpy(hook->saved, patch_target, patch_len);
  hook->saved_len = patch_len;
  hook->target = (void *)export_addr;
  hook->patch_target = patch_target;
  hook->trampoline = create_trampoline(patch_target, hook->saved, hook->saved_len);
  if (hook->trampoline == NULL) {
    probe_log("inline_hook: trampoline failed dll=%s name=%s target=%p", hook->dll_name, hook->symbol_name, patch_target);
    return 0;
  }

  if (hook->original_slot != NULL) {
    *hook->original_slot = hook->trampoline;
  }

  if (!VirtualProtect(patch_target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
    probe_log("inline_hook: VirtualProtect failed dll=%s name=%s target=%p error=%lu", hook->dll_name, hook->symbol_name,
              patch_target, (unsigned long)GetLastError());
    return 0;
  }

  target_bytes[0] = 0xe9;
  rel = (int32_t)((intptr_t)hook->replacement - (intptr_t)(target_bytes + 5));
  memcpy(target_bytes + 1, &rel, sizeof(rel));
  for (i = 5; i < patch_len; ++i) {
    target_bytes[i] = 0x90;
  }
  FlushInstructionCache(GetCurrentProcess(), patch_target, patch_len);
  (void)VirtualProtect(patch_target, patch_len, old_protect, &restore_protect);

  InterlockedExchange(&hook->installed, 1);
  probe_log("inline_hook: installed dll=%s name=%s export=%p target=%p trampoline=%p len=%lu", hook->dll_name,
            hook->symbol_name, (void *)export_addr, patch_target, hook->trampoline, (unsigned long)patch_len);
  return 1;
}

static int install_inline_hooks(int log_summary) {
  size_t i;
  int installed = 0;

  if (env_flag_enabled("SAMP_PROBE_NO_INLINE_HOOKS")) {
    if (log_summary) {
      probe_log("inline_hook: disabled by env");
    }
    return 0;
  }

  for (i = 0; i < sizeof(g_inline_hooks) / sizeof(g_inline_hooks[0]); ++i) {
    installed += install_inline_hook(&g_inline_hooks[i]);
  }

  if (log_summary) {
    probe_log("inline_hook: summary installed_or_present=%d requested=%u", installed,
              (unsigned)(sizeof(g_inline_hooks) / sizeof(g_inline_hooks[0])));
  }
  return installed;
}

static int install_samp_code_hook(probe_code_hook *hook) {
  BYTE *target_bytes;
  void *patch_target;
  size_t patch_len;
  DWORD old_protect = 0;
  DWORD restore_protect = 0;
  int32_t rel;
  size_t i;

  if (hook == NULL || InterlockedCompareExchange(&hook->installed, 0, 0) != 0) {
    return 0;
  }
  if (g_samp_base == 0 || g_samp_size == 0 || hook->rva >= g_samp_size ||
      hook->rva + hook->expected_len > g_samp_size) {
    return 0;
  }

  patch_target = (void *)(g_samp_base + (uintptr_t)hook->rva);
  if (patch_target == hook->replacement || !memory_is_readable((uintptr_t)patch_target, 8)) {
    return 0;
  }

  target_bytes = (BYTE *)patch_target;
  if (target_bytes[0] == 0xe9) {
    int32_t existing_rel = 0;
    void *existing_target;
    memcpy(&existing_rel, target_bytes + 1, sizeof(existing_rel));
    existing_target = target_bytes + 5 + existing_rel;
    if (existing_target == hook->replacement) {
      InterlockedExchange(&hook->installed, 1);
      return 1;
    }
  }

  if (hook->expected_len > 0 && memcmp(patch_target, hook->expected, hook->expected_len) != 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("samp_code_hook: skip name=%s rva=0x%08lx target=%p prologue=%s", hook->name, (unsigned long)hook->rva,
              patch_target, hex);
    InterlockedExchange(&hook->installed, -1);
    return 0;
  }

  patch_len = calculate_patch_length(patch_target);
  if (patch_len == 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("samp_code_hook: skip name=%s rva=0x%08lx target=%p prologue=%s", hook->name, (unsigned long)hook->rva,
              patch_target, hex);
    InterlockedExchange(&hook->installed, -1);
    return 0;
  }

  memcpy(hook->saved, patch_target, patch_len);
  hook->saved_len = patch_len;
  hook->patch_target = patch_target;
  hook->trampoline = create_trampoline(patch_target, hook->saved, hook->saved_len);
  if (hook->trampoline == NULL) {
    probe_log("samp_code_hook: trampoline failed name=%s rva=0x%08lx target=%p", hook->name, (unsigned long)hook->rva,
              patch_target);
    return 0;
  }

  if (hook->original_slot != NULL) {
    *hook->original_slot = hook->trampoline;
  }

  if (!VirtualProtect(patch_target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
    probe_log("samp_code_hook: VirtualProtect failed name=%s rva=0x%08lx target=%p error=%lu", hook->name,
              (unsigned long)hook->rva, patch_target, (unsigned long)GetLastError());
    return 0;
  }

  target_bytes[0] = 0xe9;
  rel = (int32_t)((intptr_t)hook->replacement - (intptr_t)(target_bytes + 5));
  memcpy(target_bytes + 1, &rel, sizeof(rel));
  for (i = 5; i < patch_len; ++i) {
    target_bytes[i] = 0x90;
  }
  FlushInstructionCache(GetCurrentProcess(), patch_target, patch_len);
  (void)VirtualProtect(patch_target, patch_len, old_protect, &restore_protect);

  InterlockedExchange(&hook->installed, 1);
  probe_log("samp_code_hook: installed name=%s rva=0x%08lx target=%p trampoline=%p len=%lu", hook->name,
            (unsigned long)hook->rva, patch_target, hook->trampoline, (unsigned long)patch_len);
  return 1;
}

static int install_samp_code_hooks(int log_summary) {
  size_t i;
  int installed = 0;

  if (env_flag_enabled("SAMP_PROBE_NO_SAMP_CODE_HOOKS")) {
    if (log_summary) {
      probe_log("samp_code_hook: disabled by env");
    }
    return 0;
  }

  for (i = 0; i < sizeof(g_samp_code_hooks) / sizeof(g_samp_code_hooks[0]); ++i) {
    installed += install_samp_code_hook(&g_samp_code_hooks[i]);
  }

  if (log_summary) {
    probe_log("samp_code_hook: summary installed=%d requested=%u", installed,
              (unsigned)(sizeof(g_samp_code_hooks) / sizeof(g_samp_code_hooks[0])));
  }
  return installed;
}

static int rva_is_sane(PIMAGE_NT_HEADERS nt, DWORD rva, DWORD min_size) {
  return rva != 0 && rva + min_size >= rva && rva + min_size <= nt->OptionalHeader.SizeOfImage;
}

static void dump_samp_sections(PIMAGE_NT_HEADERS nt) {
  PIMAGE_SECTION_HEADER section;
  WORD i;

  section = IMAGE_FIRST_SECTION(nt);
  for (i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    char name[9];
    memcpy(name, section[i].Name, 8);
    name[8] = '\0';
    probe_log("section: index=%u name=%s va=0x%08lx vsz=0x%08lx raw=0x%08lx chars=0x%08lx", (unsigned)i, name,
              (unsigned long)section[i].VirtualAddress, (unsigned long)section[i].Misc.VirtualSize,
              (unsigned long)section[i].SizeOfRawData, (unsigned long)section[i].Characteristics);
  }
}

static void dump_samp_imports(PIMAGE_NT_HEADERS nt) {
  DWORD import_rva;
  PIMAGE_IMPORT_DESCRIPTOR desc;
  int logged = 0;

  import_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if (!rva_is_sane(nt, import_rva, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
    probe_log("imports: none");
    return;
  }

  desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE *)g_samp_module + import_rva);
  for (; desc->Name != 0; ++desc) {
    const char *dll_name;
    PIMAGE_THUNK_DATA name_thunk;
    PIMAGE_THUNK_DATA iat_thunk;

    if (!rva_is_sane(nt, desc->Name, 1)) {
      continue;
    }

    dll_name = (const char *)((BYTE *)g_samp_module + desc->Name);
    name_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module +
                                     (desc->OriginalFirstThunk != 0 ? desc->OriginalFirstThunk : desc->FirstThunk));
    iat_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module + desc->FirstThunk);
    probe_log("import_dll: %s first_thunk=0x%08lx", dll_name, (unsigned long)desc->FirstThunk);

    for (; name_thunk->u1.AddressOfData != 0 && iat_thunk->u1.Function != 0; ++name_thunk, ++iat_thunk) {
      const char *symbol = NULL;
      void *target = (void *)(uintptr_t)iat_thunk->u1.Function;

      if (IMAGE_SNAP_BY_ORDINAL(name_thunk->u1.Ordinal)) {
        WORD ordinal = (WORD)IMAGE_ORDINAL(name_thunk->u1.Ordinal);
        const char *known_name = known_ordinal_name(dll_name, ordinal);
        if (known_name != NULL) {
          probe_log("import: dll=%s ordinal=%u known=%s iat=%p target=%p", dll_name, (unsigned)ordinal, known_name,
                    (void *)&iat_thunk->u1.Function, target);
        } else {
          probe_log("import: dll=%s ordinal=%u iat=%p target=%p", dll_name, (unsigned)ordinal,
                    (void *)&iat_thunk->u1.Function, target);
        }
      } else if (rva_is_sane(nt, (DWORD)name_thunk->u1.AddressOfData, sizeof(IMAGE_IMPORT_BY_NAME))) {
        PIMAGE_IMPORT_BY_NAME by_name = (PIMAGE_IMPORT_BY_NAME)((BYTE *)g_samp_module + name_thunk->u1.AddressOfData);
        symbol = (const char *)by_name->Name;
        if (InterlockedIncrement(&g_import_log_count) <= PROBE_MAX_IMPORT_LOGS) {
          probe_log("import: dll=%s name=%s iat=%p target=%p", dll_name, symbol, (void *)&iat_thunk->u1.Function, target);
        }
      }

      ++logged;
    }
  }

  probe_log("imports: logged_entries=%d", logged);
}

static int hook_one_import(PIMAGE_NT_HEADERS nt, probe_iat_hook *hook) {
  DWORD import_rva;
  PIMAGE_IMPORT_DESCRIPTOR desc;

  import_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if (!rva_is_sane(nt, import_rva, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
    return 0;
  }

  desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE *)g_samp_module + import_rva);
  for (; desc->Name != 0; ++desc) {
    const char *dll_name;
    PIMAGE_THUNK_DATA name_thunk;
    PIMAGE_THUNK_DATA iat_thunk;

    if (!rva_is_sane(nt, desc->Name, 1)) {
      continue;
    }

    dll_name = (const char *)((BYTE *)g_samp_module + desc->Name);
    if (lstrcmpiA(dll_name, hook->dll_name) != 0) {
      continue;
    }

    name_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module +
                                     (desc->OriginalFirstThunk != 0 ? desc->OriginalFirstThunk : desc->FirstThunk));
    iat_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module + desc->FirstThunk);

    for (; name_thunk->u1.AddressOfData != 0 && iat_thunk->u1.Function != 0; ++name_thunk, ++iat_thunk) {
      PIMAGE_IMPORT_BY_NAME by_name;
      void **slot;
      DWORD old_protect;
      DWORD restore_protect;

      if (IMAGE_SNAP_BY_ORDINAL(name_thunk->u1.Ordinal)) {
        WORD ordinal = (WORD)IMAGE_ORDINAL(name_thunk->u1.Ordinal);
        if (hook->ordinal == 0 || ordinal != hook->ordinal) {
          continue;
        }
      } else {
        if (!rva_is_sane(nt, (DWORD)name_thunk->u1.AddressOfData, sizeof(IMAGE_IMPORT_BY_NAME))) {
          continue;
        }

        by_name = (PIMAGE_IMPORT_BY_NAME)((BYTE *)g_samp_module + name_thunk->u1.AddressOfData);
        if (hook->symbol_name == NULL || lstrcmpA((const char *)by_name->Name, hook->symbol_name) != 0) {
          continue;
        }
      }

      slot = (void **)&iat_thunk->u1.Function;
      if (*slot == hook->replacement) {
        return 1;
      }

      if (hook->original_slot != NULL && (*hook->original_slot == NULL || lstrcmpiA(hook->dll_name, "WS2_32.dll") == 0)) {
        *hook->original_slot = *slot;
      }

      if (!VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old_protect)) {
        probe_log("hook: failed dll=%s name=%s iat=%p error=%lu", hook->dll_name, hook->symbol_name, slot,
                  (unsigned long)GetLastError());
        return 0;
      }

      *slot = hook->replacement;
      FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void *));
      (void)VirtualProtect(slot, sizeof(void *), old_protect, &restore_protect);
      InterlockedIncrement(&hook->install_count);
      if (IMAGE_SNAP_BY_ORDINAL(name_thunk->u1.Ordinal)) {
        probe_log("hook: installed dll=%s ordinal=%u name=%s iat=%p original=%p replacement=%p", hook->dll_name,
                  (unsigned)hook->ordinal, hook->symbol_name ? hook->symbol_name : "", slot,
                  hook->original_slot ? *hook->original_slot : NULL, hook->replacement);
      } else {
        probe_log("hook: installed dll=%s name=%s iat=%p original=%p replacement=%p", hook->dll_name,
                  hook->symbol_name ? hook->symbol_name : "", slot, hook->original_slot ? *hook->original_slot : NULL,
                  hook->replacement);
      }
      return 1;
    }
  }

  return 0;
}

static int install_iat_hooks(PIMAGE_NT_HEADERS nt, int log_summary) {
  size_t i;
  int installed = 0;

  for (i = 0; i < sizeof(g_hooks) / sizeof(g_hooks[0]); ++i) {
    installed += hook_one_import(nt, &g_hooks[i]);
  }

  if (log_summary) {
    probe_log("hook: summary installed=%d requested=%u", installed, (unsigned)(sizeof(g_hooks) / sizeof(g_hooks[0])));
  }
  return installed;
}

static PIMAGE_NT_HEADERS get_samp_nt_headers(void) {
  PIMAGE_DOS_HEADER dos;
  PIMAGE_NT_HEADERS nt;

  if (g_samp_module == NULL) {
    return NULL;
  }

  dos = (PIMAGE_DOS_HEADER)g_samp_module;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return NULL;
  }

  nt = (PIMAGE_NT_HEADERS)((BYTE *)g_samp_module + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    return NULL;
  }

  return nt;
}

static void resolve_helper_exports(void) {
  HMODULE wsock = GetModuleHandleA("WSOCK32.dll");
  if (wsock == NULL) {
    wsock = GetModuleHandleA("wsock32.dll");
  }
  if (wsock != NULL) {
    g_WSAGetLastError_ptr = (probe_WSAGetLastError_fn)GetProcAddress(wsock, "WSAGetLastError");
  }
}

static DWORD WINAPI probe_worker(LPVOID param) {
  DWORD start_ms;
  PIMAGE_NT_HEADERS nt;
  int hooks_disabled;
  (void)param;

  init_log_path();
  probe_log("probe: attached build=%s %s", __DATE__, __TIME__);

  start_ms = GetTickCount();
  while (WaitForSingleObject(g_stop_event, 50) == WAIT_TIMEOUT) {
    g_samp_module = GetModuleHandleA("samp.dll");
    if (g_samp_module != NULL) {
      break;
    }
    if ((DWORD)(GetTickCount() - start_ms) > PROBE_WAIT_FOR_SAMP_MS) {
      probe_log("probe: timed out waiting for samp.dll");
      return 0;
    }
  }

  if (g_samp_module == NULL) {
    return 0;
  }

  nt = get_samp_nt_headers();
  if (nt == NULL) {
    probe_log("samp: invalid PE headers module=%p", (void *)g_samp_module);
    return 0;
  }

  g_samp_base = (uintptr_t)g_samp_module;
  g_samp_size = nt->OptionalHeader.SizeOfImage;
  probe_log("samp: loaded base=0x%08lx size=0x%08lx entry=0x%08lx image_base=0x%08lx sections=%u imports_rva=0x%08lx",
            (unsigned long)g_samp_base, (unsigned long)g_samp_size, (unsigned long)nt->OptionalHeader.AddressOfEntryPoint,
            (unsigned long)nt->OptionalHeader.ImageBase, (unsigned)nt->FileHeader.NumberOfSections,
            (unsigned long)nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

  dump_samp_sections(nt);
  dump_samp_imports(nt);
  resolve_helper_exports();

  hooks_disabled = hooks_disabled_by_flag();
  if (hooks_disabled) {
    probe_log("hook: disabled by flag/env");
  } else {
    install_iat_hooks(nt, 1);
    install_inline_hooks(1);
    install_samp_code_hooks(1);
  }

  sample_transition_state("after_samp_load");

  while (WaitForSingleObject(g_stop_event, PROBE_WATCH_INTERVAL_MS) == WAIT_TIMEOUT) {
    if (!hooks_disabled) {
      (void)install_iat_hooks(nt, 0);
      (void)install_inline_hooks(0);
      (void)install_samp_code_hooks(0);
    }
    sample_watchpoints();
    sample_transition_state("tick");
  }

  probe_log("probe: stopping");
  return 0;
}

static int WINAPI hook_samp_socketlayer_sendto(SOCKET s, const char *buf, int len, unsigned long binary_address,
                                               unsigned short port) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  const unsigned char *bytes = (const unsigned char *)buf;
  const char *name = "";
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  int rc;

  if (bytes != NULL && len > 0) {
    name = samp_packet_name(bytes[0]);
  }
  if (should_log_call(n)) {
    format_binary_address(binary_address, port, addr, sizeof(addr));
    payload_to_hex(buf, len, payload, sizeof(payload));
    probe_log("samp_internal: SocketLayer.SendTo.begin count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d to=%s "
              "plain_id=0x%02x plain_name=%s data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, addr,
              (bytes != NULL && len > 0) ? (unsigned)bytes[0] : 0u, name, payload);
  }

  if (g_orig_samp_socketlayer_sendto == NULL) {
    return -1;
  }

  rc = ((probe_samp_socketlayer_sendto_fn)g_orig_samp_socketlayer_sendto)(s, buf, len, binary_address, port);
  if (should_log_call(n)) {
    probe_log("samp_internal: SocketLayer.SendTo.end count=%ld rc=%d err=%d", (long)n, rc, last_wsa_error());
  }
  return rc;
}

static void WINAPI hook_samp_process_network_packet(unsigned long binary_address, unsigned short port, const char *data, int len,
                                                    void *rak_peer) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  const unsigned char *bytes = (const unsigned char *)data;
  const char *name = "";
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);

  if (bytes != NULL && len > 0) {
    name = samp_packet_name(bytes[0]);
  }
  if (should_log_call(n)) {
    format_binary_address(binary_address, port, addr, sizeof(addr));
    payload_to_hex(data, len, payload, sizeof(payload));
    probe_log("samp_internal: ProcessNetworkPacket count=%ld caller=%p caller_rva=0x%08lx from=%s len=%d rak_peer=%p "
              "plain_id=0x%02x plain_name=%s data=%s",
              (long)n, caller, samp_rva_from_address(caller), addr, len, rak_peer,
              (bytes != NULL && len > 0) ? (unsigned)bytes[0] : 0u, name, payload);
  }

  if (g_orig_samp_process_network_packet != NULL) {
    ((probe_samp_process_network_packet_fn)g_orig_samp_process_network_packet)(binary_address, port, data, len, rak_peer);
  }
}

static int WINAPI hook_WSAStartup(WORD version, LPWSADATA data) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_WSAStartup_fn)g_orig_WSAStartup)(version, data);
  if (log_this && should_log_call(n)) {
    probe_log("call: WSAStartup count=%ld caller=%p caller_rva=0x%08lx version=0x%04x rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), (unsigned)version, rc, last_wsa_error());
  }
  return rc;
}

static int WINAPI hook_WSACleanup(void) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_WSACleanup_fn)g_orig_WSACleanup)();
  if (log_this && should_log_call(n)) {
    probe_log("call: WSACleanup count=%ld caller=%p caller_rva=0x%08lx rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), rc, last_wsa_error());
  }
  return rc;
}

static SOCKET WINAPI hook_socket(int af, int type, int protocol) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  SOCKET s = ((probe_socket_fn)g_orig_socket)(af, type, protocol);
  if (log_this && should_log_call(n)) {
    probe_log("call: socket count=%ld caller=%p caller_rva=0x%08lx af=%d type=%d proto=%d result=0x%lx err=%d",
              (long)n, caller, samp_rva_from_address(caller), af, type, protocol, (unsigned long)s, last_wsa_error());
  }
  return s;
}

static int WINAPI hook_bind(SOCKET s, const struct sockaddr *name, int namelen) {
  static LONG count;
  char addr[96];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_bind_fn)g_orig_bind)(s, name, namelen);
  if (log_this && should_log_call(n)) {
    format_sockaddr(name, namelen, addr, sizeof(addr));
    probe_log("call: bind count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx addr=%s rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)s, addr, rc, last_wsa_error());
  }
  return rc;
}

static int WINAPI hook_connect(SOCKET s, const struct sockaddr *name, int namelen) {
  static LONG count;
  char addr[96];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc;

  if (log_this && should_log_call(n)) {
    format_sockaddr(name, namelen, addr, sizeof(addr));
    probe_log("call: connect.begin count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx addr=%s", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)s, addr);
  }
  rc = ((probe_connect_fn)g_orig_connect)(s, name, namelen);
  if (log_this && should_log_call(n)) {
    format_sockaddr(name, namelen, addr, sizeof(addr));
    probe_log("call: connect.end count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx addr=%s rc=%d err=%d", (long)n,
              caller, samp_rva_from_address(caller), (unsigned long)s, addr, rc, last_wsa_error());
  }
  return rc;
}

static int WINAPI hook_closesocket(SOCKET s) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_closesocket_fn)g_orig_closesocket)(s);
  if (log_this && should_log_call(n)) {
    probe_log("call: closesocket count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)s, rc, last_wsa_error());
  }
  return rc;
}

static struct hostent *WINAPI hook_gethostbyname(const char *name) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  struct hostent *result = ((probe_gethostbyname_fn)g_orig_gethostbyname)(name);
  if (log_this && should_log_call(n)) {
    probe_log("call: gethostbyname count=%ld caller=%p caller_rva=0x%08lx name=%s result=%p err=%d", (long)n, caller,
              samp_rva_from_address(caller), name ? name : "(null)", (void *)result, last_wsa_error());
  }
  return result;
}

static int WINAPI hook_gethostname(char *name, int namelen) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_gethostname_fn)g_orig_gethostname)(name, namelen);
  if (log_this && should_log_call(n)) {
    probe_log("call: gethostname count=%ld caller=%p caller_rva=0x%08lx rc=%d value=%s err=%d", (long)n, caller,
              samp_rva_from_address(caller), rc, (rc == 0 && name != NULL) ? name : "", last_wsa_error());
  }
  return rc;
}

static unsigned long WINAPI hook_inet_addr(const char *cp) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  unsigned long rc = ((probe_inet_addr_fn)g_orig_inet_addr)(cp);
  if (log_this && should_log_call(n)) {
    probe_log("call: inet_addr count=%ld caller=%p caller_rva=0x%08lx text=%s result=0x%08lx", (long)n, caller,
              samp_rva_from_address(caller), cp ? cp : "(null)", rc);
  }
  return rc;
}

static char *WINAPI hook_inet_ntoa(struct in_addr in) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  char *rc = ((probe_inet_ntoa_fn)g_orig_inet_ntoa)(in);
  if (log_this && should_log_call(n)) {
    probe_log("call: inet_ntoa count=%ld caller=%p caller_rva=0x%08lx addr=0x%08lx text=%s", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)in.s_addr, rc ? rc : "(null)");
  }
  return rc;
}

static int WINAPI hook_send(SOCKET s, const char *buf, int len, int flags) {
  static LONG count;
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_send_fn)g_orig_send)(s, buf, len, flags);
  if (log_this && should_log_call(n)) {
    payload_to_hex(buf, len, payload, sizeof(payload));
    probe_log("call: send count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d flags=0x%x rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, flags, rc, last_wsa_error(), payload);
  }
  return rc;
}

static int WINAPI hook_recv(SOCKET s, char *buf, int len, int flags) {
  static LONG count;
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_recv_fn)g_orig_recv)(s, buf, len, flags);
  if (log_this && should_log_call(n)) {
    payload_to_hex(buf, rc > 0 ? rc : 0, payload, sizeof(payload));
    probe_log("call: recv count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d flags=0x%x rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, flags, rc, last_wsa_error(), payload);
  }
  return rc;
}

static int WINAPI hook_sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_sendto_fn)g_orig_sendto)(s, buf, len, flags, to, tolen);
  if (log_this && should_log_call(n)) {
    format_sockaddr(to, tolen, addr, sizeof(addr));
    payload_to_hex(buf, len, payload, sizeof(payload));
    probe_log("call: sendto count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d flags=0x%x to=%s rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, flags, addr, rc, last_wsa_error(),
              payload);
  }
  return rc;
}

static int WINAPI hook_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  int observed_len = fromlen != NULL ? *fromlen : 0;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_recvfrom_fn)g_orig_recvfrom)(s, buf, len, flags, from, fromlen);
  if (log_this && should_log_call(n)) {
    format_sockaddr(from, fromlen != NULL ? *fromlen : observed_len, addr, sizeof(addr));
    payload_to_hex(buf, rc > 0 ? rc : 0, payload, sizeof(payload));
    probe_log("call: recvfrom count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d flags=0x%x from=%s rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, flags, addr, rc, last_wsa_error(),
              payload);
  }
  return rc;
}

static int WINAPI hook_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_select_fn)g_orig_select)(nfds, readfds, writefds, exceptfds, timeout);
  if (log_this && should_log_call(n)) {
    probe_log("call: select count=%ld caller=%p caller_rva=0x%08lx nfds=%d rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), nfds, rc, last_wsa_error());
  }
  return rc;
}

static BOOL WINAPI hook_VirtualProtect(LPVOID address, SIZE_T size, DWORD new_protect, PDWORD old_protect) {
  static LONG count;
  LONG n = next_call_count(&count);
  BOOL rc = ((probe_VirtualProtect_fn)g_orig_VirtualProtect)(address, size, new_protect, old_protect);
  probe_log("call: VirtualProtect count=%ld addr=%p size=0x%lx new=0x%lx old=0x%lx rc=%d err=%lu", (long)n, address,
            (unsigned long)size, (unsigned long)new_protect, old_protect ? (unsigned long)*old_protect : 0ul, (int)rc,
            (unsigned long)GetLastError());
  return rc;
}

static HMODULE WINAPI hook_LoadLibraryA(LPCSTR file_name) {
  static LONG count;
  LONG n = next_call_count(&count);
  HMODULE rc = ((probe_LoadLibraryA_fn)g_orig_LoadLibraryA)(file_name);
  probe_log("call: LoadLibraryA count=%ld name=%s result=%p err=%lu", (long)n, file_name ? file_name : "(null)", (void *)rc,
            (unsigned long)GetLastError());
  if (g_samp_module != NULL) {
    PIMAGE_NT_HEADERS nt = get_samp_nt_headers();
    if (nt != NULL) {
      (void)install_iat_hooks(nt, 0);
    }
  }
  return rc;
}

static HMODULE WINAPI hook_LoadLibraryW(LPCWSTR file_name) {
  static LONG count;
  char narrowed[260];
  LONG n = next_call_count(&count);
  HMODULE rc = ((probe_LoadLibraryW_fn)g_orig_LoadLibraryW)(file_name);

  narrowed[0] = '\0';
  if (file_name != NULL) {
    WideCharToMultiByte(CP_ACP, 0, file_name, -1, narrowed, sizeof(narrowed), NULL, NULL);
  }
  probe_log("call: LoadLibraryW count=%ld name=%s result=%p err=%lu", (long)n, narrowed[0] ? narrowed : "(null)", (void *)rc,
            (unsigned long)GetLastError());
  return rc;
}

static FARPROC WINAPI hook_GetProcAddress(HMODULE module, LPCSTR proc_name) {
  static LONG count;
  LONG n = next_call_count(&count);
  FARPROC rc = ((probe_GetProcAddress_fn)g_orig_GetProcAddress)(module, proc_name);
  if (((uintptr_t)proc_name >> 16u) == 0) {
    probe_log("call: GetProcAddress count=%ld module=%p ordinal=%lu result=%p err=%lu", (long)n, (void *)module,
              (unsigned long)(uintptr_t)proc_name, (void *)rc, (unsigned long)GetLastError());
  } else {
    probe_log("call: GetProcAddress count=%ld module=%p name=%s result=%p err=%lu", (long)n, (void *)module,
              proc_name ? proc_name : "(null)", (void *)rc, (unsigned long)GetLastError());
  }
  return rc;
}

static HANDLE WINAPI hook_CreateThread(LPSECURITY_ATTRIBUTES attr, SIZE_T stack_size, LPTHREAD_START_ROUTINE start, LPVOID param,
                                       DWORD flags, LPDWORD thread_id) {
  static LONG count;
  LONG n = next_call_count(&count);
  HANDLE rc = ((probe_CreateThread_fn)g_orig_CreateThread)(attr, stack_size, start, param, flags, thread_id);
  probe_log("call: CreateThread count=%ld start=%p param=%p flags=0x%lx tid=%lu handle=%p err=%lu", (long)n, (void *)start,
            param, (unsigned long)flags, thread_id ? (unsigned long)*thread_id : 0ul, rc, (unsigned long)GetLastError());
  return rc;
}

static int should_log_exception_code(DWORD code) {
  return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
         code == EXCEPTION_STACK_OVERFLOW || code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
         code == EXCEPTION_PRIV_INSTRUCTION;
}

static LONG CALLBACK probe_exception_handler(PEXCEPTION_POINTERS info) {
  DWORD code;
  void *fault_address;
  uintptr_t ip = 0;
  uintptr_t sp = 0;
  uintptr_t bp = 0;
  DWORD_PTR info0 = 0;
  DWORD_PTR info1 = 0;

  if (info == NULL || info->ExceptionRecord == NULL) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  code = info->ExceptionRecord->ExceptionCode;
  if (!should_log_exception_code(code)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (InterlockedCompareExchange(&g_exception_logged, 1, 0) != 0) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  fault_address = info->ExceptionRecord->ExceptionAddress;
  if (info->ExceptionRecord->NumberParameters > 0) {
    info0 = info->ExceptionRecord->ExceptionInformation[0];
  }
  if (info->ExceptionRecord->NumberParameters > 1) {
    info1 = info->ExceptionRecord->ExceptionInformation[1];
  }

#if defined(_M_IX86) || defined(__i386__)
  if (info->ContextRecord != NULL) {
    ip = (uintptr_t)info->ContextRecord->Eip;
    sp = (uintptr_t)info->ContextRecord->Esp;
    bp = (uintptr_t)info->ContextRecord->Ebp;
  }
#endif

  probe_log("exception: code=0x%08lx fault=%p fault_samp_rva=0x%08lx ip=%p ip_samp_rva=0x%08lx sp=%p bp=%p info0=0x%lx info1=%p",
            (unsigned long)code, fault_address, samp_rva_from_address(fault_address), (void *)ip,
            samp_rva_from_address((void *)ip), (void *)sp, (void *)bp, (unsigned long)info0, (void *)info1);
  return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)reserved;

  if (reason == DLL_PROCESS_ATTACH) {
    DWORD thread_id = 0;
    g_self_module = (HMODULE)instance;
    DisableThreadLibraryCalls(instance);
    g_exception_handler = AddVectoredExceptionHandler(1, probe_exception_handler);
    g_stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (g_stop_event != NULL) {
      g_worker_thread = CreateThread(NULL, 0, probe_worker, NULL, 0, &thread_id);
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    if (g_stop_event != NULL) {
      SetEvent(g_stop_event);
    }
    if (g_exception_handler != NULL) {
      RemoveVectoredExceptionHandler(g_exception_handler);
      g_exception_handler = NULL;
    }
  }

  return TRUE;
}
