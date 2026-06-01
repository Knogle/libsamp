#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <process.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sampdll/net/dual_stack.h"
#include "sampdll/net/raknet_client_adapter.h"
#include "sampdll/net/tcp_bootstrap_manager.h"
#include "sampdll/runtime/boot.h"
#include "sampdll/runtime/hook_bridge.h"

#define SAMP_SETTINGS_STRING_MAX 256
#define SAMP_DEFAULT_CONNECT_PORT 7777
#define SAMP_ENTRY_GATE_READY 7
#define SAMP_LAUNCH_WAIT_MS 5
#define SAMP_GRAPHICS_TICK_MS 16
#define SAMP_LAUNCH_THREAD_JOIN_MS 2000
#define SAMP_RAKNET_PUMP_BUDGET 8
#define SAMP_RAKNET_SLEEP_TIMER 5
#define SAMP_RAKNET_CONNECT_RETRY_MS 3000
#define SAMP_RAKNET_CONNECT_STALE_MS 12000
#define SAMP_PRECONNECT_DELAY_MS 3000
#define SAMP_PRECONNECT_MAX_APPLIES 7200
#define SAMP_PRECONNECT_PED_FALLBACK_APPLIES 720
#define SAMP_PRECONNECT_WORLD_SETTLE_MS 0u
#define SAMP_PRECONNECT_PLAYER_X 1500.0f
#define SAMP_PRECONNECT_PLAYER_Y -887.0979f
#define SAMP_PRECONNECT_PLAYER_Z 32.56055f
#define SAMP_PRECONNECT_CAMERA_X 1497.803f
#define SAMP_PRECONNECT_CAMERA_Y -887.0979f
#define SAMP_PRECONNECT_CAMERA_Z 62.56055f
#define SAMP_PRECONNECT_LOOK_X 1406.65f
#define SAMP_PRECONNECT_LOOK_Y -795.7716f
#define SAMP_PRECONNECT_LOOK_Z 82.2771f
#define SAMP_DEFAULT_NETGAME_VERSION 4057u
#define SAMP_DEFAULT_CLIENT_MOD 1u
#define SAMP_DEFAULT_CLIENT_VERSION "0.3.7"
#define SAMP_DEFAULT_NICKNAME "Player"
#define SAMP_BOOTSTRAP_MANAGER_BYTES 0x142u

#define SAMP_ADDR_MENU 0xBA67A4u
#define SAMP_ADDR_MENU2 0xBA67A5u
#define SAMP_ADDR_MENU3 0xBA67A6u
#define SAMP_ADDR_STARTGAME 0xBA677Bu
#define SAMP_ADDR_GAME_STARTED 0xBA6831u
#define SAMP_ADDR_ENTRY 0xC8D4C0u
#define SAMP_ADDR_BYPASS_VIDS_USA10 0x747483u
#define SAMP_ADDR_BYPASS_VIDS_EU10 0x7474D3u
#define SAMP_ADDR_LOADING_SCREEN_NAME_A 0x866CD8u
#define SAMP_ADDR_LOADING_SCREEN_NAME_B 0x866CCCu
#define SAMP_ADDR_GAME_PROCESS_HOOK_INSTALL 0x58C246u
#define SAMP_ADDR_GAME_PROCESS_HOOK_STORAGE 0x53BED1u
#define SAMP_ADDR_SCRIPT_PROCESS_GATE 0x469EF5u
#define SAMP_ADDR_SCRIPT_PROCESS_GATE2 0x469EF6u
#define SAMP_ADDR_SCRIPT_PROCESS_CALL_DISP 0x53BFC8u
#define SAMP_ADDR_LEGACY_GRAPHICS_CALL_DISP 0x53EB13u
#define SAMP_ADDR_TIME_PASSING_PATCH 0x52CF10u
#define SAMP_ADDR_PROC_GEOMETRY_PATCH 0x53C159u
#define SAMP_ADDR_REPLAY_PROCESS_PATCH 0x53C090u
#define SAMP_ADDR_INTERIOR_PEDS_PATCH 0x440833u
#define SAMP_ADDR_PED_SHADOWS_PATCH 0x53EA08u
#define SAMP_ADDR_ANTI_PAUSE_PATCH 0x561AF0u
#define SAMP_ADDR_RANDOM_WEAPON_PICKUPS 0x5B47B0u
#define SAMP_ADDR_RESPAWN_INTERIOR_RET 0x4090A0u
#define SAMP_ADDR_RESPAWN_INTERIOR_CALL 0x441482u
#define SAMP_ADDR_MESSAGE_PRINT 0x588BE0u
#define SAMP_ADDR_IPL_VEHICLE_PATCH 0x53C06Au
#define SAMP_ADDR_CAR_GENERATOR_PATCH 0x434272u
#define SAMP_ADDR_POPULATION_ADD_PED 0x612710u
#define SAMP_ADDR_RANDOM_CARS_PROCESS 0x53C1C1u
#define SAMP_ADDR_WASTED_MESSAGE_PATCH 0x56E5ADu
#define SAMP_ADDR_CAMERA_FADE_PATCH 0x50AC20u
#define SAMP_ADDR_HWND 0xC97C1Cu
#define SAMP_ADDR_ID3D9DEVICE 0xC97C28u
#define SAMP_ADDR_ENABLE_HUD 0xBA6769u
#define SAMP_ADDR_RADAR_BLANK 0xBAA3FBu
#define SAMP_ADDR_WORLD_MINUTE 0xB70152u
#define SAMP_ADDR_WORLD_HOUR 0xB70153u
#define SAMP_ADDR_WEATHER_A 0xC81320u
#define SAMP_ADDR_WEATHER_B 0xC8131Cu
#define SAMP_ADDR_GRAVITY 0x863984u
#define SAMP_ADDR_STUNT_BONUS 0xA4A474u
#define SAMP_ADDR_ENTER_EXITS 0x96A7C8u
#define SAMP_ADDR_FIND_PLAYER_PED 0xB7CD98u
#define SAMP_ADDR_CURRENT_PLAYER 0xB7CD74u
#define SAMP_ADDR_PROCESS_ONE_COMMAND 0x469EB0u
#define SAMP_SCRIPT_THREAD_BYTES 0xE0u
#define SAMP_SCRIPT_BUF_BYTES 255u
#define SAMP_MP_BRIDGE_MAX_APPLIES 3600
#define SAMP_MP_BRIDGE_TELEPORT_PERIOD 15
#define SAMP_PED_OFFSET_MATRIX 20u
#define SAMP_PED_OFFSET_STATE_FLAGS 1132u
#define SAMP_PED_OFFSET_HEALTH 1344u
#define SAMP_PED_OFFSET_ROTATION1 1368u
#define SAMP_PED_OFFSET_ROTATION2 1372u
#define SAMP_PED_STATE_IN_VEHICLE 0x100u
#define SAMP_ENTITY_OFFSET_MODEL_INDEX 34u
#define SAMP_ENTITY_OFFSET_MOVE_SPEED 68u
#define SAMP_MATRIX_OFFSET_POS 48u
#define SAMP_GTA_ACTOR_LOCAL_ID 1
#define SAMP_GTA_PLAYER_LOCAL_ID 0
#define SAMP_DEG_TO_RAD 0.01745329251994329577f
#define SAMP_CHAT_COMPAT_MAX_LINES 8
#define SAMP_CHAT_COMPAT_LINE_BYTES 256
#define SAMP_CHAT_COMPAT_BASE_X 30
#define SAMP_CHAT_COMPAT_BASE_Y 145
#define SAMP_CHAT_COMPAT_LINE_HEIGHT 18
#define SAMP_CHAT_COMPAT_COLOR_INFO 0xFFECECECu
#define SAMP_D3D9_END_SCENE_INDEX 42u
#define SAMP_RAKNET_RPC_FLAG_GAME_STATE_MASK                                                                      \
  (SAMP_RAKNET_RPC_FLAG_PLAYER_POS | SAMP_RAKNET_RPC_FLAG_PLAYER_FACING | SAMP_RAKNET_RPC_FLAG_WEATHER |          \
   SAMP_RAKNET_RPC_FLAG_INTERIOR | SAMP_RAKNET_RPC_FLAG_CAMERA_POS | SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT)

/* Emitted in logs so test runs can be matched to the deployed DLL. */
static const char *kSampDllBuildId = __DATE__ " " __TIME__;

typedef struct samp_game_settings {
  BOOL debug;
  BOOL play_online;
  BOOL windowed_mode;
  char connect_pass[SAMP_SETTINGS_STRING_MAX + 1];
  char connect_host[SAMP_SETTINGS_STRING_MAX + 1];
  char connect_port[SAMP_SETTINGS_STRING_MAX + 1];
  char nickname[SAMP_SETTINGS_STRING_MAX + 1];
} samp_game_settings;

typedef enum samp_boot_phase {
  BOOT_PHASE_0_NONE = 0,
  BOOT_PHASE_1_RUNTIME_GUARD = 1,
  BOOT_PHASE_2_EXCEPTION_FILTER = 2,
  BOOT_PHASE_3_SETTINGS_AND_PATHS = 3,
  BOOT_PHASE_4_EARLY_BOOTSTRAP = 4,
  BOOT_PHASE_5_NETWORK_INIT = 5,
  BOOT_PHASE_6_LAUNCH_MONITOR = 6,
  BOOT_PHASE_7_READY = 7
} samp_boot_phase;

typedef enum samp_netgame_state {
  SAMP_NETGAME_WAIT_CONNECT = 0,
  SAMP_NETGAME_CONNECTING = 1,
  SAMP_NETGAME_CONNECTED = 2,
  SAMP_NETGAME_FAILED = 3
} samp_netgame_state;

typedef void(__cdecl *samp_script_process_fn)(void);
typedef HANDLE(WINAPI *samp_create_file_a_fn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL(WINAPI *samp_read_file_fn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD(WINAPI *samp_get_file_size_fn)(HANDLE, LPDWORD);
typedef DWORD(WINAPI *samp_set_file_pointer_fn)(HANDLE, LONG, PLONG, DWORD);
typedef BOOL(WINAPI *samp_close_handle_fn)(HANDLE);
typedef DWORD(WINAPI *samp_get_file_type_fn)(HANDLE);
typedef int(WINAPI *samp_show_cursor_fn)(BOOL);
typedef HRESULT(WINAPI *samp_d3d9_end_scene_fn)(void *);

typedef struct samp_id3dx_font_compat samp_id3dx_font_compat;
typedef struct samp_id3dx_font_compat_vtbl {
  HRESULT(WINAPI *QueryInterface)(samp_id3dx_font_compat *, REFIID, void **);
  ULONG(WINAPI *AddRef)(samp_id3dx_font_compat *);
  ULONG(WINAPI *Release)(samp_id3dx_font_compat *);
  HRESULT(WINAPI *GetDevice)(samp_id3dx_font_compat *, void **);
  HRESULT(WINAPI *GetDescA)(samp_id3dx_font_compat *, void *);
  HRESULT(WINAPI *GetDescW)(samp_id3dx_font_compat *, void *);
  BOOL(WINAPI *GetTextMetricsA)(samp_id3dx_font_compat *, TEXTMETRICA *);
  BOOL(WINAPI *GetTextMetricsW)(samp_id3dx_font_compat *, TEXTMETRICW *);
  HDC(WINAPI *GetDC)(samp_id3dx_font_compat *);
  HRESULT(WINAPI *GetGlyphData)(samp_id3dx_font_compat *, UINT, void **, RECT *, POINT *);
  HRESULT(WINAPI *PreloadCharacters)(samp_id3dx_font_compat *, UINT, UINT);
  HRESULT(WINAPI *PreloadGlyphs)(samp_id3dx_font_compat *, UINT, UINT);
  HRESULT(WINAPI *PreloadTextA)(samp_id3dx_font_compat *, LPCSTR, INT);
  HRESULT(WINAPI *PreloadTextW)(samp_id3dx_font_compat *, LPCWSTR, INT);
  INT(WINAPI *DrawTextA)(samp_id3dx_font_compat *, void *, LPCSTR, INT, LPRECT, DWORD, DWORD);
  INT(WINAPI *DrawTextW)(samp_id3dx_font_compat *, void *, LPCWSTR, INT, LPRECT, DWORD, DWORD);
  HRESULT(WINAPI *OnLostDevice)(samp_id3dx_font_compat *);
  HRESULT(WINAPI *OnResetDevice)(samp_id3dx_font_compat *);
} samp_id3dx_font_compat_vtbl;

struct samp_id3dx_font_compat {
  const samp_id3dx_font_compat_vtbl *lpVtbl;
};

typedef HRESULT(WINAPI *samp_d3dx_create_font_a_fn)(void *, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD,
                                                    LPCSTR, samp_id3dx_font_compat **);

typedef struct samp_bootstrap_shims {
  HMODULE kernel32_module;
  HMODULE user32_module;
  samp_create_file_a_fn create_file_a;
  samp_read_file_fn read_file;
  samp_get_file_size_fn get_file_size;
  samp_set_file_pointer_fn set_file_pointer;
  samp_close_handle_fn close_handle;
  samp_get_file_type_fn get_file_type;
  samp_show_cursor_fn show_cursor;
} samp_bootstrap_shims;

typedef struct samp_runtime_state {
  HINSTANCE instance;
  LPTOP_LEVEL_EXCEPTION_FILTER previous_filter;

  HANDLE launch_thread;
  HANDLE stop_event;

  char module_path[MAX_PATH];
  char module_dir[MAX_PATH];
  char archive_path[MAX_PATH];
  DWORD module_path_len;
  int archive_present;

  LONG process_attach_count;
  LONG thread_attach_count;
  LONG thread_detach_count;
  LONG initialized;

  LONG entry_gate;
  LONG game_inited;
  LONG start_game_called;
  LONG pregame_patches_applied;
  LONG ingame_patches_applied;
  LONG hooks_install_attempted;
  LONG hooks_installed;
  LONG hook_callback_calls;
  LONG network_inited;
  LONG do_init_stuff_calls;
  LONG do_init_stuff_active;
  LONG do_init_stuff_reentry_skips;
  LONG early_bootstrap_ready;
  LONG graphics_ticks;
  LONG monitor_started;
  LONG monitor_stopped;
  LONG net_mgr_inited;
  LONG net_mgr_connect_attempted;
  LONG game_process_hook_installed;
  LONG script_hook_installed;
  LONG net_mgr_connected;
  LONG netgame_state;
  LONG raknet_pump_calls;
  LONG raknet_join_sent;
  LONG raknet_profile_ready;
  LONG raknet_last_packet_id;
  LONG raknet_rpc_flags;
  LONG raknet_request_class_outcome;
  LONG raknet_request_spawn_outcome;
  LONG raknet_last_dialog_id;
  LONG raknet_game_rpc_flags;
  LONG raknet_init_game_applied;
  LONG online_session_drift_seen;
  LONG online_session_last_entry;
  LONG online_session_last_game_started;
  LONG preconnect_apply_count;
  LONG preconnect_ready;
  LONG preconnect_wait_logged;
  LONG preconnect_fallback_logged;
  LONG preconnect_started_logged;
  LONG preconnect_no_ped_fallback_logged;
  LONG preconnect_ped_seen_logged;
  LONG chat_connecting_logged;
  LONG chat_joining_logged;
  LONG chat_overlay_draw_logged;
  LONG chat_overlay_line_count;
  LONG chat_client_message_seq;
  LONG chat_d3d_hooked;
  LONG chat_d3d_hook_install_logged;
  LONG chat_d3d_draw_logged;
  LONG chat_d3d_draw_active;
  LONG chat_d3d_font_fail_logged;
  LONG mp_session_apply_count;
  LONG mp_session_teleport_count;
  LONG mp_session_script_failures;
  LONG mp_session_spawn_finalized;
  LONG mp_session_frontend_hold_logged;
  LONG time_passing_patch_applied;
  LONG script_process_suppressed_logged;

  int endpoint_valid;
  int connect_probe_attempted;
  int connect_probe_ok;
  int archive_probe_ok;
  DWORD archive_probe_size;
  DWORD archive_probe_type;
  int gtaweap3_font_added;
  int sampaux3_font_added;
  int script_gate_page_unprotected;
  DWORD preconnect_start_tick;
  DWORD preconnect_ped_seen_tick;
  uint8_t time_passing_saved_byte;
  uint8_t raknet_weather;
  uint8_t raknet_interior;
  uint8_t raknet_spawn_team;
  uint8_t raknet_camera_look_at_type;
  uint8_t raknet_init_world_time;
  uint8_t raknet_init_weather;
  uint8_t raknet_init_lan_mode;
  uint8_t raknet_init_disable_enter_exits;
  uint8_t raknet_init_stunt_bonus;
  uint16_t raknet_init_spawns_available;
  uint16_t raknet_init_local_player_id;
  int32_t raknet_spawn_skin;
  int32_t raknet_init_death_drop_money;
  DWORD script_gate_old_protect;
  DWORD net_mgr_last_connect_attempt_tick;
  void *bootstrap_manager_block;
  uintptr_t game_process_hook_stub;
  uintptr_t script_hook_original_target;
  uint8_t game_process_hook_saved_code[6];
  uint8_t game_process_hook_saved_storage[4];
  uint8_t script_hook_saved_disp[4];
  uint8_t script_thread[SAMP_SCRIPT_THREAD_BYTES];
  uint8_t script_buf[SAMP_SCRIPT_BUF_BYTES];
  float raknet_player_pos[3];
  float raknet_player_facing_angle;
  float raknet_camera_pos[3];
  float raknet_camera_look_at[3];
  float raknet_init_gravity;
  float raknet_init_name_tag_draw_distance;
  float raknet_init_global_chat_radius;
  float raknet_spawn_pos[3];
  float raknet_spawn_rotation;
  int32_t raknet_spawn_weapons[3];
  int32_t raknet_spawn_weapon_ammo[3];
  int32_t raknet_init_send_rates[6];
  uint8_t raknet_init_vehicle_models[SAMP_RAKNET_REQUIRED_VEHICLE_MODELS];
  char raknet_init_hostname[SAMP_RAKNET_HOSTNAME_BYTES];
  HFONT chat_overlay_font;
  DWORD chat_overlay_colors[SAMP_CHAT_COMPAT_MAX_LINES];
  char chat_overlay_lines[SAMP_CHAT_COMPAT_MAX_LINES][SAMP_CHAT_COMPAT_LINE_BYTES];
  void *chat_d3d_device;
  void **chat_d3d_vtbl;
  samp_d3d9_end_scene_fn chat_end_scene_original;
  samp_d3dx_create_font_a_fn d3dx_create_font_a;
  samp_id3dx_font_compat *chat_d3dx_font;
  char gtaweap3_font_path[MAX_PATH];
  char sampaux3_font_path[MAX_PATH];
  samp_endpoint endpoint;
  samp_raknet_join_profile raknet_join_profile;
  HMODULE d3dx9_module;
  HMODULE bass_module;
  samp_bootstrap_shims bootstrap_shims;
  samp_tcp_bootstrap_manager net_mgr;
  samp_runtime_hook_bridge hook_bridge;
  samp_game_settings settings;
} samp_runtime_state;

static samp_runtime_state g_runtime;
static samp_boot_phase g_last_phase = BOOT_PHASE_0_NONE;
static samp_script_process_fn g_script_process_original = NULL;
static LONG read_game_entry_gate_value(void);
static HRESULT WINAPI chat_compat_end_scene_hook(void *device);
static const uint8_t kGameProcessHookJmpCode[6] = {0xFFu, 0x25u, 0xD1u, 0xBEu, 0x53u, 0x00u};

static int runtime_trace_enabled(void) {
  static LONG initialized = 0;
  static int enabled = 0;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_TRACE");
  if (value != NULL && *value != '\0' && value[0] != '0') {
    enabled = 1;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static int monitor_drive_init_enabled(void) {
  static LONG initialized = 0;
  static int enabled = 1;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_MONITOR_DRIVE_INIT");
  if (value != NULL && *value != '\0' && value[0] == '0') {
    enabled = 0;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static int run_game_scripts_enabled(void) {
  static LONG initialized = 0;
  static int enabled = 1;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_RUN_GAME_SCRIPTS");
  if (value != NULL && *value != '\0' && value[0] == '0') {
    enabled = 0;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static void runtime_trace_file_line(const char *line) {
  char path[MAX_PATH];
  FILE *file = NULL;

  if (line == NULL || line[0] == '\0') {
    return;
  }

  if (g_runtime.module_dir[0] != '\0') {
    int written = snprintf(path, sizeof(path), "%ssamp_runtime.log", g_runtime.module_dir);
    if (written <= 0 || (size_t)written >= sizeof(path)) {
      return;
    }
  } else {
    strcpy(path, "samp_runtime.log");
  }

  file = fopen(path, "ab");
  if (file == NULL) {
    return;
  }
  fputs(line, file);
  fputc('\n', file);
  fclose(file);
}

static void runtime_tracef(const char *fmt, ...) {
  char message[768];
  char payload[640];
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

  (void)snprintf(message, sizeof(message), "[sampdll-runtime] %s", payload);
  runtime_trace_file_line(message);
  if (runtime_trace_enabled()) {
    OutputDebugStringA(message);
  }
}

static int chat_overlay_enabled_compat(void) {
  static LONG initialized = 0;
  static int enabled = 1;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_CHAT_OVERLAY");
  if (value != NULL && *value != '\0' && value[0] == '0') {
    enabled = 0;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static int chat_d3d_enabled_compat(void) {
  static LONG initialized = 0;
  static int enabled = 1;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_CHAT_D3D");
  if (value != NULL && *value != '\0' && value[0] == '0') {
    enabled = 0;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static int chat_d3d_early_enabled_compat(void) {
  static LONG initialized = 0;
  static int enabled = 0;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_CHAT_D3D_EARLY");
  if (value != NULL && *value != '\0' &&
      (value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T')) {
    enabled = 1;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static LONG chat_overlay_y_compat(void) {
  static LONG initialized = 0;
  static LONG value_y = SAMP_CHAT_COMPAT_BASE_Y;
  const char *env_value = NULL;
  char *endptr = NULL;
  long parsed = 0;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return value_y;
  }

  env_value = getenv("SAMPDLL_CHAT_Y");
  if (env_value != NULL && *env_value != '\0') {
    parsed = strtol(env_value, &endptr, 10);
    if (endptr != env_value && *endptr == '\0' && parsed >= 0 && parsed <= 1000) {
      value_y = (LONG)parsed;
    }
  }
  InterlockedExchange(&initialized, 1);
  return value_y;
}

static DWORD chat_compat_samp_color_to_argb(uint32_t color) {
  return (DWORD)((color >> 8u) | 0xFF000000u);
}

static COLORREF chat_compat_colorref_from_argb(DWORD color) {
  return RGB((BYTE)((color >> 16u) & 0xFFu), (BYTE)((color >> 8u) & 0xFFu), (BYTE)(color & 0xFFu));
}

static void chat_compat_filter_invalid_chars(char *line) {
  if (line == NULL) {
    return;
  }
  while (*line != '\0') {
    if (*line > 0 && *line < ' ') {
      *line = ' ';
    }
    ++line;
  }
}

static int chat_compat_is_hex_digit(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

static void chat_compat_strip_samp_color_tags(char *line) {
  char *read_cursor = line;
  char *write_cursor = line;

  if (line == NULL) {
    return;
  }

  while (*read_cursor != '\0') {
    size_t remaining = strlen(read_cursor);
    if (remaining >= 8u && read_cursor[0] == '{' && chat_compat_is_hex_digit(read_cursor[1]) &&
        chat_compat_is_hex_digit(read_cursor[2]) && chat_compat_is_hex_digit(read_cursor[3]) &&
        chat_compat_is_hex_digit(read_cursor[4]) && chat_compat_is_hex_digit(read_cursor[5]) &&
        chat_compat_is_hex_digit(read_cursor[6]) && read_cursor[7] == '}') {
      read_cursor += 8;
      continue;
    }
    *write_cursor++ = *read_cursor++;
  }
  *write_cursor = '\0';
}

static void chat_compat_add_colored_line(DWORD argb_color, const char *text) {
  char line[SAMP_CHAT_COMPAT_LINE_BYTES];
  LONG count = 0;
  LONG index = 0;

  if (text == NULL || text[0] == '\0') {
    return;
  }

  strncpy(line, text, sizeof(line) - 1u);
  line[sizeof(line) - 1u] = '\0';
  chat_compat_filter_invalid_chars(line);
  chat_compat_strip_samp_color_tags(line);
  if (line[0] == '\0') {
    return;
  }

  count = InterlockedCompareExchange(&g_runtime.chat_overlay_line_count, 0, 0);
  if (count > 0 && count <= SAMP_CHAT_COMPAT_MAX_LINES &&
      strcmp(g_runtime.chat_overlay_lines[count - 1], line) == 0 &&
      g_runtime.chat_overlay_colors[count - 1] == argb_color) {
    return;
  }

  if (count < SAMP_CHAT_COMPAT_MAX_LINES) {
    index = InterlockedIncrement(&g_runtime.chat_overlay_line_count) - 1;
    if (index < 0 || index >= SAMP_CHAT_COMPAT_MAX_LINES) {
      return;
    }
    strncpy(g_runtime.chat_overlay_lines[index], line, SAMP_CHAT_COMPAT_LINE_BYTES - 1u);
    g_runtime.chat_overlay_lines[index][SAMP_CHAT_COMPAT_LINE_BYTES - 1u] = '\0';
    g_runtime.chat_overlay_colors[index] = argb_color;
  } else {
    int i = 0;
    for (i = 1; i < SAMP_CHAT_COMPAT_MAX_LINES; ++i) {
      memcpy(g_runtime.chat_overlay_lines[i - 1], g_runtime.chat_overlay_lines[i], SAMP_CHAT_COMPAT_LINE_BYTES);
      g_runtime.chat_overlay_colors[i - 1] = g_runtime.chat_overlay_colors[i];
    }
    strncpy(g_runtime.chat_overlay_lines[SAMP_CHAT_COMPAT_MAX_LINES - 1], line, SAMP_CHAT_COMPAT_LINE_BYTES - 1u);
    g_runtime.chat_overlay_lines[SAMP_CHAT_COMPAT_MAX_LINES - 1][SAMP_CHAT_COMPAT_LINE_BYTES - 1u] = '\0';
    g_runtime.chat_overlay_colors[SAMP_CHAT_COMPAT_MAX_LINES - 1] = argb_color;
  }

  runtime_tracef("chat_compat: color=0x%08lx %s", (unsigned long)argb_color, line);
}

static void chat_compat_add_message(const char *fmt, ...) {
  char line[SAMP_CHAT_COMPAT_LINE_BYTES];
  va_list args;

  if (fmt == NULL) {
    return;
  }

  va_start(args, fmt);
  if (vsnprintf(line, sizeof(line), fmt, args) < 0) {
    va_end(args);
    return;
  }
  va_end(args);
  line[sizeof(line) - 1u] = '\0';

  chat_compat_add_colored_line(SAMP_CHAT_COMPAT_COLOR_INFO, line);
}

static HWND read_game_hwnd_compat(void) {
  uintptr_t hwnd_value = (uintptr_t)(*(volatile uint32_t *)(uintptr_t)SAMP_ADDR_HWND);
  HWND hwnd = (HWND)hwnd_value;

  if (hwnd == NULL || !IsWindow(hwnd)) {
    return NULL;
  }
  return hwnd;
}

static void *read_game_d3d_device_compat(void) {
  uintptr_t device_value = (uintptr_t)(*(volatile uint32_t *)(uintptr_t)SAMP_ADDR_ID3D9DEVICE);
  return (void *)device_value;
}

static int memory_is_readable_compat(const void *ptr, size_t size) {
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t start = (uintptr_t)ptr;
  uintptr_t end = start + size;
  uintptr_t region_end = 0;

  if (ptr == NULL || size == 0u || end < start) {
    return 0;
  }
  if (VirtualQuery(ptr, &mbi, sizeof(mbi)) != sizeof(mbi)) {
    return 0;
  }
  if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
    return 0;
  }
  region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
  return end <= region_end ? 1 : 0;
}

static void chat_compat_draw_text_outline(HDC dc, int x, int y, const char *text, DWORD argb_color) {
  if (dc == NULL || text == NULL || text[0] == '\0') {
    return;
  }

  SetTextColor(dc, RGB(0, 0, 0));
  TextOutA(dc, x - 1, y, text, (int)strlen(text));
  TextOutA(dc, x + 1, y, text, (int)strlen(text));
  TextOutA(dc, x, y - 1, text, (int)strlen(text));
  TextOutA(dc, x, y + 1, text, (int)strlen(text));
  SetTextColor(dc, chat_compat_colorref_from_argb(argb_color));
  TextOutA(dc, x, y, text, (int)strlen(text));
}

static void chat_compat_release_d3dx_font(void) {
  samp_id3dx_font_compat *font = g_runtime.chat_d3dx_font;
  if (font != NULL && font->lpVtbl != NULL && font->lpVtbl->Release != NULL) {
    font->lpVtbl->Release(font);
  }
  g_runtime.chat_d3dx_font = NULL;
  g_runtime.chat_d3d_device = NULL;
}

static int chat_compat_resolve_d3dx_create_font(void) {
  FARPROC proc = NULL;

  if (g_runtime.d3dx_create_font_a != NULL) {
    return 1;
  }
  if (g_runtime.d3dx9_module == NULL) {
    g_runtime.d3dx9_module = LoadLibraryA("d3dx9_25.dll");
    runtime_tracef("chat_d3dx: d3dx9_25 load %s", (g_runtime.d3dx9_module != NULL) ? "OK" : "FAILED");
  }
  if (g_runtime.d3dx9_module == NULL) {
    return 0;
  }

  proc = GetProcAddress(g_runtime.d3dx9_module, "D3DXCreateFontA");
  if (proc == NULL) {
    if (InterlockedCompareExchange(&g_runtime.chat_d3d_font_fail_logged, 1, 0) == 0) {
      runtime_tracef("chat_d3dx: D3DXCreateFontA missing");
    }
    return 0;
  }
  g_runtime.d3dx_create_font_a = (samp_d3dx_create_font_a_fn)proc;
  return 1;
}

static int chat_compat_font_size(void) {
  const char *value = getenv("SAMPDLL_CHAT_FONT_SIZE");
  char *endptr = NULL;
  long parsed = 0;

  if (value != NULL && *value != '\0') {
    parsed = strtol(value, &endptr, 10);
    if (endptr != value && *endptr == '\0' && parsed >= 8 && parsed <= 32) {
      return (int)parsed;
    }
  }
  return 14;
}

static int preconnect_require_ped_compat(void) {
  static LONG initialized = 0;
  static int enabled = 1;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_PRECONNECT_REQUIRE_PED");
  if (value != NULL && *value != '\0' && value[0] == '0') {
    enabled = 0;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static DWORD preconnect_world_settle_ms_compat(void) {
  static LONG initialized = 0;
  static DWORD settle_ms = SAMP_PRECONNECT_WORLD_SETTLE_MS;
  const char *value = NULL;
  char *endptr = NULL;
  unsigned long parsed = 0;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return settle_ms;
  }

  value = getenv("SAMPDLL_PRECONNECT_WORLD_SETTLE_MS");
  if (value != NULL && *value != '\0') {
    parsed = strtoul(value, &endptr, 10);
    if (endptr != value && *endptr == '\0' && parsed <= 10000ul) {
      settle_ms = (DWORD)parsed;
    }
  }
  InterlockedExchange(&initialized, 1);
  return settle_ms;
}

static int preconnect_remove_from_vehicle_compat(void) {
  static LONG initialized = 0;
  static int enabled = 0;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_PRECONNECT_REMOVE_FROM_VEHICLE");
  if (value != NULL && *value != '\0' &&
      (value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T')) {
    enabled = 1;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static int chat_compat_ensure_d3dx_font(void *device) {
  HRESULT hr = 0;
  samp_id3dx_font_compat *font = NULL;

  if (device == NULL) {
    return 0;
  }
  if (g_runtime.chat_d3dx_font != NULL && g_runtime.chat_d3d_device == device) {
    return 1;
  }

  chat_compat_release_d3dx_font();
  if (!chat_compat_resolve_d3dx_create_font()) {
    return 0;
  }

  hr = g_runtime.d3dx_create_font_a(device, chat_compat_font_size(), 0u, FW_BOLD, 1u, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &font);
  if (FAILED(hr) || font == NULL) {
    if (InterlockedCompareExchange(&g_runtime.chat_d3d_font_fail_logged, 1, 0) == 0) {
      runtime_tracef("chat_d3dx: font create failed hr=0x%08lx device=0x%08lx", (unsigned long)hr,
                     (unsigned long)(uintptr_t)device);
    }
    return 0;
  }

  g_runtime.chat_d3dx_font = font;
  g_runtime.chat_d3d_device = device;
  InterlockedExchange(&g_runtime.chat_d3d_font_fail_logged, 0);
  runtime_tracef("chat_d3dx: font created device=0x%08lx size=%d", (unsigned long)(uintptr_t)device,
                 chat_compat_font_size());
  return 1;
}

static void chat_compat_viewport_origin(int *out_x, int *out_y) {
  HWND hwnd = NULL;
  RECT client_rect;
  int viewport_x = 0;
  int viewport_w = 0;

  if (out_x != NULL) {
    *out_x = SAMP_CHAT_COMPAT_BASE_X;
  }
  if (out_y != NULL) {
    *out_y = (int)chat_overlay_y_compat();
  }

  hwnd = read_game_hwnd_compat();
  if (hwnd == NULL || !GetClientRect(hwnd, &client_rect)) {
    return;
  }

  viewport_w = (client_rect.bottom > client_rect.top) ? ((client_rect.bottom - client_rect.top) * 4 / 3) : 0;
  if (viewport_w > 0 && viewport_w < (client_rect.right - client_rect.left)) {
    viewport_x = ((client_rect.right - client_rect.left) - viewport_w) / 2;
  }
  if (out_x != NULL) {
    *out_x += viewport_x;
  }
}

static void chat_compat_d3dx_draw_text_outline(samp_id3dx_font_compat *font, RECT rect, const char *text,
                                               DWORD argb_color) {
  if (font == NULL || font->lpVtbl == NULL || font->lpVtbl->DrawTextA == NULL || text == NULL || text[0] == '\0') {
    return;
  }

  rect.top -= 1;
  font->lpVtbl->DrawTextA(font, NULL, text, -1, &rect, DT_NOCLIP | DT_SINGLELINE | DT_LEFT, 0xFF000000u);
  rect.top += 2;
  font->lpVtbl->DrawTextA(font, NULL, text, -1, &rect, DT_NOCLIP | DT_SINGLELINE | DT_LEFT, 0xFF000000u);
  rect.top -= 1;
  rect.left -= 1;
  font->lpVtbl->DrawTextA(font, NULL, text, -1, &rect, DT_NOCLIP | DT_SINGLELINE | DT_LEFT, 0xFF000000u);
  rect.left += 2;
  font->lpVtbl->DrawTextA(font, NULL, text, -1, &rect, DT_NOCLIP | DT_SINGLELINE | DT_LEFT, 0xFF000000u);
  rect.left -= 1;
  font->lpVtbl->DrawTextA(font, NULL, text, -1, &rect, DT_NOCLIP | DT_SINGLELINE | DT_LEFT, argb_color);
}

static int chat_compat_draw_d3dx_overlay(void *device) {
  LONG count = 0;
  int x = 0;
  int y = 0;
  int i = 0;

  if (!chat_overlay_enabled_compat()) {
    return 0;
  }
  count = InterlockedCompareExchange(&g_runtime.chat_overlay_line_count, 0, 0);
  if (count <= 0) {
    return 0;
  }
  if (count > SAMP_CHAT_COMPAT_MAX_LINES) {
    count = SAMP_CHAT_COMPAT_MAX_LINES;
  }
  if (!chat_compat_ensure_d3dx_font(device)) {
    return 0;
  }

  chat_compat_viewport_origin(&x, &y);
  for (i = 0; i < count; ++i) {
    RECT rect;
    rect.left = x;
    rect.top = y + (i * SAMP_CHAT_COMPAT_LINE_HEIGHT);
    rect.right = x + 900;
    rect.bottom = rect.top + SAMP_CHAT_COMPAT_LINE_HEIGHT + 4;
    chat_compat_d3dx_draw_text_outline(g_runtime.chat_d3dx_font, rect, g_runtime.chat_overlay_lines[i],
                                       g_runtime.chat_overlay_colors[i] != 0u ? g_runtime.chat_overlay_colors[i]
                                                                              : SAMP_CHAT_COMPAT_COLOR_INFO);
  }

  if (InterlockedCompareExchange(&g_runtime.chat_d3d_draw_logged, 1, 0) == 0) {
    runtime_tracef("chat_d3dx: drawing enabled device=0x%08lx lines=%ld x=%d y=%d",
                   (unsigned long)(uintptr_t)device, (long)count, x, y);
  }
  return 1;
}

static HRESULT WINAPI chat_compat_end_scene_hook(void *device) {
  samp_d3d9_end_scene_fn original = g_runtime.chat_end_scene_original;

  if (InterlockedCompareExchange(&g_runtime.chat_d3d_draw_active, 1, 0) == 0) {
    (void)chat_compat_draw_d3dx_overlay(device);
    InterlockedExchange(&g_runtime.chat_d3d_draw_active, 0);
  }

  if (original != NULL) {
    return original(device);
  }
  return S_OK;
}

static void chat_compat_try_install_d3d_hook(void) {
  void *device = NULL;
  void **vtbl = NULL;
  DWORD old_protect = 0;
  DWORD ignored_protect = 0;

  if (!chat_overlay_enabled_compat() || !chat_d3d_enabled_compat() ||
      InterlockedCompareExchange(&g_runtime.chat_d3d_hooked, 0, 0) != 0) {
    return;
  }
  if (g_runtime.settings.play_online && InterlockedCompareExchange(&g_runtime.preconnect_ready, 0, 0) == 0 &&
      !chat_d3d_early_enabled_compat()) {
    return;
  }

  device = read_game_d3d_device_compat();
  if (device == NULL) {
    return;
  }
  if (!memory_is_readable_compat(device, sizeof(void **))) {
    return;
  }

  vtbl = *(void ***)device;
  if (vtbl == NULL || !memory_is_readable_compat(&vtbl[SAMP_D3D9_END_SCENE_INDEX], sizeof(void *)) ||
      vtbl[SAMP_D3D9_END_SCENE_INDEX] == NULL ||
      vtbl[SAMP_D3D9_END_SCENE_INDEX] == (void *)chat_compat_end_scene_hook) {
    return;
  }

  if (!VirtualProtect(&vtbl[SAMP_D3D9_END_SCENE_INDEX], sizeof(void *), PAGE_EXECUTE_READWRITE, &old_protect)) {
    if (InterlockedCompareExchange(&g_runtime.chat_d3d_hook_install_logged, 1, 0) == 0) {
      runtime_tracef("chat_d3d: EndScene hook VirtualProtect failed vtbl=0x%08lx",
                     (unsigned long)(uintptr_t)vtbl);
    }
    return;
  }

  g_runtime.chat_d3d_vtbl = vtbl;
  g_runtime.chat_end_scene_original = (samp_d3d9_end_scene_fn)vtbl[SAMP_D3D9_END_SCENE_INDEX];
  vtbl[SAMP_D3D9_END_SCENE_INDEX] = (void *)chat_compat_end_scene_hook;
  FlushInstructionCache(GetCurrentProcess(), &vtbl[SAMP_D3D9_END_SCENE_INDEX], sizeof(void *));
  (void)VirtualProtect(&vtbl[SAMP_D3D9_END_SCENE_INDEX], sizeof(void *), old_protect, &ignored_protect);
  InterlockedExchange(&g_runtime.chat_d3d_hooked, 1);

  runtime_tracef("chat_d3d: EndScene hook installed device=0x%08lx vtbl=0x%08lx original=0x%08lx hook=0x%08lx",
                 (unsigned long)(uintptr_t)device, (unsigned long)(uintptr_t)vtbl,
                 (unsigned long)(uintptr_t)g_runtime.chat_end_scene_original,
                 (unsigned long)(uintptr_t)chat_compat_end_scene_hook);
}

static void chat_compat_uninstall_d3d_hook(void) {
  DWORD old_protect = 0;
  DWORD ignored_protect = 0;

  if (g_runtime.chat_d3d_vtbl == NULL || g_runtime.chat_end_scene_original == NULL) {
    return;
  }

  if (g_runtime.chat_d3d_vtbl[SAMP_D3D9_END_SCENE_INDEX] == (void *)chat_compat_end_scene_hook &&
      VirtualProtect(&g_runtime.chat_d3d_vtbl[SAMP_D3D9_END_SCENE_INDEX], sizeof(void *), PAGE_EXECUTE_READWRITE,
                     &old_protect)) {
    g_runtime.chat_d3d_vtbl[SAMP_D3D9_END_SCENE_INDEX] = (void *)g_runtime.chat_end_scene_original;
    FlushInstructionCache(GetCurrentProcess(), &g_runtime.chat_d3d_vtbl[SAMP_D3D9_END_SCENE_INDEX], sizeof(void *));
    (void)VirtualProtect(&g_runtime.chat_d3d_vtbl[SAMP_D3D9_END_SCENE_INDEX], sizeof(void *), old_protect,
                         &ignored_protect);
    runtime_tracef("chat_d3d: EndScene hook restored");
  }

  InterlockedExchange(&g_runtime.chat_d3d_hooked, 0);
  g_runtime.chat_d3d_vtbl = NULL;
  g_runtime.chat_end_scene_original = NULL;
}

static void chat_compat_draw_overlay(void) {
  LONG count = 0;
  HWND hwnd = NULL;
  HDC dc = NULL;
  HFONT old_font = NULL;
  int x = SAMP_CHAT_COMPAT_BASE_X;
  int y = (int)chat_overlay_y_compat();
  int i = 0;

  if (!chat_overlay_enabled_compat()) {
    return;
  }

  chat_compat_try_install_d3d_hook();
  if (chat_d3d_enabled_compat() && InterlockedCompareExchange(&g_runtime.chat_d3d_hooked, 0, 0) != 0) {
    return;
  }

  count = InterlockedCompareExchange(&g_runtime.chat_overlay_line_count, 0, 0);
  if (count <= 0) {
    return;
  }
  if (count > SAMP_CHAT_COMPAT_MAX_LINES) {
    count = SAMP_CHAT_COMPAT_MAX_LINES;
  }

  hwnd = read_game_hwnd_compat();
  if (hwnd == NULL) {
    return;
  }
  chat_compat_viewport_origin(&x, &y);

  dc = GetDC(hwnd);
  if (dc == NULL) {
    return;
  }

  if (g_runtime.chat_overlay_font == NULL) {
    g_runtime.chat_overlay_font =
        CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
  }
  if (g_runtime.chat_overlay_font != NULL) {
    old_font = (HFONT)SelectObject(dc, g_runtime.chat_overlay_font);
  }

  SetBkMode(dc, TRANSPARENT);
  for (i = 0; i < count; ++i) {
    chat_compat_draw_text_outline(dc, x, y + (i * SAMP_CHAT_COMPAT_LINE_HEIGHT), g_runtime.chat_overlay_lines[i],
                                  g_runtime.chat_overlay_colors[i] != 0u ? g_runtime.chat_overlay_colors[i]
                                                                         : SAMP_CHAT_COMPAT_COLOR_INFO);
  }

  if (old_font != NULL) {
    SelectObject(dc, old_font);
  }
  ReleaseDC(hwnd, dc);

  if (InterlockedCompareExchange(&g_runtime.chat_overlay_draw_logged, 1, 0) == 0) {
    runtime_tracef("chat_overlay: drawing enabled hwnd=0x%08lx lines=%ld x=%d y=%d",
                   (unsigned long)(uintptr_t)hwnd, (long)count, x, y);
  }
}

static LONG WINAPI samp_exception_filter(struct _EXCEPTION_POINTERS *exception_info) {
  unsigned long code = 0;
  unsigned long addr = 0;
  LONG phase = g_last_phase;
  LONG entry_mem = read_game_entry_gate_value();
  LONG entry_target = InterlockedCompareExchange(&g_runtime.entry_gate, 0, 0);
  LONG net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  LONG hooks = InterlockedCompareExchange(&g_runtime.hooks_installed, 0, 0);
  LONG hook_calls = InterlockedCompareExchange(&g_runtime.hook_callback_calls, 0, 0);

  if (exception_info != NULL && exception_info->ExceptionRecord != NULL) {
    code = (unsigned long)exception_info->ExceptionRecord->ExceptionCode;
    addr = (unsigned long)(uintptr_t)exception_info->ExceptionRecord->ExceptionAddress;
  }

  runtime_tracef("exception_filter: code=0x%08lx addr=0x%08lx phase=%ld entry=%ld target=%ld net=%ld hooks=%ld callbacks=%ld",
                 code, addr, (long)phase, (long)entry_mem, (long)entry_target, (long)net_state, (long)hooks,
                 (long)hook_calls);
  return EXCEPTION_CONTINUE_SEARCH;
}

static int build_module_relative_path(const char *filename, char *out_path, size_t out_path_size) {
  int written = 0;

  if (filename == NULL || *filename == '\0' || out_path == NULL || out_path_size == 0u) {
    return 0;
  }
  if (g_runtime.module_dir[0] == '\0') {
    return 0;
  }

  written = snprintf(out_path, out_path_size, "%s%s", g_runtime.module_dir, filename);
  if (written <= 0 || (size_t)written >= out_path_size) {
    out_path[0] = '\0';
    return 0;
  }
  return 1;
}

static int resolve_module_proc(HMODULE *module_slot, const char *module_name, const char *proc_name, FARPROC *out_proc) {
  HMODULE module_handle = NULL;
  FARPROC proc = NULL;

  if (module_slot == NULL || module_name == NULL || proc_name == NULL || out_proc == NULL) {
    return 0;
  }

  module_handle = *module_slot;
  if (module_handle == NULL) {
    module_handle = GetModuleHandleA(module_name);
    if (module_handle == NULL) {
      module_handle = LoadLibraryA(module_name);
    }
    *module_slot = module_handle;
  }
  if (module_handle == NULL) {
    return 0;
  }

  proc = GetProcAddress(module_handle, proc_name);
  if (proc == NULL) {
    return 0;
  }

  *out_proc = proc;
  return 1;
}

static void copy_cmd_token(const char *src, char *dst, size_t dst_size) {
  size_t i = 0;

  if (dst == NULL || dst_size == 0) {
    return;
  }

  dst[0] = '\0';
  if (src == NULL) {
    return;
  }

  while (*src == ' ') {
    ++src;
  }

  while (*src != '\0' && *src != ' ' && *src != '-' && *src != '/' && i + 1 < dst_size) {
    dst[i++] = *src++;
  }
  dst[i] = '\0';
}

static int parse_u16(const char *text, uint16_t *out) {
  unsigned long value = 0;
  size_t i = 0;

  if (text == NULL || *text == '\0' || out == NULL) {
    return 0;
  }

  for (i = 0; text[i] != '\0'; ++i) {
    char c = text[i];
    if (c < '0' || c > '9') {
      return 0;
    }
    value = (value * 10UL) + (unsigned long)(c - '0');
    if (value > 65535UL) {
      return 0;
    }
  }

  *out = (uint16_t)value;
  return 1;
}

static int parse_u32(const char *text, uint32_t *out) {
  unsigned long value = 0;
  char *endptr = NULL;

  if (text == NULL || *text == '\0' || out == NULL) {
    return 0;
  }

  value = strtoul(text, &endptr, 10);
  if (endptr == text || *endptr != '\0') {
    return 0;
  }
  if (value > 0xFFFFFFFFUL) {
    return 0;
  }
  *out = (uint32_t)value;
  return 1;
}

static DWORD preconnect_delay_ms_compat(void) {
  static LONG initialized = 0;
  static DWORD delay_ms = SAMP_PRECONNECT_DELAY_MS;
  const char *value = NULL;
  uint32_t parsed = 0;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return delay_ms;
  }

  value = getenv("SAMPDLL_PRECONNECT_DELAY_MS");
  if (parse_u32(value, &parsed) && parsed <= 30000u) {
    delay_ms = (DWORD)parsed;
  }
  InterlockedExchange(&initialized, 1);
  return delay_ms;
}

static const char *raknet_packet_name(int packet_id) {
  switch (packet_id) {
    case 10:
      return "ID_OPEN_CONNECTION_REQUEST";
    case 11:
      return "ID_OPEN_CONNECTION_REPLY";
    case 16:
      return "ID_CONNECTION_REQUEST_ACCEPTED";
    case 17:
      return "ID_CONNECTION_ATTEMPT_FAILED";
    case 19:
      return "ID_NO_FREE_INCOMING_CONNECTIONS";
    case 20:
      return "ID_DISCONNECTION_NOTIFICATION";
    case 21:
      return "ID_CONNECTION_LOST";
    case 22:
      return "ID_RSA_PUBLIC_KEY_MISMATCH";
    case 18:
      return "ID_NEW_INCOMING_CONNECTION";
    case 23:
      return "ID_CONNECTION_BANNED";
    case 24:
      return "ID_INVALID_PASSWORD";
    case 25:
      return "ID_MODIFIED_PACKET";
    default:
      return "UNKNOWN";
  }
}

static void copy_runtime_token(const char *src, char *dst, size_t dst_size) {
  size_t i = 0;

  if (dst == NULL || dst_size == 0) {
    return;
  }
  dst[0] = '\0';
  if (src == NULL) {
    return;
  }

  while (src[i] != '\0' && i + 1 < dst_size) {
    dst[i] = src[i];
    ++i;
  }
  dst[i] = '\0';
}

static void configure_raknet_join_profile_once(void) {
  const char *nickname_env = getenv("SAMPDLL_NICKNAME");
  const char *version_env = getenv("SAMPDLL_CLIENT_VERSION");
  const char *netgame_version_env = getenv("SAMPDLL_NETGAME_VERSION");
  const char *client_mod_env = getenv("SAMPDLL_CLIENT_MOD");
  uint32_t netgame_version = SAMP_DEFAULT_NETGAME_VERSION;
  uint16_t client_mod = SAMP_DEFAULT_CLIENT_MOD;

  if (InterlockedCompareExchange(&g_runtime.raknet_profile_ready, 1, 0) != 0) {
    return;
  }

  memset(&g_runtime.raknet_join_profile, 0, sizeof(g_runtime.raknet_join_profile));

  if (nickname_env != NULL && nickname_env[0] != '\0') {
    copy_runtime_token(nickname_env, g_runtime.raknet_join_profile.nickname, sizeof(g_runtime.raknet_join_profile.nickname));
  } else if (g_runtime.settings.nickname[0] != '\0') {
    copy_runtime_token(g_runtime.settings.nickname, g_runtime.raknet_join_profile.nickname,
                       sizeof(g_runtime.raknet_join_profile.nickname));
  } else {
    copy_runtime_token(SAMP_DEFAULT_NICKNAME, g_runtime.raknet_join_profile.nickname,
                       sizeof(g_runtime.raknet_join_profile.nickname));
  }

  if (version_env != NULL && version_env[0] != '\0') {
    copy_runtime_token(version_env, g_runtime.raknet_join_profile.client_version,
                       sizeof(g_runtime.raknet_join_profile.client_version));
  } else {
    copy_runtime_token(SAMP_DEFAULT_CLIENT_VERSION, g_runtime.raknet_join_profile.client_version,
                       sizeof(g_runtime.raknet_join_profile.client_version));
  }

  if (parse_u32(netgame_version_env, &netgame_version)) {
    g_runtime.raknet_join_profile.netgame_version = netgame_version;
  } else {
    g_runtime.raknet_join_profile.netgame_version = SAMP_DEFAULT_NETGAME_VERSION;
  }

  if (parse_u16(client_mod_env, &client_mod) && client_mod <= 255u) {
    g_runtime.raknet_join_profile.mod_byte = (uint8_t)client_mod;
  } else {
    g_runtime.raknet_join_profile.mod_byte = (uint8_t)SAMP_DEFAULT_CLIENT_MOD;
  }

  runtime_tracef("raknet_profile: nick='%s' version='%s' netver=%lu mod=%u", g_runtime.raknet_join_profile.nickname,
                 g_runtime.raknet_join_profile.client_version, (unsigned long)g_runtime.raknet_join_profile.netgame_version,
                 (unsigned)g_runtime.raknet_join_profile.mod_byte);
}

static void load_settings_from_command_line(samp_game_settings *settings) {
  char *cursor = GetCommandLineA();

  if (settings == NULL) {
    return;
  }
  memset(settings, 0, sizeof(*settings));

  if (cursor == NULL) {
    return;
  }

  while (*cursor != '\0') {
    if (*cursor == '-' || *cursor == '/') {
      ++cursor;
      switch (*cursor) {
        case 'd':
          settings->debug = TRUE;
          settings->play_online = FALSE;
          break;
        case 'c':
          settings->play_online = TRUE;
          settings->debug = FALSE;
          break;
        case 'w':
          settings->windowed_mode = TRUE;
          break;
        case 'z':
          ++cursor;
          copy_cmd_token(cursor, settings->connect_pass, sizeof(settings->connect_pass));
          break;
        case 'h':
          ++cursor;
          copy_cmd_token(cursor, settings->connect_host, sizeof(settings->connect_host));
          break;
        case 'p':
          ++cursor;
          copy_cmd_token(cursor, settings->connect_port, sizeof(settings->connect_port));
          break;
        case 'n':
          ++cursor;
          copy_cmd_token(cursor, settings->nickname, sizeof(settings->nickname));
          break;
        default:
          break;
      }
    }

    if (*cursor == '\0') {
      break;
    }
    ++cursor;
  }
}

static LONG resolve_initial_entry_gate(void) {
  const char *env_value = getenv("SAMPDLL_ENTRY_GATE");
  char *endptr = NULL;
  long parsed = 0;

  if (env_value == NULL || *env_value == '\0') {
    return SAMP_ENTRY_GATE_READY;
  }

  parsed = strtol(env_value, &endptr, 10);
  if (endptr == env_value || *endptr != '\0') {
    return SAMP_ENTRY_GATE_READY;
  }
  if (parsed < 0 || parsed > 255) {
    return SAMP_ENTRY_GATE_READY;
  }
  return (LONG)parsed;
}

static LONG read_game_entry_gate_value(void) {
  return *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY;
}

static uint8_t read_game_u8(uintptr_t addr) {
  return *(volatile uint8_t *)(uintptr_t)addr;
}

static void write_game_u8(uintptr_t addr, uint8_t value) {
  *(volatile uint8_t *)(uintptr_t)addr = value;
}

static int patch_nop(uintptr_t addr, size_t size) {
  DWORD old_protect = 0;
  DWORD old_protect_restore = 0;

  if (addr == 0u || size == 0u) {
    return 0;
  }
  if (!VirtualProtect((LPVOID)addr, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
    return 0;
  }
  memset((void *)addr, 0x90, size);
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addr, size);
  (void)VirtualProtect((LPVOID)addr, size, old_protect, &old_protect_restore);
  return 1;
}

static int patch_copy(uintptr_t addr, const void *src, size_t size) {
  DWORD old_protect = 0;
  DWORD old_protect_restore = 0;

  if (addr == 0u || src == NULL || size == 0u) {
    return 0;
  }
  if (!VirtualProtect((LPVOID)addr, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
    return 0;
  }
  memcpy((void *)addr, src, size);
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addr, size);
  (void)VirtualProtect((LPVOID)addr, size, old_protect, &old_protect_restore);
  return 1;
}

static int install_call_disp_hook_compat(uintptr_t call_disp_addr, void *hook_target, uint8_t saved_disp[4],
                                         uintptr_t *out_original_target) {
  uintptr_t call_opcode_addr = 0;
  int32_t saved_rel32 = 0;
  int32_t rel32 = 0;
  uint8_t opcode = 0;
  DWORD old_protect = 0;
  DWORD old_protect_restore = 0;

  if (call_disp_addr < 1u || hook_target == NULL || saved_disp == NULL || out_original_target == NULL) {
    return 0;
  }

  call_opcode_addr = call_disp_addr - 1u;
  opcode = read_game_u8(call_opcode_addr);
  if (opcode != 0xE8u) {
    runtime_tracef("script_hook: call opcode mismatch at 0x%08lx got=0x%02x", (unsigned long)call_opcode_addr,
                   (unsigned)opcode);
    return 0;
  }

  memcpy(saved_disp, (const void *)call_disp_addr, 4u);
  memcpy(&saved_rel32, (const void *)call_disp_addr, sizeof(saved_rel32));
  *out_original_target = (uintptr_t)((intptr_t)(call_opcode_addr + 5u) + (intptr_t)saved_rel32);

  rel32 = (int32_t)((intptr_t)(uintptr_t)hook_target - (intptr_t)(call_opcode_addr + 5u));
  if (!VirtualProtect((LPVOID)call_disp_addr, 4u, PAGE_EXECUTE_READWRITE, &old_protect)) {
    runtime_tracef("script_hook: call VirtualProtect failed at 0x%08lx", (unsigned long)call_disp_addr);
    return 0;
  }

  memcpy((void *)call_disp_addr, &rel32, sizeof(rel32));
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)call_opcode_addr, 5u);
  (void)VirtualProtect((LPVOID)call_disp_addr, 4u, old_protect, &old_protect_restore);
  return 1;
}

static int restore_call_disp_hook_compat(uintptr_t call_disp_addr, const uint8_t saved_disp[4]) {
  uintptr_t call_opcode_addr = 0;
  DWORD old_protect = 0;
  DWORD old_protect_restore = 0;

  if (call_disp_addr < 1u || saved_disp == NULL) {
    return 0;
  }

  call_opcode_addr = call_disp_addr - 1u;
  if (!VirtualProtect((LPVOID)call_disp_addr, 4u, PAGE_EXECUTE_READWRITE, &old_protect)) {
    runtime_tracef("script_hook: restore VirtualProtect failed at 0x%08lx", (unsigned long)call_disp_addr);
    return 0;
  }
  memcpy((void *)call_disp_addr, saved_disp, 4u);
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)call_opcode_addr, 5u);
  (void)VirtualProtect((LPVOID)call_disp_addr, 4u, old_protect, &old_protect_restore);
  return 1;
}

static void *create_game_process_hook_stub(void) {
  uint8_t *stub = NULL;
  uint8_t *cursor = NULL;

  if (g_runtime.game_process_hook_stub != 0u) {
    return (void *)(uintptr_t)g_runtime.game_process_hook_stub;
  }

  stub = (uint8_t *)VirtualAlloc(NULL, 7u, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (stub == NULL) {
    runtime_tracef("game_process_hook: stub alloc failed");
    return NULL;
  }

  cursor = stub;
  *cursor++ = 0x81u; /* add esp, 0x190 */
  *cursor++ = 0xC4u;
  *cursor++ = 0x90u;
  *cursor++ = 0x01u;
  *cursor++ = 0x00u;
  *cursor++ = 0x00u;
  *cursor++ = 0xC3u; /* ret */

  FlushInstructionCache(GetCurrentProcess(), stub, (SIZE_T)(cursor - stub));
  g_runtime.game_process_hook_stub = (uintptr_t)stub;
  runtime_tracef("game_process_hook: stub generated addr=0x%08lx", (unsigned long)(uintptr_t)stub);
  return stub;
}

static void free_game_process_hook_stub(void) {
  if (g_runtime.game_process_hook_stub == 0u) {
    return;
  }

  if (!VirtualFree((LPVOID)g_runtime.game_process_hook_stub, 0u, MEM_RELEASE)) {
    runtime_tracef("game_process_hook: stub free failed addr=0x%08lx", (unsigned long)g_runtime.game_process_hook_stub);
    return;
  }

  runtime_tracef("game_process_hook: stub freed addr=0x%08lx", (unsigned long)g_runtime.game_process_hook_stub);
  g_runtime.game_process_hook_stub = 0u;
}

static void install_game_process_hook_compat(void) {
  uint32_t hook_addr = 0;

  if (InterlockedCompareExchange(&g_runtime.game_process_hook_installed, 0, 0) != 0) {
    return;
  }

  if (create_game_process_hook_stub() == NULL) {
    return;
  }

  memcpy(g_runtime.game_process_hook_saved_storage, (const void *)SAMP_ADDR_GAME_PROCESS_HOOK_STORAGE,
         sizeof(g_runtime.game_process_hook_saved_storage));
  memcpy(g_runtime.game_process_hook_saved_code, (const void *)SAMP_ADDR_GAME_PROCESS_HOOK_INSTALL,
         sizeof(g_runtime.game_process_hook_saved_code));

  hook_addr = (uint32_t)g_runtime.game_process_hook_stub;
  if (!patch_copy(SAMP_ADDR_GAME_PROCESS_HOOK_STORAGE, &hook_addr, sizeof(hook_addr)) ||
      !patch_copy(SAMP_ADDR_GAME_PROCESS_HOOK_INSTALL, kGameProcessHookJmpCode, sizeof(kGameProcessHookJmpCode))) {
    (void)patch_copy(SAMP_ADDR_GAME_PROCESS_HOOK_STORAGE, g_runtime.game_process_hook_saved_storage,
                     sizeof(g_runtime.game_process_hook_saved_storage));
    (void)patch_copy(SAMP_ADDR_GAME_PROCESS_HOOK_INSTALL, g_runtime.game_process_hook_saved_code,
                     sizeof(g_runtime.game_process_hook_saved_code));
    return;
  }

  InterlockedExchange(&g_runtime.game_process_hook_installed, 1);
  runtime_tracef("game_process_hook: installed install=0x%08lx storage=0x%08lx stub=0x%08lx",
                 (unsigned long)SAMP_ADDR_GAME_PROCESS_HOOK_INSTALL, (unsigned long)SAMP_ADDR_GAME_PROCESS_HOOK_STORAGE,
                 (unsigned long)g_runtime.game_process_hook_stub);
}

static void uninstall_game_process_hook_compat(void) {
  if (InterlockedCompareExchange(&g_runtime.game_process_hook_installed, 0, 0) != 0) {
    (void)patch_copy(SAMP_ADDR_GAME_PROCESS_HOOK_INSTALL, g_runtime.game_process_hook_saved_code,
                     sizeof(g_runtime.game_process_hook_saved_code));
    (void)patch_copy(SAMP_ADDR_GAME_PROCESS_HOOK_STORAGE, g_runtime.game_process_hook_saved_storage,
                     sizeof(g_runtime.game_process_hook_saved_storage));
    InterlockedExchange(&g_runtime.game_process_hook_installed, 0);
    runtime_tracef("game_process_hook: uninstalled");
  }

  free_game_process_hook_stub();
}

static int ensure_script_gate_writable(void) {
  DWORD old_protect = 0;

  if (g_runtime.script_gate_page_unprotected) {
    return 1;
  }

  if (!VirtualProtect((LPVOID)SAMP_ADDR_SCRIPT_PROCESS_GATE, 2u, PAGE_EXECUTE_READWRITE, &old_protect)) {
    runtime_tracef("script_hook: gate VirtualProtect failed at 0x%08lx", (unsigned long)SAMP_ADDR_SCRIPT_PROCESS_GATE);
    return 0;
  }

  g_runtime.script_gate_old_protect = old_protect;
  g_runtime.script_gate_page_unprotected = 1;
  return 1;
}

static void restore_script_gate_writable(void) {
  DWORD old_protect_restore = 0;

  if (!g_runtime.script_gate_page_unprotected) {
    return;
  }

  (void)VirtualProtect((LPVOID)SAMP_ADDR_SCRIPT_PROCESS_GATE, 2u, g_runtime.script_gate_old_protect, &old_protect_restore);
  g_runtime.script_gate_page_unprotected = 0;
  g_runtime.script_gate_old_protect = 0;
}

static void __cdecl scripts_process_hook_compat(void) {
  if (g_runtime.settings.play_online && !run_game_scripts_enabled()) {
    write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0x8Bu);
    write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD0u);
    if (InterlockedCompareExchange(&g_runtime.script_process_suppressed_logged, 1, 0) == 0) {
      runtime_tracef("script_hook: suppressed CTheScripts::Process in online mode");
    }
    return;
  }

  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0xFFu);
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD2u);

  if (g_script_process_original != NULL) {
    g_script_process_original();
  }

  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0x8Bu);
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD0u);
}

static void install_scripts_process_hook_compat(void) {
  if (InterlockedCompareExchange(&g_runtime.script_hook_installed, 0, 0) != 0) {
    return;
  }
  if (!ensure_script_gate_writable()) {
    return;
  }
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0x8Bu);
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD0u);

  if (!install_call_disp_hook_compat(SAMP_ADDR_SCRIPT_PROCESS_CALL_DISP, (void *)scripts_process_hook_compat,
                                     g_runtime.script_hook_saved_disp, &g_runtime.script_hook_original_target)) {
    return;
  }

  g_script_process_original = (samp_script_process_fn)g_runtime.script_hook_original_target;
  InterlockedExchange(&g_runtime.script_hook_installed, 1);
  runtime_tracef("script_hook: installed call_disp=0x%08lx original=0x%08lx",
                 (unsigned long)SAMP_ADDR_SCRIPT_PROCESS_CALL_DISP, (unsigned long)g_runtime.script_hook_original_target);
}

static void uninstall_scripts_process_hook_compat(void) {
  if (InterlockedCompareExchange(&g_runtime.script_hook_installed, 0, 0) != 0) {
    (void)restore_call_disp_hook_compat(SAMP_ADDR_SCRIPT_PROCESS_CALL_DISP, g_runtime.script_hook_saved_disp);
    InterlockedExchange(&g_runtime.script_hook_installed, 0);
    g_script_process_original = NULL;
    write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0x8Bu);
    write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD0u);
    runtime_tracef("script_hook: uninstalled");
  }
  restore_script_gate_writable();
}

static void apply_pregame_compat_patches_once(void) {
  uint8_t usa_probe = read_game_u8(SAMP_ADDR_BYPASS_VIDS_USA10);
  uint8_t eu_probe = read_game_u8(SAMP_ADDR_BYPASS_VIDS_EU10);
  LONG entry_before = read_game_entry_gate_value();
  uint8_t loading_name[10];

  if (InterlockedCompareExchange(&g_runtime.pregame_patches_applied, 1, 0) != 0) {
    return;
  }

  memset(loading_name, 0, sizeof(loading_name));
  memcpy(loading_name, "title", 5u);
  if (patch_copy(SAMP_ADDR_LOADING_SCREEN_NAME_A, loading_name, sizeof(loading_name)) &&
      patch_copy(SAMP_ADDR_LOADING_SCREEN_NAME_B, loading_name, sizeof(loading_name))) {
    runtime_tracef("pregame_patch: loading_screen strings patched to 'title'");
  } else {
    runtime_tracef("pregame_patch: loading_screen string patch skipped/failed");
  }

  if (usa_probe == 0x89u) {
    (void)patch_nop(SAMP_ADDR_BYPASS_VIDS_USA10, 6u);
    write_game_u8(SAMP_ADDR_ENTRY, 5u);
    runtime_tracef("pregame_patch: USA10 bypass_videos patched entry %ld->%ld", (long)entry_before,
                   (long)read_game_entry_gate_value());
    return;
  }

  if (eu_probe == 0xC8u) {
    (void)patch_nop(SAMP_ADDR_BYPASS_VIDS_EU10, 6u);
    write_game_u8(SAMP_ADDR_ENTRY, 5u);
    runtime_tracef("pregame_patch: EU10 bypass_videos patched entry %ld->%ld", (long)entry_before,
                   (long)read_game_entry_gate_value());
    return;
  }

  runtime_tracef("pregame_patch: skipped probe_usa=0x%02x probe_eu=0x%02x entry=%ld", (unsigned)usa_probe,
                 (unsigned)eu_probe, (long)entry_before);
}

static void apply_ingame_compat_patches_once(void) {
  const uint8_t ret_opcode = 0xC3u;
  const uint8_t no_ped_patch[3] = {0x33u, 0xC0u, 0xC3u};
  unsigned long applied = 0;
  unsigned long total = 0;

  if (InterlockedCompareExchange(&g_runtime.ingame_patches_applied, 1, 0) != 0) {
    return;
  }

#define SAMP_APPLY_PATCH(EXPR)                                                                                        \
  do {                                                                                                                \
    ++total;                                                                                                          \
    if ((EXPR)) {                                                                                                     \
      ++applied;                                                                                                      \
    }                                                                                                                 \
  } while (0)

  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_PROC_GEOMETRY_PATCH, 5u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_REPLAY_PROCESS_PATCH, 5u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_INTERIOR_PEDS_PATCH, 8u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_PED_SHADOWS_PATCH, 10u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_ANTI_PAUSE_PATCH, 7u));
  SAMP_APPLY_PATCH(patch_copy(SAMP_ADDR_RANDOM_WEAPON_PICKUPS, &ret_opcode, sizeof(ret_opcode)));
  SAMP_APPLY_PATCH(patch_copy(SAMP_ADDR_RESPAWN_INTERIOR_RET, &ret_opcode, sizeof(ret_opcode)));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_RESPAWN_INTERIOR_CALL, 5u));
  SAMP_APPLY_PATCH(patch_copy(SAMP_ADDR_MESSAGE_PRINT, &ret_opcode, sizeof(ret_opcode)));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_IPL_VEHICLE_PATCH, 5u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_CAR_GENERATOR_PATCH, 5u));
  SAMP_APPLY_PATCH(patch_copy(SAMP_ADDR_POPULATION_ADD_PED, no_ped_patch, sizeof(no_ped_patch)));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_RANDOM_CARS_PROCESS, 5u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_WASTED_MESSAGE_PATCH, 5u));

#undef SAMP_APPLY_PATCH

  runtime_tracef("ingame_patch: story suppression applied=%lu/%lu msg_print=0x%02x script_gate=0x%02x%02x",
                 applied, total, (unsigned)read_game_u8(SAMP_ADDR_MESSAGE_PRINT),
                 (unsigned)read_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE),
                 (unsigned)read_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2));
}

static void clamp_launch_startgame_flags(void) {
  uint8_t game_started_before = read_game_u8(SAMP_ADDR_GAME_STARTED);
  uint8_t startgame_before = read_game_u8(SAMP_ADDR_STARTGAME);
  uint8_t menu_before = read_game_u8(SAMP_ADDR_MENU);
  uint8_t menu2_before = read_game_u8(SAMP_ADDR_MENU2);
  uint8_t menu3_before = read_game_u8(SAMP_ADDR_MENU3);

  if (game_started_before == 0u && startgame_before == 0u && menu_before == 0u && menu2_before == 0u &&
      menu3_before == 0u) {
    return;
  }

  write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
  write_game_u8(SAMP_ADDR_STARTGAME, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);

  runtime_tracef("launch_clamp: game/start/menu %u/%u/%u/%u/%u -> 0/0/0/0/0", (unsigned)game_started_before,
                 (unsigned)startgame_before, (unsigned)menu_before, (unsigned)menu2_before, (unsigned)menu3_before);
}

static void apply_time_passing_patch_compat(void) {
  uint8_t before = 0;
  const uint8_t ret_opcode = 0xC3u;

  if (!g_runtime.settings.play_online) {
    return;
  }

  before = read_game_u8(SAMP_ADDR_TIME_PASSING_PATCH);
  if (before == ret_opcode && InterlockedCompareExchange(&g_runtime.time_passing_patch_applied, 0, 0) != 0) {
    return;
  }

  if (InterlockedCompareExchange(&g_runtime.time_passing_patch_applied, 1, 0) == 0) {
    g_runtime.time_passing_saved_byte = before;
  }

  if (patch_copy(SAMP_ADDR_TIME_PASSING_PATCH, &ret_opcode, sizeof(ret_opcode))) {
    if (before != ret_opcode) {
      runtime_tracef("do_init_stuff: time passing disabled addr=0x%08lx byte 0x%02x->0x%02x",
                     (unsigned long)SAMP_ADDR_TIME_PASSING_PATCH, (unsigned)before,
                     (unsigned)read_game_u8(SAMP_ADDR_TIME_PASSING_PATCH));
    }
    return;
  }

  runtime_tracef("do_init_stuff: time passing patch failed addr=0x%08lx byte=0x%02x",
                 (unsigned long)SAMP_ADDR_TIME_PASSING_PATCH, (unsigned)before);
}

static void restore_time_passing_patch_compat(void) {
  uint8_t current = 0;

  if (InterlockedCompareExchange(&g_runtime.time_passing_patch_applied, 0, 0) == 0) {
    return;
  }

  current = read_game_u8(SAMP_ADDR_TIME_PASSING_PATCH);
  if (current == 0xC3u && g_runtime.time_passing_saved_byte != 0u) {
    (void)patch_copy(SAMP_ADDR_TIME_PASSING_PATCH, &g_runtime.time_passing_saved_byte,
                     sizeof(g_runtime.time_passing_saved_byte));
    runtime_tracef("time_passing_patch: restored addr=0x%08lx byte 0x%02x->0x%02x",
                   (unsigned long)SAMP_ADDR_TIME_PASSING_PATCH, (unsigned)current,
                   (unsigned)read_game_u8(SAMP_ADDR_TIME_PASSING_PATCH));
  }
  InterlockedExchange(&g_runtime.time_passing_patch_applied, 0);
}

static void apply_connect_wait_flags_compat(void) {
  LONG entry_before = read_game_entry_gate_value();
  uint8_t game_started_before = read_game_u8(SAMP_ADDR_GAME_STARTED);
  uint8_t menu_before = read_game_u8(SAMP_ADDR_MENU);
  uint8_t menu2_before = read_game_u8(SAMP_ADDR_MENU2);
  uint8_t menu3_before = read_game_u8(SAMP_ADDR_MENU3);
  uint8_t startgame_before = read_game_u8(SAMP_ADDR_STARTGAME);
  LONG net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  LONG entry_target = (g_runtime.entry_gate > 0) ? g_runtime.entry_gate : 7;

  if (entry_before == entry_target && game_started_before == 0u && menu_before == 0u && menu2_before == 0u &&
      menu3_before == 0u && startgame_before == 0u) {
    return;
  }

  *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = entry_target;
  write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);
  write_game_u8(SAMP_ADDR_STARTGAME, 0u);

  runtime_tracef(
      "startgame_flags: mode=%s state=%ld entry %ld->%ld game_started %u->%u menu %u/%u/%u -> %u/%u/%u startgame %u->%u",
      "CONNECT_WAIT", (long)net_state, (long)entry_before, (long)read_game_entry_gate_value(), (unsigned)game_started_before,
      (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED), (unsigned)menu_before, (unsigned)menu2_before, (unsigned)menu3_before,
      (unsigned)read_game_u8(SAMP_ADDR_MENU), (unsigned)read_game_u8(SAMP_ADDR_MENU2),
      (unsigned)read_game_u8(SAMP_ADDR_MENU3), (unsigned)startgame_before, (unsigned)read_game_u8(SAMP_ADDR_STARTGAME));
}

static void apply_startgame_flags_compat(void) {
  LONG entry_before = read_game_entry_gate_value();
  uint8_t game_started_before = read_game_u8(SAMP_ADDR_GAME_STARTED);
  uint8_t menu_before = read_game_u8(SAMP_ADDR_MENU);
  uint8_t menu2_before = read_game_u8(SAMP_ADDR_MENU2);
  uint8_t menu3_before = read_game_u8(SAMP_ADDR_MENU3);
  uint8_t startgame_before = read_game_u8(SAMP_ADDR_STARTGAME);
  LONG net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  LONG entry_target = 8;
  uint8_t game_started_target = 1u;

  *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = entry_target;
  write_game_u8(SAMP_ADDR_GAME_STARTED, game_started_target);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);
  write_game_u8(SAMP_ADDR_STARTGAME, 0u);

  runtime_tracef(
      "startgame_flags: mode=%s state=%ld entry %ld->%ld game_started %u->%u menu %u/%u/%u -> %u/%u/%u startgame %u->%u",
      "LEGACY_INGAME", (long)net_state, (long)entry_before,
      (long)read_game_entry_gate_value(), (unsigned)game_started_before, (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED),
      (unsigned)menu_before, (unsigned)menu2_before, (unsigned)menu3_before, (unsigned)read_game_u8(SAMP_ADDR_MENU),
      (unsigned)read_game_u8(SAMP_ADDR_MENU2), (unsigned)read_game_u8(SAMP_ADDR_MENU3), (unsigned)startgame_before,
      (unsigned)read_game_u8(SAMP_ADDR_STARTGAME));
}

static void apply_session_frontend_hold_flags_compat(const char *reason) {
  LONG entry_before = read_game_entry_gate_value();
  uint8_t game_started_before = read_game_u8(SAMP_ADDR_GAME_STARTED);
  uint8_t menu_before = read_game_u8(SAMP_ADDR_MENU);
  uint8_t menu2_before = read_game_u8(SAMP_ADDR_MENU2);
  uint8_t menu3_before = read_game_u8(SAMP_ADDR_MENU3);
  uint8_t startgame_before = read_game_u8(SAMP_ADDR_STARTGAME);
  uint8_t hud_before = read_game_u8(SAMP_ADDR_ENABLE_HUD);
  uint8_t radar_before = read_game_u8(SAMP_ADDR_RADAR_BLANK);
  LONG net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  LONG logged = 0;
  int changed = 0;

  *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 9;
  write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
  write_game_u8(SAMP_ADDR_STARTGAME, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);
  write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
  write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);

  changed = entry_before != 9 || game_started_before != 0u || startgame_before != 0u || menu_before != 0u ||
            menu2_before != 0u || menu3_before != 0u || hud_before != 0u || radar_before != 1u;
  logged = InterlockedCompareExchange(&g_runtime.mp_session_frontend_hold_logged, 1, 0);
  if (changed || logged == 0) {
    runtime_tracef(
        "session_frontend_hold: reason=%s state=%ld entry %ld->%ld game_started %u->%u menu %u/%u/%u/%u -> "
        "%u/%u/%u/%u hud %u->%u radar_blank %u->%u",
        reason != NULL ? reason : "unknown", (long)net_state, (long)entry_before,
        (long)read_game_entry_gate_value(), (unsigned)game_started_before,
        (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED), (unsigned)startgame_before, (unsigned)menu_before,
        (unsigned)menu2_before, (unsigned)menu3_before, (unsigned)read_game_u8(SAMP_ADDR_STARTGAME),
        (unsigned)read_game_u8(SAMP_ADDR_MENU), (unsigned)read_game_u8(SAMP_ADDR_MENU2),
        (unsigned)read_game_u8(SAMP_ADDR_MENU3), (unsigned)hud_before,
        (unsigned)read_game_u8(SAMP_ADDR_ENABLE_HUD), (unsigned)radar_before,
        (unsigned)read_game_u8(SAMP_ADDR_RADAR_BLANK));
  }
}

static void maintain_connect_wait_state(void) {
  LONG net_state = 0;

  if (!g_runtime.settings.play_online) {
    return;
  }
  if (InterlockedCompareExchange(&g_runtime.start_game_called, 0, 0) == 0) {
    return;
  }

  net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  if (net_state == SAMP_NETGAME_CONNECTED || net_state == SAMP_NETGAME_FAILED) {
    return;
  }
  if (InterlockedCompareExchange(&g_runtime.preconnect_apply_count, 0, 0) > 0) {
    return;
  }

  apply_connect_wait_flags_compat();
}

static unsigned int count_required_vehicle_models_compat(const uint8_t *models, unsigned int *out_total,
                                                         unsigned int *out_top_model, unsigned int *out_top_count) {
  unsigned int nonzero = 0u;
  unsigned int total = 0u;
  unsigned int top_model = 400u;
  unsigned int top_count = 0u;
  unsigned int i = 0u;

  if (models == NULL) {
    return 0u;
  }

  for (i = 0u; i < SAMP_RAKNET_REQUIRED_VEHICLE_MODELS; ++i) {
    unsigned int count = (unsigned int)models[i];
    if (count != 0u) {
      ++nonzero;
      total += count;
    }
    if (count > top_count) {
      top_count = count;
      top_model = 400u + i;
    }
  }

  if (out_total != NULL) {
    *out_total = total;
  }
  if (out_top_model != NULL) {
    *out_top_model = top_model;
  }
  if (out_top_count != NULL) {
    *out_top_count = top_count;
  }
  return nonzero;
}

static void apply_raknet_init_game_settings_compat(const samp_raknet_rpc_probe_snapshot *snapshot) {
  DWORD weather_value = 0u;
  DWORD stunt_value = 0u;
  int gravity_applied = 0;
  int stunt_applied = 0;
  unsigned int vehicle_total = 0u;
  unsigned int top_vehicle_model = 400u;
  unsigned int top_vehicle_count = 0u;
  unsigned int vehicle_nonzero = 0u;

  if (snapshot == NULL || (snapshot->flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) == 0u) {
    return;
  }
  if (InterlockedCompareExchange(&g_runtime.raknet_init_game_applied, 1, 0) != 0) {
    return;
  }

  g_runtime.raknet_init_spawns_available = snapshot->init_spawns_available;
  g_runtime.raknet_init_local_player_id = snapshot->init_local_player_id;
  g_runtime.raknet_init_world_time = snapshot->init_world_time;
  g_runtime.raknet_init_weather = snapshot->init_weather;
  g_runtime.raknet_weather = snapshot->init_weather;
  g_runtime.raknet_init_gravity = snapshot->init_gravity;
  g_runtime.raknet_init_lan_mode = snapshot->init_lan_mode;
  g_runtime.raknet_init_death_drop_money = snapshot->init_death_drop_money;
  g_runtime.raknet_init_disable_enter_exits = snapshot->init_disable_enter_exits;
  g_runtime.raknet_init_stunt_bonus = snapshot->init_stunt_bonus;
  g_runtime.raknet_init_name_tag_draw_distance = snapshot->init_name_tag_draw_distance;
  g_runtime.raknet_init_global_chat_radius = snapshot->init_global_chat_radius;
  memcpy(g_runtime.raknet_init_send_rates, snapshot->init_send_rates, sizeof(g_runtime.raknet_init_send_rates));
  memcpy(g_runtime.raknet_init_vehicle_models, snapshot->init_vehicle_models,
         sizeof(g_runtime.raknet_init_vehicle_models));
  strncpy(g_runtime.raknet_init_hostname, snapshot->init_hostname, sizeof(g_runtime.raknet_init_hostname) - 1u);
  g_runtime.raknet_init_hostname[sizeof(g_runtime.raknet_init_hostname) - 1u] = '\0';

  write_game_u8(SAMP_ADDR_WORLD_HOUR, snapshot->init_world_time);
  write_game_u8(SAMP_ADDR_WORLD_MINUTE, 0u);
  weather_value = (DWORD)snapshot->init_weather;
  *(volatile DWORD *)(uintptr_t)SAMP_ADDR_WEATHER_A = weather_value;
  *(volatile DWORD *)(uintptr_t)SAMP_ADDR_WEATHER_B = weather_value;

  if (snapshot->init_gravity > 0.0f && snapshot->init_gravity < 1.0f) {
    gravity_applied = patch_copy(SAMP_ADDR_GRAVITY, &snapshot->init_gravity, sizeof(snapshot->init_gravity));
  }

  stunt_value = (DWORD)(snapshot->init_stunt_bonus ? 1u : 0u);
  stunt_applied = patch_copy(SAMP_ADDR_STUNT_BONUS, &stunt_value, sizeof(stunt_value));
  if (snapshot->init_disable_enter_exits != 0u) {
    write_game_u8(SAMP_ADDR_ENTER_EXITS, 0u);
  }

  vehicle_nonzero =
      count_required_vehicle_models_compat(snapshot->init_vehicle_models, &vehicle_total, &top_vehicle_model,
                                           &top_vehicle_count);

  runtime_tracef("init_game_apply: spawns=%u local_player=%u host='%.64s' time=%u weather=%u gravity=%.6f "
                 "gravity_applied=%d lan=%u death_drop=%ld stunt=%u stunt_applied=%d enter_exits_disabled=%u "
                 "nametag_dist=%.3f chat_radius=%.3f send_rates=%ld/%ld/%ld/%ld/%ld/%ld "
                 "vehicle_models_nonzero=%u vehicle_total=%u top_vehicle=%u:%u",
                 (unsigned)snapshot->init_spawns_available, (unsigned)snapshot->init_local_player_id,
                 snapshot->init_hostname, (unsigned)snapshot->init_world_time, (unsigned)snapshot->init_weather,
                 (double)snapshot->init_gravity, gravity_applied, (unsigned)snapshot->init_lan_mode,
                 (long)snapshot->init_death_drop_money, (unsigned)snapshot->init_stunt_bonus, stunt_applied,
                 (unsigned)snapshot->init_disable_enter_exits, (double)snapshot->init_name_tag_draw_distance,
                 (double)snapshot->init_global_chat_radius, (long)snapshot->init_send_rates[0],
                 (long)snapshot->init_send_rates[1], (long)snapshot->init_send_rates[2],
                 (long)snapshot->init_send_rates[3], (long)snapshot->init_send_rates[4],
                 (long)snapshot->init_send_rates[5], vehicle_nonzero, vehicle_total, top_vehicle_model,
                 top_vehicle_count);
}

static uint32_t refresh_raknet_rpc_snapshot_compat(void) {
  samp_raknet_rpc_probe_snapshot snapshot;
  LONG previous_flags = 0;
  LONG previous_game_rpc_flags = 0;
  LONG previous_class = 0;
  LONG previous_spawn = 0;
  LONG previous_dialog = 0;
  uint32_t previous_chat_seq = 0u;
  uint32_t game_rpc_flags = 0;

  if (!InterlockedCompareExchange(&g_runtime.net_mgr_inited, 0, 0) || !samp_net_mgr_uses_raknet(&g_runtime.net_mgr) ||
      g_runtime.net_mgr.raknet_client == NULL) {
    return (uint32_t)InterlockedCompareExchange(&g_runtime.raknet_rpc_flags, 0, 0);
  }

  memset(&snapshot, 0, sizeof(snapshot));
  if (samp_raknet_client_get_rpc_probe_snapshot(g_runtime.net_mgr.raknet_client, &snapshot) != 0) {
    return (uint32_t)InterlockedCompareExchange(&g_runtime.raknet_rpc_flags, 0, 0);
  }

  previous_flags = InterlockedExchange(&g_runtime.raknet_rpc_flags, (LONG)snapshot.flags);
  previous_class = InterlockedExchange(&g_runtime.raknet_request_class_outcome, (LONG)snapshot.request_class_outcome);
  previous_spawn = InterlockedExchange(&g_runtime.raknet_request_spawn_outcome, (LONG)snapshot.request_spawn_outcome);
  previous_dialog = InterlockedExchange(&g_runtime.raknet_last_dialog_id, (LONG)snapshot.last_dialog_id);
  game_rpc_flags = snapshot.flags & SAMP_RAKNET_RPC_FLAG_GAME_STATE_MASK;
  previous_game_rpc_flags = InterlockedExchange(&g_runtime.raknet_game_rpc_flags, (LONG)game_rpc_flags);
  apply_raknet_init_game_settings_compat(&snapshot);
  previous_chat_seq = (uint32_t)InterlockedCompareExchange(&g_runtime.chat_client_message_seq, 0, 0);
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_CLIENT_MESSAGE) != 0u && snapshot.client_message_count > 0u) {
    uint32_t latest_seq = previous_chat_seq;
    uint32_t i = 0u;
    uint32_t message_count = snapshot.client_message_count;
    if (message_count > SAMP_RAKNET_CLIENT_MESSAGE_RING) {
      message_count = SAMP_RAKNET_CLIENT_MESSAGE_RING;
    }
    for (i = 0u; i < message_count; ++i) {
      const samp_raknet_client_message_probe *message = &snapshot.client_messages[i];
      if (message->seq != 0u && message->seq > previous_chat_seq && message->text[0] != '\0') {
        chat_compat_add_colored_line(chat_compat_samp_color_to_argb(message->color), message->text);
        latest_seq = message->seq;
      }
    }
    if (latest_seq != previous_chat_seq) {
      InterlockedExchange(&g_runtime.chat_client_message_seq, (LONG)latest_seq);
    }
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_PLAYER_POS) != 0u) {
    memcpy(g_runtime.raknet_player_pos, snapshot.player_pos, sizeof(g_runtime.raknet_player_pos));
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_PLAYER_FACING) != 0u) {
    g_runtime.raknet_player_facing_angle = snapshot.player_facing_angle;
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_WEATHER) != 0u) {
    g_runtime.raknet_weather = snapshot.weather;
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_INTERIOR) != 0u) {
    g_runtime.raknet_interior = snapshot.interior;
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_CAMERA_POS) != 0u) {
    memcpy(g_runtime.raknet_camera_pos, snapshot.camera_pos, sizeof(g_runtime.raknet_camera_pos));
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT) != 0u) {
    memcpy(g_runtime.raknet_camera_look_at, snapshot.camera_look_at, sizeof(g_runtime.raknet_camera_look_at));
    g_runtime.raknet_camera_look_at_type = snapshot.camera_look_at_type;
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) != 0u) {
    g_runtime.raknet_spawn_team = snapshot.spawn_team;
    g_runtime.raknet_spawn_skin = snapshot.spawn_skin;
    memcpy(g_runtime.raknet_spawn_pos, snapshot.spawn_pos, sizeof(g_runtime.raknet_spawn_pos));
    g_runtime.raknet_spawn_rotation = snapshot.spawn_rotation;
    memcpy(g_runtime.raknet_spawn_weapons, snapshot.spawn_weapons, sizeof(g_runtime.raknet_spawn_weapons));
    memcpy(g_runtime.raknet_spawn_weapon_ammo, snapshot.spawn_weapon_ammo, sizeof(g_runtime.raknet_spawn_weapon_ammo));
  }

  if ((uint32_t)previous_flags != snapshot.flags || previous_class != (LONG)snapshot.request_class_outcome ||
      previous_spawn != (LONG)snapshot.request_spawn_outcome || previous_dialog != (LONG)snapshot.last_dialog_id) {
    runtime_tracef(
        "network_prepare: rpc flags=0x%08lx init=%u dialog=%u dialog_sent=%u class_sent=%u class_reply=%u "
        "class_outcome=%u spawn_info=%u spawn_sent=%u spawn_reply=%u spawn_outcome=%u dialog_id=%u",
        (unsigned long)snapshot.flags, (snapshot.flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) ? 1u : 0u,
        (snapshot.flags & SAMP_RAKNET_RPC_FLAG_DIALOG) ? 1u : 0u,
        (snapshot.flags & SAMP_RAKNET_RPC_FLAG_DIALOG_RESPONSE_SENT) ? 1u : 0u,
        (snapshot.flags & SAMP_RAKNET_RPC_FLAG_REQUEST_CLASS_SENT) ? 1u : 0u,
        (snapshot.flags & SAMP_RAKNET_RPC_FLAG_REQUEST_CLASS_REPLY) ? 1u : 0u,
        (unsigned)snapshot.request_class_outcome, (snapshot.flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) ? 1u : 0u,
        (snapshot.flags & SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_SENT) ? 1u : 0u,
        (snapshot.flags & SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_REPLY) ? 1u : 0u,
        (unsigned)snapshot.request_spawn_outcome, (unsigned)snapshot.last_dialog_id);
  }

  if (((uint32_t)previous_flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) == 0u &&
      (snapshot.flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) != 0u) {
    runtime_tracef("network_prepare: spawn_info team=%u skin=%ld pos=(%.3f,%.3f,%.3f) rot=%.3f",
                   (unsigned)snapshot.spawn_team, (long)snapshot.spawn_skin, (double)snapshot.spawn_pos[0],
                   (double)snapshot.spawn_pos[1], (double)snapshot.spawn_pos[2],
                   (double)snapshot.spawn_rotation);
  }

  if ((uint32_t)previous_game_rpc_flags != game_rpc_flags) {
    runtime_tracef(
        "network_prepare: game_rpc flags=0x%08lx player_pos=%u facing=%u weather=%u interior=%u camera_pos=%u "
        "camera_look=%u pos=(%.3f,%.3f,%.3f) angle=%.3f weather_id=%u interior_id=%u cam=(%.3f,%.3f,%.3f) "
        "look=(%.3f,%.3f,%.3f) look_type=%u observe_only=1",
        (unsigned long)game_rpc_flags, (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_POS) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_FACING) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_WEATHER) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_INTERIOR) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_CAMERA_POS) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT) ? 1u : 0u, (double)snapshot.player_pos[0],
        (double)snapshot.player_pos[1], (double)snapshot.player_pos[2], (double)snapshot.player_facing_angle,
        (unsigned)snapshot.weather, (unsigned)snapshot.interior, (double)snapshot.camera_pos[0],
        (double)snapshot.camera_pos[1], (double)snapshot.camera_pos[2], (double)snapshot.camera_look_at[0],
        (double)snapshot.camera_look_at[1], (double)snapshot.camera_look_at[2],
        (unsigned)snapshot.camera_look_at_type);
  }

  return snapshot.flags;
}

static void maintain_online_session_state(void) {
  LONG net_state = 0;
  LONG rpc_flags = 0;
  LONG spawn_outcome = 0;
  LONG entry_before = 0;
  uint8_t game_started_before = 0;
  uint8_t startgame_before = 0;
  uint8_t menu_before = 0;
  uint8_t menu2_before = 0;
  uint8_t menu3_before = 0;
  int spawn_ready = 0;

  if (!g_runtime.settings.play_online) {
    return;
  }
  if (InterlockedCompareExchange(&g_runtime.start_game_called, 0, 0) == 0) {
    return;
  }

  net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  if (net_state == SAMP_NETGAME_FAILED) {
    return;
  }

  rpc_flags = InterlockedCompareExchange(&g_runtime.raknet_rpc_flags, 0, 0);
  if (((uint32_t)rpc_flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) == 0u) {
    return;
  }

  spawn_outcome = InterlockedCompareExchange(&g_runtime.raknet_request_spawn_outcome, 0, 0);
  spawn_ready = (((uint32_t)rpc_flags & SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_REPLY) != 0u) &&
                (spawn_outcome == 1 || spawn_outcome == 2);
  if (!spawn_ready) {
    apply_session_frontend_hold_flags_compat("await_spawn");
    return;
  }

  entry_before = read_game_entry_gate_value();
  game_started_before = read_game_u8(SAMP_ADDR_GAME_STARTED);
  startgame_before = read_game_u8(SAMP_ADDR_STARTGAME);
  menu_before = read_game_u8(SAMP_ADDR_MENU);
  menu2_before = read_game_u8(SAMP_ADDR_MENU2);
  menu3_before = read_game_u8(SAMP_ADDR_MENU3);

  if (entry_before != 8 || game_started_before != 1u) {
    LONG drift_seen = InterlockedCompareExchange(&g_runtime.online_session_drift_seen, 0, 0);
    LONG last_entry = InterlockedCompareExchange(&g_runtime.online_session_last_entry, 0, 0);
    LONG last_started = InterlockedCompareExchange(&g_runtime.online_session_last_game_started, 0, 0);
    if (drift_seen == 0 || last_entry != entry_before || last_started != (LONG)game_started_before) {
      InterlockedExchange(&g_runtime.online_session_drift_seen, 1);
      InterlockedExchange(&g_runtime.online_session_last_entry, entry_before);
      InterlockedExchange(&g_runtime.online_session_last_game_started, (LONG)game_started_before);
      runtime_tracef(
          "online_session_drift: state=%ld rpc=0x%08lx observed entry=%ld game_started=%u expected=8/1 "
          "action=observe_only",
          (long)net_state, (unsigned long)rpc_flags, (long)entry_before, (unsigned)game_started_before);
    }
  }

  if (startgame_before == 0u && menu_before == 0u && menu2_before == 0u && menu3_before == 0u) {
    return;
  }

  write_game_u8(SAMP_ADDR_STARTGAME, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);

  runtime_tracef(
      "online_session_menu_clear: state=%ld rpc=0x%08lx entry=%ld game_started=%u start/menu %u/%u/%u/%u -> 0/0/0/0",
      (long)net_state, (unsigned long)rpc_flags, (long)entry_before, (unsigned)game_started_before,
      (unsigned)startgame_before, (unsigned)menu_before, (unsigned)menu2_before, (unsigned)menu3_before);
}

static int gta_code_ptr_compat(uintptr_t ptr) {
  return ptr >= 0x00400000u && ptr < 0x00800000u;
}

static int gta_vtable_ptr_compat(uintptr_t ptr) {
  return ptr >= 0x00800000u && ptr < 0x00900000u;
}

static uintptr_t gta_find_player_ped_compat(void) {
  return *(volatile uintptr_t *)(uintptr_t)SAMP_ADDR_FIND_PLAYER_PED;
}

static int gta_script_push_i_compat(size_t *pos, int32_t value) {
  if (pos == NULL || *pos + 5u > SAMP_SCRIPT_BUF_BYTES) {
    return 0;
  }
  g_runtime.script_buf[*pos] = 0x01u;
  ++(*pos);
  memcpy(&g_runtime.script_buf[*pos], &value, sizeof(value));
  *pos += sizeof(value);
  return 1;
}

static int gta_script_push_f_compat(size_t *pos, float value) {
  if (pos == NULL || *pos + 5u > SAMP_SCRIPT_BUF_BYTES) {
    return 0;
  }
  g_runtime.script_buf[*pos] = 0x06u;
  ++(*pos);
  memcpy(&g_runtime.script_buf[*pos], &value, sizeof(value));
  *pos += sizeof(value);
  return 1;
}

static int gta_script_execute_compat(void) {
#if defined(__i386__) || defined(_M_IX86)
  uintptr_t thread = (uintptr_t)g_runtime.script_thread;
  uintptr_t script_buf = (uintptr_t)g_runtime.script_buf;
  uintptr_t process_one_command = (uintptr_t)SAMP_ADDR_PROCESS_ONE_COMMAND;

  if (!ensure_script_gate_writable()) {
    return 0;
  }

  memset(&g_runtime.script_thread[0x3C], 0, 18u * sizeof(uint32_t));
  memcpy(&g_runtime.script_thread[0x14], &script_buf, sizeof(script_buf));

  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0xFFu);
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD2u);
  __asm__ __volatile__("movl %[thread], %%ecx\n\t"
                       "call *%[process_one_command]\n\t"
                       :
                       : [thread] "r"(thread), [process_one_command] "r"(process_one_command)
                       : "eax", "ecx", "edx", "memory", "cc");
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0x8Bu);
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD0u);
  return 1;
#else
  return 0;
#endif
}

static int gta_script_command_compat(uint16_t opcode, const char *params, ...) {
  size_t pos = 2u;
  va_list args;
  const char *cursor = params;

  memset(g_runtime.script_buf, 0, sizeof(g_runtime.script_buf));
  memcpy(g_runtime.script_buf, &opcode, sizeof(opcode));

  va_start(args, params);
  while (cursor != NULL && *cursor != '\0') {
    switch (*cursor) {
      case 'i': {
        int32_t value = (int32_t)va_arg(args, int);
        if (!gta_script_push_i_compat(&pos, value)) {
          va_end(args);
          return 0;
        }
        break;
      }
      case 'f': {
        float value = (float)va_arg(args, double);
        if (!gta_script_push_f_compat(&pos, value)) {
          va_end(args);
          return 0;
        }
        break;
      }
      case 'z':
        if (pos + 1u > SAMP_SCRIPT_BUF_BYTES) {
          va_end(args);
          return 0;
        }
        g_runtime.script_buf[pos++] = 0x00u;
        break;
      default:
        va_end(args);
        return 0;
    }
    ++cursor;
  }
  va_end(args);

  return gta_script_execute_compat();
}

static void mp_bridge_record_script_failure(const char *name, uint16_t opcode) {
  LONG failures = InterlockedIncrement(&g_runtime.mp_session_script_failures);
  if (failures <= 10) {
    runtime_tracef("mp_session_bridge: script command failed name=%s opcode=0x%04x failure=%ld",
                   (name != NULL) ? name : "unknown", (unsigned)opcode, (long)failures);
  }
}

static int gta_ped_is_in_vehicle_compat(uintptr_t ped) {
  uint32_t state_flags = 0u;

  if (ped < 0x10000u || ped >= 0x80000000u) {
    return 0;
  }
  if (!memory_is_readable_compat((const void *)(ped + SAMP_PED_OFFSET_STATE_FLAGS), sizeof(state_flags))) {
    return 0;
  }

  memcpy(&state_flags, (const void *)(ped + SAMP_PED_OFFSET_STATE_FLAGS), sizeof(state_flags));
  return (state_flags & SAMP_PED_STATE_IN_VEHICLE) != 0u ? 1 : 0;
}

static int gta_entity_direct_position_compat(uintptr_t entity, float x, float y, float z) {
  uintptr_t matrix = 0;
  float zero = 0.0f;

  if (entity < 0x10000u || entity >= 0x80000000u) {
    return 0;
  }

  matrix = *(uintptr_t *)(entity + SAMP_PED_OFFSET_MATRIX);
  if (matrix < 0x10000u || matrix >= 0x80000000u) {
    return 0;
  }

  memcpy((void *)(matrix + SAMP_MATRIX_OFFSET_POS), &x, sizeof(x));
  memcpy((void *)(matrix + SAMP_MATRIX_OFFSET_POS + sizeof(float)), &y, sizeof(y));
  memcpy((void *)(matrix + SAMP_MATRIX_OFFSET_POS + (sizeof(float) * 2u)), &z, sizeof(z));
  memcpy((void *)(entity + SAMP_ENTITY_OFFSET_MOVE_SPEED), &zero, sizeof(zero));
  memcpy((void *)(entity + SAMP_ENTITY_OFFSET_MOVE_SPEED + sizeof(float)), &zero, sizeof(zero));
  memcpy((void *)(entity + SAMP_ENTITY_OFFSET_MOVE_SPEED + (sizeof(float) * 2u)), &zero, sizeof(zero));
  return 1;
}

static int gta_entity_teleport_compat(uintptr_t entity, float x, float y, float z) {
#if defined(__i386__) || defined(_M_IX86)
  uintptr_t vtable = 0;
  uintptr_t method = 0;
  uint16_t model_index = 0;

  if (entity < 0x10000u || entity >= 0x80000000u) {
    return 0;
  }

  memcpy(&model_index, (const void *)(entity + SAMP_ENTITY_OFFSET_MODEL_INDEX), sizeof(model_index));
  vtable = *(uintptr_t *)entity;
  if (model_index != 449u && model_index != 537u && model_index != 538u && gta_vtable_ptr_compat(vtable)) {
    method = *(uintptr_t *)(vtable + 56u);
    if (gta_code_ptr_compat(method)) {
      __asm__ __volatile__("pushl $0\n\t"
                           "pushl %[z]\n\t"
                           "pushl %[y]\n\t"
                           "pushl %[x]\n\t"
                           "movl %[entity], %%ecx\n\t"
                           "call *%[method]\n\t"
                           :
                           : [entity] "r"(entity), [method] "r"(method), [x] "m"(x), [y] "m"(y), [z] "m"(z)
                           : "eax", "ecx", "edx", "memory", "cc");
      return 1;
    }
  }
#endif
  return gta_entity_direct_position_compat(entity, x, y, z);
}

static int gta_preconnect_position_local_player_compat(uintptr_t ped) {
  if (preconnect_remove_from_vehicle_compat() && ped != 0u && gta_ped_is_in_vehicle_compat(ped)) {
    if (gta_script_command_compat(0x0362u, "ifff", SAMP_GTA_ACTOR_LOCAL_ID, SAMP_PRECONNECT_PLAYER_X,
                                  SAMP_PRECONNECT_PLAYER_Y, SAMP_PRECONNECT_PLAYER_Z)) {
      return 1;
    }
    mp_bridge_record_script_failure("preconnect_remove_actor_from_car_and_put_at", 0x0362u);
  }

  return gta_entity_teleport_compat(ped, SAMP_PRECONNECT_PLAYER_X, SAMP_PRECONNECT_PLAYER_Y,
                                    SAMP_PRECONNECT_PLAYER_Z);
}

static int preconnect_frontend_connect_allowed_compat(void) {
  DWORD delay_ms = 0;
  DWORD started = 0;
  DWORD now = 0;
  DWORD elapsed = 0;

  if (!g_runtime.settings.play_online) {
    return 1;
  }

  if (InterlockedCompareExchange(&g_runtime.preconnect_ready, 0, 0) == 0) {
    if (InterlockedCompareExchange(&g_runtime.preconnect_wait_logged, 1, 0) == 0) {
      runtime_tracef("preconnect_bridge: waiting for Santa Maria frontend before RakNet connect");
    }
    return 0;
  }

  delay_ms = preconnect_delay_ms_compat();
  if (delay_ms == 0u) {
    return 1;
  }

  started = g_runtime.preconnect_start_tick;
  if (started == 0u) {
    return 0;
  }

  now = GetTickCount();
  elapsed = now - started;
  if (elapsed < delay_ms) {
    return 0;
  }

  return 1;
}

static void apply_preconnect_frontend_compat(void) {
  LONG apply_count = 0;
  LONG net_state = 0;
  LONG rpc_flags = 0;
  uintptr_t ped = 0;
  DWORD now = 0;
  DWORD ped_seen_elapsed = 0;
  DWORD world_settle_ms = 0;
  int camera_ok = 1;
  int no_ped_fallback = 0;

  if (!g_runtime.settings.play_online) {
    return;
  }
  if (InterlockedCompareExchange(&g_runtime.start_game_called, 0, 0) == 0) {
    return;
  }

  net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  if (net_state == SAMP_NETGAME_FAILED) {
    return;
  }

  rpc_flags = InterlockedCompareExchange(&g_runtime.raknet_rpc_flags, 0, 0);
  if (((uint32_t)rpc_flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) != 0u) {
    return;
  }

  apply_count = InterlockedIncrement(&g_runtime.preconnect_apply_count);
  if (apply_count > SAMP_PRECONNECT_MAX_APPLIES) {
    return;
  }

  ped = gta_find_player_ped_compat();
  if (ped == 0u) {
    if (!preconnect_require_ped_compat() && apply_count >= SAMP_PRECONNECT_PED_FALLBACK_APPLIES &&
        read_game_entry_gate_value() >= 8) {
      no_ped_fallback = 1;
      if (InterlockedCompareExchange(&g_runtime.preconnect_no_ped_fallback_logged, 1, 0) == 0) {
        runtime_tracef("preconnect_bridge: continuing without local ped apply=%ld entry=%ld game_started=%u",
                       (long)apply_count, (long)read_game_entry_gate_value(),
                       (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED));
      }
    } else if (read_game_entry_gate_value() >= 8) {
      *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 8;
      write_game_u8(SAMP_ADDR_GAME_STARTED, 1u);
      write_game_u8(SAMP_ADDR_STARTGAME, 0u);
      write_game_u8(SAMP_ADDR_MENU, 0u);
      write_game_u8(SAMP_ADDR_MENU2, 0u);
      write_game_u8(SAMP_ADDR_MENU3, 0u);
      write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
      write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);
      if (apply_count <= 3 || (apply_count % 120) == 0) {
        runtime_tracef("preconnect_bridge: warming world without local ped apply=%ld entry=%ld game_started=%u",
                       (long)apply_count, (long)read_game_entry_gate_value(),
                       (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED));
      }
      return;
    } else if (apply_count <= 3 || (apply_count % 120) == 0) {
      runtime_tracef("preconnect_bridge: waiting for local ped apply=%ld entry=%ld game_started=%u",
                     (long)apply_count, (long)read_game_entry_gate_value(),
                     (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED));
      return;
    } else {
      return;
    }
  }

  world_settle_ms = preconnect_world_settle_ms_compat();
  if (ped != 0u && g_runtime.preconnect_ped_seen_tick == 0u) {
    g_runtime.preconnect_ped_seen_tick = GetTickCount();
    InterlockedExchange(&g_runtime.preconnect_no_ped_fallback_logged, 0);
    runtime_tracef("preconnect_bridge: local ped observed ped=0x%08lx apply=%ld entry=%ld game_started=%u settle_ms=%lu",
                   (unsigned long)ped, (long)apply_count, (long)read_game_entry_gate_value(),
                   (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED), (unsigned long)world_settle_ms);
  }

  if (world_settle_ms > 0u && ped != 0u && !no_ped_fallback && g_runtime.preconnect_ped_seen_tick != 0u) {
    now = GetTickCount();
    ped_seen_elapsed = now - g_runtime.preconnect_ped_seen_tick;
    if (ped_seen_elapsed < world_settle_ms) {
      *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 8;
      write_game_u8(SAMP_ADDR_GAME_STARTED, 1u);
      write_game_u8(SAMP_ADDR_STARTGAME, 0u);
      write_game_u8(SAMP_ADDR_MENU, 0u);
      write_game_u8(SAMP_ADDR_MENU2, 0u);
      write_game_u8(SAMP_ADDR_MENU3, 0u);
      write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
      write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);
      if (apply_count <= 3 || (apply_count % 120) == 0 ||
          InterlockedCompareExchange(&g_runtime.preconnect_ped_seen_logged, 1, 0) == 0) {
        runtime_tracef("preconnect_bridge: settling world ped=0x%08lx elapsed_ms=%lu/%lu entry=%ld game_started=%u",
                       (unsigned long)ped, (unsigned long)ped_seen_elapsed,
                       (unsigned long)world_settle_ms, (long)read_game_entry_gate_value(),
                       (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED));
      }
      return;
    }
  }

  *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 9;
  write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
  write_game_u8(SAMP_ADDR_STARTGAME, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);
  write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
  write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);

  if (ped != 0u) {
    (void)gta_preconnect_position_local_player_compat(ped);
  }
  if (!gta_script_command_compat(0x04E4u, "ff", SAMP_PRECONNECT_PLAYER_X, SAMP_PRECONNECT_PLAYER_Y)) {
    mp_bridge_record_script_failure("preconnect_refresh_streaming_at", 0x04E4u);
  }
  if (!gta_script_command_compat(0x015Fu, "ffffff", SAMP_PRECONNECT_CAMERA_X, SAMP_PRECONNECT_CAMERA_Y,
                                 SAMP_PRECONNECT_CAMERA_Z, 0.0f, 0.0f, 0.0f)) {
    camera_ok = 0;
    mp_bridge_record_script_failure("preconnect_set_camera_position", 0x015Fu);
  }
  if (!gta_script_command_compat(0x0160u, "fffi", SAMP_PRECONNECT_LOOK_X, SAMP_PRECONNECT_LOOK_Y,
                                 SAMP_PRECONNECT_LOOK_Z, 2)) {
    camera_ok = 0;
    mp_bridge_record_script_failure("preconnect_point_camera", 0x0160u);
  }

  now = GetTickCount();
  if (g_runtime.preconnect_start_tick == 0u) {
    g_runtime.preconnect_start_tick = now;
  }
  if (camera_ok && InterlockedCompareExchange(&g_runtime.preconnect_ready, 1, 0) == 0) {
    runtime_tracef(
        "preconnect_bridge: ready ped=0x%08lx no_ped_fallback=%d entry=%ld game_started=%u player=(%.3f,%.3f,%.3f) "
        "cam=(%.3f,%.3f,%.3f) look=(%.3f,%.3f,%.3f) delay_ms=%lu",
        (unsigned long)ped, no_ped_fallback, (long)read_game_entry_gate_value(),
        (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED), (double)SAMP_PRECONNECT_PLAYER_X,
        (double)SAMP_PRECONNECT_PLAYER_Y, (double)SAMP_PRECONNECT_PLAYER_Z, (double)SAMP_PRECONNECT_CAMERA_X,
        (double)SAMP_PRECONNECT_CAMERA_Y, (double)SAMP_PRECONNECT_CAMERA_Z, (double)SAMP_PRECONNECT_LOOK_X,
        (double)SAMP_PRECONNECT_LOOK_Y, (double)SAMP_PRECONNECT_LOOK_Z, (unsigned long)preconnect_delay_ms_compat());
  }

  if (InterlockedCompareExchange(&g_runtime.preconnect_started_logged, 1, 0) == 0) {
    chat_compat_add_message("SA-MP 0.3.7-R5 Started");
  }
  chat_compat_draw_overlay();
  if (apply_count <= 3 || (apply_count % 120) == 0) {
    runtime_tracef("preconnect_bridge: apply #%ld state=%ld ped=0x%08lx elapsed_ms=%lu camera_ok=%d",
                   (long)apply_count, (long)net_state, (unsigned long)ped,
                   (unsigned long)(now - g_runtime.preconnect_start_tick), camera_ok);
  }
}

static void apply_multiplayer_session_bridge_compat(void) {
  LONG net_state = 0;
  LONG apply_count = 0;
  LONG rpc_flags_long = 0;
  uint32_t rpc_flags = 0u;
  uint32_t game_rpc_flags = 0u;
  uintptr_t ped = 0;
  uint8_t game_started = 0;
  LONG entry = 0;
  LONG class_outcome = 0;
  LONG spawn_outcome = 0;
  uint8_t current_player = 0u;
  int has_spawn_info = 0;
  int spawn_ready = 0;
  float target_pos[3] = {0.0f, 0.0f, 0.0f};
  float target_angle = 0.0f;
  float target_z = 0.0f;

  if (!g_runtime.settings.play_online) {
    return;
  }

  net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  if (net_state != SAMP_NETGAME_CONNECTED) {
    return;
  }

  rpc_flags_long = InterlockedCompareExchange(&g_runtime.raknet_rpc_flags, 0, 0);
  rpc_flags = (uint32_t)rpc_flags_long;
  if ((rpc_flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) == 0u) {
    return;
  }

  game_rpc_flags = (uint32_t)InterlockedCompareExchange(&g_runtime.raknet_game_rpc_flags, 0, 0);
  if (game_rpc_flags == 0u) {
    return;
  }

  apply_count = InterlockedIncrement(&g_runtime.mp_session_apply_count);
  if (apply_count > SAMP_MP_BRIDGE_MAX_APPLIES) {
    return;
  }

  ped = gta_find_player_ped_compat();
  entry = read_game_entry_gate_value();
  game_started = read_game_u8(SAMP_ADDR_GAME_STARTED);
  current_player = read_game_u8(SAMP_ADDR_CURRENT_PLAYER);
  class_outcome = InterlockedCompareExchange(&g_runtime.raknet_request_class_outcome, 0, 0);
  spawn_outcome = InterlockedCompareExchange(&g_runtime.raknet_request_spawn_outcome, 0, 0);
  has_spawn_info = (rpc_flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) != 0u;
  spawn_ready = ((rpc_flags & SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_REPLY) != 0u) &&
                (spawn_outcome == 1 || spawn_outcome == 2);
  if (!spawn_ready) {
    apply_session_frontend_hold_flags_compat("class_select");
  }

  if (spawn_ready) {
    write_game_u8(SAMP_ADDR_ENABLE_HUD, 1u);
    write_game_u8(SAMP_ADDR_RADAR_BLANK, 0u);
  } else {
    write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
    write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);
  }

  if (spawn_ready && has_spawn_info) {
    memcpy(target_pos, g_runtime.raknet_spawn_pos, sizeof(target_pos));
    target_angle = g_runtime.raknet_spawn_rotation;
    target_z = g_runtime.raknet_spawn_pos[2] + 1.0f;
  } else {
    memcpy(target_pos, g_runtime.raknet_player_pos, sizeof(target_pos));
    target_angle = g_runtime.raknet_player_facing_angle;
    target_z = g_runtime.raknet_player_pos[2];
  }

  if ((game_rpc_flags & SAMP_RAKNET_RPC_FLAG_WEATHER) != 0u) {
    *(volatile DWORD *)(uintptr_t)SAMP_ADDR_WEATHER_A = (DWORD)g_runtime.raknet_weather;
    *(volatile DWORD *)(uintptr_t)SAMP_ADDR_WEATHER_B = (DWORD)g_runtime.raknet_weather;
  }

  if (apply_count <= 6 || (apply_count % 60) == 0) {
    runtime_tracef(
        "mp_session_bridge: apply #%ld flags=0x%08lx ped=0x%08lx entry=%ld game_started=%u current_player=%u "
        "class_outcome=%ld spawn_outcome=%ld spawn_ready=%d spawn_info=%d pos=(%.3f,%.3f,%.3f) angle=%.3f "
        "cam=(%.3f,%.3f,%.3f) look=(%.3f,%.3f,%.3f)",
        (long)apply_count, (unsigned long)game_rpc_flags, (unsigned long)ped, (long)entry, (unsigned)game_started,
        (unsigned)current_player, (long)class_outcome, (long)spawn_outcome, spawn_ready, has_spawn_info,
        (double)target_pos[0], (double)target_pos[1], (double)target_z, (double)target_angle,
        (double)g_runtime.raknet_camera_pos[0], (double)g_runtime.raknet_camera_pos[1], (double)g_runtime.raknet_camera_pos[2],
        (double)g_runtime.raknet_camera_look_at[0], (double)g_runtime.raknet_camera_look_at[1],
        (double)g_runtime.raknet_camera_look_at[2]);
  }

  if (!spawn_ready && (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_CAMERA_POS) != 0u &&
      !gta_script_command_compat(0x015Fu, "ffffff", g_runtime.raknet_camera_pos[0], g_runtime.raknet_camera_pos[1],
                                 g_runtime.raknet_camera_pos[2], 0.0f, 0.0f, 0.0f)) {
    mp_bridge_record_script_failure("set_camera_position", 0x015Fu);
  }

  if (!spawn_ready && (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT) != 0u &&
      !gta_script_command_compat(0x0160u, "fffi", g_runtime.raknet_camera_look_at[0],
                                 g_runtime.raknet_camera_look_at[1], g_runtime.raknet_camera_look_at[2],
                                 (int)g_runtime.raknet_camera_look_at_type)) {
    mp_bridge_record_script_failure("point_camera", 0x0160u);
  }

  if ((game_rpc_flags & SAMP_RAKNET_RPC_FLAG_INTERIOR) != 0u) {
    if (!gta_script_command_compat(0x04BBu, "i", (int)g_runtime.raknet_interior)) {
      mp_bridge_record_script_failure("select_interior", 0x04BBu);
    }
    if (ped != 0u && !gta_script_command_compat(0x0860u, "ii", SAMP_GTA_ACTOR_LOCAL_ID, (int)g_runtime.raknet_interior)) {
      mp_bridge_record_script_failure("link_actor_to_interior", 0x0860u);
    }
    if ((game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_POS) != 0u &&
        !gta_script_command_compat(0x04E4u, "ff", target_pos[0], target_pos[1])) {
      mp_bridge_record_script_failure("refresh_streaming_at", 0x04E4u);
    }
  }

  if (ped != 0u && (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_FACING) != 0u) {
    float angle_rad = target_angle * SAMP_DEG_TO_RAD;
    memcpy((void *)(ped + SAMP_PED_OFFSET_ROTATION1), &angle_rad, sizeof(angle_rad));
    memcpy((void *)(ped + SAMP_PED_OFFSET_ROTATION2), &angle_rad, sizeof(angle_rad));
    if (!gta_script_command_compat(0x0173u, "if", SAMP_GTA_ACTOR_LOCAL_ID, target_angle)) {
      mp_bridge_record_script_failure("set_actor_z_angle", 0x0173u);
    }
  }

  if (ped != 0u && class_outcome == 1 && !spawn_ready && (rpc_flags & SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_SENT) == 0u) {
    float health = 100.0f;
    memcpy((void *)(ped + SAMP_PED_OFFSET_HEALTH), &health, sizeof(health));
    if (!gta_script_command_compat(0x01B4u, "ii", SAMP_GTA_PLAYER_LOCAL_ID, 0)) {
      mp_bridge_record_script_failure("toggle_player_controllable", 0x01B4u);
    }
  }

  if (ped != 0u && spawn_ready && InterlockedCompareExchange(&g_runtime.mp_session_spawn_finalized, 1, 0) == 0) {
    const uint8_t no_fade_patch[3] = {0xC2u, 0x08u, 0x00u};
    float health = 100.0f;

    *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 8;
    write_game_u8(SAMP_ADDR_GAME_STARTED, 1u);
    write_game_u8(SAMP_ADDR_STARTGAME, 0u);
    write_game_u8(SAMP_ADDR_MENU, 0u);
    write_game_u8(SAMP_ADDR_MENU2, 0u);
    write_game_u8(SAMP_ADDR_MENU3, 0u);
    write_game_u8(SAMP_ADDR_ENABLE_HUD, 1u);
    write_game_u8(SAMP_ADDR_RADAR_BLANK, 0u);
    memcpy((void *)(ped + SAMP_PED_OFFSET_HEALTH), &health, sizeof(health));

    if (!patch_copy(SAMP_ADDR_CAMERA_FADE_PATCH, no_fade_patch, sizeof(no_fade_patch))) {
      mp_bridge_record_script_failure("camera_fade_patch", (uint16_t)SAMP_ADDR_CAMERA_FADE_PATCH);
    }
    if (!gta_script_command_compat(0x015Au, "")) {
      mp_bridge_record_script_failure("restore_camera", 0x015Au);
    }
    if (!gta_script_command_compat(0x02EBu, "")) {
      mp_bridge_record_script_failure("restore_camera_jumpcut", 0x02EBu);
    }
    if (!gta_script_command_compat(0x0373u, "")) {
      mp_bridge_record_script_failure("set_camera_behind_player", 0x0373u);
    }
    if (!gta_script_command_compat(0x01B4u, "ii", SAMP_GTA_PLAYER_LOCAL_ID, 1)) {
      mp_bridge_record_script_failure("toggle_player_controllable", 0x01B4u);
    }
    if (has_spawn_info && g_runtime.raknet_spawn_skin >= 0) {
      if (!gta_script_command_compat(0x0247u, "i", (int)g_runtime.raknet_spawn_skin)) {
        mp_bridge_record_script_failure("request_model", 0x0247u);
      }
      if (!gta_script_command_compat(0x038Bu, "")) {
        mp_bridge_record_script_failure("load_requested_models", 0x038Bu);
      }
      if (!gta_script_command_compat(0x09C7u, "ii", SAMP_GTA_PLAYER_LOCAL_ID, (int)g_runtime.raknet_spawn_skin)) {
        mp_bridge_record_script_failure("set_player_skin", 0x09C7u);
      }
    }

    runtime_tracef("mp_session_bridge: spawn_finalize outcome=%ld info=%d team=%u skin=%ld pos=(%.3f,%.3f,%.3f) "
                   "rot=%.3f entry=%ld game_started=%u",
                   (long)spawn_outcome, has_spawn_info, (unsigned)g_runtime.raknet_spawn_team,
                   (long)g_runtime.raknet_spawn_skin, (double)target_pos[0], (double)target_pos[1],
                   (double)target_z, (double)target_angle, (long)read_game_entry_gate_value(),
                   (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED));
  }

  if (ped != 0u && (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_POS) != 0u &&
      (apply_count <= 8 || (apply_count % SAMP_MP_BRIDGE_TELEPORT_PERIOD) == 0)) {
    if (gta_entity_teleport_compat(ped, target_pos[0], target_pos[1], target_z)) {
      LONG teleports = InterlockedIncrement(&g_runtime.mp_session_teleport_count);
      if (teleports <= 8 || (teleports % 20) == 0) {
        runtime_tracef("mp_session_bridge: teleport #%ld ped=0x%08lx pos=(%.3f,%.3f,%.3f)", (long)teleports,
                       (unsigned long)ped, (double)target_pos[0], (double)target_pos[1], (double)target_z);
      }
    }
  }
}

static int phase_runtime_guard_attach(HINSTANCE instance) {
  if (InterlockedCompareExchange(&g_runtime.initialized, 0, 0)) {
    InterlockedIncrement(&g_runtime.process_attach_count);
    return 1;
  }

  memset(&g_runtime, 0, sizeof(g_runtime));
  g_runtime.instance = instance;
  g_runtime.process_attach_count = 1;
  g_runtime.entry_gate = resolve_initial_entry_gate();
  g_runtime.netgame_state = SAMP_NETGAME_WAIT_CONNECT;
  runtime_tracef("process_attach guard: entry_gate_target=%ld entry_mem=%ld", (long)g_runtime.entry_gate,
                 (long)read_game_entry_gate_value());
  runtime_tracef("build: %s", kSampDllBuildId);
  g_last_phase = BOOT_PHASE_1_RUNTIME_GUARD;
  return 1;
}

static int phase_install_exception_filter(void) {
  g_runtime.previous_filter = SetUnhandledExceptionFilter(samp_exception_filter);
  g_last_phase = BOOT_PHASE_2_EXCEPTION_FILTER;
  return 1;
}

static int phase_load_settings_and_paths(void) {
  DWORD path_len = 0;
  char *slash = NULL;
  int written = 0;
  DWORD attrs = INVALID_FILE_ATTRIBUTES;

  load_settings_from_command_line(&g_runtime.settings);

  path_len = GetModuleFileNameA(g_runtime.instance, g_runtime.module_path, (DWORD)sizeof(g_runtime.module_path));
  if (path_len == 0 || path_len >= sizeof(g_runtime.module_path)) {
    return 0;
  }

  g_runtime.module_path_len = path_len;
  memcpy(g_runtime.module_dir, g_runtime.module_path, path_len + 1);

  slash = strrchr(g_runtime.module_dir, '\\');
  if (slash == NULL) {
    return 0;
  }
  slash[1] = '\0';

  written = snprintf(g_runtime.archive_path, sizeof(g_runtime.archive_path), "%ssamp.saa", g_runtime.module_dir);
  if (written <= 0 || (size_t)written >= sizeof(g_runtime.archive_path)) {
    return 0;
  }

  attrs = GetFileAttributesA(g_runtime.archive_path);
  g_runtime.archive_present = (attrs != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
  runtime_tracef("settings: online=%d debug=%d host='%s' port='%s' nick='%s' archive_present=%d",
                 g_runtime.settings.play_online ? 1 : 0, g_runtime.settings.debug ? 1 : 0, g_runtime.settings.connect_host,
                 g_runtime.settings.connect_port, g_runtime.settings.nickname, g_runtime.archive_present);

  g_last_phase = BOOT_PHASE_3_SETTINGS_AND_PATHS;
  return 1;
}

static int phase_runtime_bootstrap(void) {
  FARPROC proc = NULL;
  HANDLE archive_handle = INVALID_HANDLE_VALUE;

  if (resolve_module_proc(&g_runtime.bootstrap_shims.kernel32_module, "kernel32.dll", "CreateFileA", &proc)) {
    g_runtime.bootstrap_shims.create_file_a = (samp_create_file_a_fn)proc;
  }
  if (resolve_module_proc(&g_runtime.bootstrap_shims.kernel32_module, "kernel32.dll", "ReadFile", &proc)) {
    g_runtime.bootstrap_shims.read_file = (samp_read_file_fn)proc;
  }
  if (resolve_module_proc(&g_runtime.bootstrap_shims.kernel32_module, "kernel32.dll", "GetFileSize", &proc)) {
    g_runtime.bootstrap_shims.get_file_size = (samp_get_file_size_fn)proc;
  }
  if (resolve_module_proc(&g_runtime.bootstrap_shims.kernel32_module, "kernel32.dll", "SetFilePointer", &proc)) {
    g_runtime.bootstrap_shims.set_file_pointer = (samp_set_file_pointer_fn)proc;
  }
  if (resolve_module_proc(&g_runtime.bootstrap_shims.kernel32_module, "kernel32.dll", "CloseHandle", &proc)) {
    g_runtime.bootstrap_shims.close_handle = (samp_close_handle_fn)proc;
  }
  if (resolve_module_proc(&g_runtime.bootstrap_shims.kernel32_module, "kernel32.dll", "GetFileType", &proc)) {
    g_runtime.bootstrap_shims.get_file_type = (samp_get_file_type_fn)proc;
  }
  if (resolve_module_proc(&g_runtime.bootstrap_shims.user32_module, "user32.dll", "ShowCursor", &proc)) {
    g_runtime.bootstrap_shims.show_cursor = (samp_show_cursor_fn)proc;
  }

  runtime_tracef(
      "bootstrap_shims: CreateFileA=%p ReadFile=%p GetFileSize=%p SetFilePointer=%p CloseHandle=%p GetFileType=%p "
      "ShowCursor=%p",
      g_runtime.bootstrap_shims.create_file_a, g_runtime.bootstrap_shims.read_file, g_runtime.bootstrap_shims.get_file_size,
      g_runtime.bootstrap_shims.set_file_pointer, g_runtime.bootstrap_shims.close_handle,
      g_runtime.bootstrap_shims.get_file_type, g_runtime.bootstrap_shims.show_cursor);

  if (build_module_relative_path("gtaweap3.ttf", g_runtime.gtaweap3_font_path, sizeof(g_runtime.gtaweap3_font_path))) {
    g_runtime.gtaweap3_font_added = AddFontResourceA(g_runtime.gtaweap3_font_path);
  }
  if (build_module_relative_path("sampaux3.ttf", g_runtime.sampaux3_font_path, sizeof(g_runtime.sampaux3_font_path))) {
    g_runtime.sampaux3_font_added = AddFontResourceA(g_runtime.sampaux3_font_path);
  }
  runtime_tracef("bootstrap_fonts: gtaweap3=%d sampaux3=%d", g_runtime.gtaweap3_font_added,
                 g_runtime.sampaux3_font_added);

  if (g_runtime.bootstrap_manager_block == NULL) {
    g_runtime.bootstrap_manager_block = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SAMP_BOOTSTRAP_MANAGER_BYTES);
  }
  runtime_tracef("bootstrap_manager: size=0x%x ptr=%p", (unsigned)SAMP_BOOTSTRAP_MANAGER_BYTES,
                 g_runtime.bootstrap_manager_block);

  if (g_runtime.archive_present && g_runtime.bootstrap_shims.create_file_a != NULL &&
      g_runtime.bootstrap_shims.close_handle != NULL) {
    archive_handle = g_runtime.bootstrap_shims.create_file_a(g_runtime.archive_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                                                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (archive_handle != INVALID_HANDLE_VALUE) {
      g_runtime.archive_probe_ok = 1;
      if (g_runtime.bootstrap_shims.get_file_size != NULL) {
        g_runtime.archive_probe_size = g_runtime.bootstrap_shims.get_file_size(archive_handle, NULL);
      }
      if (g_runtime.bootstrap_shims.get_file_type != NULL) {
        g_runtime.archive_probe_type = g_runtime.bootstrap_shims.get_file_type(archive_handle);
      }
      g_runtime.bootstrap_shims.close_handle(archive_handle);
    }
  }

  runtime_tracef("bootstrap_archive: present=%d probe_ok=%d size=%lu type=%lu path='%s'", g_runtime.archive_present,
                 g_runtime.archive_probe_ok, (unsigned long)g_runtime.archive_probe_size,
                 (unsigned long)g_runtime.archive_probe_type, g_runtime.archive_path);

  InterlockedExchange(&g_runtime.early_bootstrap_ready, 1);
  g_last_phase = BOOT_PHASE_4_EARLY_BOOTSTRAP;
  return 1;
}

static int phase_network_init(void) {
  if (samp_net_runtime_init() != 0) {
    return 0;
  }
  g_last_phase = BOOT_PHASE_5_NETWORK_INIT;
  return 1;
}

static void launch_graphics_loop_hook_callback(void);
static void launch_do_init_stuff_compat(void);

/* Legacy-structured pre-game stage: init globals and apply pre-game patch phase. */
static void launch_init_game_compat(void) {
  InterlockedExchange(&g_runtime.game_inited, 1);
  apply_pregame_compat_patches_once();
}

/* Legacy-structured in-game stage: patch/hook phase before graphics loop takes over. */
static void launch_start_game_compat(void) {
  int rc = 0;

  InterlockedExchange(&g_runtime.start_game_called, 1);
  apply_ingame_compat_patches_once();

  InterlockedExchange(&g_runtime.hooks_install_attempted, 1);
  install_game_process_hook_compat();
  install_scripts_process_hook_compat();
  rc = samp_hook_bridge_install_graphics_callback(&g_runtime.hook_bridge, launch_graphics_loop_hook_callback);
  if (InterlockedCompareExchange(&g_runtime.game_process_hook_installed, 0, 0) ||
      InterlockedCompareExchange(&g_runtime.script_hook_installed, 0, 0) ||
      (rc == 0 && (g_runtime.hook_bridge.install_succeeded || g_runtime.hook_bridge.installed ||
                   g_runtime.hook_bridge.secondary_installed))) {
    InterlockedExchange(&g_runtime.hooks_installed, 1);
  }
  apply_connect_wait_flags_compat();
  runtime_tracef(
      "start_game: hooks_attempted=1 rc=%d game_process=%ld script_installed=%ld installed=%d installed2=%d "
      "configured=%d configured2=%d enabled=%d disp=0x%08lx disp2=0x%08lx",
      rc, (long)InterlockedCompareExchange(&g_runtime.game_process_hook_installed, 0, 0),
      (long)InterlockedCompareExchange(&g_runtime.script_hook_installed, 0, 0), g_runtime.hook_bridge.installed,
      g_runtime.hook_bridge.secondary_installed, g_runtime.hook_bridge.configured, g_runtime.hook_bridge.secondary_configured,
      g_runtime.hook_bridge.enabled, (unsigned long)g_runtime.hook_bridge.graphics_call_disp_addr,
      (unsigned long)g_runtime.hook_bridge.graphics_call_disp_addr_secondary);

  /* Prime init/network once immediately to avoid waiting for first graphics callback. */
  launch_do_init_stuff_compat();
  runtime_tracef("start_game: primed do_init_stuff");
}

static void launch_prepare_network_compat(void) {
  const char *host_input = NULL;
  uint16_t default_port = SAMP_DEFAULT_CONNECT_PORT;
  LONG state = 0;
  DWORD now_ms = 0;

  if (!g_runtime.settings.play_online) {
    runtime_tracef("network_prepare: skipped (play_online=0)");
    return;
  }
  if (!InterlockedCompareExchange(&g_runtime.network_inited, 0, 0)) {
    host_input = (g_runtime.settings.connect_host[0] != '\0') ? g_runtime.settings.connect_host : "127.0.0.1";
    if (g_runtime.settings.connect_port[0] != '\0') {
      (void)parse_u16(g_runtime.settings.connect_port, &default_port);
    }

    memset(&g_runtime.endpoint, 0, sizeof(g_runtime.endpoint));
    g_runtime.endpoint_valid = (samp_net_parse_hostport(host_input, default_port, &g_runtime.endpoint) == 0);
    InterlockedExchange(&g_runtime.network_inited, 1);
    runtime_tracef("network_prepare: host_input='%s' resolved='%s:%u' endpoint_valid=%d", host_input, g_runtime.endpoint.host,
                   (unsigned)g_runtime.endpoint.port, g_runtime.endpoint_valid);

    if (!g_runtime.endpoint_valid) {
      InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_FAILED);
      runtime_tracef("network_prepare: endpoint invalid -> state=FAILED");
      return;
    }
  }
  if (!g_runtime.endpoint_valid) {
    return;
  }

  if (!InterlockedCompareExchange(&g_runtime.net_mgr_inited, 0, 0)) {
    if (samp_net_mgr_init(&g_runtime.net_mgr) == 0) {
      InterlockedExchange(&g_runtime.net_mgr_inited, 1);
      runtime_tracef("network_prepare: net_mgr_init OK");
      if (samp_net_mgr_uses_raknet(&g_runtime.net_mgr)) {
        configure_raknet_join_profile_once();
      }
    } else {
      runtime_tracef("network_prepare: net_mgr_init FAILED");
    }
  }

  if (!preconnect_frontend_connect_allowed_compat()) {
    return;
  }

  state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  now_ms = GetTickCount();

  if (state == SAMP_NETGAME_CONNECTING && g_runtime.net_mgr_last_connect_attempt_tick != 0 &&
      (DWORD)(now_ms - g_runtime.net_mgr_last_connect_attempt_tick) >= SAMP_RAKNET_CONNECT_STALE_MS) {
    InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_WAIT_CONNECT);
    state = SAMP_NETGAME_WAIT_CONNECT;
    runtime_tracef("network_prepare: connect timeout -> state=WAIT_CONNECT");
  }

  if (state == SAMP_NETGAME_WAIT_CONNECT &&
      (g_runtime.net_mgr_last_connect_attempt_tick == 0 ||
       (DWORD)(now_ms - g_runtime.net_mgr_last_connect_attempt_tick) >= SAMP_RAKNET_CONNECT_RETRY_MS)) {
    int connected = 0;
    int connect_started = 0;
    int uses_raknet = 0;
    LONG attempt = InterlockedIncrement(&g_runtime.net_mgr_connect_attempted);

    g_runtime.net_mgr_last_connect_attempt_tick = now_ms;
    InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_CONNECTING);
    InterlockedExchange(&g_runtime.raknet_rpc_flags, 0);
    InterlockedExchange(&g_runtime.raknet_request_class_outcome, 0);
    InterlockedExchange(&g_runtime.raknet_request_spawn_outcome, 0);
    InterlockedExchange(&g_runtime.raknet_last_dialog_id, 0);
    InterlockedExchange(&g_runtime.raknet_game_rpc_flags, 0);
    InterlockedExchange(&g_runtime.raknet_init_game_applied, 0);
    InterlockedExchange(&g_runtime.chat_client_message_seq, 0);
    InterlockedExchange(&g_runtime.online_session_drift_seen, 0);
    InterlockedExchange(&g_runtime.online_session_last_entry, 0);
    InterlockedExchange(&g_runtime.online_session_last_game_started, 0);
    InterlockedExchange(&g_runtime.mp_session_apply_count, 0);
    InterlockedExchange(&g_runtime.mp_session_teleport_count, 0);
    InterlockedExchange(&g_runtime.mp_session_script_failures, 0);
    InterlockedExchange(&g_runtime.mp_session_frontend_hold_logged, 0);
    g_runtime.raknet_weather = 0u;
    g_runtime.raknet_interior = 0u;
    g_runtime.raknet_camera_look_at_type = 0u;
    g_runtime.raknet_init_world_time = 0u;
    g_runtime.raknet_init_weather = 0u;
    g_runtime.raknet_init_lan_mode = 0u;
    g_runtime.raknet_init_disable_enter_exits = 0u;
    g_runtime.raknet_init_stunt_bonus = 0u;
    g_runtime.raknet_init_spawns_available = 0u;
    g_runtime.raknet_init_local_player_id = 0u;
    g_runtime.raknet_init_death_drop_money = 0;
    g_runtime.raknet_player_facing_angle = 0.0f;
    g_runtime.raknet_init_gravity = 0.0f;
    g_runtime.raknet_init_name_tag_draw_distance = 0.0f;
    g_runtime.raknet_init_global_chat_radius = 0.0f;
    memset(g_runtime.raknet_player_pos, 0, sizeof(g_runtime.raknet_player_pos));
    memset(g_runtime.raknet_camera_pos, 0, sizeof(g_runtime.raknet_camera_pos));
    memset(g_runtime.raknet_camera_look_at, 0, sizeof(g_runtime.raknet_camera_look_at));
    memset(g_runtime.raknet_init_send_rates, 0, sizeof(g_runtime.raknet_init_send_rates));
    memset(g_runtime.raknet_init_vehicle_models, 0, sizeof(g_runtime.raknet_init_vehicle_models));
    memset(g_runtime.raknet_init_hostname, 0, sizeof(g_runtime.raknet_init_hostname));
    uses_raknet =
        InterlockedCompareExchange(&g_runtime.net_mgr_inited, 0, 0) ? samp_net_mgr_uses_raknet(&g_runtime.net_mgr) : 0;
    runtime_tracef("network_prepare: Connecting to %s:%u attempt=%ld uses_raknet=%d", g_runtime.endpoint.host,
                   (unsigned)g_runtime.endpoint.port, (long)attempt, uses_raknet);
    if (InterlockedCompareExchange(&g_runtime.chat_connecting_logged, 1, 0) == 0) {
      chat_compat_add_message("Connecting to %s:%u...", g_runtime.endpoint.host, (unsigned)g_runtime.endpoint.port);
    }

    if (InterlockedCompareExchange(&g_runtime.net_mgr_inited, 0, 0) && samp_net_mgr_uses_raknet(&g_runtime.net_mgr)) {
      if (g_runtime.net_mgr.raknet_client != NULL) {
        samp_raknet_client_disconnect(g_runtime.net_mgr.raknet_client, 0U, 0U);
      }
      connect_started =
          (samp_net_mgr_connect_raknet(&g_runtime.net_mgr, g_runtime.endpoint.host, g_runtime.endpoint.port, 0,
                                       SAMP_RAKNET_SLEEP_TIMER) == 0);
    } else {
      samp_socket_t sock = SAMP_INVALID_SOCKET;
      g_runtime.connect_probe_attempted = 1;
      g_runtime.connect_probe_ok =
          (samp_net_connect_dual_stack(g_runtime.endpoint.host, g_runtime.endpoint.port, 200, &sock) == 0);
      connected = g_runtime.connect_probe_ok;
      runtime_tracef("network_prepare: dual_stack probe attempted=%d ok=%d", g_runtime.connect_probe_attempted,
                     g_runtime.connect_probe_ok);
      if (sock != SAMP_INVALID_SOCKET) {
        samp_net_close_socket(sock);
      }
    }

    if (connected) {
      InterlockedExchange(&g_runtime.net_mgr_connected, 1);
      InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_CONNECTED);
      runtime_tracef("network_prepare: connect result CONNECTED");
      if (InterlockedCompareExchange(&g_runtime.start_game_called, 0, 0) != 0) {
        apply_session_frontend_hold_flags_compat("connected_no_spawn");
        runtime_tracef("network_prepare: holding frontend flags on CONNECTED");
      }
    } else if (uses_raknet && connect_started) {
      runtime_tracef("network_prepare: connect initiated (awaiting RakNet handshake)");
    } else {
      InterlockedExchange(&g_runtime.net_mgr_connected, 0);
      InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_WAIT_CONNECT);
      runtime_tracef("network_prepare: connect result WAIT_CONNECT");
    }
  }

  if (InterlockedCompareExchange(&g_runtime.net_mgr_inited, 0, 0) && samp_net_mgr_uses_raknet(&g_runtime.net_mgr)) {
    int drained = -1;
    int join_sent = 0;
    int last_packet_id = -1;
    int connected_after_pump = 0;
    uint32_t rpc_flags = 0u;
    LONG state_before = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);

    connected_after_pump = samp_net_mgr_raknet_is_connected(&g_runtime.net_mgr);
    drained = samp_raknet_client_drain_packets_autojoin(g_runtime.net_mgr.raknet_client, SAMP_RAKNET_PUMP_BUDGET,
                                                         &g_runtime.raknet_join_profile, &connected_after_pump, &join_sent,
                                                         &last_packet_id);
    rpc_flags = refresh_raknet_rpc_snapshot_compat();
    if (drained >= 0) {
      InterlockedIncrement(&g_runtime.raknet_pump_calls);
    }

    if (join_sent && InterlockedCompareExchange(&g_runtime.raknet_join_sent, 1, 0) == 0) {
      runtime_tracef("network_prepare: RPC_ClientJoin sent nick='%s' version='%s' netver=%lu mod=%u",
                     g_runtime.raknet_join_profile.nickname, g_runtime.raknet_join_profile.client_version,
                     (unsigned long)g_runtime.raknet_join_profile.netgame_version,
                     (unsigned)g_runtime.raknet_join_profile.mod_byte);
    }

    if (last_packet_id >= 0) {
      LONG previous_packet_id = InterlockedExchange(&g_runtime.raknet_last_packet_id, (LONG)last_packet_id);
      if (previous_packet_id != (LONG)last_packet_id && last_packet_id != 255) {
        runtime_tracef("network_prepare: RakNet packet id=%d (%s)", last_packet_id, raknet_packet_name(last_packet_id));
      }

      switch (last_packet_id) {
        case 17: /* ID_CONNECTION_ATTEMPT_FAILED */
        case 19: /* ID_NO_FREE_INCOMING_CONNECTIONS */
          InterlockedExchange(&g_runtime.net_mgr_connected, 0);
          InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_WAIT_CONNECT);
          runtime_tracef("network_prepare: RakNet connect not ready -> state=WAIT_CONNECT");
          break;
        case 20: /* ID_DISCONNECTION_NOTIFICATION */
        case 21: /* ID_CONNECTION_LOST */
          InterlockedExchange(&g_runtime.net_mgr_connected, 0);
          InterlockedExchange(&g_runtime.raknet_join_sent, 0);
          InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_WAIT_CONNECT);
          runtime_tracef("network_prepare: RakNet disconnected -> state=WAIT_CONNECT");
          break;
        case 23: /* ID_CONNECTION_BANNED */
        case 24: /* ID_INVALID_PASSWORD */
        case 25: /* ID_MODIFIED_PACKET */
          InterlockedExchange(&g_runtime.net_mgr_connected, 0);
          InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_FAILED);
          runtime_tracef("network_prepare: RakNet terminal packet -> state=FAILED");
          break;
        default:
          break;
      }
    }

    if (connected_after_pump) {
      InterlockedExchange(&g_runtime.net_mgr_connected, 1);
    }

    if ((rpc_flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) != 0u) {
      InterlockedExchange(&g_runtime.net_mgr_connected, 1);
      InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_CONNECTED);
      if (state_before != SAMP_NETGAME_CONNECTED) {
        runtime_tracef("network_prepare: RakNet state CONNECTED (InitGame observed)");
        if (InterlockedCompareExchange(&g_runtime.chat_joining_logged, 1, 0) == 0) {
          chat_compat_add_message("Connected. Joining the game...");
        }
        if (InterlockedCompareExchange(&g_runtime.start_game_called, 0, 0) != 0) {
          apply_session_frontend_hold_flags_compat("initgame_no_spawn");
          runtime_tracef("network_prepare: holding frontend flags on InitGame");
        }
      }
    }
  }
}

static void launch_do_init_stuff_compat(void) {
  if (InterlockedCompareExchange(&g_runtime.do_init_stuff_active, 1, 0) != 0) {
    LONG skips = InterlockedIncrement(&g_runtime.do_init_stuff_reentry_skips);
    if (skips <= 3) {
      runtime_tracef("do_init_stuff: reentry skip #%ld", (long)skips);
    }
    return;
  }

  InterlockedIncrement(&g_runtime.do_init_stuff_calls);
  if (InterlockedCompareExchange(&g_runtime.do_init_stuff_calls, 0, 0) <= 3) {
    runtime_tracef("do_init_stuff call #%ld", (long)InterlockedCompareExchange(&g_runtime.do_init_stuff_calls, 0, 0));
  }

  if (!InterlockedCompareExchange(&g_runtime.game_inited, 0, 0)) {
    launch_init_game_compat();
    runtime_tracef("do_init_stuff: forced init_game");
    goto done;
  }

  if (g_runtime.d3dx9_module == NULL) {
    g_runtime.d3dx9_module = LoadLibraryA("d3dx9_25.dll");
    runtime_tracef("do_init_stuff: d3dx9_25 load %s", (g_runtime.d3dx9_module != NULL) ? "OK" : "FAILED");
  }
  if (g_runtime.bass_module == NULL) {
    g_runtime.bass_module = LoadLibraryA("BASS.dll");
    runtime_tracef("do_init_stuff: BASS load %s", (g_runtime.bass_module != NULL) ? "OK" : "FAILED");
  }
  apply_time_passing_patch_compat();

  launch_prepare_network_compat();

done:
  InterlockedExchange(&g_runtime.do_init_stuff_active, 0);
}

static void launch_graphics_loop_hook_callback(void) {
  LONG calls = InterlockedIncrement(&g_runtime.hook_callback_calls);
  if (calls <= 3) {
    runtime_tracef("hook_callback: call #%ld", (long)calls);
  }
  apply_preconnect_frontend_compat();
  launch_do_init_stuff_compat();
  apply_multiplayer_session_bridge_compat();
  chat_compat_draw_overlay();
}

static int phase_hook_bridge_configure(void) {
  if (samp_hook_bridge_configure_from_env(&g_runtime.hook_bridge) != 0) {
    runtime_tracef("hook_configure: FAILED");
    return 0;
  }
  if (g_runtime.settings.play_online || g_runtime.settings.debug) {
    if (!g_runtime.hook_bridge.enabled) {
      g_runtime.hook_bridge.enabled = 1;
      runtime_tracef("hook_configure: enabled by settings online=%d debug=%d", g_runtime.settings.play_online ? 1 : 0,
                     g_runtime.settings.debug ? 1 : 0);
    }
    if (!g_runtime.hook_bridge.configured && !g_runtime.hook_bridge.secondary_configured) {
      g_runtime.hook_bridge.graphics_call_disp_addr = (uintptr_t)SAMP_ADDR_LEGACY_GRAPHICS_CALL_DISP;
      g_runtime.hook_bridge.configured = 1;
      runtime_tracef("hook_configure: default legacy graphics disp=0x%08lx",
                     (unsigned long)g_runtime.hook_bridge.graphics_call_disp_addr);
    }
  }
  runtime_tracef("hook_configure: enabled=%d configured=%d configured2=%d disp=0x%08lx disp2=0x%08lx",
                 g_runtime.hook_bridge.enabled, g_runtime.hook_bridge.configured, g_runtime.hook_bridge.secondary_configured,
                 (unsigned long)g_runtime.hook_bridge.graphics_call_disp_addr,
                 (unsigned long)g_runtime.hook_bridge.graphics_call_disp_addr_secondary);
  return 1;
}

static unsigned __stdcall launch_monitor_thread(void *unused) {
  (void)unused;
  LONG last_entry_gate = -1;

  InterlockedExchange(&g_runtime.monitor_started, 1);
  launch_init_game_compat();
  runtime_tracef("launch_monitor: started play_online=%d debug=%d", g_runtime.settings.play_online ? 1 : 0,
                 g_runtime.settings.debug ? 1 : 0);
  runtime_tracef("launch_monitor: drive_init=%d", monitor_drive_init_enabled());
  if (!monitor_drive_init_enabled() && g_runtime.settings.play_online) {
    runtime_tracef("launch_monitor: network pump forced while drive_init=0");
  }

  while (WaitForSingleObject(g_runtime.stop_event, SAMP_LAUNCH_WAIT_MS) == WAIT_TIMEOUT) {
    LONG observed_entry_gate = read_game_entry_gate_value();
    LONG target_entry_gate = InterlockedCompareExchange(&g_runtime.entry_gate, 0, 0);
    if (observed_entry_gate != last_entry_gate) {
      runtime_tracef("launch_monitor: entry_gate observed=%ld target=%ld", (long)observed_entry_gate,
                     (long)target_entry_gate);
      last_entry_gate = observed_entry_gate;
    }

    if (g_runtime.settings.play_online && observed_entry_gate >= 5) {
      clamp_launch_startgame_flags();
    }

    if (observed_entry_gate == target_entry_gate) {
      break;
    }
  }

  if (WaitForSingleObject(g_runtime.stop_event, 0) != WAIT_OBJECT_0) {
    if (g_runtime.settings.play_online || g_runtime.settings.debug) {
      clamp_launch_startgame_flags();
      launch_start_game_compat();
    } else {
      runtime_tracef("launch_monitor: start_game skipped (SP mode)");
    }
  }

  while (WaitForSingleObject(g_runtime.stop_event, SAMP_GRAPHICS_TICK_MS) == WAIT_TIMEOUT) {
    LONG hook_calls = InterlockedCompareExchange(&g_runtime.hook_callback_calls, 0, 0);
    if (hook_calls == 0) {
      apply_preconnect_frontend_compat();
    }
    if (monitor_drive_init_enabled() && hook_calls == 0) {
      launch_do_init_stuff_compat();
    } else if (!monitor_drive_init_enabled() && g_runtime.settings.play_online && hook_calls == 0) {
      launch_prepare_network_compat();
    }
    maintain_connect_wait_state();
    if (hook_calls != 0) {
      apply_preconnect_frontend_compat();
    }
    maintain_online_session_state();
    chat_compat_draw_overlay();
    InterlockedIncrement(&g_runtime.graphics_ticks);
  }

  InterlockedExchange(&g_runtime.monitor_stopped, 1);
  runtime_tracef("launch_monitor: stopped ticks=%ld", (long)InterlockedCompareExchange(&g_runtime.graphics_ticks, 0, 0));
  return 0;
}

static int phase_launch_monitor_start(void) {
  uintptr_t thread_handle = 0;

  g_runtime.stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
  if (g_runtime.stop_event == NULL) {
    return 0;
  }

  thread_handle = _beginthreadex(NULL, 0, launch_monitor_thread, NULL, 0, NULL);
  if (thread_handle == 0) {
    CloseHandle(g_runtime.stop_event);
    g_runtime.stop_event = NULL;
    return 0;
  }

  g_runtime.launch_thread = (HANDLE)thread_handle;
  runtime_tracef("launch_monitor: thread started handle=%p", g_runtime.launch_thread);
  g_last_phase = BOOT_PHASE_6_LAUNCH_MONITOR;
  return 1;
}

static void phase_launch_monitor_stop(void) {
  if (g_runtime.stop_event != NULL) {
    SetEvent(g_runtime.stop_event);
  }

  if (g_runtime.launch_thread != NULL) {
    WaitForSingleObject(g_runtime.launch_thread, SAMP_LAUNCH_THREAD_JOIN_MS);
    CloseHandle(g_runtime.launch_thread);
    g_runtime.launch_thread = NULL;
  }

  if (g_runtime.stop_event != NULL) {
    CloseHandle(g_runtime.stop_event);
    g_runtime.stop_event = NULL;
  }
}

static void phase_network_shutdown(void) {
  if (InterlockedCompareExchange(&g_runtime.net_mgr_inited, 0, 0)) {
    (void)samp_net_mgr_destroy(&g_runtime.net_mgr);
    InterlockedExchange(&g_runtime.net_mgr_inited, 0);
  }
  samp_net_runtime_shutdown();
}

static void phase_runtime_modules_shutdown(void) {
  chat_compat_uninstall_d3d_hook();
  chat_compat_release_d3dx_font();
  if (g_runtime.chat_overlay_font != NULL) {
    DeleteObject(g_runtime.chat_overlay_font);
    g_runtime.chat_overlay_font = NULL;
  }
  if (g_runtime.bass_module != NULL) {
    FreeLibrary(g_runtime.bass_module);
    g_runtime.bass_module = NULL;
  }
  if (g_runtime.d3dx9_module != NULL) {
    FreeLibrary(g_runtime.d3dx9_module);
    g_runtime.d3dx9_module = NULL;
  }
}

static void phase_runtime_bootstrap_shutdown(void) {
  if (g_runtime.gtaweap3_font_added > 0 && g_runtime.gtaweap3_font_path[0] != '\0') {
    RemoveFontResourceA(g_runtime.gtaweap3_font_path);
    g_runtime.gtaweap3_font_added = 0;
  }
  if (g_runtime.sampaux3_font_added > 0 && g_runtime.sampaux3_font_path[0] != '\0') {
    RemoveFontResourceA(g_runtime.sampaux3_font_path);
    g_runtime.sampaux3_font_added = 0;
  }
  if (g_runtime.bootstrap_manager_block != NULL) {
    HeapFree(GetProcessHeap(), 0u, g_runtime.bootstrap_manager_block);
    g_runtime.bootstrap_manager_block = NULL;
  }
}

static void phase_exception_filter_restore(void) {
  SetUnhandledExceptionFilter(g_runtime.previous_filter);
  g_runtime.previous_filter = NULL;
}

static void phase_runtime_guard_reset(void) {
  LONG thread_attach_count = g_runtime.thread_attach_count;
  LONG thread_detach_count = g_runtime.thread_detach_count;

  memset(&g_runtime, 0, sizeof(g_runtime));
  g_runtime.thread_attach_count = thread_attach_count;
  g_runtime.thread_detach_count = thread_detach_count;
  g_last_phase = BOOT_PHASE_0_NONE;
}

static void rollback_from_phase(samp_boot_phase phase) {
  if (phase >= BOOT_PHASE_6_LAUNCH_MONITOR) {
    phase_launch_monitor_stop();
  }
  uninstall_game_process_hook_compat();
  uninstall_scripts_process_hook_compat();
  samp_hook_bridge_uninstall(&g_runtime.hook_bridge);
  restore_time_passing_patch_compat();
  phase_runtime_modules_shutdown();
  if (phase >= BOOT_PHASE_5_NETWORK_INIT) {
    phase_network_shutdown();
  }
  if (phase >= BOOT_PHASE_4_EARLY_BOOTSTRAP) {
    phase_runtime_bootstrap_shutdown();
  }
  if (phase >= BOOT_PHASE_2_EXCEPTION_FILTER) {
    phase_exception_filter_restore();
  }
  if (phase >= BOOT_PHASE_1_RUNTIME_GUARD) {
    phase_runtime_guard_reset();
  }
}

static int process_attach(HINSTANCE instance) {
  if (InterlockedCompareExchange(&g_runtime.initialized, 0, 0)) {
    InterlockedIncrement(&g_runtime.process_attach_count);
    return 1;
  }

  if (!phase_runtime_guard_attach(instance)) {
    return 0;
  }
  if (!phase_install_exception_filter()) {
    rollback_from_phase(g_last_phase);
    return 0;
  }
  if (!phase_load_settings_and_paths()) {
    rollback_from_phase(g_last_phase);
    return 0;
  }
  if (!phase_hook_bridge_configure()) {
    rollback_from_phase(g_last_phase);
    return 0;
  }
  if (!phase_runtime_bootstrap()) {
    rollback_from_phase(g_last_phase);
    return 0;
  }
  if (!phase_network_init()) {
    rollback_from_phase(g_last_phase);
    return 0;
  }
  if (!phase_launch_monitor_start()) {
    rollback_from_phase(g_last_phase);
    return 0;
  }

  InterlockedExchange(&g_runtime.initialized, 1);
  g_last_phase = BOOT_PHASE_7_READY;
  return 1;
}

static void process_detach(LPVOID reserved) {
  (void)reserved;

  if (!InterlockedCompareExchange(&g_runtime.initialized, 0, 0)) {
    return;
  }

  phase_launch_monitor_stop();
  uninstall_game_process_hook_compat();
  uninstall_scripts_process_hook_compat();
  samp_hook_bridge_uninstall(&g_runtime.hook_bridge);
  restore_time_passing_patch_compat();
  phase_runtime_modules_shutdown();
  phase_runtime_bootstrap_shutdown();
  phase_network_shutdown();
  phase_exception_filter_restore();
  runtime_tracef("process_detach: done hooks_installed=%ld hook_callback_calls=%ld net_state=%ld",
                 (long)g_runtime.hooks_installed, (long)g_runtime.hook_callback_calls, (long)g_runtime.netgame_state);
  phase_runtime_guard_reset();
}

static int thread_attach(void) {
  InterlockedIncrement(&g_runtime.thread_attach_count);
  return 1;
}

static int thread_detach(void) {
  InterlockedIncrement(&g_runtime.thread_detach_count);
  return 1;
}

int samp_runtime_dispatch(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      return process_attach(instance);
    case DLL_PROCESS_DETACH:
      process_detach(reserved);
      return 1;
    case DLL_THREAD_ATTACH:
      return thread_attach();
    case DLL_THREAD_DETACH:
      return thread_detach();
    default:
      return 1;
  }
}
#endif
