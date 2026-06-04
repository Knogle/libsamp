#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <process.h>
#include <math.h>
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
/* OLD_02X_REF + PROBE_TRACE:
 * The legacy client drains RakClient::Receive() until empty each network tick.
 * Our replacement keeps a safety cap, but object/vehicle streaming bursts can
 * exceed small caps and correlate with later ID_CONNECTION_LOST/ID_DISCONNECTION.
 */
#define SAMP_RAKNET_PUMP_BUDGET 1024
#define SAMP_RAKNET_SLEEP_TIMER 5
#define SAMP_RAKNET_CONNECT_RETRY_MS 3000
#define SAMP_RAKNET_CONNECT_STALE_MS 12000
#define SAMP_PRECONNECT_DELAY_MS 10000
#define SAMP_PRECONNECT_MAX_APPLIES 7200
#define SAMP_PRECONNECT_PED_FALLBACK_APPLIES 720
#define SAMP_PRECONNECT_WORLD_SETTLE_MS 0u
#define SAMP_PRECONNECT_WORLD_HOUR 6u
#define SAMP_PRECONNECT_WORLD_MINUTE 0u
#define SAMP_PRECONNECT_PLAYER_X -276.7370f
#define SAMP_PRECONNECT_PLAYER_Y 2242.7578f
#define SAMP_PRECONNECT_PLAYER_Z 129.6658f
#define SAMP_PRECONNECT_CAMERA_X -476.5340f
#define SAMP_PRECONNECT_CAMERA_Y 2548.5710f
#define SAMP_PRECONNECT_CAMERA_Z 178.1568f
#define SAMP_PRECONNECT_LOOK_X -276.7370f
#define SAMP_PRECONNECT_LOOK_Y 2242.7578f
#define SAMP_PRECONNECT_LOOK_Z 129.6658f
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
#define SAMP_ADDR_SELECT_DEVICE_HOOK 0x746219u
#define SAMP_ADDR_SELECT_DEVICE_RETURN_SINGLE 0x746273u
#define SAMP_ADDR_SELECT_DEVICE_RETURN_MULTI_HIDE 0x74622Cu
#define SAMP_ADDR_SELECT_DEVICE_RETURN_MULTI_SHOW 0x746227u
#define SAMP_SELECT_DEVICE_HOOK_SIZE 6u
#define SAMP_SELECT_DEVICE_STUB_SIZE 64u
#define SAMP_ADDR_D3D9_NUM_DISPLAY_MODES 0xC97C40u
#define SAMP_ADDR_D3D9_DISPLAY_MODES 0xC97C48u
#define SAMP_ADDR_VIDEO_MODE_LIST_WIDTH_CHECK 0x745B71u
#define SAMP_ADDR_VIDEO_MODE_LIST_HEIGHT_CHECK 0x745B81u
#define SAMP_ADDR_VIDEO_MODE_LIST_ASPECT_CHECK 0x745B96u
#define SAMP_ADDR_VIDEO_MODE_LIST_VRAM_CHECK 0x745BFCu
#define SAMP_ADDR_DEVICE_DIALOG_WIDTH_CHECK 0x74596Cu
#define SAMP_ADDR_DEVICE_DIALOG_HEIGHT_CHECK 0x74597Au
#define SAMP_ADDR_DEVICE_DIALOG_ASPECT_CHECK 0x7459D0u
#define SAMP_ADDR_DEFAULT_RES_WIDTH_IMM 0x746363u
#define SAMP_ADDR_DEFAULT_RES_HEIGHT_IMM 0x746368u
#define SAMP_ADDR_RW_ENGINE_GET_NUM_SUBSYSTEMS 0x7F2C00u
#define SAMP_ADDR_RW_ENGINE_SET_SUBSYSTEM 0x7F2C90u
#define SAMP_ADDR_RW_ENGINE_GET_NUM_VIDEO_MODES 0x7F2CC0u
#define SAMP_ADDR_RW_ENGINE_GET_VIDEO_MODE_INFO 0x7F2CF0u
#define SAMP_ADDR_FRONTEND_CUR_VIDEO_MODE 0x8D6220u
#define SAMP_ADDR_FRONTEND_SAVED_VIDEO_MODE 0xBA6820u
#define SAMP_ADDR_FRONTEND_CUR_ADAPTER 0xC920F4u
#define SAMP_RW_VIDEOMODE_EXCLUSIVE 0x0001u
#define SAMP_D3DFMT_R5G6B5 23u
#define SAMP_D3DFMT_X8R8G8B8 22u
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
#define SAMP_ADDR_MONEY 0xB7CE50u
#define SAMP_ADDR_VEHICLE_TABLE 0xB74494u
#define SAMP_ADDR_VEHICLE_FROM_ID 0x4048E0u
#define SAMP_ADDR_PROCESS_ONE_COMMAND 0x469EB0u
#define SAMP_ADDR_STREAMING_REQUEST_MODEL 0x4087E0u
#define SAMP_ADDR_STREAMING_LOAD_ALL_REQUESTED 0x40EA10u
#define SAMP_ADDR_STREAMING_LOAD_SCENE 0x40EB70u
#define SAMP_ADDR_STREAMING_LOAD_SCENE_COLLISION 0x40ED80u
#define SAMP_ADDR_MODEL_INFO_PTRS 0xA9B0C8u
#define SAMP_ADDR_ANIM_ASSOC_GROUPS_PTR 0xB4EA34u
#define SAMP_ADDR_RPANIMBLEND_CLUMP_OFFSET 0xB5F878u
#define SAMP_ADDR_ENTRY_INFO_NODE_POOL_PTR 0xB7448Cu
#define SAMP_ADDR_HIGH_SPEED_MOTION_BLUR_PATCH 0x704E8Au
#define SAMP_ADDR_PED_AUDIO_INIT 0x4E68D0u
#define SAMP_ADDR_PLAYERPED_DONT_RESTORE_PLAYER_ANIMS 0x609A4Eu
#define SAMP_ADDR_PLAYERPED_PROCESS_CONTROL_ANIM_CRASH 0x609C08u
#define SAMP_ADDR_SCANLIST_ORIGINAL 0xB7D0B8u
#define SAMP_SCANLIST_SIZE (8u * 20000u)
#define SAMP_SCANLIST_ORIGINAL_RESET_SIZE (8u * 14400u)
#define SAMP_SCRIPT_THREAD_BYTES 0xE0u
#define SAMP_SCRIPT_BUF_BYTES 255u
#define SAMP_MP_BRIDGE_MAX_APPLIES 3600
#define SAMP_MP_BRIDGE_TELEPORT_PERIOD 15
#define SAMP_ONFOOT_SYNC_INTERVAL_MS 250u
#define SAMP_LOCAL_STREAM_REFRESH_INTERVAL_MS 500u
#define SAMP_LOCAL_STREAM_REFRESH_DISTANCE 35.0f
#define SAMP_PED_OFFSET_MATRIX 20u
#define SAMP_PED_OFFSET_STATE_FLAGS 1132u
#define SAMP_PED_OFFSET_HEALTH 1344u
#define SAMP_PED_OFFSET_ARMOUR 1352u
#define SAMP_PED_OFFSET_AUDIO_ENTITY 660u
#define SAMP_PED_OFFSET_ROTATION1 1368u
#define SAMP_PED_OFFSET_ROTATION2 1372u
#define SAMP_PED_OFFSET_VEHICLE 1420u
#define SAMP_PED_STATE_IN_VEHICLE 0x100u
#define SAMP_VEHICLE_SCANNER_DISTANCE 200.0f
#define SAMP_VEHICLE_OFFSET_DRIVER 1120u
#define SAMP_VEHICLE_OFFSET_PASSENGERS 1124u
#define SAMP_VEHICLE_PASSENGER_COUNT 7u
#define SAMP_VEHICLE_SCANNER_MARKER_COLOR 500
#define SAMP_VEHICLE_SCANNER_MARKER_TYPE 2
#define SAMP_ENTITY_OFFSET_RW_OBJECT 24u
#define SAMP_ENTITY_OFFSET_MODEL_INDEX 34u
#define SAMP_ENTITY_OFFSET_MOVE_SPEED 68u
#define SAMP_MATRIX_OFFSET_POS 48u
#define SAMP_POOL_OFFSET_OBJECTS 0u
#define SAMP_POOL_OFFSET_FLAGS 4u
#define SAMP_POOL_OFFSET_SIZE 8u
#define SAMP_POOL_OFFSET_TOP 12u
#define SAMP_GTA_ACTOR_LOCAL_ID 1
#define SAMP_GTA_PLAYER_LOCAL_ID 0
#define SAMP_DEG_TO_RAD 0.01745329251994329577f
#define SAMP_CHAT_COMPAT_MAX_LINES 8
#define SAMP_CHAT_COMPAT_LINE_BYTES 256
#define SAMP_CHAT_COMPAT_BASE_X 30
#define SAMP_CHAT_COMPAT_BASE_Y 145
#define SAMP_CHAT_COMPAT_LINE_HEIGHT 18
#define SAMP_CHAT_COMPAT_COLOR_INFO 0xFFECECECu
#define SAMP_SCOREBOARD_MAX_PLAYERS 1000
#define SAMP_SCOREBOARD_MAX_VISIBLE 20
#define SAMP_SCOREBOARD_FONT_SIZE 16
#define SAMP_SCOREBOARD_LINE_HEIGHT 22
#define SAMP_SCOREBOARD_COLOR_PANEL 0xB0000000u
#define SAMP_SCOREBOARD_COLOR_HEADER 0xFF101010u
#define SAMP_SCOREBOARD_COLOR_GRID 0xFF303030u
#define SAMP_SCOREBOARD_COLOR_LABEL 0xFF6496C8u
#define SAMP_SCOREBOARD_COLOR_MUTED 0xFF969696u
#define SAMP_CHAT_INPUT_MAX 128
#define SAMP_CHAT_INPUT_HISTORY 10
#define SAMP_CHAT_INPUT_DRAW_BYTES 160
#define SAMP_D3D9_END_SCENE_INDEX 42u
#define SAMP_D3D9_GET_FRONT_BUFFER_DATA_INDEX 33u
#define SAMP_D3D9_CREATE_OFFSCREEN_PLAIN_SURFACE_INDEX 36u
#define SAMP_D3D9_CLEAR_INDEX 43u
#define SAMP_D3D9_SET_RENDER_STATE_INDEX 57u
#define SAMP_D3D9_SET_TEXTURE_INDEX 65u
#define SAMP_D3D9_SET_TEXTURE_STAGE_STATE_INDEX 67u
#define SAMP_D3D9_SET_SAMPLER_STATE_INDEX 69u
#define SAMP_D3D9_DRAW_PRIMITIVE_UP_INDEX 83u
#define SAMP_D3D9_SET_FVF_INDEX 89u
#define SAMP_D3DRS_ZENABLE 7u
#define SAMP_D3DRS_SRCBLEND 19u
#define SAMP_D3DRS_DESTBLEND 20u
#define SAMP_D3DRS_ALPHABLENDENABLE 27u
#define SAMP_D3DSAMP_MAGFILTER 5u
#define SAMP_D3DSAMP_MINFILTER 6u
#define SAMP_D3DTEXF_LINEAR 2u
#define SAMP_D3DTSS_COLOROP 1u
#define SAMP_D3DTSS_COLORARG1 2u
#define SAMP_D3DTSS_COLORARG2 3u
#define SAMP_D3DTSS_ALPHAOP 4u
#define SAMP_D3DTSS_ALPHAARG1 5u
#define SAMP_D3DTSS_ALPHAARG2 6u
#define SAMP_D3DTOP_MODULATE 4u
#define SAMP_D3DTA_DIFFUSE 0u
#define SAMP_D3DTA_TEXTURE 2u
#define SAMP_D3DBLEND_SRCALPHA 5u
#define SAMP_D3DBLEND_INVSRCALPHA 6u
#define SAMP_D3DPT_TRIANGLESTRIP 5u
#define SAMP_D3DFVF_XYZRHW_DIFFUSE 0x00000044u
#define SAMP_D3DFVF_XYZRHW_DIFFUSE_TEX1 0x00000144u
#define SAMP_D3DCLEAR_TARGET 0x00000001u
#define SAMP_D3DFMT_A8R8G8B8 21u
#define SAMP_D3DPOOL_SCRATCH 3u
#define SAMP_D3DXIFF_PNG 3u
#define SAMP_CHAT_INPUT_BOX_COLOR 0x33000000u
#define SAMP_DIALOG_COMPAT_MAX_VISIBLE_ITEMS 8
#define SAMP_DIALOG_COMPAT_MAX_ITEM_BYTES 256
#define SAMP_DIALOG_COMPAT_COLOR_PANEL 0xE0000000u
#define SAMP_DIALOG_COMPAT_COLOR_HEADER 0xFF202020u
#define SAMP_DIALOG_COMPAT_COLOR_BODY 0xFF050505u
#define SAMP_DIALOG_COMPAT_COLOR_SELECTED 0xFF9E1B1Bu
#define SAMP_DIALOG_COMPAT_COLOR_BUTTON 0xFF171717u
#define SAMP_DIALOG_COMPAT_COLOR_TEXT 0xFFFFFFFFu
#define SAMP_DIALOG_COMPAT_COLOR_MUTED 0xFFB8B8B8u
#define SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS 16
#define SAMP_TEXTDRAW_COMPAT_FONT_MIN_HEIGHT 10
#define SAMP_TEXTDRAW_COMPAT_FONT_STEP 4
#define SAMP_TEXTDRAW_COMPAT_SCRIPT_WIDTH 640.0f
#define SAMP_TEXTDRAW_COMPAT_SCRIPT_HEIGHT 448.0f
#define SAMP_TEXTDRAW_COMPAT_MIN_WIDTH 48
/* OLD_02X_REF + TODO_VERIFY:
 * 0.2x CTextDraw::Draw delegates normal textdraw text/box output to GTA SA CFont.
 * Keep D3DX as fallback and for 0.3.7 preview-model textdraws until OBSERVED_037 confirms that path.
 */
#define SAMP_ADDR_SCREEN_WIDTH 0xC17044u
#define SAMP_ADDR_SCREEN_HEIGHT 0xC17048u
#define SAMP_ADDR_HUD_HORIZ_SCALE 0x859520u
#define SAMP_ADDR_HUD_VERT_SCALE 0x859524u
#define SAMP_ADDR_FONT_SET_SCALE 0x719380u
#define SAMP_ADDR_FONT_SET_COLOR 0x719430u
#define SAMP_ADDR_FONT_SET_STYLE 0x719490u
#define SAMP_ADDR_FONT_SET_LINE_WIDTH 0x7194D0u
#define SAMP_ADDR_FONT_SET_LINE_HEIGHT 0x7194E0u
#define SAMP_ADDR_FONT_SET_DROPCOLOR 0x719510u
#define SAMP_ADDR_FONT_SET_SHADOW 0x719570u
#define SAMP_ADDR_FONT_SET_OUTLINE 0x719590u
#define SAMP_ADDR_FONT_SET_PROPORTIONAL 0x7195B0u
#define SAMP_ADDR_FONT_USE_BOX 0x7195C0u
#define SAMP_ADDR_FONT_USE_BOX_COLOR 0x7195E0u
#define SAMP_ADDR_FONT_UNK12 0x719600u
#define SAMP_ADDR_FONT_SET_JUSTIFY 0x719610u
#define SAMP_ADDR_FONT_PRINT_STRING 0x71A700u
#define SAMP_TEXTDRAW_COMPAT_GTA_FONT_ENV "SAMP_TEXTDRAW_GTA_FONT"
/* GTA_REVERSED_REF + OLD_02X_REF + TODO_VERIFY:
 * 0x69E160 is CMessages::InsertPlayerControlKeysInString in gta-reversed.
 * The 0.2x Font_UnkConv wrapper returns 0x69DE90 rather than calling it like a
 * regular C function, so neither conversion helper is safe as a blind textdraw
 * normalizer until we validate the original 0.3.7 call site.
 */
#define SAMP_ADDR_MESSAGES_INSERT_PLAYER_CONTROL_KEYS 0x69E160u
#define SAMP_TEXTDRAW_COMPAT_MAX_TEXT_BYTES SAMP_RAKNET_TEXTDRAW_TEXT_BYTES
#define SAMP_OBJECT_COMPAT_MODEL_LOAD_FLAGS 0x06
#define SAMP_OBJECT_COMPAT_CREATE_BUDGET 4u
#define SAMP_OBJECT_COMPAT_ACTIVE_SOFT_CAP 96
#define SAMP_OBJECT_COMPAT_RECREATE_DELAY_MS 100u
#define SAMP_OBJECT_COMPAT_BACKPRESSURE_LOG_INTERVAL_MS 2000u
#define SAMP_OBJECT_COMPAT_CHAT_SKIP_LIMIT 3
#define SAMP_OBJECT_COMPAT_SAMP_MODEL_MIN 18631
#define SAMP_VEHICLE_COMPAT_MODEL_LOAD_FLAGS 0x06
#define SAMP_VEHICLE_COMPAT_CREATE_BUDGET 1u
#define SAMP_VEHICLE_COMPAT_CREATE_INTERVAL_MS 100u
#define SAMP_VEHICLE_COMPAT_POST_TELEPORT_HOLD_MS 1500u
#define SAMP_VEHICLE_COMPAT_MODEL_MIN 400
#define SAMP_VEHICLE_COMPAT_MODEL_MAX 611
#define SAMP_VEHICLE_COMPAT_TRAIN_PASSENGER_LOCO 538
#define SAMP_VEHICLE_COMPAT_TRAIN_FREIGHT_LOCO 537
#define SAMP_VEHICLE_COMPAT_TRAIN_PASSENGER 570
#define SAMP_VEHICLE_COMPAT_TRAIN_FREIGHT 569
#define SAMP_VEHICLE_COMPAT_TRAIN_TRAM 449
#define SAMP_GTA_MODEL_INFO_COUNT 20000u
#define SAMP_RAKNET_RPC_FLAG_GAME_STATE_MASK                                                                      \
  (SAMP_RAKNET_RPC_FLAG_PLAYER_POS | SAMP_RAKNET_RPC_FLAG_PLAYER_FACING | SAMP_RAKNET_RPC_FLAG_WEATHER |          \
   SAMP_RAKNET_RPC_FLAG_WORLD_TIME | SAMP_RAKNET_RPC_FLAG_SET_TIME_EX | SAMP_RAKNET_RPC_FLAG_TOGGLE_CLOCK |        \
   SAMP_RAKNET_RPC_FLAG_PLAYER_HEALTH | SAMP_RAKNET_RPC_FLAG_PLAYER_CONTROLLABLE |                                 \
   SAMP_RAKNET_RPC_FLAG_CAMERA_BEHIND | SAMP_RAKNET_RPC_FLAG_PLAYER_SCRIPT_EVENT |                                  \
   SAMP_RAKNET_RPC_FLAG_WORLD_VISUAL_EVENT | SAMP_RAKNET_RPC_FLAG_INTERIOR | SAMP_RAKNET_RPC_FLAG_CAMERA_POS |      \
   SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT)

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

typedef enum samp_gta_version {
  SAMP_GTA_VERSION_UNKNOWN = 0,
  SAMP_GTA_VERSION_USA10 = 1,
  SAMP_GTA_VERSION_EU10 = 2
} samp_gta_version;

typedef struct samp_gta_vector {
  float x;
  float y;
  float z;
} samp_gta_vector;

typedef struct samp_rw_video_mode {
  int32_t width;
  int32_t height;
  int32_t depth;
  int32_t flags;
  int32_t ref_rate;
  int32_t format;
} samp_rw_video_mode;

typedef struct samp_display_mode_request {
  int width;
  int height;
  int depth;
  int adapter;
} samp_display_mode_request;

typedef struct samp_d3d9_display_mode_compat {
  uint32_t width;
  uint32_t height;
  uint32_t refresh_rate;
  uint32_t format;
  int32_t flags;
} samp_d3d9_display_mode_compat;

typedef struct samp_d3drect_compat {
  LONG x1;
  LONG y1;
  LONG x2;
  LONG y2;
} samp_d3drect_compat;

typedef struct samp_d3d9_overlay_vertex_compat {
  float x;
  float y;
  float z;
  float rhw;
  DWORD color;
} samp_d3d9_overlay_vertex_compat;

typedef struct samp_d3d9_textured_vertex_compat {
  float x;
  float y;
  float z;
  float rhw;
  DWORD color;
  float u;
  float v;
} samp_d3d9_textured_vertex_compat;

typedef void(__cdecl *samp_script_process_fn)(void);
typedef HANDLE(WINAPI *samp_create_file_a_fn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL(WINAPI *samp_read_file_fn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD(WINAPI *samp_get_file_size_fn)(HANDLE, LPDWORD);
typedef DWORD(WINAPI *samp_set_file_pointer_fn)(HANDLE, LONG, PLONG, DWORD);
typedef BOOL(WINAPI *samp_close_handle_fn)(HANDLE);
typedef DWORD(WINAPI *samp_get_file_type_fn)(HANDLE);
typedef int(WINAPI *samp_show_cursor_fn)(BOOL);
typedef HRESULT(WINAPI *samp_d3d9_end_scene_fn)(void *);
typedef HRESULT(WINAPI *samp_d3d9_clear_fn)(void *, DWORD, const void *, DWORD, DWORD, float, DWORD);
typedef HRESULT(WINAPI *samp_d3d9_set_render_state_fn)(void *, DWORD, DWORD);
typedef HRESULT(WINAPI *samp_d3d9_set_texture_fn)(void *, DWORD, void *);
typedef HRESULT(WINAPI *samp_d3d9_set_texture_stage_state_fn)(void *, DWORD, DWORD, DWORD);
typedef HRESULT(WINAPI *samp_d3d9_set_sampler_state_fn)(void *, DWORD, DWORD, DWORD);
typedef HRESULT(WINAPI *samp_d3d9_set_fvf_fn)(void *, DWORD);
typedef HRESULT(WINAPI *samp_d3d9_draw_primitive_up_fn)(void *, DWORD, unsigned int, const void *, unsigned int);
typedef HRESULT(WINAPI *samp_d3d9_get_front_buffer_data_fn)(void *, UINT, void *);
typedef HRESULT(WINAPI *samp_d3d9_create_offscreen_plain_surface_fn)(void *, UINT, UINT, DWORD, DWORD, void **, void *);
typedef ULONG(WINAPI *samp_unknown_release_fn)(void *);
typedef int(__cdecl *samp_rw_get_num_subsystems_fn)(void);
typedef int(__cdecl *samp_rw_set_subsystem_fn)(int);
typedef int(__cdecl *samp_rw_get_num_video_modes_fn)(void);
typedef samp_rw_video_mode *(__cdecl *samp_rw_get_video_mode_info_fn)(samp_rw_video_mode *, unsigned int);
typedef void(__cdecl *samp_gta_font_set_scale_fn)(float, float);
typedef void(__cdecl *samp_gta_font_set_color_fn)(uint32_t);
typedef void(__cdecl *samp_gta_font_set_int_fn)(int);
typedef void(__cdecl *samp_gta_font_set_float_fn)(float);
typedef void(__cdecl *samp_gta_font_use_box_fn)(int, int);
typedef void(__cdecl *samp_gta_font_print_string_fn)(float, float, const char *);

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
typedef HRESULT(WINAPI *samp_d3dx_save_surface_to_file_a_fn)(LPCSTR, DWORD, void *, const void *, const RECT *);
typedef HRESULT(WINAPI *samp_d3dx_create_texture_from_file_a_fn)(void *, LPCSTR, void **);

typedef struct samp_dialog_layout_compat {
  int panel_x;
  int panel_y;
  int panel_w;
  int panel_h;
  int body_x;
  int body_y;
  int body_w;
  int body_h;
  int button1_x;
  int button1_y;
  int button1_w;
  int button1_h;
  int button2_x;
  int button2_y;
  int button2_w;
  int button2_h;
} samp_dialog_layout_compat;

typedef struct samp_textdraw_slot_compat {
  LONG active;
  uint32_t seq;
  samp_raknet_textdraw_transmit transmit;
  char text[SAMP_TEXTDRAW_COMPAT_MAX_TEXT_BYTES];
} samp_textdraw_slot_compat;

typedef struct samp_scoreboard_player_compat {
  LONG active;
  uint16_t player_id;
  uint8_t is_npc;
  int32_t score;
  uint32_t ping;
  uint32_t color;
  char name[SAMP_RAKNET_PLAYER_NAME_BYTES];
} samp_scoreboard_player_compat;

typedef struct samp_object_slot_compat {
  LONG active;
  LONG pending;
  LONG blocked_logged;
  uint32_t seq;
  int32_t model;
  uint32_t gta_id;
  uint8_t materials_count;
  float pos[3];
  float rot[3];
  float move_speed;
} samp_object_slot_compat;

typedef struct samp_vehicle_slot_compat {
  LONG active;
  LONG pending;
  LONG blocked_logged;
  uint32_t seq;
  int32_t model;
  uint32_t gta_id;
  uint32_t marker_id;
  uint8_t color1;
  uint8_t color2;
  uint8_t interior;
  uint8_t paintjob;
  uint8_t light_damage;
  uint8_t tyre_damage;
  uint8_t siren;
  uint8_t mods[SAMP_RAKNET_VEHICLE_MOD_SLOTS];
  float pos[3];
  float rotation;
  float health;
  uint32_t door_damage;
  uint32_t panel_damage;
  int32_t body_color1;
  int32_t body_color2;
} samp_vehicle_slot_compat;

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
  LONG raknet_pump_saturated_count;
  LONG raknet_join_sent;
  LONG raknet_logon_marked;
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
  LONG preconnect_pause_logged;
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
  LONG chat_input_active;
  LONG chat_input_hooked;
  LONG chat_input_len;
  LONG chat_input_history_count;
  LONG chat_input_history_index;
  LONG chat_input_send_count;
  LONG chat_input_command_send_count;
  LONG chat_input_send_failures;
  LONG chat_input_hook_logged;
  LONG scoreboard_offset;
  LONG scoreboard_logged;
  LONG scoreboard_d3d_font_fail_logged;
  LONG scoreboard_player_pool_event_seq;
  LONG scoreboard_score_ping_seq;
  LONG scoreboard_player_count;
  LONG scoreboard_local_player_id_valid;
  LONG scoreboard_local_player_id;
  LONG mp_session_apply_count;
  LONG mp_session_teleport_count;
  LONG mp_session_applied_player_pos_seq;
  LONG mp_session_applied_player_facing_seq;
  LONG mp_session_applied_player_health_seq;
  LONG mp_session_applied_player_controllable_seq;
  LONG mp_session_applied_camera_behind_seq;
  LONG mp_session_applied_player_armour_seq;
  LONG mp_session_applied_player_armed_weapon_seq;
  LONG mp_session_applied_reset_player_weapons_seq;
  LONG mp_session_applied_reset_player_money_seq;
  LONG mp_session_applied_play_sound_seq;
  LONG mp_session_applied_stop_audio_stream_seq;
  LONG mp_session_applied_player_color_seq;
  LONG mp_session_applied_player_team_seq;
  LONG mp_session_applied_apply_animation_seq;
  LONG mp_session_observed_world_visual_seq;
  LONG mp_session_script_failures;
  LONG mp_session_spawn_finalized;
  LONG mp_session_finalized_spawn_seq;
  LONG mp_session_frontend_hold_logged;
  LONG time_passing_patch_applied;
  LONG script_process_suppressed_logged;
  LONG script_process_allowed_after_spawn_logged;
  LONG gta_version;
  LONG preconnect_anim_wait_logged;
  LONG preconnect_clump_wait_logged;
  LONG preconnect_ped_stationary_logged;
  LONG preconnect_game_load_kicked;
  LONG preconnect_loaded_state_logged;
  LONG preconnect_scene_loaded;
  LONG mp_session_scene_loaded;
  LONG raknet_time_apply_logged;
  LONG local_stream_refresh_logged;
  LONG select_device_hook_attempted;
  LONG select_device_hook_installed;
  LONG select_device_video_mode_applied;
  LONG resolution_unlock_patches_applied;
  LONG default_resolution_patch_applied;

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
  DWORD preconnect_delay_active_ms;
  DWORD preconnect_delay_last_tick;
  uint8_t time_passing_saved_byte;
  uint8_t raknet_weather;
  uint8_t raknet_world_hour;
  uint8_t raknet_world_minute;
  uint8_t raknet_world_time_valid;
  uint8_t raknet_clock_enabled;
  uint8_t raknet_hold_time;
  uint8_t raknet_interior;
  uint8_t raknet_spawn_team;
  uint8_t raknet_camera_look_at_type;
  uint8_t raknet_player_controllable;
  uint8_t raknet_player_team;
  uint8_t raknet_apply_animation_loop;
  uint8_t raknet_apply_animation_lock_x;
  uint8_t raknet_apply_animation_lock_y;
  uint8_t raknet_apply_animation_freeze;
  uint8_t raknet_world_visual_event_type;
  uint8_t raknet_init_world_time;
  uint8_t raknet_init_weather;
  uint8_t raknet_init_lan_mode;
  uint8_t raknet_init_disable_enter_exits;
  uint8_t raknet_init_stunt_bonus;
  uint16_t raknet_init_spawns_available;
  uint16_t raknet_init_local_player_id;
  uint16_t raknet_player_color_player_id;
  uint16_t raknet_player_team_player_id;
  uint16_t raknet_apply_animation_player_id;
  uint16_t raknet_world_visual_id;
  int32_t raknet_spawn_skin;
  int32_t raknet_init_death_drop_money;
  int32_t raknet_apply_animation_time;
  int32_t raknet_world_visual_model;
  DWORD script_gate_old_protect;
  DWORD net_mgr_last_connect_attempt_tick;
  DWORD onfoot_sync_last_tick;
  DWORD local_stream_refresh_last_tick;
  void *bootstrap_manager_block;
  uintptr_t game_process_hook_stub;
  uintptr_t select_device_hook_stub;
  uintptr_t script_hook_original_target;
  uint8_t game_process_hook_saved_code[6];
  uint8_t select_device_hook_saved_code[SAMP_SELECT_DEVICE_HOOK_SIZE];
  uint8_t game_process_hook_saved_storage[4];
  uint8_t script_hook_saved_disp[4];
  uint8_t script_thread[SAMP_SCRIPT_THREAD_BYTES];
  uint8_t script_buf[SAMP_SCRIPT_BUF_BYTES];
  float raknet_player_pos[3];
  float local_stream_refresh_pos[3];
  float raknet_player_facing_angle;
  float raknet_player_health;
  float raknet_player_armour;
  float raknet_apply_animation_delta;
  LONG raknet_player_pos_seq;
  LONG raknet_player_facing_seq;
  LONG raknet_player_health_seq;
  LONG raknet_player_controllable_seq;
  LONG raknet_camera_behind_seq;
  LONG raknet_player_armour_seq;
  LONG raknet_player_armed_weapon_seq;
  LONG raknet_reset_player_weapons_seq;
  LONG raknet_reset_player_money_seq;
  LONG raknet_play_sound_seq;
  LONG raknet_stop_audio_stream_seq;
  LONG raknet_player_color_seq;
  LONG raknet_player_team_seq;
  LONG raknet_apply_animation_seq;
  LONG raknet_world_visual_event_seq;
  LONG raknet_spawn_info_seq;
  uint32_t raknet_player_armed_weapon;
  uint32_t raknet_play_sound_id;
  uint32_t raknet_player_color;
  uint32_t raknet_world_visual_color;
  float raknet_camera_pos[3];
  float raknet_camera_look_at[3];
  float raknet_play_sound_pos[3];
  float raknet_world_visual_pos[4];
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
  char raknet_apply_animation_lib[SAMP_RAKNET_ANIM_LIB_BYTES];
  char raknet_apply_animation_name[SAMP_RAKNET_ANIM_NAME_BYTES];
  char raknet_world_visual_text[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES];
  HFONT chat_overlay_font;
  DWORD chat_overlay_colors[SAMP_CHAT_COMPAT_MAX_LINES];
  char chat_overlay_lines[SAMP_CHAT_COMPAT_MAX_LINES][SAMP_CHAT_COMPAT_LINE_BYTES];
  HWND chat_input_hwnd;
  WNDPROC chat_input_old_wndproc;
  char chat_input_buffer[SAMP_CHAT_INPUT_MAX + 1];
  char chat_input_history[SAMP_CHAT_INPUT_HISTORY][SAMP_CHAT_INPUT_MAX + 1];
  char chat_input_recall_saved[SAMP_CHAT_INPUT_MAX + 1];
  LONG dialog_overlay_active;
  LONG dialog_overlay_id;
  LONG dialog_overlay_style;
  LONG dialog_overlay_selected;
  LONG dialog_overlay_scroll;
  LONG dialog_overlay_input_len;
  LONG dialog_overlay_logged;
  LONG dialog_overlay_response_count;
  LONG dialog_mouse_mode;
  LONG dialog_mouse_down;
  LONG dialog_mouse_x;
  LONG dialog_mouse_y;
  LONG dialog_mouse_raw_x;
  LONG dialog_mouse_raw_y;
  LONG dialog_mouse_logged;
  LONG screenshot_requested;
  LONG screenshot_count;
  LONG screenshot_fail_logged;
  LONG textdraw_event_seq;
  LONG textdraw_active_count;
  LONG textdraw_logged;
  LONG textdraw_d3d_font_fail_logged;
  LONG textdraw_gta_font_fail_logged;
  LONG textdraw_select_active;
  LONG textdraw_mouse_down;
  LONG object_event_seq;
  LONG object_active_count;
  LONG object_pending_count;
  LONG object_logged;
  LONG object_skip_chat_count;
  DWORD object_backpressure_last_tick;
  LONG vehicle_event_seq;
  LONG vehicle_active_count;
  LONG vehicle_pending_count;
  LONG vehicle_logged;
  DWORD vehicle_create_last_tick;
  DWORD vehicle_create_hold_until_tick;
  LONG onfoot_sync_send_count;
  LONG onfoot_sync_failures;
  LONG onfoot_sync_logged;
  LONG loading_screen_logged;
  LONG loading_screen_fail_logged;
  DWORD textdraw_select_color;
  char dialog_overlay_title[SAMP_RAKNET_DIALOG_TITLE_BYTES];
  char dialog_overlay_info[SAMP_RAKNET_DIALOG_INFO_BYTES];
  char dialog_overlay_button1[SAMP_RAKNET_DIALOG_BUTTON_BYTES];
  char dialog_overlay_button2[SAMP_RAKNET_DIALOG_BUTTON_BYTES];
  char dialog_overlay_input[SAMP_RAKNET_DIALOG_INPUT_BYTES];
  void *chat_d3d_device;
  void *scoreboard_d3d_device;
  void **chat_d3d_vtbl;
  samp_d3d9_end_scene_fn chat_end_scene_original;
  samp_d3dx_create_font_a_fn d3dx_create_font_a;
  samp_d3dx_save_surface_to_file_a_fn d3dx_save_surface_to_file_a;
  samp_id3dx_font_compat *chat_d3dx_font;
  samp_id3dx_font_compat *scoreboard_d3dx_font;
  samp_id3dx_font_compat *textdraw_d3dx_fonts[SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS];
  samp_d3dx_create_texture_from_file_a_fn d3dx_create_texture_from_file_a;
  void *loading_screen_texture;
  void *loading_screen_device;
  void *textdraw_d3d_device;
  samp_textdraw_slot_compat textdraw_slots[SAMP_RAKNET_MAX_TEXTDRAWS];
  samp_scoreboard_player_compat scoreboard_players[SAMP_SCOREBOARD_MAX_PLAYERS];
  samp_object_slot_compat object_slots[SAMP_RAKNET_MAX_OBJECTS];
  DWORD object_destroy_ticks[SAMP_RAKNET_MAX_OBJECTS];
  samp_vehicle_slot_compat vehicle_slots[SAMP_RAKNET_MAX_VEHICLES];
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
static uint8_t g_scan_list_memory[SAMP_SCANLIST_SIZE];
static LONG read_game_entry_gate_value(void);
static HRESULT WINAPI chat_compat_end_scene_hook(void *device);
static void chat_compat_viewport_rect(int *out_x, int *out_y, int *out_w, int *out_h);
static int scoreboard_compat_active(void);
static int scoreboard_compat_draw_d3dx_overlay(void *device);
static int scoreboard_compat_handle_key(UINT msg, WPARAM wparam);
static void scoreboard_compat_release_font(void);
static void scoreboard_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot);
static void dialog_compat_normalize_selection(void);
static void dialog_compat_submit(unsigned char button);
static void textdraw_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot);
static int textdraw_compat_draw_d3dx_overlay(void *device);
static int textdraw_compat_is_preview_like(const samp_textdraw_slot_compat *slot);
static int textdraw_compat_has_box(const samp_textdraw_slot_compat *slot);
static int textdraw_compat_handle_mouse(HWND hwnd, UINT msg, LPARAM lparam);
static int textdraw_compat_submit_click(uint16_t textdraw_id);
static void textdraw_compat_clear_select_mode(const char *reason);
static void textdraw_compat_release_fonts(void);
static void object_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot);
static void object_compat_reset_pool(const char *reason);
static void vehicle_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot);
static void vehicle_compat_reset_pool(const char *reason);
static int loading_screen_compat_active(void);
static int loading_screen_compat_draw_d3dx_overlay(void *device);
static void loading_screen_compat_release_texture(void);
static int gta_script_command_compat(uint16_t opcode, const char *params, ...);
static int memory_is_readable_compat(const void *ptr, size_t size);
static uintptr_t gta_find_player_ped_compat(void);
static int gta_reset_ped_audio_attributes_compat(uintptr_t ped, const char *reason);
static int gta_entity_read_position_compat(uintptr_t entity, float *x, float *y, float *z);
static void gta_streaming_request_model_compat(int32_t model_id, int32_t flags);
static void gta_streaming_load_all_requested_compat(int only_priority_requests);
static int screenshot_compat_resolve_d3dx_save_surface(void);
static LRESULT CALLBACK chat_input_wndproc_compat(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
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

static int chat_compat_hex_value(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  return -1;
}

static int chat_compat_parse_color_tag(const char *text, DWORD *out_argb) {
  DWORD rgb = 0u;
  int i = 0;

  if (text == NULL || out_argb == NULL || strlen(text) < 8u || text[0] != '{') {
    return 0;
  }
  for (i = 1; i <= 6; ++i) {
    int value = chat_compat_hex_value(text[i]);
    if (value < 0) {
      return 0;
    }
    rgb = (rgb << 4u) | (DWORD)value;
  }
  if (text[7] != '}') {
    return 0;
  }
  *out_argb = 0xFF000000u | rgb;
  return 1;
}

static void chat_compat_strip_samp_color_tags(char *line) {
  char *read_cursor = line;
  char *write_cursor = line;

  if (line == NULL) {
    return;
  }

  while (*read_cursor != '\0') {
    DWORD ignored_color = 0u;
    if (chat_compat_parse_color_tag(read_cursor, &ignored_color)) {
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

static int chat_input_enabled_compat(void) {
  static LONG initialized = 0;
  static int enabled = 1;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_CHAT_INPUT");
  if (value != NULL && *value != '\0' && value[0] == '0') {
    enabled = 0;
  }
  InterlockedExchange(&initialized, 1);
  return enabled;
}

static LRESULT chat_input_call_original_compat(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  WNDPROC old_proc = g_runtime.chat_input_old_wndproc;

  if (old_proc != NULL && old_proc != chat_input_wndproc_compat) {
    return CallWindowProcA(old_proc, hwnd, msg, wparam, lparam);
  }
  return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static void chat_input_close_compat(void) {
  if (InterlockedExchange(&g_runtime.chat_input_active, 0) != 0) {
    runtime_tracef("chat_input: closed");
  }
  InterlockedExchange(&g_runtime.chat_input_len, 0);
  InterlockedExchange(&g_runtime.chat_input_history_index, -1);
  g_runtime.chat_input_buffer[0] = '\0';
  g_runtime.chat_input_recall_saved[0] = '\0';
}

static void chat_input_open_compat(void) {
  if (!chat_input_enabled_compat() || !g_runtime.settings.play_online) {
    return;
  }

  g_runtime.chat_input_buffer[0] = '\0';
  g_runtime.chat_input_recall_saved[0] = '\0';
  InterlockedExchange(&g_runtime.chat_input_len, 0);
  InterlockedExchange(&g_runtime.chat_input_history_index, -1);
  if (InterlockedExchange(&g_runtime.chat_input_active, 1) == 0) {
    runtime_tracef("chat_input: opened");
  }
}

static void chat_input_append_char_compat(char ch) {
  LONG len = InterlockedCompareExchange(&g_runtime.chat_input_len, 0, 0);

  if (len < 0) {
    len = 0;
  }
  if (len >= SAMP_CHAT_INPUT_MAX) {
    return;
  }

  g_runtime.chat_input_buffer[len] = ch;
  g_runtime.chat_input_buffer[len + 1] = '\0';
  InterlockedExchange(&g_runtime.chat_input_len, len + 1);
  InterlockedExchange(&g_runtime.chat_input_history_index, -1);
}

static void chat_input_backspace_compat(void) {
  LONG len = InterlockedCompareExchange(&g_runtime.chat_input_len, 0, 0);

  if (len <= 0) {
    return;
  }

  --len;
  g_runtime.chat_input_buffer[len] = '\0';
  InterlockedExchange(&g_runtime.chat_input_len, len);
  InterlockedExchange(&g_runtime.chat_input_history_index, -1);
}

static void chat_input_set_buffer_compat(const char *text) {
  size_t len = 0u;

  if (text == NULL) {
    text = "";
  }

  strncpy(g_runtime.chat_input_buffer, text, SAMP_CHAT_INPUT_MAX);
  g_runtime.chat_input_buffer[SAMP_CHAT_INPUT_MAX] = '\0';
  len = strlen(g_runtime.chat_input_buffer);
  if (len > SAMP_CHAT_INPUT_MAX) {
    len = SAMP_CHAT_INPUT_MAX;
  }
  InterlockedExchange(&g_runtime.chat_input_len, (LONG)len);
}

static void chat_input_add_history_compat(const char *line) {
  LONG count = InterlockedCompareExchange(&g_runtime.chat_input_history_count, 0, 0);
  int i = 0;

  if (line == NULL || line[0] == '\0') {
    return;
  }
  if (count > 0 && count <= SAMP_CHAT_INPUT_HISTORY && strcmp(g_runtime.chat_input_history[0], line) == 0) {
    return;
  }
  if (count < 0 || count > SAMP_CHAT_INPUT_HISTORY) {
    count = SAMP_CHAT_INPUT_HISTORY;
  }
  if (count < SAMP_CHAT_INPUT_HISTORY) {
    ++count;
  }

  for (i = (int)count - 1; i > 0; --i) {
    memcpy(g_runtime.chat_input_history[i], g_runtime.chat_input_history[i - 1], SAMP_CHAT_INPUT_MAX + 1);
  }
  strncpy(g_runtime.chat_input_history[0], line, SAMP_CHAT_INPUT_MAX);
  g_runtime.chat_input_history[0][SAMP_CHAT_INPUT_MAX] = '\0';
  InterlockedExchange(&g_runtime.chat_input_history_count, count);
}

static void chat_input_recall_up_compat(void) {
  LONG count = InterlockedCompareExchange(&g_runtime.chat_input_history_count, 0, 0);
  LONG index = InterlockedCompareExchange(&g_runtime.chat_input_history_index, 0, 0);

  if (count <= 0) {
    return;
  }
  if (count > SAMP_CHAT_INPUT_HISTORY) {
    count = SAMP_CHAT_INPUT_HISTORY;
  }

  if (index < 0) {
    strncpy(g_runtime.chat_input_recall_saved, g_runtime.chat_input_buffer, SAMP_CHAT_INPUT_MAX);
    g_runtime.chat_input_recall_saved[SAMP_CHAT_INPUT_MAX] = '\0';
    index = 0;
  } else if (index + 1 < count) {
    ++index;
  }

  chat_input_set_buffer_compat(g_runtime.chat_input_history[index]);
  InterlockedExchange(&g_runtime.chat_input_history_index, index);
}

static void chat_input_recall_down_compat(void) {
  LONG index = InterlockedCompareExchange(&g_runtime.chat_input_history_index, 0, 0);

  if (index < 0) {
    return;
  }
  if (index > 0) {
    --index;
    chat_input_set_buffer_compat(g_runtime.chat_input_history[index]);
    InterlockedExchange(&g_runtime.chat_input_history_index, index);
    return;
  }

  chat_input_set_buffer_compat(g_runtime.chat_input_recall_saved);
  InterlockedExchange(&g_runtime.chat_input_history_index, -1);
}

static int chat_input_ascii_ieq_compat(char left, char right) {
  if (left >= 'A' && left <= 'Z') {
    left = (char)(left - 'A' + 'a');
  }
  if (right >= 'A' && right <= 'Z') {
    right = (char)(right - 'A' + 'a');
  }
  return left == right;
}

static int chat_input_only_spaces_compat(const char *text) {
  if (text == NULL) {
    return 1;
  }
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  return *text == '\0';
}

static int chat_input_is_local_quit_command_compat(const char *line) {
  const char *cursor = line;

  if (cursor == NULL) {
    return 0;
  }
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }
  if (*cursor != '/') {
    return 0;
  }
  ++cursor;
  if (chat_input_ascii_ieq_compat(cursor[0], 'q') && chat_input_only_spaces_compat(cursor + 1)) {
    return 1;
  }
  if (chat_input_ascii_ieq_compat(cursor[0], 'q') && chat_input_ascii_ieq_compat(cursor[1], 'u') &&
      chat_input_ascii_ieq_compat(cursor[2], 'i') && chat_input_ascii_ieq_compat(cursor[3], 't') &&
      chat_input_only_spaces_compat(cursor + 4)) {
    return 1;
  }
  return 0;
}

static void chat_input_request_quit_compat(const char *line) {
  HWND hwnd = read_game_hwnd_compat();

  if (g_runtime.net_mgr.raknet_client != NULL) {
    samp_raknet_client_disconnect(g_runtime.net_mgr.raknet_client, 0U, 0U);
  }
  chat_compat_add_message("Exiting...");
  runtime_tracef("chat_input: local quit command hwnd=0x%08lx text='%s'", (unsigned long)(uintptr_t)hwnd,
                 line != NULL ? line : "");
  if (hwnd != NULL) {
    PostMessageA(hwnd, WM_CLOSE, 0, 0);
  }
  PostQuitMessage(0);
}

static void chat_input_submit_compat(void) {
  char line[SAMP_CHAT_INPUT_MAX + 1];
  LONG len = InterlockedCompareExchange(&g_runtime.chat_input_len, 0, 0);
  int result = -1;

  if (len < 0) {
    len = 0;
  }
  if (len > SAMP_CHAT_INPUT_MAX) {
    len = SAMP_CHAT_INPUT_MAX;
  }
  memcpy(line, g_runtime.chat_input_buffer, (size_t)len);
  line[len] = '\0';

  chat_input_close_compat();
  if (line[0] == '\0') {
    return;
  }

  chat_input_add_history_compat(line);
  if (chat_input_is_local_quit_command_compat(line)) {
    chat_input_request_quit_compat(line);
    return;
  }
  if (g_runtime.net_mgr.raknet_client == NULL || !samp_raknet_client_is_connected(g_runtime.net_mgr.raknet_client)) {
    chat_compat_add_message("Chat not connected.");
    InterlockedIncrement(&g_runtime.chat_input_send_failures);
    runtime_tracef("chat_input: send skipped not connected text='%s'", line);
    return;
  }

  if (line[0] == '/') {
    result = samp_raknet_client_send_server_command(g_runtime.net_mgr.raknet_client, line);
    if (result == 0) {
      InterlockedIncrement(&g_runtime.chat_input_command_send_count);
    }
  } else {
    result = samp_raknet_client_send_chat(g_runtime.net_mgr.raknet_client, line);
    if (result == 0) {
      InterlockedIncrement(&g_runtime.chat_input_send_count);
    }
  }

  if (result != 0) {
    InterlockedIncrement(&g_runtime.chat_input_send_failures);
    chat_compat_add_message("Chat send failed.");
  }
  runtime_tracef("chat_input: submitted command=%d len=%ld result=%d text='%s'", line[0] == '/' ? 1 : 0, (long)len,
                 result, line);
}

static void chat_input_format_draw_line_compat(char *out_line, size_t out_size) {
  char caret = '_';
  DWORD tick = GetTickCount();

  if (out_line == NULL || out_size == 0u) {
    return;
  }
  out_line[0] = '\0';
  if (InterlockedCompareExchange(&g_runtime.chat_input_active, 0, 0) == 0) {
    return;
  }

  if ((tick / 450u) % 2u != 0u) {
    caret = ' ';
  }
  (void)snprintf(out_line, out_size, "> %s%c", g_runtime.chat_input_buffer, caret);
  out_line[out_size - 1u] = '\0';
}

static int dialog_compat_active(void) {
  return InterlockedCompareExchange(&g_runtime.dialog_overlay_active, 0, 0) != 0;
}

static int dialog_compat_uses_list(LONG style) {
  return style == 2 || style == 4 || style == 5;
}

static int dialog_compat_uses_input(LONG style) {
  return style == 1 || style == 3;
}

static int dialog_compat_has_header(LONG style) {
  return style == 5;
}

static int dialog_compat_get_line(const char *text, int index, char *out_line, size_t out_size) {
  const char *cursor = text;
  const char *line_start = NULL;
  size_t len = 0u;
  int current = 0;

  if (out_line == NULL || out_size == 0u) {
    return 0;
  }
  out_line[0] = '\0';
  if (text == NULL || index < 0) {
    return 0;
  }

  while (*cursor != '\0') {
    line_start = cursor;
    while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r') {
      ++cursor;
    }
    if (current == index) {
      len = (size_t)(cursor - line_start);
      if (len >= out_size) {
        len = out_size - 1u;
      }
      memcpy(out_line, line_start, len);
      out_line[len] = '\0';
      chat_compat_filter_invalid_chars(out_line);
      chat_compat_strip_samp_color_tags(out_line);
      for (len = 0u; out_line[len] != '\0'; ++len) {
        if (out_line[len] == '\t') {
          out_line[len] = ' ';
        }
      }
      return 1;
    }
    while (*cursor == '\n' || *cursor == '\r') {
      ++cursor;
    }
    ++current;
  }
  return 0;
}

static int dialog_compat_line_count(const char *text) {
  const char *cursor = text;
  int count = 0;

  if (text == NULL || *text == '\0') {
    return 0;
  }
  while (*cursor != '\0') {
    ++count;
    while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r') {
      ++cursor;
    }
    while (*cursor == '\n' || *cursor == '\r') {
      ++cursor;
    }
  }
  return count;
}

static int dialog_compat_item_count(void) {
  LONG style = InterlockedCompareExchange(&g_runtime.dialog_overlay_style, 0, 0);
  int count = dialog_compat_line_count(g_runtime.dialog_overlay_info);

  if (dialog_compat_has_header(style) && count > 0) {
    --count;
  }
  return count < 0 ? 0 : count;
}

static void dialog_compat_set_mouse_mode(int enabled) {
  HWND hwnd = NULL;
  POINT cursor;
  int viewport_x = 0;
  int viewport_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  int i = 0;

  if (enabled) {
    if (InterlockedExchange(&g_runtime.dialog_mouse_mode, 1) != 0) {
      SetCursor(LoadCursorA(NULL, IDC_ARROW));
      return;
    }

    hwnd = read_game_hwnd_compat();
    if (hwnd != NULL && GetCursorPos(&cursor) && ScreenToClient(hwnd, &cursor)) {
      InterlockedExchange(&g_runtime.dialog_mouse_x, cursor.x);
      InterlockedExchange(&g_runtime.dialog_mouse_y, cursor.y);
      InterlockedExchange(&g_runtime.dialog_mouse_raw_x, cursor.x);
      InterlockedExchange(&g_runtime.dialog_mouse_raw_y, cursor.y);
    } else {
      chat_compat_viewport_rect(&viewport_x, &viewport_y, &viewport_w, &viewport_h);
      InterlockedExchange(&g_runtime.dialog_mouse_x, viewport_x + (viewport_w / 2));
      InterlockedExchange(&g_runtime.dialog_mouse_y, viewport_y + (viewport_h / 2));
      InterlockedExchange(&g_runtime.dialog_mouse_raw_x, viewport_x + (viewport_w / 2));
      InterlockedExchange(&g_runtime.dialog_mouse_raw_y, viewport_y + (viewport_h / 2));
    }

    ClipCursor(NULL);
    if (g_runtime.bootstrap_shims.show_cursor != NULL) {
      while (i++ < 8 && g_runtime.bootstrap_shims.show_cursor(TRUE) < 0) {
      }
    } else {
      while (i++ < 8 && ShowCursor(TRUE) < 0) {
      }
    }
    SetCursor(LoadCursorA(NULL, IDC_ARROW));
    if (InterlockedCompareExchange(&g_runtime.dialog_mouse_logged, 1, 0) == 0) {
      runtime_tracef("dialog_overlay: mouse mode enabled hwnd=0x%08lx", (unsigned long)(uintptr_t)hwnd);
    }
    return;
  }

  if (InterlockedExchange(&g_runtime.dialog_mouse_mode, 0) == 0) {
    return;
  }
  InterlockedExchange(&g_runtime.dialog_mouse_down, 0);
  ClipCursor(NULL);
  i = 0;
  if (g_runtime.bootstrap_shims.show_cursor != NULL) {
    while (i++ < 8 && g_runtime.bootstrap_shims.show_cursor(FALSE) >= 0) {
    }
  } else {
    while (i++ < 8 && ShowCursor(FALSE) >= 0) {
    }
  }
  runtime_tracef("dialog_overlay: mouse mode disabled");
}

static void dialog_compat_close(void) {
  if (InterlockedCompareExchange(&g_runtime.textdraw_select_active, 0, 0) == 0) {
    dialog_compat_set_mouse_mode(0);
  }
  InterlockedExchange(&g_runtime.dialog_overlay_active, 0);
  InterlockedExchange(&g_runtime.dialog_overlay_input_len, 0);
  g_runtime.dialog_overlay_input[0] = '\0';
}

static int dialog_compat_layout(samp_dialog_layout_compat *layout) {
  int viewport_x = 0;
  int viewport_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  int panel_w = 520;
  int panel_h = 292;

  if (layout == NULL) {
    return 0;
  }
  memset(layout, 0, sizeof(*layout));
  chat_compat_viewport_rect(&viewport_x, &viewport_y, &viewport_w, &viewport_h);
  if (viewport_w < panel_w + 40) {
    panel_w = viewport_w - 40;
  }
  if (viewport_h < panel_h + 40) {
    panel_h = viewport_h - 40;
  }
  if (panel_w < 320) {
    panel_w = 320;
  }
  if (panel_h < 220) {
    panel_h = 220;
  }

  layout->panel_w = panel_w;
  layout->panel_h = panel_h;
  layout->panel_x = viewport_x + ((viewport_w - panel_w) / 2);
  layout->panel_y = viewport_y + ((viewport_h - panel_h) / 2);
  layout->body_x = layout->panel_x + 14;
  layout->body_y = layout->panel_y + 42;
  layout->body_w = panel_w - 28;
  layout->body_h = panel_h - 92;
  layout->button1_y = layout->panel_y + panel_h - 42;
  layout->button1_h = 28;
  layout->button2_y = layout->button1_y;
  layout->button2_h = 28;

  if (g_runtime.dialog_overlay_button2[0] != '\0') {
    layout->button2_x = layout->panel_x + panel_w - 204;
    layout->button2_w = 88;
    layout->button1_x = layout->panel_x + panel_w - 102;
    layout->button1_w = 88;
  } else {
    layout->button1_x = layout->panel_x + ((panel_w - 96) / 2);
    layout->button1_w = 96;
    layout->button2_x = 0;
    layout->button2_w = 0;
  }
  return 1;
}

static int dialog_compat_point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
  return rw > 0 && rh > 0 && x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void dialog_compat_record_mouse(HWND hwnd, LPARAM lparam) {
  int raw_x = (short)LOWORD(lparam);
  int raw_y = (short)HIWORD(lparam);
  int last_x = (int)InterlockedCompareExchange(&g_runtime.dialog_mouse_raw_x, 0, 0);
  int last_y = (int)InterlockedCompareExchange(&g_runtime.dialog_mouse_raw_y, 0, 0);
  int virtual_x = (int)InterlockedCompareExchange(&g_runtime.dialog_mouse_x, 0, 0);
  int virtual_y = (int)InterlockedCompareExchange(&g_runtime.dialog_mouse_y, 0, 0);
  int viewport_x = 0;
  int viewport_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  int center_x = 0;
  int center_y = 0;
  int raw_near_center = 0;
  int last_near_center = 0;
  int dx = raw_x - last_x;
  int dy = raw_y - last_y;

  chat_compat_viewport_rect(&viewport_x, &viewport_y, &viewport_w, &viewport_h);
  center_x = viewport_x + (viewport_w / 2);
  center_y = viewport_y + (viewport_h / 2);
  raw_near_center = abs(raw_x - center_x) <= 3 && abs(raw_y - center_y) <= 3;
  last_near_center = abs(last_x - center_x) <= 3 && abs(last_y - center_y) <= 3;
  InterlockedExchange(&g_runtime.dialog_mouse_raw_x, raw_x);
  InterlockedExchange(&g_runtime.dialog_mouse_raw_y, raw_y);

  /* INFERRED: GTA/Wine recenters the hardware cursor during gameplay.
     Treat that warp back to screen center as bookkeeping, not user input. */
  if (raw_near_center && !last_near_center) {
    if (hwnd != NULL) {
      SetCursor(LoadCursorA(NULL, IDC_ARROW));
    }
    return;
  }
  if (dx != 0 || dy != 0) {
    virtual_x += dx;
    virtual_y += dy;
    if (viewport_w > 0) {
      if (virtual_x < viewport_x) {
        virtual_x = viewport_x;
      } else if (virtual_x >= viewport_x + viewport_w) {
        virtual_x = viewport_x + viewport_w - 1;
      }
    }
    if (viewport_h > 0) {
      if (virtual_y < viewport_y) {
        virtual_y = viewport_y;
      } else if (virtual_y >= viewport_y + viewport_h) {
        virtual_y = viewport_y + viewport_h - 1;
      }
    }
    InterlockedExchange(&g_runtime.dialog_mouse_x, virtual_x);
    InterlockedExchange(&g_runtime.dialog_mouse_y, virtual_y);
  }
  if (hwnd != NULL) {
    SetCursor(LoadCursorA(NULL, IDC_ARROW));
  }
}

static void dialog_compat_select_from_mouse(int x, int y, int submit) {
  LONG style = InterlockedCompareExchange(&g_runtime.dialog_overlay_style, 0, 0);
  LONG scroll = InterlockedCompareExchange(&g_runtime.dialog_overlay_scroll, 0, 0);
  samp_dialog_layout_compat layout;
  int line_y = 0;
  int row = 0;
  int item = 0;
  int item_count = 0;

  if (!dialog_compat_uses_list(style) || !dialog_compat_layout(&layout)) {
    return;
  }
  line_y = layout.body_y + 8;
  if (dialog_compat_has_header(style)) {
    line_y += 22;
  }
  if (!dialog_compat_point_in_rect(x, y, layout.body_x, line_y, layout.body_w, layout.body_h - (line_y - layout.body_y))) {
    return;
  }

  row = (y - line_y) / 22;
  if (row < 0 || row >= SAMP_DIALOG_COMPAT_MAX_VISIBLE_ITEMS) {
    return;
  }
  item = (int)scroll + row;
  item_count = dialog_compat_item_count();
  if (item < 0 || item >= item_count) {
    return;
  }
  InterlockedExchange(&g_runtime.dialog_overlay_selected, item);
  dialog_compat_normalize_selection();
  if (submit) {
    dialog_compat_submit(1u);
  }
}

static int dialog_compat_handle_mouse(HWND hwnd, UINT msg, LPARAM lparam) {
  LONG x = 0;
  LONG y = 0;
  samp_dialog_layout_compat layout;

  if (!dialog_compat_active()) {
    return 0;
  }

  dialog_compat_record_mouse(hwnd, lparam);
  x = InterlockedCompareExchange(&g_runtime.dialog_mouse_x, 0, 0);
  y = InterlockedCompareExchange(&g_runtime.dialog_mouse_y, 0, 0);

  if (msg == WM_MOUSEMOVE) {
    return 1;
  }
  if (msg == WM_LBUTTONDBLCLK) {
    dialog_compat_select_from_mouse((int)x, (int)y, 1);
    return 1;
  }
  if (msg == WM_LBUTTONDOWN) {
    InterlockedExchange(&g_runtime.dialog_mouse_down, 1);
    dialog_compat_select_from_mouse((int)x, (int)y, 0);
    return 1;
  }
  if (msg == WM_LBUTTONUP) {
    InterlockedExchange(&g_runtime.dialog_mouse_down, 0);
    if (!dialog_compat_layout(&layout)) {
      return 1;
    }
    if (dialog_compat_point_in_rect((int)x, (int)y, layout.button1_x, layout.button1_y, layout.button1_w,
                                    layout.button1_h)) {
      dialog_compat_submit(1u);
      return 1;
    }
    if (dialog_compat_point_in_rect((int)x, (int)y, layout.button2_x, layout.button2_y, layout.button2_w,
                                    layout.button2_h)) {
      dialog_compat_submit(0u);
      return 1;
    }
  }
  return 1;
}

static void screenshot_compat_request(void) {
  InterlockedExchange(&g_runtime.screenshot_requested, 1);
  runtime_tracef("screenshot: requested by F8");
}

static void dialog_compat_normalize_selection(void) {
  LONG selected = InterlockedCompareExchange(&g_runtime.dialog_overlay_selected, 0, 0);
  LONG scroll = InterlockedCompareExchange(&g_runtime.dialog_overlay_scroll, 0, 0);
  int count = dialog_compat_item_count();

  if (count <= 0) {
    selected = -1;
    scroll = 0;
  } else {
    if (selected < 0) {
      selected = 0;
    }
    if (selected >= count) {
      selected = count - 1;
    }
    if (scroll < 0) {
      scroll = 0;
    }
    if (selected < scroll) {
      scroll = selected;
    }
    if (selected >= scroll + SAMP_DIALOG_COMPAT_MAX_VISIBLE_ITEMS) {
      scroll = selected - SAMP_DIALOG_COMPAT_MAX_VISIBLE_ITEMS + 1;
    }
  }

  InterlockedExchange(&g_runtime.dialog_overlay_selected, selected);
  InterlockedExchange(&g_runtime.dialog_overlay_scroll, scroll);
}

static void dialog_compat_move_selection(int delta) {
  LONG selected = InterlockedCompareExchange(&g_runtime.dialog_overlay_selected, 0, 0);
  int count = dialog_compat_item_count();

  if (count <= 0) {
    return;
  }
  selected += delta;
  if (selected < 0) {
    selected = 0;
  }
  if (selected >= count) {
    selected = count - 1;
  }
  InterlockedExchange(&g_runtime.dialog_overlay_selected, selected);
  dialog_compat_normalize_selection();
}

static void dialog_compat_append_input_char(char ch) {
  LONG len = InterlockedCompareExchange(&g_runtime.dialog_overlay_input_len, 0, 0);

  if (len < 0) {
    len = 0;
  }
  if (len >= (LONG)(SAMP_RAKNET_DIALOG_INPUT_BYTES - 1u)) {
    return;
  }
  g_runtime.dialog_overlay_input[len] = ch;
  g_runtime.dialog_overlay_input[len + 1] = '\0';
  InterlockedExchange(&g_runtime.dialog_overlay_input_len, len + 1);
}

static void dialog_compat_backspace_input(void) {
  LONG len = InterlockedCompareExchange(&g_runtime.dialog_overlay_input_len, 0, 0);

  if (len <= 0) {
    return;
  }
  --len;
  g_runtime.dialog_overlay_input[len] = '\0';
  InterlockedExchange(&g_runtime.dialog_overlay_input_len, len);
}

static void dialog_compat_submit(unsigned char button) {
  LONG style = InterlockedCompareExchange(&g_runtime.dialog_overlay_style, 0, 0);
  LONG dialog_id = InterlockedCompareExchange(&g_runtime.dialog_overlay_id, 0, 0);
  LONG selected = InterlockedCompareExchange(&g_runtime.dialog_overlay_selected, 0, 0);
  int16_t listitem = -1;
  char input[SAMP_RAKNET_DIALOG_INPUT_BYTES];
  int result = -1;

  if (!dialog_compat_active() || dialog_id < 0 || dialog_id > 65535) {
    return;
  }
  input[0] = '\0';

  if (button != 0u && dialog_compat_uses_list(style)) {
    int line_index = (int)selected + (dialog_compat_has_header(style) ? 1 : 0);
    if (selected < 0 || !dialog_compat_get_line(g_runtime.dialog_overlay_info, line_index, input, sizeof(input))) {
      return;
    }
    listitem = (int16_t)selected;
  } else if (button != 0u && dialog_compat_uses_input(style)) {
    strncpy(input, g_runtime.dialog_overlay_input, sizeof(input) - 1u);
    input[sizeof(input) - 1u] = '\0';
  }

  if (g_runtime.net_mgr.raknet_client != NULL) {
    result = samp_raknet_client_queue_dialog_response(g_runtime.net_mgr.raknet_client, (uint16_t)dialog_id, button,
                                                      listitem, input);
  }
  if (result == 0) {
    InterlockedIncrement(&g_runtime.dialog_overlay_response_count);
    dialog_compat_close();
  }
  runtime_tracef("dialog_overlay: submit dialog=%ld button=%u listitem=%ld input='%s' result=%d", (long)dialog_id,
                 (unsigned)button, (long)listitem, input, result);
}

static void dialog_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot) {
  LONG current_id = InterlockedCompareExchange(&g_runtime.dialog_overlay_id, 0, 0);
  int new_dialog = 0;

  if (snapshot == NULL || (snapshot->flags & SAMP_RAKNET_RPC_FLAG_DIALOG) == 0u) {
    if (dialog_compat_active()) {
      runtime_tracef("dialog_overlay: closed by rpc state");
    }
    dialog_compat_close();
    return;
  }

  new_dialog = !dialog_compat_active() || current_id != (LONG)snapshot->last_dialog_id;
  InterlockedExchange(&g_runtime.dialog_overlay_active, 1);
  dialog_compat_set_mouse_mode(1);
  InterlockedExchange(&g_runtime.dialog_overlay_id, (LONG)snapshot->last_dialog_id);
  InterlockedExchange(&g_runtime.dialog_overlay_style, (LONG)snapshot->last_dialog_style);
  strncpy(g_runtime.dialog_overlay_title, snapshot->dialog_title, sizeof(g_runtime.dialog_overlay_title) - 1u);
  g_runtime.dialog_overlay_title[sizeof(g_runtime.dialog_overlay_title) - 1u] = '\0';
  strncpy(g_runtime.dialog_overlay_info, snapshot->dialog_info, sizeof(g_runtime.dialog_overlay_info) - 1u);
  g_runtime.dialog_overlay_info[sizeof(g_runtime.dialog_overlay_info) - 1u] = '\0';
  strncpy(g_runtime.dialog_overlay_button1, snapshot->dialog_button1, sizeof(g_runtime.dialog_overlay_button1) - 1u);
  g_runtime.dialog_overlay_button1[sizeof(g_runtime.dialog_overlay_button1) - 1u] = '\0';
  strncpy(g_runtime.dialog_overlay_button2, snapshot->dialog_button2, sizeof(g_runtime.dialog_overlay_button2) - 1u);
  g_runtime.dialog_overlay_button2[sizeof(g_runtime.dialog_overlay_button2) - 1u] = '\0';
  if (g_runtime.dialog_overlay_title[0] == '\0') {
    strcpy(g_runtime.dialog_overlay_title, "SA-MP Dialog");
  }
  if (g_runtime.dialog_overlay_button1[0] == '\0') {
    strcpy(g_runtime.dialog_overlay_button1, "OK");
  }

  if (new_dialog) {
    chat_input_close_compat();
    InterlockedExchange(&g_runtime.dialog_overlay_selected, dialog_compat_uses_list(snapshot->last_dialog_style) ? 0 : -1);
    InterlockedExchange(&g_runtime.dialog_overlay_scroll, 0);
    InterlockedExchange(&g_runtime.dialog_overlay_input_len, 0);
    InterlockedExchange(&g_runtime.dialog_overlay_logged, 0);
    g_runtime.dialog_overlay_input[0] = '\0';
  }
  dialog_compat_normalize_selection();

  if (InterlockedCompareExchange(&g_runtime.dialog_overlay_logged, 1, 0) == 0) {
    runtime_tracef("dialog_overlay: active id=%u style=%u title='%s' items=%d button1='%s' button2='%s'",
                   (unsigned)snapshot->last_dialog_id, (unsigned)snapshot->last_dialog_style,
                   g_runtime.dialog_overlay_title, dialog_compat_item_count(), g_runtime.dialog_overlay_button1,
                   g_runtime.dialog_overlay_button2);
  }
}

static void textdraw_compat_hide_slot(uint16_t textdraw_id) {
  samp_textdraw_slot_compat *slot = NULL;

  if (textdraw_id >= SAMP_RAKNET_MAX_TEXTDRAWS) {
    return;
  }
  slot = &g_runtime.textdraw_slots[textdraw_id];
  if (InterlockedExchange(&slot->active, 0) != 0) {
    LONG count = InterlockedDecrement(&g_runtime.textdraw_active_count);
    if (count < 0) {
      InterlockedExchange(&g_runtime.textdraw_active_count, 0);
    }
  }
  slot->text[0] = '\0';
}

static void textdraw_compat_apply_event(const samp_raknet_textdraw_event *event) {
  samp_textdraw_slot_compat *slot = NULL;

  if (event == NULL || event->seq == 0u || event->textdraw_id >= SAMP_RAKNET_MAX_TEXTDRAWS) {
    return;
  }

  if (event->action == SAMP_RAKNET_TEXTDRAW_ACTION_HIDE) {
    textdraw_compat_hide_slot(event->textdraw_id);
    runtime_tracef("textdraw: hide seq=%lu id=%u", (unsigned long)event->seq, (unsigned)event->textdraw_id);
    return;
  }

  slot = &g_runtime.textdraw_slots[event->textdraw_id];
  if (event->action == SAMP_RAKNET_TEXTDRAW_ACTION_SHOW) {
    if (InterlockedExchange(&slot->active, 1) == 0) {
      InterlockedIncrement(&g_runtime.textdraw_active_count);
    }
    slot->seq = event->seq;
    memcpy(&slot->transmit, &event->transmit, sizeof(slot->transmit));
    strncpy(slot->text, event->text, sizeof(slot->text) - 1u);
    slot->text[sizeof(slot->text) - 1u] = '\0';
    runtime_tracef("textdraw: show seq=%lu id=%u pos=(%.2f,%.2f) size=(%.2f,%.2f) style=%u flags=0x%02x selectable=%u model=%u zoom=%.2f text='%s'",
                   (unsigned long)event->seq, (unsigned)event->textdraw_id, (double)slot->transmit.x,
                   (double)slot->transmit.y, (double)slot->transmit.line_width,
                   (double)slot->transmit.line_height, (unsigned)slot->transmit.style,
                   (unsigned)slot->transmit.flags, (unsigned)slot->transmit.selectable,
                   (unsigned)slot->transmit.preview_model, (double)slot->transmit.preview_zoom, slot->text);
    return;
  }

  if (event->action == SAMP_RAKNET_TEXTDRAW_ACTION_EDIT && InterlockedCompareExchange(&slot->active, 0, 0) != 0) {
    slot->seq = event->seq;
    strncpy(slot->text, event->text, sizeof(slot->text) - 1u);
    slot->text[sizeof(slot->text) - 1u] = '\0';
    runtime_tracef("textdraw: edit seq=%lu id=%u text='%s'", (unsigned long)event->seq,
                   (unsigned)event->textdraw_id, slot->text);
  }
}

static void textdraw_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot) {
  uint32_t previous_seq = 0u;
  uint32_t latest_seq = 0u;
  uint32_t count = 0u;
  uint32_t i = 0u;

  if (snapshot == NULL) {
    return;
  }

  if ((snapshot->flags & SAMP_RAKNET_RPC_FLAG_TEXTDRAW_SELECT) != 0u && snapshot->textdraw_select_active != 0u) {
    if (InterlockedExchange(&g_runtime.textdraw_select_active, 1) == 0) {
      runtime_tracef("textdraw: select mode enabled color=0x%08lx", (unsigned long)snapshot->textdraw_select_color);
    }
    g_runtime.textdraw_select_color = snapshot->textdraw_select_color;
    dialog_compat_set_mouse_mode(1);
  } else if (InterlockedCompareExchange(&g_runtime.textdraw_select_active, 0, 0) != 0) {
    InterlockedExchange(&g_runtime.textdraw_select_active, 0);
    InterlockedExchange(&g_runtime.textdraw_mouse_down, 0);
    if (!dialog_compat_active()) {
      dialog_compat_set_mouse_mode(0);
    }
    runtime_tracef("textdraw: select mode disabled by rpc state");
  }

  if ((snapshot->flags & SAMP_RAKNET_RPC_FLAG_TEXTDRAW_EVENT) == 0u) {
    return;
  }

  previous_seq = (uint32_t)InterlockedCompareExchange(&g_runtime.textdraw_event_seq, 0, 0);
  latest_seq = previous_seq;
  count = snapshot->textdraw_event_count;
  if (count > SAMP_RAKNET_TEXTDRAW_EVENT_RING) {
    count = SAMP_RAKNET_TEXTDRAW_EVENT_RING;
  }

  for (i = 0u; i < count; ++i) {
    const samp_raknet_textdraw_event *event = &snapshot->textdraw_events[i];
    if (event->seq != 0u && event->seq > previous_seq) {
      textdraw_compat_apply_event(event);
      latest_seq = event->seq;
    }
  }

  if (latest_seq != previous_seq) {
    InterlockedExchange(&g_runtime.textdraw_event_seq, (LONG)latest_seq);
  }
}

static int object_compat_id_valid(uint16_t object_id) {
  return object_id < SAMP_RAKNET_MAX_OBJECTS;
}

static int object_compat_model_plausible(int32_t model) {
  return model >= 0 && model < (int32_t)SAMP_GTA_MODEL_INFO_COUNT;
}

static int object_compat_is_samp_custom_model(int32_t model) {
  return model >= SAMP_OBJECT_COMPAT_SAMP_MODEL_MIN && model < (int32_t)SAMP_GTA_MODEL_INFO_COUNT;
}

static uintptr_t object_compat_model_info_ptr(int32_t model) {
  const uintptr_t *model_info = NULL;

  if (!object_compat_model_plausible(model)) {
    return 0u;
  }

  /* GTA_REVERSED_REF:
   * CModelInfo::ms_modelInfoPtrs lives at 0xA9B0C8 and has 20000 entries.
   * CStreaming::RequestModel assumes the entry exists, so probe it before
   * asking GTA to stream SA-MP/open.mp object IDs.
   */
  model_info = (const uintptr_t *)(uintptr_t)SAMP_ADDR_MODEL_INFO_PTRS;
  return model_info[model];
}

static int object_compat_model_available(int32_t model) {
  return object_compat_model_info_ptr(model) != 0u;
}

static void object_compat_decrement_pending_count(void) {
  LONG count = InterlockedDecrement(&g_runtime.object_pending_count);
  if (count < 0) {
    InterlockedExchange(&g_runtime.object_pending_count, 0);
  }
}

static void object_compat_clear_pending_slot(samp_object_slot_compat *slot) {
  if (slot != NULL && InterlockedExchange(&slot->pending, 0) != 0) {
    object_compat_decrement_pending_count();
  }
}

static LONG object_compat_active_count(void) {
  LONG count = InterlockedCompareExchange(&g_runtime.object_active_count, 0, 0);
  if (count < 0) {
    InterlockedExchange(&g_runtime.object_active_count, 0);
    return 0;
  }
  return count;
}

static int object_compat_recreate_delay_elapsed(uint16_t object_id, DWORD now) {
  DWORD destroyed_at = 0u;

  if (!object_compat_id_valid(object_id)) {
    return 0;
  }
  destroyed_at = g_runtime.object_destroy_ticks[object_id];
  return destroyed_at == 0u || (DWORD)(now - destroyed_at) >= SAMP_OBJECT_COMPAT_RECREATE_DELAY_MS;
}

static void object_compat_log_backpressure(LONG active, LONG pending, const char *reason) {
  DWORD now = GetTickCount();
  DWORD previous = g_runtime.object_backpressure_last_tick;

  if (previous != 0u && (DWORD)(now - previous) < SAMP_OBJECT_COMPAT_BACKPRESSURE_LOG_INTERVAL_MS) {
    return;
  }

  g_runtime.object_backpressure_last_tick = now;
  runtime_tracef("object: defer creates active=%ld pending=%ld cap=%u reason=%s", (long)active, (long)pending,
                 (unsigned)SAMP_OBJECT_COMPAT_ACTIVE_SOFT_CAP, reason != NULL ? reason : "backpressure");
}

static void object_compat_log_skip(uint16_t object_id, uint32_t seq, int32_t model, const char *reason,
                                   int chat_visible) {
  LONG chat_count = 0;
  uintptr_t model_info = object_compat_model_info_ptr(model);

  runtime_tracef("object_exception: skip id=%u seq=%lu model=%ld reason=%s model_info=0x%08lx",
                 (unsigned)object_id, (unsigned long)seq, (long)model, reason != NULL ? reason : "unknown",
                 (unsigned long)model_info);

  if (!chat_visible) {
    return;
  }

  chat_count = InterlockedIncrement(&g_runtime.object_skip_chat_count);
  if (chat_count <= SAMP_OBJECT_COMPAT_CHAT_SKIP_LIMIT) {
    chat_compat_add_message("Object exception: model %ld skipped (%s).", (long)model,
                            reason != NULL ? reason : "unsupported");
  } else if (chat_count == SAMP_OBJECT_COMPAT_CHAT_SKIP_LIMIT + 1) {
    chat_compat_add_message("Object exception: more unsupported objects skipped; see samp_runtime.log.");
  }
}

static void object_compat_skip_pending_slot(uint16_t object_id, samp_object_slot_compat *slot, const char *reason) {
  int32_t model = 0;
  uint32_t seq = 0u;

  if (slot == NULL) {
    return;
  }

  model = slot->model;
  seq = slot->seq;
  object_compat_log_skip(object_id, seq, model, reason, 1);
  object_compat_clear_pending_slot(slot);
  memset(slot, 0, sizeof(*slot));
}

static int object_compat_vec_plausible(const float vec[3]) {
  return vec != NULL && isfinite(vec[0]) && isfinite(vec[1]) && isfinite(vec[2]) && vec[0] > -30000.0f &&
         vec[0] < 30000.0f && vec[1] > -30000.0f && vec[1] < 30000.0f && vec[2] > -5000.0f &&
         vec[2] < 20000.0f;
}

static int object_compat_rot_plausible(const float rot[3]) {
  return rot != NULL && isfinite(rot[0]) && isfinite(rot[1]) && isfinite(rot[2]) && rot[0] > -10000.0f &&
         rot[0] < 10000.0f && rot[1] > -10000.0f && rot[1] < 10000.0f && rot[2] > -10000.0f &&
         rot[2] < 10000.0f;
}

static int object_compat_move_rot_requested(const float rot[3]) {
  if (!object_compat_rot_plausible(rot)) {
    return 0;
  }
  return fabsf(rot[0] + 1000.0f) > 0.01f || fabsf(rot[1] + 1000.0f) > 0.01f ||
         fabsf(rot[2] + 1000.0f) > 0.01f;
}

static void object_compat_destroy_slot(uint16_t object_id, const char *reason) {
  samp_object_slot_compat *slot = NULL;
  uint32_t gta_id = 0u;
  int was_pending = 0;

  if (!object_compat_id_valid(object_id)) {
    return;
  }

  slot = &g_runtime.object_slots[object_id];
  was_pending = InterlockedExchange(&slot->pending, 0) != 0;
  if (was_pending) {
    object_compat_decrement_pending_count();
  }

  if (InterlockedExchange(&slot->active, 0) == 0) {
    if (was_pending) {
      g_runtime.object_destroy_ticks[object_id] = GetTickCount();
      runtime_tracef("object: destroy_pending id=%u reason=%s", (unsigned)object_id,
                     reason != NULL ? reason : "unknown");
      memset(slot, 0, sizeof(*slot));
    }
    return;
  }

  gta_id = slot->gta_id;
  g_runtime.object_destroy_ticks[object_id] = GetTickCount();
  (void)gta_script_command_compat(0x0108u, "i", (int)gta_id);
  memset(slot, 0, sizeof(*slot));
  {
    LONG count = InterlockedDecrement(&g_runtime.object_active_count);
    if (count < 0) {
      InterlockedExchange(&g_runtime.object_active_count, 0);
    }
  }
  runtime_tracef("object: destroy id=%u gta=%lu reason=%s", (unsigned)object_id, (unsigned long)gta_id,
                 reason != NULL ? reason : "unknown");
}

static int object_compat_create_slot(const samp_raknet_object_event *event) {
  samp_object_slot_compat *slot = NULL;
  const char *skip_reason = NULL;

  if (event == NULL) {
    return 0;
  }

  if (!object_compat_id_valid(event->object_id)) {
    object_compat_log_skip(event->object_id, event->seq, event->model, "invalid_object_id", 0);
    return 0;
  }
  if (!object_compat_model_plausible(event->model)) {
    object_compat_log_skip(event->object_id, event->seq, event->model, "invalid_model", 1);
    return 0;
  }
  if (!object_compat_vec_plausible(event->pos) || !object_compat_rot_plausible(event->rot)) {
    object_compat_log_skip(event->object_id, event->seq, event->model, "invalid_transform", 1);
    return 0;
  }
  if (event->has_attachment != 0u) {
    /* OPENMP_REF + TODO_VERIFY:
     * Attached objects use extra attachment metadata in RPC 44. Creating them as standalone
     * GTA objects at their placeholder position corrupts the object path, so keep this as a
     * logged no-op until AttachObjectToVehicle/Object/Player is implemented.
     */
    object_compat_log_skip(event->object_id, event->seq, event->model, "attached_object", 0);
    return 0;
  }

  if (object_compat_is_samp_custom_model(event->model)) {
    skip_reason = "samp_custom_model";
  } else if (!object_compat_model_available(event->model)) {
    skip_reason = "missing_model_info";
  }
  if (skip_reason != NULL) {
    /* PROBE_TRACE + GTA_REVERSED_REF + INFERRED:
     * Only vanilla GTA-SA models are safe for this minimal object bridge. SA-MP custom objects
     * and missing CModelInfo entries must not reach RequestModel/CreateObject, because the
     * original crash happens in CStreaming when model metadata is absent.
     */
    object_compat_log_skip(event->object_id, event->seq, event->model, skip_reason, 1);
    return 0;
  }

  object_compat_destroy_slot(event->object_id, "replace");

  slot = &g_runtime.object_slots[event->object_id];
  memset(slot, 0, sizeof(*slot));
  slot->seq = event->seq;
  slot->model = event->model;
  slot->materials_count = event->materials_count;
  memcpy(slot->pos, event->pos, sizeof(slot->pos));
  memcpy(slot->rot, event->rot, sizeof(slot->rot));
  InterlockedExchange(&slot->pending, 1);
  InterlockedIncrement(&g_runtime.object_pending_count);

  /* OPENMP_REF + PROBE_TRACE + INFERRED:
   * Keep the decoded open.mp 0.3.7 object state, but defer GTA object creation until
   * Spawn() has finalized. The trace shows the server streams objects while the language
   * dialog is still open; calling RequestModel/CreateObject from that phase crashes in
   * CStreaming for unavailable SA-MP custom models such as 19894.
   */
  if (event->materials_count != 0u) {
    runtime_tracef("object: materials ignored id=%u model=%ld count=%u", (unsigned)event->object_id,
                   (long)event->model, (unsigned)event->materials_count);
  }
  runtime_tracef("object: queue_create seq=%lu id=%u model=%ld materials=%u pos=(%.3f,%.3f,%.3f) rot=(%.3f,%.3f,%.3f)",
                 (unsigned long)event->seq, (unsigned)event->object_id, (long)event->model,
                 (unsigned)event->materials_count, (double)event->pos[0], (double)event->pos[1],
                 (double)event->pos[2], (double)event->rot[0], (double)event->rot[1], (double)event->rot[2]);
  return 1;
}

static int object_compat_apply_pending_slot(uint16_t object_id, samp_object_slot_compat *slot) {
  uint32_t gta_id = 0u;
  LONG active = 0;
  LONG pending = 0;
  DWORD now = GetTickCount();
  uintptr_t model_info = 0u;

  if (slot == NULL || InterlockedCompareExchange(&slot->pending, 0, 0) == 0 ||
      InterlockedCompareExchange(&slot->active, 0, 0) != 0 || !object_compat_model_plausible(slot->model) ||
      !object_compat_vec_plausible(slot->pos) || !object_compat_rot_plausible(slot->rot)) {
    return 0;
  }

  active = object_compat_active_count();
  pending = InterlockedCompareExchange(&g_runtime.object_pending_count, 0, 0);
  if (active >= SAMP_OBJECT_COMPAT_ACTIVE_SOFT_CAP) {
    /* PROBE_TRACE + INFERRED:
     * Replacement runs crashed in GTA code while applying ScrCreateObject at about 127 active
     * script-created objects. Keep the authoritative server object state pending, but stop
     * feeding GTA's script object path before pool pressure reaches that unstable range.
     * TODO_VERIFY against an original 0.3.7 object-pool trace.
     */
    object_compat_log_backpressure(active, pending, "active_soft_cap");
    return 0;
  }

  if (!object_compat_recreate_delay_elapsed(object_id, now)) {
    return 0;
  }

  if (object_compat_is_samp_custom_model(slot->model)) {
    if (InterlockedCompareExchange(&slot->blocked_logged, 1, 0) == 0) {
      object_compat_skip_pending_slot(object_id, slot, "samp_custom_model");
    }
    return 0;
  }

  if (!object_compat_model_available(slot->model)) {
    if (InterlockedCompareExchange(&slot->blocked_logged, 1, 0) == 0) {
      object_compat_skip_pending_slot(object_id, slot, "missing_model_info");
    }
    return 0;
  }

  /* OLD_02X_REF + GTA_REVERSED_REF:
   * 0.2x creates client objects with create_object -> put_object_at -> set_object_rotation.
   * gta-reversed shows opcode 0107 asks CModelInfo/CObject/CWorld immediately, so gate that
   * path on CModelInfo availability and spawned client state.
   */
  model_info = object_compat_model_info_ptr(slot->model);
  runtime_tracef("object: create_begin seq=%lu id=%u model=%ld model_info=0x%08lx active=%ld pending=%ld pos=(%.3f,%.3f,%.3f) rot=(%.3f,%.3f,%.3f)",
                 (unsigned long)slot->seq, (unsigned)object_id, (long)slot->model, (unsigned long)model_info,
                 (long)active, (long)pending, (double)slot->pos[0], (double)slot->pos[1],
                 (double)slot->pos[2], (double)slot->rot[0], (double)slot->rot[1], (double)slot->rot[2]);
  gta_streaming_request_model_compat(slot->model, SAMP_OBJECT_COMPAT_MODEL_LOAD_FLAGS);
  gta_streaming_load_all_requested_compat(0);
  if (!gta_script_command_compat(0x0107u, "ifffv", (int)slot->model, slot->pos[0], slot->pos[1], slot->pos[2],
                                 &gta_id)) {
    runtime_tracef("object: create failed id=%u model=%ld opcode=create_object", (unsigned)object_id,
                   (long)slot->model);
    object_compat_clear_pending_slot(slot);
    memset(slot, 0, sizeof(*slot));
    return 0;
  }
  if (!gta_script_command_compat(0x01BCu, "ifff", (int)gta_id, slot->pos[0], slot->pos[1], slot->pos[2])) {
    runtime_tracef("object: create failed id=%u gta=%lu opcode=put_object_at", (unsigned)object_id,
                   (unsigned long)gta_id);
    (void)gta_script_command_compat(0x0108u, "i", (int)gta_id);
    object_compat_clear_pending_slot(slot);
    memset(slot, 0, sizeof(*slot));
    return 0;
  }
  (void)gta_script_command_compat(0x0453u, "ifff", (int)gta_id, slot->rot[0], slot->rot[1], slot->rot[2]);

  slot->gta_id = gta_id;
  InterlockedExchange(&slot->active, 1);
  object_compat_clear_pending_slot(slot);
  InterlockedIncrement(&g_runtime.object_active_count);
  runtime_tracef("object: create seq=%lu id=%u gta=%lu model=%ld pos=(%.3f,%.3f,%.3f) rot=(%.3f,%.3f,%.3f)",
                 (unsigned long)slot->seq, (unsigned)object_id, (unsigned long)gta_id, (long)slot->model,
                 (double)slot->pos[0], (double)slot->pos[1], (double)slot->pos[2], (double)slot->rot[0],
                 (double)slot->rot[1], (double)slot->rot[2]);
  return 1;
}

static void object_compat_apply_event(const samp_raknet_object_event *event) {
  samp_object_slot_compat *slot = NULL;

  if (event == NULL || event->seq == 0u || !object_compat_id_valid(event->object_id)) {
    return;
  }

  if (event->action == SAMP_RAKNET_OBJECT_ACTION_CREATE) {
    (void)object_compat_create_slot(event);
    return;
  }

  if (event->action == SAMP_RAKNET_OBJECT_ACTION_DESTROY) {
    object_compat_destroy_slot(event->object_id, "rpc");
    return;
  }

  slot = &g_runtime.object_slots[event->object_id];
  if (InterlockedCompareExchange(&slot->active, 0, 0) == 0 &&
      InterlockedCompareExchange(&slot->pending, 0, 0) != 0) {
    slot->seq = event->seq;
    if (event->action == SAMP_RAKNET_OBJECT_ACTION_SET_POS && object_compat_vec_plausible(event->pos)) {
      memcpy(slot->pos, event->pos, sizeof(slot->pos));
      runtime_tracef("object: pending_set_pos seq=%lu id=%u pos=(%.3f,%.3f,%.3f)", (unsigned long)event->seq,
                     (unsigned)event->object_id, (double)event->pos[0], (double)event->pos[1],
                     (double)event->pos[2]);
      return;
    }
    if (event->action == SAMP_RAKNET_OBJECT_ACTION_SET_ROT && object_compat_rot_plausible(event->rot)) {
      memcpy(slot->rot, event->rot, sizeof(slot->rot));
      runtime_tracef("object: pending_set_rot seq=%lu id=%u rot=(%.3f,%.3f,%.3f)", (unsigned long)event->seq,
                     (unsigned)event->object_id, (double)event->rot[0], (double)event->rot[1],
                     (double)event->rot[2]);
      return;
    }
    if (event->action == SAMP_RAKNET_OBJECT_ACTION_MOVE && object_compat_vec_plausible(event->move_to)) {
      memcpy(slot->pos, event->move_to, sizeof(slot->pos));
      if (object_compat_move_rot_requested(event->rot)) {
        memcpy(slot->rot, event->rot, sizeof(slot->rot));
      }
      slot->move_speed = event->move_speed;
      runtime_tracef("object: pending_move seq=%lu id=%u to=(%.3f,%.3f,%.3f) speed=%.3f", (unsigned long)event->seq,
                     (unsigned)event->object_id, (double)event->move_to[0], (double)event->move_to[1],
                     (double)event->move_to[2], (double)event->move_speed);
      return;
    }
    if (event->action == SAMP_RAKNET_OBJECT_ACTION_STOP) {
      slot->move_speed = 0.0f;
      runtime_tracef("object: pending_stop seq=%lu id=%u", (unsigned long)event->seq, (unsigned)event->object_id);
      return;
    }
  }

  if (InterlockedCompareExchange(&slot->active, 0, 0) == 0) {
    runtime_tracef("object: ignored action=%u id=%u seq=%lu reason=inactive", (unsigned)event->action,
                   (unsigned)event->object_id, (unsigned long)event->seq);
    return;
  }

  slot->seq = event->seq;
  if (event->action == SAMP_RAKNET_OBJECT_ACTION_SET_POS && object_compat_vec_plausible(event->pos)) {
    (void)gta_script_command_compat(0x01BCu, "ifff", (int)slot->gta_id, event->pos[0], event->pos[1], event->pos[2]);
    memcpy(slot->pos, event->pos, sizeof(slot->pos));
    runtime_tracef("object: set_pos seq=%lu id=%u gta=%lu pos=(%.3f,%.3f,%.3f)", (unsigned long)event->seq,
                   (unsigned)event->object_id, (unsigned long)slot->gta_id, (double)event->pos[0],
                   (double)event->pos[1], (double)event->pos[2]);
    return;
  }

  if (event->action == SAMP_RAKNET_OBJECT_ACTION_SET_ROT && object_compat_rot_plausible(event->rot)) {
    (void)gta_script_command_compat(0x0453u, "ifff", (int)slot->gta_id, event->rot[0], event->rot[1], event->rot[2]);
    memcpy(slot->rot, event->rot, sizeof(slot->rot));
    runtime_tracef("object: set_rot seq=%lu id=%u gta=%lu rot=(%.3f,%.3f,%.3f)", (unsigned long)event->seq,
                   (unsigned)event->object_id, (unsigned long)slot->gta_id, (double)event->rot[0],
                   (double)event->rot[1], (double)event->rot[2]);
    return;
  }

  if (event->action == SAMP_RAKNET_OBJECT_ACTION_MOVE && object_compat_vec_plausible(event->move_from) &&
      object_compat_vec_plausible(event->move_to)) {
    /* TODO_VERIFY:
     * Full SA-MP object movement needs a per-frame object pool/interpolator. Until that exists,
     * apply start and final positions immediately, but keep the open.mp target rotation when present.
     */
    (void)gta_script_command_compat(0x01BCu, "ifff", (int)slot->gta_id, event->move_from[0], event->move_from[1],
                                    event->move_from[2]);
    (void)gta_script_command_compat(0x01BCu, "ifff", (int)slot->gta_id, event->move_to[0], event->move_to[1],
                                    event->move_to[2]);
    if (object_compat_move_rot_requested(event->rot)) {
      (void)gta_script_command_compat(0x0453u, "ifff", (int)slot->gta_id, event->rot[0], event->rot[1],
                                      event->rot[2]);
      memcpy(slot->rot, event->rot, sizeof(slot->rot));
    }
    memcpy(slot->pos, event->move_to, sizeof(slot->pos));
    slot->move_speed = event->move_speed;
    runtime_tracef("object: move_immediate seq=%lu id=%u gta=%lu from=(%.3f,%.3f,%.3f) to=(%.3f,%.3f,%.3f) speed=%.3f rot_requested=%d",
                   (unsigned long)event->seq, (unsigned)event->object_id, (unsigned long)slot->gta_id,
                   (double)event->move_from[0], (double)event->move_from[1], (double)event->move_from[2],
                   (double)event->move_to[0], (double)event->move_to[1], (double)event->move_to[2],
                   (double)event->move_speed, object_compat_move_rot_requested(event->rot));
    return;
  }

  if (event->action == SAMP_RAKNET_OBJECT_ACTION_STOP) {
    slot->move_speed = 0.0f;
    runtime_tracef("object: stop seq=%lu id=%u gta=%lu", (unsigned long)event->seq, (unsigned)event->object_id,
                   (unsigned long)slot->gta_id);
  }
}

static int object_compat_can_flush_pending(void) {
  return InterlockedCompareExchange(&g_runtime.mp_session_spawn_finalized, 0, 0) != 0 &&
         InterlockedCompareExchange(&g_runtime.mp_session_scene_loaded, 0, 0) != 0;
}

static void object_compat_flush_pending(uint32_t budget) {
  uint32_t applied = 0u;
  uint32_t i = 0u;
  LONG active = 0;
  LONG pending = 0;

  pending = InterlockedCompareExchange(&g_runtime.object_pending_count, 0, 0);
  if (pending <= 0 || budget == 0u || !object_compat_can_flush_pending()) {
    return;
  }

  active = object_compat_active_count();
  if (active >= SAMP_OBJECT_COMPAT_ACTIVE_SOFT_CAP) {
    object_compat_log_backpressure(active, pending, "flush_soft_cap");
    return;
  }

  for (i = 0u; i < SAMP_RAKNET_MAX_OBJECTS; ++i) {
    samp_object_slot_compat *slot = &g_runtime.object_slots[i];
    active = object_compat_active_count();
    if (active >= SAMP_OBJECT_COMPAT_ACTIVE_SOFT_CAP) {
      object_compat_log_backpressure(active, InterlockedCompareExchange(&g_runtime.object_pending_count, 0, 0),
                                     "flush_budget_cap");
      break;
    }
    if (InterlockedCompareExchange(&slot->pending, 0, 0) != 0 &&
        InterlockedCompareExchange(&slot->active, 0, 0) == 0) {
      if (object_compat_apply_pending_slot((uint16_t)i, slot)) {
        ++applied;
        if (applied >= budget) {
          break;
        }
      }
    }
  }

  if (applied != 0u) {
    runtime_tracef("object: flush applied=%lu active=%ld pending=%ld", (unsigned long)applied,
                   (long)InterlockedCompareExchange(&g_runtime.object_active_count, 0, 0),
                   (long)InterlockedCompareExchange(&g_runtime.object_pending_count, 0, 0));
  }
}

static void object_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot) {
  uint32_t previous_seq = 0u;
  uint32_t latest_seq = 0u;
  uint32_t count = 0u;
  uint32_t i = 0u;

  if (snapshot == NULL) {
    return;
  }

  if ((snapshot->flags & SAMP_RAKNET_RPC_FLAG_OBJECT_EVENT) == 0u) {
    object_compat_flush_pending(SAMP_OBJECT_COMPAT_CREATE_BUDGET);
    return;
  }

  previous_seq = (uint32_t)InterlockedCompareExchange(&g_runtime.object_event_seq, 0, 0);
  latest_seq = previous_seq;
  count = snapshot->object_event_count;
  if (count > SAMP_RAKNET_OBJECT_EVENT_RING) {
    count = SAMP_RAKNET_OBJECT_EVENT_RING;
  }

  for (i = 0u; i < count; ++i) {
    const samp_raknet_object_event *event = &snapshot->object_events[i];
    if (event->seq != 0u && event->seq > previous_seq) {
      object_compat_apply_event(event);
      latest_seq = event->seq;
    }
  }

  if (latest_seq != previous_seq) {
    InterlockedExchange(&g_runtime.object_event_seq, (LONG)latest_seq);
  }
  if (InterlockedCompareExchange(&g_runtime.object_logged, 1, 0) == 0) {
    runtime_tracef("object: bridge active events=%lu active=%ld pending=%ld latest_seq=%lu", (unsigned long)count,
                   (long)InterlockedCompareExchange(&g_runtime.object_active_count, 0, 0),
                   (long)InterlockedCompareExchange(&g_runtime.object_pending_count, 0, 0),
                   (unsigned long)latest_seq);
  }
  object_compat_flush_pending(SAMP_OBJECT_COMPAT_CREATE_BUDGET);
}

static void object_compat_reset_pool(const char *reason) {
  unsigned int i = 0u;

  for (i = 0u; i < SAMP_RAKNET_MAX_OBJECTS; ++i) {
    if (InterlockedCompareExchange(&g_runtime.object_slots[i].active, 0, 0) != 0) {
      object_compat_destroy_slot((uint16_t)i, reason);
    } else if (InterlockedCompareExchange(&g_runtime.object_slots[i].pending, 0, 0) != 0) {
      object_compat_destroy_slot((uint16_t)i, reason);
    }
  }
  InterlockedExchange(&g_runtime.object_event_seq, 0);
  InterlockedExchange(&g_runtime.object_active_count, 0);
  InterlockedExchange(&g_runtime.object_pending_count, 0);
  InterlockedExchange(&g_runtime.object_logged, 0);
  InterlockedExchange(&g_runtime.object_skip_chat_count, 0);
  g_runtime.object_backpressure_last_tick = 0u;
  memset(g_runtime.object_slots, 0, sizeof(g_runtime.object_slots));
  memset(g_runtime.object_destroy_ticks, 0, sizeof(g_runtime.object_destroy_ticks));
}

static int vehicle_compat_id_valid(uint16_t vehicle_id) {
  return vehicle_id < SAMP_RAKNET_MAX_VEHICLES;
}

static int vehicle_compat_model_valid(int32_t model) {
  return model >= SAMP_VEHICLE_COMPAT_MODEL_MIN && model <= SAMP_VEHICLE_COMPAT_MODEL_MAX;
}

static int vehicle_compat_is_train_model(int32_t model) {
  return model == SAMP_VEHICLE_COMPAT_TRAIN_PASSENGER_LOCO || model == SAMP_VEHICLE_COMPAT_TRAIN_FREIGHT_LOCO ||
         model == SAMP_VEHICLE_COMPAT_TRAIN_PASSENGER || model == SAMP_VEHICLE_COMPAT_TRAIN_FREIGHT ||
         model == SAMP_VEHICLE_COMPAT_TRAIN_TRAM;
}

static int vehicle_compat_vec_plausible(const float vec[3]) {
  return vec != NULL && isfinite(vec[0]) && isfinite(vec[1]) && isfinite(vec[2]) && vec[0] > -30000.0f &&
         vec[0] < 30000.0f && vec[1] > -30000.0f && vec[1] < 30000.0f && vec[2] > -5000.0f &&
         vec[2] < 20000.0f;
}

static int vehicle_compat_rotation_plausible(float rotation) {
  return isfinite(rotation) && rotation > -720.0f && rotation < 720.0f;
}

static int vehicle_compat_health_plausible(float health) {
  return isfinite(health) && health >= 0.0f && health <= 10000.0f;
}

static void vehicle_compat_decrement_pending_count(void) {
  LONG count = InterlockedDecrement(&g_runtime.vehicle_pending_count);
  if (count < 0) {
    InterlockedExchange(&g_runtime.vehicle_pending_count, 0);
  }
}

static void vehicle_compat_clear_pending_slot(samp_vehicle_slot_compat *slot) {
  if (slot != NULL && InterlockedExchange(&slot->pending, 0) != 0) {
    vehicle_compat_decrement_pending_count();
  }
}

static void vehicle_compat_log_skip(uint16_t vehicle_id, uint32_t seq, int32_t model, const char *reason) {
  runtime_tracef("vehicle_exception: skip id=%u seq=%lu model=%ld reason=%s model_info=0x%08lx",
                 (unsigned)vehicle_id, (unsigned long)seq, (long)model, reason != NULL ? reason : "unknown",
                 (unsigned long)object_compat_model_info_ptr(model));
}

static void vehicle_compat_skip_pending_slot(uint16_t vehicle_id, samp_vehicle_slot_compat *slot,
                                             const char *reason) {
  int32_t model = 0;
  uint32_t seq = 0u;

  if (slot == NULL) {
    return;
  }

  model = slot->model;
  seq = slot->seq;
  vehicle_compat_log_skip(vehicle_id, seq, model, reason);
  vehicle_compat_clear_pending_slot(slot);
  memset(slot, 0, sizeof(*slot));
}

static unsigned int vehicle_compat_count_mods(const uint8_t mods[SAMP_RAKNET_VEHICLE_MOD_SLOTS]) {
  unsigned int count = 0u;
  unsigned int i = 0u;

  if (mods == NULL) {
    return 0u;
  }
  for (i = 0u; i < SAMP_RAKNET_VEHICLE_MOD_SLOTS; ++i) {
    if (mods[i] != 0u) {
      ++count;
    }
  }
  return count;
}

static void vehicle_compat_disable_marker(samp_vehicle_slot_compat *slot) {
  if (slot == NULL || slot->marker_id == 0u) {
    return;
  }

  (void)gta_script_command_compat(0x0164u, "i", (int)slot->marker_id);
  slot->marker_id = 0u;
}

static void vehicle_compat_destroy_slot(uint16_t vehicle_id, const char *reason) {
  samp_vehicle_slot_compat *slot = NULL;
  uint32_t gta_id = 0u;
  int was_pending = 0;

  if (!vehicle_compat_id_valid(vehicle_id)) {
    return;
  }

  slot = &g_runtime.vehicle_slots[vehicle_id];
  was_pending = InterlockedExchange(&slot->pending, 0) != 0;
  if (was_pending) {
    vehicle_compat_decrement_pending_count();
  }

  if (InterlockedExchange(&slot->active, 0) == 0) {
    if (was_pending) {
      runtime_tracef("vehicle: destroy_pending id=%u reason=%s", (unsigned)vehicle_id,
                     reason != NULL ? reason : "unknown");
      memset(slot, 0, sizeof(*slot));
    }
    return;
  }

  gta_id = slot->gta_id;
  if (slot->marker_id != 0u) {
    /* OLD_02X_REF + TODO_VERIFY:
     * CVehicle::~CVehicle disables the per-vehicle radar marker before destroying
     * the GTA vehicle. Keep the marker lifecycle coupled to our temporary slot.
     */
    vehicle_compat_disable_marker(slot);
  }
  /* OLD_02X_REF:
   * CVehicle::~CVehicle uses destroy_car for non-train vehicles. Trains are intentionally
   * skipped in this first bridge, so destroy_car is the only active remove path here.
   */
  (void)gta_script_command_compat(0x00A6u, "i", (int)gta_id);
  memset(slot, 0, sizeof(*slot));
  {
    LONG count = InterlockedDecrement(&g_runtime.vehicle_active_count);
    if (count < 0) {
      InterlockedExchange(&g_runtime.vehicle_active_count, 0);
    }
  }
  runtime_tracef("vehicle: destroy id=%u gta=%lu reason=%s", (unsigned)vehicle_id, (unsigned long)gta_id,
                 reason != NULL ? reason : "unknown");
}

static int vehicle_compat_create_slot(const samp_raknet_vehicle_event *event) {
  samp_vehicle_slot_compat *slot = NULL;

  if (event == NULL) {
    return 0;
  }

  if (!vehicle_compat_id_valid(event->vehicle_id)) {
    vehicle_compat_log_skip(event->vehicle_id, event->seq, event->model, "invalid_vehicle_id");
    return 0;
  }
  if (!vehicle_compat_model_valid(event->model)) {
    vehicle_compat_log_skip(event->vehicle_id, event->seq, event->model, "invalid_model");
    return 0;
  }
  if (!vehicle_compat_vec_plausible(event->pos) || !vehicle_compat_rotation_plausible(event->rotation) ||
      !vehicle_compat_health_plausible(event->health)) {
    vehicle_compat_log_skip(event->vehicle_id, event->seq, event->model, "invalid_transform");
    return 0;
  }
  if (vehicle_compat_is_train_model(event->model)) {
    /* OLD_02X_REF + TODO_VERIFY:
     * 0.2x creates trains through create_train and attaches carriages. Using create_car for
     * train IDs is not equivalent, so leave train rendering disabled until that path is traced.
     */
    vehicle_compat_log_skip(event->vehicle_id, event->seq, event->model, "train_model_unsupported");
    return 0;
  }
  if (!object_compat_model_available(event->model)) {
    vehicle_compat_log_skip(event->vehicle_id, event->seq, event->model, "missing_model_info");
    return 0;
  }

  vehicle_compat_destroy_slot(event->vehicle_id, "replace");

  slot = &g_runtime.vehicle_slots[event->vehicle_id];
  memset(slot, 0, sizeof(*slot));
  slot->seq = event->seq;
  slot->model = event->model;
  slot->color1 = event->color1;
  slot->color2 = event->color2;
  slot->interior = event->interior;
  slot->paintjob = event->paintjob;
  slot->light_damage = event->light_damage;
  slot->tyre_damage = event->tyre_damage;
  slot->siren = event->siren;
  memcpy(slot->mods, event->mods, sizeof(slot->mods));
  memcpy(slot->pos, event->pos, sizeof(slot->pos));
  slot->rotation = event->rotation;
  slot->health = event->health;
  slot->door_damage = event->door_damage;
  slot->panel_damage = event->panel_damage;
  slot->body_color1 = event->body_color1;
  slot->body_color2 = event->body_color2;
  InterlockedExchange(&slot->pending, 1);
  InterlockedIncrement(&g_runtime.vehicle_pending_count);

  runtime_tracef("vehicle: queue_create seq=%lu id=%u model=%ld pos=(%.3f,%.3f,%.3f) rot=%.3f colors=%u/%u health=%.3f interior=%u siren=%u mods=%u paintjob=%u",
                 (unsigned long)event->seq, (unsigned)event->vehicle_id, (long)event->model,
                 (double)event->pos[0], (double)event->pos[1], (double)event->pos[2],
                 (double)event->rotation, (unsigned)event->color1, (unsigned)event->color2,
                 (double)event->health, (unsigned)event->interior, (unsigned)event->siren,
                 vehicle_compat_count_mods(event->mods), (unsigned)event->paintjob);
  return 1;
}

static int vehicle_compat_apply_pending_slot(uint16_t vehicle_id, samp_vehicle_slot_compat *slot) {
  uint32_t gta_id = 0u;

  if (slot == NULL || InterlockedCompareExchange(&slot->pending, 0, 0) == 0 ||
      InterlockedCompareExchange(&slot->active, 0, 0) != 0 || !vehicle_compat_model_valid(slot->model) ||
      !vehicle_compat_vec_plausible(slot->pos) || !vehicle_compat_rotation_plausible(slot->rotation) ||
      !vehicle_compat_health_plausible(slot->health)) {
    return 0;
  }

  if (vehicle_compat_is_train_model(slot->model)) {
    if (InterlockedCompareExchange(&slot->blocked_logged, 1, 0) == 0) {
      vehicle_compat_skip_pending_slot(vehicle_id, slot, "train_model_unsupported");
    }
    return 0;
  }

  if (!object_compat_model_available(slot->model)) {
    if (InterlockedCompareExchange(&slot->blocked_logged, 1, 0) == 0) {
      vehicle_compat_skip_pending_slot(vehicle_id, slot, "missing_model_info");
    }
    return 0;
  }

  /* OLD_02X_REF + OPENMP_REF + INFERRED:
   * open.mp streams RPC 164 before/around spawn; the 0.2x client creates vanilla vehicles
   * with create_car, then set_car_z_angle. We defer this until the MP session is spawned,
   * matching the existing object bridge gate to avoid CStreaming work during dialogs/loading.
   */
  runtime_tracef("vehicle: create_begin seq=%lu id=%u model=%ld pos=(%.3f,%.3f,%.3f) rot=%.3f",
                 (unsigned long)slot->seq, (unsigned)vehicle_id, (long)slot->model, (double)slot->pos[0],
                 (double)slot->pos[1], (double)slot->pos[2], (double)slot->rotation);
  gta_streaming_request_model_compat(slot->model, SAMP_VEHICLE_COMPAT_MODEL_LOAD_FLAGS);
  gta_streaming_load_all_requested_compat(0);
  if (!gta_script_command_compat(0x00A5u, "ifffv", (int)slot->model, slot->pos[0], slot->pos[1],
                                 slot->pos[2] + 0.1f, &gta_id)) {
    runtime_tracef("vehicle: create failed id=%u model=%ld opcode=create_car", (unsigned)vehicle_id,
                   (long)slot->model);
    vehicle_compat_clear_pending_slot(slot);
    memset(slot, 0, sizeof(*slot));
    return 0;
  }
  (void)gta_script_command_compat(0x0175u, "if", (int)gta_id, slot->rotation);
  (void)gta_script_command_compat(0x09C4u, "ii", (int)gta_id, 0);
  (void)gta_script_command_compat(0x07FFu, "ii", (int)gta_id, 0);
  (void)gta_script_command_compat(0x053Fu, "ii", (int)gta_id, 0);
  /* OLD_02X_REF + TODO_VERIFY:
   * CVehicle sets dwDoorsLocked=0 after create_car and SetLockedState(false) uses
   * lock_car(gta_id, 0). We do not write VEHICLE_TYPE directly yet, so use the
   * script command side first.
   */
  (void)gta_script_command_compat(0x0519u, "ii", (int)gta_id, 0);
  if (slot->interior != 0u) {
    (void)gta_script_command_compat(0x0840u, "ii", (int)gta_id, (int)slot->interior);
  }
  if (slot->color1 != 255u || slot->color2 != 255u) {
    /* OLD_02X_REF + TODO_VERIFY:
     * 0.2x writes CVehicle color bytes directly because set_car_color could crash. We do not
     * have a verified VEHICLE_TYPE layout in this runtime yet, so colour application is deferred.
     */
    runtime_tracef("vehicle: color_deferred id=%u gta=%lu colors=%u/%u", (unsigned)vehicle_id,
                   (unsigned long)gta_id, (unsigned)slot->color1, (unsigned)slot->color2);
  }
  if (slot->paintjob != 255u || vehicle_compat_count_mods(slot->mods) != 0u || slot->door_damage != 0u ||
      slot->panel_damage != 0u || slot->light_damage != 0u || slot->tyre_damage != 0u || slot->siren != 0u) {
    runtime_tracef("vehicle: extra_state_deferred id=%u gta=%lu paintjob=%u mods=%u door=0x%08lx panel=0x%08lx light=%u tyre=%u siren=%u",
                   (unsigned)vehicle_id, (unsigned long)gta_id, (unsigned)slot->paintjob,
                   vehicle_compat_count_mods(slot->mods), (unsigned long)slot->door_damage,
                   (unsigned long)slot->panel_damage, (unsigned)slot->light_damage, (unsigned)slot->tyre_damage,
                   (unsigned)slot->siren);
  }

  slot->gta_id = gta_id;
  InterlockedExchange(&slot->active, 1);
  vehicle_compat_clear_pending_slot(slot);
  InterlockedIncrement(&g_runtime.vehicle_active_count);
  runtime_tracef("vehicle: create seq=%lu id=%u gta=%lu model=%ld pos=(%.3f,%.3f,%.3f) rot=%.3f active=%ld pending=%ld",
                 (unsigned long)slot->seq, (unsigned)vehicle_id, (unsigned long)gta_id, (long)slot->model,
                 (double)slot->pos[0], (double)slot->pos[1], (double)slot->pos[2], (double)slot->rotation,
                 (long)InterlockedCompareExchange(&g_runtime.vehicle_active_count, 0, 0),
                 (long)InterlockedCompareExchange(&g_runtime.vehicle_pending_count, 0, 0));
  return 1;
}

static void vehicle_compat_apply_event(const samp_raknet_vehicle_event *event) {
  if (event == NULL || event->seq == 0u || !vehicle_compat_id_valid(event->vehicle_id)) {
    return;
  }

  if (event->action == SAMP_RAKNET_VEHICLE_ACTION_CREATE) {
    (void)vehicle_compat_create_slot(event);
    return;
  }

  if (event->action == SAMP_RAKNET_VEHICLE_ACTION_DESTROY) {
    vehicle_compat_destroy_slot(event->vehicle_id, "rpc");
    return;
  }

  runtime_tracef("vehicle: ignored action=%u id=%u seq=%lu reason=unknown_action", (unsigned)event->action,
                 (unsigned)event->vehicle_id, (unsigned long)event->seq);
}

static void vehicle_compat_flush_pending(uint32_t budget) {
  uint32_t applied = 0u;
  uint32_t i = 0u;
  LONG pending = 0;
  DWORD now = 0u;
  DWORD hold_until = 0u;

  pending = InterlockedCompareExchange(&g_runtime.vehicle_pending_count, 0, 0);
  if (pending <= 0 || budget == 0u || !object_compat_can_flush_pending()) {
    return;
  }
  now = GetTickCount();
  hold_until = g_runtime.vehicle_create_hold_until_tick;
  if (hold_until != 0u && (LONG)(now - hold_until) < 0) {
    return;
  }
  if (g_runtime.vehicle_create_last_tick != 0u &&
      (DWORD)(now - g_runtime.vehicle_create_last_tick) < SAMP_VEHICLE_COMPAT_CREATE_INTERVAL_MS) {
    return;
  }

  for (i = 0u; i < SAMP_RAKNET_MAX_VEHICLES; ++i) {
    samp_vehicle_slot_compat *slot = &g_runtime.vehicle_slots[i];
    if (InterlockedCompareExchange(&slot->pending, 0, 0) != 0 &&
        InterlockedCompareExchange(&slot->active, 0, 0) == 0) {
      if (vehicle_compat_apply_pending_slot((uint16_t)i, slot)) {
        g_runtime.vehicle_create_last_tick = now;
        ++applied;
        if (applied >= budget) {
          break;
        }
      }
    }
  }

  if (applied != 0u) {
    runtime_tracef("vehicle: flush applied=%lu active=%ld pending=%ld", (unsigned long)applied,
                   (long)InterlockedCompareExchange(&g_runtime.vehicle_active_count, 0, 0),
                   (long)InterlockedCompareExchange(&g_runtime.vehicle_pending_count, 0, 0));
  }
}

static uintptr_t vehicle_compat_game_pool_get_at(uint32_t gta_id) {
#if defined(__i386__) || defined(_M_IX86)
  uintptr_t table = 0u;
  uintptr_t vehicle = 0u;

  if (!memory_is_readable_compat((const void *)(uintptr_t)SAMP_ADDR_VEHICLE_TABLE, sizeof(table))) {
    return 0u;
  }
  memcpy(&table, (const void *)(uintptr_t)SAMP_ADDR_VEHICLE_TABLE, sizeof(table));
  if (table < 0x10000u || table >= 0x80000000u) {
    return 0u;
  }

  /*
   * OLD_02X_REF + TODO_VERIFY:
   * samp/client/game/util.cpp calls GTA's vehicle-pool lookup with ECX=[0xB74494],
   * push gta_id, call 0x4048E0. Keep the call identical but guarded by readable
   * pool state until we have an OBSERVED_037 layout trace for the vehicle pool.
   */
  __asm__ __volatile__("movl %[table], %%ecx\n\t"
                       "pushl %[id]\n\t"
                       "call *%[from_id]\n\t"
                       "movl %%eax, %[vehicle]\n\t"
                       : [vehicle] "=r"(vehicle)
                       : [table] "r"(table), [id] "r"((int)gta_id),
                         [from_id] "r"((uintptr_t)SAMP_ADDR_VEHICLE_FROM_ID)
                       : "eax", "ecx", "edx", "memory", "cc");
  if (vehicle < 0x10000u || vehicle >= 0x80000000u) {
    return 0u;
  }
  return vehicle;
#else
  (void)gta_id;
  return 0u;
#endif
}

static int vehicle_compat_is_occupied(uintptr_t vehicle) {
  uintptr_t occupant = 0u;
  unsigned int i = 0u;

  if (vehicle < 0x10000u || vehicle >= 0x80000000u) {
    return 0;
  }

  /*
   * OLD_02X_REF + TODO_VERIFY:
   * VEHICLE_TYPE::pDriver is at +1120, pPassengers[7] starts at +1124 in the
   * 0.2x common.h layout. This mirrors CVehicle::IsOccupied() for scanner markers.
   */
  if (memory_is_readable_compat((const void *)(vehicle + SAMP_VEHICLE_OFFSET_DRIVER), sizeof(occupant))) {
    memcpy(&occupant, (const void *)(vehicle + SAMP_VEHICLE_OFFSET_DRIVER), sizeof(occupant));
    if (occupant != 0u) {
      return 1;
    }
  }

  if (!memory_is_readable_compat((const void *)(vehicle + SAMP_VEHICLE_OFFSET_PASSENGERS),
                                 sizeof(occupant) * SAMP_VEHICLE_PASSENGER_COUNT)) {
    return 0;
  }
  for (i = 0u; i < SAMP_VEHICLE_PASSENGER_COUNT; ++i) {
    memcpy(&occupant, (const void *)(vehicle + SAMP_VEHICLE_OFFSET_PASSENGERS + (i * sizeof(occupant))),
           sizeof(occupant));
    if (occupant != 0u) {
      return 1;
    }
  }
  return 0;
}

static int vehicle_compat_near_local_ped(const samp_vehicle_slot_compat *slot, uintptr_t vehicle, uintptr_t ped) {
  float ped_x = 0.0f;
  float ped_y = 0.0f;
  float ped_z = 0.0f;
  float veh_x = 0.0f;
  float veh_y = 0.0f;
  float veh_z = 0.0f;
  float dx = 0.0f;
  float dy = 0.0f;
  float dz = 0.0f;
  float dist_sq = 0.0f;
  const float limit_sq = SAMP_VEHICLE_SCANNER_DISTANCE * SAMP_VEHICLE_SCANNER_DISTANCE;

  if (slot == NULL || ped == 0u || !gta_entity_read_position_compat(ped, &ped_x, &ped_y, &ped_z)) {
    return 0;
  }

  veh_x = slot->pos[0];
  veh_y = slot->pos[1];
  veh_z = slot->pos[2];
  if (vehicle != 0u) {
    (void)gta_entity_read_position_compat(vehicle, &veh_x, &veh_y, &veh_z);
  }

  dx = veh_x - ped_x;
  dy = veh_y - ped_y;
  dz = veh_z - ped_z;
  dist_sq = (dx * dx) + (dy * dy) + (dz * dz);
  return isfinite(dist_sq) && dist_sq < limit_sq ? 1 : 0;
}

static void vehicle_compat_update_scanner_marker(uint16_t vehicle_id, samp_vehicle_slot_compat *slot, uintptr_t ped) {
  uintptr_t vehicle = 0u;
  int should_show = 0;

  if (slot == NULL || InterlockedCompareExchange(&slot->active, 0, 0) == 0 || slot->gta_id == 0u) {
    return;
  }

  vehicle = vehicle_compat_game_pool_get_at(slot->gta_id);
  /*
   * OLD_02X_REF + TODO_VERIFY:
   * 0.2x CVehicle::ProcessMarkers shows normal scanner markers only when the
   * vehicle is within CSCANNER_DISTANCE and not occupied. Objective/special
   * markers are deliberately not emulated here yet because no 0.3.7 trace has
   * identified their RPC path in this bridge.
   */
  should_show = (vehicle_compat_near_local_ped(slot, vehicle, ped) && !vehicle_compat_is_occupied(vehicle));
  if (should_show) {
    if (slot->marker_id == 0u &&
        !gta_script_command_compat(0x0161u, "iiiv", (int)slot->gta_id, 1, SAMP_VEHICLE_SCANNER_MARKER_TYPE,
                                   &slot->marker_id)) {
      runtime_tracef("vehicle: marker_create_failed id=%u gta=%lu", (unsigned)vehicle_id,
                     (unsigned long)slot->gta_id);
      slot->marker_id = 0u;
    }
    if (slot->marker_id != 0u) {
      (void)gta_script_command_compat(0x0165u, "ii", (int)slot->marker_id,
                                      SAMP_VEHICLE_SCANNER_MARKER_COLOR);
    }
  } else {
    vehicle_compat_disable_marker(slot);
  }
}

static void vehicle_compat_process_markers(uintptr_t ped) {
  unsigned int i = 0u;

  if (ped == 0u) {
    return;
  }

  for (i = 0u; i < SAMP_RAKNET_MAX_VEHICLES; ++i) {
    vehicle_compat_update_scanner_marker((uint16_t)i, &g_runtime.vehicle_slots[i], ped);
  }
}

static void vehicle_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot) {
  uint32_t previous_seq = 0u;
  uint32_t latest_seq = 0u;
  uint32_t count = 0u;
  uint32_t i = 0u;

  if (snapshot == NULL) {
    return;
  }

  if ((snapshot->flags & SAMP_RAKNET_RPC_FLAG_VEHICLE_EVENT) == 0u) {
    vehicle_compat_flush_pending(SAMP_VEHICLE_COMPAT_CREATE_BUDGET);
    return;
  }

  previous_seq = (uint32_t)InterlockedCompareExchange(&g_runtime.vehicle_event_seq, 0, 0);
  latest_seq = previous_seq;
  count = snapshot->vehicle_event_count;
  if (count > SAMP_RAKNET_VEHICLE_EVENT_RING) {
    count = SAMP_RAKNET_VEHICLE_EVENT_RING;
  }

  for (i = 0u; i < count; ++i) {
    const samp_raknet_vehicle_event *event = &snapshot->vehicle_events[i];
    if (event->seq != 0u && event->seq > previous_seq) {
      vehicle_compat_apply_event(event);
      latest_seq = event->seq;
    }
  }

  if (latest_seq != previous_seq) {
    InterlockedExchange(&g_runtime.vehicle_event_seq, (LONG)latest_seq);
  }
  if (InterlockedCompareExchange(&g_runtime.vehicle_logged, 1, 0) == 0) {
    runtime_tracef("vehicle: bridge active events=%lu active=%ld pending=%ld latest_seq=%lu", (unsigned long)count,
                   (long)InterlockedCompareExchange(&g_runtime.vehicle_active_count, 0, 0),
                   (long)InterlockedCompareExchange(&g_runtime.vehicle_pending_count, 0, 0),
                   (unsigned long)latest_seq);
  }
  vehicle_compat_flush_pending(SAMP_VEHICLE_COMPAT_CREATE_BUDGET);
}

static void vehicle_compat_reset_pool(const char *reason) {
  unsigned int i = 0u;

  for (i = 0u; i < SAMP_RAKNET_MAX_VEHICLES; ++i) {
    if (InterlockedCompareExchange(&g_runtime.vehicle_slots[i].active, 0, 0) != 0) {
      vehicle_compat_destroy_slot((uint16_t)i, reason);
    } else if (InterlockedCompareExchange(&g_runtime.vehicle_slots[i].pending, 0, 0) != 0) {
      vehicle_compat_destroy_slot((uint16_t)i, reason);
    }
  }
  InterlockedExchange(&g_runtime.vehicle_event_seq, 0);
  InterlockedExchange(&g_runtime.vehicle_active_count, 0);
  InterlockedExchange(&g_runtime.vehicle_pending_count, 0);
  InterlockedExchange(&g_runtime.vehicle_logged, 0);
  g_runtime.vehicle_create_last_tick = 0u;
  g_runtime.vehicle_create_hold_until_tick = 0u;
  memset(g_runtime.vehicle_slots, 0, sizeof(g_runtime.vehicle_slots));
}

static void chat_input_uninstall_wndproc_compat(void) {
  HWND hwnd = g_runtime.chat_input_hwnd;
  WNDPROC old_proc = g_runtime.chat_input_old_wndproc;

  if (hwnd != NULL && old_proc != NULL && IsWindow(hwnd) &&
      (WNDPROC)GetWindowLongPtrA(hwnd, GWLP_WNDPROC) == chat_input_wndproc_compat) {
    (void)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)old_proc);
    runtime_tracef("chat_input: wndproc restored hwnd=0x%08lx", (unsigned long)(uintptr_t)hwnd);
  }

  InterlockedExchange(&g_runtime.chat_input_hooked, 0);
  g_runtime.chat_input_hwnd = NULL;
  g_runtime.chat_input_old_wndproc = NULL;
}

static void chat_input_try_install_wndproc_compat(void) {
  HWND hwnd = NULL;
  WNDPROC old_proc = NULL;
  DWORD error = ERROR_SUCCESS;

  if (!chat_input_enabled_compat() || !g_runtime.settings.play_online) {
    return;
  }

  hwnd = read_game_hwnd_compat();
  if (hwnd == NULL) {
    return;
  }

  if (InterlockedCompareExchange(&g_runtime.chat_input_hooked, 0, 0) != 0) {
    if (g_runtime.chat_input_hwnd == hwnd) {
      return;
    }
    chat_input_uninstall_wndproc_compat();
  }

  if ((WNDPROC)GetWindowLongPtrA(hwnd, GWLP_WNDPROC) == chat_input_wndproc_compat) {
    g_runtime.chat_input_hwnd = hwnd;
    InterlockedExchange(&g_runtime.chat_input_hooked, 1);
    return;
  }

  SetLastError(ERROR_SUCCESS);
  old_proc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)chat_input_wndproc_compat);
  error = GetLastError();
  if (old_proc == NULL && error != ERROR_SUCCESS) {
    if (InterlockedCompareExchange(&g_runtime.chat_input_hook_logged, 1, 0) == 0) {
      runtime_tracef("chat_input: wndproc install failed hwnd=0x%08lx error=%lu", (unsigned long)(uintptr_t)hwnd,
                     (unsigned long)error);
    }
    return;
  }

  g_runtime.chat_input_hwnd = hwnd;
  g_runtime.chat_input_old_wndproc = old_proc;
  InterlockedExchange(&g_runtime.chat_input_hooked, 1);
  runtime_tracef("chat_input: wndproc installed hwnd=0x%08lx old=0x%08lx", (unsigned long)(uintptr_t)hwnd,
                 (unsigned long)(uintptr_t)old_proc);
}

static LRESULT CALLBACK chat_input_wndproc_compat(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  const int active = InterlockedCompareExchange(&g_runtime.chat_input_active, 0, 0) != 0;
  const int dialog_active = dialog_compat_active();
  const int textdraw_select_active = InterlockedCompareExchange(&g_runtime.textdraw_select_active, 0, 0) != 0;

  if (!chat_input_enabled_compat() || !g_runtime.settings.play_online) {
    return chat_input_call_original_compat(hwnd, msg, wparam, lparam);
  }

  if ((msg == WM_KEYDOWN || msg == WM_KEYUP) && wparam == VK_F8) {
    if (msg == WM_KEYDOWN && (lparam & 0x40000000L) == 0) {
      screenshot_compat_request();
    }
    return 0;
  }

  if (dialog_active) {
    LONG style = InterlockedCompareExchange(&g_runtime.dialog_overlay_style, 0, 0);

    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK) {
      if (dialog_compat_handle_mouse(hwnd, msg, lparam)) {
        return 0;
      }
    }

    if (msg == WM_KEYUP) {
      return 0;
    }

    if (msg == WM_KEYDOWN) {
      if (wparam == VK_RETURN) {
        dialog_compat_submit(1u);
        return 0;
      }
      if (wparam == VK_ESCAPE) {
        dialog_compat_submit(0u);
        return 0;
      }
      if (dialog_compat_uses_list(style)) {
        if (wparam == VK_UP) {
          dialog_compat_move_selection(-1);
          return 0;
        }
        if (wparam == VK_DOWN) {
          dialog_compat_move_selection(1);
          return 0;
        }
        if (wparam == VK_PRIOR) {
          dialog_compat_move_selection(-SAMP_DIALOG_COMPAT_MAX_VISIBLE_ITEMS);
          return 0;
        }
        if (wparam == VK_NEXT) {
          dialog_compat_move_selection(SAMP_DIALOG_COMPAT_MAX_VISIBLE_ITEMS);
          return 0;
        }
      }
      if (dialog_compat_uses_input(style) && wparam == VK_BACK) {
        dialog_compat_backspace_input();
        return 0;
      }
      return 0;
    }

    if (msg == WM_CHAR) {
      if (dialog_compat_uses_input(style)) {
        if (wparam == '\b') {
          dialog_compat_backspace_input();
          return 0;
        }
        if (wparam == '\r' || wparam == '\n') {
          dialog_compat_submit(1u);
          return 0;
        }
        if (wparam == 27u) {
          dialog_compat_submit(0u);
          return 0;
        }
        if (wparam >= 32u && wparam <= 126u) {
          dialog_compat_append_input_char((char)wparam);
          return 0;
        }
      }
      return 0;
    }
  }

  if (textdraw_select_active) {
    if (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK) {
      if (textdraw_compat_handle_mouse(hwnd, msg, lparam)) {
        return 0;
      }
    }
    if (msg == WM_KEYDOWN && wparam == VK_ESCAPE) {
      textdraw_compat_clear_select_mode("escape_passthrough");
      return chat_input_call_original_compat(hwnd, msg, wparam, lparam);
    }
  }

  if (scoreboard_compat_handle_key(msg, wparam)) {
    return 0;
  }

  if (msg == WM_CHAR) {
    if (!active) {
      if (wparam == 't' || wparam == 'T' || wparam == '`') {
        chat_input_open_compat();
        return 0;
      }
      return chat_input_call_original_compat(hwnd, msg, wparam, lparam);
    }

    if (wparam == '\r' || wparam == '\n') {
      chat_input_submit_compat();
      return 0;
    }
    if (wparam == 27u) {
      chat_input_close_compat();
      return 0;
    }
    if (wparam == '\b') {
      chat_input_backspace_compat();
      return 0;
    }
    if (wparam >= 32u && wparam <= 126u) {
      chat_input_append_char_compat((char)wparam);
      return 0;
    }
    return 0;
  }

  if (msg == WM_KEYUP) {
    if (wparam == VK_F6) {
      if (active) {
        chat_input_close_compat();
      } else {
        chat_input_open_compat();
      }
      return 0;
    }
    if (active) {
      if (wparam == VK_ESCAPE) {
        chat_input_close_compat();
        return 0;
      }
      if (wparam == VK_RETURN) {
        chat_input_submit_compat();
        return 0;
      }
      if (wparam == VK_UP || wparam == VK_DOWN || wparam == VK_BACK) {
        return 0;
      }
      return 0;
    }
  }

  if (msg == WM_KEYDOWN && active) {
    if (wparam == VK_UP) {
      chat_input_recall_up_compat();
      return 0;
    }
    if (wparam == VK_DOWN) {
      chat_input_recall_down_compat();
      return 0;
    }
    if (wparam == VK_ESCAPE || wparam == VK_RETURN || wparam == VK_BACK || wparam == VK_LEFT ||
        wparam == VK_RIGHT || wparam == VK_HOME || wparam == VK_END) {
      return 0;
    }
    return 0;
  }

  return chat_input_call_original_compat(hwnd, msg, wparam, lparam);
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

static void screenshot_compat_release_unknown(void *unknown) {
  void **vtbl = NULL;
  samp_unknown_release_fn release_fn = NULL;

  if (unknown == NULL || !memory_is_readable_compat(unknown, sizeof(void **))) {
    return;
  }
  vtbl = *(void ***)unknown;
  if (vtbl == NULL || !memory_is_readable_compat(&vtbl[2], sizeof(void *)) || vtbl[2] == NULL) {
    return;
  }
  release_fn = (samp_unknown_release_fn)vtbl[2];
  (void)release_fn(unknown);
}

static int screenshot_compat_next_path(char *out_path, size_t out_size) {
  LONG start = InterlockedCompareExchange(&g_runtime.screenshot_count, 0, 0);
  LONG candidate = 0;
  int written = 0;
  int i = 0;

  if (out_path == NULL || out_size == 0u) {
    return 0;
  }
  out_path[0] = '\0';

  for (i = 0; i < 1000; ++i) {
    candidate = (start + i) % 1000;
    written = snprintf(out_path, out_size, "%ssa-mp-%03ld.png", g_runtime.module_dir, (long)candidate);
    if (written <= 0 || (size_t)written >= out_size) {
      return 0;
    }
    if (GetFileAttributesA(out_path) == INVALID_FILE_ATTRIBUTES) {
      InterlockedExchange(&g_runtime.screenshot_count, (candidate + 1) % 1000);
      return 1;
    }
  }

  candidate = start % 1000;
  written = snprintf(out_path, out_size, "%ssa-mp-%03ld.png", g_runtime.module_dir, (long)candidate);
  if (written <= 0 || (size_t)written >= out_size) {
    return 0;
  }
  InterlockedExchange(&g_runtime.screenshot_count, (candidate + 1) % 1000);
  return 1;
}

static void screenshot_compat_capture_if_requested(void *device) {
  void **vtbl = NULL;
  samp_d3d9_create_offscreen_plain_surface_fn create_surface_fn = NULL;
  samp_d3d9_get_front_buffer_data_fn get_front_buffer_fn = NULL;
  void *surface = NULL;
  HRESULT hr = E_FAIL;
  UINT width = 0u;
  UINT height = 0u;
  char path[MAX_PATH];
  RECT rect;
  RECT *save_rect = NULL;
  POINT point;
  HWND hwnd = NULL;

  if (InterlockedExchange(&g_runtime.screenshot_requested, 0) == 0) {
    return;
  }

  if (device == NULL || !memory_is_readable_compat(device, sizeof(void **)) ||
      !screenshot_compat_resolve_d3dx_save_surface()) {
    chat_compat_add_message("Unable to save screenshot.");
    runtime_tracef("screenshot: capture skipped device=0x%08lx", (unsigned long)(uintptr_t)device);
    return;
  }
  vtbl = *(void ***)device;
  if (vtbl == NULL ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_CREATE_OFFSCREEN_PLAIN_SURFACE_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_GET_FRONT_BUFFER_DATA_INDEX], sizeof(void *)) ||
      vtbl[SAMP_D3D9_CREATE_OFFSCREEN_PLAIN_SURFACE_INDEX] == NULL ||
      vtbl[SAMP_D3D9_GET_FRONT_BUFFER_DATA_INDEX] == NULL) {
    chat_compat_add_message("Unable to save screenshot.");
    runtime_tracef("screenshot: d3d vtbl missing device=0x%08lx", (unsigned long)(uintptr_t)device);
    return;
  }

  if (!screenshot_compat_next_path(path, sizeof(path))) {
    chat_compat_add_message("Unable to save screenshot.");
    runtime_tracef("screenshot: filename failed");
    return;
  }

  width = (UINT)GetSystemMetrics(SM_CXSCREEN);
  height = (UINT)GetSystemMetrics(SM_CYSCREEN);
  if (width == 0u || height == 0u) {
    width = 800u;
    height = 600u;
  }

  create_surface_fn = (samp_d3d9_create_offscreen_plain_surface_fn)vtbl[SAMP_D3D9_CREATE_OFFSCREEN_PLAIN_SURFACE_INDEX];
  get_front_buffer_fn = (samp_d3d9_get_front_buffer_data_fn)vtbl[SAMP_D3D9_GET_FRONT_BUFFER_DATA_INDEX];
  hr = create_surface_fn(device, width, height, SAMP_D3DFMT_A8R8G8B8, SAMP_D3DPOOL_SCRATCH, &surface, NULL);
  if (FAILED(hr) || surface == NULL) {
    chat_compat_add_message("Unable to save screenshot.");
    runtime_tracef("screenshot: CreateOffscreenPlainSurface failed hr=0x%08lx size=%ux%u", (unsigned long)hr,
                   (unsigned)width, (unsigned)height);
    return;
  }

  hr = get_front_buffer_fn(device, 0u, surface);
  if (SUCCEEDED(hr)) {
    hwnd = read_game_hwnd_compat();
    if (hwnd != NULL && GetClientRect(hwnd, &rect)) {
      point.x = 0;
      point.y = 0;
      if (ClientToScreen(hwnd, &point)) {
        rect.left += point.x;
        rect.right += point.x;
        rect.top += point.y;
        rect.bottom += point.y;
        save_rect = &rect;
      }
    }
    hr = g_runtime.d3dx_save_surface_to_file_a(path, SAMP_D3DXIFF_PNG, surface, NULL, save_rect);
  }
  screenshot_compat_release_unknown(surface);

  if (SUCCEEDED(hr)) {
    InterlockedExchange(&g_runtime.screenshot_fail_logged, 0);
    chat_compat_add_message("Screenshot Taken - %s", path);
    runtime_tracef("screenshot: saved '%s'", path);
  } else {
    chat_compat_add_message("Unable to save screenshot.");
    if (InterlockedCompareExchange(&g_runtime.screenshot_fail_logged, 1, 0) == 0) {
      runtime_tracef("screenshot: save failed hr=0x%08lx path='%s'", (unsigned long)hr, path);
    }
  }
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

static void chat_compat_draw_text_outline_segments(HDC dc, int x, int y, const char *text, DWORD default_argb_color) {
  const char *cursor = text;
  int draw_x = x;
  DWORD color = default_argb_color;

  if (dc == NULL || text == NULL || text[0] == '\0') {
    return;
  }

  while (*cursor != '\0') {
    char segment[SAMP_CHAT_COMPAT_LINE_BYTES];
    size_t len = 0u;
    DWORD tag_color = 0u;
    SIZE extent;

    if (chat_compat_parse_color_tag(cursor, &tag_color)) {
      color = tag_color;
      cursor += 8;
      continue;
    }

    while (cursor[len] != '\0' && !chat_compat_parse_color_tag(cursor + len, &tag_color) &&
           len + 1u < sizeof(segment)) {
      ++len;
    }
    if (len == 0u) {
      ++cursor;
      continue;
    }
    memcpy(segment, cursor, len);
    segment[len] = '\0';
    chat_compat_draw_text_outline(dc, draw_x, y, segment, color);
    memset(&extent, 0, sizeof(extent));
    if (GetTextExtentPoint32A(dc, segment, (int)len, &extent) && extent.cx > 0) {
      draw_x += extent.cx;
    } else {
      draw_x += (int)len * 8;
    }
    cursor += len;
  }
}

static void textdraw_compat_release_fonts(void) {
  int i = 0;

  for (i = 0; i < SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS; ++i) {
    samp_id3dx_font_compat *font = g_runtime.textdraw_d3dx_fonts[i];
    if (font != NULL && font->lpVtbl != NULL && font->lpVtbl->Release != NULL) {
      font->lpVtbl->Release(font);
    }
    g_runtime.textdraw_d3dx_fonts[i] = NULL;
  }
  g_runtime.textdraw_d3d_device = NULL;
}

static void chat_compat_release_d3dx_font(void) {
  samp_id3dx_font_compat *font = g_runtime.chat_d3dx_font;
  if (font != NULL && font->lpVtbl != NULL && font->lpVtbl->Release != NULL) {
    font->lpVtbl->Release(font);
  }
  g_runtime.chat_d3dx_font = NULL;
  g_runtime.chat_d3d_device = NULL;
  scoreboard_compat_release_font();
  textdraw_compat_release_fonts();
  loading_screen_compat_release_texture();
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

static int screenshot_compat_resolve_d3dx_save_surface(void) {
  FARPROC proc = NULL;

  if (g_runtime.d3dx_save_surface_to_file_a != NULL) {
    return 1;
  }
  if (g_runtime.d3dx9_module == NULL) {
    g_runtime.d3dx9_module = LoadLibraryA("d3dx9_25.dll");
    runtime_tracef("screenshot: d3dx9_25 load %s", (g_runtime.d3dx9_module != NULL) ? "OK" : "FAILED");
  }
  if (g_runtime.d3dx9_module == NULL) {
    return 0;
  }

  proc = GetProcAddress(g_runtime.d3dx9_module, "D3DXSaveSurfaceToFileA");
  if (proc == NULL) {
    if (InterlockedCompareExchange(&g_runtime.screenshot_fail_logged, 1, 0) == 0) {
      runtime_tracef("screenshot: D3DXSaveSurfaceToFileA missing");
    }
    return 0;
  }
  g_runtime.d3dx_save_surface_to_file_a = (samp_d3dx_save_surface_to_file_a_fn)proc;
  return 1;
}

static int loading_screen_compat_resolve_d3dx_create_texture(void) {
  FARPROC proc = NULL;

  if (g_runtime.d3dx_create_texture_from_file_a != NULL) {
    return 1;
  }
  if (g_runtime.d3dx9_module == NULL) {
    g_runtime.d3dx9_module = LoadLibraryA("d3dx9_25.dll");
    runtime_tracef("loading_screen: d3dx9_25 load %s", (g_runtime.d3dx9_module != NULL) ? "OK" : "FAILED");
  }
  if (g_runtime.d3dx9_module == NULL) {
    return 0;
  }

  proc = GetProcAddress(g_runtime.d3dx9_module, "D3DXCreateTextureFromFileA");
  if (proc == NULL) {
    if (InterlockedCompareExchange(&g_runtime.loading_screen_fail_logged, 1, 0) == 0) {
      runtime_tracef("loading_screen: D3DXCreateTextureFromFileA missing");
    }
    return 0;
  }
  g_runtime.d3dx_create_texture_from_file_a = (samp_d3dx_create_texture_from_file_a_fn)proc;
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

static int preconnect_move_ped_compat(void) {
  static LONG initialized = 0;
  static int enabled = 0;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0)) {
    return enabled;
  }

  value = getenv("SAMPDLL_PRECONNECT_MOVE_PED");
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

static void chat_compat_viewport_rect(int *out_x, int *out_y, int *out_w, int *out_h) {
  HWND hwnd = NULL;
  RECT client_rect;
  int client_w = 800;
  int client_h = 600;
  int viewport_x = 0;
  int viewport_w = 800;

  hwnd = read_game_hwnd_compat();
  if (hwnd != NULL && GetClientRect(hwnd, &client_rect)) {
    client_w = client_rect.right - client_rect.left;
    client_h = client_rect.bottom - client_rect.top;
    if (client_w <= 0) {
      client_w = 800;
    }
    if (client_h <= 0) {
      client_h = 600;
    }
  }

  viewport_w = client_h * 4 / 3;
  if (viewport_w > 0 && viewport_w < client_w) {
    viewport_x = (client_w - viewport_w) / 2;
  } else {
    viewport_w = client_w;
  }

  if (out_x != NULL) {
    *out_x = viewport_x;
  }
  if (out_y != NULL) {
    *out_y = 0;
  }
  if (out_w != NULL) {
    *out_w = viewport_w;
  }
  if (out_h != NULL) {
    *out_h = client_h;
  }
}

static void chat_compat_d3dx_draw_text(samp_id3dx_font_compat *font, RECT rect, const char *text, DWORD argb_color,
                                       DWORD flags) {
  if (font == NULL || font->lpVtbl == NULL || font->lpVtbl->DrawTextA == NULL || text == NULL || text[0] == '\0') {
    return;
  }
  font->lpVtbl->DrawTextA(font, NULL, text, -1, &rect, flags, argb_color);
}

static int chat_compat_d3dx_measure_text_width(samp_id3dx_font_compat *font, const char *text, int fallback_width);

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

static void chat_compat_d3dx_draw_text_outline_segments(samp_id3dx_font_compat *font, RECT rect, const char *text,
                                                        DWORD default_argb_color) {
  const char *cursor = text;
  DWORD color = default_argb_color;

  if (font == NULL || font->lpVtbl == NULL || font->lpVtbl->DrawTextA == NULL || text == NULL || text[0] == '\0') {
    return;
  }

  while (*cursor != '\0') {
    char segment[SAMP_CHAT_COMPAT_LINE_BYTES];
    size_t len = 0u;
    DWORD tag_color = 0u;
    int width = 0;

    if (chat_compat_parse_color_tag(cursor, &tag_color)) {
      color = tag_color;
      cursor += 8;
      continue;
    }

    while (cursor[len] != '\0' && !chat_compat_parse_color_tag(cursor + len, &tag_color) &&
           len + 1u < sizeof(segment)) {
      ++len;
    }
    if (len == 0u) {
      ++cursor;
      continue;
    }
    memcpy(segment, cursor, len);
    segment[len] = '\0';
    chat_compat_d3dx_draw_text_outline(font, rect, segment, color);
    width = chat_compat_d3dx_measure_text_width(font, segment, (int)len * 8);
    rect.left += width;
    rect.right += width;
    cursor += len;
  }
}

static int chat_compat_d3dx_measure_raw_text_width(samp_id3dx_font_compat *font, const char *text,
                                                   int fallback_width) {
  RECT rect;

  if (font == NULL || font->lpVtbl == NULL || font->lpVtbl->DrawTextA == NULL || text == NULL || text[0] == '\0') {
    return fallback_width > 0 ? fallback_width : 0;
  }
  rect.left = 0;
  rect.top = 0;
  rect.right = 4096;
  rect.bottom = 64;
  font->lpVtbl->DrawTextA(font, NULL, text, -1, &rect, DT_CALCRECT | DT_NOCLIP | DT_SINGLELINE | DT_LEFT,
                          0xFFFFFFFFu);
  if (rect.right > rect.left) {
    return (int)(rect.right - rect.left);
  }
  return fallback_width > 0 ? fallback_width : 0;
}

static int chat_compat_d3dx_measure_space_width(samp_id3dx_font_compat *font) {
  int with_space = chat_compat_d3dx_measure_raw_text_width(font, "x x", 18);
  int without_space = chat_compat_d3dx_measure_raw_text_width(font, "xx", 12);
  int width = with_space - without_space;

  if (width < 3) {
    return 4;
  }
  if (width > 16) {
    return 8;
  }
  return width;
}

static int chat_compat_d3dx_measure_text_width(samp_id3dx_font_compat *font, const char *text, int fallback_width) {
  size_t len = 0u;
  size_t left = 0u;
  size_t right = 0u;
  int edge_space_units = 0;
  int width = 0;

  if (font == NULL || font->lpVtbl == NULL || font->lpVtbl->DrawTextA == NULL || text == NULL || text[0] == '\0') {
    return fallback_width > 0 ? fallback_width : 0;
  }

  width = chat_compat_d3dx_measure_raw_text_width(font, text, fallback_width);

  /* INFERRED + TODO_VERIFY:
   * D3DX DrawText(DT_CALCRECT) can trim leading/trailing whitespace. Chat color
   * segments often split exactly around spaces, so restore that advance locally
   * to avoid rendering "command /mode" as "command/mode".
   */
  len = strlen(text);
  while (left < len && (text[left] == ' ' || text[left] == '\t')) {
    edge_space_units += text[left] == '\t' ? 4 : 1;
    ++left;
  }
  right = len;
  while (right > left && (text[right - 1u] == ' ' || text[right - 1u] == '\t')) {
    edge_space_units += text[right - 1u] == '\t' ? 4 : 1;
    --right;
  }
  if (edge_space_units > 0) {
    width += edge_space_units * chat_compat_d3dx_measure_space_width(font);
  }
  return width;
}

static void dialog_compat_d3d_fill_rect(void *device, int x, int y, int w, int h, DWORD argb_color) {
  void **vtbl = NULL;
  samp_d3d9_clear_fn clear_fn = NULL;
  samp_d3drect_compat rect;

  if (device == NULL || w <= 0 || h <= 0 || x < 0 || y < 0) {
    return;
  }
  if (!memory_is_readable_compat(device, sizeof(void **))) {
    return;
  }
  vtbl = *(void ***)device;
  if (vtbl == NULL || !memory_is_readable_compat(&vtbl[SAMP_D3D9_CLEAR_INDEX], sizeof(void *)) ||
      vtbl[SAMP_D3D9_CLEAR_INDEX] == NULL) {
    return;
  }

  rect.x1 = (LONG)x;
  rect.y1 = (LONG)y;
  rect.x2 = (LONG)(x + w);
  rect.y2 = (LONG)(y + h);
  clear_fn = (samp_d3d9_clear_fn)vtbl[SAMP_D3D9_CLEAR_INDEX];
  (void)clear_fn(device, 1u, &rect, SAMP_D3DCLEAR_TARGET, argb_color, 1.0f, 0u);
}

static int dialog_compat_d3d_alpha_rect(void *device, int x, int y, int w, int h, DWORD argb_color) {
  void **vtbl = NULL;
  samp_d3d9_set_render_state_fn set_render_state = NULL;
  samp_d3d9_set_fvf_fn set_fvf = NULL;
  samp_d3d9_draw_primitive_up_fn draw_primitive_up = NULL;
  samp_d3d9_overlay_vertex_compat vertices[4];

  if (device == NULL || w <= 0 || h <= 0 || x < 0 || y < 0) {
    return 0;
  }
  if (!memory_is_readable_compat(device, sizeof(void **))) {
    return 0;
  }
  vtbl = *(void ***)device;
  if (vtbl == NULL || !memory_is_readable_compat(&vtbl[SAMP_D3D9_SET_RENDER_STATE_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_SET_FVF_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_DRAW_PRIMITIVE_UP_INDEX], sizeof(void *)) ||
      vtbl[SAMP_D3D9_SET_RENDER_STATE_INDEX] == NULL || vtbl[SAMP_D3D9_SET_FVF_INDEX] == NULL ||
      vtbl[SAMP_D3D9_DRAW_PRIMITIVE_UP_INDEX] == NULL) {
    return 0;
  }

  vertices[0].x = (float)x;
  vertices[0].y = (float)y;
  vertices[0].z = 0.0f;
  vertices[0].rhw = 1.0f;
  vertices[0].color = argb_color;
  vertices[1].x = (float)(x + w);
  vertices[1].y = (float)y;
  vertices[1].z = 0.0f;
  vertices[1].rhw = 1.0f;
  vertices[1].color = argb_color;
  vertices[2].x = (float)x;
  vertices[2].y = (float)(y + h);
  vertices[2].z = 0.0f;
  vertices[2].rhw = 1.0f;
  vertices[2].color = argb_color;
  vertices[3].x = (float)(x + w);
  vertices[3].y = (float)(y + h);
  vertices[3].z = 0.0f;
  vertices[3].rhw = 1.0f;
  vertices[3].color = argb_color;

  set_render_state = (samp_d3d9_set_render_state_fn)vtbl[SAMP_D3D9_SET_RENDER_STATE_INDEX];
  set_fvf = (samp_d3d9_set_fvf_fn)vtbl[SAMP_D3D9_SET_FVF_INDEX];
  draw_primitive_up = (samp_d3d9_draw_primitive_up_fn)vtbl[SAMP_D3D9_DRAW_PRIMITIVE_UP_INDEX];
  (void)set_render_state(device, SAMP_D3DRS_ZENABLE, 0u);
  (void)set_render_state(device, SAMP_D3DRS_ALPHABLENDENABLE, 1u);
  (void)set_render_state(device, SAMP_D3DRS_SRCBLEND, SAMP_D3DBLEND_SRCALPHA);
  (void)set_render_state(device, SAMP_D3DRS_DESTBLEND, SAMP_D3DBLEND_INVSRCALPHA);
  (void)set_fvf(device, SAMP_D3DFVF_XYZRHW_DIFFUSE);
  return SUCCEEDED(draw_primitive_up(device, SAMP_D3DPT_TRIANGLESTRIP, 2u, vertices, sizeof(vertices[0])));
}

static void textdraw_compat_d3d_fill_rect(void *device, int x, int y, int w, int h, DWORD argb_color) {
  DWORD alpha = argb_color & 0xFF000000u;

  /* OLD_02X_REF + GTA_REVERSED_REF + TODO_VERIFY:
   * CFont/CSprite2d textdraw boxes are alpha-blended. IDirect3DDevice9::Clear
   * ignores that blend state, so the D3DX fallback must use a primitive path
   * for translucent TextDrawBoxColour/BackgroundColour values.
   */
  if (alpha == 0u) {
    return;
  }
  if (alpha != 0xFF000000u && dialog_compat_d3d_alpha_rect(device, x, y, w, h, argb_color)) {
    return;
  }
  dialog_compat_d3d_fill_rect(device, x, y, w, h, argb_color);
}

static int scoreboard_compat_connected(void) {
  LONG net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);

  if (net_state != SAMP_NETGAME_CONNECTED) {
    return 0;
  }
  if (g_runtime.net_mgr.raknet_client == NULL || !samp_raknet_client_is_connected(g_runtime.net_mgr.raknet_client)) {
    return 0;
  }
  return 1;
}

static int scoreboard_compat_available(void) {
  LONG net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);

  if (!chat_input_enabled_compat() || !g_runtime.settings.play_online || net_state == SAMP_NETGAME_FAILED) {
    return 0;
  }
  return 1;
}

static int scoreboard_compat_active(void) {
  if (!scoreboard_compat_available()) {
    return 0;
  }
  if (InterlockedCompareExchange(&g_runtime.chat_input_active, 0, 0) != 0 || dialog_compat_active() ||
      InterlockedCompareExchange(&g_runtime.textdraw_select_active, 0, 0) != 0) {
    return 0;
  }
  return (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
}

static int scoreboard_compat_handle_key(UINT msg, WPARAM wparam) {
  if (!scoreboard_compat_available()) {
    return 0;
  }
  if (InterlockedCompareExchange(&g_runtime.chat_input_active, 0, 0) != 0 || dialog_compat_active() ||
      InterlockedCompareExchange(&g_runtime.textdraw_select_active, 0, 0) != 0) {
    return 0;
  }

  if (msg == WM_CHAR && wparam == '\t') {
    return 1;
  }
  if (msg != WM_KEYDOWN && msg != WM_KEYUP) {
    return 0;
  }
  if (wparam == VK_TAB) {
    return 1;
  }
  if ((GetAsyncKeyState(VK_TAB) & 0x8000) != 0) {
    if (wparam == VK_PRIOR) {
      LONG offset = InterlockedCompareExchange(&g_runtime.scoreboard_offset, 0, 0);
      offset -= SAMP_SCOREBOARD_MAX_VISIBLE;
      if (offset < 0) {
        offset = 0;
      }
      InterlockedExchange(&g_runtime.scoreboard_offset, offset);
      return 1;
    }
    if (wparam == VK_NEXT) {
      LONG offset = InterlockedCompareExchange(&g_runtime.scoreboard_offset, 0, 0);
      LONG count = InterlockedCompareExchange(&g_runtime.scoreboard_player_count, 0, 0);
      offset += SAMP_SCOREBOARD_MAX_VISIBLE;
      if (count <= SAMP_SCOREBOARD_MAX_VISIBLE) {
        offset = 0;
      } else if (offset >= count) {
        offset = count - SAMP_SCOREBOARD_MAX_VISIBLE;
      }
      if (offset < 0) {
        offset = 0;
      }
      InterlockedExchange(&g_runtime.scoreboard_offset, offset);
      return 1;
    }
  }
  return 0;
}

static void scoreboard_compat_release_font(void) {
  samp_id3dx_font_compat *font = g_runtime.scoreboard_d3dx_font;

  if (font != NULL && font->lpVtbl != NULL && font->lpVtbl->Release != NULL) {
    font->lpVtbl->Release(font);
  }
  g_runtime.scoreboard_d3dx_font = NULL;
  g_runtime.scoreboard_d3d_device = NULL;
}

static int scoreboard_compat_ensure_font(void *device) {
  HRESULT hr = 0;
  samp_id3dx_font_compat *font = NULL;

  if (device == NULL) {
    return 0;
  }
  if (g_runtime.scoreboard_d3dx_font != NULL && g_runtime.scoreboard_d3d_device == device) {
    return 1;
  }

  scoreboard_compat_release_font();
  if (!chat_compat_resolve_d3dx_create_font()) {
    return 0;
  }

  hr = g_runtime.d3dx_create_font_a(device, SAMP_SCOREBOARD_FONT_SIZE, 0u, FW_BOLD, 1u, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial",
                                    &font);
  if (FAILED(hr) || font == NULL) {
    if (InterlockedCompareExchange(&g_runtime.scoreboard_d3d_font_fail_logged, 1, 0) == 0) {
      runtime_tracef("scoreboard_d3dx: font create failed hr=0x%08lx device=0x%08lx", (unsigned long)hr,
                     (unsigned long)(uintptr_t)device);
    }
    return 0;
  }

  g_runtime.scoreboard_d3dx_font = font;
  g_runtime.scoreboard_d3d_device = device;
  InterlockedExchange(&g_runtime.scoreboard_d3d_font_fail_logged, 0);
  runtime_tracef("scoreboard_d3dx: font created device=0x%08lx size=%d evidence=OLD_02X_REF,TODO_VERIFY",
                 (unsigned long)(uintptr_t)device, SAMP_SCOREBOARD_FONT_SIZE);
  return 1;
}

static const char *scoreboard_compat_local_name(void) {
  if (g_runtime.raknet_join_profile.nickname[0] != '\0') {
    return g_runtime.raknet_join_profile.nickname;
  }
  if (g_runtime.settings.nickname[0] != '\0') {
    return g_runtime.settings.nickname;
  }
  return SAMP_DEFAULT_NICKNAME;
}

static samp_scoreboard_player_compat *scoreboard_compat_player_slot(uint16_t player_id) {
  if (player_id >= SAMP_SCOREBOARD_MAX_PLAYERS) {
    return NULL;
  }
  return &g_runtime.scoreboard_players[player_id];
}

static int scoreboard_compat_player_visible(const samp_scoreboard_player_compat *slot) {
  return slot != NULL && slot->active != 0 && slot->is_npc == 0u;
}

static int scoreboard_compat_recount_visible_players(void) {
  int count = 0;
  uint16_t player_id = 0u;

  for (player_id = 0u; player_id < SAMP_SCOREBOARD_MAX_PLAYERS; ++player_id) {
    if (scoreboard_compat_player_visible(&g_runtime.scoreboard_players[player_id])) {
      ++count;
    }
  }
  InterlockedExchange(&g_runtime.scoreboard_player_count, (LONG)count);
  return count;
}

static int scoreboard_compat_slot_name_conflicts_with_local(const samp_scoreboard_player_compat *slot) {
  const char *local_name = scoreboard_compat_local_name();

  return slot != NULL && slot->active != 0 && slot->is_npc == 0u && slot->name[0] != '\0' &&
         strcmp(slot->name, local_name) != 0;
}

static int scoreboard_compat_set_local_player_id(uint16_t player_id, const char *source) {
  LONG previous_valid = 0;
  LONG previous_id = 0;

  if (player_id >= SAMP_SCOREBOARD_MAX_PLAYERS) {
    return 0;
  }

  previous_valid = InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id_valid, 0, 0);
  previous_id = InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id, 0, 0);
  if (previous_valid != 0 && previous_id == (LONG)player_id) {
    return 1;
  }

  InterlockedExchange(&g_runtime.scoreboard_local_player_id, (LONG)player_id);
  InterlockedExchange(&g_runtime.scoreboard_local_player_id_valid, 1);
  runtime_tracef("scoreboard: local_player_id=%u source=%s previous=%ld previous_valid=%ld "
                 "evidence=PROBE_TRACE,INFERRED,TODO_VERIFY",
                 (unsigned)player_id, source != NULL ? source : "unknown", (long)previous_id,
                 (long)previous_valid);
  return 1;
}

static samp_scoreboard_player_compat *scoreboard_compat_ensure_local_player(void) {
  LONG local_valid = InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id_valid, 0, 0);
  LONG local_id = InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id, 0, 0);
  uint16_t player_id = 0u;
  samp_scoreboard_player_compat *slot = NULL;
  const char *name = scoreboard_compat_local_name();

  if (local_valid == 0 || local_id < 0 || local_id >= (LONG)SAMP_SCOREBOARD_MAX_PLAYERS) {
    return NULL;
  }

  player_id = (uint16_t)local_id;
  slot = scoreboard_compat_player_slot(player_id);
  if (slot == NULL) {
    return NULL;
  }
  if (scoreboard_compat_slot_name_conflicts_with_local(slot)) {
    runtime_tracef("scoreboard: local slot conflict id=%u existing='%s' local='%s' skipped=1 "
                   "evidence=PROBE_TRACE,TODO_VERIFY",
                   (unsigned)player_id, slot->name, name);
    return NULL;
  }

  InterlockedExchange(&slot->active, 1);
  slot->player_id = player_id;
  slot->is_npc = 0u;
  if (slot->color == 0u) {
    slot->color = g_runtime.raknet_player_color;
  }
  if (slot->name[0] == '\0') {
    strncpy(slot->name, name, sizeof(slot->name) - 1u);
    slot->name[sizeof(slot->name) - 1u] = '\0';
  }
  return slot;
}

static int scoreboard_compat_local_id(void) {
  LONG local_valid = InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id_valid, 0, 0);
  LONG local_id = InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id, 0, 0);

  if (local_valid == 0 || local_id < 0 || local_id >= (LONG)SAMP_SCOREBOARD_MAX_PLAYERS) {
    return -1;
  }
  return (int)local_id;
}

static void scoreboard_compat_update_from_snapshot(const samp_raknet_rpc_probe_snapshot *snapshot) {
  uint32_t previous_event_seq = 0u;
  uint32_t latest_event_seq = 0u;
  uint32_t previous_score_seq = 0u;
  uint32_t i = 0u;

  if (snapshot == NULL) {
    return;
  }

  if (snapshot->auth_local_player_id_valid != 0u &&
      snapshot->auth_local_player_id < SAMP_SCOREBOARD_MAX_PLAYERS) {
    scoreboard_compat_set_local_player_id(snapshot->auth_local_player_id, "auth_player_index");
  } else if ((snapshot->flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) != 0u &&
             snapshot->init_local_player_id < SAMP_SCOREBOARD_MAX_PLAYERS) {
    scoreboard_compat_set_local_player_id(snapshot->init_local_player_id, "init_game");
  } else if (snapshot->player_color_seq != 0u &&
             snapshot->player_color_player_id < SAMP_SCOREBOARD_MAX_PLAYERS) {
    const samp_scoreboard_player_compat *slot =
        scoreboard_compat_player_slot(snapshot->player_color_player_id);
    if (!scoreboard_compat_slot_name_conflicts_with_local(slot)) {
      scoreboard_compat_set_local_player_id(snapshot->player_color_player_id, "player_color_rpc");
    }
  }

  if ((snapshot->flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) != 0u) {
    scoreboard_compat_ensure_local_player();
  }

  previous_event_seq = (uint32_t)InterlockedCompareExchange(&g_runtime.scoreboard_player_pool_event_seq, 0, 0);
  latest_event_seq = previous_event_seq;
  if (snapshot->player_pool_event_count > 0u) {
    uint32_t count = snapshot->player_pool_event_count;
    if (count > SAMP_RAKNET_PLAYER_POOL_EVENT_RING) {
      count = SAMP_RAKNET_PLAYER_POOL_EVENT_RING;
    }
    for (i = 0u; i < count; ++i) {
      const samp_raknet_player_pool_event *event = &snapshot->player_pool_events[i];
      samp_scoreboard_player_compat *slot = NULL;

      if (event->seq == 0u || event->seq <= previous_event_seq) {
        continue;
      }
      if (event->seq > latest_event_seq) {
        latest_event_seq = event->seq;
      }
      slot = scoreboard_compat_player_slot(event->player_id);
      if (slot == NULL) {
        runtime_tracef("scoreboard: ignored player_pool_event action=%u id=%u out_of_range evidence=PROBE_TRACE",
                       (unsigned)event->action, (unsigned)event->player_id);
        continue;
      }

      if (event->action == SAMP_RAKNET_PLAYER_POOL_ACTION_JOIN) {
        InterlockedExchange(&slot->active, 1);
        slot->player_id = event->player_id;
        slot->is_npc = event->is_npc;
        slot->color = event->color;
        if (event->name[0] != '\0') {
          strncpy(slot->name, event->name, sizeof(slot->name) - 1u);
          slot->name[sizeof(slot->name) - 1u] = '\0';
        } else if (slot->name[0] == '\0') {
          (void)snprintf(slot->name, sizeof(slot->name), "Player%u", (unsigned)event->player_id);
          slot->name[sizeof(slot->name) - 1u] = '\0';
        }
      } else if (event->action == SAMP_RAKNET_PLAYER_POOL_ACTION_QUIT) {
        memset(slot, 0, sizeof(*slot));
        slot->player_id = event->player_id;
      }
    }
  }
  if (latest_event_seq != previous_event_seq) {
    InterlockedExchange(&g_runtime.scoreboard_player_pool_event_seq, (LONG)latest_event_seq);
  }

  previous_score_seq = (uint32_t)InterlockedCompareExchange(&g_runtime.scoreboard_score_ping_seq, 0, 0);
  if (snapshot->score_ping_seq != 0u && snapshot->score_ping_seq != previous_score_seq) {
    uint32_t count = snapshot->score_ping_count;
    if (count > SAMP_RAKNET_SCORE_PING_MAX_ENTRIES) {
      count = SAMP_RAKNET_SCORE_PING_MAX_ENTRIES;
    }
    for (i = 0u; i < count; ++i) {
      const samp_raknet_score_ping_entry *entry = &snapshot->score_ping_entries[i];
      samp_scoreboard_player_compat *slot = scoreboard_compat_player_slot(entry->player_id);
      if (slot == NULL) {
        continue;
      }
      if (InterlockedCompareExchange(&slot->active, 0, 0) == 0 &&
          InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id_valid, 0, 0) != 0 &&
          entry->player_id == (uint16_t)InterlockedCompareExchange(&g_runtime.scoreboard_local_player_id, 0, 0)) {
        slot = scoreboard_compat_ensure_local_player();
      }
      if (slot == NULL || InterlockedCompareExchange(&slot->active, 0, 0) == 0) {
        continue;
      }
      slot->score = entry->score;
      slot->ping = entry->ping;
    }
    InterlockedExchange(&g_runtime.scoreboard_score_ping_seq, (LONG)snapshot->score_ping_seq);
  }

  if (scoreboard_compat_recount_visible_players() == 0 &&
      (snapshot->flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) != 0u) {
    scoreboard_compat_ensure_local_player();
    scoreboard_compat_recount_visible_players();
  }
}

static void scoreboard_compat_draw_border(void *device, int x, int y, int w, int h, DWORD color) {
  dialog_compat_d3d_fill_rect(device, x, y, w, 1, color);
  dialog_compat_d3d_fill_rect(device, x, y + h - 1, w, 1, color);
  dialog_compat_d3d_fill_rect(device, x, y, 1, h, color);
  dialog_compat_d3d_fill_rect(device, x + w - 1, y, 1, h, color);
}

static void scoreboard_compat_draw_text_shadowed(samp_id3dx_font_compat *font, RECT rect, const char *text,
                                                 DWORD color, DWORD flags) {
  RECT shadow = rect;

  shadow.left += 1;
  shadow.top += 1;
  shadow.right += 1;
  shadow.bottom += 1;
  chat_compat_d3dx_draw_text(font, shadow, text, 0xFF000000u, flags);
  chat_compat_d3dx_draw_text(font, rect, text, color, flags);
}

static int scoreboard_compat_draw_d3dx_overlay(void *device) {
  samp_id3dx_font_compat *font = NULL;
  int viewport_x = 0;
  int viewport_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  int panel_w = 0;
  int panel_h = 0;
  int panel_x = 0;
  int panel_y = 0;
  int row_y = 0;
  int col_id = 0;
  int col_name = 0;
  int col_score = 0;
  int col_ping = 0;
  int connected = 0;
  int player_count = 0;
  int offset = 0;
  int first_visible = 0;
  int last_visible = 0;
  int visible_index = 0;
  int drawn_rows = 0;
  uint16_t player_id = 0u;
  RECT rect;
  char players_text[64];
  char value[64];
  const char *host = NULL;

  if (!scoreboard_compat_active() || !scoreboard_compat_ensure_font(device)) {
    return 0;
  }

  font = g_runtime.scoreboard_d3dx_font;
  connected = scoreboard_compat_connected();
  if (connected) {
    scoreboard_compat_ensure_local_player();
  }
  player_count = scoreboard_compat_recount_visible_players();
  offset = (int)InterlockedCompareExchange(&g_runtime.scoreboard_offset, 0, 0);
  if (offset < 0 || player_count <= SAMP_SCOREBOARD_MAX_VISIBLE) {
    offset = 0;
  } else if (offset >= player_count) {
    offset = player_count - SAMP_SCOREBOARD_MAX_VISIBLE;
  }
  InterlockedExchange(&g_runtime.scoreboard_offset, (LONG)offset);
  first_visible = player_count > 0 ? offset + 1 : 0;
  last_visible = offset + SAMP_SCOREBOARD_MAX_VISIBLE;
  if (last_visible > player_count) {
    last_visible = player_count;
  }

  chat_compat_viewport_rect(&viewport_x, &viewport_y, &viewport_w, &viewport_h);
  if (viewport_w <= 0 || viewport_h <= 0) {
    return 0;
  }

  panel_w = viewport_w - 80;
  if (panel_w > 900) {
    panel_w = 900;
  }
  if (panel_w < 560) {
    panel_w = viewport_w - 24;
  }
  panel_h = viewport_h - 90;
  if (panel_h > 560) {
    panel_h = 560;
  }
  if (panel_h < 300) {
    panel_h = viewport_h - 24;
  }
  panel_x = viewport_x + ((viewport_w - panel_w) / 2);
  panel_y = viewport_y + ((viewport_h - panel_h) / 2);
  if (panel_x < 0) {
    panel_x = 0;
  }
  if (panel_y < 0) {
    panel_y = 0;
  }

  if (!dialog_compat_d3d_alpha_rect(device, panel_x, panel_y, panel_w, panel_h, SAMP_SCOREBOARD_COLOR_PANEL)) {
    dialog_compat_d3d_fill_rect(device, panel_x, panel_y, panel_w, panel_h, 0xFF000000u);
  }
  dialog_compat_d3d_fill_rect(device, panel_x, panel_y, panel_w, 42, SAMP_SCOREBOARD_COLOR_HEADER);
  scoreboard_compat_draw_border(device, panel_x, panel_y, panel_w, panel_h, SAMP_SCOREBOARD_COLOR_GRID);

  host = g_runtime.raknet_init_hostname[0] != '\0' ? g_runtime.raknet_init_hostname : "SA-MP 0.3.7-R5";
  if (connected && player_count > 0) {
    (void)snprintf(players_text, sizeof(players_text), "Players: %d-%d of %d", first_visible, last_visible,
                   player_count);
  } else {
    (void)snprintf(players_text, sizeof(players_text), "Players: 0-0 of 0");
  }
  players_text[sizeof(players_text) - 1u] = '\0';

  rect.left = panel_x + 14;
  rect.top = panel_y + 8;
  rect.right = panel_x + panel_w - 14;
  rect.bottom = panel_y + 34;
  scoreboard_compat_draw_text_shadowed(font, rect, host, 0xFFFF0000u, DT_SINGLELINE | DT_LEFT | DT_NOCLIP);
  scoreboard_compat_draw_text_shadowed(font, rect, players_text, SAMP_SCOREBOARD_COLOR_MUTED,
                                       DT_SINGLELINE | DT_RIGHT | DT_NOCLIP);

  col_id = panel_x + 18;
  col_name = panel_x + 78;
  col_score = panel_x + panel_w - 230;
  col_ping = panel_x + panel_w - 112;

  row_y = panel_y + 48;
  rect.left = col_id;
  rect.top = row_y;
  rect.right = col_name - 10;
  rect.bottom = row_y + SAMP_SCOREBOARD_LINE_HEIGHT;
  scoreboard_compat_draw_text_shadowed(font, rect, "id", SAMP_SCOREBOARD_COLOR_LABEL,
                                       DT_SINGLELINE | DT_CENTER | DT_NOCLIP);
  rect.left = col_name;
  rect.right = col_score - 10;
  scoreboard_compat_draw_text_shadowed(font, rect, "name", SAMP_SCOREBOARD_COLOR_LABEL,
                                       DT_SINGLELINE | DT_LEFT | DT_NOCLIP);
  rect.left = col_score;
  rect.right = col_ping - 10;
  scoreboard_compat_draw_text_shadowed(font, rect, "score", SAMP_SCOREBOARD_COLOR_LABEL,
                                       DT_SINGLELINE | DT_CENTER | DT_NOCLIP);
  rect.left = col_ping;
  rect.right = panel_x + panel_w - 18;
  scoreboard_compat_draw_text_shadowed(font, rect, "ping", SAMP_SCOREBOARD_COLOR_LABEL,
                                       DT_SINGLELINE | DT_CENTER | DT_NOCLIP);

  row_y += SAMP_SCOREBOARD_LINE_HEIGHT + 8;
  dialog_compat_d3d_fill_rect(device, panel_x + 10, row_y - 5, panel_w - 20, 1, SAMP_SCOREBOARD_COLOR_GRID);
  if (!connected) {
    if (InterlockedCompareExchange(&g_runtime.scoreboard_logged, 1, 0) == 0) {
      runtime_tracef("scoreboard: drawing enabled preconnect evidence=OLD_02X_REF TODO_VERIFY remote_pool=decoded");
    }
    return 1;
  }

  for (player_id = 0u; player_id < SAMP_SCOREBOARD_MAX_PLAYERS; ++player_id) {
    const samp_scoreboard_player_compat *slot = &g_runtime.scoreboard_players[player_id];
    DWORD row_color = 0xFFFFFFFFu;

    if (!scoreboard_compat_player_visible(slot)) {
      continue;
    }
    if (visible_index++ < offset) {
      continue;
    }
    if (drawn_rows >= SAMP_SCOREBOARD_MAX_VISIBLE) {
      break;
    }
    row_color = slot->color != 0u ? chat_compat_samp_color_to_argb(slot->color) : 0xFFFFFFFFu;

    rect.left = col_id;
    rect.top = row_y;
    rect.right = col_name - 10;
    rect.bottom = row_y + SAMP_SCOREBOARD_LINE_HEIGHT;
    (void)snprintf(value, sizeof(value), "%u", (unsigned)slot->player_id);
    value[sizeof(value) - 1u] = '\0';
    scoreboard_compat_draw_text_shadowed(font, rect, value, row_color, DT_SINGLELINE | DT_CENTER | DT_NOCLIP);

    rect.left = col_name;
    rect.right = col_score - 10;
    scoreboard_compat_draw_text_shadowed(font, rect, slot->name[0] != '\0' ? slot->name : "Player",
                                         row_color, DT_SINGLELINE | DT_LEFT | DT_NOCLIP);

    rect.left = col_score;
    rect.right = col_ping - 10;
    (void)snprintf(value, sizeof(value), "%ld", (long)slot->score);
    value[sizeof(value) - 1u] = '\0';
    scoreboard_compat_draw_text_shadowed(font, rect, value, row_color, DT_SINGLELINE | DT_CENTER | DT_NOCLIP);

    rect.left = col_ping;
    rect.right = panel_x + panel_w - 18;
    (void)snprintf(value, sizeof(value), "%lu", (unsigned long)slot->ping);
    value[sizeof(value) - 1u] = '\0';
    scoreboard_compat_draw_text_shadowed(font, rect, value, row_color, DT_SINGLELINE | DT_CENTER | DT_NOCLIP);

    row_y += SAMP_SCOREBOARD_LINE_HEIGHT;
    ++drawn_rows;
  }

  if (InterlockedCompareExchange(&g_runtime.scoreboard_logged, 1, 0) == 0) {
    runtime_tracef("scoreboard: drawing enabled local_id=%d players=%d host='%s' evidence=OLD_02X_REF,OPENMP_REF,PROBE_TRACE remote_pool=decoded",
                   scoreboard_compat_local_id(), player_count, host);
  }
  return 1;
}

static void loading_screen_compat_release_texture(void) {
  if (g_runtime.loading_screen_texture != NULL) {
    screenshot_compat_release_unknown(g_runtime.loading_screen_texture);
  }
  g_runtime.loading_screen_texture = NULL;
  g_runtime.loading_screen_device = NULL;
}

static int loading_screen_compat_active(void) {
  LONG entry_gate = 0;

  if (!g_runtime.settings.play_online) {
    return 0;
  }
  if (InterlockedCompareExchange(&g_runtime.preconnect_ready, 0, 0) != 0) {
    return 0;
  }
  entry_gate = read_game_entry_gate_value();
  return entry_gate >= 5 && entry_gate <= 7;
}

static int loading_screen_compat_ensure_texture(void *device) {
  char path[MAX_PATH];
  int written = 0;
  HRESULT hr = E_FAIL;

  if (device == NULL) {
    return 0;
  }
  if (g_runtime.loading_screen_texture != NULL && g_runtime.loading_screen_device == device) {
    return 1;
  }
  loading_screen_compat_release_texture();
  if (!loading_screen_compat_resolve_d3dx_create_texture()) {
    return 0;
  }
  if (g_runtime.module_dir[0] == '\0') {
    return 0;
  }
  written = snprintf(path, sizeof(path), "%sloadingscreen.jpg", g_runtime.module_dir);
  if (written <= 0 || (size_t)written >= sizeof(path)) {
    return 0;
  }
  if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
    if (InterlockedCompareExchange(&g_runtime.loading_screen_fail_logged, 1, 0) == 0) {
      runtime_tracef("loading_screen: missing '%s'", path);
    }
    return 0;
  }

  hr = g_runtime.d3dx_create_texture_from_file_a(device, path, &g_runtime.loading_screen_texture);
  if (FAILED(hr) || g_runtime.loading_screen_texture == NULL) {
    if (InterlockedCompareExchange(&g_runtime.loading_screen_fail_logged, 1, 0) == 0) {
      runtime_tracef("loading_screen: texture load failed hr=0x%08lx path='%s'", (unsigned long)hr, path);
    }
    g_runtime.loading_screen_texture = NULL;
    return 0;
  }
  g_runtime.loading_screen_device = device;
  InterlockedExchange(&g_runtime.loading_screen_fail_logged, 0);
  runtime_tracef("loading_screen: texture loaded '%s'", path);
  return 1;
}

static int loading_screen_compat_draw_d3dx_overlay(void *device) {
  void **vtbl = NULL;
  samp_d3d9_set_render_state_fn set_render_state = NULL;
  samp_d3d9_set_texture_fn set_texture = NULL;
  samp_d3d9_set_texture_stage_state_fn set_texture_stage_state = NULL;
  samp_d3d9_set_sampler_state_fn set_sampler_state = NULL;
  samp_d3d9_set_fvf_fn set_fvf = NULL;
  samp_d3d9_draw_primitive_up_fn draw_primitive_up = NULL;
  samp_d3d9_textured_vertex_compat vertices[4];
  HWND hwnd = NULL;
  RECT client_rect;
  int width = 800;
  int height = 600;

  if (!loading_screen_compat_active() || !loading_screen_compat_ensure_texture(device)) {
    return 0;
  }
  if (!memory_is_readable_compat(device, sizeof(void **))) {
    return 0;
  }
  vtbl = *(void ***)device;
  if (vtbl == NULL || !memory_is_readable_compat(&vtbl[SAMP_D3D9_SET_RENDER_STATE_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_SET_TEXTURE_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_SET_TEXTURE_STAGE_STATE_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_SET_SAMPLER_STATE_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_SET_FVF_INDEX], sizeof(void *)) ||
      !memory_is_readable_compat(&vtbl[SAMP_D3D9_DRAW_PRIMITIVE_UP_INDEX], sizeof(void *)) ||
      vtbl[SAMP_D3D9_SET_RENDER_STATE_INDEX] == NULL || vtbl[SAMP_D3D9_SET_TEXTURE_INDEX] == NULL ||
      vtbl[SAMP_D3D9_SET_TEXTURE_STAGE_STATE_INDEX] == NULL || vtbl[SAMP_D3D9_SET_SAMPLER_STATE_INDEX] == NULL ||
      vtbl[SAMP_D3D9_SET_FVF_INDEX] == NULL || vtbl[SAMP_D3D9_DRAW_PRIMITIVE_UP_INDEX] == NULL) {
    return 0;
  }

  hwnd = read_game_hwnd_compat();
  if (hwnd != NULL && GetClientRect(hwnd, &client_rect)) {
    width = client_rect.right - client_rect.left;
    height = client_rect.bottom - client_rect.top;
    if (width <= 0) {
      width = 800;
    }
    if (height <= 0) {
      height = 600;
    }
  }

  vertices[0].x = -0.5f;
  vertices[0].y = (float)height - 0.5f;
  vertices[0].z = 0.1f;
  vertices[0].rhw = 1.0f;
  vertices[0].color = 0xFFFFFFFFu;
  vertices[0].u = 0.0f;
  vertices[0].v = 1.0f;
  vertices[1].x = -0.5f;
  vertices[1].y = -0.5f;
  vertices[1].z = 0.1f;
  vertices[1].rhw = 1.0f;
  vertices[1].color = 0xFFFFFFFFu;
  vertices[1].u = 0.0f;
  vertices[1].v = 0.0f;
  vertices[2].x = (float)width + 0.5f;
  vertices[2].y = (float)height - 0.5f;
  vertices[2].z = 0.1f;
  vertices[2].rhw = 1.0f;
  vertices[2].color = 0xFFFFFFFFu;
  vertices[2].u = 1.0f;
  vertices[2].v = 1.0f;
  vertices[3].x = (float)width + 0.5f;
  vertices[3].y = -0.5f;
  vertices[3].z = 0.1f;
  vertices[3].rhw = 1.0f;
  vertices[3].color = 0xFFFFFFFFu;
  vertices[3].u = 1.0f;
  vertices[3].v = 0.0f;

  set_render_state = (samp_d3d9_set_render_state_fn)vtbl[SAMP_D3D9_SET_RENDER_STATE_INDEX];
  set_texture = (samp_d3d9_set_texture_fn)vtbl[SAMP_D3D9_SET_TEXTURE_INDEX];
  set_texture_stage_state = (samp_d3d9_set_texture_stage_state_fn)vtbl[SAMP_D3D9_SET_TEXTURE_STAGE_STATE_INDEX];
  set_sampler_state = (samp_d3d9_set_sampler_state_fn)vtbl[SAMP_D3D9_SET_SAMPLER_STATE_INDEX];
  set_fvf = (samp_d3d9_set_fvf_fn)vtbl[SAMP_D3D9_SET_FVF_INDEX];
  draw_primitive_up = (samp_d3d9_draw_primitive_up_fn)vtbl[SAMP_D3D9_DRAW_PRIMITIVE_UP_INDEX];

  /* OLD_02X_REF + INFERRED:
     saco/spawnscreen.cpp draws a D3D texture as a transformed fullscreen quad.
     We use the same rendering shape for the 0.3.7 loading image, but gate it by our current preconnect state. */
  (void)set_render_state(device, SAMP_D3DRS_ZENABLE, 0u);
  (void)set_render_state(device, SAMP_D3DRS_ALPHABLENDENABLE, 1u);
  (void)set_render_state(device, SAMP_D3DRS_SRCBLEND, SAMP_D3DBLEND_SRCALPHA);
  (void)set_render_state(device, SAMP_D3DRS_DESTBLEND, SAMP_D3DBLEND_INVSRCALPHA);
  (void)set_texture_stage_state(device, 0u, SAMP_D3DTSS_COLOROP, SAMP_D3DTOP_MODULATE);
  (void)set_texture_stage_state(device, 0u, SAMP_D3DTSS_COLORARG1, SAMP_D3DTA_TEXTURE);
  (void)set_texture_stage_state(device, 0u, SAMP_D3DTSS_COLORARG2, SAMP_D3DTA_DIFFUSE);
  (void)set_texture_stage_state(device, 0u, SAMP_D3DTSS_ALPHAOP, SAMP_D3DTOP_MODULATE);
  (void)set_texture_stage_state(device, 0u, SAMP_D3DTSS_ALPHAARG1, SAMP_D3DTA_TEXTURE);
  (void)set_texture_stage_state(device, 0u, SAMP_D3DTSS_ALPHAARG2, SAMP_D3DTA_DIFFUSE);
  (void)set_sampler_state(device, 0u, SAMP_D3DSAMP_MINFILTER, SAMP_D3DTEXF_LINEAR);
  (void)set_sampler_state(device, 0u, SAMP_D3DSAMP_MAGFILTER, SAMP_D3DTEXF_LINEAR);
  (void)set_texture(device, 0u, g_runtime.loading_screen_texture);
  (void)set_fvf(device, SAMP_D3DFVF_XYZRHW_DIFFUSE_TEX1);
  if (FAILED(draw_primitive_up(device, SAMP_D3DPT_TRIANGLESTRIP, 2u, vertices, sizeof(vertices[0])))) {
    return 0;
  }
  (void)set_texture(device, 0u, NULL);
  if (InterlockedCompareExchange(&g_runtime.loading_screen_logged, 1, 0) == 0) {
    runtime_tracef("loading_screen: drawing enabled size=%dx%d", width, height);
  }
  return 1;
}

static DWORD textdraw_compat_abgr_to_argb(uint32_t color) {
  /* OLD_02X_REF + OPENMP_REF:
   * TextDraw API colors are RGBA, but the legacy server stores RGBA_ABGR()
   * in TEXT_DRAW_TRANSMIT. Convert that client-side ABGR value for D3D.
   */
  return (DWORD)((color & 0xFF00FF00u) | ((color & 0x000000FFu) << 16u) |
                 ((color & 0x00FF0000u) >> 16u));
}

static int textdraw_compat_abgr_has_alpha(uint32_t color) {
  return (color & 0xFF000000u) != 0u;
}

static int textdraw_compat_read_gta_screen(float *screen_width, float *screen_height, float *hud_horiz,
                                           float *hud_vert) {
  int width = 0;
  int height = 0;
  float horiz = 0.0f;
  float vert = 0.0f;

  if (screen_width == NULL || screen_height == NULL || hud_horiz == NULL || hud_vert == NULL) {
    return 0;
  }
  if (!memory_is_readable_compat((const void *)(uintptr_t)SAMP_ADDR_SCREEN_WIDTH, sizeof(width)) ||
      !memory_is_readable_compat((const void *)(uintptr_t)SAMP_ADDR_SCREEN_HEIGHT, sizeof(height)) ||
      !memory_is_readable_compat((const void *)(uintptr_t)SAMP_ADDR_HUD_HORIZ_SCALE, sizeof(horiz)) ||
      !memory_is_readable_compat((const void *)(uintptr_t)SAMP_ADDR_HUD_VERT_SCALE, sizeof(vert))) {
    return 0;
  }

  width = *(volatile int *)(uintptr_t)SAMP_ADDR_SCREEN_WIDTH;
  height = *(volatile int *)(uintptr_t)SAMP_ADDR_SCREEN_HEIGHT;
  horiz = *(volatile float *)(uintptr_t)SAMP_ADDR_HUD_HORIZ_SCALE;
  vert = *(volatile float *)(uintptr_t)SAMP_ADDR_HUD_VERT_SCALE;
  if (width < 320 || width > 10000 || height < 200 || height > 10000 || !isfinite(horiz) || !isfinite(vert) ||
      horiz <= 0.0f || horiz > 4.0f || vert <= 0.0f || vert > 4.0f) {
    return 0;
  }

  *screen_width = (float)width;
  *screen_height = (float)height;
  *hud_horiz = horiz;
  *hud_vert = vert;
  return 1;
}

static int textdraw_compat_gta_font_ready(void) {
  const uintptr_t addrs[] = {SAMP_ADDR_FONT_SET_SCALE,
                             SAMP_ADDR_FONT_SET_COLOR,
                             SAMP_ADDR_FONT_SET_STYLE,
                             SAMP_ADDR_FONT_SET_LINE_WIDTH,
                             SAMP_ADDR_FONT_SET_LINE_HEIGHT,
                             SAMP_ADDR_FONT_SET_DROPCOLOR,
                             SAMP_ADDR_FONT_SET_SHADOW,
                             SAMP_ADDR_FONT_SET_OUTLINE,
                             SAMP_ADDR_FONT_SET_PROPORTIONAL,
                             SAMP_ADDR_FONT_USE_BOX,
                             SAMP_ADDR_FONT_USE_BOX_COLOR,
                             SAMP_ADDR_FONT_UNK12,
                             SAMP_ADDR_FONT_SET_JUSTIFY,
                             SAMP_ADDR_FONT_PRINT_STRING};
  LONG gta_version = InterlockedCompareExchange(&g_runtime.gta_version, 0, 0);
  size_t i = 0u;

  if (gta_version == SAMP_GTA_VERSION_UNKNOWN) {
    return 0;
  }
  for (i = 0u; i < sizeof(addrs) / sizeof(addrs[0]); ++i) {
    if (!memory_is_readable_compat((const void *)(uintptr_t)addrs[i], 1u)) {
      if (InterlockedCompareExchange(&g_runtime.textdraw_gta_font_fail_logged, 1, 0) == 0) {
        runtime_tracef("textdraw_gta_font: unavailable gta_version=%ld addr=0x%08lx", (long)gta_version,
                       (unsigned long)addrs[i]);
      }
      return 0;
    }
  }
  InterlockedExchange(&g_runtime.textdraw_gta_font_fail_logged, 0);
  return 1;
}

static int textdraw_compat_gta_font_enabled(void) {
  static LONG initialized = 0;
  static int enabled = 0;
  const char *value = NULL;

  if (InterlockedCompareExchange(&initialized, 0, 0) != 0) {
    return enabled;
  }

  value = getenv(SAMP_TEXTDRAW_COMPAT_GTA_FONT_ENV);
  if (value != NULL && value[0] != '\0' && value[0] != '0') {
    enabled = 1;
  }
  InterlockedExchange(&initialized, 1);
  runtime_tracef("textdraw_gta_font: enabled=%d env=%s evidence=OLD_02X_REF,GTA_REVERSED_REF,TODO_VERIFY", enabled,
                 SAMP_TEXTDRAW_COMPAT_GTA_FONT_ENV);
  return enabled;
}

static int textdraw_compat_gta_font_style(uint8_t style) {
  /*
   * GTA_REVERSED_REF + TODO_VERIFY:
   * gta-reversed PR #1414 documents that GTA's script font styles are mapped
   * into a small CFont data set. Keep unknown SA-MP values out of SetFontStyle
   * until we have an OBSERVED_037 trace for them.
   */
  if (style <= 3u) {
    return (int)style;
  }
  return 1;
}

static void textdraw_compat_prepare_gta_text(const char *input, char *output, size_t output_size) {
  const char *read_cursor = input;
  char *write_cursor = output;
  char *write_end = output != NULL && output_size > 0u ? output + output_size - 1u : NULL;

  if (output == NULL || output_size == 0u) {
    return;
  }
  output[0] = '\0';
  if (input == NULL) {
    return;
  }

  while (*read_cursor != '\0' && write_cursor < write_end) {
    unsigned char ch = (unsigned char)*read_cursor;
    if (ch < ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
      *write_cursor++ = ' ';
      ++read_cursor;
      continue;
    }
    if (*read_cursor == '_') {
      *write_cursor++ = ' ';
      ++read_cursor;
      continue;
    }
    *write_cursor++ = *read_cursor++;
  }
  *write_cursor = '\0';
  chat_compat_strip_samp_color_tags(output);
}

static int textdraw_compat_draw_gta_font_slot(const samp_textdraw_slot_compat *slot) {
  samp_gta_font_set_scale_fn set_scale = (samp_gta_font_set_scale_fn)(uintptr_t)SAMP_ADDR_FONT_SET_SCALE;
  samp_gta_font_set_color_fn set_color = (samp_gta_font_set_color_fn)(uintptr_t)SAMP_ADDR_FONT_SET_COLOR;
  samp_gta_font_set_int_fn set_style = (samp_gta_font_set_int_fn)(uintptr_t)SAMP_ADDR_FONT_SET_STYLE;
  samp_gta_font_set_float_fn set_line_width = (samp_gta_font_set_float_fn)(uintptr_t)SAMP_ADDR_FONT_SET_LINE_WIDTH;
  samp_gta_font_set_float_fn set_line_height = (samp_gta_font_set_float_fn)(uintptr_t)SAMP_ADDR_FONT_SET_LINE_HEIGHT;
  samp_gta_font_set_color_fn set_drop_color = (samp_gta_font_set_color_fn)(uintptr_t)SAMP_ADDR_FONT_SET_DROPCOLOR;
  samp_gta_font_set_int_fn set_shadow = (samp_gta_font_set_int_fn)(uintptr_t)SAMP_ADDR_FONT_SET_SHADOW;
  samp_gta_font_set_int_fn set_outline = (samp_gta_font_set_int_fn)(uintptr_t)SAMP_ADDR_FONT_SET_OUTLINE;
  samp_gta_font_set_int_fn set_proportional = (samp_gta_font_set_int_fn)(uintptr_t)SAMP_ADDR_FONT_SET_PROPORTIONAL;
  samp_gta_font_use_box_fn use_box = (samp_gta_font_use_box_fn)(uintptr_t)SAMP_ADDR_FONT_USE_BOX;
  samp_gta_font_set_color_fn use_box_color = (samp_gta_font_set_color_fn)(uintptr_t)SAMP_ADDR_FONT_USE_BOX_COLOR;
  samp_gta_font_set_int_fn unk12 = (samp_gta_font_set_int_fn)(uintptr_t)SAMP_ADDR_FONT_UNK12;
  samp_gta_font_set_int_fn set_justify = (samp_gta_font_set_int_fn)(uintptr_t)SAMP_ADDR_FONT_SET_JUSTIFY;
  samp_gta_font_print_string_fn print_string = (samp_gta_font_print_string_fn)(uintptr_t)SAMP_ADDR_FONT_PRINT_STRING;
  float screen_width = 0.0f;
  float screen_height = 0.0f;
  float hud_horiz = 0.0f;
  float hud_vert = 0.0f;
  float scale_x = 0.0f;
  float scale_y = 0.0f;
  float use_x = 0.0f;
  float use_y = 0.0f;
  char text[SAMP_TEXTDRAW_COMPAT_MAX_TEXT_BYTES];
  int proportional = 0;

  if (slot == NULL || textdraw_compat_is_preview_like(slot)) {
    return 0;
  }
  if (!textdraw_compat_gta_font_enabled()) {
    return 0;
  }
  if (!isfinite(slot->transmit.x) || !isfinite(slot->transmit.y) || !isfinite(slot->transmit.letter_width) ||
      !isfinite(slot->transmit.letter_height) || !isfinite(slot->transmit.line_width) ||
      !isfinite(slot->transmit.line_height)) {
    return 0;
  }
  if (!textdraw_compat_gta_font_ready() ||
      !textdraw_compat_read_gta_screen(&screen_width, &screen_height, &hud_horiz, &hud_vert)) {
    return 0;
  }

  textdraw_compat_prepare_gta_text(slot->text, text, sizeof(text));
  if (text[0] == '\0' && !textdraw_compat_has_box(slot)) {
    return 1;
  }

  /* OLD_02X_REF + GTA_REVERSED_REF + TODO_VERIFY:
   * Mirrors CTextDraw::Draw scaling/positioning. The old source routes text
   * through GTA message helpers, but gta-reversed identifies 0x69E160 as
   * InsertPlayerControlKeysInString. We avoid that call here until OBSERVED_037
   * validates the exact 0.3.7 buffer contract.
   */
  scale_y = screen_height * hud_vert * slot->transmit.letter_height * 0.5f;
  scale_x = screen_width * hud_horiz * slot->transmit.letter_width;
  use_y = screen_height - ((SAMP_TEXTDRAW_COMPAT_SCRIPT_HEIGHT - slot->transmit.y) * (screen_height * hud_vert));
  use_x = screen_width - ((SAMP_TEXTDRAW_COMPAT_SCRIPT_WIDTH - slot->transmit.x) * (screen_width * hud_horiz));
  proportional = (slot->transmit.flags & 0x10u) != 0u ? 1 : 0;

  set_scale(scale_x, scale_y);
  set_color(slot->transmit.letter_color);
  unk12(0);
  if ((slot->transmit.flags & 0x04u) != 0u) {
    set_justify(2);
  } else if ((slot->transmit.flags & 0x08u) != 0u) {
    set_justify(0);
  } else {
    set_justify(1);
  }
  set_line_width(screen_width * hud_horiz * slot->transmit.line_width);
  set_line_height(screen_width * hud_horiz * slot->transmit.line_height);
  use_box(textdraw_compat_has_box(slot), 0);
  use_box_color(slot->transmit.box_color);
  set_proportional(proportional);
  set_drop_color(slot->transmit.background_color);
  if (slot->transmit.outline != 0u) {
    set_outline(slot->transmit.outline);
  } else {
    set_shadow(slot->transmit.shadow);
  }
  set_style(textdraw_compat_gta_font_style(slot->transmit.style));
  print_string(use_x, use_y, text);
  set_outline(0);
  set_shadow(0);
  return 1;
}

static void textdraw_compat_prepare_text(const char *input, char *output, size_t output_size) {
  const char *read_cursor = input;
  char *write_cursor = output;
  char *write_end = output != NULL && output_size > 0u ? output + output_size - 1u : NULL;

  if (output == NULL || output_size == 0u) {
    return;
  }
  output[0] = '\0';
  if (input == NULL) {
    return;
  }

  while (*read_cursor != '\0' && write_cursor < write_end) {
    if (read_cursor[0] == '~') {
      const char *end = strchr(read_cursor + 1, '~');
      if (end != NULL) {
        if ((end - read_cursor) == 2 && (read_cursor[1] == 'n' || read_cursor[1] == 'N')) {
          *write_cursor++ = '\n';
        }
        read_cursor = end + 1;
        continue;
      }
    }
    if ((unsigned char)*read_cursor < ' ' && *read_cursor != '\n' && *read_cursor != '\r' && *read_cursor != '\t') {
      *write_cursor++ = ' ';
      ++read_cursor;
      continue;
    }
    /* OLD_02X_REF:
       GTA's CFont conversion path treats '_' in script text as a visible space.
       Our D3DX fallback has to normalize this before drawing. */
    if (*read_cursor == '_') {
      *write_cursor++ = ' ';
      ++read_cursor;
      continue;
    }
    *write_cursor++ = *read_cursor++;
  }
  *write_cursor = '\0';
  chat_compat_strip_samp_color_tags(output);
}

static void textdraw_compat_prepare_render_text(const char *input, char *output, size_t output_size) {
  const char *read_cursor = input;
  char *write_cursor = output;
  char *write_end = output != NULL && output_size > 0u ? output + output_size - 1u : NULL;

  if (output == NULL || output_size == 0u) {
    return;
  }
  output[0] = '\0';
  if (input == NULL) {
    return;
  }

  while (*read_cursor != '\0' && write_cursor < write_end) {
    if (read_cursor[0] == '~') {
      const char *end = strchr(read_cursor + 1, '~');
      if (end != NULL) {
        size_t token_len = (size_t)(end - read_cursor + 1);
        if (token_len == 3u && (read_cursor[1] == 'n' || read_cursor[1] == 'N')) {
          *write_cursor++ = '\n';
          read_cursor = end + 1;
          continue;
        }
        if (token_len <= 7u && (size_t)(write_end - write_cursor) >= token_len) {
          memcpy(write_cursor, read_cursor, token_len);
          write_cursor += token_len;
        }
        read_cursor = end + 1;
        continue;
      }
    }
    if ((unsigned char)*read_cursor < ' ' && *read_cursor != '\n' && *read_cursor != '\r' && *read_cursor != '\t') {
      *write_cursor++ = ' ';
      ++read_cursor;
      continue;
    }
    if (*read_cursor == '_') {
      *write_cursor++ = ' ';
      ++read_cursor;
      continue;
    }
    *write_cursor++ = *read_cursor++;
  }
  *write_cursor = '\0';
}

static int textdraw_compat_clamp_int(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static int textdraw_compat_font_pixel_height(const samp_textdraw_slot_compat *slot) {
  int viewport_x = 0;
  int viewport_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  float letter_height = 1.0f;
  int height = 14;

  if (slot != NULL && isfinite(slot->transmit.letter_height) && slot->transmit.letter_height > 0.0f) {
    letter_height = slot->transmit.letter_height;
  }
  chat_compat_viewport_rect(&viewport_x, &viewport_y, &viewport_w, &viewport_h);
  if (viewport_h <= 0) {
    viewport_h = 600;
  }

  /* OLD_02X_REF + GTA_REVERSED_REF + INFERRED:
   * Legacy CTextDraw feeds CFont scale_y = screenH * hudVert * letterHeight * 0.5.
   * GTA's visible glyph height is roughly 18x that scale. In our D3DX fallback
   * this collapses to viewportH / 448 * letterHeight * 9.
   */
  height = (int)((((float)viewport_h / SAMP_TEXTDRAW_COMPAT_SCRIPT_HEIGHT) * letter_height * 9.0f) + 0.5f);
  return textdraw_compat_clamp_int(height, SAMP_TEXTDRAW_COMPAT_FONT_MIN_HEIGHT,
                                   SAMP_TEXTDRAW_COMPAT_FONT_MIN_HEIGHT +
                                       ((SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS - 1) * SAMP_TEXTDRAW_COMPAT_FONT_STEP));
}

static int textdraw_compat_font_bucket(const samp_textdraw_slot_compat *slot) {
  int height = textdraw_compat_font_pixel_height(slot);
  int bucket = (height - SAMP_TEXTDRAW_COMPAT_FONT_MIN_HEIGHT + (SAMP_TEXTDRAW_COMPAT_FONT_STEP / 2)) /
               SAMP_TEXTDRAW_COMPAT_FONT_STEP;

  return textdraw_compat_clamp_int(bucket, 0, SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS - 1);
}

static int textdraw_compat_font_height_for_bucket(int bucket) {
  bucket = textdraw_compat_clamp_int(bucket, 0, SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS - 1);
  return SAMP_TEXTDRAW_COMPAT_FONT_MIN_HEIGHT + (bucket * SAMP_TEXTDRAW_COMPAT_FONT_STEP);
}

static int textdraw_compat_is_preview_like(const samp_textdraw_slot_compat *slot) {
  return slot != NULL && (slot->transmit.style == 4u || slot->transmit.style == 5u);
}

static int textdraw_compat_has_box(const samp_textdraw_slot_compat *slot) {
  return slot != NULL && (slot->transmit.flags & 0x01u) != 0u;
}

static int textdraw_compat_is_box_marker_text(const char *text) {
  const char *cursor = text;

  if (text == NULL) {
    return 0;
  }
  if (lstrcmpiA(text, "box") == 0 || lstrcmpiA(text, "_") == 0) {
    return 1;
  }
  while (*cursor != '\0') {
    if (*cursor != ' ' && *cursor != '\t') {
      return 0;
    }
    ++cursor;
  }
  return cursor != text;
}

static samp_id3dx_font_compat *textdraw_compat_ensure_font(void *device, int bucket) {
  HRESULT hr = 0;
  samp_id3dx_font_compat *font = NULL;
  int height = 0;

  if (device == NULL) {
    return NULL;
  }
  if (bucket < 0) {
    bucket = 0;
  }
  if (bucket >= SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS) {
    bucket = SAMP_TEXTDRAW_COMPAT_FONT_BUCKETS - 1;
  }
  if (g_runtime.textdraw_d3d_device != NULL && g_runtime.textdraw_d3d_device != device) {
    textdraw_compat_release_fonts();
  }
  if (g_runtime.textdraw_d3dx_fonts[bucket] != NULL && g_runtime.textdraw_d3d_device == device) {
    return g_runtime.textdraw_d3dx_fonts[bucket];
  }
  if (!chat_compat_resolve_d3dx_create_font()) {
    return NULL;
  }

  height = textdraw_compat_font_height_for_bucket(bucket);
  hr = g_runtime.d3dx_create_font_a(device, height, 0u, FW_BOLD, 1u, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &font);
  if (FAILED(hr) || font == NULL) {
    if (InterlockedCompareExchange(&g_runtime.textdraw_d3d_font_fail_logged, 1, 0) == 0) {
      runtime_tracef("textdraw_d3dx: font create failed hr=0x%08lx bucket=%d height=%d device=0x%08lx",
                     (unsigned long)hr, bucket, height, (unsigned long)(uintptr_t)device);
    }
    return NULL;
  }
  g_runtime.textdraw_d3dx_fonts[bucket] = font;
  g_runtime.textdraw_d3d_device = device;
  InterlockedExchange(&g_runtime.textdraw_d3d_font_fail_logged, 0);
  runtime_tracef("textdraw_d3dx: font created bucket=%d height=%d device=0x%08lx", bucket, height,
                 (unsigned long)(uintptr_t)device);
  return font;
}

static int textdraw_compat_slot_rect(const samp_textdraw_slot_compat *slot, int *out_x, int *out_y, int *out_w,
                                     int *out_h) {
  int viewport_x = 0;
  int viewport_y = 0;
  int viewport_w = 0;
  int viewport_h = 0;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
  int x = 0;
  int y = 0;
  int w = SAMP_TEXTDRAW_COMPAT_MIN_WIDTH;
  int h = 22;
  int preview_like = 0;
  int center_align = 0;
  int right_align = 0;
  int line_height_used_as_width = 0;

  if (slot == NULL) {
    return 0;
  }
  chat_compat_viewport_rect(&viewport_x, &viewport_y, &viewport_w, &viewport_h);
  if (viewport_w <= 0 || viewport_h <= 0) {
    return 0;
  }
  scale_x = (float)viewport_w / SAMP_TEXTDRAW_COMPAT_SCRIPT_WIDTH;
  scale_y = (float)viewport_h / SAMP_TEXTDRAW_COMPAT_SCRIPT_HEIGHT;
  x = viewport_x + (int)(slot->transmit.x * scale_x);
  y = viewport_y + (int)(slot->transmit.y * scale_y);
  preview_like = textdraw_compat_is_preview_like(slot);
  center_align = (slot->transmit.flags & 0x08u) != 0u;
  right_align = (slot->transmit.flags & 0x04u) != 0u;
  if (preview_like && slot->transmit.line_width > 0.0f) {
    w = (int)(slot->transmit.line_width * scale_x);
  } else if (center_align && slot->transmit.line_height > 0.0f) {
    w = (int)(slot->transmit.line_height * scale_x);
    line_height_used_as_width = 1;
  } else if (slot->transmit.line_width > slot->transmit.x) {
    w = (int)((slot->transmit.line_width - slot->transmit.x) * scale_x);
  }
  if (preview_like && slot->transmit.line_height > 0.0f) {
    h = (int)(slot->transmit.line_height * scale_y);
  } else if (!line_height_used_as_width && slot->transmit.line_height > slot->transmit.y) {
    h = (int)((slot->transmit.line_height - slot->transmit.y) * scale_y);
  }
  if (w < SAMP_TEXTDRAW_COMPAT_MIN_WIDTH) {
    w = SAMP_TEXTDRAW_COMPAT_MIN_WIDTH;
  }
  if (!preview_like && (center_align || right_align) && w <= SAMP_TEXTDRAW_COMPAT_MIN_WIDTH) {
    w = viewport_w / 2;
  }
  if (w > viewport_w) {
    w = viewport_w;
  }
  if ((!preview_like && line_height_used_as_width) || h < 18 ||
      (!preview_like && slot->transmit.line_height <= 0.0f && slot->transmit.letter_height > 2.0f)) {
    h = textdraw_compat_font_height_for_bucket(textdraw_compat_font_bucket(slot)) + 6;
  }
  if (h > viewport_h) {
    h = viewport_h;
  }

  if (center_align) {
    x -= w / 2;
  } else if (right_align) {
    x -= w;
  }
  if (x < viewport_x) {
    x = viewport_x;
  }
  if (x + w > viewport_x + viewport_w) {
    w = viewport_x + viewport_w - x;
  }

  if (out_x != NULL) {
    *out_x = x;
  }
  if (out_y != NULL) {
    *out_y = y;
  }
  if (out_w != NULL) {
    *out_w = w;
  }
  if (out_h != NULL) {
    *out_h = h;
  }
  return w > 0 && h > 0;
}

static int textdraw_compat_hit_test(int x, int y, uint16_t *out_textdraw_id) {
  int i = 0;

  if (out_textdraw_id != NULL) {
    *out_textdraw_id = 0xFFFFu;
  }
  for (i = (int)SAMP_RAKNET_MAX_TEXTDRAWS - 1; i >= 0; --i) {
    samp_textdraw_slot_compat *slot = &g_runtime.textdraw_slots[i];
    int rx = 0;
    int ry = 0;
    int rw = 0;
    int rh = 0;

    if (InterlockedCompareExchange(&slot->active, 0, 0) == 0) {
      continue;
    }
    if (slot->transmit.selectable == 0u) {
      continue;
    }
    if (!textdraw_compat_slot_rect(slot, &rx, &ry, &rw, &rh)) {
      continue;
    }
    if (dialog_compat_point_in_rect(x, y, rx, ry, rw, rh)) {
      if (out_textdraw_id != NULL) {
        *out_textdraw_id = (uint16_t)i;
      }
      return 1;
    }
  }
  return 0;
}

static int textdraw_compat_submit_click(uint16_t textdraw_id) {
  int result = -1;

  if (g_runtime.net_mgr.raknet_client != NULL) {
    result = samp_raknet_client_send_textdraw_click(g_runtime.net_mgr.raknet_client, textdraw_id);
  }
  if (result == 0) {
    textdraw_compat_clear_select_mode("click");
  }
  runtime_tracef("textdraw: click id=%u result=%d", (unsigned)textdraw_id, result);
  return result;
}

static void textdraw_compat_clear_select_mode(const char *reason) {
  LONG was_select = InterlockedExchange(&g_runtime.textdraw_select_active, 0);
  LONG was_down = InterlockedExchange(&g_runtime.textdraw_mouse_down, 0);
  LONG mouse_mode = InterlockedCompareExchange(&g_runtime.dialog_mouse_mode, 0, 0);
  int dialog_active = dialog_compat_active();

  if (!dialog_active && (was_select != 0 || was_down != 0 || mouse_mode != 0)) {
    dialog_compat_set_mouse_mode(0);
  }
  if (was_select != 0 || was_down != 0 || (!dialog_active && mouse_mode != 0)) {
    runtime_tracef("textdraw: select mode cleared reason=%s active=%ld mouse_down=%ld dialog=%d mouse_mode=%ld",
                   reason != NULL ? reason : "unknown", (long)was_select, (long)was_down, dialog_active,
                   (long)mouse_mode);
  }
}

static int textdraw_compat_handle_mouse(HWND hwnd, UINT msg, LPARAM lparam) {
  POINT cursor;

  if (InterlockedCompareExchange(&g_runtime.textdraw_select_active, 0, 0) == 0) {
    return 0;
  }
  if (msg != WM_MOUSEMOVE && msg != WM_LBUTTONDOWN && msg != WM_LBUTTONUP && msg != WM_LBUTTONDBLCLK) {
    return 0;
  }

  dialog_compat_record_mouse(hwnd, lparam);
  cursor.x = (LONG)InterlockedCompareExchange(&g_runtime.dialog_mouse_x, 0, 0);
  cursor.y = (LONG)InterlockedCompareExchange(&g_runtime.dialog_mouse_y, 0, 0);

  if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
    InterlockedExchange(&g_runtime.textdraw_mouse_down, 1);
    return 1;
  }
  if (msg == WM_LBUTTONUP && InterlockedExchange(&g_runtime.textdraw_mouse_down, 0) != 0) {
    uint16_t textdraw_id = 0xFFFFu;
    if (textdraw_compat_hit_test(cursor.x, cursor.y, &textdraw_id)) {
      (void)textdraw_compat_submit_click(textdraw_id);
    }
    return 1;
  }
  return 1;
}

static void textdraw_compat_draw_rect_outline(void *device, int x, int y, int w, int h, DWORD color) {
  if (device == NULL || w <= 0 || h <= 0) {
    return;
  }
  textdraw_compat_d3d_fill_rect(device, x, y, w, 1, color);
  textdraw_compat_d3d_fill_rect(device, x, y + h - 1, w, 1, color);
  textdraw_compat_d3d_fill_rect(device, x, y, 1, h, color);
  textdraw_compat_d3d_fill_rect(device, x + w - 1, y, 1, h, color);
}

static void ui_compat_draw_cursor(void *device, int x, int y) {
  static const uint8_t outline_widths[17] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 7u, 7u, 4u, 4u, 2u, 2u};
  static const uint8_t fill_offsets[17] = {0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 3u, 4u, 0u, 0u};
  static const uint8_t fill_widths[17] = {0u, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 4u, 3u, 3u, 2u, 2u, 1u, 0u, 0u};
  int row = 0;

  if (device == NULL) {
    return;
  }
  for (row = 0; row < (int)(sizeof(outline_widths) / sizeof(outline_widths[0])); ++row) {
    if (outline_widths[row] != 0u) {
      dialog_compat_d3d_fill_rect(device, x, y + (row * 2), outline_widths[row] * 2, 2, 0xFF000000u);
    }
    if (fill_widths[row] != 0u) {
      dialog_compat_d3d_fill_rect(device, x + (fill_offsets[row] * 2), y + (row * 2), fill_widths[row] * 2, 2,
                                  0xFFFFFFFFu);
    }
  }
}

static void textdraw_compat_draw_preview_placeholder(void *device, const samp_textdraw_slot_compat *slot, int x, int y,
                                                     int w, int h) {
  DWORD fill = 0xFF202020u;
  DWORD border = 0xFF8C8C8Cu;

  if (device == NULL || slot == NULL || w <= 0 || h <= 0) {
    return;
  }
  if (textdraw_compat_abgr_has_alpha(slot->transmit.box_color)) {
    fill = textdraw_compat_abgr_to_argb(slot->transmit.box_color);
  }
  if (textdraw_compat_abgr_has_alpha(slot->transmit.background_color)) {
    border = textdraw_compat_abgr_to_argb(slot->transmit.background_color);
  }
  textdraw_compat_d3d_fill_rect(device, x, y, w, h, fill);
  textdraw_compat_draw_rect_outline(device, x, y, w, h, border);
}

static DWORD textdraw_compat_color_with_rgb(DWORD current_color, DWORD rgb) {
  DWORD alpha = current_color & 0xFF000000u;
  if (alpha == 0u) {
    alpha = 0xFF000000u;
  }
  return alpha | (rgb & 0x00FFFFFFu);
}

static DWORD textdraw_compat_brighten_color(DWORD color) {
  DWORD alpha = color & 0xFF000000u;
  DWORD red = (color >> 16u) & 0xFFu;
  DWORD green = (color >> 8u) & 0xFFu;
  DWORD blue = color & 0xFFu;

  if (alpha == 0u) {
    alpha = 0xFF000000u;
  }
  red = red + (red / 2u);
  green = green + (green / 2u);
  blue = blue + (blue / 2u);
  if (red > 255u) {
    red = 255u;
  }
  if (green > 255u) {
    green = 255u;
  }
  if (blue > 255u) {
    blue = 255u;
  }
  return alpha | (red << 16u) | (green << 8u) | blue;
}

static int textdraw_compat_parse_gta_token(const char *text, DWORD *inout_color, size_t *out_len) {
  const char *end = NULL;
  char token = '\0';

  if (out_len != NULL) {
    *out_len = 0u;
  }
  if (text == NULL || text[0] != '~') {
    return 0;
  }
  end = strchr(text + 1, '~');
  if (end == NULL) {
    return 0;
  }
  if (out_len != NULL) {
    *out_len = (size_t)(end - text + 1);
  }
  if ((end - text) != 2) {
    return 1;
  }

  token = text[1];
  if (token >= 'A' && token <= 'Z') {
    token = (char)(token - 'A' + 'a');
  }

  /* GTA_REVERSED_REF:
   * Mirrors CFont::ParseToken color tokens closely enough for the D3DX fallback.
   * Button/image tokens are consumed but not rendered here. TODO_VERIFY against 0.3.7 traces.
   */
  switch (token) {
    case 'b':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_color_with_rgb(*inout_color, 0x000060D0u);
      }
      break;
    case 'g':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_color_with_rgb(*inout_color, 0x0000C000u);
      }
      break;
    case 'h':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_brighten_color(*inout_color);
      }
      break;
    case 'l':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_color_with_rgb(*inout_color, 0x00000000u);
      }
      break;
    case 'p':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_color_with_rgb(*inout_color, 0x00C060D0u);
      }
      break;
    case 'r':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_color_with_rgb(*inout_color, 0x00E02020u);
      }
      break;
    case 's':
    case 'w':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_color_with_rgb(*inout_color, 0x00D8D8D8u);
      }
      break;
    case 'y':
      if (inout_color != NULL) {
        *inout_color = textdraw_compat_color_with_rgb(*inout_color, 0x00E8C020u);
      }
      break;
    default:
      break;
  }
  return 1;
}

static int textdraw_compat_measure_render_line_width(samp_id3dx_font_compat *font, const char *text,
                                                     size_t text_len) {
  size_t offset = 0u;
  int width = 0;

  if (font == NULL || text == NULL || text_len == 0u) {
    return 0;
  }

  while (offset < text_len) {
    char segment[SAMP_TEXTDRAW_COMPAT_MAX_TEXT_BYTES];
    DWORD tag_color = 0u;
    size_t token_len = 0u;
    size_t len = 0u;

    if (chat_compat_parse_color_tag(text + offset, &tag_color)) {
      offset += 8u;
      continue;
    }
    if (textdraw_compat_parse_gta_token(text + offset, NULL, &token_len) && token_len > 0u) {
      offset += token_len;
      continue;
    }
    while (offset + len < text_len && text[offset + len] != '\n' &&
           !chat_compat_parse_color_tag(text + offset + len, &tag_color) &&
           !textdraw_compat_parse_gta_token(text + offset + len, NULL, &token_len) &&
           len + 1u < sizeof(segment)) {
      ++len;
    }
    if (len == 0u) {
      ++offset;
      continue;
    }
    memcpy(segment, text + offset, len);
    segment[len] = '\0';
    width += chat_compat_d3dx_measure_text_width(font, segment, (int)len * 8);
    offset += len;
  }
  return width;
}

static void textdraw_compat_draw_text_segment(samp_id3dx_font_compat *font, RECT rect, const char *text, DWORD color,
                                              DWORD outline_color, int outline_pixels) {
  int offset = 1;

  if (font == NULL || text == NULL || text[0] == '\0') {
    return;
  }
  if (outline_pixels > 0) {
    if (outline_pixels > 2) {
      outline_pixels = 2;
    }
    for (offset = 1; offset <= outline_pixels; ++offset) {
      RECT shadow_rect = rect;
      OffsetRect(&shadow_rect, -offset, 0);
      chat_compat_d3dx_draw_text(font, shadow_rect, text, outline_color, DT_NOCLIP | DT_SINGLELINE | DT_LEFT);
      shadow_rect = rect;
      OffsetRect(&shadow_rect, offset, 0);
      chat_compat_d3dx_draw_text(font, shadow_rect, text, outline_color, DT_NOCLIP | DT_SINGLELINE | DT_LEFT);
      shadow_rect = rect;
      OffsetRect(&shadow_rect, 0, -offset);
      chat_compat_d3dx_draw_text(font, shadow_rect, text, outline_color, DT_NOCLIP | DT_SINGLELINE | DT_LEFT);
      shadow_rect = rect;
      OffsetRect(&shadow_rect, 0, offset);
      chat_compat_d3dx_draw_text(font, shadow_rect, text, outline_color, DT_NOCLIP | DT_SINGLELINE | DT_LEFT);
    }
  }
  chat_compat_d3dx_draw_text(font, rect, text, color, DT_NOCLIP | DT_SINGLELINE | DT_LEFT);
}

static void textdraw_compat_draw_render_line(samp_id3dx_font_compat *font, RECT rect, const char *text,
                                             size_t text_len, DWORD default_color, DWORD flags, DWORD outline_color,
                                             int outline_pixels) {
  DWORD color = default_color;
  size_t offset = 0u;
  int line_width = 0;

  if (font == NULL || text == NULL || text_len == 0u) {
    return;
  }

  line_width = textdraw_compat_measure_render_line_width(font, text, text_len);
  if ((flags & DT_CENTER) != 0u && rect.right > rect.left + line_width) {
    rect.left += ((rect.right - rect.left) - line_width) / 2;
  } else if ((flags & DT_RIGHT) != 0u && rect.right > rect.left + line_width) {
    rect.left = rect.right - line_width;
  }

  while (offset < text_len) {
    char segment[SAMP_TEXTDRAW_COMPAT_MAX_TEXT_BYTES];
    DWORD tag_color = 0u;
    size_t token_len = 0u;
    size_t len = 0u;
    int width = 0;

    if (chat_compat_parse_color_tag(text + offset, &tag_color)) {
      color = tag_color;
      offset += 8u;
      continue;
    }
    if (textdraw_compat_parse_gta_token(text + offset, &color, &token_len) && token_len > 0u) {
      offset += token_len;
      continue;
    }
    while (offset + len < text_len && text[offset + len] != '\n' &&
           !chat_compat_parse_color_tag(text + offset + len, &tag_color) &&
           !textdraw_compat_parse_gta_token(text + offset + len, NULL, &token_len) &&
           len + 1u < sizeof(segment)) {
      ++len;
    }
    if (len == 0u) {
      ++offset;
      continue;
    }
    memcpy(segment, text + offset, len);
    segment[len] = '\0';
    textdraw_compat_draw_text_segment(font, rect, segment, color, outline_color, outline_pixels);
    width = chat_compat_d3dx_measure_text_width(font, segment, (int)len * 8);
    rect.left += width;
    rect.right += width;
    offset += len;
  }
}

static void textdraw_compat_draw_text_segments(samp_id3dx_font_compat *font, RECT rect, const char *text,
                                               DWORD default_color, DWORD flags, DWORD outline_color,
                                               int outline_pixels, int line_height) {
  const char *line_start = text;
  const char *cursor = text;
  RECT line_rect = rect;

  if (font == NULL || text == NULL || text[0] == '\0') {
    return;
  }
  if (line_height < 10) {
    line_height = 10;
  }

  while (*cursor != '\0') {
    if (*cursor == '\n') {
      textdraw_compat_draw_render_line(font, line_rect, line_start, (size_t)(cursor - line_start), default_color, flags,
                                       outline_color, outline_pixels);
      line_rect.top += line_height;
      line_rect.bottom += line_height;
      line_start = cursor + 1;
    }
    ++cursor;
  }
  if (cursor != line_start) {
    textdraw_compat_draw_render_line(font, line_rect, line_start, (size_t)(cursor - line_start), default_color, flags,
                                     outline_color, outline_pixels);
  }
}

static void textdraw_compat_draw_slot(void *device, samp_textdraw_slot_compat *slot) {
  samp_id3dx_font_compat *font = NULL;
  RECT rect;
  char plain_text[SAMP_TEXTDRAW_COMPAT_MAX_TEXT_BYTES];
  char render_text[SAMP_TEXTDRAW_COMPAT_MAX_TEXT_BYTES];
  DWORD color = 0xFFFFFFFFu;
  DWORD flags = DT_NOCLIP | DT_LEFT;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int bucket = 0;
  int font_height = 0;
  int draw_text = 0;
  int use_box = 0;

  if (device == NULL || slot == NULL || InterlockedCompareExchange(&slot->active, 0, 0) == 0) {
    return;
  }
  if (textdraw_compat_draw_gta_font_slot(slot)) {
    return;
  }
  textdraw_compat_prepare_text(slot->text, plain_text, sizeof(plain_text));
  textdraw_compat_prepare_render_text(slot->text, render_text, sizeof(render_text));
  if (!textdraw_compat_slot_rect(slot, &x, &y, &w, &h)) {
    return;
  }

  use_box = textdraw_compat_has_box(slot);
  draw_text = plain_text[0] != '\0' || render_text[0] != '\0';
  if (use_box && textdraw_compat_is_box_marker_text(plain_text)) {
    draw_text = 0;
  }
  if (slot->transmit.style == 4u) {
    textdraw_compat_draw_preview_placeholder(device, slot, x, y, w, h);
    return;
  }
  if (slot->transmit.style == 5u) {
    textdraw_compat_draw_preview_placeholder(device, slot, x, y, w, h);
    return;
  }
  if (use_box && textdraw_compat_abgr_has_alpha(slot->transmit.box_color)) {
    textdraw_compat_d3d_fill_rect(device, x, y, w, h, textdraw_compat_abgr_to_argb(slot->transmit.box_color));
  }
  if (!draw_text) {
    return;
  }

  bucket = textdraw_compat_font_bucket(slot);
  font_height = textdraw_compat_font_height_for_bucket(bucket);
  font = textdraw_compat_ensure_font(device, bucket);
  if (font == NULL) {
    return;
  }

  color = textdraw_compat_abgr_to_argb(slot->transmit.letter_color);
  rect.left = x;
  rect.top = y;
  rect.right = x + w;
  rect.bottom = y + h;
  if ((slot->transmit.flags & 0x08u) != 0u) {
    flags = DT_NOCLIP | DT_CENTER;
  } else if ((slot->transmit.flags & 0x04u) != 0u) {
    flags = DT_NOCLIP | DT_RIGHT;
  }
  if (strchr(render_text, '\n') != NULL) {
    flags |= DT_WORDBREAK;
  } else {
    flags |= DT_SINGLELINE;
  }
  if (render_text[0] != '\0') {
    DWORD outline_color = textdraw_compat_abgr_to_argb(slot->transmit.background_color);
    int outline_pixels = 0;
    if (outline_color == color) {
      outline_color = 0xFF000000u;
    }
    if (slot->transmit.outline != 0u) {
      outline_pixels = (int)slot->transmit.outline;
    } else if (slot->transmit.shadow != 0u) {
      outline_pixels = 1;
    }
    textdraw_compat_draw_text_segments(font, rect, render_text, color, flags, outline_color, outline_pixels,
                                       font_height + 2);
  }
}

static int textdraw_compat_draw_d3dx_overlay(void *device) {
  LONG active_count = InterlockedCompareExchange(&g_runtime.textdraw_active_count, 0, 0);
  int drawn = 0;
  int i = 0;

  if (device == NULL || active_count <= 0) {
    return 0;
  }
  for (i = 0; i < (int)SAMP_RAKNET_MAX_TEXTDRAWS; ++i) {
    if (InterlockedCompareExchange(&g_runtime.textdraw_slots[i].active, 0, 0) != 0) {
      textdraw_compat_draw_slot(device, &g_runtime.textdraw_slots[i]);
      ++drawn;
    }
  }
  if (InterlockedCompareExchange(&g_runtime.textdraw_select_active, 0, 0) != 0 && !dialog_compat_active()) {
    LONG mouse_x = InterlockedCompareExchange(&g_runtime.dialog_mouse_x, 0, 0);
    LONG mouse_y = InterlockedCompareExchange(&g_runtime.dialog_mouse_y, 0, 0);
    uint16_t hover_id = 0xFFFFu;

    if (textdraw_compat_hit_test((int)mouse_x, (int)mouse_y, &hover_id) && hover_id < SAMP_RAKNET_MAX_TEXTDRAWS) {
      samp_textdraw_slot_compat *slot = &g_runtime.textdraw_slots[hover_id];
      int rx = 0;
      int ry = 0;
      int rw = 0;
      int rh = 0;

      if (textdraw_compat_slot_rect(slot, &rx, &ry, &rw, &rh)) {
        textdraw_compat_draw_rect_outline(device, rx - 2, ry - 2, rw + 4, rh + 4, 0xFF000000u);
        textdraw_compat_draw_rect_outline(device, rx - 1, ry - 1, rw + 2, rh + 2, 0xFFFFFFFFu);
      }
    }
    ui_compat_draw_cursor(device, (int)mouse_x, (int)mouse_y);
  }
  if (drawn > 0 && InterlockedCompareExchange(&g_runtime.textdraw_logged, 1, 0) == 0) {
    runtime_tracef("textdraw_d3dx: drawing enabled active=%ld drawn=%d", (long)active_count, drawn);
  }
  return drawn > 0;
}

static void dialog_compat_draw_button(samp_id3dx_font_compat *font, void *device, int x, int y, int w, int h,
                                      const char *text, int primary) {
  RECT rect;

  dialog_compat_d3d_fill_rect(device, x, y, w, h, primary ? SAMP_DIALOG_COMPAT_COLOR_SELECTED
                                                          : SAMP_DIALOG_COMPAT_COLOR_BUTTON);
  rect.left = x;
  rect.top = y + 5;
  rect.right = x + w;
  rect.bottom = y + h;
  chat_compat_d3dx_draw_text(font, rect, text != NULL && text[0] != '\0' ? text : "OK", SAMP_DIALOG_COMPAT_COLOR_TEXT,
                             DT_SINGLELINE | DT_CENTER | DT_NOCLIP);
}

static int dialog_compat_draw_d3dx_overlay(void *device, samp_id3dx_font_compat *font) {
  LONG style = InterlockedCompareExchange(&g_runtime.dialog_overlay_style, 0, 0);
  LONG selected = InterlockedCompareExchange(&g_runtime.dialog_overlay_selected, 0, 0);
  LONG scroll = InterlockedCompareExchange(&g_runtime.dialog_overlay_scroll, 0, 0);
  samp_dialog_layout_compat layout;
  int panel_x = 0;
  int panel_y = 0;
  int panel_w = 0;
  int panel_h = 0;
  int body_x = 0;
  int body_y = 0;
  int body_w = 0;
  int body_h = 0;
  RECT rect;
  int i = 0;

  if (!dialog_compat_active() || font == NULL || device == NULL) {
    return 0;
  }

  dialog_compat_normalize_selection();
  selected = InterlockedCompareExchange(&g_runtime.dialog_overlay_selected, 0, 0);
  scroll = InterlockedCompareExchange(&g_runtime.dialog_overlay_scroll, 0, 0);
  if (!dialog_compat_layout(&layout)) {
    return 0;
  }
  panel_x = layout.panel_x;
  panel_y = layout.panel_y;
  panel_w = layout.panel_w;
  panel_h = layout.panel_h;
  body_x = layout.body_x;
  body_y = layout.body_y;
  body_w = layout.body_w;
  body_h = layout.body_h;

  dialog_compat_d3d_fill_rect(device, panel_x - 2, panel_y - 2, panel_w + 4, panel_h + 4, 0xFF000000u);
  dialog_compat_d3d_fill_rect(device, panel_x, panel_y, panel_w, panel_h, SAMP_DIALOG_COMPAT_COLOR_PANEL);
  dialog_compat_d3d_fill_rect(device, panel_x, panel_y, panel_w, 30, SAMP_DIALOG_COMPAT_COLOR_HEADER);
  dialog_compat_d3d_fill_rect(device, body_x, body_y, body_w, body_h, SAMP_DIALOG_COMPAT_COLOR_BODY);

  rect.left = panel_x + 12;
  rect.top = panel_y + 7;
  rect.right = panel_x + panel_w - 12;
  rect.bottom = panel_y + 29;
  chat_compat_d3dx_draw_text(font, rect, g_runtime.dialog_overlay_title, SAMP_DIALOG_COMPAT_COLOR_TEXT,
                             DT_SINGLELINE | DT_LEFT | DT_NOCLIP);

  if (dialog_compat_uses_list(style)) {
    int line_y = body_y + 8;
    int header_offset = dialog_compat_has_header(style) ? 1 : 0;
    int item_count = dialog_compat_item_count();

    if (header_offset != 0) {
      char header[SAMP_DIALOG_COMPAT_MAX_ITEM_BYTES];
      if (dialog_compat_get_line(g_runtime.dialog_overlay_info, 0, header, sizeof(header))) {
        rect.left = body_x + 8;
        rect.top = line_y;
        rect.right = body_x + body_w - 8;
        rect.bottom = line_y + 20;
        chat_compat_d3dx_draw_text(font, rect, header, SAMP_DIALOG_COMPAT_COLOR_MUTED,
                                   DT_SINGLELINE | DT_LEFT | DT_NOCLIP);
        line_y += 22;
      }
    }

    for (i = 0; i < SAMP_DIALOG_COMPAT_MAX_VISIBLE_ITEMS && (scroll + i) < item_count; ++i) {
      char line[SAMP_DIALOG_COMPAT_MAX_ITEM_BYTES];
      int item_index = (int)scroll + i;
      int line_index = item_index + header_offset;
      int item_y = line_y + (i * 22);

      if (!dialog_compat_get_line(g_runtime.dialog_overlay_info, line_index, line, sizeof(line))) {
        continue;
      }
      if (item_index == selected) {
        dialog_compat_d3d_fill_rect(device, body_x + 4, item_y - 1, body_w - 8, 21,
                                    SAMP_DIALOG_COMPAT_COLOR_SELECTED);
      }
      rect.left = body_x + 10;
      rect.top = item_y + 1;
      rect.right = body_x + body_w - 10;
      rect.bottom = item_y + 21;
      chat_compat_d3dx_draw_text(font, rect, line, SAMP_DIALOG_COMPAT_COLOR_TEXT,
                                 DT_SINGLELINE | DT_LEFT | DT_NOCLIP);
    }
  } else if (dialog_compat_uses_input(style)) {
    char input_line[SAMP_RAKNET_DIALOG_INPUT_BYTES];
    size_t input_len = 0u;
    DWORD tick = GetTickCount();

    rect.left = body_x + 8;
    rect.top = body_y + 8;
    rect.right = body_x + body_w - 8;
    rect.bottom = body_y + 86;
    chat_compat_d3dx_draw_text(font, rect, g_runtime.dialog_overlay_info, SAMP_DIALOG_COMPAT_COLOR_TEXT,
                               DT_WORDBREAK | DT_LEFT | DT_NOCLIP);

    strncpy(input_line, g_runtime.dialog_overlay_input, sizeof(input_line) - 2u);
    input_line[sizeof(input_line) - 2u] = '\0';
    if (style == 3) {
      for (input_len = 0u; input_line[input_len] != '\0'; ++input_len) {
        input_line[input_len] = '*';
      }
    } else {
      input_len = strlen(input_line);
    }
    if ((tick / 450u) % 2u == 0u && input_len + 1u < sizeof(input_line)) {
      input_line[input_len] = '_';
      input_line[input_len + 1u] = '\0';
    }

    dialog_compat_d3d_fill_rect(device, body_x + 8, body_y + body_h - 42, body_w - 16, 28, 0xFF101010u);
    rect.left = body_x + 14;
    rect.top = body_y + body_h - 36;
    rect.right = body_x + body_w - 14;
    rect.bottom = body_y + body_h - 8;
    chat_compat_d3dx_draw_text(font, rect, input_line, SAMP_DIALOG_COMPAT_COLOR_TEXT,
                               DT_SINGLELINE | DT_LEFT | DT_NOCLIP);
  } else {
    rect.left = body_x + 8;
    rect.top = body_y + 8;
    rect.right = body_x + body_w - 8;
    rect.bottom = body_y + body_h - 8;
    chat_compat_d3dx_draw_text(font, rect, g_runtime.dialog_overlay_info, SAMP_DIALOG_COMPAT_COLOR_TEXT,
                               DT_WORDBREAK | DT_LEFT | DT_NOCLIP);
  }

  if (g_runtime.dialog_overlay_button2[0] != '\0') {
    dialog_compat_draw_button(font, device, layout.button2_x, layout.button2_y, layout.button2_w, layout.button2_h,
                              g_runtime.dialog_overlay_button2, 0);
    dialog_compat_draw_button(font, device, layout.button1_x, layout.button1_y, layout.button1_w, layout.button1_h,
                              g_runtime.dialog_overlay_button1, 1);
  } else {
    dialog_compat_draw_button(font, device, layout.button1_x, layout.button1_y, layout.button1_w, layout.button1_h,
                              g_runtime.dialog_overlay_button1, 1);
  }

  {
    LONG mouse_x = InterlockedCompareExchange(&g_runtime.dialog_mouse_x, 0, 0);
    LONG mouse_y = InterlockedCompareExchange(&g_runtime.dialog_mouse_y, 0, 0);
    ui_compat_draw_cursor(device, (int)mouse_x, (int)mouse_y);
  }

  return 1;
}

static int chat_compat_draw_d3dx_overlay(void *device) {
  LONG count = 0;
  LONG input_active = 0;
  LONG dialog_active = 0;
  LONG textdraw_active = 0;
  int loading_active = 0;
  int scoreboard_active = 0;
  int x = 0;
  int y = 0;
  int i = 0;

  if (!chat_overlay_enabled_compat()) {
    return 0;
  }
  chat_input_try_install_wndproc_compat();
  count = InterlockedCompareExchange(&g_runtime.chat_overlay_line_count, 0, 0);
  input_active = InterlockedCompareExchange(&g_runtime.chat_input_active, 0, 0);
  dialog_active = InterlockedCompareExchange(&g_runtime.dialog_overlay_active, 0, 0);
  textdraw_active = InterlockedCompareExchange(&g_runtime.textdraw_active_count, 0, 0);
  loading_active = loading_screen_compat_active();
  scoreboard_active = scoreboard_compat_active();
  if (count <= 0 && input_active == 0 && dialog_active == 0 && textdraw_active <= 0 && !loading_active &&
      !scoreboard_active) {
    return 0;
  }
  if (count < 0) {
    count = 0;
  }
  if (count > SAMP_CHAT_COMPAT_MAX_LINES) {
    count = SAMP_CHAT_COMPAT_MAX_LINES;
  }
  if (!chat_compat_ensure_d3dx_font(device)) {
    return 0;
  }

  if (loading_active) {
    (void)loading_screen_compat_draw_d3dx_overlay(device);
  }
  if (textdraw_active > 0 && !scoreboard_active) {
    (void)textdraw_compat_draw_d3dx_overlay(device);
  }
  chat_compat_viewport_origin(&x, &y);
  for (i = 0; i < count; ++i) {
    RECT rect;
    rect.left = x;
    rect.top = y + (i * SAMP_CHAT_COMPAT_LINE_HEIGHT);
    rect.right = x + 900;
    rect.bottom = rect.top + SAMP_CHAT_COMPAT_LINE_HEIGHT + 4;
    chat_compat_d3dx_draw_text_outline_segments(
        g_runtime.chat_d3dx_font, rect, g_runtime.chat_overlay_lines[i],
        g_runtime.chat_overlay_colors[i] != 0u ? g_runtime.chat_overlay_colors[i] : SAMP_CHAT_COMPAT_COLOR_INFO);
  }
  if (input_active != 0) {
    char input_line[SAMP_CHAT_INPUT_DRAW_BYTES];
    RECT rect;
    int input_y = y + (count * SAMP_CHAT_COMPAT_LINE_HEIGHT) + 5;
    int input_w = 0;
    int box_x = x - 4;

    chat_input_format_draw_line_compat(input_line, sizeof(input_line));
    input_w = chat_compat_d3dx_measure_text_width(g_runtime.chat_d3dx_font, input_line,
                                                  (int)strlen(input_line) * 8) +
              12;
    if (input_w < 28) {
      input_w = 28;
    }
    if (input_w > 900) {
      input_w = 900;
    }
    if (box_x < 0) {
      box_x = 0;
    }
    if (!dialog_compat_d3d_alpha_rect(device, box_x, input_y - 2, input_w, SAMP_CHAT_COMPAT_LINE_HEIGHT + 6,
                                      SAMP_CHAT_INPUT_BOX_COLOR)) {
      dialog_compat_d3d_fill_rect(device, box_x, input_y - 2, input_w, SAMP_CHAT_COMPAT_LINE_HEIGHT + 6, 0xFF000000u);
    }
    rect.left = x;
    rect.top = input_y;
    rect.right = x + 900;
    rect.bottom = rect.top + SAMP_CHAT_COMPAT_LINE_HEIGHT + 4;
    chat_compat_d3dx_draw_text_outline(g_runtime.chat_d3dx_font, rect, input_line, 0xFFFFFFFFu);
  }
  if (dialog_active != 0) {
    (void)dialog_compat_draw_d3dx_overlay(device, g_runtime.chat_d3dx_font);
  } else if (scoreboard_active) {
    (void)scoreboard_compat_draw_d3dx_overlay(device);
  }

  if (InterlockedCompareExchange(&g_runtime.chat_d3d_draw_logged, 1, 0) == 0) {
    runtime_tracef("chat_d3dx: drawing enabled device=0x%08lx lines=%ld dialog=%ld textdraws=%ld scoreboard=%d x=%d y=%d",
                   (unsigned long)(uintptr_t)device, (long)count, (long)dialog_active, (long)textdraw_active,
                   scoreboard_active, x, y);
  }
  return 1;
}

static HRESULT WINAPI chat_compat_end_scene_hook(void *device) {
  samp_d3d9_end_scene_fn original = g_runtime.chat_end_scene_original;

  if (InterlockedCompareExchange(&g_runtime.chat_d3d_draw_active, 1, 0) == 0) {
    (void)chat_compat_draw_d3dx_overlay(device);
    screenshot_compat_capture_if_requested(device);
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
  LONG net_state = 0;
  LONG preconnect_ready = 0;
  LONG textdraw_active = 0;
  int loading_active = 0;
  int dialog_active = 0;

  if (!chat_overlay_enabled_compat() || !chat_d3d_enabled_compat() ||
      InterlockedCompareExchange(&g_runtime.chat_d3d_hooked, 0, 0) != 0) {
    return;
  }

  dialog_active = dialog_compat_active();
  loading_active = loading_screen_compat_active();
  preconnect_ready = InterlockedCompareExchange(&g_runtime.preconnect_ready, 0, 0);
  textdraw_active = InterlockedCompareExchange(&g_runtime.textdraw_active_count, 0, 0);
  net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  if (g_runtime.settings.play_online && net_state < SAMP_NETGAME_CONNECTED && preconnect_ready == 0 &&
      !chat_d3d_early_enabled_compat() && !dialog_active && textdraw_active <= 0 && !loading_active) {
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
  LONG input_active = 0;
  HWND hwnd = NULL;
  HDC dc = NULL;
  HFONT old_font = NULL;
  int x = SAMP_CHAT_COMPAT_BASE_X;
  int y = (int)chat_overlay_y_compat();
  int i = 0;

  if (!chat_overlay_enabled_compat()) {
    return;
  }

  chat_input_try_install_wndproc_compat();
  chat_compat_try_install_d3d_hook();
  if (chat_d3d_enabled_compat() && InterlockedCompareExchange(&g_runtime.chat_d3d_hooked, 0, 0) != 0) {
    return;
  }

  count = InterlockedCompareExchange(&g_runtime.chat_overlay_line_count, 0, 0);
  input_active = InterlockedCompareExchange(&g_runtime.chat_input_active, 0, 0);
  if (count <= 0 && input_active == 0) {
    return;
  }
  if (count < 0) {
    count = 0;
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
    chat_compat_draw_text_outline_segments(dc, x, y + (i * SAMP_CHAT_COMPAT_LINE_HEIGHT),
                                           g_runtime.chat_overlay_lines[i],
                                           g_runtime.chat_overlay_colors[i] != 0u ? g_runtime.chat_overlay_colors[i]
                                                                                  : SAMP_CHAT_COMPAT_COLOR_INFO);
  }
  if (input_active != 0) {
    char input_line[SAMP_CHAT_INPUT_DRAW_BYTES];
    int input_y = y + (count * SAMP_CHAT_COMPAT_LINE_HEIGHT) + 5;
    int input_w = 0;
    RECT input_box;
    HBRUSH input_brush = NULL;
    SIZE input_size;

    chat_input_format_draw_line_compat(input_line, sizeof(input_line));
    memset(&input_size, 0, sizeof(input_size));
    if (GetTextExtentPoint32A(dc, input_line, (int)strlen(input_line), &input_size) && input_size.cx > 0) {
      input_w = input_size.cx + 12;
    } else {
      input_w = (int)strlen(input_line) * 8 + 12;
    }
    if (input_w < 28) {
      input_w = 28;
    }
    if (input_w > 900) {
      input_w = 900;
    }
    input_box.left = x - 4;
    input_box.top = input_y - 2;
    input_box.right = input_box.left + input_w;
    input_box.bottom = input_y + SAMP_CHAT_COMPAT_LINE_HEIGHT + 4;
    input_brush = CreateSolidBrush(RGB(0, 0, 0));
    if (input_brush != NULL) {
      FillRect(dc, &input_box, input_brush);
      DeleteObject(input_brush);
    }
    chat_compat_draw_text_outline(dc, x, input_y, input_line, 0xFFFFFFFFu);
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
  unsigned long access_kind = 0;
  unsigned long access_addr = 0;
  LONG phase = g_last_phase;
  LONG entry_mem = read_game_entry_gate_value();
  LONG entry_target = InterlockedCompareExchange(&g_runtime.entry_gate, 0, 0);
  LONG net_state = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);
  LONG hooks = InterlockedCompareExchange(&g_runtime.hooks_installed, 0, 0);
  LONG hook_calls = InterlockedCompareExchange(&g_runtime.hook_callback_calls, 0, 0);
  LONG gta_version = InterlockedCompareExchange(&g_runtime.gta_version, 0, 0);
  LONG ingame_patches = InterlockedCompareExchange(&g_runtime.ingame_patches_applied, 0, 0);
  uintptr_t anim_assoc = (uintptr_t)(*(volatile uint32_t *)(uintptr_t)SAMP_ADDR_ANIM_ASSOC_GROUPS_PTR);
  uint32_t clump_offset = *(volatile uint32_t *)(uintptr_t)SAMP_ADDR_RPANIMBLEND_CLUMP_OFFSET;
  uintptr_t entry_pool = 0u;
  uintptr_t entry_pool_objects = 0u;
  uintptr_t entry_pool_flags = 0u;
  uint32_t entry_pool_size = 0u;
  uint32_t entry_pool_top = 0u;
  uintptr_t context_ip = 0u;
  uintptr_t context_sp = 0u;
  uintptr_t delete_arg = 0u;

  if (memory_is_readable_compat((const void *)(uintptr_t)SAMP_ADDR_ENTRY_INFO_NODE_POOL_PTR, sizeof(entry_pool))) {
    memcpy(&entry_pool, (const void *)(uintptr_t)SAMP_ADDR_ENTRY_INFO_NODE_POOL_PTR, sizeof(entry_pool));
    if (entry_pool >= 0x10000u &&
        memory_is_readable_compat((const void *)entry_pool, SAMP_POOL_OFFSET_TOP + sizeof(entry_pool_top))) {
      memcpy(&entry_pool_objects, (const void *)(entry_pool + SAMP_POOL_OFFSET_OBJECTS), sizeof(entry_pool_objects));
      memcpy(&entry_pool_flags, (const void *)(entry_pool + SAMP_POOL_OFFSET_FLAGS), sizeof(entry_pool_flags));
      memcpy(&entry_pool_size, (const void *)(entry_pool + SAMP_POOL_OFFSET_SIZE), sizeof(entry_pool_size));
      memcpy(&entry_pool_top, (const void *)(entry_pool + SAMP_POOL_OFFSET_TOP), sizeof(entry_pool_top));
    }
  }

  if (exception_info != NULL && exception_info->ExceptionRecord != NULL) {
    code = (unsigned long)exception_info->ExceptionRecord->ExceptionCode;
    addr = (unsigned long)(uintptr_t)exception_info->ExceptionRecord->ExceptionAddress;
    if (exception_info->ExceptionRecord->NumberParameters >= 2u) {
      access_kind = (unsigned long)exception_info->ExceptionRecord->ExceptionInformation[0];
      access_addr = (unsigned long)exception_info->ExceptionRecord->ExceptionInformation[1];
    }
  }
#if defined(__i386__) || defined(_M_IX86)
  if (exception_info != NULL && exception_info->ContextRecord != NULL) {
    context_ip = (uintptr_t)exception_info->ContextRecord->Eip;
    context_sp = (uintptr_t)exception_info->ContextRecord->Esp;
    if (memory_is_readable_compat((const void *)(context_sp + 4u), sizeof(delete_arg))) {
      memcpy(&delete_arg, (const void *)(context_sp + 4u), sizeof(delete_arg));
    }
  }
#endif

  runtime_tracef("exception_filter: code=0x%08lx addr=0x%08lx access=%lu/0x%08lx phase=%ld entry=%ld target=%ld "
                 "net=%ld hooks=%ld callbacks=%ld gta_version=%ld ingame_patches=%ld anim_assoc=0x%08lx "
                 "clump_offset=0x%08lx ip=0x%08lx sp=0x%08lx delete_arg=0x%08lx entry_pool=0x%08lx "
                 "entry_objects=0x%08lx entry_flags=0x%08lx entry_size=%lu entry_top=%lu",
                 code, addr, access_kind, access_addr, (long)phase, (long)entry_mem, (long)entry_target,
                 (long)net_state, (long)hooks, (long)hook_calls, (long)gta_version, (long)ingame_patches,
                 (unsigned long)anim_assoc, (unsigned long)clump_offset, (unsigned long)context_ip,
                 (unsigned long)context_sp, (unsigned long)delete_arg, (unsigned long)entry_pool,
                 (unsigned long)entry_pool_objects, (unsigned long)entry_pool_flags,
                 (unsigned long)entry_pool_size, (unsigned long)entry_pool_top);
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
    case 12:
      return "ID_AUTH_KEY";
    case 20:
      return "ID_RPC";
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

static int game_pointer_plausible_compat(uintptr_t ptr) {
  return ptr >= 0x10000u && ptr < 0xF0000000u;
}

static int patch_write_u32(uintptr_t addr, uint32_t value) {
  return patch_copy(addr, &value, sizeof(value));
}

static int patch_write_u8(uintptr_t addr, uint8_t value) {
  return patch_copy(addr, &value, sizeof(value));
}

static void apply_resolution_unlock_patches_compat(void) {
  uint8_t jump_short = 0xEBu;
  int applied = 0;

  if (InterlockedCompareExchange(&g_runtime.resolution_unlock_patches_applied, 1, 0) != 0) {
    return;
  }

  applied += patch_nop(SAMP_ADDR_VIDEO_MODE_LIST_WIDTH_CHECK, 6u) ? 1 : 0;
  applied += patch_nop(SAMP_ADDR_VIDEO_MODE_LIST_HEIGHT_CHECK, 6u) ? 1 : 0;
  applied += patch_write_u8(SAMP_ADDR_VIDEO_MODE_LIST_ASPECT_CHECK, jump_short) ? 1 : 0;
  applied += patch_nop(SAMP_ADDR_VIDEO_MODE_LIST_VRAM_CHECK, 2u) ? 1 : 0;
  applied += patch_nop(SAMP_ADDR_DEVICE_DIALOG_WIDTH_CHECK, 6u) ? 1 : 0;
  applied += patch_nop(SAMP_ADDR_DEVICE_DIALOG_HEIGHT_CHECK, 6u) ? 1 : 0;
  applied += patch_write_u8(SAMP_ADDR_DEVICE_DIALOG_ASPECT_CHECK, jump_short) ? 1 : 0;

  runtime_tracef("resolution_unlock: applied=%d/7", applied);
}

static void apply_default_resolution_patch_compat(const samp_display_mode_request *request) {
  int width_ok = 0;
  int height_ok = 0;

  if (request == NULL) {
    return;
  }
  if (request->width < 640 || request->height < 480) {
    return;
  }
  if (InterlockedCompareExchange(&g_runtime.default_resolution_patch_applied, 1, 0) != 0) {
    return;
  }

  width_ok = patch_write_u32(SAMP_ADDR_DEFAULT_RES_WIDTH_IMM, (uint32_t)request->width);
  height_ok = patch_write_u32(SAMP_ADDR_DEFAULT_RES_HEIGHT_IMM, (uint32_t)request->height);
  runtime_tracef("resolution_unlock: default_res=%dx%d width_patch=%d height_patch=%d", request->width,
                 request->height, width_ok, height_ok);
}

static int env_flag_enabled_default_compat(const char *name, int default_enabled) {
  const char *value = NULL;

  value = getenv(name);
  if (value == NULL || *value == '\0') {
    return default_enabled ? 1 : 0;
  }
  if (value[0] == '0' || value[0] == 'n' || value[0] == 'N' || value[0] == 'f' || value[0] == 'F') {
    return 0;
  }
  return 1;
}

static void display_mode_desktop_request_compat(samp_display_mode_request *request) {
  HDC dc = NULL;
  int depth = 32;
  int width = 800;
  int height = 600;

  if (request == NULL) {
    return;
  }

  width = GetSystemMetrics(SM_CXSCREEN);
  height = GetSystemMetrics(SM_CYSCREEN);
  dc = GetDC(NULL);
  if (dc != NULL) {
    int caps_depth = GetDeviceCaps(dc, BITSPIXEL);
    if (caps_depth >= 16) {
      depth = caps_depth;
    }
    ReleaseDC(NULL, dc);
  }
  if (width < 640) {
    width = 800;
  }
  if (height < 480) {
    height = 600;
  }

  request->width = width;
  request->height = height;
  request->depth = depth;
  request->adapter = 0;
}

static int parse_display_mode_request_compat(const char *text, samp_display_mode_request *request) {
  const char *cursor = text;
  char *endptr = NULL;
  unsigned long values[4] = {0, 0, 0, 0};
  int count = 0;

  if (text == NULL || *text == '\0' || request == NULL) {
    return 0;
  }

  while (*cursor != '\0' && count < 4) {
    values[count] = strtoul(cursor, &endptr, 10);
    if (endptr == cursor || values[count] > 8192UL) {
      return 0;
    }
    ++count;
    cursor = endptr;
    if (*cursor == 'x' || *cursor == 'X') {
      ++cursor;
      continue;
    }
    break;
  }

  if (*cursor != '\0' || count < 2 || values[0] < 640UL || values[1] < 480UL) {
    return 0;
  }

  request->width = (int)values[0];
  request->height = (int)values[1];
  request->depth = (count >= 3 && values[2] >= 16UL) ? (int)values[2] : request->depth;
  request->adapter = (count >= 4) ? (int)values[3] : request->adapter;
  return 1;
}

static void load_display_mode_request_compat(samp_display_mode_request *request) {
  const char *env_value = NULL;
  samp_display_mode_request parsed;

  if (request == NULL) {
    return;
  }

  display_mode_desktop_request_compat(request);
  parsed = *request;
  env_value = getenv("SAMPDLL_DISPLAY_RESOLUTION");
  if (env_value != NULL && *env_value != '\0') {
    if (parse_display_mode_request_compat(env_value, &parsed)) {
      *request = parsed;
    } else {
      runtime_tracef("select_device: ignored invalid SAMPDLL_DISPLAY_RESOLUTION='%s'", env_value);
    }
  }
}

static uint32_t video_mode_score_compat(const samp_rw_video_mode *mode, const samp_display_mode_request *request) {
  uint32_t score = 0;
  uint32_t width_delta = 0;
  uint32_t height_delta = 0;

  if (mode == NULL || request == NULL) {
    return 0xFFFFFFFFu;
  }

  width_delta = (mode->width > request->width) ? (uint32_t)(mode->width - request->width)
                                               : (uint32_t)(request->width - mode->width);
  height_delta = (mode->height > request->height) ? (uint32_t)(mode->height - request->height)
                                                  : (uint32_t)(request->height - mode->height);
  score = width_delta + height_delta;
  if (mode->depth != request->depth) {
    score += 100000u;
  }
  if (mode->width > request->width || mode->height > request->height) {
    score += 1000u;
  }
  return score;
}

static int d3d9_display_mode_depth_compat(uint32_t format) {
  if (format == SAMP_D3DFMT_X8R8G8B8) {
    return 32;
  }
  if (format == SAMP_D3DFMT_R5G6B5) {
    return 16;
  }
  return 0;
}

static uint32_t d3d9_display_mode_format_for_depth_compat(int depth) {
  return (depth >= 32) ? SAMP_D3DFMT_X8R8G8B8 : SAMP_D3DFMT_R5G6B5;
}

static int display_mode_index_is_used_compat(int index, const int *used_indices, int used_count) {
  int i = 0;

  if (used_indices == NULL || used_count <= 0) {
    return 0;
  }
  for (i = 0; i < used_count; ++i) {
    if (used_indices[i] == index) {
      return 1;
    }
  }
  return 0;
}

static int force_display_mode_slot_compat(const samp_display_mode_request *request, const int *used_indices,
                                          int used_count) {
  samp_d3d9_display_mode_compat *modes = NULL;
  samp_d3d9_display_mode_compat forced;
  int count = 0;
  int exact_index = -1;
  int best_index = -1;
  uint32_t best_score = 0xFFFFFFFFu;
  int i = 0;

  if (request == NULL) {
    return -1;
  }

  count = *(volatile int32_t *)(uintptr_t)SAMP_ADDR_D3D9_NUM_DISPLAY_MODES;
  modes = *(samp_d3d9_display_mode_compat **)(uintptr_t)SAMP_ADDR_D3D9_DISPLAY_MODES;
  if (count <= 0 || count > 4096 || !game_pointer_plausible_compat((uintptr_t)modes)) {
    runtime_tracef("display_mode_force: skipped count=%d modes=%p target=%dx%dx%d", count, modes, request->width,
                   request->height, request->depth);
    return -1;
  }

  for (i = 0; i < count; ++i) {
    samp_rw_video_mode score_mode;
    int depth = d3d9_display_mode_depth_compat(modes[i].format);
    uint32_t score = 0;

    if ((modes[i].flags & SAMP_RW_VIDEOMODE_EXCLUSIVE) == 0) {
      continue;
    }
    if (modes[i].width < 640u || modes[i].height < 480u) {
      continue;
    }
    if ((int)modes[i].width == request->width && (int)modes[i].height == request->height &&
        (request->depth <= 0 || depth == request->depth)) {
      exact_index = i;
      break;
    }
    if (display_mode_index_is_used_compat(i, used_indices, used_count)) {
      continue;
    }

    memset(&score_mode, 0, sizeof(score_mode));
    score_mode.width = (int32_t)modes[i].width;
    score_mode.height = (int32_t)modes[i].height;
    score_mode.depth = depth > 0 ? depth : 32;
    score_mode.flags = modes[i].flags;
    score = video_mode_score_compat(&score_mode, request);
    if (best_index < 0 || score < best_score) {
      best_index = i;
      best_score = score;
    }
  }

  if (exact_index >= 0) {
    runtime_tracef("display_mode_force: target=%dx%dx%d already present index=%d", request->width, request->height,
                   request->depth, exact_index);
    return exact_index;
  }
  if (best_index < 0) {
    runtime_tracef("display_mode_force: no replacement slot for target=%dx%dx%d count=%d", request->width,
                   request->height, request->depth, count);
    return -1;
  }

  forced.width = (uint32_t)request->width;
  forced.height = (uint32_t)request->height;
  forced.refresh_rate = 0u;
  forced.format = d3d9_display_mode_format_for_depth_compat(request->depth);
  forced.flags = SAMP_RW_VIDEOMODE_EXCLUSIVE;
  if (!patch_copy((uintptr_t)&modes[best_index], &forced, sizeof(forced))) {
    runtime_tracef("display_mode_force: patch failed index=%d target=%dx%dx%d", best_index, request->width,
                   request->height, request->depth);
    return -1;
  }

  runtime_tracef("display_mode_force: replaced index=%d target=%dx%dx%d format=%lu old_score=%lu count=%d", best_index,
                 request->width, request->height, request->depth, (unsigned long)forced.format,
                 (unsigned long)best_score, count);
  return best_index;
}

static void force_startup_display_modes_compat(const samp_display_mode_request *request) {
  samp_display_mode_request extra;
  const char *extra_env = NULL;
  int used_indices[4] = {-1, -1, -1, -1};
  int used_count = 0;
  int forced = -1;

  memset(&extra, 0, sizeof(extra));
  extra.width = 2560;
  extra.height = 1440;
  extra.depth = 32;
  extra.adapter = 0;

  extra_env = getenv("SAMPDLL_EXTRA_RESOLUTION");
  if (extra_env != NULL && *extra_env != '\0') {
    samp_display_mode_request parsed = extra;
    if (parse_display_mode_request_compat(extra_env, &parsed)) {
      extra = parsed;
    } else {
      runtime_tracef("display_mode_force: ignored invalid SAMPDLL_EXTRA_RESOLUTION='%s'", extra_env);
    }
  }

  forced = force_display_mode_slot_compat(&extra, used_indices, used_count);
  if (forced >= 0 && used_count < (int)(sizeof(used_indices) / sizeof(used_indices[0]))) {
    used_indices[used_count++] = forced;
  }
  if (request != NULL && (request->width != extra.width || request->height != extra.height || request->depth != extra.depth)) {
    forced = force_display_mode_slot_compat(request, used_indices, used_count);
    if (forced >= 0 && used_count < (int)(sizeof(used_indices) / sizeof(used_indices[0]))) {
      used_indices[used_count++] = forced;
    }
  }
}

static int find_best_video_mode_compat(const samp_display_mode_request *request, samp_rw_video_mode *best_mode) {
  samp_rw_get_num_video_modes_fn get_num_video_modes = (samp_rw_get_num_video_modes_fn)(uintptr_t)SAMP_ADDR_RW_ENGINE_GET_NUM_VIDEO_MODES;
  samp_rw_get_video_mode_info_fn get_video_mode_info =
      (samp_rw_get_video_mode_info_fn)(uintptr_t)SAMP_ADDR_RW_ENGINE_GET_VIDEO_MODE_INFO;
  int num_modes = 0;
  int best_index = -1;
  uint32_t best_score = 0xFFFFFFFFu;
  int i = 0;

  if (request == NULL || best_mode == NULL) {
    return -1;
  }

  memset(best_mode, 0, sizeof(*best_mode));
  num_modes = get_num_video_modes();
  if (num_modes <= 0 || num_modes > 4096) {
    runtime_tracef("select_device: invalid video mode count=%d", num_modes);
    return -1;
  }

  for (i = 0; i < num_modes; ++i) {
    samp_rw_video_mode mode;
    uint32_t score = 0;

    memset(&mode, 0, sizeof(mode));
    if (get_video_mode_info(&mode, (unsigned int)i) == NULL) {
      continue;
    }
    if (mode.width < 640 || mode.height < 480) {
      continue;
    }
    if ((mode.flags & SAMP_RW_VIDEOMODE_EXCLUSIVE) == 0) {
      continue;
    }

    score = video_mode_score_compat(&mode, request);
    if (best_index < 0 || score < best_score) {
      best_index = i;
      best_score = score;
      *best_mode = mode;
    }
  }

  runtime_tracef("select_device: modes=%d requested=%dx%dx%d adapter=%d best=%d best_mode=%dx%dx%d flags=0x%x score=%lu",
                 num_modes, request->width, request->height, request->depth, request->adapter, best_index,
                 best_mode->width, best_mode->height, best_mode->depth, (unsigned int)best_mode->flags,
                 (unsigned long)best_score);
  return best_index;
}

static int apply_startup_video_mode_compat(int *out_num_subsystems) {
  samp_rw_get_num_subsystems_fn get_num_subsystems =
      (samp_rw_get_num_subsystems_fn)(uintptr_t)SAMP_ADDR_RW_ENGINE_GET_NUM_SUBSYSTEMS;
  samp_rw_set_subsystem_fn set_subsystem = (samp_rw_set_subsystem_fn)(uintptr_t)SAMP_ADDR_RW_ENGINE_SET_SUBSYSTEM;
  samp_display_mode_request request;
  samp_rw_video_mode best_mode;
  int num_subsystems = 0;
  int best_index = -1;

  if (out_num_subsystems != NULL) {
    *out_num_subsystems = 0;
  }
  num_subsystems = get_num_subsystems();
  if (out_num_subsystems != NULL) {
    *out_num_subsystems = num_subsystems;
  }
  if (InterlockedCompareExchange(&g_runtime.select_device_video_mode_applied, 1, 0) != 0) {
    return 1;
  }

  load_display_mode_request_compat(&request);
  apply_default_resolution_patch_compat(&request);
  if (num_subsystems > 0 && request.adapter >= 0 && request.adapter < num_subsystems) {
    int subsystem_result = set_subsystem(request.adapter);
    (void)patch_write_u32(SAMP_ADDR_FRONTEND_CUR_ADAPTER, (uint32_t)request.adapter);
    runtime_tracef("select_device: adapters=%d selected_adapter=%d set_result=%d", num_subsystems, request.adapter,
                   subsystem_result);
  } else {
    runtime_tracef("select_device: adapters=%d requested_adapter=%d not selected", num_subsystems, request.adapter);
  }

  force_startup_display_modes_compat(&request);
  best_index = find_best_video_mode_compat(&request, &best_mode);
  if (best_index < 0) {
    return 0;
  }

  (void)patch_write_u32(SAMP_ADDR_FRONTEND_CUR_VIDEO_MODE, (uint32_t)best_index);
  (void)patch_write_u32(SAMP_ADDR_FRONTEND_SAVED_VIDEO_MODE, (uint32_t)best_index);
  runtime_tracef("select_device: applied mode_index=%d mode=%dx%dx%d ref=%d format=%d", best_index, best_mode.width,
                 best_mode.height, best_mode.depth, best_mode.ref_rate, best_mode.format);
  return 1;
}

static int __cdecl select_device_on_call_compat(void) {
  int num_subsystems = 0;
  int applied = 0;
  int dialog_enabled = 0;
  int force_dialog = 0;
  int result = 0;

  applied = apply_startup_video_mode_compat(&num_subsystems);
  dialog_enabled = env_flag_enabled_default_compat("SAMPDLL_DEVICE_SELECTION", 1);
  force_dialog = env_flag_enabled_default_compat("SAMPDLL_DEVICE_SELECTION_FORCE", 0);

  if (dialog_enabled && (num_subsystems > 1 || force_dialog)) {
    result = 1;
  } else if (num_subsystems > 1) {
    result = 2;
  } else {
    result = 0;
  }

  runtime_tracef("select_device: applied=%d adapters=%d dialog=%d force=%d result=%d", applied, num_subsystems,
                 dialog_enabled, force_dialog, result);
  return result;
}

static void write_u32_unaligned_compat(uint8_t *dst, uint32_t value) {
  if (dst == NULL) {
    return;
  }
  dst[0] = (uint8_t)(value & 0xFFu);
  dst[1] = (uint8_t)((value >> 8u) & 0xFFu);
  dst[2] = (uint8_t)((value >> 16u) & 0xFFu);
  dst[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static int build_select_device_stub_compat(uint8_t *code, size_t code_size) {
  size_t n = 0;
  size_t single_branch = 0;
  size_t show_branch = 0;
  size_t single_label = 0;
  size_t show_label = 0;

  if (code == NULL || code_size < SAMP_SELECT_DEVICE_STUB_SIZE || sizeof(uintptr_t) != 4u) {
    return 0;
  }

  memset(code, 0xCC, code_size);
  code[n++] = 0x60u; /* pushad */
  code[n++] = 0xB8u; /* mov eax, select_device_on_call_compat */
  write_u32_unaligned_compat(&code[n], (uint32_t)(uintptr_t)select_device_on_call_compat);
  n += 4u;
  code[n++] = 0xFFu; /* call eax */
  code[n++] = 0xD0u;
  code[n++] = 0x83u; /* cmp eax, 1 */
  code[n++] = 0xF8u;
  code[n++] = 0x01u;
  code[n++] = 0x61u; /* popad, flags from cmp are preserved */

  single_branch = n;
  code[n++] = 0x7Cu; /* jl single */
  code[n++] = 0x00u;
  show_branch = n;
  code[n++] = 0x74u; /* je multi show */
  code[n++] = 0x00u;

  code[n++] = 0xB8u; /* mov eax, 1; MTA-compatible multi-hide path */
  write_u32_unaligned_compat(&code[n], 1u);
  n += 4u;
  code[n++] = 0x68u; /* push RETURN_MULTI_HIDE */
  write_u32_unaligned_compat(&code[n], SAMP_ADDR_SELECT_DEVICE_RETURN_MULTI_HIDE);
  n += 4u;
  code[n++] = 0xC3u; /* ret */

  single_label = n;
  code[single_branch + 1u] = (uint8_t)(single_label - (single_branch + 2u));
  code[n++] = 0x68u; /* push RETURN_SINGLE */
  write_u32_unaligned_compat(&code[n], SAMP_ADDR_SELECT_DEVICE_RETURN_SINGLE);
  n += 4u;
  code[n++] = 0xC3u;

  show_label = n;
  code[show_branch + 1u] = (uint8_t)(show_label - (show_branch + 2u));
  code[n++] = 0x68u; /* push RETURN_MULTI_SHOW */
  write_u32_unaligned_compat(&code[n], SAMP_ADDR_SELECT_DEVICE_RETURN_MULTI_SHOW);
  n += 4u;
  code[n++] = 0xC3u;

  return (n <= code_size) ? 1 : 0;
}

static void install_select_device_hook_compat(void) {
  uint8_t *stub = NULL;
  uint8_t patch[SAMP_SELECT_DEVICE_HOOK_SIZE] = {0};

  if (!env_flag_enabled_default_compat("SAMPDLL_SELECT_DEVICE_HOOK", 1)) {
    runtime_tracef("select_device_hook: disabled by env");
    return;
  }
  if (InterlockedCompareExchange(&g_runtime.select_device_hook_attempted, 1, 0) != 0) {
    return;
  }
  if (sizeof(uintptr_t) != 4u) {
    runtime_tracef("select_device_hook: skipped because pointer size is %lu", (unsigned long)sizeof(uintptr_t));
    return;
  }

  stub = (uint8_t *)VirtualAlloc(NULL, SAMP_SELECT_DEVICE_STUB_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (stub == NULL) {
    runtime_tracef("select_device_hook: stub allocation failed gle=%lu", (unsigned long)GetLastError());
    return;
  }
  if (!build_select_device_stub_compat(stub, SAMP_SELECT_DEVICE_STUB_SIZE)) {
    VirtualFree(stub, 0u, MEM_RELEASE);
    runtime_tracef("select_device_hook: stub build failed");
    return;
  }
  FlushInstructionCache(GetCurrentProcess(), stub, SAMP_SELECT_DEVICE_STUB_SIZE);

  memcpy(g_runtime.select_device_hook_saved_code, (const void *)(uintptr_t)SAMP_ADDR_SELECT_DEVICE_HOOK,
         sizeof(g_runtime.select_device_hook_saved_code));
  g_runtime.select_device_hook_stub = (uintptr_t)stub;

  patch[0] = 0xFFu; /* jmp dword ptr [&g_runtime.select_device_hook_stub] */
  patch[1] = 0x25u;
  write_u32_unaligned_compat(&patch[2], (uint32_t)(uintptr_t)&g_runtime.select_device_hook_stub);
  if (!patch_copy(SAMP_ADDR_SELECT_DEVICE_HOOK, patch, sizeof(patch))) {
    g_runtime.select_device_hook_stub = 0u;
    VirtualFree(stub, 0u, MEM_RELEASE);
    runtime_tracef("select_device_hook: patch failed");
    return;
  }

  InterlockedExchange(&g_runtime.select_device_hook_installed, 1);
  runtime_tracef("select_device_hook: installed hook=0x%lx stub=%p saved=%02x %02x %02x %02x %02x %02x",
                 (unsigned long)SAMP_ADDR_SELECT_DEVICE_HOOK, stub, g_runtime.select_device_hook_saved_code[0],
                 g_runtime.select_device_hook_saved_code[1], g_runtime.select_device_hook_saved_code[2],
                 g_runtime.select_device_hook_saved_code[3], g_runtime.select_device_hook_saved_code[4],
                 g_runtime.select_device_hook_saved_code[5]);
}

static void uninstall_select_device_hook_compat(void) {
  uintptr_t stub = 0u;

  if (!InterlockedCompareExchange(&g_runtime.select_device_hook_installed, 0, 0)) {
    return;
  }

  (void)patch_copy(SAMP_ADDR_SELECT_DEVICE_HOOK, g_runtime.select_device_hook_saved_code,
                   sizeof(g_runtime.select_device_hook_saved_code));
  InterlockedExchange(&g_runtime.select_device_hook_installed, 0);
  stub = g_runtime.select_device_hook_stub;
  g_runtime.select_device_hook_stub = 0u;
  if (stub != 0u) {
    VirtualFree((void *)stub, 0u, MEM_RELEASE);
  }
  runtime_tracef("select_device_hook: uninstalled");
}

static int patch_zero_memory(uintptr_t addr, size_t size) {
  DWORD old_protect = 0;
  DWORD old_protect_restore = 0;

  if (addr == 0u || size == 0u) {
    return 0;
  }
  if (!VirtualProtect((LPVOID)addr, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
    return 0;
  }
  memset((void *)addr, 0, size);
  FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addr, size);
  (void)VirtualProtect((LPVOID)addr, size, old_protect, &old_protect_restore);
  return 1;
}

static void relocate_scan_list_compat_once(unsigned long *applied, unsigned long *total) {
  static const uintptr_t kScanReloc1Usa[14] = {0x5DC7AAu, 0x41A85Du, 0x41A864u, 0x408259u, 0x711B32u,
                                               0x699CF8u, 0x4092ECu, 0x40914Eu, 0x408702u, 0x564220u,
                                               0x564172u, 0x563845u, 0x84E9C2u, 0x85652Du};
  static const uintptr_t kScanReloc1Eu[14] = {0x5DC7AAu, 0x41A85Du, 0x41A864u, 0x408261u, 0x711B32u,
                                              0x699CF8u, 0x4092ECu, 0x40914Eu, 0x408702u, 0x564220u,
                                              0x564172u, 0x563845u, 0x84EA02u, 0x85656Du};
  static const uintptr_t kScanReloc2Usa[56] = {
      0x0040D68Cu, 0x005664D7u, 0x00566586u, 0x00408706u, 0x0056B3B1u, 0x0056AD91u, 0x0056A85Fu,
      0x005675FAu, 0x0056CD84u, 0x0056CC79u, 0x0056CB51u, 0x0056CA4Au, 0x0056C664u, 0x0056C569u,
      0x0056C445u, 0x0056C341u, 0x0056BD46u, 0x0056BC53u, 0x0056BE56u, 0x0056A940u, 0x00567735u,
      0x00546738u, 0x0054BB23u, 0x006E31AAu, 0x0040DC29u, 0x00534A09u, 0x00534D6Bu, 0x00564B59u,
      0x00564DA9u, 0x0067FF5Du, 0x00568CB9u, 0x00568EFBu, 0x00569F57u, 0x00569537u, 0x00569127u,
      0x0056B4B5u, 0x0056B594u, 0x0056B2C3u, 0x0056AF74u, 0x0056AE95u, 0x0056BF4Fu, 0x0056ACA3u,
      0x0056A766u, 0x0056A685u, 0x0070B9BAu, 0x0056479Du, 0x0070ACB2u, 0x006063C7u, 0x00699CFEu,
      0x0041A861u, 0x0040E061u, 0x0040DF5Eu, 0x0040DDCEu, 0x0040DB0Eu, 0x0040D98Cu, 0x01566855u};
  static const uintptr_t kScanReloc2Eu[56] = {
      0x0040D68Cu, 0x005664D7u, 0x00566586u, 0x00408706u, 0x0056B3B1u, 0x0056AD91u, 0x0056A85Fu,
      0x005675FAu, 0x0056CD84u, 0x0056CC79u, 0x0056CB51u, 0x0056CA4Au, 0x0056C664u, 0x0056C569u,
      0x0056C445u, 0x0056C341u, 0x0056BD46u, 0x0056BC53u, 0x0056BE56u, 0x0056A940u, 0x00567735u,
      0x00546738u, 0x0054BB23u, 0x006E31AAu, 0x0040DC29u, 0x00534A09u, 0x00534D6Bu, 0x00564B59u,
      0x00564DA9u, 0x0067FF5Du, 0x00568CB9u, 0x00568EFBu, 0x00569F57u, 0x00569537u, 0x00569127u,
      0x0056B4B5u, 0x0056B594u, 0x0056B2C3u, 0x0056AF74u, 0x0056AE95u, 0x0056BF4Fu, 0x0056ACA3u,
      0x0056A766u, 0x0056A685u, 0x0070B9BAu, 0x0056479Du, 0x0070ACB2u, 0x006063C7u, 0x00699CFEu,
      0x0041A861u, 0x0040E061u, 0x0040DF5Eu, 0x0040DDCEu, 0x0040DB0Eu, 0x0040D98Cu, 0x01566845u};
  static const uintptr_t kScanReloc3[11] = {0x004091C5u, 0x00409367u, 0x0040D9C5u, 0x0040DB47u,
                                            0x0040DC61u, 0x0040DE07u, 0x0040DF97u, 0x0040E09Au,
                                            0x00534A98u, 0x00534DFAu, 0x0071CDB0u};
  static const uintptr_t kScanRelocEnd[4] = {0x005634A6u, 0x005638DFu, 0x0056420Fu, 0x00564283u};
  const uintptr_t *scan_reloc1 = NULL;
  const uintptr_t *scan_reloc2 = NULL;
  uintptr_t scan_base = (uintptr_t)&g_scan_list_memory[0];
  uintptr_t scan_end = scan_base + sizeof(g_scan_list_memory);
  LONG version = InterlockedCompareExchange(&g_runtime.gta_version, 0, 0);
  unsigned long local_applied = 0;
  unsigned long local_total = 0;
  size_t i = 0;

  if (version == SAMP_GTA_VERSION_USA10) {
    scan_reloc1 = kScanReloc1Usa;
    scan_reloc2 = kScanReloc2Usa;
  } else if (version == SAMP_GTA_VERSION_EU10) {
    scan_reloc1 = kScanReloc1Eu;
    scan_reloc2 = kScanReloc2Eu;
  } else {
    runtime_tracef("scanlist_patch: skipped unknown gta_version=%ld", (long)version);
    return;
  }

  memset(g_scan_list_memory, 0, sizeof(g_scan_list_memory));

  for (i = 0; i < 14u; ++i) {
    ++local_total;
    if (patch_write_u32(scan_reloc1[i], (uint32_t)scan_base)) {
      ++local_applied;
    }
  }
  for (i = 0; i < 56u; ++i) {
    ++local_total;
    if (patch_write_u32(scan_reloc2[i] + 3u, (uint32_t)scan_base)) {
      ++local_applied;
    }
  }
  for (i = 0; i < 11u; ++i) {
    ++local_total;
    if (patch_write_u32(kScanReloc3[i] + 3u, (uint32_t)(scan_base + 4u))) {
      ++local_applied;
    }
  }
  for (i = 0; i < 4u; ++i) {
    ++local_total;
    if (patch_write_u32(kScanRelocEnd[i], (uint32_t)scan_end)) {
      ++local_applied;
    }
  }

  ++local_total;
  if (patch_write_u32(0x40936Au, (uint32_t)(scan_base + 4u))) {
    ++local_applied;
  }
  ++local_total;
  if (patch_zero_memory(SAMP_ADDR_SCANLIST_ORIGINAL, SAMP_SCANLIST_ORIGINAL_RESET_SIZE)) {
    ++local_applied;
  }

  if (applied != NULL) {
    *applied += local_applied;
  }
  if (total != NULL) {
    *total += local_total;
  }
  runtime_tracef("scanlist_patch: version=%ld base=0x%08lx end=0x%08lx applied=%lu/%lu clump_offset=0x%08lx",
                 (long)version, (unsigned long)scan_base, (unsigned long)scan_end, local_applied, local_total,
                 (unsigned long)(*(volatile uint32_t *)(uintptr_t)SAMP_ADDR_RPANIMBLEND_CLUMP_OFFSET));
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
  LONG spawn_finalized = InterlockedCompareExchange(&g_runtime.mp_session_spawn_finalized, 0, 0);

  if (g_runtime.settings.play_online && !run_game_scripts_enabled() && spawn_finalized == 0) {
    write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0x8Bu);
    write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD0u);
    if (InterlockedCompareExchange(&g_runtime.script_process_suppressed_logged, 1, 0) == 0) {
      runtime_tracef("script_hook: suppressed CTheScripts::Process before online spawn");
    }
    return;
  }

  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE, 0xFFu);
  write_game_u8(SAMP_ADDR_SCRIPT_PROCESS_GATE2, 0xD2u);

  if (g_runtime.settings.play_online && spawn_finalized != 0 &&
      InterlockedCompareExchange(&g_runtime.script_process_allowed_after_spawn_logged, 1, 0) == 0) {
    /*
     * OLD_02X_REF + INFERRED:
     * Legacy SA-MP starts GTA's normal world loop and only steers spawn/session state around it. Keeping
     * CTheScripts::Process suppressed after spawn starves GTA-SA's own world/streaming process in long drives.
     * TODO_VERIFY against an original 0.3.7 golden trace with the ASI probe.
     */
    runtime_tracef("script_hook: allowing CTheScripts::Process after online spawn");
  }

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
    InterlockedExchange(&g_runtime.gta_version, SAMP_GTA_VERSION_USA10);
    (void)patch_nop(SAMP_ADDR_BYPASS_VIDS_USA10, 6u);
    write_game_u8(SAMP_ADDR_ENTRY, 5u);
    runtime_tracef("pregame_patch: USA10 bypass_videos patched entry %ld->%ld", (long)entry_before,
                   (long)read_game_entry_gate_value());
    return;
  }

  if (eu_probe == 0xC8u) {
    InterlockedExchange(&g_runtime.gta_version, SAMP_GTA_VERSION_EU10);
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
  /*
   * OLD_02X_REF + TODO_VERIFY:
   * Legacy 0.2x disables GTA's high-speed motion blur with a 5-byte NOP at 0x704E8A. Apply the same GTA-SA
   * address conservatively until the original 0.3.7 samp.dll patch bytes are captured.
   */
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_HIGH_SPEED_MOTION_BLUR_PATCH, 5u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_PLAYERPED_DONT_RESTORE_PLAYER_ANIMS, 6u));
  SAMP_APPLY_PATCH(patch_nop(SAMP_ADDR_PLAYERPED_PROCESS_CONTROL_ANIM_CRASH, 39u));
  relocate_scan_list_compat_once(&applied, &total);

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

static int valid_world_time_compat(uint8_t hour, uint8_t minute) {
  return hour < 24u && minute < 60u;
}

static void apply_network_time_weather_compat(const char *reason) {
  DWORD weather_value = (DWORD)g_runtime.raknet_weather;
  int time_valid = valid_world_time_compat(g_runtime.raknet_world_hour, g_runtime.raknet_world_minute) &&
                   g_runtime.raknet_world_time_valid != 0u;

  /*
   * OLD_02X_REF + PROBE_TRACE + INFERRED:
   * CNetGame::Process() keeps weather fixed every tick and re-applies world time while HoldTime is active.
   * 0.3.7-R5 traces from open.mp show WorldTime(id=94), SetTimeEx(id=29) and ToggleClock(id=144); apply only
   * bounds-checked values so the observed invalid InitGame time byte cannot corrupt GTA's clock state.
   */
  if (time_valid) {
    write_game_u8(SAMP_ADDR_WORLD_HOUR, g_runtime.raknet_world_hour);
    write_game_u8(SAMP_ADDR_WORLD_MINUTE, g_runtime.raknet_world_minute);
  }
  *(volatile DWORD *)(uintptr_t)SAMP_ADDR_WEATHER_A = weather_value;
  *(volatile DWORD *)(uintptr_t)SAMP_ADDR_WEATHER_B = weather_value;

  if (InterlockedCompareExchange(&g_runtime.raknet_time_apply_logged, 1, 0) == 0) {
    runtime_tracef("time_weather_apply: reason=%s time_valid=%d time=%u:%02u clock=%u hold=%u weather=%u",
                   reason != NULL ? reason : "unknown", time_valid, (unsigned)g_runtime.raknet_world_hour,
                   (unsigned)g_runtime.raknet_world_minute, (unsigned)g_runtime.raknet_clock_enabled,
                   (unsigned)g_runtime.raknet_hold_time, (unsigned)g_runtime.raknet_weather);
  }
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
      "LOAD_KICK", (long)net_state, (long)entry_before,
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
  LONG entry_target = 9;
  uint8_t game_started_target = 0u;
  LONG logged = 0;
  int changed = 0;

  *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = entry_target;
  write_game_u8(SAMP_ADDR_GAME_STARTED, game_started_target);
  write_game_u8(SAMP_ADDR_STARTGAME, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);
  write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
  write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);

  changed = entry_before != entry_target || game_started_before != game_started_target || startgame_before != 0u ||
            menu_before != 0u || menu2_before != 0u || menu3_before != 0u || hud_before != 0u || radar_before != 1u;
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
  if (InterlockedCompareExchange(&g_runtime.start_game_called, 0, 0) != 0) {
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

  if (snapshot->init_world_time < 24u) {
    g_runtime.raknet_world_hour = snapshot->init_world_time;
    g_runtime.raknet_world_minute = 0u;
    g_runtime.raknet_world_time_valid = 1u;
    write_game_u8(SAMP_ADDR_WORLD_HOUR, snapshot->init_world_time);
    write_game_u8(SAMP_ADDR_WORLD_MINUTE, 0u);
  } else {
    runtime_tracef("init_game_apply: ignoring invalid init_world_time=%u", (unsigned)snapshot->init_world_time);
  }
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
  LONG previous_player_pos_seq = 0;
  LONG previous_player_facing_seq = 0;
  LONG previous_player_health_seq = 0;
  LONG previous_player_controllable_seq = 0;
  LONG previous_camera_behind_seq = 0;
  LONG previous_player_armour_seq = 0;
  LONG previous_player_armed_weapon_seq = 0;
  LONG previous_reset_player_weapons_seq = 0;
  LONG previous_reset_player_money_seq = 0;
  LONG previous_play_sound_seq = 0;
  LONG previous_stop_audio_stream_seq = 0;
  LONG previous_player_color_seq = 0;
  LONG previous_player_team_seq = 0;
  LONG previous_apply_animation_seq = 0;
  LONG previous_world_visual_event_seq = 0;
  LONG previous_spawn_info_seq = 0;
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
  dialog_compat_update_from_snapshot(&snapshot);
  textdraw_compat_update_from_snapshot(&snapshot);
  scoreboard_compat_update_from_snapshot(&snapshot);
  object_compat_update_from_snapshot(&snapshot);
  vehicle_compat_update_from_snapshot(&snapshot);
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
  previous_player_pos_seq = InterlockedCompareExchange(&g_runtime.raknet_player_pos_seq, 0, 0);
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_PLAYER_POS) != 0u && snapshot.player_pos_seq != 0u &&
      snapshot.player_pos_seq != (uint32_t)previous_player_pos_seq) {
    memcpy(g_runtime.raknet_player_pos, snapshot.player_pos, sizeof(g_runtime.raknet_player_pos));
    InterlockedExchange(&g_runtime.raknet_player_pos_seq, (LONG)snapshot.player_pos_seq);
    runtime_tracef("network_prepare: player_pos seq=%lu previous=%ld pos=(%.3f,%.3f,%.3f)",
                   (unsigned long)snapshot.player_pos_seq, (long)previous_player_pos_seq,
                   (double)snapshot.player_pos[0], (double)snapshot.player_pos[1],
                   (double)snapshot.player_pos[2]);
  }
  previous_player_facing_seq = InterlockedCompareExchange(&g_runtime.raknet_player_facing_seq, 0, 0);
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_PLAYER_FACING) != 0u && snapshot.player_facing_seq != 0u &&
      snapshot.player_facing_seq != (uint32_t)previous_player_facing_seq) {
    g_runtime.raknet_player_facing_angle = snapshot.player_facing_angle;
    InterlockedExchange(&g_runtime.raknet_player_facing_seq, (LONG)snapshot.player_facing_seq);
    runtime_tracef("network_prepare: player_facing seq=%lu previous=%ld angle=%.3f",
                   (unsigned long)snapshot.player_facing_seq, (long)previous_player_facing_seq,
                   (double)snapshot.player_facing_angle);
  }
  previous_player_health_seq = InterlockedCompareExchange(&g_runtime.raknet_player_health_seq, 0, 0);
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_PLAYER_HEALTH) != 0u && snapshot.player_health_seq != 0u &&
      snapshot.player_health_seq != (uint32_t)previous_player_health_seq) {
    g_runtime.raknet_player_health = snapshot.player_health;
    InterlockedExchange(&g_runtime.raknet_player_health_seq, (LONG)snapshot.player_health_seq);
    runtime_tracef("network_prepare: player_health seq=%lu previous=%ld health=%.3f",
                   (unsigned long)snapshot.player_health_seq, (long)previous_player_health_seq,
                   (double)snapshot.player_health);
  }
  previous_player_controllable_seq =
      InterlockedCompareExchange(&g_runtime.raknet_player_controllable_seq, 0, 0);
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_PLAYER_CONTROLLABLE) != 0u &&
      snapshot.player_controllable_seq != 0u &&
      snapshot.player_controllable_seq != (uint32_t)previous_player_controllable_seq) {
    g_runtime.raknet_player_controllable = snapshot.player_controllable != 0u ? 1u : 0u;
    InterlockedExchange(&g_runtime.raknet_player_controllable_seq, (LONG)snapshot.player_controllable_seq);
    runtime_tracef("network_prepare: player_controllable seq=%lu previous=%ld controllable=%u",
                   (unsigned long)snapshot.player_controllable_seq, (long)previous_player_controllable_seq,
                   (unsigned)g_runtime.raknet_player_controllable);
  }
  previous_camera_behind_seq = InterlockedCompareExchange(&g_runtime.raknet_camera_behind_seq, 0, 0);
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_CAMERA_BEHIND) != 0u && snapshot.camera_behind_seq != 0u &&
      snapshot.camera_behind_seq != (uint32_t)previous_camera_behind_seq) {
    InterlockedExchange(&g_runtime.raknet_camera_behind_seq, (LONG)snapshot.camera_behind_seq);
    runtime_tracef("network_prepare: camera_behind seq=%lu previous=%ld",
                   (unsigned long)snapshot.camera_behind_seq, (long)previous_camera_behind_seq);
  }
  previous_player_armour_seq = InterlockedCompareExchange(&g_runtime.raknet_player_armour_seq, 0, 0);
  if (snapshot.player_armour_seq != 0u && snapshot.player_armour_seq != (uint32_t)previous_player_armour_seq) {
    g_runtime.raknet_player_armour = snapshot.player_armour;
    InterlockedExchange(&g_runtime.raknet_player_armour_seq, (LONG)snapshot.player_armour_seq);
    runtime_tracef("network_prepare: player_armour seq=%lu previous=%ld armour=%.3f",
                   (unsigned long)snapshot.player_armour_seq, (long)previous_player_armour_seq,
                   (double)snapshot.player_armour);
  }
  previous_player_armed_weapon_seq = InterlockedCompareExchange(&g_runtime.raknet_player_armed_weapon_seq, 0, 0);
  if (snapshot.player_armed_weapon_seq != 0u &&
      snapshot.player_armed_weapon_seq != (uint32_t)previous_player_armed_weapon_seq) {
    g_runtime.raknet_player_armed_weapon = snapshot.player_armed_weapon;
    InterlockedExchange(&g_runtime.raknet_player_armed_weapon_seq, (LONG)snapshot.player_armed_weapon_seq);
    runtime_tracef("network_prepare: armed_weapon seq=%lu previous=%ld weapon=%lu",
                   (unsigned long)snapshot.player_armed_weapon_seq, (long)previous_player_armed_weapon_seq,
                   (unsigned long)snapshot.player_armed_weapon);
  }
  previous_reset_player_weapons_seq = InterlockedCompareExchange(&g_runtime.raknet_reset_player_weapons_seq, 0, 0);
  if (snapshot.reset_player_weapons_seq != 0u &&
      snapshot.reset_player_weapons_seq != (uint32_t)previous_reset_player_weapons_seq) {
    InterlockedExchange(&g_runtime.raknet_reset_player_weapons_seq, (LONG)snapshot.reset_player_weapons_seq);
    runtime_tracef("network_prepare: reset_weapons seq=%lu previous=%ld",
                   (unsigned long)snapshot.reset_player_weapons_seq, (long)previous_reset_player_weapons_seq);
  }
  previous_reset_player_money_seq = InterlockedCompareExchange(&g_runtime.raknet_reset_player_money_seq, 0, 0);
  if (snapshot.reset_player_money_seq != 0u &&
      snapshot.reset_player_money_seq != (uint32_t)previous_reset_player_money_seq) {
    InterlockedExchange(&g_runtime.raknet_reset_player_money_seq, (LONG)snapshot.reset_player_money_seq);
    runtime_tracef("network_prepare: reset_money seq=%lu previous=%ld",
                   (unsigned long)snapshot.reset_player_money_seq, (long)previous_reset_player_money_seq);
  }
  previous_play_sound_seq = InterlockedCompareExchange(&g_runtime.raknet_play_sound_seq, 0, 0);
  if (snapshot.play_sound_seq != 0u && snapshot.play_sound_seq != (uint32_t)previous_play_sound_seq) {
    g_runtime.raknet_play_sound_id = snapshot.play_sound_id;
    memcpy(g_runtime.raknet_play_sound_pos, snapshot.play_sound_pos, sizeof(g_runtime.raknet_play_sound_pos));
    InterlockedExchange(&g_runtime.raknet_play_sound_seq, (LONG)snapshot.play_sound_seq);
    runtime_tracef("network_prepare: play_sound seq=%lu previous=%ld sound=%lu pos=(%.3f,%.3f,%.3f)",
                   (unsigned long)snapshot.play_sound_seq, (long)previous_play_sound_seq,
                   (unsigned long)snapshot.play_sound_id, (double)snapshot.play_sound_pos[0],
                   (double)snapshot.play_sound_pos[1], (double)snapshot.play_sound_pos[2]);
  }
  previous_stop_audio_stream_seq = InterlockedCompareExchange(&g_runtime.raknet_stop_audio_stream_seq, 0, 0);
  if (snapshot.stop_audio_stream_seq != 0u &&
      snapshot.stop_audio_stream_seq != (uint32_t)previous_stop_audio_stream_seq) {
    InterlockedExchange(&g_runtime.raknet_stop_audio_stream_seq, (LONG)snapshot.stop_audio_stream_seq);
    runtime_tracef("network_prepare: stop_audio_stream seq=%lu previous=%ld decoded_only=1",
                   (unsigned long)snapshot.stop_audio_stream_seq, (long)previous_stop_audio_stream_seq);
  }
  previous_player_color_seq = InterlockedCompareExchange(&g_runtime.raknet_player_color_seq, 0, 0);
  if (snapshot.player_color_seq != 0u && snapshot.player_color_seq != (uint32_t)previous_player_color_seq) {
    g_runtime.raknet_player_color_player_id = snapshot.player_color_player_id;
    g_runtime.raknet_player_color = snapshot.player_color;
    InterlockedExchange(&g_runtime.raknet_player_color_seq, (LONG)snapshot.player_color_seq);
    runtime_tracef("network_prepare: player_color seq=%lu previous=%ld player=%u color=0x%08lx decoded_only=1",
                   (unsigned long)snapshot.player_color_seq, (long)previous_player_color_seq,
                   (unsigned)snapshot.player_color_player_id, (unsigned long)snapshot.player_color);
  }
  previous_player_team_seq = InterlockedCompareExchange(&g_runtime.raknet_player_team_seq, 0, 0);
  if (snapshot.player_team_seq != 0u && snapshot.player_team_seq != (uint32_t)previous_player_team_seq) {
    g_runtime.raknet_player_team_player_id = snapshot.player_team_player_id;
    g_runtime.raknet_player_team = snapshot.player_team;
    InterlockedExchange(&g_runtime.raknet_player_team_seq, (LONG)snapshot.player_team_seq);
    runtime_tracef("network_prepare: player_team seq=%lu previous=%ld player=%u team=%u decoded_only=1",
                   (unsigned long)snapshot.player_team_seq, (long)previous_player_team_seq,
                   (unsigned)snapshot.player_team_player_id, (unsigned)snapshot.player_team);
  }
  previous_apply_animation_seq = InterlockedCompareExchange(&g_runtime.raknet_apply_animation_seq, 0, 0);
  if (snapshot.apply_animation_seq != 0u && snapshot.apply_animation_seq != (uint32_t)previous_apply_animation_seq) {
    g_runtime.raknet_apply_animation_player_id = snapshot.apply_animation_player_id;
    strncpy(g_runtime.raknet_apply_animation_lib, snapshot.apply_animation_lib,
            sizeof(g_runtime.raknet_apply_animation_lib) - 1u);
    g_runtime.raknet_apply_animation_lib[sizeof(g_runtime.raknet_apply_animation_lib) - 1u] = '\0';
    strncpy(g_runtime.raknet_apply_animation_name, snapshot.apply_animation_name,
            sizeof(g_runtime.raknet_apply_animation_name) - 1u);
    g_runtime.raknet_apply_animation_name[sizeof(g_runtime.raknet_apply_animation_name) - 1u] = '\0';
    g_runtime.raknet_apply_animation_delta = snapshot.apply_animation_delta;
    g_runtime.raknet_apply_animation_loop = snapshot.apply_animation_loop;
    g_runtime.raknet_apply_animation_lock_x = snapshot.apply_animation_lock_x;
    g_runtime.raknet_apply_animation_lock_y = snapshot.apply_animation_lock_y;
    g_runtime.raknet_apply_animation_freeze = snapshot.apply_animation_freeze;
    g_runtime.raknet_apply_animation_time = snapshot.apply_animation_time;
    InterlockedExchange(&g_runtime.raknet_apply_animation_seq, (LONG)snapshot.apply_animation_seq);
    runtime_tracef("network_prepare: apply_animation seq=%lu previous=%ld player=%u lib='%s' name='%s' decoded_only=1",
                   (unsigned long)snapshot.apply_animation_seq, (long)previous_apply_animation_seq,
                   (unsigned)snapshot.apply_animation_player_id, snapshot.apply_animation_lib,
                   snapshot.apply_animation_name);
  }
  previous_world_visual_event_seq = InterlockedCompareExchange(&g_runtime.raknet_world_visual_event_seq, 0, 0);
  if (snapshot.world_visual_event_seq != 0u &&
      snapshot.world_visual_event_seq != (uint32_t)previous_world_visual_event_seq) {
    g_runtime.raknet_world_visual_event_type = snapshot.world_visual_event_type;
    g_runtime.raknet_world_visual_id = snapshot.world_visual_id;
    g_runtime.raknet_world_visual_model = snapshot.world_visual_model;
    g_runtime.raknet_world_visual_color = snapshot.world_visual_color;
    memcpy(g_runtime.raknet_world_visual_pos, snapshot.world_visual_pos,
           sizeof(g_runtime.raknet_world_visual_pos));
    strncpy(g_runtime.raknet_world_visual_text, snapshot.world_visual_text,
            sizeof(g_runtime.raknet_world_visual_text) - 1u);
    g_runtime.raknet_world_visual_text[sizeof(g_runtime.raknet_world_visual_text) - 1u] = '\0';
    InterlockedExchange(&g_runtime.raknet_world_visual_event_seq, (LONG)snapshot.world_visual_event_seq);
    runtime_tracef("network_prepare: world_visual seq=%lu previous=%ld type=%u id=%u model=%ld color=0x%08lx text='%.96s'",
                   (unsigned long)snapshot.world_visual_event_seq, (long)previous_world_visual_event_seq,
                   (unsigned)snapshot.world_visual_event_type, (unsigned)snapshot.world_visual_id,
                   (long)snapshot.world_visual_model, (unsigned long)snapshot.world_visual_color,
                   snapshot.world_visual_text);
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_WEATHER) != 0u) {
    g_runtime.raknet_weather = snapshot.weather;
  }
  if ((snapshot.flags & (SAMP_RAKNET_RPC_FLAG_WORLD_TIME | SAMP_RAKNET_RPC_FLAG_SET_TIME_EX)) != 0u &&
      valid_world_time_compat(snapshot.world_time_hour, snapshot.world_time_minute)) {
    g_runtime.raknet_world_hour = snapshot.world_time_hour;
    g_runtime.raknet_world_minute = snapshot.world_time_minute;
    g_runtime.raknet_world_time_valid = 1u;
  }
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_TOGGLE_CLOCK) != 0u) {
    g_runtime.raknet_clock_enabled = snapshot.clock_enabled != 0u ? 1u : 0u;
    g_runtime.raknet_hold_time = snapshot.clock_enabled != 0u ? 0u : 1u;
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
  previous_spawn_info_seq = InterlockedCompareExchange(&g_runtime.raknet_spawn_info_seq, 0, 0);
  if ((snapshot.flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) != 0u && snapshot.spawn_info_seq != 0u &&
      snapshot.spawn_info_seq != (uint32_t)previous_spawn_info_seq) {
    g_runtime.raknet_spawn_team = snapshot.spawn_team;
    g_runtime.raknet_spawn_skin = snapshot.spawn_skin;
    memcpy(g_runtime.raknet_spawn_pos, snapshot.spawn_pos, sizeof(g_runtime.raknet_spawn_pos));
    g_runtime.raknet_spawn_rotation = snapshot.spawn_rotation;
    memcpy(g_runtime.raknet_spawn_weapons, snapshot.spawn_weapons, sizeof(g_runtime.raknet_spawn_weapons));
    memcpy(g_runtime.raknet_spawn_weapon_ammo, snapshot.spawn_weapon_ammo, sizeof(g_runtime.raknet_spawn_weapon_ammo));
    InterlockedExchange(&g_runtime.raknet_spawn_info_seq, (LONG)snapshot.spawn_info_seq);
    InterlockedExchange(&g_runtime.mp_session_spawn_finalized, 0);
    InterlockedExchange(&g_runtime.mp_session_scene_loaded, 0);
    runtime_tracef("network_prepare: spawn_info seq=%lu previous=%ld team=%u skin=%ld pos=(%.3f,%.3f,%.3f) rot=%.3f",
                   (unsigned long)snapshot.spawn_info_seq, (long)previous_spawn_info_seq,
                   (unsigned)snapshot.spawn_team, (long)snapshot.spawn_skin, (double)snapshot.spawn_pos[0],
                   (double)snapshot.spawn_pos[1], (double)snapshot.spawn_pos[2],
                   (double)snapshot.spawn_rotation);
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

  if (snapshot.spawn_info_seq == 0u && ((uint32_t)previous_flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) == 0u &&
      (snapshot.flags & SAMP_RAKNET_RPC_FLAG_SPAWN_INFO) != 0u) {
    runtime_tracef("network_prepare: spawn_info team=%u skin=%ld pos=(%.3f,%.3f,%.3f) rot=%.3f",
                   (unsigned)snapshot.spawn_team, (long)snapshot.spawn_skin, (double)snapshot.spawn_pos[0],
                   (double)snapshot.spawn_pos[1], (double)snapshot.spawn_pos[2],
                   (double)snapshot.spawn_rotation);
  }

  if ((uint32_t)previous_game_rpc_flags != game_rpc_flags) {
    runtime_tracef(
        "network_prepare: game_rpc flags=0x%08lx player_pos=%u facing=%u weather=%u interior=%u camera_pos=%u "
        "camera_look=%u health=%u controllable=%u camera_behind=%u pos=(%.3f,%.3f,%.3f) angle=%.3f "
        "health_value=%.3f controllable_value=%u weather_id=%u interior_id=%u cam=(%.3f,%.3f,%.3f) "
        "look=(%.3f,%.3f,%.3f) look_type=%u observe_only=1",
        (unsigned long)game_rpc_flags, (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_POS) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_FACING) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_WEATHER) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_INTERIOR) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_CAMERA_POS) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_HEALTH) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_CONTROLLABLE) ? 1u : 0u,
        (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_CAMERA_BEHIND) ? 1u : 0u, (double)snapshot.player_pos[0],
        (double)snapshot.player_pos[1], (double)snapshot.player_pos[2], (double)snapshot.player_facing_angle,
        (double)snapshot.player_health, (unsigned)snapshot.player_controllable,
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
  apply_network_time_weather_compat("online_session");

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

  if (entry_before != 9 || game_started_before != 0u) {
    LONG drift_seen = InterlockedCompareExchange(&g_runtime.online_session_drift_seen, 0, 0);
    LONG last_entry = InterlockedCompareExchange(&g_runtime.online_session_last_entry, 0, 0);
    LONG last_started = InterlockedCompareExchange(&g_runtime.online_session_last_game_started, 0, 0);
    if (drift_seen == 0 || last_entry != entry_before || last_started != (LONG)game_started_before) {
      InterlockedExchange(&g_runtime.online_session_drift_seen, 1);
      InterlockedExchange(&g_runtime.online_session_last_entry, entry_before);
      InterlockedExchange(&g_runtime.online_session_last_game_started, (LONG)game_started_before);
      runtime_tracef(
          "online_session_drift: state=%ld rpc=0x%08lx observed entry=%ld game_started=%u expected=9/0 "
          "action=correct",
          (long)net_state, (unsigned long)rpc_flags, (long)entry_before, (unsigned)game_started_before);
    }
    *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 9;
    write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
  }

  if (startgame_before != 0u) {
    write_game_u8(SAMP_ADDR_STARTGAME, 0u);
    runtime_tracef(
        "online_session_startgame_clear: state=%ld rpc=0x%08lx entry=%ld game_started=%u startgame %u->0 "
        "menu=%u/%u/%u evidence=OLD_02X_REF,PROBE_TRACE,TODO_VERIFY",
        (long)net_state, (unsigned long)rpc_flags, (long)entry_before, (unsigned)game_started_before,
        (unsigned)startgame_before, (unsigned)menu_before, (unsigned)menu2_before, (unsigned)menu3_before);
  }

  /*
   * OLD_02X_REF + PROBE_TRACE:
   * During a spawned online session, ESC sets the GTA frontend/menu byte to 1. Clearing it every Process tick
   * immediately closes the pause menu. The 0.2x client only clears ADDR_MENU during CGame::StartGame(), then
   * reads IsMenuActive() to skip SA-MP overlay drawing while GTA's frontend is active. Keep the pre-spawn
   * frontend clamps above, but leave the real GTA menu flags alone once RequestSpawn/Spawn has succeeded.
   */
  if (menu_before != 0u || menu2_before != 0u || menu3_before != 0u) {
    return;
  }

  write_game_u8(SAMP_ADDR_ENABLE_HUD, 1u);
  write_game_u8(SAMP_ADDR_RADAR_BLANK, 0u);

  if (startgame_before == 0u) {
    return;
  }
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

static int gta_script_execute_with_locals_compat(const uint32_t *locals, uint32_t local_count) {
#if defined(__i386__) || defined(_M_IX86)
  uintptr_t thread = (uintptr_t)g_runtime.script_thread;
  uintptr_t script_buf = (uintptr_t)g_runtime.script_buf;
  uintptr_t process_one_command = (uintptr_t)SAMP_ADDR_PROCESS_ONE_COMMAND;
  uint32_t copy_count = local_count;

  if (!ensure_script_gate_writable()) {
    return 0;
  }

  memset(&g_runtime.script_thread[0x3C], 0, 18u * sizeof(uint32_t));
  if (locals != NULL && copy_count > 0u) {
    if (copy_count > 18u) {
      copy_count = 18u;
    }
    memcpy(&g_runtime.script_thread[0x3C], locals, copy_count * sizeof(uint32_t));
  }
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
  uint32_t local_values[18];
  uint32_t *local_outputs[18];
  uint16_t var_pos = 0u;
  int result = 0;

  memset(g_runtime.script_buf, 0, sizeof(g_runtime.script_buf));
  memcpy(g_runtime.script_buf, &opcode, sizeof(opcode));
  memset(local_values, 0, sizeof(local_values));
  memset(local_outputs, 0, sizeof(local_outputs));

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
      case 'v': {
        uint32_t *value = va_arg(args, uint32_t *);
        if (var_pos >= 18u || pos + 3u > SAMP_SCRIPT_BUF_BYTES) {
          va_end(args);
          return 0;
        }
        g_runtime.script_buf[pos++] = 0x03u;
        memcpy(&g_runtime.script_buf[pos], &var_pos, sizeof(var_pos));
        pos += sizeof(var_pos);
        local_outputs[var_pos] = value;
        local_values[var_pos] = value != NULL ? *value : 0u;
        ++var_pos;
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

  result = gta_script_execute_with_locals_compat(local_values, var_pos);
  if (result && var_pos > 0u) {
    uint16_t i = 0u;
    for (i = 0u; i < var_pos; ++i) {
      if (local_outputs[i] != NULL) {
        memcpy(local_outputs[i], &g_runtime.script_thread[0x3C + ((size_t)i * sizeof(uint32_t))], sizeof(uint32_t));
      }
    }
  }

  return result;
}

static void gta_streaming_request_model_compat(int32_t model_id, int32_t flags) {
  typedef void(__cdecl *gta_streaming_request_model_fn)(int32_t, int32_t);
  gta_streaming_request_model_fn request_model =
      (gta_streaming_request_model_fn)(uintptr_t)SAMP_ADDR_STREAMING_REQUEST_MODEL;

  if (model_id < 0) {
    return;
  }
  request_model(model_id, flags);
}

static void gta_streaming_load_all_requested_compat(int only_priority_requests) {
  typedef void(__cdecl *gta_streaming_load_all_requested_fn)(int);
  gta_streaming_load_all_requested_fn load_all =
      (gta_streaming_load_all_requested_fn)(uintptr_t)SAMP_ADDR_STREAMING_LOAD_ALL_REQUESTED;

  load_all(only_priority_requests ? 1 : 0);
}

static void gta_streaming_load_scene_collision_compat(const samp_gta_vector *point) {
  typedef void(__cdecl *gta_streaming_load_scene_collision_fn)(const samp_gta_vector *);
  gta_streaming_load_scene_collision_fn load_collision =
      (gta_streaming_load_scene_collision_fn)(uintptr_t)SAMP_ADDR_STREAMING_LOAD_SCENE_COLLISION;

  if (point == NULL) {
    return;
  }
  load_collision(point);
}

static void gta_streaming_load_scene_compat(const samp_gta_vector *point) {
  typedef void(__cdecl *gta_streaming_load_scene_fn)(const samp_gta_vector *);
  gta_streaming_load_scene_fn load_scene =
      (gta_streaming_load_scene_fn)(uintptr_t)SAMP_ADDR_STREAMING_LOAD_SCENE;

  if (point == NULL) {
    return;
  }
  load_scene(point);
}

static void gta_prepare_scene_at_compat(const char *reason, float x, float y, float z, int32_t skin_model) {
  samp_gta_vector point;

  point.x = x;
  point.y = y;
  point.z = z;

  if (skin_model >= 0) {
    gta_streaming_request_model_compat(skin_model, 0x06);
  }
  gta_streaming_load_scene_collision_compat(&point);
  gta_streaming_load_scene_compat(&point);
  if (skin_model >= 0) {
    gta_streaming_load_all_requested_compat(0);
  }
  (void)gta_script_command_compat(0x04E4u, "ff", x, y);

  runtime_tracef("scene_prepare: reason=%s pos=(%.3f,%.3f,%.3f) skin=%ld entry=%ld game_started=%u",
                 reason != NULL ? reason : "unknown", (double)x, (double)y, (double)z, (long)skin_model,
                 (long)read_game_entry_gate_value(), (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED));
}

static void mp_bridge_record_script_failure(const char *name, uint16_t opcode) {
  LONG failures = InterlockedIncrement(&g_runtime.mp_session_script_failures);
  if (failures <= 10) {
    runtime_tracef("mp_session_bridge: script command failed name=%s opcode=0x%04x failure=%ld",
                   (name != NULL) ? name : "unknown", (unsigned)opcode, (long)failures);
  }
}

static int gta_refresh_streaming_at_compat(const char *reason, float x, float y) {
  int ok = gta_script_command_compat(0x04E4u, "ff", x, y);

  if (!ok) {
    mp_bridge_record_script_failure("refresh_streaming_at", 0x04E4u);
    return 0;
  }
  runtime_tracef("mp_session_bridge: refresh_streaming_at reason=%s pos=(%.3f,%.3f)",
                 reason != NULL ? reason : "unknown", (double)x, (double)y);
  return 1;
}

static int gta_restart_if_wasted_at_compat(const char *reason, float x, float y, float z, float angle) {
  int ok = gta_script_command_compat(0x016Cu, "ffffi", x, y, z, angle, 0);

  if (!ok) {
    mp_bridge_record_script_failure("restart_if_wasted_at", 0x016Cu);
    return 0;
  }
  runtime_tracef("mp_session_bridge: restart_if_wasted_at reason=%s pos=(%.3f,%.3f,%.3f) angle=%.3f",
                 reason != NULL ? reason : "unknown", (double)x, (double)y, (double)z, (double)angle);
  return 1;
}

static int mp_session_apply_player_play_sound_compat(uint32_t sound_id, float x, float y, float z) {
  int ok = 0;

  if (!isfinite(x) || !isfinite(y) || !isfinite(z)) {
    return 0;
  }

  /* OLD_02X_REF + PROBE_TRACE:
   * ScrPlaySound reads sound,x,y,z and CGame::PlaySound applies opcode 0x018C "fffi".
   * TODO_VERIFY against original 0.3.7 binary trace for edge cases such as zero-vector sounds.
   */
  ok = gta_script_command_compat(0x018Cu, "fffi", x, y, z, (int)sound_id);
  if (!ok) {
    mp_bridge_record_script_failure("player_play_sound_rpc", 0x018Cu);
    return 0;
  }
  return 1;
}

static int gta_reset_ped_audio_attributes_compat(uintptr_t ped, const char *reason) {
#if defined(__i386__) || defined(_M_IX86)
  uintptr_t audio_entity = 0u;
  uintptr_t function = (uintptr_t)SAMP_ADDR_PED_AUDIO_INIT;

  if (ped < 0x10000u || ped >= 0x80000000u ||
      !memory_is_readable_compat((const void *)ped, SAMP_PED_OFFSET_AUDIO_ENTITY + sizeof(uintptr_t))) {
    return 0;
  }
  audio_entity = ped + SAMP_PED_OFFSET_AUDIO_ENTITY;
  if (!memory_is_readable_compat((const void *)audio_entity, sizeof(uintptr_t))) {
    return 0;
  }

  /*
   * OLD_02X_REF + TODO_VERIFY:
   * Legacy CPlayerPed::SetModelIndex reinitializes the ped audio attributes immediately after SetModelIndex:
   * ECX = ped + 660, push ped, call 0x4E68D0. This narrows the CJ-voice fix to the local ped instead of globally
   * muting vehicle or ped audio paths.
   */
  __asm__ __volatile__("pushl %[ped]\n\t"
                       "movl %[audio_entity], %%ecx\n\t"
                       "call *%[function]\n\t"
                       :
                       : [ped] "r"(ped), [audio_entity] "r"(audio_entity), [function] "r"(function)
                       : "eax", "ecx", "edx", "memory", "cc");
  runtime_tracef("ped_audio: reset reason=%s ped=0x%08lx audio=0x%08lx",
                 reason != NULL ? reason : "unknown", (unsigned long)ped, (unsigned long)audio_entity);
  return 1;
#else
  (void)ped;
  (void)reason;
  return 0;
#endif
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

  if (!memory_is_readable_compat((const void *)(entity + SAMP_PED_OFFSET_MATRIX), sizeof(matrix))) {
    return 0;
  }
  memcpy(&matrix, (const void *)(entity + SAMP_PED_OFFSET_MATRIX), sizeof(matrix));
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

static int gta_entity_read_position_compat(uintptr_t entity, float *x, float *y, float *z) {
  uintptr_t matrix = 0;

  if (x == NULL || y == NULL || z == NULL || entity < 0x10000u || entity >= 0x80000000u) {
    return 0;
  }

  if (!memory_is_readable_compat((const void *)(entity + SAMP_PED_OFFSET_MATRIX), sizeof(matrix))) {
    return 0;
  }
  memcpy(&matrix, (const void *)(entity + SAMP_PED_OFFSET_MATRIX), sizeof(matrix));
  if (matrix < 0x10000u || matrix >= 0x80000000u) {
    return 0;
  }

  memcpy(x, (const void *)(matrix + SAMP_MATRIX_OFFSET_POS), sizeof(*x));
  memcpy(y, (const void *)(matrix + SAMP_MATRIX_OFFSET_POS + sizeof(float)), sizeof(*y));
  memcpy(z, (const void *)(matrix + SAMP_MATRIX_OFFSET_POS + (sizeof(float) * 2u)), sizeof(*z));
  return 1;
}

static int gta_entity_read_move_speed_compat(uintptr_t entity, float out_speed[3]) {
  if (out_speed == NULL || entity < 0x10000u || entity >= 0x80000000u ||
      !memory_is_readable_compat((const void *)(entity + SAMP_ENTITY_OFFSET_MOVE_SPEED), sizeof(float) * 3u)) {
    return 0;
  }

  memcpy(out_speed, (const void *)(entity + SAMP_ENTITY_OFFSET_MOVE_SPEED), sizeof(float) * 3u);
  if (!isfinite(out_speed[0]) || !isfinite(out_speed[1]) || !isfinite(out_speed[2])) {
    out_speed[0] = 0.0f;
    out_speed[1] = 0.0f;
    out_speed[2] = 0.0f;
    return 0;
  }
  return 1;
}

static int valid_world_position_compat(float x, float y, float z) {
  return isfinite(x) && isfinite(y) && isfinite(z) && x > -20000.0f && x < 20000.0f && y > -20000.0f &&
         y < 20000.0f && z > -1000.0f && z < 200000.0f;
}

static uintptr_t gta_local_stream_entity_compat(uintptr_t ped) {
  uintptr_t vehicle = 0u;

  if (ped < 0x10000u || ped >= 0x80000000u) {
    return 0u;
  }
  if (gta_ped_is_in_vehicle_compat(ped) &&
      memory_is_readable_compat((const void *)(ped + SAMP_PED_OFFSET_VEHICLE), sizeof(vehicle))) {
    memcpy(&vehicle, (const void *)(ped + SAMP_PED_OFFSET_VEHICLE), sizeof(vehicle));
    if (vehicle >= 0x10000u && vehicle < 0x80000000u) {
      return vehicle;
    }
  }
  return ped;
}

static void refresh_local_streaming_from_entity_compat(uintptr_t ped) {
  uintptr_t entity = 0u;
  DWORD now = 0u;
  DWORD last = 0u;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float dx = 0.0f;
  float dy = 0.0f;
  float dz = 0.0f;
  float dist_sq = 0.0f;
  const float min_dist_sq = SAMP_LOCAL_STREAM_REFRESH_DISTANCE * SAMP_LOCAL_STREAM_REFRESH_DISTANCE;
  int ok = 0;

  entity = gta_local_stream_entity_compat(ped);
  if (entity == 0u || !gta_entity_read_position_compat(entity, &x, &y, &z) || !valid_world_position_compat(x, y, z)) {
    return;
  }

  now = GetTickCount();
  last = g_runtime.local_stream_refresh_last_tick;
  if (last != 0u && (DWORD)(now - last) < SAMP_LOCAL_STREAM_REFRESH_INTERVAL_MS) {
    return;
  }

  dx = x - g_runtime.local_stream_refresh_pos[0];
  dy = y - g_runtime.local_stream_refresh_pos[1];
  dz = z - g_runtime.local_stream_refresh_pos[2];
  dist_sq = (dx * dx) + (dy * dy) + (dz * dz);
  if (last != 0u && dist_sq < min_dist_sq) {
    return;
  }

  g_runtime.local_stream_refresh_last_tick = now;
  g_runtime.local_stream_refresh_pos[0] = x;
  g_runtime.local_stream_refresh_pos[1] = y;
  g_runtime.local_stream_refresh_pos[2] = z;

  /*
   * OLD_02X_REF + GTA_REVERSED_REF + INFERRED:
   * Legacy spawn/teleport paths call RefreshStreamingAt. While driving, no server SetPlayerPos RPC may arrive for
   * every local movement, so keep GTA's streamer warm from the local vehicle/ped position without forcing movement.
   * GTA's scene/collision loaders match the existing spawn/teleport prepare path and are only ticked on movement.
   */
  ok = gta_script_command_compat(0x04E4u, "ff", x, y);
  if (!ok) {
    mp_bridge_record_script_failure("local_refresh_streaming_at", 0x04E4u);
    return;
  }
  {
    samp_gta_vector point;
    point.x = x;
    point.y = y;
    point.z = z;
    gta_streaming_load_scene_collision_compat(&point);
    gta_streaming_load_scene_compat(&point);
  }
  if (InterlockedCompareExchange(&g_runtime.local_stream_refresh_logged, 1, 0) == 0) {
    runtime_tracef("local_stream_refresh: entity=0x%08lx pos=(%.3f,%.3f,%.3f) interval_ms=%lu distance=%.1f",
                   (unsigned long)entity, (double)x, (double)y, (double)z,
                   (unsigned long)SAMP_LOCAL_STREAM_REFRESH_INTERVAL_MS,
                   (double)SAMP_LOCAL_STREAM_REFRESH_DISTANCE);
  }
}

static float samp_absf_compat(float value) {
  return value < 0.0f ? -value : value;
}

static int gta_entity_teleport_compat(uintptr_t entity, float x, float y, float z) {
  int method_called = 0;
  int direct_ok = 0;
  float actual_x = 0.0f;
  float actual_y = 0.0f;
  float actual_z = 0.0f;
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
      method_called = 1;
    }
  }
#endif
  /*
   * OLD_02X_REF + PROBE_TRACE + TODO_VERIFY:
   * CPlayerPed::TeleportTo is the legacy path, but our wrapper cannot assume the vtable call actually committed
   * the final matrix. Keep the engine-side teleport and verify/fix the matrix directly so server SetPlayerPos
   * cannot silently leave the actor near the old/0,0,0 location.
   */
  direct_ok = gta_entity_direct_position_compat(entity, x, y, z);
  if (gta_entity_read_position_compat(entity, &actual_x, &actual_y, &actual_z)) {
    float dx = samp_absf_compat(actual_x - x);
    float dy = samp_absf_compat(actual_y - y);
    float dz = samp_absf_compat(actual_z - z);
    if (dx > 1.0f || dy > 1.0f || dz > 1.0f) {
      runtime_tracef("mp_session_bridge: teleport matrix mismatch method=%d direct=%d target=(%.3f,%.3f,%.3f) "
                     "actual=(%.3f,%.3f,%.3f) delta=(%.3f,%.3f,%.3f)",
                     method_called, direct_ok, (double)x, (double)y, (double)z, (double)actual_x,
                     (double)actual_y, (double)actual_z, (double)dx, (double)dy, (double)dz);
    }
  }
  return (method_called || direct_ok) ? 1 : 0;
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

static int gta_frontend_menu_active_compat(void) {
  return read_game_u8(SAMP_ADDR_MENU) != 0u || read_game_u8(SAMP_ADDR_MENU2) != 0u ||
         read_game_u8(SAMP_ADDR_MENU3) != 0u;
}

static int preconnect_frontend_connect_allowed_compat(void) {
  DWORD delay_ms = 0;
  DWORD now = 0;
  DWORD last = 0;
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
  if (gta_frontend_menu_active_compat()) {
    g_runtime.preconnect_delay_last_tick = 0u;
    if (InterlockedCompareExchange(&g_runtime.preconnect_pause_logged, 1, 0) == 0) {
      runtime_tracef("preconnect_bridge: connect delay paused by GTA frontend menu elapsed_ms=%lu/%lu "
                     "evidence=OLD_02X_REF,INFERRED,TODO_VERIFY",
                     (unsigned long)g_runtime.preconnect_delay_active_ms, (unsigned long)delay_ms);
    }
    return 0;
  }
  if (delay_ms == 0u) {
    return 1;
  }

  now = GetTickCount();
  last = g_runtime.preconnect_delay_last_tick;
  if (last == 0u) {
    g_runtime.preconnect_delay_last_tick = now;
    return 0;
  }

  elapsed = g_runtime.preconnect_delay_active_ms + (now - last);
  g_runtime.preconnect_delay_active_ms = elapsed;
  g_runtime.preconnect_delay_last_tick = now;
  if (elapsed < delay_ms) {
    return 0;
  }

  return 1;
}

static uintptr_t gta_entity_rw_object_compat(uintptr_t entity) {
  if (entity == 0u || !memory_is_readable_compat((const void *)(entity + SAMP_ENTITY_OFFSET_RW_OBJECT),
                                                 sizeof(uint32_t))) {
    return 0u;
  }
  return (uintptr_t)(*(volatile uint32_t *)(entity + SAMP_ENTITY_OFFSET_RW_OBJECT));
}

static int rp_anim_blend_clump_offset_ready_compat(uint32_t clump_offset) {
  return clump_offset != 0u && clump_offset != 0xFFFFFFFFu && clump_offset < 0x1000u;
}

static int preconnect_anim_runtime_ready_compat(uintptr_t *anim_assoc_out, uint32_t *clump_offset_out) {
  uintptr_t anim_assoc = (uintptr_t)(*(volatile uint32_t *)(uintptr_t)SAMP_ADDR_ANIM_ASSOC_GROUPS_PTR);
  uint32_t clump_offset = *(volatile uint32_t *)(uintptr_t)SAMP_ADDR_RPANIMBLEND_CLUMP_OFFSET;

  if (anim_assoc_out != NULL) {
    *anim_assoc_out = anim_assoc;
  }
  if (clump_offset_out != NULL) {
    *clump_offset_out = clump_offset;
  }

  return game_pointer_plausible_compat(anim_assoc) && rp_anim_blend_clump_offset_ready_compat(clump_offset);
}

static int preconnect_ped_anim_ready_compat(uintptr_t ped, uintptr_t *rw_object_out, uintptr_t *anim_assoc_out,
                                            uint32_t *clump_offset_out) {
  uintptr_t rw_object = gta_entity_rw_object_compat(ped);
  int anim_ready = preconnect_anim_runtime_ready_compat(anim_assoc_out, clump_offset_out);

  if (rw_object_out != NULL) {
    *rw_object_out = rw_object;
  }

  return anim_ready && game_pointer_plausible_compat(rw_object);
}

static void preconnect_request_game_load_once_compat(LONG apply_count, const char *reason) {
  LONG entry_before = read_game_entry_gate_value();
  uint8_t game_started_before = read_game_u8(SAMP_ADDR_GAME_STARTED);
  uint8_t startgame_before = read_game_u8(SAMP_ADDR_STARTGAME);

  if (InterlockedCompareExchange(&g_runtime.preconnect_game_load_kicked, 1, 0) == 0) {
    *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 8;
    write_game_u8(SAMP_ADDR_GAME_STARTED, 1u);
    write_game_u8(SAMP_ADDR_STARTGAME, 0u);
    write_game_u8(SAMP_ADDR_MENU, 0u);
    write_game_u8(SAMP_ADDR_MENU2, 0u);
    write_game_u8(SAMP_ADDR_MENU3, 0u);
    write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
    write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);
    runtime_tracef("preconnect_bridge: kicked game load reason=%s apply=%ld entry %ld->%ld game_started %u->%u "
                   "startgame %u->%u",
                   reason != NULL ? reason : "unknown", (long)apply_count, (long)entry_before,
                   (long)read_game_entry_gate_value(), (unsigned)game_started_before,
                   (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED), (unsigned)startgame_before,
                   (unsigned)read_game_u8(SAMP_ADDR_STARTGAME));
    return;
  }

  write_game_u8(SAMP_ADDR_STARTGAME, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);
  write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
  write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);
}

static void preconnect_hold_world_warmup_compat(LONG apply_count, const char *reason) {
  uintptr_t anim_assoc = 0u;
  uint32_t clump_offset = 0u;
  int anim_ready = preconnect_anim_runtime_ready_compat(&anim_assoc, &clump_offset);

  preconnect_request_game_load_once_compat(apply_count, reason);

  if (apply_count <= 3 || (apply_count % 120) == 0) {
    anim_ready = preconnect_anim_runtime_ready_compat(&anim_assoc, &clump_offset);
    runtime_tracef("preconnect_bridge: warming world reason=%s apply=%ld entry=%ld game_started=%u anim_ready=%d "
                   "anim_assoc=0x%08lx clump_offset=0x%08lx",
                   reason != NULL ? reason : "unknown", (long)apply_count, (long)read_game_entry_gate_value(),
                   (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED), anim_ready, (unsigned long)anim_assoc,
                   (unsigned long)clump_offset);
  }
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
  int anim_ready = 0;
  uintptr_t ped_rw_object = 0u;
  uintptr_t anim_assoc = 0u;
  uint32_t clump_offset = 0u;

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

  /*
   * OLD_02X_REF + INFERRED:
   * The legacy client lets GTA own its frontend once MENU is active and only skips SA-MP overlay drawing through
   * IsMenuActive(). In pre-connect we also freeze camera/streaming nudges and the delayed RakNet connect while
   * ESC/pause is open, so the user can sit in the GTA menu without the background connect timer expiring.
   */
  if (InterlockedCompareExchange(&g_runtime.preconnect_ready, 0, 0) != 0 && gta_frontend_menu_active_compat()) {
    g_runtime.preconnect_delay_last_tick = 0u;
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
    } else if (read_game_entry_gate_value() >= 7) {
      preconnect_hold_world_warmup_compat(apply_count, "no_local_ped");
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
      preconnect_hold_world_warmup_compat(apply_count, "settle");
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

  if (ped != 0u && !no_ped_fallback && InterlockedCompareExchange(&g_runtime.preconnect_ready, 0, 0) == 0) {
    anim_ready = preconnect_ped_anim_ready_compat(ped, &ped_rw_object, &anim_assoc, &clump_offset);
    if (!anim_ready) {
      preconnect_hold_world_warmup_compat(apply_count, "ped_anim_not_ready");
      if (InterlockedCompareExchange(&g_runtime.preconnect_anim_wait_logged, 1, 0) == 0) {
        runtime_tracef("preconnect_bridge: anim gate held ped=0x%08lx rw=0x%08lx anim_assoc=0x%08lx "
                       "clump_offset=0x%08lx",
                       (unsigned long)ped, (unsigned long)ped_rw_object, (unsigned long)anim_assoc,
                       (unsigned long)clump_offset);
      }
      if (ped_rw_object == 0u &&
          InterlockedCompareExchange(&g_runtime.preconnect_clump_wait_logged, 1, 0) == 0) {
        runtime_tracef("preconnect_bridge: waiting for local ped RenderWare object ped=0x%08lx", (unsigned long)ped);
      }
      return;
    }
  } else {
    anim_ready = preconnect_anim_runtime_ready_compat(&anim_assoc, &clump_offset);
    if (ped != 0u) {
      ped_rw_object = gta_entity_rw_object_compat(ped);
    }
  }

  /*
   * OBSERVED_037 + PROBE_TRACE:
   * The original R5 DLL only uses entry=8/game_started=1 as a transient GTA load kick. Once the local ped/pools
   * exist, pre-connect frontend settles at entry=9/game_started=0 before RakNet traffic starts.
   */
  *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 9;
  write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
  write_game_u8(SAMP_ADDR_STARTGAME, 0u);
  write_game_u8(SAMP_ADDR_MENU, 0u);
  write_game_u8(SAMP_ADDR_MENU2, 0u);
  write_game_u8(SAMP_ADDR_MENU3, 0u);
  write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
  write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);
  write_game_u8(SAMP_ADDR_WORLD_HOUR, SAMP_PRECONNECT_WORLD_HOUR);
  write_game_u8(SAMP_ADDR_WORLD_MINUTE, SAMP_PRECONNECT_WORLD_MINUTE);

  if (InterlockedCompareExchange(&g_runtime.preconnect_loaded_state_logged, 1, 0) == 0) {
    runtime_tracef("preconnect_bridge: entered loaded frontend state apply=%ld entry=%ld game_started=%u "
                   "startgame=%u anim_ready=%d ped=0x%08lx",
                   (long)apply_count, (long)read_game_entry_gate_value(),
                   (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED),
                   (unsigned)read_game_u8(SAMP_ADDR_STARTGAME), anim_ready, (unsigned long)ped);
  }

  if (InterlockedCompareExchange(&g_runtime.preconnect_scene_loaded, 1, 0) == 0) {
    gta_prepare_scene_at_compat("preconnect", SAMP_PRECONNECT_LOOK_X, SAMP_PRECONNECT_LOOK_Y, SAMP_PRECONNECT_LOOK_Z, -1);
    *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 9;
    write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
    write_game_u8(SAMP_ADDR_STARTGAME, 0u);
    write_game_u8(SAMP_ADDR_MENU, 0u);
    write_game_u8(SAMP_ADDR_MENU2, 0u);
    write_game_u8(SAMP_ADDR_MENU3, 0u);
    write_game_u8(SAMP_ADDR_ENABLE_HUD, 0u);
    write_game_u8(SAMP_ADDR_RADAR_BLANK, 1u);
    write_game_u8(SAMP_ADDR_WORLD_HOUR, SAMP_PRECONNECT_WORLD_HOUR);
    write_game_u8(SAMP_ADDR_WORLD_MINUTE, SAMP_PRECONNECT_WORLD_MINUTE);
  }

  if (ped != 0u && preconnect_move_ped_compat()) {
    (void)gta_preconnect_position_local_player_compat(ped);
  } else if (ped != 0u && InterlockedCompareExchange(&g_runtime.preconnect_ped_stationary_logged, 1, 0) == 0) {
    runtime_tracef("preconnect_bridge: keeping local ped stationary during camera frontend ped=0x%08lx",
                   (unsigned long)ped);
  }
  if ((apply_count <= 6 || (apply_count % 60) == 0) &&
      !gta_script_command_compat(0x04E4u, "ff", SAMP_PRECONNECT_LOOK_X, SAMP_PRECONNECT_LOOK_Y)) {
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
        "cam=(%.3f,%.3f,%.3f) look=(%.3f,%.3f,%.3f) anim_ready=%d rw=0x%08lx anim_assoc=0x%08lx "
        "clump_offset=0x%08lx delay_ms=%lu time=%u:%02u",
        (unsigned long)ped, no_ped_fallback, (long)read_game_entry_gate_value(),
        (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED), (double)SAMP_PRECONNECT_PLAYER_X,
        (double)SAMP_PRECONNECT_PLAYER_Y, (double)SAMP_PRECONNECT_PLAYER_Z, (double)SAMP_PRECONNECT_CAMERA_X,
        (double)SAMP_PRECONNECT_CAMERA_Y, (double)SAMP_PRECONNECT_CAMERA_Z, (double)SAMP_PRECONNECT_LOOK_X,
        (double)SAMP_PRECONNECT_LOOK_Y, (double)SAMP_PRECONNECT_LOOK_Z, anim_ready,
        (unsigned long)ped_rw_object, (unsigned long)anim_assoc, (unsigned long)clump_offset,
        (unsigned long)preconnect_delay_ms_compat(), (unsigned)read_game_u8(SAMP_ADDR_WORLD_HOUR),
        (unsigned)read_game_u8(SAMP_ADDR_WORLD_MINUTE));
  }

  if (InterlockedCompareExchange(&g_runtime.preconnect_started_logged, 1, 0) == 0) {
    chat_compat_add_message("SA-MP 0.3.7-R5 Started");
  }
  chat_compat_draw_overlay();
  if (apply_count <= 3 || (apply_count % 120) == 0) {
    runtime_tracef("preconnect_bridge: apply #%ld state=%ld ped=0x%08lx elapsed_ms=%lu camera_ok=%d anim_ready=%d",
                   (long)apply_count, (long)net_state, (unsigned long)ped,
                   (unsigned long)(now - g_runtime.preconnect_start_tick), camera_ok, anim_ready);
  }
}

static uint8_t byte_from_percent_float_compat(float value, uint8_t fallback) {
  if (!isfinite(value)) {
    return fallback;
  }
  if (value <= 0.0f) {
    return 0u;
  }
  if (value >= 255.0f) {
    return 255u;
  }
  return (uint8_t)(value + 0.5f);
}

static void send_onfoot_sync_compat(uintptr_t ped) {
  samp_raknet_onfoot_sync sync;
  DWORD now = 0u;
  DWORD last = 0u;
  float angle_rad = 0.0f;
  float health = 100.0f;
  int result = 0;
  LONG send_count = 0;

  if (ped == 0u || g_runtime.net_mgr.raknet_client == NULL ||
      InterlockedCompareExchange(&g_runtime.mp_session_spawn_finalized, 0, 0) == 0 ||
      gta_ped_is_in_vehicle_compat(ped)) {
    return;
  }

  now = GetTickCount();
  last = g_runtime.onfoot_sync_last_tick;
  if (last != 0u && (DWORD)(now - last) < SAMP_ONFOOT_SYNC_INTERVAL_MS) {
    return;
  }
  g_runtime.onfoot_sync_last_tick = now;

  memset(&sync, 0, sizeof(sync));
  if (!gta_entity_read_position_compat(ped, &sync.position[0], &sync.position[1], &sync.position[2])) {
    InterlockedIncrement(&g_runtime.onfoot_sync_failures);
    return;
  }
  if (!isfinite(sync.position[0]) || !isfinite(sync.position[1]) || !isfinite(sync.position[2]) ||
      sync.position[0] <= -20000.0f || sync.position[0] >= 20000.0f || sync.position[1] <= -20000.0f ||
      sync.position[1] >= 20000.0f || sync.position[2] <= -1000.0f || sync.position[2] >= 200000.0f) {
    InterlockedIncrement(&g_runtime.onfoot_sync_failures);
    return;
  }

  (void)gta_entity_read_move_speed_compat(ped, sync.move_speed);
  if (memory_is_readable_compat((const void *)(ped + SAMP_PED_OFFSET_ROTATION2), sizeof(angle_rad))) {
    memcpy(&angle_rad, (const void *)(ped + SAMP_PED_OFFSET_ROTATION2), sizeof(angle_rad));
  }
  if (!isfinite(angle_rad)) {
    angle_rad = g_runtime.raknet_spawn_rotation * SAMP_DEG_TO_RAD;
  }

  /*
   * OPENMP_REF + INFERRED:
   * SAMPFUNCS' 0.3.7 SDK documents the same 68-byte on-foot payload. For the first streaming-compatible pass we
   * derive yaw only from the local ped rotation and leave keys/weapon/animation neutral; server vehicle streaming
   * primarily needs a valid on-foot state plus position.
   */
  sync.quaternion[0] = cosf(angle_rad * -0.5f);
  sync.quaternion[1] = 0.0f;
  sync.quaternion[2] = 0.0f;
  sync.quaternion[3] = sinf(angle_rad * -0.5f);

  if (memory_is_readable_compat((const void *)(ped + SAMP_PED_OFFSET_HEALTH), sizeof(health))) {
    memcpy(&health, (const void *)(ped + SAMP_PED_OFFSET_HEALTH), sizeof(health));
  }
  sync.health = byte_from_percent_float_compat(health, 100u);
  sync.armour = 0u;
  sync.current_weapon = 0u;
  sync.special_action = 0u;
  sync.surfing_vehicle_id = 0u;

  result = samp_raknet_client_send_onfoot_sync(g_runtime.net_mgr.raknet_client, &sync);
  if (result == 0) {
    send_count = InterlockedIncrement(&g_runtime.onfoot_sync_send_count);
    if (InterlockedCompareExchange(&g_runtime.onfoot_sync_logged, 1, 0) == 0 || (send_count % 20) == 0) {
      runtime_tracef("onfoot_sync: send #%ld pos=(%.3f,%.3f,%.3f) speed=(%.3f,%.3f,%.3f) health=%u",
                     (long)send_count, (double)sync.position[0], (double)sync.position[1],
                     (double)sync.position[2], (double)sync.move_speed[0], (double)sync.move_speed[1],
                     (double)sync.move_speed[2], (unsigned)sync.health);
    }
  } else {
    LONG failures = InterlockedIncrement(&g_runtime.onfoot_sync_failures);
    if (failures <= 3 || (failures % 20) == 0) {
      runtime_tracef("onfoot_sync: send failed result=%d failures=%ld", result, (long)failures);
    }
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
  LONG player_pos_seq = 0;
  LONG applied_player_pos_seq = 0;
  LONG player_facing_seq = 0;
  LONG applied_player_facing_seq = 0;
  LONG player_health_seq = 0;
  LONG applied_player_health_seq = 0;
  LONG player_controllable_seq = 0;
  LONG applied_player_controllable_seq = 0;
  LONG camera_behind_seq = 0;
  LONG applied_camera_behind_seq = 0;
  LONG player_armour_seq = 0;
  LONG applied_player_armour_seq = 0;
  LONG player_armed_weapon_seq = 0;
  LONG applied_player_armed_weapon_seq = 0;
  LONG reset_player_weapons_seq = 0;
  LONG applied_reset_player_weapons_seq = 0;
  LONG reset_player_money_seq = 0;
  LONG applied_reset_player_money_seq = 0;
  LONG play_sound_seq = 0;
  LONG applied_play_sound_seq = 0;
  LONG stop_audio_stream_seq = 0;
  LONG applied_stop_audio_stream_seq = 0;
  LONG player_color_seq = 0;
  LONG applied_player_color_seq = 0;
  LONG player_team_seq = 0;
  LONG applied_player_team_seq = 0;
  LONG apply_animation_seq = 0;
  LONG applied_apply_animation_seq = 0;
  LONG world_visual_event_seq = 0;
  LONG observed_world_visual_seq = 0;
  LONG spawn_info_seq = 0;
  LONG finalized_spawn_seq = 0;
  int spawn_finalized = 0;

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
  spawn_info_seq = InterlockedCompareExchange(&g_runtime.raknet_spawn_info_seq, 0, 0);
  finalized_spawn_seq = InterlockedCompareExchange(&g_runtime.mp_session_finalized_spawn_seq, 0, 0);
  spawn_finalized = InterlockedCompareExchange(&g_runtime.mp_session_spawn_finalized, 0, 0) != 0;
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

  apply_network_time_weather_compat("mp_session_bridge");

  if (apply_count <= 6 || (apply_count % 60) == 0) {
    runtime_tracef(
        "mp_session_bridge: apply #%ld flags=0x%08lx ped=0x%08lx entry=%ld game_started=%u current_player=%u "
        "class_outcome=%ld spawn_outcome=%ld spawn_ready=%d spawn_info=%d spawn_seq=%ld finalized_seq=%ld "
        "pos=(%.3f,%.3f,%.3f) angle=%.3f "
        "cam=(%.3f,%.3f,%.3f) look=(%.3f,%.3f,%.3f)",
        (long)apply_count, (unsigned long)game_rpc_flags, (unsigned long)ped, (long)entry, (unsigned)game_started,
        (unsigned)current_player, (long)class_outcome, (long)spawn_outcome, spawn_ready, has_spawn_info,
        (long)spawn_info_seq, (long)finalized_spawn_seq, (double)target_pos[0], (double)target_pos[1],
        (double)target_z, (double)target_angle,
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

  /*
   * OLD_02X_REF + PROBE_TRACE:
   * ScrSetPlayerFacingAngle is an RPC event, while these bridge flags are sticky. After Spawn() consumes the initial
   * rotation, keep local camera/ped steering free and let the seq-gated live handler below consume later RPC updates.
   */
  if (ped != 0u && !spawn_finalized && (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_FACING) != 0u) {
    float angle_rad = target_angle * SAMP_DEG_TO_RAD;
    memcpy((void *)(ped + SAMP_PED_OFFSET_ROTATION1), &angle_rad, sizeof(angle_rad));
    memcpy((void *)(ped + SAMP_PED_OFFSET_ROTATION2), &angle_rad, sizeof(angle_rad));
    if (!gta_script_command_compat(0x0173u, "if", SAMP_GTA_ACTOR_LOCAL_ID, target_angle)) {
      mp_bridge_record_script_failure("set_actor_z_angle", 0x0173u);
    }
  }

  if (ped != 0u) {
    /*
     * OLD_02X_REF + PROBE_TRACE + TODO_VERIFY:
     * Apply one-shot script RPCs by sequence. This keeps sticky snapshot flags from permanently fighting local
     * control/camera after spawn while still honoring later server events.
     */
    player_health_seq = InterlockedCompareExchange(&g_runtime.raknet_player_health_seq, 0, 0);
    applied_player_health_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_player_health_seq, 0, 0);
    if (player_health_seq != 0 && player_health_seq != applied_player_health_seq) {
      float health = g_runtime.raknet_player_health;
      if (isfinite(health) && health >= 0.0f && health <= 10000.0f) {
        memcpy((void *)(ped + SAMP_PED_OFFSET_HEALTH), &health, sizeof(health));
        InterlockedExchange(&g_runtime.mp_session_applied_player_health_seq, player_health_seq);
        runtime_tracef("mp_session_bridge: apply_player_health seq=%ld previous=%ld health=%.3f",
                       (long)player_health_seq, (long)applied_player_health_seq, (double)health);
      } else {
        runtime_tracef("mp_session_bridge: skip_invalid_player_health seq=%ld health=%.3f",
                       (long)player_health_seq, (double)health);
      }
    }

    player_controllable_seq = InterlockedCompareExchange(&g_runtime.raknet_player_controllable_seq, 0, 0);
    applied_player_controllable_seq =
        InterlockedCompareExchange(&g_runtime.mp_session_applied_player_controllable_seq, 0, 0);
    if (player_controllable_seq != 0 && player_controllable_seq != applied_player_controllable_seq) {
      int controllable = g_runtime.raknet_player_controllable != 0u ? 1 : 0;
      if (!gta_script_command_compat(0x01B4u, "ii", SAMP_GTA_PLAYER_LOCAL_ID, controllable)) {
        mp_bridge_record_script_failure("toggle_player_controllable_rpc", 0x01B4u);
      }
      if (!gta_script_command_compat(0x04D7u, "ii", SAMP_GTA_ACTOR_LOCAL_ID, controllable ? 0 : 1)) {
        mp_bridge_record_script_failure("lock_actor_controllable_rpc", 0x04D7u);
      }
      if (controllable && !gta_ped_is_in_vehicle_compat(ped)) {
        float current_x = 0.0f;
        float current_y = 0.0f;
        float current_z = 0.0f;
        if (gta_entity_read_position_compat(ped, &current_x, &current_y, &current_z) &&
            valid_world_position_compat(current_x, current_y, current_z)) {
          (void)gta_entity_teleport_compat(ped, current_x, current_y, current_z);
        }
      }
      InterlockedExchange(&g_runtime.mp_session_applied_player_controllable_seq, player_controllable_seq);
      runtime_tracef("mp_session_bridge: apply_player_controllable seq=%ld previous=%ld controllable=%d",
                     (long)player_controllable_seq, (long)applied_player_controllable_seq, controllable);
    }

    camera_behind_seq = InterlockedCompareExchange(&g_runtime.raknet_camera_behind_seq, 0, 0);
    applied_camera_behind_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_camera_behind_seq, 0, 0);
    if (camera_behind_seq != 0 && camera_behind_seq != applied_camera_behind_seq) {
      if (!gta_script_command_compat(0x0373u, "")) {
        mp_bridge_record_script_failure("set_camera_behind_player_rpc", 0x0373u);
      }
      InterlockedExchange(&g_runtime.mp_session_applied_camera_behind_seq, camera_behind_seq);
      runtime_tracef("mp_session_bridge: apply_camera_behind seq=%ld previous=%ld", (long)camera_behind_seq,
                     (long)applied_camera_behind_seq);
    }

    /*
     * OLD_02X_REF + OPENMP_REF + TODO_VERIFY:
     * These server-triggered local-player state RPCs match the legacy server/client payload shape. Apply only
     * bounded values and keep non-rendering semantic state (team/color/animations) as sequenced observations until
     * the original 0.3.7-R5 path is traced.
     */
    player_armour_seq = InterlockedCompareExchange(&g_runtime.raknet_player_armour_seq, 0, 0);
    applied_player_armour_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_player_armour_seq, 0, 0);
    if (player_armour_seq != 0 && player_armour_seq != applied_player_armour_seq) {
      float armour = g_runtime.raknet_player_armour;
      if (isfinite(armour) && armour >= 0.0f && armour <= 10000.0f) {
        memcpy((void *)(ped + SAMP_PED_OFFSET_ARMOUR), &armour, sizeof(armour));
        InterlockedExchange(&g_runtime.mp_session_applied_player_armour_seq, player_armour_seq);
        runtime_tracef("mp_session_bridge: apply_player_armour seq=%ld previous=%ld armour=%.3f",
                       (long)player_armour_seq, (long)applied_player_armour_seq, (double)armour);
      } else {
        runtime_tracef("mp_session_bridge: skip_invalid_player_armour seq=%ld armour=%.3f",
                       (long)player_armour_seq, (double)armour);
      }
    }

    player_armed_weapon_seq = InterlockedCompareExchange(&g_runtime.raknet_player_armed_weapon_seq, 0, 0);
    applied_player_armed_weapon_seq =
        InterlockedCompareExchange(&g_runtime.mp_session_applied_player_armed_weapon_seq, 0, 0);
    if (player_armed_weapon_seq != 0 && player_armed_weapon_seq != applied_player_armed_weapon_seq) {
      uint32_t weapon = g_runtime.raknet_player_armed_weapon;
      if (weapon <= 255u) {
        if (!gta_script_command_compat(0x01B9u, "ii", SAMP_GTA_ACTOR_LOCAL_ID, (int)weapon)) {
          mp_bridge_record_script_failure("set_armed_weapon_rpc", 0x01B9u);
        }
        InterlockedExchange(&g_runtime.mp_session_applied_player_armed_weapon_seq, player_armed_weapon_seq);
        runtime_tracef("mp_session_bridge: apply_armed_weapon seq=%ld previous=%ld weapon=%lu",
                       (long)player_armed_weapon_seq, (long)applied_player_armed_weapon_seq,
                       (unsigned long)weapon);
      }
    }

    reset_player_weapons_seq = InterlockedCompareExchange(&g_runtime.raknet_reset_player_weapons_seq, 0, 0);
    applied_reset_player_weapons_seq =
        InterlockedCompareExchange(&g_runtime.mp_session_applied_reset_player_weapons_seq, 0, 0);
    if (reset_player_weapons_seq != 0 && reset_player_weapons_seq != applied_reset_player_weapons_seq) {
      if (!gta_script_command_compat(0x03B8u, "i", SAMP_GTA_PLAYER_LOCAL_ID)) {
        mp_bridge_record_script_failure("reset_player_weapons_rpc", 0x03B8u);
      }
      InterlockedExchange(&g_runtime.mp_session_applied_reset_player_weapons_seq, reset_player_weapons_seq);
      runtime_tracef("mp_session_bridge: apply_reset_weapons seq=%ld previous=%ld",
                     (long)reset_player_weapons_seq, (long)applied_reset_player_weapons_seq);
    }

    reset_player_money_seq = InterlockedCompareExchange(&g_runtime.raknet_reset_player_money_seq, 0, 0);
    applied_reset_player_money_seq =
        InterlockedCompareExchange(&g_runtime.mp_session_applied_reset_player_money_seq, 0, 0);
    if (reset_player_money_seq != 0 && reset_player_money_seq != applied_reset_player_money_seq) {
      int32_t money = 0;
      if (memory_is_readable_compat((const void *)(uintptr_t)SAMP_ADDR_MONEY, sizeof(money))) {
        memcpy(&money, (const void *)(uintptr_t)SAMP_ADDR_MONEY, sizeof(money));
      }
      if (money != 0) {
        if (!gta_script_command_compat(0x0109u, "ii", SAMP_GTA_PLAYER_LOCAL_ID, -money)) {
          mp_bridge_record_script_failure("reset_player_money_rpc", 0x0109u);
        }
      }
      InterlockedExchange(&g_runtime.mp_session_applied_reset_player_money_seq, reset_player_money_seq);
      runtime_tracef("mp_session_bridge: apply_reset_money seq=%ld previous=%ld previous_money=%ld",
                     (long)reset_player_money_seq, (long)applied_reset_player_money_seq, (long)money);
    }

    play_sound_seq = InterlockedCompareExchange(&g_runtime.raknet_play_sound_seq, 0, 0);
    applied_play_sound_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_play_sound_seq, 0, 0);
    if (play_sound_seq != 0 && play_sound_seq != applied_play_sound_seq) {
      float x = g_runtime.raknet_play_sound_pos[0];
      float y = g_runtime.raknet_play_sound_pos[1];
      float z = g_runtime.raknet_play_sound_pos[2];
      if (mp_session_apply_player_play_sound_compat(g_runtime.raknet_play_sound_id, x, y, z)) {
        InterlockedExchange(&g_runtime.mp_session_applied_play_sound_seq, play_sound_seq);
        runtime_tracef("mp_session_bridge: apply_player_play_sound seq=%ld previous=%ld sound=%lu "
                       "pos=(%.3f,%.3f,%.3f)",
                       (long)play_sound_seq, (long)applied_play_sound_seq,
                       (unsigned long)g_runtime.raknet_play_sound_id, (double)x, (double)y, (double)z);
      }
    }

    stop_audio_stream_seq = InterlockedCompareExchange(&g_runtime.raknet_stop_audio_stream_seq, 0, 0);
    applied_stop_audio_stream_seq =
        InterlockedCompareExchange(&g_runtime.mp_session_applied_stop_audio_stream_seq, 0, 0);
    if (stop_audio_stream_seq != 0 && stop_audio_stream_seq != applied_stop_audio_stream_seq) {
      InterlockedExchange(&g_runtime.mp_session_applied_stop_audio_stream_seq, stop_audio_stream_seq);
      runtime_tracef("mp_session_bridge: observe_stop_audio_stream seq=%ld previous=%ld backend=not_wired TODO_VERIFY=1",
                     (long)stop_audio_stream_seq, (long)applied_stop_audio_stream_seq);
    }

    player_color_seq = InterlockedCompareExchange(&g_runtime.raknet_player_color_seq, 0, 0);
    applied_player_color_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_player_color_seq, 0, 0);
    if (player_color_seq != 0 && player_color_seq != applied_player_color_seq) {
      InterlockedExchange(&g_runtime.mp_session_applied_player_color_seq, player_color_seq);
      runtime_tracef("mp_session_bridge: observe_player_color seq=%ld previous=%ld player=%u color=0x%08lx TODO_VERIFY=1",
                     (long)player_color_seq, (long)applied_player_color_seq,
                     (unsigned)g_runtime.raknet_player_color_player_id,
                     (unsigned long)g_runtime.raknet_player_color);
    }

    player_team_seq = InterlockedCompareExchange(&g_runtime.raknet_player_team_seq, 0, 0);
    applied_player_team_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_player_team_seq, 0, 0);
    if (player_team_seq != 0 && player_team_seq != applied_player_team_seq) {
      InterlockedExchange(&g_runtime.mp_session_applied_player_team_seq, player_team_seq);
      runtime_tracef("mp_session_bridge: observe_player_team seq=%ld previous=%ld player=%u team=%u TODO_VERIFY=1",
                     (long)player_team_seq, (long)applied_player_team_seq,
                     (unsigned)g_runtime.raknet_player_team_player_id, (unsigned)g_runtime.raknet_player_team);
    }

    apply_animation_seq = InterlockedCompareExchange(&g_runtime.raknet_apply_animation_seq, 0, 0);
    applied_apply_animation_seq =
        InterlockedCompareExchange(&g_runtime.mp_session_applied_apply_animation_seq, 0, 0);
    if (apply_animation_seq != 0 && apply_animation_seq != applied_apply_animation_seq) {
      InterlockedExchange(&g_runtime.mp_session_applied_apply_animation_seq, apply_animation_seq);
      runtime_tracef(
          "mp_session_bridge: observe_apply_animation seq=%ld previous=%ld player=%u lib='%s' name='%s' delta=%.3f "
          "flags=%u/%u/%u/%u time=%ld apply_deferred=1 TODO_VERIFY=1",
          (long)apply_animation_seq, (long)applied_apply_animation_seq,
          (unsigned)g_runtime.raknet_apply_animation_player_id, g_runtime.raknet_apply_animation_lib,
          g_runtime.raknet_apply_animation_name, (double)g_runtime.raknet_apply_animation_delta,
          (unsigned)g_runtime.raknet_apply_animation_loop, (unsigned)g_runtime.raknet_apply_animation_lock_x,
          (unsigned)g_runtime.raknet_apply_animation_lock_y, (unsigned)g_runtime.raknet_apply_animation_freeze,
          (long)g_runtime.raknet_apply_animation_time);
    }
  }

  world_visual_event_seq = InterlockedCompareExchange(&g_runtime.raknet_world_visual_event_seq, 0, 0);
  observed_world_visual_seq = InterlockedCompareExchange(&g_runtime.mp_session_observed_world_visual_seq, 0, 0);
  if (world_visual_event_seq != 0 && world_visual_event_seq != observed_world_visual_seq) {
    InterlockedExchange(&g_runtime.mp_session_observed_world_visual_seq, world_visual_event_seq);
    runtime_tracef("mp_session_bridge: observe_world_visual seq=%ld previous=%ld type=%u id=%u model=%ld color=0x%08lx text='%.96s' TODO_VERIFY=1",
                   (long)world_visual_event_seq, (long)observed_world_visual_seq,
                   (unsigned)g_runtime.raknet_world_visual_event_type, (unsigned)g_runtime.raknet_world_visual_id,
                   (long)g_runtime.raknet_world_visual_model,
                   (unsigned long)g_runtime.raknet_world_visual_color, g_runtime.raknet_world_visual_text);
  }

  if (ped != 0u && class_outcome == 1 && !spawn_ready && (rpc_flags & SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_SENT) == 0u) {
    float health = 100.0f;
    memcpy((void *)(ped + SAMP_PED_OFFSET_HEALTH), &health, sizeof(health));
    if (!gta_script_command_compat(0x01B4u, "ii", SAMP_GTA_PLAYER_LOCAL_ID, 0)) {
      mp_bridge_record_script_failure("toggle_player_controllable", 0x01B4u);
    }
    if (!gta_script_command_compat(0x04D7u, "ii", SAMP_GTA_ACTOR_LOCAL_ID, 1)) {
      mp_bridge_record_script_failure("lock_actor_class_select", 0x04D7u);
    }
  }

  if (ped != 0u && spawn_ready && has_spawn_info && spawn_info_seq != 0 && spawn_info_seq != finalized_spawn_seq) {
    const uint8_t no_fade_patch[3] = {0xC2u, 0x08u, 0x00u};
    float health = 100.0f;
    InterlockedExchange(&g_runtime.mp_session_spawn_finalized, 1);
    InterlockedExchange(&g_runtime.mp_session_finalized_spawn_seq, spawn_info_seq);

    /*
     * OBSERVED_037 + PROBE_TRACE:
     * Original 0.3.7-R5 remains at entry=9/game_started=0 through dialog/freeroam selection and spawn. The
     * entry=8/game_started=1 phase is only the earlier GTA-load kick.
     */
    *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 9;
    write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
    write_game_u8(SAMP_ADDR_STARTGAME, 0u);
    write_game_u8(SAMP_ADDR_MENU, 0u);
    write_game_u8(SAMP_ADDR_MENU2, 0u);
    write_game_u8(SAMP_ADDR_MENU3, 0u);
    write_game_u8(SAMP_ADDR_ENABLE_HUD, 1u);
    write_game_u8(SAMP_ADDR_RADAR_BLANK, 0u);
    memcpy((void *)(ped + SAMP_PED_OFFSET_HEALTH), &health, sizeof(health));
    textdraw_compat_clear_select_mode("spawn_finalize");

    if (has_spawn_info) {
      (void)gta_entity_teleport_compat(ped, target_pos[0], target_pos[1], target_z);
      if (InterlockedCompareExchange(&g_runtime.mp_session_scene_loaded, 1, 0) == 0) {
        gta_prepare_scene_at_compat("spawn", target_pos[0], target_pos[1], target_z,
                                    g_runtime.raknet_spawn_skin >= 0 ? g_runtime.raknet_spawn_skin : -1);
        *(volatile LONG *)(uintptr_t)SAMP_ADDR_ENTRY = 9;
        write_game_u8(SAMP_ADDR_GAME_STARTED, 0u);
        write_game_u8(SAMP_ADDR_STARTGAME, 0u);
        write_game_u8(SAMP_ADDR_MENU, 0u);
        write_game_u8(SAMP_ADDR_MENU2, 0u);
        write_game_u8(SAMP_ADDR_MENU3, 0u);
        write_game_u8(SAMP_ADDR_ENABLE_HUD, 1u);
        write_game_u8(SAMP_ADDR_RADAR_BLANK, 0u);
      }
    }

    if (has_spawn_info) {
      /*
       * OLD_02X_REF:
       * CLocalPlayer::Spawn() performs RefreshStreamingAt(spawn.x, spawn.y) and
       * CPlayerPed::RestartIfWastedAt(spawn.pos, spawn.rot) before model/weapon setup and final TeleportTo(z + 1).
       */
      (void)gta_refresh_streaming_at_compat("spawn", target_pos[0], target_pos[1]);
      (void)gta_restart_if_wasted_at_compat("spawn", target_pos[0], target_pos[1], target_pos[2], target_angle);
    }

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
    if (!gta_script_command_compat(0x04D7u, "ii", SAMP_GTA_ACTOR_LOCAL_ID, 0)) {
      mp_bridge_record_script_failure("unlock_actor_spawn", 0x04D7u);
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
      } else {
        (void)gta_reset_ped_audio_attributes_compat(ped, "spawn_skin");
      }
    }
    if (has_spawn_info) {
      (void)gta_entity_teleport_compat(ped, target_pos[0], target_pos[1], target_z);
      (void)gta_reset_ped_audio_attributes_compat(ped, "spawn_finalize");
      if (!gta_script_command_compat(0x0373u, "")) {
        mp_bridge_record_script_failure("set_camera_behind_player_after_spawn_teleport", 0x0373u);
      }
    }

    {
      int spawn_notify_result = -1;

      if (g_runtime.net_mgr.raknet_client != NULL) {
        spawn_notify_result = samp_raknet_client_send_spawn_notification_for_seq(g_runtime.net_mgr.raknet_client,
                                                                                 (uint32_t)spawn_info_seq);
      }
      runtime_tracef("mp_session_bridge: spawn_notify_after_finalize seq=%ld result=%d", (long)spawn_info_seq,
                     spawn_notify_result);
    }

    /*
     * PROBE_TRACE + OPENMP_REF + INFERRED:
     * SuperFreeroam's spawn and /teles paths use SetPlayerPos/SetPlayerFacingAngle. The RPC flags stay sticky, so
     * mark all pre-finalize values as consumed and only apply later seq bumps as live server movement.
     */
    InterlockedExchange(&g_runtime.mp_session_applied_player_pos_seq,
                        InterlockedCompareExchange(&g_runtime.raknet_player_pos_seq, 0, 0));
    InterlockedExchange(&g_runtime.mp_session_applied_player_facing_seq,
                        InterlockedCompareExchange(&g_runtime.raknet_player_facing_seq, 0, 0));
    InterlockedExchange(&g_runtime.mp_session_teleport_count, 0);
    g_runtime.local_stream_refresh_last_tick = 0u;

    runtime_tracef("mp_session_bridge: spawn_finalize seq=%ld previous=%ld outcome=%ld info=%d team=%u skin=%ld "
                   "pos=(%.3f,%.3f,%.3f) rot=%.3f entry=%ld game_started=%u",
                   (long)spawn_info_seq, (long)finalized_spawn_seq, (long)spawn_outcome, has_spawn_info,
                   (unsigned)g_runtime.raknet_spawn_team,
                   (long)g_runtime.raknet_spawn_skin, (double)target_pos[0], (double)target_pos[1],
                   (double)target_z, (double)target_angle, (long)read_game_entry_gate_value(),
                   (unsigned)read_game_u8(SAMP_ADDR_GAME_STARTED));
  }

  if (ped != 0u && spawn_ready && InterlockedCompareExchange(&g_runtime.mp_session_spawn_finalized, 0, 0) != 0) {
    player_pos_seq = InterlockedCompareExchange(&g_runtime.raknet_player_pos_seq, 0, 0);
    applied_player_pos_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_player_pos_seq, 0, 0);
    if (player_pos_seq != 0 && player_pos_seq != applied_player_pos_seq) {
      float server_pos[3];
      int teleported = 0;

      memcpy(server_pos, g_runtime.raknet_player_pos, sizeof(server_pos));
      gta_prepare_scene_at_compat("server_player_pos", server_pos[0], server_pos[1], server_pos[2],
                                  g_runtime.raknet_spawn_skin >= 0 ? g_runtime.raknet_spawn_skin : -1);
      teleported = gta_entity_teleport_compat(ped, server_pos[0], server_pos[1], server_pos[2]);
      if (!gta_script_command_compat(0x0373u, "")) {
        mp_bridge_record_script_failure("set_camera_behind_player_after_pos", 0x0373u);
      }
      InterlockedExchange(&g_runtime.mp_session_applied_player_pos_seq, player_pos_seq);
      InterlockedIncrement(&g_runtime.mp_session_teleport_count);
      if (teleported) {
        /*
         * PROBE_TRACE + GTA_REVERSED_REF + INFERRED:
         * Grove Street /teles streamed a large vehicle burst immediately after SetPlayerPos and crashed in
         * GTA's CPhysical::Add (0x00544bc8) during create_car. Hold and pace vehicle creation while the
         * scene/streaming state settles; TODO_VERIFY against original 0.3.7 golden trace.
         */
        g_runtime.vehicle_create_hold_until_tick = GetTickCount() + SAMP_VEHICLE_COMPAT_POST_TELEPORT_HOLD_MS;
        g_runtime.vehicle_create_last_tick = 0u;
        g_runtime.local_stream_refresh_last_tick = 0u;
        runtime_tracef("vehicle: hold_after_teleport ms=%lu pos=(%.3f,%.3f,%.3f)",
                       (unsigned long)SAMP_VEHICLE_COMPAT_POST_TELEPORT_HOLD_MS, (double)server_pos[0],
                       (double)server_pos[1], (double)server_pos[2]);
      }
      runtime_tracef(
          "mp_session_bridge: apply_player_pos seq=%ld previous=%ld teleported=%d pos=(%.3f,%.3f,%.3f) entry=%ld",
          (long)player_pos_seq, (long)applied_player_pos_seq, teleported, (double)server_pos[0],
          (double)server_pos[1], (double)server_pos[2], (long)read_game_entry_gate_value());
    }

    player_facing_seq = InterlockedCompareExchange(&g_runtime.raknet_player_facing_seq, 0, 0);
    applied_player_facing_seq = InterlockedCompareExchange(&g_runtime.mp_session_applied_player_facing_seq, 0, 0);
    if (player_facing_seq != 0 && player_facing_seq != applied_player_facing_seq) {
      float angle = g_runtime.raknet_player_facing_angle;
      float angle_rad = angle * SAMP_DEG_TO_RAD;

      memcpy((void *)(ped + SAMP_PED_OFFSET_ROTATION1), &angle_rad, sizeof(angle_rad));
      memcpy((void *)(ped + SAMP_PED_OFFSET_ROTATION2), &angle_rad, sizeof(angle_rad));
      if (!gta_script_command_compat(0x0173u, "if", SAMP_GTA_ACTOR_LOCAL_ID, angle)) {
        mp_bridge_record_script_failure("set_actor_z_angle_after_pos", 0x0173u);
      }
      InterlockedExchange(&g_runtime.mp_session_applied_player_facing_seq, player_facing_seq);
      runtime_tracef("mp_session_bridge: apply_player_facing seq=%ld previous=%ld angle=%.3f",
                     (long)player_facing_seq, (long)applied_player_facing_seq, (double)angle);
    }
  }

  if (ped != 0u && spawn_ready && InterlockedCompareExchange(&g_runtime.mp_session_spawn_finalized, 0, 0) != 0) {
    refresh_local_streaming_from_entity_compat(ped);
    send_onfoot_sync_compat(ped);
    vehicle_compat_process_markers(ped);
  }

  if (ped != 0u && !spawn_ready && (game_rpc_flags & SAMP_RAKNET_RPC_FLAG_PLAYER_POS) != 0u &&
      InterlockedCompareExchange(&g_runtime.mp_session_teleport_count, 0, 0) == 0) {
    InterlockedExchange(&g_runtime.mp_session_teleport_count, -1);
    runtime_tracef("mp_session_bridge: suppressing pre-spawn player-pos teleport ped=0x%08lx pos=(%.3f,%.3f,%.3f)",
                   (unsigned long)ped, (double)target_pos[0], (double)target_pos[1], (double)target_z);
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

  apply_resolution_unlock_patches_compat();
  install_select_device_hook_compat();
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
  apply_startgame_flags_compat();
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
    InterlockedExchange(&g_runtime.dialog_overlay_active, 0);
    InterlockedExchange(&g_runtime.dialog_overlay_id, 0);
    InterlockedExchange(&g_runtime.dialog_overlay_style, 0);
    InterlockedExchange(&g_runtime.dialog_overlay_selected, -1);
    InterlockedExchange(&g_runtime.dialog_overlay_scroll, 0);
    InterlockedExchange(&g_runtime.dialog_overlay_input_len, 0);
    InterlockedExchange(&g_runtime.dialog_overlay_logged, 0);
    g_runtime.dialog_overlay_title[0] = '\0';
    g_runtime.dialog_overlay_info[0] = '\0';
    g_runtime.dialog_overlay_button1[0] = '\0';
    g_runtime.dialog_overlay_button2[0] = '\0';
    g_runtime.dialog_overlay_input[0] = '\0';
    InterlockedExchange(&g_runtime.textdraw_event_seq, 0);
    InterlockedExchange(&g_runtime.textdraw_active_count, 0);
    InterlockedExchange(&g_runtime.textdraw_logged, 0);
    InterlockedExchange(&g_runtime.textdraw_select_active, 0);
    InterlockedExchange(&g_runtime.textdraw_mouse_down, 0);
    g_runtime.textdraw_select_color = 0u;
    memset(g_runtime.textdraw_slots, 0, sizeof(g_runtime.textdraw_slots));
    InterlockedExchange(&g_runtime.scoreboard_offset, 0);
    InterlockedExchange(&g_runtime.scoreboard_logged, 0);
    InterlockedExchange(&g_runtime.scoreboard_player_pool_event_seq, 0);
    InterlockedExchange(&g_runtime.scoreboard_score_ping_seq, 0);
    InterlockedExchange(&g_runtime.scoreboard_player_count, 0);
    InterlockedExchange(&g_runtime.scoreboard_local_player_id_valid, 0);
    InterlockedExchange(&g_runtime.scoreboard_local_player_id, 0);
    memset(g_runtime.scoreboard_players, 0, sizeof(g_runtime.scoreboard_players));
    object_compat_reset_pool("connect_reset");
    vehicle_compat_reset_pool("connect_reset");
    InterlockedExchange(&g_runtime.onfoot_sync_send_count, 0);
    InterlockedExchange(&g_runtime.onfoot_sync_failures, 0);
    InterlockedExchange(&g_runtime.onfoot_sync_logged, 0);
    g_runtime.onfoot_sync_last_tick = 0u;
    InterlockedExchange(&g_runtime.online_session_drift_seen, 0);
    InterlockedExchange(&g_runtime.online_session_last_entry, 0);
    InterlockedExchange(&g_runtime.online_session_last_game_started, 0);
    InterlockedExchange(&g_runtime.mp_session_apply_count, 0);
    InterlockedExchange(&g_runtime.mp_session_teleport_count, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_pos_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_facing_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_health_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_controllable_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_camera_behind_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_armour_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_armed_weapon_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_reset_player_weapons_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_reset_player_money_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_play_sound_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_stop_audio_stream_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_color_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_player_team_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_applied_apply_animation_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_observed_world_visual_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_script_failures, 0);
    InterlockedExchange(&g_runtime.mp_session_frontend_hold_logged, 0);
    InterlockedExchange(&g_runtime.mp_session_spawn_finalized, 0);
    InterlockedExchange(&g_runtime.mp_session_finalized_spawn_seq, 0);
    InterlockedExchange(&g_runtime.mp_session_scene_loaded, 0);
    InterlockedExchange(&g_runtime.raknet_time_apply_logged, 0);
    InterlockedExchange(&g_runtime.local_stream_refresh_logged, 0);
    g_runtime.raknet_weather = 0u;
    g_runtime.raknet_world_hour = 12u;
    g_runtime.raknet_world_minute = 0u;
    g_runtime.raknet_world_time_valid = 0u;
    g_runtime.raknet_clock_enabled = 0u;
    g_runtime.raknet_hold_time = 1u;
    g_runtime.raknet_interior = 0u;
    g_runtime.raknet_camera_look_at_type = 0u;
    g_runtime.raknet_player_controllable = 1u;
    g_runtime.raknet_player_team = 0u;
    g_runtime.raknet_apply_animation_loop = 0u;
    g_runtime.raknet_apply_animation_lock_x = 0u;
    g_runtime.raknet_apply_animation_lock_y = 0u;
    g_runtime.raknet_apply_animation_freeze = 0u;
    g_runtime.raknet_world_visual_event_type = 0u;
    g_runtime.raknet_init_world_time = 0u;
    g_runtime.raknet_init_weather = 0u;
    g_runtime.raknet_init_lan_mode = 0u;
    g_runtime.raknet_init_disable_enter_exits = 0u;
    g_runtime.raknet_init_stunt_bonus = 0u;
    g_runtime.raknet_init_spawns_available = 0u;
    g_runtime.raknet_init_local_player_id = 0u;
    g_runtime.raknet_player_color_player_id = 0u;
    g_runtime.raknet_player_team_player_id = 0u;
    g_runtime.raknet_apply_animation_player_id = 0u;
    g_runtime.raknet_world_visual_id = 0u;
    g_runtime.raknet_init_death_drop_money = 0;
    g_runtime.raknet_apply_animation_time = 0;
    g_runtime.raknet_world_visual_model = 0;
    g_runtime.raknet_player_facing_angle = 0.0f;
    g_runtime.raknet_player_health = 100.0f;
    g_runtime.raknet_player_armour = 0.0f;
    g_runtime.raknet_apply_animation_delta = 0.0f;
    g_runtime.local_stream_refresh_last_tick = 0u;
    InterlockedExchange(&g_runtime.raknet_logon_marked, 0);
    InterlockedExchange(&g_runtime.raknet_player_pos_seq, 0);
    InterlockedExchange(&g_runtime.raknet_player_facing_seq, 0);
    InterlockedExchange(&g_runtime.raknet_player_health_seq, 0);
    InterlockedExchange(&g_runtime.raknet_player_controllable_seq, 0);
    InterlockedExchange(&g_runtime.raknet_camera_behind_seq, 0);
    InterlockedExchange(&g_runtime.raknet_player_armour_seq, 0);
    InterlockedExchange(&g_runtime.raknet_player_armed_weapon_seq, 0);
    InterlockedExchange(&g_runtime.raknet_reset_player_weapons_seq, 0);
    InterlockedExchange(&g_runtime.raknet_reset_player_money_seq, 0);
    InterlockedExchange(&g_runtime.raknet_play_sound_seq, 0);
    InterlockedExchange(&g_runtime.raknet_stop_audio_stream_seq, 0);
    InterlockedExchange(&g_runtime.raknet_player_color_seq, 0);
    InterlockedExchange(&g_runtime.raknet_player_team_seq, 0);
    InterlockedExchange(&g_runtime.raknet_apply_animation_seq, 0);
    InterlockedExchange(&g_runtime.raknet_world_visual_event_seq, 0);
    InterlockedExchange(&g_runtime.raknet_spawn_info_seq, 0);
    g_runtime.raknet_player_armed_weapon = 0u;
    g_runtime.raknet_play_sound_id = 0u;
    g_runtime.raknet_player_color = 0u;
    g_runtime.raknet_world_visual_color = 0u;
    g_runtime.raknet_init_gravity = 0.0f;
    g_runtime.raknet_init_name_tag_draw_distance = 0.0f;
    g_runtime.raknet_init_global_chat_radius = 0.0f;
    memset(g_runtime.raknet_player_pos, 0, sizeof(g_runtime.raknet_player_pos));
    memset(g_runtime.local_stream_refresh_pos, 0, sizeof(g_runtime.local_stream_refresh_pos));
    memset(g_runtime.raknet_camera_pos, 0, sizeof(g_runtime.raknet_camera_pos));
    memset(g_runtime.raknet_camera_look_at, 0, sizeof(g_runtime.raknet_camera_look_at));
    memset(g_runtime.raknet_play_sound_pos, 0, sizeof(g_runtime.raknet_play_sound_pos));
    memset(g_runtime.raknet_world_visual_pos, 0, sizeof(g_runtime.raknet_world_visual_pos));
    memset(g_runtime.raknet_apply_animation_lib, 0, sizeof(g_runtime.raknet_apply_animation_lib));
    memset(g_runtime.raknet_apply_animation_name, 0, sizeof(g_runtime.raknet_apply_animation_name));
    memset(g_runtime.raknet_world_visual_text, 0, sizeof(g_runtime.raknet_world_visual_text));
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
    int disconnect_packet = 0;
    uint32_t rpc_flags = 0u;
    LONG state_before = InterlockedCompareExchange(&g_runtime.netgame_state, 0, 0);

    connected_after_pump = samp_net_mgr_raknet_is_connected(&g_runtime.net_mgr);
    drained = samp_raknet_client_drain_packets_autojoin(g_runtime.net_mgr.raknet_client, SAMP_RAKNET_PUMP_BUDGET,
                                                         &g_runtime.raknet_join_profile, &connected_after_pump, &join_sent,
                                                         &last_packet_id);
    rpc_flags = refresh_raknet_rpc_snapshot_compat();
    if (drained >= 0) {
      InterlockedIncrement(&g_runtime.raknet_pump_calls);
      if (drained >= SAMP_RAKNET_PUMP_BUDGET) {
        LONG saturated = InterlockedIncrement(&g_runtime.raknet_pump_saturated_count);
        if (saturated <= 8 || (saturated % 60) == 0) {
          runtime_tracef("network_prepare: pump saturated drained=%d budget=%d count=%ld state_before=%ld connected=%d",
                         drained, SAMP_RAKNET_PUMP_BUDGET, (long)saturated, (long)state_before,
                         connected_after_pump);
        }
      }
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
        case 29: /* ID_CONNECTION_ATTEMPT_FAILED */
        case 31: /* ID_NO_FREE_INCOMING_CONNECTIONS */
          InterlockedExchange(&g_runtime.net_mgr_connected, 0);
          InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_WAIT_CONNECT);
          runtime_tracef("network_prepare: RakNet connect not ready -> state=WAIT_CONNECT");
          break;
        case 32: /* ID_DISCONNECTION_NOTIFICATION */
        case 33: /* ID_CONNECTION_LOST */
          disconnect_packet = 1;
          InterlockedExchange(&g_runtime.net_mgr_connected, 0);
          InterlockedExchange(&g_runtime.raknet_join_sent, 0);
          InterlockedExchange(&g_runtime.raknet_logon_marked, 0);
          InterlockedExchange(&g_runtime.raknet_rpc_flags, 0);
          InterlockedExchange(&g_runtime.raknet_game_rpc_flags, 0);
          InterlockedExchange(&g_runtime.raknet_request_class_outcome, 0);
          InterlockedExchange(&g_runtime.raknet_request_spawn_outcome, 0);
          InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_WAIT_CONNECT);
          runtime_tracef("network_prepare: RakNet disconnected -> state=WAIT_CONNECT packet=%d drained=%d state_before=%ld "
                         "pump_calls=%ld saturated=%ld",
                         last_packet_id, drained, (long)state_before,
                         (long)InterlockedCompareExchange(&g_runtime.raknet_pump_calls, 0, 0),
                         (long)InterlockedCompareExchange(&g_runtime.raknet_pump_saturated_count, 0, 0));
          break;
        case 36: /* ID_CONNECTION_BANNED */
        case 37: /* ID_INVALID_PASSWORD */
          InterlockedExchange(&g_runtime.net_mgr_connected, 0);
          InterlockedExchange(&g_runtime.netgame_state, SAMP_NETGAME_FAILED);
          runtime_tracef("network_prepare: RakNet terminal packet -> state=FAILED");
          break;
        case 38: /* ID_MODIFIED_PACKET */
          /*
           * PROBE_TRACE + INFERRED:
           * A replacement run produced ID_MODIFIED_PACKET after normal user RPCs and then spammed FAILED before
           * ID_CONNECTION_LOST. Treat this as a bad/ignored packet signal, not as a local terminal state.
           * TODO_VERIFY against original 0.3.7 RakNet traces.
           */
          runtime_tracef("network_prepare: RakNet modified packet ignored -> keep state=%ld", (long)state_before);
          break;
        default:
          break;
      }
    }

    if (disconnect_packet) {
      rpc_flags = 0u;
      connected_after_pump = 0;
    }

    if (connected_after_pump) {
      InterlockedExchange(&g_runtime.net_mgr_connected, 1);
    }

    if (connected_after_pump && (rpc_flags & SAMP_RAKNET_RPC_FLAG_INIT_GAME) != 0u) {
      if (InterlockedCompareExchange(&g_runtime.raknet_logon_marked, 1, 0) == 0) {
        int mark_result = samp_raknet_client_mark_logged_on(g_runtime.net_mgr.raknet_client);
        /*
         * INFERRED + PROBE_TRACE:
         * Replacement runs dropped with ID_CONNECTION_LOST almost exactly 30s after join while
         * pump_saturated=0. The embedded SA-MP 0.3.7 RakPeer fork disconnects CONNECTED peers
         * after 30s while remoteSystem.isLogon is false. InitGame is the first confirmed game
         * logon marker we currently observe on the client side. TODO_VERIFY against original 0.3.7.
         */
        runtime_tracef("network_prepare: RakNet mark logged-on result=%d flags=0x%08lx state_before=%ld", mark_result,
                       (unsigned long)rpc_flags, (long)state_before);
        if (mark_result != 0) {
          InterlockedExchange(&g_runtime.raknet_logon_marked, 0);
        }
      }
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
  chat_input_uninstall_wndproc_compat();
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
  uninstall_select_device_hook_compat();
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
  uninstall_select_device_hook_compat();
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
