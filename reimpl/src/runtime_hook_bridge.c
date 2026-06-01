#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sampdll/runtime/hook_bridge.h"

#define SAMP_LEGACY_GRAPHICS_CALL_DISP_ADDR 0x53EB13u
#define SAMP_LEGACY_GRAPHICS_CALL_DISP_ADDR_SECONDARY 0x53EB1Au

typedef void(__cdecl *samp_graphics_target_fn)(void);

static void (*g_graphics_callback)(void) = NULL;
static samp_graphics_target_fn g_original_graphics_target_primary = NULL;
static samp_graphics_target_fn g_original_graphics_target_secondary = NULL;
static void *g_generated_graphics_thunk_primary = NULL;
static void *g_generated_graphics_thunk_secondary = NULL;

#define SAMP_GRAPHICS_HOOK_STUB_SIZE 25u

static int hook_trace_enabled(void) {
  const char *value = getenv("SAMPDLL_TRACE");
  return (value != NULL && *value != '\0' && value[0] != '0') ? 1 : 0;
}

static void hook_trace_file_line(const char *line) {
  FILE *file = NULL;

  if (line == NULL || line[0] == '\0') {
    return;
  }

  file = fopen("samp_hook_trace.log", "ab");
  if (file == NULL) {
    return;
  }
  fputs(line, file);
  fputc('\n', file);
  fclose(file);
}

static void hook_tracef(const char *fmt, ...) {
  char message[640];
  char payload[560];
  int payload_len = 0;
  va_list args;

  if (fmt == NULL) {
    return;
  }

  va_start(args, fmt);
  payload_len = vsnprintf(payload, sizeof(payload), fmt, args);
  va_end(args);
  if (payload_len < 0) {
    return;
  }

  (void)snprintf(message, sizeof(message), "[sampdll-hook] %s", payload);
  hook_trace_file_line(message);
  if (hook_trace_enabled()) {
    OutputDebugStringA(message);
  }
}

static int parse_u32_env_hex(const char *text, uint32_t *out_value) {
  unsigned long parsed = 0;
  char *endptr = NULL;

  if (text == NULL || out_value == NULL || *text == '\0') {
    return -1;
  }

  parsed = strtoul(text, &endptr, 0);
  if (endptr == text || *endptr != '\0') {
    return -1;
  }
  if (parsed > 0xffffffffUL) {
    return -1;
  }

  *out_value = (uint32_t)parsed;
  return 0;
}

static int env_flag_is_enabled(const char *name) {
  const char *value = getenv(name);
  if (value == NULL || *value == '\0') {
    return 0;
  }
  return (value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T') ? 1 : 0;
}

static void free_generated_graphics_hook_thunk(void **slot, const char *slot_name) {
  if (slot == NULL || *slot == NULL) {
    return;
  }

  if (!VirtualFree(*slot, 0u, MEM_RELEASE)) {
    hook_tracef("thunk: slot=%s free failed addr=0x%08lx", slot_name, (unsigned long)(uintptr_t)(*slot));
  } else {
    hook_tracef("thunk: slot=%s freed addr=0x%08lx", slot_name, (unsigned long)(uintptr_t)(*slot));
  }
  *slot = NULL;
}

static void free_generated_graphics_hook_thunks(void) {
  free_generated_graphics_hook_thunk(&g_generated_graphics_thunk_secondary, "secondary");
  free_generated_graphics_hook_thunk(&g_generated_graphics_thunk_primary, "primary");
}

static void *create_generated_graphics_hook_thunk(samp_graphics_target_fn *original_target_slot, const char *slot_name) {
  uint8_t *thunk = NULL;
  uint8_t *cursor = NULL;

  if (original_target_slot == NULL) {
    return NULL;
  }

  thunk = (uint8_t *)VirtualAlloc(NULL, SAMP_GRAPHICS_HOOK_STUB_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (thunk == NULL) {
    hook_tracef("thunk: slot=%s alloc failed", slot_name);
    return NULL;
  }

  cursor = thunk;

  /* pushad */
  *cursor++ = 0x60u;
  /* mov eax, [g_graphics_callback] */
  *cursor++ = 0xA1u;
  {
    uint32_t callback_slot_addr = (uint32_t)(uintptr_t)&g_graphics_callback;
    memcpy(cursor, &callback_slot_addr, sizeof(callback_slot_addr));
  }
  cursor += 4;
  /* test eax, eax ; jz skip_call ; call eax */
  *cursor++ = 0x85u;
  *cursor++ = 0xC0u;
  *cursor++ = 0x74u;
  *cursor++ = 0x02u;
  *cursor++ = 0xFFu;
  *cursor++ = 0xD0u;
  /* popad */
  *cursor++ = 0x61u;
  /* mov eax, [g_original_graphics_target_*] */
  *cursor++ = 0xA1u;
  {
    uint32_t original_slot_addr = (uint32_t)(uintptr_t)original_target_slot;
    memcpy(cursor, &original_slot_addr, sizeof(original_slot_addr));
  }
  cursor += 4;
  /* test eax, eax ; jz ret ; jmp eax ; ret */
  *cursor++ = 0x85u;
  *cursor++ = 0xC0u;
  *cursor++ = 0x74u;
  *cursor++ = 0x02u;
  *cursor++ = 0xFFu;
  *cursor++ = 0xE0u;
  *cursor++ = 0xC3u;

  FlushInstructionCache(GetCurrentProcess(), thunk, (SIZE_T)(cursor - thunk));
  hook_tracef("thunk: slot=%s generated addr=0x%08lx callback_slot=0x%08lx original_slot=0x%08lx", slot_name,
              (unsigned long)(uintptr_t)thunk, (unsigned long)(uintptr_t)&g_graphics_callback,
              (unsigned long)(uintptr_t)original_target_slot);
  return thunk;
}

static int install_call_disp_hook(uintptr_t call_disp_addr, void (*hook_thunk)(void), uint8_t *out_opcode, uint8_t saved_disp[4],
                                  uintptr_t *out_original_target, samp_graphics_target_fn *runtime_original_target_slot,
                                  const char *slot_name) {
  uintptr_t call_opcode_addr = 0;
  int32_t saved_rel32 = 0;
  int32_t rel32 = 0;
  DWORD old_protect = 0;
  DWORD old_protect_restore = 0;
  uint8_t opcode = 0;

  if (call_disp_addr < 1u || hook_thunk == NULL || out_opcode == NULL || saved_disp == NULL || out_original_target == NULL) {
    return -1;
  }

  call_opcode_addr = call_disp_addr - 1u;
  opcode = *(volatile const uint8_t *)(call_opcode_addr);
  if (opcode != 0xE8u) {
    hook_tracef("install: slot=%s opcode mismatch at 0x%08lx got=0x%02x expected=0xE8", slot_name,
                (unsigned long)call_opcode_addr, (unsigned)opcode);
    return -1;
  }

  *out_opcode = opcode;
  memcpy(saved_disp, (const void *)call_disp_addr, 4u);
  memcpy(&saved_rel32, (const void *)call_disp_addr, sizeof(saved_rel32));
  *out_original_target = (uintptr_t)((intptr_t)(call_opcode_addr + 5u) + (intptr_t)saved_rel32);
  if (runtime_original_target_slot != NULL) {
    *runtime_original_target_slot = (samp_graphics_target_fn)(*out_original_target);
  }

  rel32 = (int32_t)((intptr_t)(uintptr_t)hook_thunk - (intptr_t)(call_opcode_addr + 5u));
  if (!VirtualProtect((LPVOID)call_disp_addr, 4u, PAGE_EXECUTE_READWRITE, &old_protect)) {
    hook_tracef("install: slot=%s VirtualProtect failed addr=0x%08lx", slot_name, (unsigned long)call_disp_addr);
    return -1;
  }

  memcpy((void *)call_disp_addr, &rel32, sizeof(rel32));
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)call_opcode_addr, 5u);
  (void)VirtualProtect((LPVOID)call_disp_addr, 4u, old_protect, &old_protect_restore);

  hook_tracef("install: slot=%s success call_disp=0x%08lx thunk=0x%08lx original=0x%08lx rel32=0x%08x", slot_name,
              (unsigned long)call_disp_addr, (unsigned long)(uintptr_t)hook_thunk, (unsigned long)(*out_original_target),
              (unsigned int)rel32);
  return 0;
}

static int restore_call_disp_hook(uintptr_t call_disp_addr, const uint8_t saved_disp[4], const char *slot_name) {
  uintptr_t call_opcode_addr = 0;
  DWORD old_protect = 0;
  DWORD old_protect_restore = 0;

  if (call_disp_addr < 1u || saved_disp == NULL) {
    return -1;
  }

  call_opcode_addr = call_disp_addr - 1u;
  if (!VirtualProtect((LPVOID)call_disp_addr, 4u, PAGE_EXECUTE_READWRITE, &old_protect)) {
    hook_tracef("uninstall: slot=%s VirtualProtect failed addr=0x%08lx", slot_name, (unsigned long)call_disp_addr);
    return -1;
  }

  memcpy((void *)call_disp_addr, saved_disp, 4u);
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)call_opcode_addr, 5u);
  (void)VirtualProtect((LPVOID)call_disp_addr, 4u, old_protect, &old_protect_restore);
  hook_tracef("uninstall: slot=%s restored call_disp=0x%08lx", slot_name, (unsigned long)call_disp_addr);
  return 0;
}

int samp_hook_bridge_configure_from_env(samp_runtime_hook_bridge *bridge) {
  uint32_t custom_addr_primary = 0;
  uint32_t custom_addr_secondary = 0;
  const char *custom_text_primary = NULL;
  const char *custom_text_secondary = NULL;
  int legacy_mode = 0;
  int legacy_secondary_mode = 0;

  if (bridge == NULL) {
    return -1;
  }

  memset(bridge, 0, sizeof(*bridge));

  legacy_mode = env_flag_is_enabled("SAMPDLL_ENABLE_LEGACY_02X_HOOKS");
  legacy_secondary_mode = env_flag_is_enabled("SAMPDLL_ENABLE_LEGACY_02X_HOOKS_SECONDARY");
  bridge->enabled = legacy_mode || env_flag_is_enabled("SAMPDLL_ENABLE_RUNTIME_HOOKS");

  custom_text_primary = getenv("SAMPDLL_HOOK_GRAPHICS_CALL_DISP");
  if (custom_text_primary != NULL && *custom_text_primary != '\0') {
    if (parse_u32_env_hex(custom_text_primary, &custom_addr_primary) != 0) {
      hook_tracef("configure: invalid custom disp '%s'", custom_text_primary);
      return -1;
    }
    bridge->graphics_call_disp_addr = (uintptr_t)custom_addr_primary;
    bridge->configured = 1;
  }

  custom_text_secondary = getenv("SAMPDLL_HOOK_GRAPHICS_CALL_DISP2");
  if (custom_text_secondary != NULL && *custom_text_secondary != '\0') {
    if (parse_u32_env_hex(custom_text_secondary, &custom_addr_secondary) != 0) {
      hook_tracef("configure: invalid custom disp2 '%s'", custom_text_secondary);
      return -1;
    }
    bridge->graphics_call_disp_addr_secondary = (uintptr_t)custom_addr_secondary;
    bridge->secondary_configured = 1;
  }

  if (legacy_mode) {
    if (!bridge->configured) {
      bridge->graphics_call_disp_addr = (uintptr_t)SAMP_LEGACY_GRAPHICS_CALL_DISP_ADDR;
      bridge->configured = 1;
    }
    if (!bridge->secondary_configured && legacy_secondary_mode) {
      bridge->graphics_call_disp_addr_secondary = (uintptr_t)SAMP_LEGACY_GRAPHICS_CALL_DISP_ADDR_SECONDARY;
      bridge->secondary_configured = 1;
    }
    hook_tracef("configure: legacy disp=0x%08lx disp2=0x%08lx enabled=%d secondary=%d",
                (unsigned long)bridge->graphics_call_disp_addr, (unsigned long)bridge->graphics_call_disp_addr_secondary,
                bridge->enabled, bridge->secondary_configured ? 1 : 0);
    return 0;
  }

  if (bridge->configured || bridge->secondary_configured) {
    hook_tracef("configure: custom disp=0x%08lx disp2=0x%08lx enabled=%d", (unsigned long)bridge->graphics_call_disp_addr,
                (unsigned long)bridge->graphics_call_disp_addr_secondary, bridge->enabled);
  } else {
    hook_tracef("configure: no disp configured enabled=%d", bridge->enabled);
  }
  return 0;
}

int samp_hook_bridge_install_graphics_callback(samp_runtime_hook_bridge *bridge, void (*graphics_callback)(void)) {
  int primary_rc = 0;
  int secondary_rc = 0;
  int primary_attempted = 0;
  int secondary_attempted = 0;

  if (bridge == NULL || graphics_callback == NULL) {
    return -1;
  }

  bridge->install_attempted = 1;
  bridge->installed = 0;
  bridge->secondary_installed = 0;
  bridge->install_succeeded = 0;
  if (!bridge->enabled || (!bridge->configured && !bridge->secondary_configured)) {
    hook_tracef("install: skipped enabled=%d configured=%d configured2=%d disp=0x%08lx disp2=0x%08lx", bridge->enabled,
                bridge->configured, bridge->secondary_configured, (unsigned long)bridge->graphics_call_disp_addr,
                (unsigned long)bridge->graphics_call_disp_addr_secondary);
    return 0;
  }

  g_graphics_callback = graphics_callback;
  free_generated_graphics_hook_thunks();
  g_original_graphics_target_primary = NULL;
  g_original_graphics_target_secondary = NULL;

  if (bridge->configured && bridge->graphics_call_disp_addr >= 1u) {
    g_generated_graphics_thunk_primary = create_generated_graphics_hook_thunk(&g_original_graphics_target_primary, "primary");
    if (g_generated_graphics_thunk_primary == NULL) {
      primary_rc = -1;
    }
  }

  if (bridge->secondary_configured && bridge->graphics_call_disp_addr_secondary >= 1u) {
    g_generated_graphics_thunk_secondary =
        create_generated_graphics_hook_thunk(&g_original_graphics_target_secondary, "secondary");
    if (g_generated_graphics_thunk_secondary == NULL) {
      secondary_rc = -1;
    }
  }

  if (bridge->configured && bridge->graphics_call_disp_addr >= 1u && g_generated_graphics_thunk_primary != NULL) {
    primary_attempted = 1;
    primary_rc = install_call_disp_hook(bridge->graphics_call_disp_addr,
                                        (void (*)(void))(uintptr_t)g_generated_graphics_thunk_primary,
                                        &bridge->graphics_call_opcode, bridge->graphics_call_saved_disp,
                                        &bridge->graphics_call_original_target, &g_original_graphics_target_primary,
                                        "primary");
    if (primary_rc == 0) {
      bridge->installed = 1;
      bridge->install_succeeded = 1;
    }
  }

  if (bridge->secondary_configured && bridge->graphics_call_disp_addr_secondary >= 1u &&
      g_generated_graphics_thunk_secondary != NULL) {
    secondary_attempted = 1;
    secondary_rc =
        install_call_disp_hook(bridge->graphics_call_disp_addr_secondary,
                               (void (*)(void))(uintptr_t)g_generated_graphics_thunk_secondary,
                               &bridge->graphics_call_opcode_secondary, bridge->graphics_call_saved_disp_secondary,
                               &bridge->graphics_call_original_target_secondary, &g_original_graphics_target_secondary,
                               "secondary");
    if (secondary_rc == 0) {
      bridge->secondary_installed = 1;
      bridge->install_succeeded = 1;
    }
  }

  hook_tracef("install: summary primary_attempted=%d primary_rc=%d primary_installed=%d secondary_attempted=%d "
              "secondary_rc=%d secondary_installed=%d",
              primary_attempted, primary_rc, bridge->installed ? 1 : 0, secondary_attempted, secondary_rc,
              bridge->secondary_installed ? 1 : 0);

  if (bridge->install_succeeded) {
    return 0;
  }

  g_graphics_callback = NULL;
  g_original_graphics_target_primary = NULL;
  g_original_graphics_target_secondary = NULL;
  free_generated_graphics_hook_thunks();
  return (primary_attempted || secondary_attempted) ? -1 : 0;
}

void samp_hook_bridge_uninstall(samp_runtime_hook_bridge *bridge) {
  if (bridge == NULL) {
    return;
  }

  if (bridge->secondary_installed) {
    (void)restore_call_disp_hook(bridge->graphics_call_disp_addr_secondary, bridge->graphics_call_saved_disp_secondary,
                                 "secondary");
    bridge->secondary_installed = 0;
  }

  if (bridge->installed) {
    (void)restore_call_disp_hook(bridge->graphics_call_disp_addr, bridge->graphics_call_saved_disp, "primary");
    bridge->installed = 0;
  }

  bridge->install_succeeded = 0;
  g_graphics_callback = NULL;
  g_original_graphics_target_primary = NULL;
  g_original_graphics_target_secondary = NULL;
  free_generated_graphics_hook_thunks();
}
#endif
