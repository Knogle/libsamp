#ifndef SAMPDLL_RUNTIME_HOOK_BRIDGE_H
#define SAMPDLL_RUNTIME_HOOK_BRIDGE_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

typedef struct samp_runtime_hook_bridge {
  int enabled;
  int configured;
  int installed;
  int install_attempted;
  int install_succeeded;
  int callback_after_original;

  uintptr_t graphics_call_disp_addr;
  uint8_t graphics_call_opcode;
  uint8_t graphics_call_saved_disp[4];
  uintptr_t graphics_call_original_target;

  int secondary_configured;
  int secondary_installed;
  uintptr_t graphics_call_disp_addr_secondary;
  uint8_t graphics_call_opcode_secondary;
  uint8_t graphics_call_saved_disp_secondary[4];
  uintptr_t graphics_call_original_target_secondary;
} samp_runtime_hook_bridge;

int samp_hook_bridge_configure_from_env(samp_runtime_hook_bridge *bridge);

int samp_hook_bridge_install_graphics_callback(samp_runtime_hook_bridge *bridge, void (*graphics_callback)(void));

void samp_hook_bridge_uninstall(samp_runtime_hook_bridge *bridge);
#endif

#endif
