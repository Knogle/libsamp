#include <windows.h>
#include <wincrypt.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "samp_re/rpc175_logic.h"
#include "samp_re/rpc178_logic.h"

#define SAMP_RE_DISABLED_FLAG "samp_re_disabled.flag"
#define SAMP_RE_PROBE_ACTOR_FLAG "samp_probe_actor_hooks.flag"
#define SAMP_RE_PROBE_ACTOR_HEAVY_FLAG "samp_probe_actor_heavy.flag"
#define SAMP_RE_FUNCTION_RPC175 "rpc175_set_actor_facing_angle"
#define SAMP_RE_FUNCTION_RPC178 "rpc178_set_actor_health"

#define SAMP_RE_R5_TIMESTAMP UINT32_C(0x6372c39e)
#define SAMP_RE_R5_ENTRY_RVA UINT32_C(0x000cbc90)
#define SAMP_RE_R5_IMAGE_BASE UINT32_C(0x10000000)
#define SAMP_RE_R5_IMAGE_SIZE UINT32_C(0x0027e000)
#define SAMP_RE_R5_SECTION_COUNT 5u

#define SAMP_RE_RPC175_RVA UINT32_C(0x0001d9f0)
#define SAMP_RE_RPC175_PATCH_LENGTH 7u
#define SAMP_RE_RPC175_SEH_METADATA_RVA UINT32_C(0x000e167b)
#define SAMP_RE_RPC178_RVA UINT32_C(0x0001dbe0)
#define SAMP_RE_RPC178_PATCH_LENGTH 7u
#define SAMP_RE_RPC178_SEH_METADATA_RVA UINT32_C(0x000e16bb)
#define SAMP_RE_NETGAME_GLOBAL_RVA UINT32_C(0x0026eb94)
#define SAMP_RE_NETGAME_POOLS_OFFSET UINT32_C(0x000003de)
#define SAMP_RE_POOLS_ACTOR_POOL_OFFSET UINT32_C(0x00000010)
#define SAMP_RE_CACTOR_SET_FACING_ANGLE_RVA UINT32_C(0x0009c570)
#define SAMP_RE_CACTOR_SET_FACING_ANGLE_PROLOGUE_LENGTH 6u
#define SAMP_RE_CACTOR_SET_HEALTH_RVA UINT32_C(0x0009c5d0)
#define SAMP_RE_CACTOR_SET_HEALTH_PROLOGUE_LENGTH 5u
#define SAMP_RE_PED_FROM_REF_RVA UINT32_C(0x000b3b70)
#define SAMP_RE_PED_FROM_REF_PROLOGUE_LENGTH 5u
#define SAMP_RE_ANGLE_CONVERSION_RVA UINT32_C(0x000b5970)
#define SAMP_RE_ANGLE_CONVERSION_PROLOGUE_LENGTH 17u
#define SAMP_RE_ANGLE_360_CONSTANT_RVA UINT32_C(0x000e6124)
#define SAMP_RE_ANGLE_ZERO_CONSTANT_RVA UINT32_C(0x000e5940)
#define SAMP_RE_ANGLE_180_CONSTANT_RVA UINT32_C(0x000ed468)
#define SAMP_RE_ANGLE_INVERSE_180_CONSTANT_RVA UINT32_C(0x000ed47c)
#define SAMP_RE_ANGLE_PI_CONSTANT_RVA UINT32_C(0x000ed480)
#define SAMP_RE_CACTOR_GTA_REF_OFFSET UINT32_C(0x00000044)
#define SAMP_RE_CACTOR_PED_OFFSET UINT32_C(0x00000048)
#define SAMP_RE_CPED_HEALTH_OFFSET UINT32_C(0x00000540)
#define SAMP_RE_CPED_HEADING_OFFSET UINT32_C(0x0000055c)

#define SAMP_RE_RPC175_TRAMPOLINE_LENGTH (SAMP_RE_RPC175_PATCH_LENGTH + 5u)
#define SAMP_RE_RPC178_TRAMPOLINE_LENGTH (SAMP_RE_RPC178_PATCH_LENGTH + 5u)
#define SAMP_RE_DISABLED_POLL_MS 250u

typedef enum samp_re_mode {
  SAMP_RE_MODE_BYPASS = 0,
  SAMP_RE_MODE_SHADOW = 1,
  SAMP_RE_MODE_REPLACE = 2
} samp_re_mode;

typedef struct samp_re_rpc_parameters_prefix {
  uint8_t *input;
  int32_t number_of_bits_of_data;
} samp_re_rpc_parameters_prefix;

typedef void(__cdecl *samp_re_rpc_handler_fn)(
    samp_re_rpc_parameters_prefix *parameters);

#if defined(__GNUC__)
typedef void(__attribute__((thiscall)) * samp_re_set_health_raw_fn)(
    void *actor, uint32_t health_bits);
#else
typedef void(__thiscall * samp_re_set_health_raw_fn)(void *actor,
                                                     uint32_t health_bits);
#endif

#if defined(__GNUC__)
typedef void *(__attribute__((stdcall)) * samp_re_ped_from_ref_fn)(
    uint32_t gta_ref);
#else
typedef void *(__stdcall * samp_re_ped_from_ref_fn)(uint32_t gta_ref);
#endif

typedef enum samp_re_actor_lookup_result {
  SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE = -1,
  SAMP_RE_ACTOR_LOOKUP_NOOP = 0,
  SAMP_RE_ACTOR_LOOKUP_FOUND = 1
} samp_re_actor_lookup_result;

_Static_assert(sizeof(void *) == 4u, "samp_re is a Win32-only overlay");
_Static_assert(sizeof(samp_re_rpc_parameters_prefix) == 8u,
               "RPCParameters prefix size must match R5");
_Static_assert(offsetof(samp_re_rpc_parameters_prefix, input) == 0u,
               "RPCParameters::input offset must match R5");
_Static_assert(
    offsetof(samp_re_rpc_parameters_prefix, number_of_bits_of_data) == 4u,
    "RPCParameters::numberOfBitsOfData offset must match R5");

static const uint8_t g_supported_samp_sha256[32] = {
    0xb7u, 0x2bu, 0x5du, 0xbeu, 0x72u, 0x5fu, 0x81u, 0x86u,
    0x4cu, 0xa3u, 0xf7u, 0x8bu, 0xc7u, 0x06u, 0x3bu, 0xdau,
    0x56u, 0xccu, 0x05u, 0xfcu, 0x71u, 0x88u, 0xafu, 0x82u,
    0x2fu, 0xa7u, 0xa7u, 0x54u, 0x43u, 0x25u, 0x53u, 0xa2u};

static const uint8_t
    g_set_health_expected[SAMP_RE_CACTOR_SET_HEALTH_PROLOGUE_LENGTH] = {
        0x8bu, 0x41u, 0x48u, 0x85u, 0xc0u};
static const uint8_t g_set_facing_angle_expected
    [SAMP_RE_CACTOR_SET_FACING_ANGLE_PROLOGUE_LENGTH] = {
        0x56u, 0x8bu, 0xf1u, 0x8bu, 0x46u, 0x48u};
static const uint8_t
    g_ped_from_ref_expected[SAMP_RE_PED_FROM_REF_PROLOGUE_LENGTH] = {
        0x55u, 0x8bu, 0xecu, 0x51u, 0x53u};
static const uint8_t g_angle_conversion_expected
    [SAMP_RE_ANGLE_CONVERSION_PROLOGUE_LENGTH] = {
        0xd9u, 0x44u, 0x24u, 0x04u, 0xd8u, 0x1du, 0x24u, 0x61u,
        0x0eu, 0x10u, 0xdfu, 0xe0u, 0xf6u, 0xc4u, 0x41u, 0x74u,
        0x4eu};

static HMODULE g_self_module;
static uintptr_t g_samp_base;
static samp_re_mode g_mode;
static CRITICAL_SECTION g_log_lock;
static LONG g_log_ready;
static char g_log_path[MAX_PATH];
static LONG g_hook_state;
static LONG g_active_calls;
static LONG g_call_sequence;
static LONG g_rpc175_call_sequence;
static uint8_t *g_hook_target;
static uint8_t g_hook_saved[SAMP_RE_RPC178_PATCH_LENGTH];
static uint8_t g_hook_patch[SAMP_RE_RPC178_PATCH_LENGTH];
static uint8_t *g_rpc175_hook_target;
static uint8_t g_rpc175_hook_saved[SAMP_RE_RPC175_PATCH_LENGTH];
static uint8_t g_rpc175_hook_patch[SAMP_RE_RPC175_PATCH_LENGTH];
static samp_re_rpc_handler_fn g_original_rpc178;
static samp_re_rpc_handler_fn g_original_rpc175;
static samp_re_set_health_raw_fn g_set_actor_health;
static samp_re_ped_from_ref_fn g_ped_from_ref;
static int g_rpc175_hook_installed;
static int g_rpc178_hook_installed;
static char g_disabled_flag_path[MAX_PATH];

static const char *mode_name(samp_re_mode mode) {
  switch (mode) {
  case SAMP_RE_MODE_SHADOW:
    return "shadow";
  case SAMP_RE_MODE_REPLACE:
    return "replace";
  default:
    return "bypass";
  }
}

static int path_beside_self(const char *leaf, char *out_path,
                            size_t out_path_size) {
  DWORD length;
  char *separator;
  size_t directory_length;
  size_t leaf_length;

  if (leaf == NULL || out_path == NULL || out_path_size == 0u ||
      g_self_module == NULL) {
    return 0;
  }

  length = GetModuleFileNameA(g_self_module, out_path, (DWORD)out_path_size);
  if (length == 0u || (size_t)length >= out_path_size) {
    out_path[0] = '\0';
    return 0;
  }

  separator = strrchr(out_path, '\\');
  if (separator == NULL) {
    separator = strrchr(out_path, '/');
  }
  if (separator == NULL) {
    out_path[0] = '\0';
    return 0;
  }

  directory_length = (size_t)(separator + 1 - out_path);
  leaf_length = strlen(leaf);
  if (directory_length > out_path_size - 1u ||
      leaf_length > out_path_size - directory_length - 1u) {
    out_path[0] = '\0';
    return 0;
  }
  memcpy(out_path + directory_length, leaf, leaf_length + 1u);
  return 1;
}

static int append_log_leaf(const char *directory, const char *leaf,
                           char *out_path, size_t out_path_size) {
  size_t directory_length;
  int needs_separator;
  int written;

  if (directory == NULL || leaf == NULL || out_path == NULL ||
      out_path_size == 0u) {
    return 0;
  }
  directory_length = strlen(directory);
  if (directory_length == 0u) {
    return 0;
  }
  needs_separator =
      directory[directory_length - 1u] != '\\' &&
      directory[directory_length - 1u] != '/';
  written = snprintf(out_path, out_path_size, "%s%s%s", directory,
                     needs_separator ? "\\" : "", leaf);
  return written > 0 && (size_t)written < out_path_size;
}

static int log_path_is_writable(const char *path) {
  HANDLE file;

  if (path == NULL || *path == '\0') {
    return 0;
  }
  file = CreateFileA(path, FILE_APPEND_DATA,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    return 0;
  }
  (void)CloseHandle(file);
  return 1;
}

static int initialize_log(void) {
  char log_directory[MAX_PATH];
  DWORD length;
  int ready = 0;

  InitializeCriticalSection(&g_log_lock);
  g_log_path[0] = '\0';
  log_directory[0] = '\0';

  length = GetEnvironmentVariableA("SAMPDLL_LOG_DIR", log_directory,
                                   (DWORD)sizeof(log_directory));
  if (length > 0u && (size_t)length < sizeof(log_directory) &&
      append_log_leaf(log_directory, "samp_re.log", g_log_path,
                      sizeof(g_log_path)) &&
      log_path_is_writable(g_log_path)) {
    ready = 1;
  }
  if (!ready &&
      path_beside_self("samp_re.log", g_log_path, sizeof(g_log_path))) {
    ready = log_path_is_writable(g_log_path);
  }
  InterlockedExchange(&g_log_ready, ready);
  return ready;
}

static void samp_re_log(const char *format, ...) {
  char message[1024];
  char line[1280];
  SYSTEMTIME now;
  HANDLE file;
  DWORD line_length;
  DWORD written = 0u;
  int formatted;
  va_list arguments;

  if (InterlockedCompareExchange(&g_log_ready, 0, 0) == 0 ||
      format == NULL) {
    return;
  }

  va_start(arguments, format);
  (void)vsnprintf(message, sizeof(message), format, arguments);
  va_end(arguments);
  message[sizeof(message) - 1u] = '\0';

  GetLocalTime(&now);
  formatted = snprintf(
      line, sizeof(line),
      "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
      (unsigned)now.wYear, (unsigned)now.wMonth, (unsigned)now.wDay,
      (unsigned)now.wHour, (unsigned)now.wMinute, (unsigned)now.wSecond,
      (unsigned)now.wMilliseconds, message);
  if (formatted <= 0) {
    return;
  }
  line[sizeof(line) - 1u] = '\0';
  line_length = (DWORD)strlen(line);

  EnterCriticalSection(&g_log_lock);
  file = CreateFileA(g_log_path, FILE_APPEND_DATA,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file != INVALID_HANDLE_VALUE) {
    (void)WriteFile(file, line, line_length, &written, NULL);
    (void)CloseHandle(file);
  }
  LeaveCriticalSection(&g_log_lock);
}

static int env_value_is_enabled(const char *value) {
  return value != NULL &&
         (_stricmp(value, "1") == 0 || _stricmp(value, "true") == 0 ||
          _stricmp(value, "yes") == 0 || _stricmp(value, "on") == 0 ||
          _stricmp(value, "enabled") == 0);
}

static int env_flag_enabled(const char *name) {
  const char *value = name != NULL ? getenv(name) : NULL;
  return env_value_is_enabled(value);
}

static samp_re_mode mode_from_environment(int *valid) {
  const char *value = getenv("SAMP_RE_MODE");

  if (valid != NULL) {
    *valid = 1;
  }
  if (value == NULL || *value == '\0' || _stricmp(value, "bypass") == 0) {
    return SAMP_RE_MODE_BYPASS;
  }
  if (_stricmp(value, "shadow") == 0) {
    return SAMP_RE_MODE_SHADOW;
  }
  if (_stricmp(value, "replace") == 0) {
    return SAMP_RE_MODE_REPLACE;
  }
  if (valid != NULL) {
    *valid = 0;
  }
  return SAMP_RE_MODE_BYPASS;
}

static int is_function_separator(char value) {
  return value == ',' || value == ';' || value == ' ' || value == '\t' ||
         value == '\r' || value == '\n';
}

static int function_enabled(const char *function_name) {
  const char *list = getenv("SAMP_RE_FUNCTIONS");
  size_t expected_length;

  if (list == NULL || function_name == NULL) {
    return 0;
  }
  expected_length = strlen(function_name);
  while (*list != '\0') {
    const char *token;
    size_t token_length;

    while (*list != '\0' && is_function_separator(*list)) {
      ++list;
    }
    token = list;
    while (*list != '\0' && !is_function_separator(*list)) {
      ++list;
    }
    token_length = (size_t)(list - token);
    if (token_length == expected_length &&
        _strnicmp(token, function_name, expected_length) == 0) {
      return 1;
    }
  }
  return 0;
}

static int file_exists(const char *path) {
  DWORD attributes;
  if (path == NULL || *path == '\0') {
    return 0;
  }
  attributes = GetFileAttributesA(path);
  return attributes != INVALID_FILE_ATTRIBUTES;
}

static int flag_beside_self_exists(const char *leaf, char *resolved,
                                   size_t resolved_size) {
  char local_path[MAX_PATH];
  char *path = resolved != NULL ? resolved : local_path;
  size_t path_size = resolved != NULL ? resolved_size : sizeof(local_path);

  if (!path_beside_self(leaf, path, path_size)) {
    return 0;
  }
  return file_exists(path);
}

static int memory_is_readable(uintptr_t address, size_t size) {
  uintptr_t cursor;
  uintptr_t end;

  if (address == 0u || size == 0u || address > UINTPTR_MAX - size) {
    return 0;
  }
  cursor = address;
  end = address + size;
  while (cursor < end) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t region_end;

    if (VirtualQuery((const void *)cursor, &info, sizeof(info)) !=
        sizeof(info)) {
      return 0;
    }
    DWORD access = info.Protect & 0xffu;
    if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0u ||
        (access != PAGE_READONLY && access != PAGE_READWRITE &&
         access != PAGE_WRITECOPY && access != PAGE_EXECUTE_READ &&
         access != PAGE_EXECUTE_READWRITE &&
         access != PAGE_EXECUTE_WRITECOPY)) {
      return 0;
    }
    if ((uintptr_t)info.BaseAddress > UINTPTR_MAX - info.RegionSize) {
      return 0;
    }
    region_end = (uintptr_t)info.BaseAddress + info.RegionSize;
    if (region_end <= cursor) {
      return 0;
    }
    cursor = region_end < end ? region_end : end;
  }
  return 1;
}

static int memory_is_writable(uintptr_t address, size_t size) {
  uintptr_t cursor;
  uintptr_t end;

  if (address == 0u || size == 0u || address > UINTPTR_MAX - size) {
    return 0;
  }
  cursor = address;
  end = address + size;
  while (cursor < end) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t region_end;
    DWORD access;

    if (VirtualQuery((const void *)cursor, &info, sizeof(info)) !=
        sizeof(info)) {
      return 0;
    }
    access = info.Protect & 0xffu;
    if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0u ||
        (access != PAGE_READWRITE && access != PAGE_WRITECOPY &&
         access != PAGE_EXECUTE_READWRITE &&
         access != PAGE_EXECUTE_WRITECOPY)) {
      return 0;
    }
    if ((uintptr_t)info.BaseAddress > UINTPTR_MAX - info.RegionSize) {
      return 0;
    }
    region_end = (uintptr_t)info.BaseAddress + info.RegionSize;
    if (region_end <= cursor) {
      return 0;
    }
    cursor = region_end < end ? region_end : end;
  }
  return 1;
}

static int checked_add(uintptr_t base, uint32_t offset,
                       uintptr_t *out_address) {
  if (out_address == NULL || base > UINTPTR_MAX - (uintptr_t)offset) {
    return 0;
  }
  *out_address = base + (uintptr_t)offset;
  return 1;
}

static int read_u32(uintptr_t address, uint32_t *out_value) {
  if (out_value == NULL || !memory_is_readable(address, sizeof(*out_value))) {
    return 0;
  }
  memcpy(out_value, (const void *)address, sizeof(*out_value));
  return 1;
}

static int read_actor_health_bits(const void *actor, uint32_t *out_value) {
  uintptr_t address;
  uint32_t ped_value;

  if (actor == NULL || out_value == NULL ||
      !checked_add((uintptr_t)actor, SAMP_RE_CACTOR_PED_OFFSET, &address) ||
      !read_u32(address, &ped_value) || ped_value == 0u ||
      !checked_add((uintptr_t)ped_value, SAMP_RE_CPED_HEALTH_OFFSET,
                   &address)) {
    return 0;
  }
  return read_u32(address, out_value);
}

static int read_actor_heading_bits(const void *actor, uint32_t *out_value) {
  uintptr_t address;
  uint32_t ped_value;

  if (actor == NULL || out_value == NULL ||
      !checked_add((uintptr_t)actor, SAMP_RE_CACTOR_PED_OFFSET, &address) ||
      !read_u32(address, &ped_value) || ped_value == 0u ||
      !checked_add((uintptr_t)ped_value, SAMP_RE_CPED_HEADING_OFFSET,
                   &address)) {
    return 0;
  }
  return read_u32(address, out_value);
}

static PIMAGE_NT_HEADERS32 get_nt_headers(HMODULE module) {
  uintptr_t base = (uintptr_t)module;
  IMAGE_DOS_HEADER dos;
  uintptr_t nt_address;
  PIMAGE_NT_HEADERS32 nt;

  if (!memory_is_readable(base, sizeof(dos))) {
    return NULL;
  }
  memcpy(&dos, (const void *)base, sizeof(dos));
  if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
      !checked_add(base, (uint32_t)dos.e_lfanew, &nt_address) ||
      !memory_is_readable(nt_address, sizeof(IMAGE_NT_HEADERS32))) {
    return NULL;
  }
  nt = (PIMAGE_NT_HEADERS32)nt_address;
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
    return NULL;
  }
  return nt;
}

static void bytes_to_hex(const uint8_t *bytes, size_t byte_count, char *output,
                         size_t output_size) {
  static const char digits[] = "0123456789abcdef";
  size_t index;

  if (output == NULL || output_size == 0u) {
    return;
  }
  if (bytes == NULL || byte_count > (output_size - 1u) / 2u) {
    output[0] = '\0';
    return;
  }
  for (index = 0u; index < byte_count; ++index) {
    output[index * 2u] = digits[bytes[index] >> 4u];
    output[index * 2u + 1u] = digits[bytes[index] & 0x0fu];
  }
  output[byte_count * 2u] = '\0';
}

static int sha256_module_file(HMODULE module, uint8_t output[32],
                              char *module_path, size_t module_path_size,
                              DWORD *out_error) {
  HCRYPTPROV provider = 0;
  HCRYPTHASH hash = 0;
  HANDLE file = INVALID_HANDLE_VALUE;
  uint8_t buffer[8192];
  DWORD bytes_read = 0u;
  DWORD hash_size = 32u;
  DWORD error = ERROR_SUCCESS;
  DWORD path_length;
  int ok = 0;

  if (output == NULL || module_path == NULL || module_path_size == 0u) {
    if (out_error != NULL) {
      *out_error = ERROR_INVALID_PARAMETER;
    }
    return 0;
  }

  path_length =
      GetModuleFileNameA(module, module_path, (DWORD)module_path_size);
  if (path_length == 0u || (size_t)path_length >= module_path_size) {
    error = GetLastError();
    goto done;
  }
  file = CreateFileA(module_path, GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    error = GetLastError();
    goto done;
  }
  if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_AES,
                            CRYPT_VERIFYCONTEXT)) {
    error = GetLastError();
    goto done;
  }
  if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
    error = GetLastError();
    goto done;
  }
  for (;;) {
    if (!ReadFile(file, buffer, (DWORD)sizeof(buffer), &bytes_read, NULL)) {
      error = GetLastError();
      goto done;
    }
    if (bytes_read == 0u) {
      break;
    }
    if (!CryptHashData(hash, buffer, bytes_read, 0)) {
      error = GetLastError();
      goto done;
    }
  }
  if (!CryptGetHashParam(hash, HP_HASHVAL, output, &hash_size, 0) ||
      hash_size != 32u) {
    error = GetLastError();
    goto done;
  }
  ok = 1;

done:
  if (hash != 0) {
    (void)CryptDestroyHash(hash);
  }
  if (provider != 0) {
    (void)CryptReleaseContext(provider, 0);
  }
  if (file != INVALID_HANDLE_VALUE) {
    (void)CloseHandle(file);
  }
  if (out_error != NULL) {
    *out_error = error;
  }
  return ok;
}

static int verify_r5_identity(HMODULE samp_module) {
  PIMAGE_NT_HEADERS32 nt = get_nt_headers(samp_module);
  uint8_t actual_hash[32];
  char actual_hash_hex[65];
  char module_path[MAX_PATH];
  DWORD hash_error = ERROR_SUCCESS;

  memset(actual_hash, 0, sizeof(actual_hash));
  actual_hash_hex[0] = '\0';
  module_path[0] = '\0';

  if (!sha256_module_file(samp_module, actual_hash, module_path,
                          sizeof(module_path), &hash_error)) {
    samp_re_log("identity result=reject reason=sha256_failed error=%lu",
                (unsigned long)hash_error);
    return 0;
  }
  bytes_to_hex(actual_hash, sizeof(actual_hash), actual_hash_hex,
               sizeof(actual_hash_hex));
  if (memcmp(actual_hash, g_supported_samp_sha256, sizeof(actual_hash)) != 0) {
    samp_re_log(
        "identity result=reject reason=sha256 actual=%s supported="
        "b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2",
        actual_hash_hex);
    return 0;
  }
  if (nt == NULL) {
    samp_re_log("identity result=reject reason=invalid_pe sha256=%s",
                actual_hash_hex);
    return 0;
  }
  if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
      nt->FileHeader.NumberOfSections != SAMP_RE_R5_SECTION_COUNT ||
      nt->FileHeader.TimeDateStamp != SAMP_RE_R5_TIMESTAMP ||
      nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER32) ||
      nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
      nt->OptionalHeader.AddressOfEntryPoint != SAMP_RE_R5_ENTRY_RVA ||
      nt->OptionalHeader.ImageBase != SAMP_RE_R5_IMAGE_BASE ||
      nt->OptionalHeader.SizeOfImage != SAMP_RE_R5_IMAGE_SIZE) {
    samp_re_log(
        "identity result=reject reason=pe machine=0x%04x sections=%u "
        "timestamp=0x%08lx optional_size=0x%04x magic=0x%04x entry=0x%08lx "
        "preferred_base=0x%08lx image_size=0x%08lx sha256=%s",
        (unsigned)nt->FileHeader.Machine,
        (unsigned)nt->FileHeader.NumberOfSections,
        (unsigned long)nt->FileHeader.TimeDateStamp,
        (unsigned)nt->FileHeader.SizeOfOptionalHeader,
        (unsigned)nt->OptionalHeader.Magic,
        (unsigned long)nt->OptionalHeader.AddressOfEntryPoint,
        (unsigned long)nt->OptionalHeader.ImageBase,
        (unsigned long)nt->OptionalHeader.SizeOfImage, actual_hash_hex);
    return 0;
  }

  samp_re_log(
      "identity result=accept module=%p path=\"%s\" sha256=%s "
      "timestamp=0x%08lx entry=0x%08lx image_size=0x%08lx "
      "evidence=STATIC_037",
      (void *)samp_module, module_path, actual_hash_hex,
      (unsigned long)nt->FileHeader.TimeDateStamp,
      (unsigned long)nt->OptionalHeader.AddressOfEntryPoint,
      (unsigned long)nt->OptionalHeader.SizeOfImage);
  return 1;
}

static int pin_overlay_and_samp(HMODULE samp_module) {
  HMODULE pinned_self = NULL;
  HMODULE pinned_samp = NULL;

  if (!GetModuleHandleExA(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
              GET_MODULE_HANDLE_EX_FLAG_PIN,
          (LPCSTR)(const void *)&g_self_module, &pinned_self)) {
    samp_re_log("pin result=reject module=self error=%lu",
                (unsigned long)GetLastError());
    return 0;
  }
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, "samp.dll",
                          &pinned_samp) ||
      pinned_samp != samp_module) {
    samp_re_log("pin result=reject module=samp error=%lu actual=%p expected=%p",
                (unsigned long)GetLastError(), (void *)pinned_samp,
                (void *)samp_module);
    return 0;
  }
  samp_re_log("pin result=accept self=%p samp=%p", (void *)pinned_self,
              (void *)pinned_samp);
  return 1;
}

static int actor_probe_conflict_present(void) {
  char path[MAX_PATH];

  if (env_flag_enabled("SAMP_PROBE_ACTOR_HOOKS")) {
    samp_re_log(
        "conflict result=reject source=env name=SAMP_PROBE_ACTOR_HOOKS");
    return 1;
  }
  if (env_flag_enabled("SAMP_PROBE_ACTOR_HEAVY")) {
    samp_re_log(
        "conflict result=reject source=env name=SAMP_PROBE_ACTOR_HEAVY");
    return 1;
  }
  if (flag_beside_self_exists(SAMP_RE_PROBE_ACTOR_FLAG, path, sizeof(path))) {
    samp_re_log("conflict result=reject source=flag path=\"%s\"", path);
    return 1;
  }
  if (flag_beside_self_exists(SAMP_RE_PROBE_ACTOR_HEAVY_FLAG, path,
                              sizeof(path))) {
    samp_re_log("conflict result=reject source=flag path=\"%s\"", path);
    return 1;
  }
  return 0;
}

static int verify_rpc175_targets(void) {
  uint8_t expected_rpc175[SAMP_RE_RPC175_PATCH_LENGTH] = {0x6au, 0xffu,
                                                          0x68u};
  uint8_t expected_conversion[SAMP_RE_ANGLE_CONVERSION_PROLOGUE_LENGTH];
  uint32_t seh_runtime_absolute;
  uint32_t angle_360_runtime_absolute;
  uint32_t constant_value = 0u;
  uintptr_t target_address;
  uintptr_t set_facing_address;
  uintptr_t ped_from_ref_address;
  uintptr_t conversion_address;
  uintptr_t constant_address;
  char actual_hex[2u * SAMP_RE_ANGLE_CONVERSION_PROLOGUE_LENGTH + 1u];

  if (!checked_add(g_samp_base, SAMP_RE_RPC175_RVA, &target_address) ||
      !checked_add(g_samp_base, SAMP_RE_CACTOR_SET_FACING_ANGLE_RVA,
                   &set_facing_address) ||
      !checked_add(g_samp_base, SAMP_RE_PED_FROM_REF_RVA,
                   &ped_from_ref_address) ||
      !checked_add(g_samp_base, SAMP_RE_ANGLE_CONVERSION_RVA,
                   &conversion_address) ||
      g_samp_base >
          (uintptr_t)(UINT32_MAX - SAMP_RE_RPC175_SEH_METADATA_RVA) ||
      g_samp_base >
          (uintptr_t)(UINT32_MAX - SAMP_RE_ANGLE_360_CONSTANT_RVA)) {
    samp_re_log(
        "preflight result=reject function=rpc175_set_actor_facing_angle "
        "reason=address_overflow");
    return 0;
  }
  if (!memory_is_readable(target_address, SAMP_RE_RPC175_PATCH_LENGTH) ||
      !memory_is_readable(
          set_facing_address,
          SAMP_RE_CACTOR_SET_FACING_ANGLE_PROLOGUE_LENGTH) ||
      !memory_is_readable(ped_from_ref_address,
                          SAMP_RE_PED_FROM_REF_PROLOGUE_LENGTH) ||
      !memory_is_readable(conversion_address,
                          SAMP_RE_ANGLE_CONVERSION_PROLOGUE_LENGTH)) {
    samp_re_log(
        "preflight result=reject function=rpc175_set_actor_facing_angle "
        "reason=target_unreadable rpc=%p set_facing=%p ped_from_ref=%p "
        "conversion=%p",
        (void *)target_address, (void *)set_facing_address,
        (void *)ped_from_ref_address, (void *)conversion_address);
    return 0;
  }

  seh_runtime_absolute =
      (uint32_t)(g_samp_base + SAMP_RE_RPC175_SEH_METADATA_RVA);
  memcpy(expected_rpc175 + 3u, &seh_runtime_absolute,
         sizeof(seh_runtime_absolute));
  if (memcmp((const void *)target_address, expected_rpc175,
             sizeof(expected_rpc175)) != 0) {
    bytes_to_hex((const uint8_t *)target_address, sizeof(expected_rpc175),
                 actual_hex, sizeof(actual_hex));
    samp_re_log(
        "preflight result=reject function=rpc175_set_actor_facing_angle "
        "reason=prologue_or_hook_conflict rva=0x%08lx actual=%s "
        "expected_relocated_seh=0x%08lx",
        (unsigned long)SAMP_RE_RPC175_RVA, actual_hex,
        (unsigned long)seh_runtime_absolute);
    return 0;
  }
  if (memcmp((const void *)set_facing_address, g_set_facing_angle_expected,
             sizeof(g_set_facing_angle_expected)) != 0) {
    bytes_to_hex((const uint8_t *)set_facing_address,
                 sizeof(g_set_facing_angle_expected), actual_hex,
                 sizeof(actual_hex));
    samp_re_log(
        "preflight result=reject function=cactor_set_facing_angle "
        "reason=prologue_or_hook_conflict rva=0x%08lx actual=%s",
        (unsigned long)SAMP_RE_CACTOR_SET_FACING_ANGLE_RVA, actual_hex);
    return 0;
  }
  if (memcmp((const void *)ped_from_ref_address, g_ped_from_ref_expected,
             sizeof(g_ped_from_ref_expected)) != 0) {
    bytes_to_hex((const uint8_t *)ped_from_ref_address,
                 sizeof(g_ped_from_ref_expected), actual_hex,
                 sizeof(actual_hex));
    samp_re_log(
        "preflight result=reject function=ped_from_ref "
        "reason=prologue_or_hook_conflict rva=0x%08lx actual=%s",
        (unsigned long)SAMP_RE_PED_FROM_REF_RVA, actual_hex);
    return 0;
  }

  memcpy(expected_conversion, g_angle_conversion_expected,
         sizeof(expected_conversion));
  angle_360_runtime_absolute =
      (uint32_t)(g_samp_base + SAMP_RE_ANGLE_360_CONSTANT_RVA);
  memcpy(expected_conversion + 6u, &angle_360_runtime_absolute,
         sizeof(angle_360_runtime_absolute));
  if (memcmp((const void *)conversion_address, expected_conversion,
             sizeof(expected_conversion)) != 0) {
    bytes_to_hex((const uint8_t *)conversion_address,
                 sizeof(expected_conversion), actual_hex,
                 sizeof(actual_hex));
    samp_re_log(
        "preflight result=reject function=angle_conversion "
        "reason=prologue_or_hook_conflict rva=0x%08lx actual=%s",
        (unsigned long)SAMP_RE_ANGLE_CONVERSION_RVA, actual_hex);
    return 0;
  }

#define VERIFY_RPC175_CONSTANT(rva, expected)                                  \
  do {                                                                          \
    constant_value = 0u;                                                        \
    if (!checked_add(g_samp_base, (rva), &constant_address) ||                  \
        !read_u32(constant_address, &constant_value) ||                         \
        constant_value != (expected)) {                                         \
      samp_re_log(                                                               \
          "preflight result=reject function=angle_conversion "                  \
          "reason=constant_mismatch rva=0x%08lx actual=0x%08lx "                \
          "expected=0x%08lx",                                                   \
          (unsigned long)(rva), (unsigned long)constant_value,                  \
          (unsigned long)(expected));                                           \
      return 0;                                                                 \
    }                                                                           \
  } while (0)

  VERIFY_RPC175_CONSTANT(SAMP_RE_ANGLE_360_CONSTANT_RVA,
                         UINT32_C(0x43b40000));
  VERIFY_RPC175_CONSTANT(SAMP_RE_ANGLE_ZERO_CONSTANT_RVA,
                         UINT32_C(0x00000000));
  VERIFY_RPC175_CONSTANT(SAMP_RE_ANGLE_180_CONSTANT_RVA,
                         UINT32_C(0x43340000));
  VERIFY_RPC175_CONSTANT(SAMP_RE_ANGLE_INVERSE_180_CONSTANT_RVA,
                         UINT32_C(0x3bb60b61));
  VERIFY_RPC175_CONSTANT(SAMP_RE_ANGLE_PI_CONSTANT_RVA,
                         UINT32_C(0x40490fdb));
#undef VERIFY_RPC175_CONSTANT

  g_rpc175_hook_target = (uint8_t *)target_address;
  memcpy(g_rpc175_hook_saved, expected_rpc175,
         sizeof(g_rpc175_hook_saved));
  g_ped_from_ref = (samp_re_ped_from_ref_fn)(void *)ped_from_ref_address;
  samp_re_log(
      "preflight result=accept rpc175_rva=0x%08lx "
      "set_facing_rva=0x%08lx ped_from_ref_rva=0x%08lx "
      "conversion_rva=0x%08lx cactor_gta_ref=0x%02lx "
      "cactor_ped=0x%02lx cped_heading=0x%03lx patch_len=%u "
      "seh_runtime=0x%08lx evidence=STATIC_037",
      (unsigned long)SAMP_RE_RPC175_RVA,
      (unsigned long)SAMP_RE_CACTOR_SET_FACING_ANGLE_RVA,
      (unsigned long)SAMP_RE_PED_FROM_REF_RVA,
      (unsigned long)SAMP_RE_ANGLE_CONVERSION_RVA,
      (unsigned long)SAMP_RE_CACTOR_GTA_REF_OFFSET,
      (unsigned long)SAMP_RE_CACTOR_PED_OFFSET,
      (unsigned long)SAMP_RE_CPED_HEADING_OFFSET,
      (unsigned)SAMP_RE_RPC175_PATCH_LENGTH,
      (unsigned long)seh_runtime_absolute);
  return 1;
}

static int verify_rpc178_targets(void) {
  uint8_t expected_rpc178[SAMP_RE_RPC178_PATCH_LENGTH] = {0x6au, 0xffu,
                                                          0x68u};
  uint32_t seh_runtime_absolute;
  uintptr_t target_address;
  uintptr_t downstream_address;
  char actual_hex[2u * SAMP_RE_RPC178_PATCH_LENGTH + 1u];

  if (!checked_add(g_samp_base, SAMP_RE_RPC178_RVA, &target_address) ||
      !checked_add(g_samp_base, SAMP_RE_CACTOR_SET_HEALTH_RVA,
                   &downstream_address) ||
      g_samp_base >
          (uintptr_t)(UINT32_MAX - SAMP_RE_RPC178_SEH_METADATA_RVA)) {
    samp_re_log("preflight result=reject reason=address_overflow");
    return 0;
  }
  if (!memory_is_readable(target_address, SAMP_RE_RPC178_PATCH_LENGTH) ||
      !memory_is_readable(downstream_address,
                          SAMP_RE_CACTOR_SET_HEALTH_PROLOGUE_LENGTH)) {
    samp_re_log("preflight result=reject reason=target_unreadable rpc=%p "
                "downstream=%p",
                (void *)target_address, (void *)downstream_address);
    return 0;
  }

  seh_runtime_absolute =
      (uint32_t)(g_samp_base + SAMP_RE_RPC178_SEH_METADATA_RVA);
  memcpy(expected_rpc178 + 3u, &seh_runtime_absolute,
         sizeof(seh_runtime_absolute));
  if (memcmp((const void *)target_address, expected_rpc178,
             sizeof(expected_rpc178)) != 0) {
    bytes_to_hex((const uint8_t *)target_address, sizeof(expected_rpc178),
                 actual_hex, sizeof(actual_hex));
    samp_re_log(
        "preflight result=reject function=rpc178_set_actor_health "
        "reason=prologue_or_hook_conflict rva=0x%08lx actual=%s "
        "expected_relocated_seh=0x%08lx",
        (unsigned long)SAMP_RE_RPC178_RVA, actual_hex,
        (unsigned long)seh_runtime_absolute);
    return 0;
  }
  if (memcmp((const void *)downstream_address, g_set_health_expected,
             sizeof(g_set_health_expected)) != 0) {
    char downstream_hex[2u * SAMP_RE_CACTOR_SET_HEALTH_PROLOGUE_LENGTH + 1u];
    bytes_to_hex((const uint8_t *)downstream_address,
                 SAMP_RE_CACTOR_SET_HEALTH_PROLOGUE_LENGTH, downstream_hex,
                 sizeof(downstream_hex));
    samp_re_log(
        "preflight result=reject function=cactor_set_health "
        "reason=prologue_or_hook_conflict rva=0x%08lx actual=%s",
        (unsigned long)SAMP_RE_CACTOR_SET_HEALTH_RVA, downstream_hex);
    return 0;
  }

  g_hook_target = (uint8_t *)target_address;
  memcpy(g_hook_saved, expected_rpc178, sizeof(g_hook_saved));
  g_set_actor_health = (samp_re_set_health_raw_fn)downstream_address;
  samp_re_log(
      "preflight result=accept rpc178_rva=0x%08lx downstream_rva=0x%08lx "
      "patch_len=%u seh_runtime=0x%08lx evidence=STATIC_037,PROBE_TRACE",
      (unsigned long)SAMP_RE_RPC178_RVA,
      (unsigned long)SAMP_RE_CACTOR_SET_HEALTH_RVA,
      (unsigned)SAMP_RE_RPC178_PATCH_LENGTH,
      (unsigned long)seh_runtime_absolute);
  return 1;
}

static void __cdecl hook_rpc178_set_actor_health(
    samp_re_rpc_parameters_prefix *parameters);

static int create_rpc178_trampoline(void) {
  uint8_t *trampoline;
  uint32_t relative_back;
  DWORD old_protect = 0u;

  trampoline = (uint8_t *)VirtualAlloc(
      NULL, SAMP_RE_RPC178_TRAMPOLINE_LENGTH, MEM_COMMIT | MEM_RESERVE,
      PAGE_EXECUTE_READWRITE);
  if (trampoline == NULL) {
    samp_re_log("hook result=reject stage=trampoline_alloc error=%lu",
                (unsigned long)GetLastError());
    return 0;
  }

  memcpy(trampoline, g_hook_saved, SAMP_RE_RPC178_PATCH_LENGTH);
  trampoline[SAMP_RE_RPC178_PATCH_LENGTH] = 0xe9u;
  relative_back =
      (uint32_t)((uintptr_t)g_hook_target + SAMP_RE_RPC178_PATCH_LENGTH -
                 ((uintptr_t)trampoline +
                  SAMP_RE_RPC178_TRAMPOLINE_LENGTH));
  memcpy(trampoline + SAMP_RE_RPC178_PATCH_LENGTH + 1u, &relative_back,
         sizeof(relative_back));
  if (!FlushInstructionCache(GetCurrentProcess(), trampoline,
                             SAMP_RE_RPC178_TRAMPOLINE_LENGTH) ||
      !VirtualProtect(trampoline, SAMP_RE_RPC178_TRAMPOLINE_LENGTH,
                      PAGE_EXECUTE_READ, &old_protect)) {
    DWORD error = GetLastError();
    (void)VirtualFree(trampoline, 0u, MEM_RELEASE);
    samp_re_log("hook result=reject stage=trampoline_publish error=%lu",
                (unsigned long)error);
    return 0;
  }

  g_original_rpc178 = (samp_re_rpc_handler_fn)(void *)trampoline;
  return 1;
}

static int rollback_hook_before_publication(DWORD original_protect,
                                            const char *reason) {
  DWORD ignored_protect = 0u;
  int restored;
  int protection_restored;

  if (memcmp(g_hook_target, g_hook_patch, sizeof(g_hook_patch)) != 0) {
    samp_re_log(
        "hook result=ownership_lost stage=pre_publication_rollback "
        "reason=%s action=leave_foreign_bytes",
        reason != NULL ? reason : "unknown");
    return 0;
  }
  memcpy(g_hook_target, g_hook_saved, sizeof(g_hook_saved));
  (void)FlushInstructionCache(GetCurrentProcess(), g_hook_target,
                              sizeof(g_hook_saved));
  restored =
      memcmp(g_hook_target, g_hook_saved, sizeof(g_hook_saved)) == 0;
  protection_restored =
      VirtualProtect(g_hook_target, sizeof(g_hook_saved), original_protect,
                     &ignored_protect) != FALSE;
  samp_re_log(
      "hook result=rollback stage=pre_publication reason=%s bytes_restored=%d "
      "protection_restored=%d",
      reason != NULL ? reason : "unknown", restored, protection_restored);
  return restored && protection_restored;
}

static int install_rpc178_hook(void) {
  DWORD original_protect = 0u;
  DWORD ignored_protect = 0u;
  uint32_t relative_hook;
  int restore_protection_ok;

  if (g_hook_target == NULL || !create_rpc178_trampoline()) {
    return 0;
  }

  g_hook_patch[0] = 0xe9u;
  relative_hook =
      (uint32_t)((uintptr_t)(void *)&hook_rpc178_set_actor_health -
                 ((uintptr_t)g_hook_target + 5u));
  memcpy(g_hook_patch + 1u, &relative_hook, sizeof(relative_hook));
  g_hook_patch[5] = 0x90u;
  g_hook_patch[6] = 0x90u;

  if (!VirtualProtect(g_hook_target, sizeof(g_hook_saved),
                      PAGE_EXECUTE_READWRITE, &original_protect)) {
    samp_re_log("hook result=reject stage=target_unprotect error=%lu",
                (unsigned long)GetLastError());
    return 0;
  }
  if (memcmp(g_hook_target, g_hook_saved, sizeof(g_hook_saved)) != 0) {
    (void)VirtualProtect(g_hook_target, sizeof(g_hook_saved),
                         original_protect, &ignored_protect);
    samp_re_log(
        "hook result=reject stage=prewrite reason=ownership_changed");
    return 0;
  }

  memcpy(g_hook_target, g_hook_patch, sizeof(g_hook_patch));
  if (!FlushInstructionCache(GetCurrentProcess(), g_hook_target,
                             sizeof(g_hook_patch))) {
    (void)rollback_hook_before_publication(original_protect,
                                           "instruction_cache_flush");
    return 0;
  }
  if (memcmp(g_hook_target, g_hook_patch, sizeof(g_hook_patch)) != 0) {
    (void)VirtualProtect(g_hook_target, sizeof(g_hook_patch),
                         original_protect, &ignored_protect);
    samp_re_log(
        "hook result=ownership_lost stage=postwrite_verify "
        "action=leave_foreign_bytes");
    return 0;
  }

  restore_protection_ok =
      VirtualProtect(g_hook_target, sizeof(g_hook_patch), original_protect,
                     &ignored_protect) != FALSE;
  if (!restore_protection_ok) {
    (void)rollback_hook_before_publication(original_protect,
                                           "restore_protection");
    return 0;
  }
  if (memcmp(g_hook_target, g_hook_patch, sizeof(g_hook_patch)) != 0) {
    samp_re_log(
        "hook result=ownership_lost stage=postprotect_verify "
        "action=leave_foreign_bytes");
    return 0;
  }

  g_rpc178_hook_installed = 1;
  samp_re_log(
      "hook result=installed function=rpc178_set_actor_health "
      "target=%p trampoline=%p patch=e9_rel32_9090 patch_len=%u mode=%s "
      "evidence=STATIC_037,TODO_VERIFY",
      (void *)g_hook_target, (void *)g_original_rpc178,
      (unsigned)SAMP_RE_RPC178_PATCH_LENGTH, mode_name(g_mode));
  return 1;
}

static void __cdecl hook_rpc175_set_actor_facing_angle(
    samp_re_rpc_parameters_prefix *parameters);

static int create_rpc175_trampoline(void) {
  uint8_t *trampoline;
  uint32_t relative_back;
  DWORD old_protect = 0u;

  trampoline = (uint8_t *)VirtualAlloc(
      NULL, SAMP_RE_RPC175_TRAMPOLINE_LENGTH, MEM_COMMIT | MEM_RESERVE,
      PAGE_EXECUTE_READWRITE);
  if (trampoline == NULL) {
    samp_re_log(
        "hook result=reject function=rpc175_set_actor_facing_angle "
        "stage=trampoline_alloc error=%lu",
        (unsigned long)GetLastError());
    return 0;
  }

  memcpy(trampoline, g_rpc175_hook_saved, SAMP_RE_RPC175_PATCH_LENGTH);
  trampoline[SAMP_RE_RPC175_PATCH_LENGTH] = 0xe9u;
  relative_back =
      (uint32_t)((uintptr_t)g_rpc175_hook_target +
                     SAMP_RE_RPC175_PATCH_LENGTH -
                 ((uintptr_t)trampoline +
                  SAMP_RE_RPC175_TRAMPOLINE_LENGTH));
  memcpy(trampoline + SAMP_RE_RPC175_PATCH_LENGTH + 1u, &relative_back,
         sizeof(relative_back));
  if (!FlushInstructionCache(GetCurrentProcess(), trampoline,
                             SAMP_RE_RPC175_TRAMPOLINE_LENGTH) ||
      !VirtualProtect(trampoline, SAMP_RE_RPC175_TRAMPOLINE_LENGTH,
                      PAGE_EXECUTE_READ, &old_protect)) {
    DWORD error = GetLastError();
    (void)VirtualFree(trampoline, 0u, MEM_RELEASE);
    samp_re_log(
        "hook result=reject function=rpc175_set_actor_facing_angle "
        "stage=trampoline_publish error=%lu",
        (unsigned long)error);
    return 0;
  }

  g_original_rpc175 = (samp_re_rpc_handler_fn)(void *)trampoline;
  return 1;
}

static int rollback_rpc175_hook_before_publication(DWORD original_protect,
                                                   const char *reason) {
  DWORD ignored_protect = 0u;
  int restored;
  int protection_restored;

  if (memcmp(g_rpc175_hook_target, g_rpc175_hook_patch,
             sizeof(g_rpc175_hook_patch)) != 0) {
    samp_re_log(
        "hook result=ownership_lost function=rpc175_set_actor_facing_angle "
        "stage=pre_publication_rollback reason=%s "
        "action=leave_foreign_bytes",
        reason != NULL ? reason : "unknown");
    return 0;
  }
  memcpy(g_rpc175_hook_target, g_rpc175_hook_saved,
         sizeof(g_rpc175_hook_saved));
  (void)FlushInstructionCache(GetCurrentProcess(), g_rpc175_hook_target,
                              sizeof(g_rpc175_hook_saved));
  restored = memcmp(g_rpc175_hook_target, g_rpc175_hook_saved,
                    sizeof(g_rpc175_hook_saved)) == 0;
  protection_restored =
      VirtualProtect(g_rpc175_hook_target, sizeof(g_rpc175_hook_saved),
                     original_protect, &ignored_protect) != FALSE;
  samp_re_log(
      "hook result=rollback function=rpc175_set_actor_facing_angle "
      "stage=pre_publication reason=%s bytes_restored=%d "
      "protection_restored=%d",
      reason != NULL ? reason : "unknown", restored, protection_restored);
  return restored && protection_restored;
}

static int install_rpc175_hook(void) {
  DWORD original_protect = 0u;
  DWORD ignored_protect = 0u;
  uint32_t relative_hook;
  int restore_protection_ok;

  if (g_rpc175_hook_target == NULL || !create_rpc175_trampoline()) {
    return 0;
  }

  g_rpc175_hook_patch[0] = 0xe9u;
  relative_hook =
      (uint32_t)((uintptr_t)(void *)&hook_rpc175_set_actor_facing_angle -
                 ((uintptr_t)g_rpc175_hook_target + 5u));
  memcpy(g_rpc175_hook_patch + 1u, &relative_hook, sizeof(relative_hook));
  g_rpc175_hook_patch[5] = 0x90u;
  g_rpc175_hook_patch[6] = 0x90u;

  if (!VirtualProtect(g_rpc175_hook_target, sizeof(g_rpc175_hook_saved),
                      PAGE_EXECUTE_READWRITE, &original_protect)) {
    samp_re_log(
        "hook result=reject function=rpc175_set_actor_facing_angle "
        "stage=target_unprotect error=%lu",
        (unsigned long)GetLastError());
    return 0;
  }
  if (memcmp(g_rpc175_hook_target, g_rpc175_hook_saved,
             sizeof(g_rpc175_hook_saved)) != 0) {
    (void)VirtualProtect(g_rpc175_hook_target,
                         sizeof(g_rpc175_hook_saved), original_protect,
                         &ignored_protect);
    samp_re_log(
        "hook result=reject function=rpc175_set_actor_facing_angle "
        "stage=prewrite reason=ownership_changed");
    return 0;
  }

  memcpy(g_rpc175_hook_target, g_rpc175_hook_patch,
         sizeof(g_rpc175_hook_patch));
  if (!FlushInstructionCache(GetCurrentProcess(), g_rpc175_hook_target,
                             sizeof(g_rpc175_hook_patch))) {
    (void)rollback_rpc175_hook_before_publication(
        original_protect, "instruction_cache_flush");
    return 0;
  }
  if (memcmp(g_rpc175_hook_target, g_rpc175_hook_patch,
             sizeof(g_rpc175_hook_patch)) != 0) {
    (void)VirtualProtect(g_rpc175_hook_target,
                         sizeof(g_rpc175_hook_patch), original_protect,
                         &ignored_protect);
    samp_re_log(
        "hook result=ownership_lost function=rpc175_set_actor_facing_angle "
        "stage=postwrite_verify action=leave_foreign_bytes");
    return 0;
  }

  restore_protection_ok =
      VirtualProtect(g_rpc175_hook_target, sizeof(g_rpc175_hook_patch),
                     original_protect, &ignored_protect) != FALSE;
  if (!restore_protection_ok) {
    (void)rollback_rpc175_hook_before_publication(
        original_protect, "restore_protection");
    return 0;
  }
  if (memcmp(g_rpc175_hook_target, g_rpc175_hook_patch,
             sizeof(g_rpc175_hook_patch)) != 0) {
    samp_re_log(
        "hook result=ownership_lost function=rpc175_set_actor_facing_angle "
        "stage=postprotect_verify action=leave_foreign_bytes");
    return 0;
  }

  g_rpc175_hook_installed = 1;
  samp_re_log(
      "hook result=installed function=rpc175_set_actor_facing_angle "
      "target=%p trampoline=%p patch=e9_rel32_9090 patch_len=%u mode=%s "
      "evidence=STATIC_037,TODO_VERIFY",
      (void *)g_rpc175_hook_target, (void *)g_original_rpc175,
      (unsigned)SAMP_RE_RPC175_PATCH_LENGTH, mode_name(g_mode));
  return 1;
}

static void switch_hooks_to_live_bypass(void) {
  if (InterlockedCompareExchange(&g_hook_state, 2, 1) == 1) {
    samp_re_log(
        "kill_switch result=live_bypass hook_bytes=retained "
        "late_calls=original_trampoline active_calls=%ld "
        "rpc175_installed=%d rpc178_installed=%d "
        "physical_restore=process_teardown",
        (long)InterlockedCompareExchange(&g_active_calls, 0, 0),
        g_rpc175_hook_installed, g_rpc178_hook_installed);
  }
}

static void call_original_rpc178_once(
    samp_re_rpc_parameters_prefix *parameters) {
  samp_re_rpc_handler_fn original = g_original_rpc178;
  if (original != NULL) {
    original(parameters);
  } else {
    samp_re_log(
        "rpc178 result=drop reason=missing_trampoline invariant=violated");
  }
}

static int decode_runtime_rpc178(
    samp_re_rpc_parameters_prefix *parameters,
    samp_re_rpc178_payload *out_payload, const char **out_reason) {
  samp_re_rpc_parameters_prefix prefix;

  if (out_reason != NULL) {
    *out_reason = "unknown";
  }
  if (parameters == NULL ||
      !memory_is_readable((uintptr_t)parameters, sizeof(prefix))) {
    if (out_reason != NULL) {
      *out_reason = "prefix_unreadable";
    }
    return 0;
  }
  memcpy(&prefix, parameters, sizeof(prefix));
  if (prefix.number_of_bits_of_data != 48) {
    if (out_reason != NULL) {
      *out_reason = "bits_not_48";
    }
    return 0;
  }
  if (prefix.input == NULL ||
      !memory_is_readable((uintptr_t)prefix.input, 6u)) {
    if (out_reason != NULL) {
      *out_reason = "payload_unreadable";
    }
    return 0;
  }
  if (!samp_re_decode_rpc178(prefix.input, 6u, 48u, out_payload)) {
    if (out_reason != NULL) {
      *out_reason = "decode_rejected";
    }
    return 0;
  }
  if (out_reason != NULL) {
    *out_reason = "exact_48_bits";
  }
  return 1;
}

static samp_re_actor_lookup_result lookup_actor(
    uint16_t actor_id, void **out_actor, const char **out_reason) {
  uintptr_t address;
  uint32_t netgame_value;
  uint32_t pools_value;
  uint32_t actor_pool_value;
  uint32_t active_value;
  uint32_t actor_value;
  samp_re_actor_pool_slot_offsets offsets;

  if (out_actor != NULL) {
    *out_actor = NULL;
  }
  if (out_reason != NULL) {
    *out_reason = "unknown";
  }

  if (!checked_add(g_samp_base, SAMP_RE_NETGAME_GLOBAL_RVA, &address) ||
      !read_u32(address, &netgame_value) || netgame_value == 0u) {
    if (out_reason != NULL) {
      *out_reason = "netgame_unavailable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  if (!checked_add((uintptr_t)netgame_value, SAMP_RE_NETGAME_POOLS_OFFSET,
                   &address) ||
      !read_u32(address, &pools_value) || pools_value == 0u) {
    if (out_reason != NULL) {
      *out_reason = "pools_unavailable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  if (!checked_add((uintptr_t)pools_value,
                   SAMP_RE_POOLS_ACTOR_POOL_OFFSET, &address) ||
      !read_u32(address, &actor_pool_value)) {
    if (out_reason != NULL) {
      *out_reason = "actor_pool_slot_unreadable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  if (actor_pool_value == 0u) {
    if (out_reason != NULL) {
      *out_reason = "actor_pool_null";
    }
    return SAMP_RE_ACTOR_LOOKUP_NOOP;
  }
  if (!samp_re_actor_pool_offsets(actor_id, &offsets)) {
    if (out_reason != NULL) {
      *out_reason = "actor_id_out_of_range";
    }
    return SAMP_RE_ACTOR_LOOKUP_NOOP;
  }
  if (!checked_add((uintptr_t)actor_pool_value, offsets.active_state,
                   &address) ||
      !read_u32(address, &active_value)) {
    if (out_reason != NULL) {
      *out_reason = "active_state_unreadable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  if (active_value == 0u) {
    if (out_reason != NULL) {
      *out_reason = "actor_inactive";
    }
    return SAMP_RE_ACTOR_LOOKUP_NOOP;
  }
  if (!checked_add((uintptr_t)actor_pool_value, offsets.actor_pointer,
                   &address) ||
      !read_u32(address, &actor_value)) {
    if (out_reason != NULL) {
      *out_reason = "actor_pointer_unreadable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  if (actor_value == 0u) {
    if (out_reason != NULL) {
      *out_reason = "actor_null";
    }
    return SAMP_RE_ACTOR_LOOKUP_NOOP;
  }

  if (out_actor != NULL) {
    *out_actor = (void *)(uintptr_t)actor_value;
  }
  if (out_reason != NULL) {
    *out_reason = "actor_found";
  }
  return SAMP_RE_ACTOR_LOOKUP_FOUND;
}

static void call_original_rpc175_once(
    samp_re_rpc_parameters_prefix *parameters) {
  samp_re_rpc_handler_fn original = g_original_rpc175;
  if (original != NULL) {
    original(parameters);
  } else {
    samp_re_log(
        "rpc175 result=drop reason=missing_trampoline invariant=violated");
  }
}

static int decode_runtime_rpc175(
    samp_re_rpc_parameters_prefix *parameters,
    samp_re_rpc175_payload *out_payload, const char **out_reason) {
  samp_re_rpc_parameters_prefix prefix;

  if (out_reason != NULL) {
    *out_reason = "unknown";
  }
  if (parameters == NULL ||
      !memory_is_readable((uintptr_t)parameters, sizeof(prefix))) {
    if (out_reason != NULL) {
      *out_reason = "prefix_unreadable";
    }
    return 0;
  }
  memcpy(&prefix, parameters, sizeof(prefix));
  if (prefix.number_of_bits_of_data < 48) {
    if (out_reason != NULL) {
      *out_reason = "bits_below_48";
    }
    return 0;
  }
  if (prefix.input == NULL ||
      !memory_is_readable((uintptr_t)prefix.input, 6u)) {
    if (out_reason != NULL) {
      *out_reason = "payload_unreadable";
    }
    return 0;
  }
  if (!samp_re_decode_rpc175(
          prefix.input, 6u, (uint32_t)prefix.number_of_bits_of_data,
          out_payload)) {
    if (out_reason != NULL) {
      *out_reason = "decode_rejected";
    }
    return 0;
  }
  if (out_reason != NULL) {
    *out_reason = prefix.number_of_bits_of_data == 48
                      ? "exact_48_bits"
                      : "first_48_bits";
  }
  return 1;
}

static samp_re_actor_lookup_result prepare_actor_facing_write(
    void *actor, uintptr_t *out_heading_address, uint32_t *out_gta_ref,
    void **out_cached_ped, void **out_resolved_ped, const char **out_reason) {
  uintptr_t address;
  uint32_t gta_ref;
  uint32_t cached_ped_value;
  void *resolved_ped;

  if (out_heading_address != NULL) {
    *out_heading_address = 0u;
  }
  if (out_gta_ref != NULL) {
    *out_gta_ref = 0u;
  }
  if (out_cached_ped != NULL) {
    *out_cached_ped = NULL;
  }
  if (out_resolved_ped != NULL) {
    *out_resolved_ped = NULL;
  }
  if (out_reason != NULL) {
    *out_reason = "unknown";
  }
  if (actor == NULL) {
    if (out_reason != NULL) {
      *out_reason = "actor_null";
    }
    return SAMP_RE_ACTOR_LOOKUP_NOOP;
  }

  /*
   * STATIC_037:
   * CActor::SetFacingAngle at samp.dll+0x0009C570 checks cached CPed at
   * CActor+0x48 first, resolves CActor+0x44 through samp.dll+0x000B3B70,
   * and only writes when both checks succeed.
   */
  if (!checked_add((uintptr_t)actor, SAMP_RE_CACTOR_PED_OFFSET, &address) ||
      !read_u32(address, &cached_ped_value)) {
    if (out_reason != NULL) {
      *out_reason = "actor_ped_slot_unreadable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  if (cached_ped_value == 0u) {
    if (out_reason != NULL) {
      *out_reason = "ped_null";
    }
    return SAMP_RE_ACTOR_LOOKUP_NOOP;
  }
  if (!checked_add((uintptr_t)actor, SAMP_RE_CACTOR_GTA_REF_OFFSET,
                   &address) ||
      !read_u32(address, &gta_ref)) {
    if (out_reason != NULL) {
      *out_reason = "actor_gta_ref_unreadable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  if (g_ped_from_ref == NULL) {
    if (out_reason != NULL) {
      *out_reason = "ped_from_ref_missing";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }
  resolved_ped = g_ped_from_ref(gta_ref);
  if (resolved_ped == NULL) {
    if (out_reason != NULL) {
      *out_reason = "gta_ref_not_resolved";
    }
    return SAMP_RE_ACTOR_LOOKUP_NOOP;
  }
  if (!checked_add((uintptr_t)cached_ped_value,
                   SAMP_RE_CPED_HEADING_OFFSET, &address) ||
      !memory_is_writable(address, sizeof(uint32_t))) {
    if (out_reason != NULL) {
      *out_reason = "heading_not_writable";
    }
    return SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
  }

  if (out_heading_address != NULL) {
    *out_heading_address = address;
  }
  if (out_gta_ref != NULL) {
    *out_gta_ref = gta_ref;
  }
  if (out_cached_ped != NULL) {
    *out_cached_ped = (void *)(uintptr_t)cached_ped_value;
  }
  if (out_resolved_ped != NULL) {
    *out_resolved_ped = resolved_ped;
  }
  if (out_reason != NULL) {
    *out_reason = "write_ready";
  }
  return SAMP_RE_ACTOR_LOOKUP_FOUND;
}

static void __cdecl hook_rpc175_set_actor_facing_angle(
    samp_re_rpc_parameters_prefix *parameters) {
  samp_re_rpc175_payload payload = {0};
  const char *decode_reason = NULL;
  const char *lookup_reason = NULL;
  const char *write_reason = NULL;
  void *actor = NULL;
  samp_re_actor_lookup_result lookup;
  LONG sequence = InterlockedIncrement(&g_rpc175_call_sequence);
  LONG state;

  InterlockedIncrement(&g_active_calls);
  state = InterlockedCompareExchange(&g_hook_state, 0, 0);
  if (state != 1) {
    samp_re_log(
        "rpc175 seq=%ld mode=bypass reason=hook_draining state=%ld "
        "original_calls=1",
        (long)sequence, (long)state);
    call_original_rpc175_once(parameters);
    InterlockedDecrement(&g_active_calls);
    return;
  }

  if (g_mode == SAMP_RE_MODE_SHADOW) {
    uint32_t stored_heading_bits = 0u;
    int decoded =
        decode_runtime_rpc175(parameters, &payload, &decode_reason);
    int readback = 0;

    if (decoded) {
      lookup = lookup_actor(payload.actor_id, &actor, &lookup_reason);
    } else {
      lookup = SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE;
      lookup_reason = "decode_rejected";
    }
    call_original_rpc175_once(parameters);
    if (decoded && lookup == SAMP_RE_ACTOR_LOOKUP_FOUND) {
      readback = read_actor_heading_bits(actor, &stored_heading_bits);
    }
    samp_re_log(
        "rpc175 seq=%ld mode=shadow decoded=%d decode_reason=%s actor=%u "
        "angle_bits=0x%08lx lookup=%d lookup_reason=%s readback=%d "
        "stored_heading_bits=0x%08lx original_calls=1 "
        "evidence=STATIC_037,TODO_VERIFY",
        (long)sequence, decoded,
        decode_reason != NULL ? decode_reason : "unknown",
        decoded ? (unsigned)payload.actor_id : 0u,
        decoded ? (unsigned long)payload.angle_bits : 0ul, (int)lookup,
        lookup_reason != NULL ? lookup_reason : "unknown", readback,
        readback ? (unsigned long)stored_heading_bits : 0ul);
    InterlockedDecrement(&g_active_calls);
    return;
  }

  if (g_mode != SAMP_RE_MODE_REPLACE ||
      !decode_runtime_rpc175(parameters, &payload, &decode_reason)) {
    samp_re_log(
        "rpc175 seq=%ld mode=replace result=fallback reason=%s "
        "original_calls=1",
        (long)sequence,
        decode_reason != NULL ? decode_reason : "mode_not_replace");
    call_original_rpc175_once(parameters);
    InterlockedDecrement(&g_active_calls);
    return;
  }

  lookup = lookup_actor(payload.actor_id, &actor, &lookup_reason);
  if (lookup == SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE) {
    samp_re_log(
        "rpc175 seq=%ld mode=replace result=fallback actor=%u "
        "angle_bits=0x%08lx reason=%s original_calls=1",
        (long)sequence, (unsigned)payload.actor_id,
        (unsigned long)payload.angle_bits,
        lookup_reason != NULL ? lookup_reason : "lookup_unavailable");
    call_original_rpc175_once(parameters);
  } else if (lookup == SAMP_RE_ACTOR_LOOKUP_NOOP) {
    samp_re_log(
        "rpc175 seq=%ld mode=replace result=noop actor=%u "
        "angle_bits=0x%08lx reason=%s original_calls=0",
        (long)sequence, (unsigned)payload.actor_id,
        (unsigned long)payload.angle_bits,
        lookup_reason != NULL ? lookup_reason : "original_noop");
  } else {
    uintptr_t heading_address = 0u;
    uint32_t gta_ref = 0u;
    uint32_t heading_bits = 0u;
    uint32_t stored_heading_bits = 0u;
    void *cached_ped = NULL;
    void *resolved_ped = NULL;
    int readback;

    lookup = prepare_actor_facing_write(
        actor, &heading_address, &gta_ref, &cached_ped, &resolved_ped,
        &write_reason);
    if (lookup == SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE) {
      samp_re_log(
          "rpc175 seq=%ld mode=replace result=fallback actor=%u "
          "angle_bits=0x%08lx conversion=not_run reason=%s "
          "original_calls=1",
          (long)sequence, (unsigned)payload.actor_id,
          (unsigned long)payload.angle_bits,
          write_reason != NULL ? write_reason : "write_unavailable");
      call_original_rpc175_once(parameters);
    } else if (lookup == SAMP_RE_ACTOR_LOOKUP_NOOP) {
      samp_re_log(
          "rpc175 seq=%ld mode=replace result=noop actor=%u "
          "angle_bits=0x%08lx conversion=not_run reason=%s "
          "original_calls=0",
          (long)sequence, (unsigned)payload.actor_id,
          (unsigned long)payload.angle_bits,
          write_reason != NULL ? write_reason : "original_noop");
    } else {
      heading_bits = samp_re_rpc175_heading_bits(payload.angle_bits);
      memcpy((void *)heading_address, &heading_bits, sizeof(heading_bits));
      readback = read_u32(heading_address, &stored_heading_bits);
      samp_re_log(
          "rpc175 seq=%ld mode=replace result=applied actor=%u "
          "angle_bits=0x%08lx heading_bits=0x%08lx actor_ptr=%p "
          "gta_ref=%lu cached_ped=%p resolved_ped=%p "
          "cped_heading_offset=0x%03lx readback=%d "
          "stored_heading_bits=0x%08lx heading_match=%d "
          "original_calls=0 "
          "evidence=OBSERVED_037,PROBE_TRACE,STATIC_037,TODO_VERIFY",
          (long)sequence, (unsigned)payload.actor_id,
          (unsigned long)payload.angle_bits, (unsigned long)heading_bits,
          actor, (unsigned long)gta_ref, cached_ped, resolved_ped,
          (unsigned long)SAMP_RE_CPED_HEADING_OFFSET, readback,
          readback ? (unsigned long)stored_heading_bits : 0ul,
          readback && stored_heading_bits == heading_bits);
    }
  }
  InterlockedDecrement(&g_active_calls);
}

static void __cdecl hook_rpc178_set_actor_health(
    samp_re_rpc_parameters_prefix *parameters) {
  samp_re_rpc178_payload payload = {0};
  const char *decode_reason = NULL;
  const char *lookup_reason = NULL;
  void *actor = NULL;
  samp_re_actor_lookup_result lookup;
  LONG sequence = InterlockedIncrement(&g_call_sequence);
  LONG state;

  InterlockedIncrement(&g_active_calls);
  state = InterlockedCompareExchange(&g_hook_state, 0, 0);
  if (state != 1) {
    samp_re_log(
        "rpc178 seq=%ld mode=bypass reason=hook_draining state=%ld "
        "original_calls=1",
        (long)sequence, (long)state);
    call_original_rpc178_once(parameters);
    InterlockedDecrement(&g_active_calls);
    return;
  }

  if (g_mode == SAMP_RE_MODE_SHADOW) {
    int decoded =
        decode_runtime_rpc178(parameters, &payload, &decode_reason);
    samp_re_log(
        "rpc178 seq=%ld mode=shadow decoded=%d decode_reason=%s actor=%u "
        "health_bits=0x%08lx original_calls=1",
        (long)sequence, decoded,
        decode_reason != NULL ? decode_reason : "unknown",
        decoded ? (unsigned)payload.actor_id : 0u,
        decoded ? (unsigned long)payload.health_bits : 0ul);
    call_original_rpc178_once(parameters);
    InterlockedDecrement(&g_active_calls);
    return;
  }

  if (g_mode != SAMP_RE_MODE_REPLACE ||
      !decode_runtime_rpc178(parameters, &payload, &decode_reason)) {
    samp_re_log(
        "rpc178 seq=%ld mode=replace result=fallback reason=%s "
        "original_calls=1",
        (long)sequence,
        decode_reason != NULL ? decode_reason : "mode_not_replace");
    call_original_rpc178_once(parameters);
    InterlockedDecrement(&g_active_calls);
    return;
  }

  lookup = lookup_actor(payload.actor_id, &actor, &lookup_reason);
  if (lookup == SAMP_RE_ACTOR_LOOKUP_UNAVAILABLE) {
    samp_re_log(
        "rpc178 seq=%ld mode=replace result=fallback actor=%u "
        "health_bits=0x%08lx reason=%s original_calls=1",
        (long)sequence, (unsigned)payload.actor_id,
        (unsigned long)payload.health_bits,
        lookup_reason != NULL ? lookup_reason : "lookup_unavailable");
    call_original_rpc178_once(parameters);
  } else if (lookup == SAMP_RE_ACTOR_LOOKUP_NOOP) {
    samp_re_log(
        "rpc178 seq=%ld mode=replace result=noop actor=%u "
        "health_bits=0x%08lx reason=%s original_calls=0",
        (long)sequence, (unsigned)payload.actor_id,
        (unsigned long)payload.health_bits,
        lookup_reason != NULL ? lookup_reason : "original_noop");
  } else if (g_set_actor_health != NULL) {
    uint32_t stored_health_bits = 0u;
    int readback;
    g_set_actor_health(actor, payload.health_bits);
    readback = read_actor_health_bits(actor, &stored_health_bits);
    samp_re_log(
        "rpc178 seq=%ld mode=replace result=applied actor=%u "
        "health_bits=0x%08lx actor_ptr=%p downstream_rva=0x%08lx "
        "readback=%d stored_health_bits=0x%08lx health_match=%d "
        "original_calls=0 evidence=STATIC_037,TODO_VERIFY",
        (long)sequence, (unsigned)payload.actor_id,
        (unsigned long)payload.health_bits, actor,
        (unsigned long)SAMP_RE_CACTOR_SET_HEALTH_RVA, readback,
        readback ? (unsigned long)stored_health_bits : 0ul,
        readback && stored_health_bits == payload.health_bits);
  } else {
    samp_re_log(
        "rpc178 seq=%ld mode=replace result=fallback actor=%u "
        "health_bits=0x%08lx reason=missing_downstream original_calls=1",
        (long)sequence, (unsigned)payload.actor_id,
        (unsigned long)payload.health_bits);
    call_original_rpc178_once(parameters);
  }
  InterlockedDecrement(&g_active_calls);
}

static DWORD WINAPI samp_re_worker(void *unused) {
  int mode_valid = 0;
  int rpc175_enabled;
  int rpc178_enabled;
  HMODULE samp_module = NULL;

  (void)unused;
  if (!initialize_log()) {
    return 0u;
  }
  samp_re_log("log result=ready path=\"%s\"", g_log_path);
  g_mode = mode_from_environment(&mode_valid);
  rpc175_enabled = function_enabled(SAMP_RE_FUNCTION_RPC175);
  rpc178_enabled = function_enabled(SAMP_RE_FUNCTION_RPC178);

  if (!path_beside_self(SAMP_RE_DISABLED_FLAG, g_disabled_flag_path,
                        sizeof(g_disabled_flag_path))) {
    samp_re_log(
        "startup result=bypass reason=self_path_unavailable mode=%s",
        mode_name(g_mode));
    return 0u;
  }
  samp_re_log(
      "startup mode=%s mode_valid=%d rpc175_enabled=%d rpc178_enabled=%d "
      "disabled_env=%d disabled_flag=%d default=bypass",
      mode_name(g_mode), mode_valid, rpc175_enabled, rpc178_enabled,
      env_flag_enabled("SAMP_RE_DISABLE"),
      file_exists(g_disabled_flag_path));

  if (!mode_valid || g_mode == SAMP_RE_MODE_BYPASS ||
      (!rpc175_enabled && !rpc178_enabled) ||
      env_flag_enabled("SAMP_RE_DISABLE") ||
      file_exists(g_disabled_flag_path)) {
    samp_re_log("startup result=bypass reason=configuration");
    return 0u;
  }
  if (actor_probe_conflict_present()) {
    samp_re_log("startup result=bypass reason=actor_probe_conflict");
    return 0u;
  }

  while (samp_module == NULL) {
    if (file_exists(g_disabled_flag_path)) {
      samp_re_log("startup result=bypass reason=disabled_while_waiting");
      return 0u;
    }
    samp_module = GetModuleHandleA("samp.dll");
    if (samp_module == NULL) {
      Sleep(100u);
    }
  }
  g_samp_base = (uintptr_t)samp_module;

  if (!pin_overlay_and_samp(samp_module) || !verify_r5_identity(samp_module)) {
    samp_re_log("startup result=bypass reason=preflight_or_install");
    return 0u;
  }
  if (file_exists(g_disabled_flag_path) ||
      env_flag_enabled("SAMP_RE_DISABLE")) {
    samp_re_log("startup result=bypass reason=disabled_before_install");
    return 0u;
  }
  if (actor_probe_conflict_present()) {
    samp_re_log(
        "startup result=bypass reason=actor_probe_conflict_before_install");
    return 0u;
  }
  if ((rpc175_enabled && !verify_rpc175_targets()) ||
      (rpc178_enabled && !verify_rpc178_targets())) {
    samp_re_log("startup result=bypass reason=preflight");
    return 0u;
  }
  if ((rpc175_enabled && !install_rpc175_hook()) ||
      (rpc178_enabled && !install_rpc178_hook())) {
    samp_re_log(
        "startup result=live_bypass reason=install "
        "rpc175_installed=%d rpc178_installed=%d "
        "installed_hooks=original_trampoline_only",
        g_rpc175_hook_installed, g_rpc178_hook_installed);
    return 0u;
  }
  InterlockedExchange(&g_hook_state, 1);
  samp_re_log(
      "startup result=active mode=%s rpc175_enabled=%d rpc178_enabled=%d",
      mode_name(g_mode), rpc175_enabled, rpc178_enabled);

  while (InterlockedCompareExchange(&g_hook_state, 0, 0) == 1) {
    Sleep(SAMP_RE_DISABLED_POLL_MS);
    if (file_exists(g_disabled_flag_path) ||
        env_flag_enabled("SAMP_RE_DISABLE")) {
      switch_hooks_to_live_bypass();
    }
  }
  return 0u;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {
    HANDLE worker;
    g_self_module = module;
    DisableThreadLibraryCalls(module);
    worker = CreateThread(NULL, 0u, samp_re_worker, NULL, 0u, NULL);
    if (worker != NULL) {
      (void)CloseHandle(worker);
    }
  }
  return TRUE;
}
