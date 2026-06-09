#include "sampdll/net/raknet_client_adapter.h"

#include <cstdarg>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "raknet/BitStream.h"
#include "raknet/GetTime.h"
#include "raknet/NetworkTypes.h"
#include "raknet/PacketEnumerations.h"
#include "raknet/PacketPriority.h"
#include "raknet/RakClient.h"
#include "raknet/RakClientInterface.h"
#include "raknet/RakNetworkFactory.h"
#include "raknet/StringCompressor.h"

extern "C" const char *samp_raknet_knogle_auth_response(const char *challenge);

namespace {
constexpr unsigned char kPacketIdInvalid = 0xFFu;
constexpr unsigned char kPacketVehicleSync = 200u;
constexpr unsigned char kPacketPlayerSync = 207u;
constexpr RakNet::RPCID kRpcClientJoin = static_cast<RakNet::RPCID>(25u);
constexpr RakNet::RPCID kRpcDialogResponse = static_cast<RakNet::RPCID>(62u);
constexpr RakNet::RPCID kRpcSpawn = static_cast<RakNet::RPCID>(52u);
constexpr RakNet::RPCID kRpcServerCommand = static_cast<RakNet::RPCID>(50u);
constexpr RakNet::RPCID kRpcChat = static_cast<RakNet::RPCID>(101u);
constexpr RakNet::RPCID kRpcClientCheck = static_cast<RakNet::RPCID>(103u);
constexpr RakNet::RPCID kRpcClickTextDraw = static_cast<RakNet::RPCID>(83u);
constexpr unsigned int kRpcScrDialogBox = 61U;
constexpr RakNet::RPCID kRpcRequestClass = static_cast<RakNet::RPCID>(128u);
constexpr RakNet::RPCID kRpcRequestSpawn = static_cast<RakNet::RPCID>(129u);
constexpr RakNet::RPCID kRpcUpdateScoresPingsIPs = static_cast<RakNet::RPCID>(155u);
constexpr unsigned int kDefaultNetgameVersion = 4057u;
constexpr unsigned char kDefaultModByte = 1u;
constexpr const char *kDefaultNickname = "Player";
constexpr const char *kDefaultClientVersion = "0.3.7";
constexpr unsigned long kGpciFactor = 0x3e9UL;
constexpr unsigned int kRpcTraceMaxBytes = 32U;
constexpr unsigned int kRpcRegisterMax = 200U;
constexpr RakNet::RakNetTime kInitialSpawnDelayMs = 2200U;
constexpr RakNet::RakNetTime kClassSelectionManualDelayMs = 2000U;
constexpr RakNet::RakNetTime kClassSelectionReselectDelayMs = 250U;
constexpr RakNet::RakNetTime kPostDialogSpawnDelayMs = 450U;
constexpr RakNet::RakNetTime kSpawnRetryDelayMs = 1600U;
constexpr RakNet::RakNetTime kSelectModeClickDelayMs = 250U;
constexpr RakNet::RakNetTime kScorePingUpdateMs = 3000U;
constexpr unsigned int kMaxSpawnRetries = 4U;
constexpr unsigned short kDefaultFreeroamTextDrawId = 24U;
constexpr unsigned int kObservedPlayerSpawnInfoBytes = 45U;
constexpr unsigned int kPlayerSpawnInfoBytes = 46U;
constexpr unsigned int kDialogTitleBytes = 256U;
constexpr unsigned int kDialogInfoBytes = 4096U;
constexpr unsigned int kDialogButtonBytes = 64U;
constexpr unsigned int kDialogInputBytes = 256U;
constexpr unsigned int kChatInputBytes = 128U;
constexpr unsigned int kCompactTextDrawTransmitBytes = 40U;
constexpr unsigned int kTextDrawShowMinBytes = 2U + kCompactTextDrawTransmitBytes;
constexpr unsigned int kTextDrawEditMinBytes = 2U;
constexpr unsigned int kOpenMpTextDrawHeaderBytes = 67U;
constexpr unsigned int kOpenMpTextDrawStringLengthOffset = 65U;
constexpr unsigned int kOpenMpTextDrawStringOffset = 67U;
constexpr unsigned int kOpenMpObjectCreateAttachVehicleOffset = 35U;
constexpr unsigned int kOpenMpObjectCreateAttachObjectOffset = 37U;
constexpr unsigned int kOpenMpObjectCreateMaterialCountOffset = 39U;
constexpr unsigned int kOpenMpObjectCreateAttachDataBytes = 25U;
constexpr unsigned int kOpenMpObjectCreateMinBytes = kOpenMpObjectCreateMaterialCountOffset + 1U;
constexpr unsigned int kOpenMpObjectSetPosMinBytes = 2U + 3U * 4U;
constexpr unsigned int kOpenMpObjectSetRotMinBytes = 2U + 3U * 4U;
constexpr unsigned int kOpenMpObjectDestroyMinBytes = 2U;
constexpr unsigned int kOpenMpObjectMoveMinBytes = 2U + 10U * 4U;
constexpr unsigned int kOpenMpObjectStopMinBytes = 2U;
constexpr unsigned short kOpenMpInvalidObjectId = 0xFFFFU;
constexpr unsigned short kOpenMpInvalidVehicleId = 0xFFFFU;
constexpr unsigned int kOpenMpVehicleAddBytes = 63U;
constexpr unsigned int kOpenMpVehicleModOffset = 40U;
constexpr unsigned int kOpenMpVehiclePaintjobOffset = 54U;
constexpr unsigned int kOpenMpVehicleBodyColor1Offset = 55U;
constexpr unsigned int kOpenMpVehicleBodyColor2Offset = 59U;
constexpr unsigned int kOpenMpVehicleRemoveMinBytes = 2U;
constexpr int kSampVehicleModelMin = 400;
constexpr int kSampVehicleModelMax = 611;
static_assert(sizeof(samp_raknet_onfoot_sync) == 68U, "SA-MP 0.3.7 on-foot sync payload must be 68 bytes");
static_assert(sizeof(samp_raknet_incar_sync) == 63U, "SA-MP 0.3.7 in-car sync payload must be 63 bytes");

bool packet_resets_session_state(unsigned char packet_id) {
  return packet_id == static_cast<unsigned char>(RakNet::ID_DISCONNECTION_NOTIFICATION) ||
         packet_id == static_cast<unsigned char>(RakNet::ID_CONNECTION_LOST);
}

struct RpcProbeState {
  RakNet::RakClientInterface *client;
  char local_nickname[SAMP_RAKNET_PLAYER_NAME_BYTES];
  unsigned char rpc_ids[256];
  unsigned int counts[256];
  unsigned int handler_logged[256];
  int registered;
  int saw_init_game;
  int saw_request_class_reply;
  int saw_request_spawn_reply;
  int saw_spawn_info;
  int saw_dialog;
  int saw_player_pos;
  int saw_player_facing;
  int saw_player_health;
  int saw_player_controllable;
  int saw_weather;
  int saw_world_time;
  int saw_set_time_ex;
  int saw_toggle_clock;
  int saw_interior;
  int saw_camera_pos;
  int saw_camera_look_at;
  int saw_camera_behind;
  int saw_client_message;
  int saw_textdraw_select;
  int saw_object_event;
  int saw_vehicle_event;
  int pending_request_class;
  int pending_request_spawn;
  int pending_select_mode_freeroam_click;
  int pending_dialog_response;
  int pending_request_class_id;
  int sent_request_class;
  int waiting_for_request_class_reply;
  int sent_dialog_response;
  int sent_select_mode_freeroam_click;
  unsigned int request_spawn_send_count;
  unsigned int request_spawn_retry_count;
  int sent_spawn_notify;
  unsigned int sent_spawn_notify_seq;
  int manual_spawn_shift_down;
  int selected_class;
  unsigned short last_dialog_id;
  unsigned char last_dialog_style;
  unsigned short dialog_response_id;
  unsigned char dialog_response_button;
  std::int16_t dialog_response_listitem;
  char dialog_response_input[kDialogInputBytes];
  char dialog_title[kDialogTitleBytes];
  char dialog_info[kDialogInfoBytes];
  char dialog_button1[kDialogButtonBytes];
  char dialog_button2[kDialogButtonBytes];
  unsigned short init_spawns_available;
  unsigned short init_local_player_id;
  unsigned char auth_local_player_id_valid;
  unsigned short auth_local_player_id;
  unsigned char init_show_player_tags;
  unsigned char init_show_player_markers;
  unsigned char init_tire_popping;
  unsigned char init_world_time;
  unsigned char init_weather;
  float init_gravity;
  unsigned char init_lan_mode;
  std::int32_t init_death_drop_money;
  unsigned char init_instagib;
  unsigned char init_zone_names;
  unsigned char init_use_cj_walk;
  unsigned char init_allow_weapons;
  unsigned char init_limit_global_chat_radius;
  float init_global_chat_radius;
  unsigned char init_stunt_bonus;
  float init_name_tag_draw_distance;
  unsigned char init_disable_enter_exits;
  unsigned char init_name_tag_los;
  std::int32_t init_send_rates[6];
  char init_hostname[SAMP_RAKNET_HOSTNAME_BYTES];
  unsigned char init_vehicle_models[SAMP_RAKNET_REQUIRED_VEHICLE_MODELS];
  unsigned char request_class_outcome;
  unsigned char request_spawn_outcome;
  float player_pos[3];
  float player_facing_angle;
  float player_health;
  unsigned int player_pos_seq;
  unsigned int player_facing_seq;
  unsigned int player_health_seq;
  unsigned int player_controllable_seq;
  unsigned int camera_behind_seq;
  unsigned int player_armour_seq;
  unsigned int player_armed_weapon_seq;
  unsigned int reset_player_weapons_seq;
  unsigned int reset_player_money_seq;
  unsigned int play_sound_seq;
  unsigned int stop_audio_stream_seq;
  unsigned int player_color_seq;
  unsigned int player_team_seq;
  unsigned int apply_animation_seq;
  unsigned int world_visual_event_seq;
  unsigned int client_check_response_count;
  unsigned char player_controllable;
  float player_armour;
  unsigned int player_armed_weapon;
  unsigned int play_sound_id;
  float play_sound_pos[3];
  unsigned short player_color_player_id;
  unsigned int player_color;
  unsigned short player_team_player_id;
  unsigned char player_team;
  unsigned short apply_animation_player_id;
  char apply_animation_lib[SAMP_RAKNET_ANIM_LIB_BYTES];
  char apply_animation_name[SAMP_RAKNET_ANIM_NAME_BYTES];
  float apply_animation_delta;
  unsigned char apply_animation_loop;
  unsigned char apply_animation_lock_x;
  unsigned char apply_animation_lock_y;
  unsigned char apply_animation_freeze;
  std::int32_t apply_animation_time;
  unsigned char world_visual_event_type;
  unsigned short world_visual_id;
  std::int32_t world_visual_model;
  unsigned int world_visual_color;
  unsigned int world_visual_material_background;
  float world_visual_pos[4];
  unsigned char world_visual_material_type;
  unsigned char world_visual_material_slot;
  unsigned char world_visual_material_size;
  unsigned char world_visual_material_font_size;
  unsigned char world_visual_material_bold;
  unsigned char world_visual_material_alignment;
  char world_visual_text[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES];
  char world_visual_material_txd[SAMP_RAKNET_WORLD_VISUAL_NAME_BYTES];
  char world_visual_material_texture[SAMP_RAKNET_WORLD_VISUAL_NAME_BYTES];
  unsigned char weather;
  unsigned char world_time_hour;
  unsigned char world_time_minute;
  unsigned char clock_enabled;
  unsigned char interior;
  float camera_pos[3];
  float camera_look_at[3];
  unsigned char camera_look_at_type;
  unsigned char spawn_team;
  unsigned int spawn_info_seq;
  std::int32_t spawn_skin;
  float spawn_pos[3];
  float spawn_rotation;
  std::int32_t spawn_weapons[3];
  std::int32_t spawn_weapon_ammo[3];
  unsigned int client_message_seq;
  samp_raknet_client_message_probe client_messages[SAMP_RAKNET_CLIENT_MESSAGE_RING];
  unsigned int textdraw_event_seq;
  samp_raknet_textdraw_event textdraw_events[SAMP_RAKNET_TEXTDRAW_EVENT_RING];
  unsigned char textdraw_select_active;
  unsigned int textdraw_select_color;
  unsigned int object_event_seq;
  samp_raknet_object_event object_events[SAMP_RAKNET_OBJECT_EVENT_RING];
  unsigned int vehicle_event_seq;
  samp_raknet_vehicle_event vehicle_events[SAMP_RAKNET_VEHICLE_EVENT_RING];
  unsigned int remote_player_event_seq;
  samp_raknet_remote_player_event remote_player_events[SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING];
  unsigned int remote_player_sync_seq;
  samp_raknet_remote_onfoot_sync remote_player_syncs[SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING];
  unsigned int map_icon_event_seq;
  samp_raknet_map_icon_event map_icon_events[SAMP_RAKNET_MAP_ICON_EVENT_RING];
  unsigned int name_tag_event_seq;
  samp_raknet_name_tag_event name_tag_events[SAMP_RAKNET_NAME_TAG_EVENT_RING];
  unsigned int player_pool_event_seq;
  samp_raknet_player_pool_event player_pool_events[SAMP_RAKNET_PLAYER_POOL_EVENT_RING];
  unsigned int score_ping_seq;
  unsigned int score_ping_count;
  samp_raknet_score_ping_entry score_ping_entries[SAMP_RAKNET_SCORE_PING_MAX_ENTRIES];
  RakNet::RakNetTime class_selection_ready_time;
  RakNet::RakNetTime last_request_class_time;
  RakNet::RakNetTime next_request_spawn_time;
  RakNet::RakNetTime select_mode_click_ready_time;
  RakNet::RakNetTime next_score_ping_update_time;
#ifdef _WIN32
  int dialog_window_active;
#endif
};

RpcProbeState g_rpc_probe = {};

void trace_netf(const char *fmt, ...) {
  FILE *file = std::fopen("samp_net_trace.log", "ab");
  if (file == nullptr) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  std::vfprintf(file, fmt, args);
  va_end(args);
  std::fputc('\n', file);
  std::fclose(file);
}

bool env_bool_enabled(const char *name, bool default_value) {
  const char *value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return default_value;
  }
  return value[0] != '0' && value[0] != 'n' && value[0] != 'N' && value[0] != 'f' && value[0] != 'F';
}

bool auto_request_spawn_enabled() {
  return env_bool_enabled("SAMPDLL_AUTO_REQUEST_SPAWN", false);
}

bool auto_select_freeroam_enabled() {
  return env_bool_enabled("SAMPDLL_AUTO_SELECT_FREEROAM", false);
}

int clamp_class_id(int selected_class) {
  const int spawns_available =
      g_rpc_probe.init_spawns_available != 0U ? static_cast<int>(g_rpc_probe.init_spawns_available) : 1;

  if (spawns_available <= 0) {
    return 0;
  }
  if (selected_class < 0) {
    return spawns_available - 1;
  }
  if (selected_class >= spawns_available) {
    return 0;
  }
  return selected_class;
}

void schedule_request_class(int selected_class, const char *reason) {
  const int clamped_class = clamp_class_id(selected_class);

  g_rpc_probe.selected_class = clamped_class;
  g_rpc_probe.pending_request_class_id = clamped_class;
  g_rpc_probe.pending_request_class = 1;
  trace_netf("rpc-state RequestClass scheduled reason=%s selected_class=%d spawns_available=%u evidence=INFERRED,OPENMP_REF",
             reason != nullptr ? reason : "unknown", clamped_class,
             static_cast<unsigned int>(g_rpc_probe.init_spawns_available));
}

bool class_selection_manual_ready(void) {
  return !g_rpc_probe.saw_dialog && !g_rpc_probe.pending_request_spawn && !g_rpc_probe.pending_request_class &&
         !g_rpc_probe.waiting_for_request_class_reply && g_rpc_probe.saw_request_class_reply &&
         g_rpc_probe.request_class_outcome != 0U && g_rpc_probe.request_spawn_send_count == 0U &&
         g_rpc_probe.class_selection_ready_time != 0U && RakNet::GetTime() >= g_rpc_probe.class_selection_ready_time;
}

bool debug_dialog_window_enabled() {
  const char *value = std::getenv("SAMPDLL_DEBUG_DIALOG");

  if (value != nullptr && value[0] != '\0') {
    return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
  }
#ifdef _WIN32
  {
    const char *cmdline = GetCommandLineA();
    if (cmdline != nullptr && std::strstr(cmdline, "--debug-dialog") != nullptr) {
      return true;
    }
  }
#endif
  return false;
}

void schedule_request_spawn_if_ready(const char *reason, RakNet::RakNetTime delay_ms) {
  RakNet::RakNetTime now = RakNet::GetTime();
  RakNet::RakNetTime ready_time = now + delay_ms;

  if (!auto_request_spawn_enabled()) {
    trace_netf("rpc-state RequestSpawn not scheduled reason=%s auto_disabled=1 observe_only=1",
               reason != nullptr ? reason : "unknown");
    return;
  }
  if (g_rpc_probe.pending_request_spawn || g_rpc_probe.request_spawn_send_count != 0U) {
    return;
  }
  if (!g_rpc_probe.saw_request_class_reply || g_rpc_probe.request_class_outcome == 0U) {
    return;
  }
  if (g_rpc_probe.saw_dialog) {
    trace_netf("rpc-state RequestSpawn delayed reason=%s pending_dialog id=%u observe_only=1",
               reason != nullptr ? reason : "unknown", static_cast<unsigned int>(g_rpc_probe.last_dialog_id));
    return;
  }

  if (g_rpc_probe.class_selection_ready_time != 0U && g_rpc_probe.class_selection_ready_time > ready_time) {
    ready_time = g_rpc_probe.class_selection_ready_time;
  }
  g_rpc_probe.pending_request_spawn = 1;
  g_rpc_probe.next_request_spawn_time = ready_time;
  trace_netf("rpc-state RequestSpawn scheduled reason=%s delay_ms=%u ready_in_ms=%u",
             reason != nullptr ? reason : "unknown", static_cast<unsigned int>(delay_ms),
             static_cast<unsigned int>(ready_time > now ? ready_time - now : 0U));
}

void schedule_select_mode_freeroam_click(const char *reason) {
  if (!auto_select_freeroam_enabled()) {
    trace_netf("rpc-state SelectMode Freeroam click not scheduled reason=%s auto_disabled=1 observe_only=1",
               reason != nullptr ? reason : "unknown");
    return;
  }
  if (g_rpc_probe.sent_select_mode_freeroam_click || g_rpc_probe.pending_select_mode_freeroam_click) {
    return;
  }
  if (g_rpc_probe.request_spawn_send_count == 0U || !g_rpc_probe.saw_request_class_reply ||
      g_rpc_probe.request_class_outcome == 0U || g_rpc_probe.saw_dialog) {
    return;
  }

  g_rpc_probe.pending_select_mode_freeroam_click = 1;
  g_rpc_probe.select_mode_click_ready_time = RakNet::GetTime() + kSelectModeClickDelayMs;
  trace_netf("rpc-state SelectMode Freeroam click scheduled reason=%s textdraw=%u delay_ms=%u",
             reason != nullptr ? reason : "unknown", static_cast<unsigned int>(kDefaultFreeroamTextDrawId),
             static_cast<unsigned int>(kSelectModeClickDelayMs));
}

enum RpcLocalStatus {
  kRpcLocalUnknown = 0,
  kRpcLocalOutgoing,
  kRpcLocalDummy,
  kRpcLocalDecoded,
  kRpcLocalImplemented
};

struct RpcMeta {
  unsigned int id;
  const char *name;
  RpcLocalStatus local_status;
  const char *source;
};

// OPENMP_REF + STATIC_037 + PROBE_TRACE + INFERRED:
// 0.3.7 server-triggered RPC inventory, cross-checked with open.mp semantics,
// SAMPFUNCS constants and observed original-DLL/golden-trace traffic. TODO_VERIFY
// remaining IDs against focused original 0.3.7-R5 traces before treating them as
// exact semantics.
const RpcMeta kRpcMeta[] = {
    {11U, "ScrSetPlayerName", kRpcLocalDummy, "SAMPFUNCS_037"},
    {12U, "ScrSetPlayerPos", kRpcLocalImplemented, "PROBE_TRACE"},
    {13U, "ScrSetPlayerPosFindZ", kRpcLocalDummy, "SAMPFUNCS_037"},
    {14U, "ScrSetPlayerHealth", kRpcLocalImplemented, "STATIC_037"},
    {15U, "ScrTogglePlayerControllable", kRpcLocalImplemented, "STATIC_037"},
    {16U, "ScrPlaySound", kRpcLocalImplemented, "STATIC_037"},
    {17U, "ScrSetWorldBounds", kRpcLocalDummy, "STATIC_037"},
    {18U, "ScrGivePlayerMoney", kRpcLocalDummy, "STATIC_037"},
    {19U, "ScrSetPlayerFacingAngle", kRpcLocalImplemented, "PROBE_TRACE"},
    {20U, "ScrResetPlayerMoney", kRpcLocalImplemented, "STATIC_037"},
    {21U, "ScrResetPlayerWeapons", kRpcLocalImplemented, "STATIC_037"},
    {22U, "ScrGivePlayerWeapon", kRpcLocalDummy, "STATIC_037"},
    {23U, "OnPlayerClickPlayer", kRpcLocalOutgoing, "OPENMP_REF"},
    {24U, "ScrSetVehicleParamsEx", kRpcLocalDummy, "SAMPFUNCS_037"},
    {25U, "ClientJoin", kRpcLocalOutgoing, "OPENMP_REF"},
    {26U, "EnterVehicle", kRpcLocalOutgoing, "OPENMP_REF"},
    {27U, "EnterEditObject", kRpcLocalOutgoing, "OPENMP_REF"},
    {28U, "ScrCancelEdit", kRpcLocalDummy, "SAMPFUNCS_037"},
    {29U, "ScrSetPlayerTime/SetTimeEx", kRpcLocalImplemented, "PROBE_TRACE"},
    {30U, "ScrToggleClock", kRpcLocalDummy, "SAMPFUNCS_037"},
    {31U, "ScriptCash", kRpcLocalOutgoing, "OPENMP_REF"},
    {32U, "ScrWorldPlayerAdd", kRpcLocalImplemented, "STATIC_037,PROBE_TRACE"},
    {33U, "ScrSetPlayerShopName", kRpcLocalDummy, "SAMPFUNCS_037"},
    {34U, "ScrSetPlayerSkillLevel", kRpcLocalDummy, "SAMPFUNCS_037"},
    {35U, "ScrSetPlayerDrunkLevel", kRpcLocalDummy, "SAMPFUNCS_037"},
    {36U, "ScrCreate3DTextLabel", kRpcLocalDecoded, "OPENMP_REF"},
    {37U, "ScrDisableCheckpoint", kRpcLocalDummy, "STATIC_037"},
    {38U, "ScrSetRaceCheckpoint", kRpcLocalDummy, "STATIC_037"},
    {39U, "ScrDisableRaceCheckpoint", kRpcLocalDummy, "STATIC_037"},
    {40U, "ScrGameModeRestart", kRpcLocalDummy, "STATIC_037"},
    {41U, "ScrPlayAudioStream", kRpcLocalDummy, "SAMPFUNCS_037"},
    {42U, "ScrStopAudioStream", kRpcLocalDecoded, "OPENMP_REF"},
    {43U, "ScrRemoveBuildingForPlayer", kRpcLocalDecoded, "OPENMP_REF"},
    {44U, "ScrCreateObject", kRpcLocalImplemented, "PROBE_TRACE"},
    {45U, "ScrSetObjectPos", kRpcLocalImplemented, "PROBE_TRACE"},
    {46U, "ScrSetObjectRot", kRpcLocalImplemented, "PROBE_TRACE"},
    {47U, "ScrDestroyObject", kRpcLocalImplemented, "PROBE_TRACE"},
    {50U, "ServerCommand", kRpcLocalOutgoing, "OPENMP_REF"},
    {52U, "Spawn", kRpcLocalOutgoing, "OPENMP_REF"},
    {53U, "Death", kRpcLocalOutgoing, "OPENMP_REF"},
    {54U, "NPCJoin", kRpcLocalOutgoing, "OPENMP_REF"},
    {55U, "ScrDeathMessage", kRpcLocalDummy, "STATIC_037"},
    {56U, "ScrSetPlayerMapIcon", kRpcLocalImplemented, "INFERRED,TODO_VERIFY"},
    {57U, "ScrRemoveVehicleComponent", kRpcLocalDummy, "STATIC_037"},
    {58U, "ScrUpdate3DTextLabel", kRpcLocalDummy, "SAMPFUNCS_037"},
    {59U, "ScrChatBubble", kRpcLocalDummy, "SAMPFUNCS_037"},
    {60U, "ScrSomeUpdate", kRpcLocalDummy, "SAMPFUNCS_037"},
    {61U, "ScrShowDialog", kRpcLocalImplemented, "PROBE_TRACE"},
    {62U, "DialogResponse", kRpcLocalOutgoing, "OPENMP_REF"},
    {63U, "ScrDestroyPickup", kRpcLocalDummy, "STATIC_037"},
    {65U, "ScrLinkVehicleToInterior", kRpcLocalDummy, "STATIC_037"},
    {66U, "ScrSetPlayerArmour", kRpcLocalImplemented, "OPENMP_REF"},
    {67U, "ScrSetPlayerArmedWeapon", kRpcLocalImplemented, "OPENMP_REF"},
    {68U, "ScrSetSpawnInfo", kRpcLocalImplemented, "PROBE_TRACE"},
    {69U, "ScrSetPlayerTeam", kRpcLocalDecoded, "OPENMP_REF"},
    {70U, "ScrPutPlayerInVehicle", kRpcLocalImplemented, "PROBE_TRACE,STATIC_037,TODO_VERIFY"},
    {71U, "ScrRemovePlayerFromVehicle", kRpcLocalDummy, "STATIC_037"},
    {72U, "ScrSetPlayerColor", kRpcLocalDecoded, "OPENMP_REF"},
    {73U, "ScrDisplayGameText", kRpcLocalDummy, "STATIC_037"},
    {74U, "ScrForceClassSelection", kRpcLocalDummy, "STATIC_037"},
    {75U, "ScrAttachObjectToPlayer", kRpcLocalDecoded, "PROBE_TRACE"},
    {76U, "ScrInitMenu", kRpcLocalDummy, "STATIC_037"},
    {77U, "ScrShowMenu", kRpcLocalDummy, "STATIC_037"},
    {78U, "ScrHideMenu", kRpcLocalDummy, "STATIC_037"},
    {79U, "ScrCreateExplosion", kRpcLocalDummy, "STATIC_037"},
    {80U, "ScrShowPlayerNameTagForPlayer", kRpcLocalImplemented, "INFERRED,TODO_VERIFY"},
    {81U, "ScrAttachCameraToObject", kRpcLocalDummy, "SAMPFUNCS_037"},
    {82U, "ScrInterpolateCamera", kRpcLocalDummy, "SAMPFUNCS_037"},
    {83U, "ClickTextDraw/SelectTextDraw", kRpcLocalImplemented, "PROBE_TRACE"},
    {84U, "ScrSetObjectMaterial", kRpcLocalDecoded, "OPENMP_REF"},
    {85U, "ScrGangZoneStopFlash", kRpcLocalDummy, "SAMPFUNCS_037"},
    {86U, "ScrApplyAnimation", kRpcLocalDecoded, "STATIC_037"},
    {87U, "ScrClearAnimations", kRpcLocalDummy, "STATIC_037"},
    {88U, "ScrSetPlayerSpecialAction", kRpcLocalDummy, "STATIC_037"},
    {89U, "ScrSetPlayerFightingStyle", kRpcLocalDummy, "SAMPFUNCS_037"},
    {90U, "ScrSetPlayerVelocity", kRpcLocalDummy, "SAMPFUNCS_037"},
    {91U, "ScrSetVehicleVelocity", kRpcLocalDummy, "SAMPFUNCS_037"},
    {93U, "ScrClientMessage", kRpcLocalImplemented, "PROBE_TRACE"},
    {94U, "ScrSetWorldTime", kRpcLocalImplemented, "PROBE_TRACE"},
    {95U, "ScrCreatePickup", kRpcLocalDummy, "STATIC_037"},
    {96U, "ScmEvent", kRpcLocalOutgoing, "OPENMP_REF"},
    {97U, "WeaponPickupDestroy", kRpcLocalOutgoing, "OPENMP_REF"},
    {99U, "ScrMoveObject", kRpcLocalImplemented, "PROBE_TRACE"},
    {101U, "Chat", kRpcLocalImplemented, "INFERRED,OPENMP_REF,TODO_VERIFY"},
    {102U, "ServerNetStats", kRpcLocalOutgoing, "OPENMP_REF"},
    {103U, "ClientCheck", kRpcLocalImplemented, "OPENMP_REF"},
    {104U, "ScrEnableStuntBonusForPlayer", kRpcLocalDummy, "STATIC_037"},
    {105U, "ScrTextDrawSetString", kRpcLocalImplemented, "PROBE_TRACE"},
    {106U, "DamageVehicle", kRpcLocalOutgoing, "OPENMP_REF"},
    {107U, "ScrSetCheckpoint", kRpcLocalDummy, "STATIC_037"},
    {108U, "ScrGangZoneCreate", kRpcLocalDecoded, "OPENMP_REF"},
    {112U, "ScrPlayCrimeReport", kRpcLocalDummy, "SAMPFUNCS_037"},
    {113U, "ScrSetPlayerAttachedObject", kRpcLocalDummy, "SAMPFUNCS_037"},
    {115U, "GiveTakeDamage", kRpcLocalOutgoing, "OPENMP_REF"},
    {116U, "EditAttachedObject", kRpcLocalOutgoing, "OPENMP_REF"},
    {117U, "EditObject", kRpcLocalOutgoing, "OPENMP_REF"},
    {118U, "SetInteriorId", kRpcLocalOutgoing, "OPENMP_REF"},
    {119U, "MapMarker", kRpcLocalOutgoing, "OPENMP_REF"},
    {120U, "ScrGangZoneDestroy", kRpcLocalDummy, "STATIC_037"},
    {121U, "ScrGangZoneFlash", kRpcLocalDummy, "STATIC_037"},
    {122U, "ScrStopObject", kRpcLocalImplemented, "PROBE_TRACE"},
    {123U, "ScrSetNumberPlate", kRpcLocalDummy, "STATIC_037"},
    {124U, "ScrTogglePlayerSpectating", kRpcLocalDummy, "STATIC_037"},
    {126U, "ScrPlayerSpectatePlayer", kRpcLocalDummy, "STATIC_037"},
    {127U, "ScrPlayerSpectateVehicle", kRpcLocalDummy, "STATIC_037"},
    {128U, "RequestClass", kRpcLocalImplemented, "OPENMP_REF"},
    {129U, "RequestSpawn", kRpcLocalImplemented, "OPENMP_REF"},
    {131U, "PickedUpPickup", kRpcLocalOutgoing, "OPENMP_REF"},
    {132U, "MenuSelect", kRpcLocalOutgoing, "OPENMP_REF"},
    {133U, "ScrSetPlayerWantedLevel", kRpcLocalDummy, "STATIC_037"},
    {134U, "ScrShowTextDraw", kRpcLocalImplemented, "PROBE_TRACE"},
    {135U, "ScrHideTextDraw", kRpcLocalImplemented, "PROBE_TRACE"},
    {136U, "ScrEditTextDraw/VehicleDestroyed", kRpcLocalImplemented, "PROBE_TRACE"},
    {137U, "ScrServerJoin", kRpcLocalDecoded, "PROBE_TRACE"},
    {138U, "ScrServerQuit", kRpcLocalDecoded, "STATIC_037"},
    {139U, "ScrInitGame", kRpcLocalImplemented, "PROBE_TRACE"},
    {140U, "MenuQuit", kRpcLocalOutgoing, "OPENMP_REF"},
    {144U, "ScrRemovePlayerMapIcon/TogClockCompat", kRpcLocalImplemented, "INFERRED,PROBE_TRACE,TODO_VERIFY"},
    {145U, "ScrSetPlayerAmmo", kRpcLocalDummy, "SAMPFUNCS_037"},
    {146U, "ScrSetGravity", kRpcLocalDummy, "STATIC_037"},
    {147U, "ScrSetVehicleHealth", kRpcLocalImplemented, "PROBE_TRACE"},
    {148U, "ScrAttachTrailerToVehicle", kRpcLocalDummy, "STATIC_037"},
    {149U, "ScrDetachTrailerFromVehicle", kRpcLocalDummy, "STATIC_037"},
    {152U, "ScrSetWeather", kRpcLocalImplemented, "PROBE_TRACE"},
    {153U, "ScrSetPlayerSkin", kRpcLocalDummy, "STATIC_037"},
    {154U, "ExitVehicle", kRpcLocalOutgoing, "OPENMP_REF"},
    {155U, "UpdateScoresPingsIPs", kRpcLocalImplemented, "STATIC_037"},
    {156U, "ScrSetPlayerInterior", kRpcLocalImplemented, "PROBE_TRACE"},
    {157U, "ScrSetPlayerCameraPos", kRpcLocalImplemented, "PROBE_TRACE"},
    {158U, "ScrSetPlayerCameraLookAt", kRpcLocalImplemented, "PROBE_TRACE"},
    {159U, "ScrSetVehiclePos", kRpcLocalDummy, "STATIC_037"},
    {160U, "ScrSetVehicleZAngle", kRpcLocalDummy, "STATIC_037"},
    {161U, "ScrSetVehicleParamsForPlayer", kRpcLocalDummy, "STATIC_037"},
    {162U, "ScrSetCameraBehindPlayer", kRpcLocalImplemented, "STATIC_037"},
    {163U, "ScrWorldPlayerRemove", kRpcLocalImplemented, "SAMPFUNCS_037,PROBE_TRACE"},
    {164U, "ScrWorldVehicleAdd", kRpcLocalImplemented, "PROBE_TRACE"},
    {165U, "ScrWorldVehicleRemove", kRpcLocalImplemented, "PROBE_TRACE"},
    {166U, "ScrWorldPlayerDeath", kRpcLocalImplemented, "SAMPFUNCS_037,PROBE_TRACE"},
};

const RpcMeta *rpc_meta(unsigned int rpc_id) {
  for (unsigned int i = 0U; i < (sizeof(kRpcMeta) / sizeof(kRpcMeta[0])); ++i) {
    if (kRpcMeta[i].id == rpc_id) {
      return &kRpcMeta[i];
    }
  }
  return nullptr;
}

const char *rpc_local_status_name(RpcLocalStatus status) {
  switch (status) {
    case kRpcLocalOutgoing:
      return "outgoing";
    case kRpcLocalDummy:
      return "dummy";
    case kRpcLocalDecoded:
      return "decoded";
    case kRpcLocalImplemented:
      return "implemented";
    case kRpcLocalUnknown:
    default:
      return "unknown";
  }
}

RpcLocalStatus rpc_local_status(unsigned int rpc_id) {
  const RpcMeta *meta = rpc_meta(rpc_id);
  return meta != nullptr ? meta->local_status : kRpcLocalUnknown;
}

const char *rpc_name(unsigned int rpc_id) {
  const RpcMeta *meta = rpc_meta(rpc_id);
  return meta != nullptr ? meta->name : "unknown";
}

const char *rpc_source(unsigned int rpc_id) {
  const RpcMeta *meta = rpc_meta(rpc_id);
  return meta != nullptr ? meta->source : "TODO_VERIFY";
}

unsigned int rpc_min_payload_bytes(unsigned int rpc_id) {
  switch (rpc_id) {
    case 12U:
    case 157U:
    case 158U:
      return 12U;
    case 14U:
      return 4U;
    case 15U:
      return 1U;
    case 16U:
      return 16U;
    case 19U:
      return 4U;
    case 36U:
      return 27U;
    case 43U:
      return 20U;
    case 29U:
      return 2U;
    case 32U:
      return 28U;
    case 44U:
      return kOpenMpObjectCreateMinBytes;
    case 45U:
      return kOpenMpObjectSetPosMinBytes;
    case 46U:
      return kOpenMpObjectSetRotMinBytes;
    case 47U:
      return kOpenMpObjectDestroyMinBytes;
    case 61U:
      return 2U;
    case 66U:
    case 67U:
      return 4U;
    case 68U:
      return kObservedPlayerSpawnInfoBytes;
    case 69U:
      return 3U;
    case 70U:
    case 71U:
      return 2U;
    case 72U:
      return 6U;
    case 83U:
      return 0U;
    case 84U:
      return 4U;
    case 86U:
      return 16U;
    case 93U:
      return 8U;
    case 94U:
    case 144U:
    case 152U:
    case 156U:
      return 1U;
    case 99U:
      return kOpenMpObjectMoveMinBytes;
    case 103U:
      return 9U;
    case 108U:
      return 22U;
    case 105U:
    case 122U:
    case 135U:
    case 136U:
    case 163U:
    case 165U:
    case 166U:
      return 2U;
    case 134U:
      return kTextDrawShowMinBytes;
    case 139U:
    case 162U:
      return 0U;
    case 164U:
      return kOpenMpVehicleAddBytes;
    default:
      return 0U;
  }
}

const char *rpc_handler_mode(unsigned int rpc_id) {
  switch (rpc_local_status(rpc_id)) {
    case kRpcLocalOutgoing:
      return "unexpected_inbound";
    case kRpcLocalDummy:
      return "noop";
    case kRpcLocalDecoded:
      return "decoded";
    case kRpcLocalImplemented:
      return "applied_partial";
    case kRpcLocalUnknown:
    default:
      return "unknown";
  }
}

const char *rpc_handler_action(unsigned int rpc_id) {
  switch (rpc_local_status(rpc_id)) {
    case kRpcLocalImplemented:
      return "dispatch_existing";
    case kRpcLocalDecoded:
      return "decode_log_only";
    case kRpcLocalDummy:
    case kRpcLocalOutgoing:
    case kRpcLocalUnknown:
    default:
      return "log_only";
  }
}

bool rpc_handler_surface_should_log(unsigned int rpc_id, unsigned int bytes) {
  if (rpc_id >= 256U) {
    return true;
  }

  const unsigned int min_bytes = rpc_min_payload_bytes(rpc_id);
  if (min_bytes != 0U && bytes < min_bytes) {
    return true;
  }

  const unsigned int count = g_rpc_probe.counts[rpc_id];
  return g_rpc_probe.handler_logged[rpc_id] == 0U || (count % 64U) == 0U;
}

void observe_rpc_handler_surface(unsigned int rpc_id, unsigned int bytes, unsigned int bits) {
  if (!rpc_handler_surface_should_log(rpc_id, bytes)) {
    return;
  }

  if (rpc_id < 256U) {
    g_rpc_probe.handler_logged[rpc_id] = 1U;
  }

  const unsigned int min_bytes = rpc_min_payload_bytes(rpc_id);
  const int payload_short = min_bytes != 0U && bytes < min_bytes;
  trace_netf("rpc-handler id=%u name=%s mode=%s local=%s source=%s count=%u min_bytes=%u bytes=%u bits=%u payload=%s action=%s TODO_VERIFY=%d",
             rpc_id, rpc_name(rpc_id), rpc_handler_mode(rpc_id), rpc_local_status_name(rpc_local_status(rpc_id)),
             rpc_source(rpc_id), (rpc_id < 256U) ? g_rpc_probe.counts[rpc_id] : 0U, min_bytes, bytes, bits,
             payload_short ? "short" : "ok", rpc_handler_action(rpc_id),
             (rpc_local_status(rpc_id) == kRpcLocalUnknown || rpc_local_status(rpc_id) == kRpcLocalDummy) ? 1 : 0);
}

unsigned int rpc_known_meta_count() {
  return static_cast<unsigned int>(sizeof(kRpcMeta) / sizeof(kRpcMeta[0]));
}

unsigned int rpc_dummy_meta_count() {
  unsigned int count = 0U;
  for (unsigned int i = 0U; i < (sizeof(kRpcMeta) / sizeof(kRpcMeta[0])); ++i) {
    if (kRpcMeta[i].local_status == kRpcLocalDummy) {
      ++count;
    }
  }
  return count;
}

void reset_rpc_probe_runtime(RakNet::RakClientInterface *client) {
  g_rpc_probe.client = client;
  std::memset(g_rpc_probe.local_nickname, 0, sizeof(g_rpc_probe.local_nickname));
  std::strncpy(g_rpc_probe.local_nickname, kDefaultNickname, sizeof(g_rpc_probe.local_nickname) - 1U);
  std::memset(g_rpc_probe.counts, 0, sizeof(g_rpc_probe.counts));
  std::memset(g_rpc_probe.handler_logged, 0, sizeof(g_rpc_probe.handler_logged));
  g_rpc_probe.saw_init_game = 0;
  g_rpc_probe.saw_request_class_reply = 0;
  g_rpc_probe.saw_request_spawn_reply = 0;
  g_rpc_probe.saw_spawn_info = 0;
  g_rpc_probe.saw_dialog = 0;
  g_rpc_probe.saw_player_pos = 0;
  g_rpc_probe.saw_player_facing = 0;
  g_rpc_probe.saw_player_health = 0;
  g_rpc_probe.saw_player_controllable = 0;
  g_rpc_probe.saw_weather = 0;
  g_rpc_probe.saw_world_time = 0;
  g_rpc_probe.saw_set_time_ex = 0;
  g_rpc_probe.saw_toggle_clock = 0;
  g_rpc_probe.saw_interior = 0;
  g_rpc_probe.saw_camera_pos = 0;
  g_rpc_probe.saw_camera_look_at = 0;
  g_rpc_probe.saw_camera_behind = 0;
  g_rpc_probe.saw_client_message = 0;
  g_rpc_probe.saw_textdraw_select = 0;
  g_rpc_probe.saw_object_event = 0;
  g_rpc_probe.saw_vehicle_event = 0;
  g_rpc_probe.pending_request_class = 0;
  g_rpc_probe.pending_request_spawn = 0;
  g_rpc_probe.pending_select_mode_freeroam_click = 0;
  g_rpc_probe.pending_dialog_response = 0;
  g_rpc_probe.pending_request_class_id = 0;
  g_rpc_probe.sent_request_class = 0;
  g_rpc_probe.waiting_for_request_class_reply = 0;
  g_rpc_probe.sent_dialog_response = 0;
  g_rpc_probe.sent_select_mode_freeroam_click = 0;
  g_rpc_probe.request_spawn_send_count = 0;
  g_rpc_probe.request_spawn_retry_count = 0;
  g_rpc_probe.sent_spawn_notify = 0;
  g_rpc_probe.sent_spawn_notify_seq = 0U;
  g_rpc_probe.manual_spawn_shift_down = 0;
  g_rpc_probe.selected_class = 0;
  g_rpc_probe.last_dialog_id = 0;
  g_rpc_probe.last_dialog_style = 0;
  g_rpc_probe.dialog_response_id = 0;
  g_rpc_probe.dialog_response_button = 0;
  g_rpc_probe.dialog_response_listitem = -1;
  g_rpc_probe.dialog_response_input[0] = '\0';
  g_rpc_probe.dialog_title[0] = '\0';
  g_rpc_probe.dialog_info[0] = '\0';
  g_rpc_probe.dialog_button1[0] = '\0';
  g_rpc_probe.dialog_button2[0] = '\0';
  g_rpc_probe.init_spawns_available = 0;
  g_rpc_probe.init_local_player_id = 0;
  g_rpc_probe.auth_local_player_id_valid = 0U;
  g_rpc_probe.auth_local_player_id = 0;
  g_rpc_probe.init_show_player_tags = 0;
  g_rpc_probe.init_show_player_markers = 0;
  g_rpc_probe.init_tire_popping = 0;
  g_rpc_probe.init_world_time = 0;
  g_rpc_probe.init_weather = 0;
  g_rpc_probe.init_gravity = 0.0f;
  g_rpc_probe.init_lan_mode = 0;
  g_rpc_probe.init_death_drop_money = 0;
  g_rpc_probe.init_instagib = 0;
  g_rpc_probe.init_zone_names = 0;
  g_rpc_probe.init_use_cj_walk = 0;
  g_rpc_probe.init_allow_weapons = 0;
  g_rpc_probe.init_limit_global_chat_radius = 0;
  g_rpc_probe.init_global_chat_radius = 0.0f;
  g_rpc_probe.init_stunt_bonus = 0;
  g_rpc_probe.init_name_tag_draw_distance = 0.0f;
  g_rpc_probe.init_disable_enter_exits = 0;
  g_rpc_probe.init_name_tag_los = 0;
  std::memset(g_rpc_probe.init_send_rates, 0, sizeof(g_rpc_probe.init_send_rates));
  std::memset(g_rpc_probe.init_hostname, 0, sizeof(g_rpc_probe.init_hostname));
  std::memset(g_rpc_probe.init_vehicle_models, 0, sizeof(g_rpc_probe.init_vehicle_models));
  g_rpc_probe.request_class_outcome = 0;
  g_rpc_probe.request_spawn_outcome = 0;
  std::memset(g_rpc_probe.player_pos, 0, sizeof(g_rpc_probe.player_pos));
  g_rpc_probe.player_facing_angle = 0.0f;
  g_rpc_probe.player_health = 100.0f;
  g_rpc_probe.player_pos_seq = 0U;
  g_rpc_probe.player_facing_seq = 0U;
  g_rpc_probe.player_health_seq = 0U;
  g_rpc_probe.player_controllable_seq = 0U;
  g_rpc_probe.camera_behind_seq = 0U;
  g_rpc_probe.player_armour_seq = 0U;
  g_rpc_probe.player_armed_weapon_seq = 0U;
  g_rpc_probe.reset_player_weapons_seq = 0U;
  g_rpc_probe.reset_player_money_seq = 0U;
  g_rpc_probe.play_sound_seq = 0U;
  g_rpc_probe.stop_audio_stream_seq = 0U;
  g_rpc_probe.player_color_seq = 0U;
  g_rpc_probe.player_team_seq = 0U;
  g_rpc_probe.apply_animation_seq = 0U;
  g_rpc_probe.world_visual_event_seq = 0U;
  g_rpc_probe.client_check_response_count = 0U;
  g_rpc_probe.player_controllable = 1U;
  g_rpc_probe.player_armour = 0.0f;
  g_rpc_probe.player_armed_weapon = 0U;
  g_rpc_probe.play_sound_id = 0U;
  std::memset(g_rpc_probe.play_sound_pos, 0, sizeof(g_rpc_probe.play_sound_pos));
  g_rpc_probe.player_color_player_id = 0U;
  g_rpc_probe.player_color = 0U;
  g_rpc_probe.player_team_player_id = 0U;
  g_rpc_probe.player_team = 0U;
  g_rpc_probe.apply_animation_player_id = 0U;
  g_rpc_probe.apply_animation_lib[0] = '\0';
  g_rpc_probe.apply_animation_name[0] = '\0';
  g_rpc_probe.apply_animation_delta = 0.0f;
  g_rpc_probe.apply_animation_loop = 0U;
  g_rpc_probe.apply_animation_lock_x = 0U;
  g_rpc_probe.apply_animation_lock_y = 0U;
  g_rpc_probe.apply_animation_freeze = 0U;
  g_rpc_probe.apply_animation_time = 0;
  g_rpc_probe.world_visual_event_type = 0U;
  g_rpc_probe.world_visual_id = 0U;
  g_rpc_probe.world_visual_model = 0;
  g_rpc_probe.world_visual_color = 0U;
  g_rpc_probe.world_visual_material_background = 0U;
  std::memset(g_rpc_probe.world_visual_pos, 0, sizeof(g_rpc_probe.world_visual_pos));
  g_rpc_probe.world_visual_material_type = 0U;
  g_rpc_probe.world_visual_material_slot = 0U;
  g_rpc_probe.world_visual_material_size = 0U;
  g_rpc_probe.world_visual_material_font_size = 0U;
  g_rpc_probe.world_visual_material_bold = 0U;
  g_rpc_probe.world_visual_material_alignment = 0U;
  g_rpc_probe.world_visual_text[0] = '\0';
  g_rpc_probe.world_visual_material_txd[0] = '\0';
  g_rpc_probe.world_visual_material_texture[0] = '\0';
  g_rpc_probe.weather = 0;
  g_rpc_probe.world_time_hour = 12U;
  g_rpc_probe.world_time_minute = 0U;
  g_rpc_probe.clock_enabled = 0U;
  g_rpc_probe.interior = 0;
  std::memset(g_rpc_probe.camera_pos, 0, sizeof(g_rpc_probe.camera_pos));
  std::memset(g_rpc_probe.camera_look_at, 0, sizeof(g_rpc_probe.camera_look_at));
  g_rpc_probe.camera_look_at_type = 0;
  g_rpc_probe.spawn_team = 0;
  g_rpc_probe.spawn_info_seq = 0U;
  g_rpc_probe.spawn_skin = 0;
  std::memset(g_rpc_probe.spawn_pos, 0, sizeof(g_rpc_probe.spawn_pos));
  g_rpc_probe.spawn_rotation = 0.0f;
  std::memset(g_rpc_probe.spawn_weapons, 0, sizeof(g_rpc_probe.spawn_weapons));
  std::memset(g_rpc_probe.spawn_weapon_ammo, 0, sizeof(g_rpc_probe.spawn_weapon_ammo));
  g_rpc_probe.client_message_seq = 0;
  std::memset(g_rpc_probe.client_messages, 0, sizeof(g_rpc_probe.client_messages));
  g_rpc_probe.textdraw_event_seq = 0U;
  std::memset(g_rpc_probe.textdraw_events, 0, sizeof(g_rpc_probe.textdraw_events));
  g_rpc_probe.textdraw_select_active = 0U;
  g_rpc_probe.textdraw_select_color = 0U;
  g_rpc_probe.object_event_seq = 0U;
  std::memset(g_rpc_probe.object_events, 0, sizeof(g_rpc_probe.object_events));
  g_rpc_probe.vehicle_event_seq = 0U;
  std::memset(g_rpc_probe.vehicle_events, 0, sizeof(g_rpc_probe.vehicle_events));
  g_rpc_probe.remote_player_event_seq = 0U;
  std::memset(g_rpc_probe.remote_player_events, 0, sizeof(g_rpc_probe.remote_player_events));
  g_rpc_probe.remote_player_sync_seq = 0U;
  std::memset(g_rpc_probe.remote_player_syncs, 0, sizeof(g_rpc_probe.remote_player_syncs));
  g_rpc_probe.map_icon_event_seq = 0U;
  std::memset(g_rpc_probe.map_icon_events, 0, sizeof(g_rpc_probe.map_icon_events));
  g_rpc_probe.name_tag_event_seq = 0U;
  std::memset(g_rpc_probe.name_tag_events, 0, sizeof(g_rpc_probe.name_tag_events));
  g_rpc_probe.player_pool_event_seq = 0U;
  std::memset(g_rpc_probe.player_pool_events, 0, sizeof(g_rpc_probe.player_pool_events));
  g_rpc_probe.score_ping_seq = 0U;
  g_rpc_probe.score_ping_count = 0U;
  std::memset(g_rpc_probe.score_ping_entries, 0, sizeof(g_rpc_probe.score_ping_entries));
  g_rpc_probe.class_selection_ready_time = 0;
  g_rpc_probe.last_request_class_time = 0;
  g_rpc_probe.next_request_spawn_time = 0;
  g_rpc_probe.select_mode_click_ready_time = 0;
  g_rpc_probe.next_score_ping_update_time = 0;
#ifdef _WIN32
  g_rpc_probe.dialog_window_active = 0;
#endif
}

unsigned short read_le16(const unsigned char *data) {
  return static_cast<unsigned short>(static_cast<unsigned short>(data[0]) |
                                     (static_cast<unsigned short>(data[1]) << 8));
}

unsigned int read_le32(const unsigned char *data) {
  return static_cast<unsigned int>(data[0]) | (static_cast<unsigned int>(data[1]) << 8) |
         (static_cast<unsigned int>(data[2]) << 16) | (static_cast<unsigned int>(data[3]) << 24);
}

float read_le_float(const unsigned char *data) {
  const unsigned int raw = read_le32(data);
  float value = 0.0f;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

std::int32_t read_le_i32(const unsigned char *data) {
  return static_cast<std::int32_t>(read_le32(data));
}

void read_vec3(const unsigned char *data, float out[3]) {
  out[0] = read_le_float(data);
  out[1] = read_le_float(data + 4);
  out[2] = read_le_float(data + 8);
}

unsigned char client_check_result(unsigned char type, unsigned int address, unsigned short offset, unsigned short count) {
  /*
   * OPENMP_REF + PROBE_TRACE + TODO_VERIFY:
   * open.mp writes ClientCheck requests as type/address/offset/count and expects a type/address/result response on
   * the same RPC id. The observed 0x48 request is used by common scripts as a client capability probe; return a
   * neutral result until original 0.3.7 behaviour is traced per check type.
   */
  (void)type;
  (void)address;
  (void)offset;
  (void)count;
  return 0U;
}

bool send_client_check_response(RakNet::RakClientInterface *rak_client, unsigned char type, unsigned int address,
                                unsigned char result) {
  RakNet::BitStream bs_send;

  if (rak_client == nullptr) {
    return false;
  }

  bs_send.Write(type);
  bs_send.Write(address);
  bs_send.Write(result);
  const int sent = rak_client->RPC(kRpcClientCheck, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                  RakNet::UNASSIGNED_NETWORK_ID, nullptr);
  ++g_rpc_probe.client_check_response_count;
  trace_netf("rpc-user-out id=103 name=ClientCheck type=0x%02x address=0x%08x result=0x%02x sent=%d count=%u",
             static_cast<unsigned int>(type), address, static_cast<unsigned int>(result), sent,
             g_rpc_probe.client_check_response_count);
  return sent != 0;
}

bool handle_client_check_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 9U) {
    return false;
  }

  const unsigned char type = data[0];
  const unsigned int address = read_le32(data + 1U);
  const unsigned short offset = read_le16(data + 5U);
  const unsigned short count = read_le16(data + 7U);
  const unsigned char result = client_check_result(type, address, offset, count);
  const bool sent = send_client_check_response(g_rpc_probe.client, type, address, result);
  trace_netf("rpc-state id=103 client_check type=0x%02x address=0x%08x offset=%u count=%u result=0x%02x sent=%d",
             static_cast<unsigned int>(type), address, static_cast<unsigned int>(offset),
             static_cast<unsigned int>(count), static_cast<unsigned int>(result), sent ? 1 : 0);
  return sent;
}

bool spawn_info_plausible(std::int32_t skin, const float pos[3], float rotation) {
  return skin >= 0 && skin <= 311 && std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
         std::isfinite(rotation) && pos[0] > -20000.0f && pos[0] < 20000.0f && pos[1] > -20000.0f &&
         pos[1] < 20000.0f && pos[2] > -1000.0f && pos[2] < 10000.0f && rotation > -720.0f &&
         rotation < 720.0f;
}

void read_spawn_info(const unsigned char *data, unsigned int bytes, const char *source) {
  unsigned int offset = 0;
  unsigned char extra = 0U;
  unsigned char team = 0U;
  std::int32_t skin = 0;
  float pos[3] = {0.0f, 0.0f, 0.0f};
  float rotation = 0.0f;
  std::int32_t weapons[3] = {0, 0, 0};
  std::int32_t ammo[3] = {0, 0, 0};

  if (data == nullptr || bytes < kObservedPlayerSpawnInfoBytes) {
    return;
  }

  team = data[offset++];
  skin = read_le_i32(data + offset);
  offset += 4U;

  if (bytes >= kPlayerSpawnInfoBytes) {
    extra = data[offset++];
  }
  if (bytes < (offset + 40U)) {
    return;
  }

  read_vec3(data + offset, pos);
  offset += 12U;
  rotation = read_le_float(data + offset);
  offset += 4U;
  for (unsigned int i = 0; i < 3U; ++i) {
    weapons[i] = read_le_i32(data + offset);
    offset += 4U;
  }
  for (unsigned int i = 0; i < 3U; ++i) {
    ammo[i] = read_le_i32(data + offset);
    offset += 4U;
  }

  if (!spawn_info_plausible(skin, pos, rotation)) {
    trace_netf("rpc-state spawn_info source=%s invalid=1 ignored=1 bytes=%u extra=%u team=%u skin=%d pos=%.3f %.3f %.3f rot=%.3f",
               source != nullptr ? source : "unknown", bytes, static_cast<unsigned int>(extra),
               static_cast<unsigned int>(team), static_cast<int>(skin), static_cast<double>(pos[0]),
               static_cast<double>(pos[1]), static_cast<double>(pos[2]), static_cast<double>(rotation));
    return;
  }

  g_rpc_probe.spawn_team = team;
  g_rpc_probe.spawn_skin = skin;
  g_rpc_probe.spawn_pos[0] = pos[0];
  g_rpc_probe.spawn_pos[1] = pos[1];
  g_rpc_probe.spawn_pos[2] = pos[2];
  g_rpc_probe.spawn_rotation = rotation;
  for (unsigned int i = 0; i < 3U; ++i) {
    g_rpc_probe.spawn_weapons[i] = weapons[i];
    g_rpc_probe.spawn_weapon_ammo[i] = ammo[i];
  }
  g_rpc_probe.saw_spawn_info = 1;
  ++g_rpc_probe.spawn_info_seq;
  if (g_rpc_probe.spawn_info_seq == 0U) {
    g_rpc_probe.spawn_info_seq = 1U;
  }

  trace_netf("rpc-state spawn_info source=%s seq=%u bytes=%u extra=%u team=%u skin=%d pos=%.3f %.3f %.3f rot=%.3f weapons=%d/%d/%d ammo=%d/%d/%d",
             source != nullptr ? source : "unknown", static_cast<unsigned int>(g_rpc_probe.spawn_info_seq), bytes,
             static_cast<unsigned int>(extra),
             static_cast<unsigned int>(g_rpc_probe.spawn_team), static_cast<int>(g_rpc_probe.spawn_skin),
             static_cast<double>(g_rpc_probe.spawn_pos[0]), static_cast<double>(g_rpc_probe.spawn_pos[1]),
             static_cast<double>(g_rpc_probe.spawn_pos[2]), static_cast<double>(g_rpc_probe.spawn_rotation),
             static_cast<int>(g_rpc_probe.spawn_weapons[0]), static_cast<int>(g_rpc_probe.spawn_weapons[1]),
             static_cast<int>(g_rpc_probe.spawn_weapons[2]), static_cast<int>(g_rpc_probe.spawn_weapon_ammo[0]),
             static_cast<int>(g_rpc_probe.spawn_weapon_ammo[1]), static_cast<int>(g_rpc_probe.spawn_weapon_ammo[2]));
}

bool object_id_valid(unsigned int object_id) {
  return object_id < SAMP_RAKNET_MAX_OBJECTS;
}

bool object_model_plausible(std::int32_t model) {
  return model >= 0 && model <= 20000;
}

bool object_vec_plausible(const float vec[3]) {
  return vec != nullptr && std::isfinite(vec[0]) && std::isfinite(vec[1]) && std::isfinite(vec[2]) &&
         vec[0] > -30000.0f && vec[0] < 30000.0f && vec[1] > -30000.0f && vec[1] < 30000.0f &&
         vec[2] > -5000.0f && vec[2] < 20000.0f;
}

bool object_rot_plausible(const float vec[3]) {
  return vec != nullptr && std::isfinite(vec[0]) && std::isfinite(vec[1]) && std::isfinite(vec[2]) &&
         vec[0] > -10000.0f && vec[0] < 10000.0f && vec[1] > -10000.0f && vec[1] < 10000.0f &&
         vec[2] > -10000.0f && vec[2] < 10000.0f;
}

samp_raknet_object_event *queue_object_event(unsigned char action, unsigned short object_id) {
  ++g_rpc_probe.object_event_seq;
  if (g_rpc_probe.object_event_seq == 0U) {
    g_rpc_probe.object_event_seq = 1U;
  }
  const unsigned int slot = (g_rpc_probe.object_event_seq - 1U) % SAMP_RAKNET_OBJECT_EVENT_RING;
  samp_raknet_object_event *event = &g_rpc_probe.object_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = g_rpc_probe.object_event_seq;
  event->action = action;
  event->object_id = object_id;
  g_rpc_probe.saw_object_event = 1;
  return event;
}

bool decode_object_create_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  unsigned short attach_vehicle_id = kOpenMpInvalidVehicleId;
  unsigned short attach_object_id = kOpenMpInvalidObjectId;
  unsigned int material_count_offset = kOpenMpObjectCreateMaterialCountOffset;
  unsigned int materials_count = 0U;
  std::int32_t model = 0;
  float draw_distance = 0.0f;
  float pos[3] = {0.0f, 0.0f, 0.0f};
  float rot[3] = {0.0f, 0.0f, 0.0f};
  samp_raknet_object_event *event = nullptr;
  const bool has_attachment = data != nullptr && bytes >= kOpenMpObjectCreateMinBytes &&
                              (read_le16(data + kOpenMpObjectCreateAttachVehicleOffset) != kOpenMpInvalidVehicleId ||
                               read_le16(data + kOpenMpObjectCreateAttachObjectOffset) != kOpenMpInvalidObjectId);

  if (data == nullptr || bytes < kOpenMpObjectCreateMinBytes) {
    return false;
  }
  object_id = read_le16(data);
  model = read_le_i32(data + 2U);
  read_vec3(data + 6U, pos);
  read_vec3(data + 18U, rot);
  draw_distance = read_le_float(data + 30U);
  attach_vehicle_id = read_le16(data + kOpenMpObjectCreateAttachVehicleOffset);
  attach_object_id = read_le16(data + kOpenMpObjectCreateAttachObjectOffset);
  if (has_attachment) {
    material_count_offset += kOpenMpObjectCreateAttachDataBytes;
    if (bytes < material_count_offset + 1U) {
      trace_netf("object-decode: create invalid_attachment_payload id=%u bytes=%u material_offset=%u attach_vehicle=%u attach_object=%u",
                 static_cast<unsigned int>(object_id), bytes, material_count_offset,
                 static_cast<unsigned int>(attach_vehicle_id), static_cast<unsigned int>(attach_object_id));
      return false;
    }
  }
  materials_count = static_cast<unsigned int>(data[material_count_offset]);
  if (!object_id_valid(object_id) || !object_model_plausible(model) || !object_vec_plausible(pos) ||
      !object_rot_plausible(rot) || !std::isfinite(draw_distance) || draw_distance < 0.0f ||
      draw_distance > 10000.0f) {
    trace_netf("object-decode: create invalid id=%u model=%d bytes=%u draw=%.3f pos=%.3f %.3f %.3f rot=%.3f %.3f %.3f",
               static_cast<unsigned int>(object_id), static_cast<int>(model), bytes,
               static_cast<double>(draw_distance), static_cast<double>(pos[0]), static_cast<double>(pos[1]),
               static_cast<double>(pos[2]),
               static_cast<double>(rot[0]), static_cast<double>(rot[1]), static_cast<double>(rot[2]));
    return false;
  }

  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_CREATE, object_id);
  event->model = model;
  std::memcpy(event->pos, pos, sizeof(event->pos));
  std::memcpy(event->rot, rot, sizeof(event->rot));
  event->has_pos = 1U;
  event->has_attachment = has_attachment ? 1U : 0U;
  event->materials_count = static_cast<unsigned char>(materials_count);
  trace_netf("object-decode: create seq=%u id=%u model=%d draw=%.3f attach=%u materials=%u pos=%.3f %.3f %.3f rot=%.3f %.3f %.3f",
             event->seq, static_cast<unsigned int>(object_id), static_cast<int>(model),
             static_cast<double>(draw_distance), has_attachment ? 1U : 0U, materials_count,
             static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]),
             static_cast<double>(rot[0]), static_cast<double>(rot[1]), static_cast<double>(rot[2]));
  return true;
}

bool decode_object_set_pos_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  float pos[3] = {0.0f, 0.0f, 0.0f};
  samp_raknet_object_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpObjectSetPosMinBytes) {
    return false;
  }
  object_id = read_le16(data);
  read_vec3(data + 2U, pos);
  if (!object_id_valid(object_id) || !object_vec_plausible(pos)) {
    trace_netf("object-decode: set_pos invalid id=%u bytes=%u pos=%.3f %.3f %.3f",
               static_cast<unsigned int>(object_id), bytes, static_cast<double>(pos[0]),
               static_cast<double>(pos[1]), static_cast<double>(pos[2]));
    return false;
  }

  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_SET_POS, object_id);
  std::memcpy(event->pos, pos, sizeof(event->pos));
  event->has_pos = 1U;
  trace_netf("object-decode: set_pos seq=%u id=%u pos=%.3f %.3f %.3f", event->seq,
             static_cast<unsigned int>(object_id), static_cast<double>(pos[0]), static_cast<double>(pos[1]),
             static_cast<double>(pos[2]));
  return true;
}

bool decode_object_set_rot_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  float rot[3] = {0.0f, 0.0f, 0.0f};
  samp_raknet_object_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpObjectSetRotMinBytes) {
    return false;
  }
  object_id = read_le16(data);
  read_vec3(data + 2U, rot);
  if (!object_id_valid(object_id) || !object_rot_plausible(rot)) {
    trace_netf("object-decode: set_rot invalid id=%u bytes=%u rot=%.3f %.3f %.3f",
               static_cast<unsigned int>(object_id), bytes, static_cast<double>(rot[0]),
               static_cast<double>(rot[1]), static_cast<double>(rot[2]));
    return false;
  }

  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_SET_ROT, object_id);
  std::memcpy(event->rot, rot, sizeof(event->rot));
  trace_netf("object-decode: set_rot seq=%u id=%u rot=%.3f %.3f %.3f", event->seq,
             static_cast<unsigned int>(object_id), static_cast<double>(rot[0]), static_cast<double>(rot[1]),
             static_cast<double>(rot[2]));
  return true;
}

bool decode_object_destroy_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  samp_raknet_object_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpObjectDestroyMinBytes) {
    return false;
  }
  object_id = read_le16(data);
  if (!object_id_valid(object_id)) {
    trace_netf("object-decode: destroy invalid id=%u bytes=%u", static_cast<unsigned int>(object_id), bytes);
    return false;
  }

  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_DESTROY, object_id);
  trace_netf("object-decode: destroy seq=%u id=%u", event->seq, static_cast<unsigned int>(object_id));
  return true;
}

bool decode_object_move_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  float from[3] = {0.0f, 0.0f, 0.0f};
  float to[3] = {0.0f, 0.0f, 0.0f};
  float target_rot[3] = {0.0f, 0.0f, 0.0f};
  float speed = 0.0f;
  samp_raknet_object_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpObjectMoveMinBytes) {
    return false;
  }
  object_id = read_le16(data);
  read_vec3(data + 2U, from);
  read_vec3(data + 14U, to);
  speed = read_le_float(data + 26U);
  read_vec3(data + 30U, target_rot);
  if (!object_id_valid(object_id) || !object_vec_plausible(from) || !object_vec_plausible(to) ||
      !object_rot_plausible(target_rot) || !std::isfinite(speed) || speed < 0.0f || speed > 10000.0f) {
    trace_netf("object-decode: move invalid id=%u bytes=%u from=%.3f %.3f %.3f to=%.3f %.3f %.3f speed=%.3f rot=%.3f %.3f %.3f",
               static_cast<unsigned int>(object_id), bytes, static_cast<double>(from[0]),
               static_cast<double>(from[1]), static_cast<double>(from[2]), static_cast<double>(to[0]),
               static_cast<double>(to[1]), static_cast<double>(to[2]), static_cast<double>(speed),
               static_cast<double>(target_rot[0]), static_cast<double>(target_rot[1]),
               static_cast<double>(target_rot[2]));
    return false;
  }

  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_MOVE, object_id);
  std::memcpy(event->move_from, from, sizeof(event->move_from));
  std::memcpy(event->move_to, to, sizeof(event->move_to));
  std::memcpy(event->rot, target_rot, sizeof(event->rot));
  event->move_speed = speed;
  event->has_pos = 1U;
  trace_netf("object-decode: move seq=%u id=%u from=%.3f %.3f %.3f to=%.3f %.3f %.3f speed=%.3f rot=%.3f %.3f %.3f",
             event->seq, static_cast<unsigned int>(object_id), static_cast<double>(from[0]),
             static_cast<double>(from[1]), static_cast<double>(from[2]), static_cast<double>(to[0]),
             static_cast<double>(to[1]), static_cast<double>(to[2]), static_cast<double>(speed),
             static_cast<double>(target_rot[0]), static_cast<double>(target_rot[1]),
             static_cast<double>(target_rot[2]));
  return true;
}

bool decode_object_stop_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  samp_raknet_object_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpObjectStopMinBytes) {
    return false;
  }
  object_id = read_le16(data);
  if (!object_id_valid(object_id)) {
    trace_netf("object-decode: stop invalid id=%u bytes=%u", static_cast<unsigned int>(object_id), bytes);
    return false;
  }

  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_STOP, object_id);
  trace_netf("object-decode: stop seq=%u id=%u", event->seq, static_cast<unsigned int>(object_id));
  return true;
}

bool vehicle_id_valid(unsigned int vehicle_id) {
  return vehicle_id < SAMP_RAKNET_MAX_VEHICLES;
}

bool vehicle_model_valid(std::int32_t model) {
  return model >= kSampVehicleModelMin && model <= kSampVehicleModelMax;
}

bool vehicle_vec_plausible(const float vec[3]) {
  return vec != nullptr && std::isfinite(vec[0]) && std::isfinite(vec[1]) && std::isfinite(vec[2]) &&
         vec[0] > -30000.0f && vec[0] < 30000.0f && vec[1] > -30000.0f && vec[1] < 30000.0f &&
         vec[2] > -5000.0f && vec[2] < 20000.0f;
}

bool vehicle_rotation_plausible(float rotation) {
  return std::isfinite(rotation) && rotation > -720.0f && rotation < 720.0f;
}

bool vehicle_health_plausible(float health) {
  return std::isfinite(health) && health >= 0.0f && health <= 10000.0f;
}

samp_raknet_vehicle_event *queue_vehicle_event(unsigned char action, unsigned short vehicle_id) {
  ++g_rpc_probe.vehicle_event_seq;
  if (g_rpc_probe.vehicle_event_seq == 0U) {
    g_rpc_probe.vehicle_event_seq = 1U;
  }
  const unsigned int slot = (g_rpc_probe.vehicle_event_seq - 1U) % SAMP_RAKNET_VEHICLE_EVENT_RING;
  samp_raknet_vehicle_event *event = &g_rpc_probe.vehicle_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = g_rpc_probe.vehicle_event_seq;
  event->action = action;
  event->vehicle_id = vehicle_id;
  g_rpc_probe.saw_vehicle_event = 1;
  return event;
}

bool decode_vehicle_add_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short vehicle_id = 0U;
  std::int32_t model = 0;
  float pos[3] = {0.0f, 0.0f, 0.0f};
  float rotation = 0.0f;
  float health = 0.0f;
  unsigned int mods_nonzero = 0U;
  samp_raknet_vehicle_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpVehicleAddBytes) {
    trace_netf("vehicle-decode: create unsupported_payload bytes=%u expected_openmp_bytes=%u",
               bytes, kOpenMpVehicleAddBytes);
    return false;
  }

  /* OPENMP_REF:
   * Shared/NetCode/vehicle.hpp StreamInVehicle RPC 164 writes this 63-byte 0.3.7 payload.
   * Other compact payload variants are shorter; do not reinterpret those as vehicles here.
   */
  vehicle_id = read_le16(data);
  model = read_le_i32(data + 2U);
  read_vec3(data + 6U, pos);
  rotation = read_le_float(data + 18U);
  health = read_le_float(data + 24U);
  if (!vehicle_id_valid(vehicle_id) || !vehicle_model_valid(model) || !vehicle_vec_plausible(pos) ||
      !vehicle_rotation_plausible(rotation) || !vehicle_health_plausible(health)) {
    trace_netf("vehicle-decode: create invalid id=%u model=%d bytes=%u pos=%.3f %.3f %.3f rot=%.3f health=%.3f",
               static_cast<unsigned int>(vehicle_id), static_cast<int>(model), bytes,
               static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]),
               static_cast<double>(rotation), static_cast<double>(health));
    return false;
  }

  event = queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_CREATE, vehicle_id);
  event->model = model;
  std::memcpy(event->pos, pos, sizeof(event->pos));
  event->rotation = rotation;
  event->color1 = data[22U];
  event->color2 = data[23U];
  event->health = health;
  event->interior = data[28U];
  event->door_damage = read_le32(data + 29U);
  event->panel_damage = read_le32(data + 33U);
  event->light_damage = data[37U];
  event->tyre_damage = data[38U];
  event->siren = data[39U];
  std::memcpy(event->mods, data + kOpenMpVehicleModOffset, sizeof(event->mods));
  event->paintjob = data[kOpenMpVehiclePaintjobOffset];
  event->body_color1 = read_le_i32(data + kOpenMpVehicleBodyColor1Offset);
  event->body_color2 = read_le_i32(data + kOpenMpVehicleBodyColor2Offset);

  for (unsigned int i = 0U; i < SAMP_RAKNET_VEHICLE_MOD_SLOTS; ++i) {
    if (event->mods[i] != 0U) {
      ++mods_nonzero;
    }
  }

  trace_netf("vehicle-decode: create seq=%u id=%u model=%d pos=%.3f %.3f %.3f rot=%.3f colors=%u/%u health=%.3f interior=%u siren=%u mods=%u paintjob=%u body=%d/%d",
             event->seq, static_cast<unsigned int>(vehicle_id), static_cast<int>(model),
             static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]),
             static_cast<double>(rotation), static_cast<unsigned int>(event->color1),
             static_cast<unsigned int>(event->color2), static_cast<double>(health),
             static_cast<unsigned int>(event->interior), static_cast<unsigned int>(event->siren), mods_nonzero,
             static_cast<unsigned int>(event->paintjob), static_cast<int>(event->body_color1),
             static_cast<int>(event->body_color2));
  return true;
}

bool decode_vehicle_remove_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short vehicle_id = 0U;
  samp_raknet_vehicle_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpVehicleRemoveMinBytes) {
    return false;
  }
  vehicle_id = read_le16(data);
  if (!vehicle_id_valid(vehicle_id)) {
    trace_netf("vehicle-decode: destroy invalid id=%u bytes=%u", static_cast<unsigned int>(vehicle_id), bytes);
    return false;
  }

  event = queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_DESTROY, vehicle_id);
  trace_netf("vehicle-decode: destroy seq=%u id=%u", event->seq, static_cast<unsigned int>(vehicle_id));
  return true;
}

bool decode_vehicle_health_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short vehicle_id = 0U;
  float health = 0.0f;
  samp_raknet_vehicle_event *event = nullptr;

  if (data == nullptr || bytes < 6U) {
    return false;
  }

  /* PROBE_TRACE + SAMPFUNCS_037 + TODO_VERIFY:
   * RPC 147 repeatedly arrives as uint16 vehicle id + float health. The observed
   * payload 11 00 00 00 7a 44 decodes to vehicle 17, health 1000.0.
   */
  vehicle_id = read_le16(data);
  health = read_le_float(data + 2U);
  if (!vehicle_id_valid(vehicle_id) || !vehicle_health_plausible(health)) {
    trace_netf("vehicle-decode: health invalid id=%u bytes=%u health=%.3f",
               static_cast<unsigned int>(vehicle_id), bytes, static_cast<double>(health));
    return false;
  }

  event = queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_SET_HEALTH, vehicle_id);
  event->health = health;
  trace_netf("vehicle-decode: health seq=%u id=%u health=%.3f", event->seq,
             static_cast<unsigned int>(vehicle_id), static_cast<double>(health));
  return true;
}

bool decode_put_player_in_vehicle_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short vehicle_id = 0U;
  unsigned char seat_id = 0U;
  samp_raknet_vehicle_event *event = nullptr;

  if (data == nullptr || bytes < 3U) {
    return false;
  }

  /* PROBE_TRACE + STATIC_037 + TODO_VERIFY:
   * 2026-06-09 bare-vtest /vput observed RPC 70 as 3 bytes:
   * uint16 vehicle id + uint8 seat id, e.g. 01 00 00 => vehicle 1, driver.
   */
  vehicle_id = read_le16(data);
  seat_id = data[2U];
  if (!vehicle_id_valid(vehicle_id) || seat_id > 7U) {
    trace_netf("vehicle-decode: put_player invalid id=%u seat=%u bytes=%u",
               static_cast<unsigned int>(vehicle_id), static_cast<unsigned int>(seat_id), bytes);
    return false;
  }

  event = queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_PUT_LOCAL_PLAYER, vehicle_id);
  event->seat_id = seat_id;
  trace_netf("vehicle-decode: put_player seq=%u id=%u seat=%u bytes=%u", event->seq,
             static_cast<unsigned int>(vehicle_id), static_cast<unsigned int>(seat_id), bytes);
  return true;
}

bool decode_init_game_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short spawns_available = 0;
  unsigned short local_player_id = 0;
  bool show_player_tags = false;
  bool show_player_markers = false;
  bool tire_popping = false;
  unsigned char world_time = 0;
  unsigned char weather = 0;
  float gravity = 0.0f;
  bool lan_mode = false;
  std::int32_t death_drop_money = 0;
  bool instagib = false;
  bool zone_names = false;
  bool use_cj_walk = false;
  bool allow_weapons = false;
  bool limit_global_chat_radius = false;
  float global_chat_radius = 0.0f;
  bool stunt_bonus = false;
  float name_tag_draw_distance = 0.0f;
  bool disable_enter_exits = false;
  bool name_tag_los = true;
  std::int32_t send_rates[6] = {0, 0, 0, 0, 0, 0};
  unsigned char hostname_len = 0;
  char hostname[SAMP_RAKNET_HOSTNAME_BYTES] = {0};
  unsigned char vehicle_models[SAMP_RAKNET_REQUIRED_VEHICLE_MODELS] = {0};
  unsigned int vehicle_model_nonzero = 0U;
  unsigned int vehicle_model_total = 0U;
  unsigned int top_vehicle_model = 400U;
  unsigned int top_vehicle_count = 0U;

  if (data == nullptr || bytes == 0U) {
    return false;
  }

  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);

  if (!bs.Read(spawns_available) || !bs.Read(local_player_id) || !bs.Read(show_player_tags) ||
      !bs.Read(show_player_markers) || !bs.Read(tire_popping) || !bs.Read(world_time) || !bs.Read(weather) ||
      !bs.Read(gravity) || !bs.Read(lan_mode) || !bs.Read(death_drop_money) || !bs.Read(instagib) ||
      !bs.Read(zone_names) || !bs.Read(use_cj_walk) || !bs.Read(allow_weapons) ||
      !bs.Read(limit_global_chat_radius) || !bs.Read(global_chat_radius) || !bs.Read(stunt_bonus) ||
      !bs.Read(name_tag_draw_distance) || !bs.Read(disable_enter_exits) || !bs.Read(name_tag_los)) {
    return false;
  }

  for (unsigned int i = 0U; i < 6U; ++i) {
    if (!bs.Read(send_rates[i])) {
      return false;
    }
  }

  if (!bs.Read(hostname_len)) {
    return false;
  }
  if (hostname_len > 0U && !bs.Read(hostname, static_cast<int>(hostname_len))) {
    return false;
  }
  hostname[SAMP_RAKNET_HOSTNAME_BYTES - 1U] = '\0';

  if (!bs.Read(reinterpret_cast<char *>(vehicle_models), SAMP_RAKNET_REQUIRED_VEHICLE_MODELS)) {
    return false;
  }

  g_rpc_probe.init_spawns_available = spawns_available;
  g_rpc_probe.init_local_player_id = local_player_id;
  g_rpc_probe.init_show_player_tags = show_player_tags ? 1U : 0U;
  g_rpc_probe.init_show_player_markers = show_player_markers ? 1U : 0U;
  g_rpc_probe.init_tire_popping = tire_popping ? 1U : 0U;
  g_rpc_probe.init_world_time = world_time;
  g_rpc_probe.init_weather = weather;
  g_rpc_probe.init_gravity = gravity;
  g_rpc_probe.init_lan_mode = lan_mode ? 1U : 0U;
  g_rpc_probe.init_death_drop_money = death_drop_money;
  g_rpc_probe.init_instagib = instagib ? 1U : 0U;
  g_rpc_probe.init_zone_names = zone_names ? 1U : 0U;
  g_rpc_probe.init_use_cj_walk = use_cj_walk ? 1U : 0U;
  g_rpc_probe.init_allow_weapons = allow_weapons ? 1U : 0U;
  g_rpc_probe.init_limit_global_chat_radius = limit_global_chat_radius ? 1U : 0U;
  g_rpc_probe.init_global_chat_radius = global_chat_radius;
  g_rpc_probe.init_stunt_bonus = stunt_bonus ? 1U : 0U;
  g_rpc_probe.init_name_tag_draw_distance = name_tag_draw_distance;
  g_rpc_probe.init_disable_enter_exits = disable_enter_exits ? 1U : 0U;
  g_rpc_probe.init_name_tag_los = name_tag_los ? 1U : 0U;
  std::memcpy(g_rpc_probe.init_send_rates, send_rates, sizeof(g_rpc_probe.init_send_rates));
  std::strncpy(g_rpc_probe.init_hostname, hostname, sizeof(g_rpc_probe.init_hostname) - 1U);
  g_rpc_probe.init_hostname[sizeof(g_rpc_probe.init_hostname) - 1U] = '\0';
  std::memcpy(g_rpc_probe.init_vehicle_models, vehicle_models, sizeof(g_rpc_probe.init_vehicle_models));

  for (unsigned int i = 0U; i < SAMP_RAKNET_REQUIRED_VEHICLE_MODELS; ++i) {
    const unsigned int count = static_cast<unsigned int>(vehicle_models[i]);
    if (count != 0U) {
      ++vehicle_model_nonzero;
      vehicle_model_total += count;
    }
    if (count > top_vehicle_count) {
      top_vehicle_count = count;
      top_vehicle_model = 400U + i;
    }
  }

  trace_netf("rpc-state id=139 init_game spawns=%u local_player=%u tags=%u markers=%u tire=%u time=%u weather=%u "
             "gravity=%.6f lan=%u death_drop=%d instagib=%u zones=%u cj=%u weapons=%u chat_limit=%u "
             "chat_radius=%.3f stunt=%u nametag_dist=%.3f enter_exits_disabled=%u los=%u "
             "send_rates=%d/%d/%d/%d/%d/%d host='%s' veh_nonzero=%u veh_total=%u top_vehicle=%u:%u",
             static_cast<unsigned int>(spawns_available), static_cast<unsigned int>(local_player_id),
             show_player_tags ? 1U : 0U, show_player_markers ? 1U : 0U, tire_popping ? 1U : 0U,
             static_cast<unsigned int>(world_time), static_cast<unsigned int>(weather), static_cast<double>(gravity),
             lan_mode ? 1U : 0U, static_cast<int>(death_drop_money), instagib ? 1U : 0U, zone_names ? 1U : 0U,
             use_cj_walk ? 1U : 0U, allow_weapons ? 1U : 0U, limit_global_chat_radius ? 1U : 0U,
             static_cast<double>(global_chat_radius), stunt_bonus ? 1U : 0U,
             static_cast<double>(name_tag_draw_distance), disable_enter_exits ? 1U : 0U, name_tag_los ? 1U : 0U,
             static_cast<int>(send_rates[0]), static_cast<int>(send_rates[1]), static_cast<int>(send_rates[2]),
             static_cast<int>(send_rates[3]), static_cast<int>(send_rates[4]), static_cast<int>(send_rates[5]),
             hostname, vehicle_model_nonzero, vehicle_model_total, top_vehicle_model, top_vehicle_count);
  return true;
}

void copy_text(char *out, size_t out_size, const char *input) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  out[0] = '\0';
  if (input == nullptr) {
    return;
  }
  std::strncpy(out, input, out_size - 1U);
  out[out_size - 1U] = '\0';
}

unsigned int bump_seq(unsigned int *seq) {
  if (seq == nullptr) {
    return 0U;
  }
  ++(*seq);
  if (*seq == 0U) {
    *seq = 1U;
  }
  return *seq;
}

bool copy_bounded_bytes(char *out, size_t out_size, const unsigned char *data, unsigned int len) {
  if (out == nullptr || out_size == 0U) {
    return false;
  }
  out[0] = '\0';
  if (data == nullptr) {
    return false;
  }
  const unsigned int copy_len = len < (out_size - 1U) ? len : static_cast<unsigned int>(out_size - 1U);
  if (copy_len > 0U) {
    std::memcpy(out, data, copy_len);
  }
  out[copy_len] = '\0';
  return len < out_size;
}

bool read_dynstr8_plain(const unsigned char *data, unsigned int bytes, unsigned int *offset, char *out,
                        size_t out_size) {
  if (data == nullptr || offset == nullptr || *offset >= bytes) {
    return false;
  }
  const unsigned int len = static_cast<unsigned int>(data[*offset]);
  ++(*offset);
  if (len > bytes - *offset) {
    return false;
  }
  (void)copy_bounded_bytes(out, out_size, data + *offset, len);
  *offset += len;
  return true;
}

void queue_player_pool_event(unsigned char action, unsigned short player_id, unsigned int color, unsigned char is_npc,
                             unsigned char reason, const char *name) {
  const unsigned int seq = bump_seq(&g_rpc_probe.player_pool_event_seq);
  const unsigned int index = (seq - 1U) % SAMP_RAKNET_PLAYER_POOL_EVENT_RING;
  samp_raknet_player_pool_event *event = &g_rpc_probe.player_pool_events[index];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->action = action;
  event->player_id = player_id;
  event->color = color;
  event->is_npc = is_npc;
  event->reason = reason;
  copy_text(event->name, sizeof(event->name), name);
}

void queue_map_icon_event(unsigned char action, unsigned char index, const float pos[3], unsigned char icon,
                          unsigned int color) {
  const unsigned int seq = bump_seq(&g_rpc_probe.map_icon_event_seq);
  const unsigned int slot = (seq - 1U) % SAMP_RAKNET_MAP_ICON_EVENT_RING;
  samp_raknet_map_icon_event *event = &g_rpc_probe.map_icon_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->action = action;
  event->index = index;
  event->icon = icon;
  event->color = color;
  if (pos != nullptr) {
    event->pos[0] = pos[0];
    event->pos[1] = pos[1];
    event->pos[2] = pos[2];
  }
}

void queue_name_tag_event(unsigned short player_id, unsigned char show) {
  const unsigned int seq = bump_seq(&g_rpc_probe.name_tag_event_seq);
  const unsigned int slot = (seq - 1U) % SAMP_RAKNET_NAME_TAG_EVENT_RING;
  samp_raknet_name_tag_event *event = &g_rpc_probe.name_tag_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->player_id = player_id;
  event->show = show != 0U ? 1U : 0U;
}

bool decode_server_join_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short player_id = 0U;
  unsigned int color = 0xFFFFFFFFU;
  unsigned char is_npc = 0U;
  unsigned int name_len = 0U;
  char name[SAMP_RAKNET_PLAYER_NAME_BYTES] = {0};

  if (data == nullptr || bytes < 2U) {
    return false;
  }

  /*
   * PROBE_TRACE + OPENMP_REF + TODO_VERIFY:
   * Current 0.3.7/open.mp traces carry uint16 player id, uint32 colour, uint8 NPC flag,
   * uint8 name length, then name. A shorter compact fallback is kept only for malformed
   * or incomplete traces while the exact original-DLL edge cases are still being verified.
   */
  if (bytes >= 8U) {
    player_id = read_le16(data);
    color = read_le32(data + 2U);
    is_npc = data[6U];
    name_len = data[7U];
    if (name_len <= (SAMP_RAKNET_PLAYER_NAME_BYTES - 1U) && (8U + name_len) <= bytes) {
      (void)copy_bounded_bytes(name, sizeof(name), data + 8U, name_len);
      queue_player_pool_event(SAMP_RAKNET_PLAYER_POOL_ACTION_JOIN, player_id, color, is_npc, 0U, name);
      trace_netf("rpc-state id=137 server_join player=%u npc=%u color=0x%08x name='%s' layout=037",
                 static_cast<unsigned int>(player_id), static_cast<unsigned int>(is_npc), color, name);
      return true;
    }
  }

  name_len = data[1U];
  if (name_len <= (SAMP_RAKNET_PLAYER_NAME_BYTES - 1U) && (2U + name_len) <= bytes) {
    player_id = data[0U];
    (void)copy_bounded_bytes(name, sizeof(name), data + 2U, name_len);
    queue_player_pool_event(SAMP_RAKNET_PLAYER_POOL_ACTION_JOIN, player_id, color, 0U, 0U, name);
    trace_netf("rpc-state id=137 server_join player=%u npc=0 color=0xffffffff name='%s' layout=compact",
               static_cast<unsigned int>(player_id), name);
    return true;
  }

  return false;
}

bool decode_server_quit_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short player_id = 0U;
  unsigned char reason = 0U;

  if (data == nullptr || bytes < 2U) {
    return false;
  }

  if (bytes >= 3U) {
    player_id = read_le16(data);
    reason = data[2U];
    queue_player_pool_event(SAMP_RAKNET_PLAYER_POOL_ACTION_QUIT, player_id, 0U, 0U, reason, "");
    trace_netf("rpc-state id=138 server_quit player=%u reason=%u layout=037",
               static_cast<unsigned int>(player_id), static_cast<unsigned int>(reason));
    return true;
  }

  player_id = data[0U];
  reason = data[1U];
  queue_player_pool_event(SAMP_RAKNET_PLAYER_POOL_ACTION_QUIT, player_id, 0U, 0U, reason, "");
  trace_netf("rpc-state id=138 server_quit player=%u reason=%u layout=compact",
             static_cast<unsigned int>(player_id), static_cast<unsigned int>(reason));
  return true;
}

void queue_remote_player_event(unsigned char action, unsigned short player_id, unsigned char team, std::int32_t skin,
                               const float pos[3], float rotation, unsigned int color, unsigned char fighting_style,
                               unsigned char visible, unsigned char death_reason) {
  const unsigned int seq = bump_seq(&g_rpc_probe.remote_player_event_seq);
  const unsigned int index = (seq - 1U) % SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING;
  samp_raknet_remote_player_event *event = &g_rpc_probe.remote_player_events[index];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->action = action;
  event->player_id = player_id;
  event->team = team;
  event->skin = skin;
  event->color = color;
  event->fighting_style = fighting_style;
  event->visible = visible;
  event->death_reason = death_reason;
  event->rotation = rotation;
  if (pos != nullptr) {
    event->pos[0] = pos[0];
    event->pos[1] = pos[1];
    event->pos[2] = pos[2];
  }
}

bool decode_world_player_add_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short player_id = 0U;
  unsigned char team = 0U;
  std::int32_t skin = 0;
  float pos[3] = {0.0f, 0.0f, 0.0f};
  float rotation = 0.0f;
  unsigned int color = 0U;
  unsigned char fighting_style = 4U;
  std::int32_t visible = 1;
  bool decoded = false;

  if (data == nullptr) {
    return false;
  }

  /*
   * STATIC_037 + TODO_VERIFY:
   * WorldPlayerAdd reads playerId, team, skin, pos, rotation, colour, fightingStyle, BOOL visible before Spawn().
   * 0.3.7 traces should still validate the BOOL width, so we keep failures logged instead of guessing variants here.
   */
  if (bytes >= 25U) {
    RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
    decoded = bs.Read(player_id) && bs.Read(team) && bs.Read(skin) && bs.Read(pos[0]) && bs.Read(pos[1]) &&
              bs.Read(pos[2]) && bs.Read(rotation) && bs.Read(color) && bs.Read(fighting_style) && bs.Read(visible);
  }

  if (!decoded && bytes >= 28U) {
    /*
     * PROBE_TRACE + TODO_VERIFY:
     * Observed open.mp/0.3.7-R5 payload is a byte-aligned 50-byte record:
     * uint16 player, uint8 team, int32 skin, vec3 pos, float rotation, uint32 colour,
     * uint8 fightingStyle, followed by extra 0.3.x fields we currently keep opaque.
     */
    player_id = read_le16(data);
    team = data[2U];
    skin = read_le_i32(data + 3U);
    read_vec3(data + 7U, pos);
    rotation = read_le_float(data + 19U);
    color = read_le32(data + 23U);
    fighting_style = data[27U];
    visible = 1;
    decoded = true;
  }

  if (!decoded) {
    return false;
  }

  queue_remote_player_event(SAMP_RAKNET_REMOTE_PLAYER_ACTION_ADD, player_id, team, skin, pos, rotation, color,
                            fighting_style, visible != 0 ? 1U : 0U, 0U);
  trace_netf("rpc-state id=32 world_player_add seq=%u player=%u team=%u skin=%ld pos=%.3f %.3f %.3f rot=%.3f color=0x%08x fighting=%u visible=%d",
             g_rpc_probe.remote_player_event_seq, static_cast<unsigned int>(player_id), static_cast<unsigned int>(team),
             static_cast<long>(skin), static_cast<double>(pos[0]), static_cast<double>(pos[1]),
             static_cast<double>(pos[2]), static_cast<double>(rotation), color,
             static_cast<unsigned int>(fighting_style), visible);
  return true;
}

bool decode_world_player_remove_payload(const unsigned char *data, unsigned int bytes, unsigned char action,
                                        unsigned char reason) {
  unsigned short player_id = 0U;
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);

  if (data == nullptr || bytes < 2U || !bs.Read(player_id)) {
    return false;
  }
  queue_remote_player_event(action, player_id, 0U, 0, nullptr, 0.0f, 0U, 0U, 0U, reason);
  trace_netf("rpc-state id=%u world_player_%s seq=%u player=%u reason=%u", action == SAMP_RAKNET_REMOTE_PLAYER_ACTION_DEATH ? 166U : 163U,
             action == SAMP_RAKNET_REMOTE_PLAYER_ACTION_DEATH ? "death" : "remove",
             g_rpc_probe.remote_player_event_seq, static_cast<unsigned int>(player_id), static_cast<unsigned int>(reason));
  return true;
}

unsigned char decode_health_armour_nibble(unsigned char nibble) {
  if (nibble == 0x0FU) {
    return 100U;
  }
  if (nibble == 0U) {
    return 0U;
  }
  return static_cast<unsigned char>(nibble * 7U);
}

void queue_remote_onfoot_sync(const samp_raknet_remote_onfoot_sync *sync) {
  const unsigned int seq = bump_seq(&g_rpc_probe.remote_player_sync_seq);
  const unsigned int index = (seq - 1U) % SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING;
  samp_raknet_remote_onfoot_sync *event = &g_rpc_probe.remote_player_syncs[index];

  if (sync == nullptr) {
    return;
  }
  std::memcpy(event, sync, sizeof(*event));
  event->seq = seq;
}

bool decode_remote_onfoot_sync_packet(const unsigned char *data, unsigned int bytes) {
  unsigned char packet_id = 0U;
  unsigned char health_armour = 0U;
  bool has_lr = false;
  bool has_ud = false;
  bool has_move_x = false;
  bool has_move_y = false;
  bool has_move_z = false;
  bool has_surfing = false;
  samp_raknet_remote_onfoot_sync sync;
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);

  std::memset(&sync, 0, sizeof(sync));
  sync.surfing_vehicle_id = 0xFFFFU;
  if (data == nullptr || bytes < 2U || !bs.Read(packet_id) || packet_id != kPacketPlayerSync ||
      !bs.Read(sync.player_id)) {
    return false;
  }

  /*
   * STATIC_037 + OPENMP_REF + TODO_VERIFY:
   * Server -> client on-foot sync is packet 207 plus a bit-packed remote-player payload. The old client stores
   * it into ONFOOT_SYNC_DATA and marks the remote player UPDATE_TYPE_ONFOOT.
   */
  if (!bs.Read(has_lr)) {
    return false;
  }
  if (has_lr && !bs.Read(sync.left_right_keys)) {
    return false;
  }
  if (!bs.Read(has_ud)) {
    return false;
  }
  if (has_ud && !bs.Read(sync.up_down_keys)) {
    return false;
  }
  if (!bs.Read(sync.keys) || !bs.Read(reinterpret_cast<char *>(sync.position), static_cast<int>(sizeof(sync.position))) ||
      !bs.Read(sync.rotation) || !bs.Read(health_armour) || !bs.Read(sync.current_weapon) ||
      !bs.Read(sync.special_action)) {
    return false;
  }
  sync.armour = decode_health_armour_nibble(static_cast<unsigned char>(health_armour & 0x0FU));
  sync.health = decode_health_armour_nibble(static_cast<unsigned char>(health_armour >> 4U));

  if (!bs.Read(has_move_x)) {
    return false;
  }
  if (has_move_x && !bs.Read(sync.move_speed[0])) {
    return false;
  }
  if (!bs.Read(has_move_y)) {
    return false;
  }
  if (has_move_y && !bs.Read(sync.move_speed[1])) {
    return false;
  }
  if (!bs.Read(has_move_z)) {
    return false;
  }
  if (has_move_z && !bs.Read(sync.move_speed[2])) {
    return false;
  }
  if (!bs.Read(has_surfing)) {
    return false;
  }
  if (has_surfing && (!bs.Read(sync.surfing_vehicle_id) || !bs.Read(sync.surfing_offsets[0]) ||
                      !bs.Read(sync.surfing_offsets[1]) || !bs.Read(sync.surfing_offsets[2]))) {
    return false;
  }

  queue_remote_onfoot_sync(&sync);
  if (g_rpc_probe.remote_player_sync_seq <= 3U || (g_rpc_probe.remote_player_sync_seq % 64U) == 0U) {
    trace_netf("packet-state id=207 remote_onfoot seq=%u player=%u pos=%.3f %.3f %.3f rot=%.3f hp=%u ar=%u weapon=%u",
               g_rpc_probe.remote_player_sync_seq, static_cast<unsigned int>(sync.player_id),
               static_cast<double>(sync.position[0]), static_cast<double>(sync.position[1]),
               static_cast<double>(sync.position[2]), static_cast<double>(sync.rotation),
               static_cast<unsigned int>(sync.health), static_cast<unsigned int>(sync.armour),
               static_cast<unsigned int>(sync.current_weapon));
  }
  return true;
}

bool decode_update_scores_pings_payload(const unsigned char *data, unsigned int bytes) {
  unsigned int entry_size = 0U;
  unsigned int count = 0U;
  unsigned int out_count = 0U;
  int id_is_16_bit = 0;

  if (data == nullptr || bytes == 0U) {
    return false;
  }

  if ((bytes % 10U) == 0U) {
    entry_size = 10U;
    id_is_16_bit = 1;
  } else if ((bytes % 9U) == 0U) {
    entry_size = 9U;
  } else {
    return false;
  }

  count = bytes / entry_size;
  if (count > SAMP_RAKNET_SCORE_PING_MAX_ENTRIES) {
    count = SAMP_RAKNET_SCORE_PING_MAX_ENTRIES;
  }

  for (unsigned int i = 0U; i < count; ++i) {
    const unsigned int offset = i * entry_size;
    samp_raknet_score_ping_entry *entry = &g_rpc_probe.score_ping_entries[out_count];

    if (id_is_16_bit) {
      entry->player_id = read_le16(data + offset);
      entry->score = read_le_i32(data + offset + 2U);
      entry->ping = read_le32(data + offset + 6U);
    } else {
      entry->player_id = data[offset];
      entry->score = read_le_i32(data + offset + 1U);
      entry->ping = read_le32(data + offset + 5U);
    }
    ++out_count;
  }

  g_rpc_probe.score_ping_count = out_count;
  (void)bump_seq(&g_rpc_probe.score_ping_seq);
  trace_netf("rpc-state id=155 score_ping_update count=%u layout=%s bytes=%u evidence=STATIC_037,OPENMP_REF,TODO_VERIFY",
             out_count, id_is_16_bit ? "037" : "compact", bytes);
  return true;
}

void set_world_visual_event(unsigned char type, unsigned short id, std::int32_t model, unsigned int color,
                            const float *pos, const char *text) {
  const unsigned int seq = bump_seq(&g_rpc_probe.world_visual_event_seq);
  g_rpc_probe.world_visual_event_type = type;
  g_rpc_probe.world_visual_id = id;
  g_rpc_probe.world_visual_model = model;
  g_rpc_probe.world_visual_color = color;
  g_rpc_probe.world_visual_material_background = 0U;
  if (pos != nullptr) {
    std::memcpy(g_rpc_probe.world_visual_pos, pos, sizeof(g_rpc_probe.world_visual_pos));
  } else {
    std::memset(g_rpc_probe.world_visual_pos, 0, sizeof(g_rpc_probe.world_visual_pos));
  }
  g_rpc_probe.world_visual_material_type = 0U;
  g_rpc_probe.world_visual_material_slot = 0U;
  g_rpc_probe.world_visual_material_size = 0U;
  g_rpc_probe.world_visual_material_font_size = 0U;
  g_rpc_probe.world_visual_material_bold = 0U;
  g_rpc_probe.world_visual_material_alignment = 0U;
  copy_text(g_rpc_probe.world_visual_text, sizeof(g_rpc_probe.world_visual_text), text);
  g_rpc_probe.world_visual_material_txd[0] = '\0';
  g_rpc_probe.world_visual_material_texture[0] = '\0';
  trace_netf("rpc-world-visual: seq=%u type=%u id=%u model=%d color=0x%08x pos=%.3f %.3f %.3f %.3f text='%s' observe_only=1 TODO_VERIFY=1",
             seq, static_cast<unsigned int>(type), static_cast<unsigned int>(id), static_cast<int>(model), color,
             static_cast<double>(g_rpc_probe.world_visual_pos[0]),
             static_cast<double>(g_rpc_probe.world_visual_pos[1]),
             static_cast<double>(g_rpc_probe.world_visual_pos[2]),
             static_cast<double>(g_rpc_probe.world_visual_pos[3]), g_rpc_probe.world_visual_text);
}

void set_object_material_event(unsigned short object_id, unsigned char material_type, unsigned char material_slot,
                               std::int32_t model, unsigned int color, unsigned int background,
                               unsigned char material_size, unsigned char font_size, unsigned char bold,
                               unsigned char alignment, const char *txd, const char *texture, const char *summary) {
  float pos[4] = {static_cast<float>(material_type), static_cast<float>(material_slot),
                  static_cast<float>(font_size), static_cast<float>(alignment)};
  const unsigned int seq = bump_seq(&g_rpc_probe.world_visual_event_seq);
  g_rpc_probe.world_visual_event_type = SAMP_RAKNET_WORLD_VISUAL_SET_OBJECT_MATERIAL;
  g_rpc_probe.world_visual_id = object_id;
  g_rpc_probe.world_visual_model = model;
  g_rpc_probe.world_visual_color = color;
  g_rpc_probe.world_visual_material_background = background;
  std::memcpy(g_rpc_probe.world_visual_pos, pos, sizeof(g_rpc_probe.world_visual_pos));
  g_rpc_probe.world_visual_material_type = material_type;
  g_rpc_probe.world_visual_material_slot = material_slot;
  g_rpc_probe.world_visual_material_size = material_size;
  g_rpc_probe.world_visual_material_font_size = font_size;
  g_rpc_probe.world_visual_material_bold = bold;
  g_rpc_probe.world_visual_material_alignment = alignment;
  copy_text(g_rpc_probe.world_visual_text, sizeof(g_rpc_probe.world_visual_text), summary);
  copy_text(g_rpc_probe.world_visual_material_txd, sizeof(g_rpc_probe.world_visual_material_txd), txd);
  copy_text(g_rpc_probe.world_visual_material_texture, sizeof(g_rpc_probe.world_visual_material_texture), texture);
  trace_netf("rpc-world-visual: seq=%u type=set_object_material id=%u mat_type=%u slot=%u model=%d color=0x%08x "
             "bg=0x%08x txd='%s' texture='%s' summary='%s' apply_pending=1 evidence=OPENMP_REF,PROBE_TRACE,TODO_VERIFY",
             seq, static_cast<unsigned int>(object_id), static_cast<unsigned int>(material_type),
             static_cast<unsigned int>(material_slot), static_cast<int>(model), color, background,
             g_rpc_probe.world_visual_material_txd, g_rpc_probe.world_visual_material_texture,
             g_rpc_probe.world_visual_text);
}

bool decode_player_armour_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) {
    return false;
  }
  const float armour = read_le_float(data);
  if (!std::isfinite(armour) || armour < 0.0f || armour > 10000.0f) {
    trace_netf("rpc-state id=66 invalid_player_armour=%.3f bytes=%u ignored=1", static_cast<double>(armour),
               bytes);
    return false;
  }
  g_rpc_probe.player_armour = armour;
  const unsigned int seq = bump_seq(&g_rpc_probe.player_armour_seq);
  trace_netf("rpc-state id=66 player_armour_seq=%u armour=%.3f apply_pending=1", seq,
             static_cast<double>(armour));
  return true;
}

bool decode_player_armed_weapon_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) {
    return false;
  }
  const unsigned int weapon = read_le32(data);
  if (weapon > 255U) {
    trace_netf("rpc-state id=67 invalid_armed_weapon=%u bytes=%u ignored=1", weapon, bytes);
    return false;
  }
  g_rpc_probe.player_armed_weapon = weapon;
  const unsigned int seq = bump_seq(&g_rpc_probe.player_armed_weapon_seq);
  trace_netf("rpc-state id=67 armed_weapon_seq=%u weapon=%u apply_pending=1", seq, weapon);
  return true;
}

bool decode_play_sound_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 16U) {
    return false;
  }
  const unsigned int sound = read_le32(data);
  float pos[3] = {0.0f, 0.0f, 0.0f};
  read_vec3(data + 4U, pos);
  if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2])) {
    trace_netf("rpc-state id=16 invalid_sound_pos sound=%u bytes=%u ignored=1", sound, bytes);
    return false;
  }
  g_rpc_probe.play_sound_id = sound;
  std::memcpy(g_rpc_probe.play_sound_pos, pos, sizeof(g_rpc_probe.play_sound_pos));
  const unsigned int seq = bump_seq(&g_rpc_probe.play_sound_seq);
  trace_netf("rpc-state id=16 play_sound_seq=%u sound=%u pos=%.3f %.3f %.3f apply_pending=1", seq, sound,
             static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]));
  return true;
}

bool decode_player_team_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 3U) {
    return false;
  }
  g_rpc_probe.player_team_player_id = read_le16(data);
  g_rpc_probe.player_team = data[2U];
  const unsigned int seq = bump_seq(&g_rpc_probe.player_team_seq);
  trace_netf("rpc-state id=69 player_team_seq=%u player=%u team=%u decoded_only=1", seq,
             static_cast<unsigned int>(g_rpc_probe.player_team_player_id),
             static_cast<unsigned int>(g_rpc_probe.player_team));
  return true;
}

bool decode_player_color_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 6U) {
    return false;
  }
  g_rpc_probe.player_color_player_id = read_le16(data);
  g_rpc_probe.player_color = read_le32(data + 2U);
  const unsigned int seq = bump_seq(&g_rpc_probe.player_color_seq);
  trace_netf("rpc-state id=72 player_color_seq=%u player=%u color=0x%08x decoded_only=1", seq,
             static_cast<unsigned int>(g_rpc_probe.player_color_player_id), g_rpc_probe.player_color);
  return true;
}

bool decode_apply_animation_payload(const unsigned char *data, unsigned int bytes) {
  unsigned int offset = 0U;
  unsigned int lib_len = 0U;
  unsigned int name_len = 0U;

  if (data == nullptr || bytes < 16U) {
    return false;
  }
  g_rpc_probe.apply_animation_player_id = read_le16(data + offset);
  offset += 2U;
  lib_len = data[offset++];
  if (lib_len > bytes - offset) {
    return false;
  }
  (void)copy_bounded_bytes(g_rpc_probe.apply_animation_lib, sizeof(g_rpc_probe.apply_animation_lib), data + offset,
                           lib_len);
  offset += lib_len;
  if (offset >= bytes) {
    return false;
  }
  name_len = data[offset++];
  if (name_len > bytes - offset) {
    return false;
  }
  (void)copy_bounded_bytes(g_rpc_probe.apply_animation_name, sizeof(g_rpc_probe.apply_animation_name), data + offset,
                           name_len);
  offset += name_len;
  if (bytes - offset < 8U) {
    return false;
  }
  g_rpc_probe.apply_animation_delta = read_le_float(data + offset);
  offset += 4U;
  g_rpc_probe.apply_animation_loop = data[offset++] != 0U ? 1U : 0U;
  g_rpc_probe.apply_animation_lock_x = data[offset++] != 0U ? 1U : 0U;
  g_rpc_probe.apply_animation_lock_y = data[offset++] != 0U ? 1U : 0U;
  g_rpc_probe.apply_animation_freeze = data[offset++] != 0U ? 1U : 0U;
  if (bytes - offset < 4U) {
    return false;
  }
  g_rpc_probe.apply_animation_time = read_le_i32(data + offset);
  const unsigned int seq = bump_seq(&g_rpc_probe.apply_animation_seq);
  trace_netf("rpc-state id=86 apply_animation_seq=%u player=%u lib='%s' name='%s' delta=%.3f flags=%u/%u/%u/%u time=%d decoded_only=1 TODO_VERIFY=1",
             seq, static_cast<unsigned int>(g_rpc_probe.apply_animation_player_id),
             g_rpc_probe.apply_animation_lib, g_rpc_probe.apply_animation_name,
             static_cast<double>(g_rpc_probe.apply_animation_delta),
             static_cast<unsigned int>(g_rpc_probe.apply_animation_loop),
             static_cast<unsigned int>(g_rpc_probe.apply_animation_lock_x),
             static_cast<unsigned int>(g_rpc_probe.apply_animation_lock_y),
             static_cast<unsigned int>(g_rpc_probe.apply_animation_freeze),
             static_cast<int>(g_rpc_probe.apply_animation_time));
  return true;
}

bool decode_set_object_material_payload(const unsigned char *data, unsigned int bytes) {
  unsigned int offset = 0U;
  unsigned short object_id = 0U;
  unsigned char material_type = 0U;
  unsigned char material_id = 0U;
  std::int32_t model = 0;
  unsigned int color = 0U;
  unsigned int background = 0U;
  unsigned char material_size = 0U;
  unsigned char font_size = 0U;
  unsigned char bold = 0U;
  unsigned char alignment = 0U;
  char left[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES] = {0};
  char right[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES] = {0};
  char text[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES] = {0};

  if (data == nullptr || bytes < 4U) {
    return false;
  }
  object_id = read_le16(data + offset);
  offset += 2U;
  material_type = data[offset++];
  material_id = data[offset++];
  if (material_type == 1U) {
    if (bytes - offset < 2U) {
      return false;
    }
    model = static_cast<std::int32_t>(read_le16(data + offset));
    offset += 2U;
    if (!read_dynstr8_plain(data, bytes, &offset, left, sizeof(left)) ||
        !read_dynstr8_plain(data, bytes, &offset, right, sizeof(right)) || bytes - offset < 4U) {
      return false;
    }
    color = read_le32(data + offset);
    std::snprintf(text, sizeof(text), "type=default slot=%u txd='%s' texture='%s'", static_cast<unsigned int>(material_id),
                  left, right);
  } else if (material_type == 2U) {
    if (bytes - offset < 12U) {
      return false;
    }
    material_size = data[offset++];
    if (!read_dynstr8_plain(data, bytes, &offset, right, sizeof(right)) || bytes - offset < 11U) {
      return false;
    }
    font_size = data[offset++];
    bold = data[offset++];
    color = read_le32(data + offset);
    offset += 4U;
    background = read_le32(data + offset);
    offset += 4U;
    alignment = data[offset++];
    std::snprintf(text, sizeof(text), "type=text slot=%u size=%u font='%s' bold=%u bg=0x%08x compressed_text=1",
                  static_cast<unsigned int>(material_id), material_size, right, bold, background);
  } else {
    std::snprintf(text, sizeof(text), "type=%u slot=%u unsupported", static_cast<unsigned int>(material_type),
                  static_cast<unsigned int>(material_id));
  }

  set_object_material_event(object_id, material_type, material_id, model, color, background, material_size,
                            font_size, bold, alignment, left, right, text);
  return true;
}

bool decode_create_3d_text_label_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 27U) {
    return false;
  }
  float pos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const unsigned short label_id = read_le16(data);
  const unsigned int color = read_le32(data + 2U);
  read_vec3(data + 6U, pos);
  pos[3] = read_le_float(data + 18U);
  const unsigned int los = data[22U] != 0U ? 1U : 0U;
  const unsigned int player_attach = read_le16(data + 23U);
  const unsigned int vehicle_attach = read_le16(data + 25U);
  char text[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES] = {0};
  std::snprintf(text, sizeof(text), "draw=%.3f los=%u attach=%u/%u compressed_text=1",
                static_cast<double>(pos[3]), los, player_attach, vehicle_attach);
  set_world_visual_event(SAMP_RAKNET_WORLD_VISUAL_CREATE_3D_TEXT_LABEL, label_id, 0, color, pos, text);
  return true;
}

bool decode_gang_zone_create_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 22U) {
    return false;
  }
  float pos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const unsigned short zone_id = read_le16(data);
  pos[0] = read_le_float(data + 2U);
  pos[1] = read_le_float(data + 6U);
  pos[2] = read_le_float(data + 10U);
  pos[3] = read_le_float(data + 14U);
  const unsigned int color = read_le32(data + 18U);
  if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2]) || !std::isfinite(pos[3])) {
    return false;
  }
  set_world_visual_event(SAMP_RAKNET_WORLD_VISUAL_GANG_ZONE_CREATE, zone_id, 0, color, pos, "gang_zone_create");
  return true;
}

bool decode_remove_building_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 20U) {
    return false;
  }
  float pos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const std::int32_t model = read_le_i32(data);
  read_vec3(data + 4U, pos);
  pos[3] = read_le_float(data + 16U);
  if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2]) || !std::isfinite(pos[3])) {
    return false;
  }
  set_world_visual_event(SAMP_RAKNET_WORLD_VISUAL_REMOVE_BUILDING, 0U, model, 0U, pos,
                         "remove_building_for_player");
  return true;
}

bool decode_set_map_icon_payload(const unsigned char *data, unsigned int bytes) {
  float pos[3] = {0.0f, 0.0f, 0.0f};
  unsigned char index = 0U;
  unsigned char icon = 0U;
  unsigned int color = 0U;

  if (data == nullptr || bytes < 18U) {
    return false;
  }
  index = data[0U];
  if (index >= SAMP_RAKNET_MAP_ICON_MAX) {
    trace_netf("rpc-state id=56 set_map_icon invalid_index=%u bytes=%u ignored=1",
               static_cast<unsigned int>(index), bytes);
    return false;
  }
  read_vec3(data + 1U, pos);
  icon = data[13U];
  color = read_le32(data + 14U);
  if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2])) {
    return false;
  }
  queue_map_icon_event(SAMP_RAKNET_MAP_ICON_ACTION_SET, index, pos, icon, color);
  trace_netf("rpc-state id=56 set_map_icon seq=%u index=%u icon=%u color=0x%08x pos=%.3f %.3f %.3f evidence=INFERRED,TODO_VERIFY",
             g_rpc_probe.map_icon_event_seq, static_cast<unsigned int>(index), static_cast<unsigned int>(icon),
             color, static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]));
  return true;
}

bool decode_remove_map_icon_payload(const unsigned char *data, unsigned int bytes) {
  unsigned char index = 0U;

  if (data == nullptr || bytes < 1U) {
    return false;
  }
  index = data[0U];
  if (index >= SAMP_RAKNET_MAP_ICON_MAX) {
    trace_netf("rpc-state id=144 remove_map_icon invalid_index=%u bytes=%u ignored=1",
               static_cast<unsigned int>(index), bytes);
    return false;
  }
  queue_map_icon_event(SAMP_RAKNET_MAP_ICON_ACTION_REMOVE, index, nullptr, 0U, 0U);
  trace_netf("rpc-state id=144 remove_map_icon seq=%u index=%u evidence=INFERRED,TODO_VERIFY",
             g_rpc_probe.map_icon_event_seq, static_cast<unsigned int>(index));
  return true;
}

bool decode_show_name_tag_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short player_id = 0U;
  unsigned char show = 0U;

  if (data == nullptr || bytes < 3U) {
    return false;
  }
  player_id = read_le16(data);
  show = data[2U] != 0U ? 1U : 0U;
  if (player_id >= SAMP_RAKNET_MAX_PLAYERS) {
    trace_netf("rpc-state id=80 show_name_tag invalid_player=%u show=%u ignored=1",
               static_cast<unsigned int>(player_id), static_cast<unsigned int>(show));
    return false;
  }
  queue_name_tag_event(player_id, show);
  trace_netf("rpc-state id=80 show_name_tag seq=%u player=%u show=%u evidence=INFERRED,TODO_VERIFY",
             g_rpc_probe.name_tag_event_seq, static_cast<unsigned int>(player_id), static_cast<unsigned int>(show));
  return true;
}

void filter_chat_text(char *text) {
  if (text == nullptr) {
    return;
  }
  while (*text != '\0') {
    if (*text > 0 && *text < ' ') {
      *text = ' ';
    }
    ++text;
  }
}

unsigned int queue_client_message_line(unsigned int color, const char *text) {
  unsigned int seq = 0U;
  unsigned int slot = 0U;
  samp_raknet_client_message_probe *message = nullptr;

  if (text == nullptr || text[0] == '\0') {
    return 0U;
  }

  seq = ++g_rpc_probe.client_message_seq;
  slot = (seq - 1U) % SAMP_RAKNET_CLIENT_MESSAGE_RING;
  message = &g_rpc_probe.client_messages[slot];
  std::memset(message, 0, sizeof(*message));
  message->seq = seq;
  message->color = color;
  copy_text(message->text, sizeof(message->text), text);
  filter_chat_text(message->text);
  g_rpc_probe.saw_client_message = 1;
  return seq;
}

const samp_raknet_player_pool_event *find_recent_player_join(unsigned short player_id) {
  const samp_raknet_player_pool_event *best = nullptr;
  unsigned int best_seq = 0U;

  for (unsigned int i = 0U; i < SAMP_RAKNET_PLAYER_POOL_EVENT_RING; ++i) {
    const samp_raknet_player_pool_event *event = &g_rpc_probe.player_pool_events[i];
    if (event->seq > best_seq && event->action == SAMP_RAKNET_PLAYER_POOL_ACTION_JOIN &&
        event->player_id == player_id && event->name[0] != '\0') {
      best = event;
      best_seq = event->seq;
    }
  }
  return best;
}

bool player_id_is_local(unsigned short player_id) {
  if (g_rpc_probe.auth_local_player_id_valid != 0U && g_rpc_probe.auth_local_player_id == player_id) {
    return true;
  }
  return g_rpc_probe.saw_init_game != 0 && g_rpc_probe.init_local_player_id == player_id;
}

void sanitize_textdraw_text(char *text) {
  if (text == nullptr) {
    return;
  }
  while (*text != '\0') {
    const unsigned char ch = static_cast<unsigned char>(*text);
    if (ch < ' ' && *text != '\n' && *text != '\r' && *text != '\t') {
      *text = ' ';
    }
    ++text;
  }
}

void trim_textdraw_text_tail(char *text) {
  size_t len = 0U;

  if (text == nullptr) {
    return;
  }
  len = std::strlen(text);
  while (len > 0U) {
    const unsigned char ch = static_cast<unsigned char>(text[len - 1U]);
    if (ch != '\0' && ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t') {
      break;
    }
    text[len - 1U] = '\0';
    --len;
  }
}

bool textdraw_text_has_printable_content(const char *text) {
  unsigned int printable = 0U;
  unsigned int controls = 0U;
  unsigned int high_bytes = 0U;

  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  while (*text != '\0') {
    const unsigned char ch = static_cast<unsigned char>(*text);
    if (ch >= ' ' || *text == '\n' || *text == '\r' || *text == '\t') {
      ++printable;
      if (ch >= 0x80U) {
        ++high_bytes;
      }
    } else {
      ++controls;
    }
    ++text;
  }
  return printable > 0U && controls == 0U && high_bytes * 3U <= printable * 2U;
}

bool copy_textdraw_plain_tail(const unsigned char *data, unsigned int bytes, unsigned int offset, char *out,
                              size_t out_size) {
  unsigned int available = 0U;
  unsigned int copy_len = 0U;

  if (data == nullptr || out == nullptr || out_size == 0U || offset > bytes) {
    return false;
  }
  out[0] = '\0';
  available = bytes - offset;
  if (available == 0U) {
    return true;
  }

  if (data[offset] > 0U && data[offset] <= available - 1U) {
    copy_len = data[offset];
    if (copy_len >= out_size) {
      copy_len = static_cast<unsigned int>(out_size - 1U);
    }
    std::memcpy(out, data + offset + 1U, copy_len);
    out[copy_len] = '\0';
    sanitize_textdraw_text(out);
    trim_textdraw_text_tail(out);
    if (textdraw_text_has_printable_content(out)) {
      return true;
    }
    out[0] = '\0';
  }

  copy_len = available;
  if (copy_len >= out_size) {
    copy_len = static_cast<unsigned int>(out_size - 1U);
  }
  if (copy_len > 0U) {
    std::memcpy(out, data + offset, copy_len);
  }
  out[copy_len] = '\0';
  sanitize_textdraw_text(out);
  trim_textdraw_text_tail(out);
  return out[0] == '\0' || textdraw_text_has_printable_content(out);
}

bool copy_textdraw_bytes(const unsigned char *data, unsigned int bytes, unsigned int offset, unsigned int text_len,
                         char *out, size_t out_size) {
  unsigned int copy_len = 0U;

  if (data == nullptr || out == nullptr || out_size == 0U || offset > bytes) {
    return false;
  }
  out[0] = '\0';
  if (text_len > bytes - offset) {
    return false;
  }
  copy_len = text_len;
  if (copy_len >= out_size) {
    copy_len = static_cast<unsigned int>(out_size - 1U);
  }
  if (copy_len > 0U) {
    std::memcpy(out, data + offset, copy_len);
  }
  out[copy_len] = '\0';
  sanitize_textdraw_text(out);
  trim_textdraw_text_tail(out);
  return out[0] == '\0' || textdraw_text_has_printable_content(out);
}

bool decode_textdraw_dynstr16_tail(const unsigned char *data, unsigned int bytes, unsigned int length_offset,
                                   char *out, size_t out_size) {
  unsigned int text_len = 0U;

  if (data == nullptr || out == nullptr || out_size == 0U || length_offset + 2U > bytes) {
    return false;
  }
  text_len = read_le16(data + length_offset);
  return copy_textdraw_bytes(data, bytes, length_offset + 2U, text_len, out, out_size);
}

bool decode_textdraw_compressed_tail(const unsigned char *data, unsigned int bytes, unsigned int offset, char *out,
                                     size_t out_size) {
  RakNet::StringCompressor *compressor = RakNet::StringCompressor::Instance();

  if (data == nullptr || out == nullptr || out_size == 0U || offset >= bytes || compressor == nullptr) {
    return false;
  }
  out[0] = '\0';
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
  bs.SetReadOffset(static_cast<int>(offset * 8U));
  if (!compressor->DecodeString(out, static_cast<int>(out_size), &bs)) {
    return false;
  }
  out[out_size - 1U] = '\0';
  sanitize_textdraw_text(out);
  trim_textdraw_text_tail(out);
  return textdraw_text_has_printable_content(out);
}

void decode_textdraw_tail(const unsigned char *data, unsigned int bytes, unsigned int offset, char *out,
                          size_t out_size) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  out[0] = '\0';
  if (decode_textdraw_compressed_tail(data, bytes, offset, out, out_size)) {
    return;
  }
  if (!copy_textdraw_plain_tail(data, bytes, offset, out, out_size)) {
    out[0] = '\0';
  }
}

void queue_textdraw_event(unsigned char action, unsigned short textdraw_id,
                          const samp_raknet_textdraw_transmit *transmit, const char *text);

bool textdraw_openmp_show_payload_plausible(const samp_raknet_textdraw_transmit *transmit, unsigned int bytes,
                                            unsigned int text_len) {
  if (transmit == nullptr || bytes < kOpenMpTextDrawHeaderBytes ||
      kOpenMpTextDrawStringOffset + text_len > bytes) {
    return false;
  }
  return std::isfinite(transmit->letter_width) && std::isfinite(transmit->letter_height) &&
         std::isfinite(transmit->line_width) && std::isfinite(transmit->line_height) &&
         std::isfinite(transmit->x) && std::isfinite(transmit->y) && transmit->letter_width >= -20.0f &&
         transmit->letter_width <= 20.0f && transmit->letter_height >= -20.0f &&
         transmit->letter_height <= 200.0f && transmit->line_width >= -10000.0f &&
         transmit->line_width <= 10000.0f && transmit->line_height >= -10000.0f &&
         transmit->line_height <= 10000.0f && transmit->x >= -5000.0f && transmit->x <= 10000.0f &&
         transmit->y >= -5000.0f && transmit->y <= 10000.0f && transmit->style <= 5U;
}

bool decode_textdraw_openmp_show_payload(const unsigned char *data, unsigned int bytes, unsigned short textdraw_id) {
  samp_raknet_textdraw_transmit transmit;
  char text[SAMP_RAKNET_TEXTDRAW_TEXT_BYTES];
  unsigned int text_len = 0U;

  if (data == nullptr || bytes < kOpenMpTextDrawHeaderBytes) {
    return false;
  }

  std::memset(&transmit, 0, sizeof(transmit));
  transmit.flags = data[2U];
  transmit.letter_width = read_le_float(data + 3U);
  transmit.letter_height = read_le_float(data + 7U);
  transmit.letter_color = read_le32(data + 11U);
  transmit.line_width = read_le_float(data + 15U);
  transmit.line_height = read_le_float(data + 19U);
  transmit.box_color = read_le32(data + 23U);
  transmit.shadow = data[27U];
  transmit.outline = data[28U];
  transmit.background_color = read_le32(data + 29U);
  transmit.style = data[33U];
  transmit.selectable = data[34U];
  transmit.x = read_le_float(data + 35U);
  transmit.y = read_le_float(data + 39U);
  transmit.preview_model = read_le16(data + 43U);
  transmit.preview_rotation[0] = read_le_float(data + 45U);
  transmit.preview_rotation[1] = read_le_float(data + 49U);
  transmit.preview_rotation[2] = read_le_float(data + 53U);
  transmit.preview_zoom = read_le_float(data + 57U);
  transmit.preview_color1 = static_cast<int16_t>(read_le16(data + 61U));
  transmit.preview_color2 = static_cast<int16_t>(read_le16(data + 63U));
  text_len = read_le16(data + kOpenMpTextDrawStringLengthOffset);

  if (!textdraw_openmp_show_payload_plausible(&transmit, bytes, text_len)) {
    return false;
  }

  std::memset(text, 0, sizeof(text));
  if (!copy_textdraw_bytes(data, bytes, kOpenMpTextDrawStringOffset, text_len, text, sizeof(text))) {
    text[0] = '\0';
  }
  queue_textdraw_event(SAMP_RAKNET_TEXTDRAW_ACTION_SHOW, textdraw_id, &transmit, text);
  trace_netf("textdraw-decode: show seq=%u id=%u variant=openmp pos=(%.3f,%.3f) size=(%.3f,%.3f) style=%u flags=0x%02x selectable=%u model=%u zoom=%.3f text_len=%u text='%s'",
             g_rpc_probe.textdraw_event_seq, static_cast<unsigned int>(textdraw_id),
             static_cast<double>(transmit.x), static_cast<double>(transmit.y),
             static_cast<double>(transmit.line_width), static_cast<double>(transmit.line_height),
             static_cast<unsigned int>(transmit.style), static_cast<unsigned int>(transmit.flags),
             static_cast<unsigned int>(transmit.selectable), static_cast<unsigned int>(transmit.preview_model),
             static_cast<double>(transmit.preview_zoom), text_len, text);
  return true;
}

void queue_textdraw_event(unsigned char action, unsigned short textdraw_id,
                          const samp_raknet_textdraw_transmit *transmit, const char *text) {
  ++g_rpc_probe.textdraw_event_seq;
  const unsigned int slot = (g_rpc_probe.textdraw_event_seq - 1U) % SAMP_RAKNET_TEXTDRAW_EVENT_RING;
  samp_raknet_textdraw_event *event = &g_rpc_probe.textdraw_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = g_rpc_probe.textdraw_event_seq;
  event->action = action;
  event->textdraw_id = textdraw_id;
  if (transmit != nullptr) {
    std::memcpy(&event->transmit, transmit, sizeof(event->transmit));
  }
  if (text != nullptr) {
    std::memcpy(event->text, text, SAMP_RAKNET_TEXTDRAW_TEXT_BYTES - 1U);
    event->text[SAMP_RAKNET_TEXTDRAW_TEXT_BYTES - 1U] = '\0';
    sanitize_textdraw_text(event->text);
  }
}

bool decode_textdraw_show_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short textdraw_id = 0U;
  samp_raknet_textdraw_transmit transmit;
  char text[SAMP_RAKNET_TEXTDRAW_TEXT_BYTES];

  if (data == nullptr || bytes < kTextDrawShowMinBytes) {
    return false;
  }
  textdraw_id = read_le16(data);
  if (textdraw_id >= SAMP_RAKNET_MAX_TEXTDRAWS) {
    trace_netf("textdraw-decode: show invalid id=%u bytes=%u", static_cast<unsigned int>(textdraw_id), bytes);
    return false;
  }

  if (decode_textdraw_openmp_show_payload(data, bytes, textdraw_id)) {
    return true;
  }

  std::memset(&transmit, 0, sizeof(transmit));
  std::memcpy(&transmit, data + 2U, kCompactTextDrawTransmitBytes);
  std::memset(text, 0, sizeof(text));
  decode_textdraw_tail(data, bytes, 2U + kCompactTextDrawTransmitBytes, text, sizeof(text));
  queue_textdraw_event(SAMP_RAKNET_TEXTDRAW_ACTION_SHOW, textdraw_id, &transmit, text);
  trace_netf("textdraw-decode: show seq=%u id=%u variant=compact pos=(%.3f,%.3f) style=%u flags=0x%02x tail=%u text='%s'",
             g_rpc_probe.textdraw_event_seq, static_cast<unsigned int>(textdraw_id),
             static_cast<double>(transmit.x), static_cast<double>(transmit.y),
             static_cast<unsigned int>(transmit.style), static_cast<unsigned int>(transmit.flags),
             bytes - (2U + kCompactTextDrawTransmitBytes), text);
  return true;
}

bool decode_textdraw_hide_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short textdraw_id = 0U;

  if (data == nullptr || bytes < 2U) {
    return false;
  }
  textdraw_id = read_le16(data);
  if (textdraw_id >= SAMP_RAKNET_MAX_TEXTDRAWS) {
    trace_netf("textdraw-decode: hide invalid id=%u bytes=%u", static_cast<unsigned int>(textdraw_id), bytes);
    return false;
  }
  queue_textdraw_event(SAMP_RAKNET_TEXTDRAW_ACTION_HIDE, textdraw_id, nullptr, nullptr);
  trace_netf("textdraw-decode: hide seq=%u id=%u", g_rpc_probe.textdraw_event_seq,
             static_cast<unsigned int>(textdraw_id));
  return true;
}

bool decode_textdraw_edit_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short textdraw_id = 0U;
  char text[SAMP_RAKNET_TEXTDRAW_TEXT_BYTES];

  if (data == nullptr || bytes < kTextDrawEditMinBytes) {
    return false;
  }
  textdraw_id = read_le16(data);
  if (textdraw_id >= SAMP_RAKNET_MAX_TEXTDRAWS) {
    trace_netf("textdraw-decode: edit invalid id=%u bytes=%u", static_cast<unsigned int>(textdraw_id), bytes);
    return false;
  }
  std::memset(text, 0, sizeof(text));
  if (!decode_textdraw_dynstr16_tail(data, bytes, 2U, text, sizeof(text))) {
    decode_textdraw_tail(data, bytes, 2U, text, sizeof(text));
  }
  queue_textdraw_event(SAMP_RAKNET_TEXTDRAW_ACTION_EDIT, textdraw_id, nullptr, text);
  trace_netf("textdraw-decode: edit seq=%u id=%u tail=%u text='%s'", g_rpc_probe.textdraw_event_seq,
             static_cast<unsigned int>(textdraw_id), bytes - 2U, text);
  return true;
}

bool read_dialog_plain_field(const unsigned char *data, unsigned int bytes, unsigned int *offset, char *out,
                             size_t out_size) {
  unsigned int len = 0U;
  unsigned int copy_len = 0U;

  if (data == nullptr || offset == nullptr || out == nullptr || out_size == 0U || *offset >= bytes) {
    return false;
  }
  out[0] = '\0';
  len = data[*offset];
  ++(*offset);
  if (len > bytes - *offset) {
    return false;
  }

  copy_len = len;
  if (copy_len >= out_size) {
    copy_len = static_cast<unsigned int>(out_size - 1U);
  }
  if (copy_len > 0U) {
    std::memcpy(out, data + *offset, copy_len);
  }
  out[copy_len] = '\0';
  *offset += len;
  return true;
}

void sanitize_dialog_text(char *text) {
  if (text == nullptr) {
    return;
  }
  while (*text != '\0') {
    const unsigned char ch = static_cast<unsigned char>(*text);
    if (ch < ' ' && *text != '\n' && *text != '\r' && *text != '\t') {
      *text = ' ';
    }
    ++text;
  }
}

void copy_dialog_remaining_info(const unsigned char *data, unsigned int bytes, unsigned int offset) {
  unsigned int copy_len = 0U;

  g_rpc_probe.dialog_info[0] = '\0';
  if (data == nullptr || offset >= bytes) {
    return;
  }

  copy_len = bytes - offset;
  while (copy_len > 0U && data[offset + copy_len - 1U] == '\0') {
    --copy_len;
  }
  if (copy_len >= sizeof(g_rpc_probe.dialog_info)) {
    copy_len = static_cast<unsigned int>(sizeof(g_rpc_probe.dialog_info) - 1U);
  }
  if (copy_len > 0U) {
    std::memcpy(g_rpc_probe.dialog_info, data + offset, copy_len);
  }
  g_rpc_probe.dialog_info[copy_len] = '\0';
  sanitize_dialog_text(g_rpc_probe.dialog_info);
}

bool dialog_text_has_printable_content(const char *text) {
  unsigned int printable = 0U;
  unsigned int controls = 0U;
  unsigned int high_bytes = 0U;

  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  while (*text != '\0') {
    const unsigned char ch = static_cast<unsigned char>(*text);
    if (ch >= ' ' || *text == '\n' || *text == '\r' || *text == '\t') {
      ++printable;
      if (ch >= 0x80U) {
        ++high_bytes;
      }
    } else {
      ++controls;
    }
    ++text;
  }
  return printable > 0U && controls == 0U && high_bytes * 4U <= printable;
}

bool decode_dialog_compressed_info_tail(const unsigned char *data, unsigned int bytes, unsigned int offset) {
  char info[kDialogInfoBytes];
  RakNet::StringCompressor *compressor = RakNet::StringCompressor::Instance();

  if (data == nullptr || offset >= bytes || compressor == nullptr) {
    return false;
  }

  std::memset(info, 0, sizeof(info));
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
  bs.SetReadOffset(static_cast<int>(offset * 8U));
  if (!compressor->DecodeString(info, static_cast<int>(sizeof(info)), &bs)) {
    return false;
  }
  sanitize_dialog_text(info);
  if (!dialog_text_has_printable_content(info)) {
    return false;
  }

  copy_text(g_rpc_probe.dialog_info, sizeof(g_rpc_probe.dialog_info), info);
  return true;
}

bool decode_dialog_payload_plain(const unsigned char *data, unsigned int bytes) {
  unsigned int offset = 0U;

  if (data == nullptr || bytes < 4U) {
    return false;
  }

  g_rpc_probe.last_dialog_id = read_le16(data);
  g_rpc_probe.last_dialog_style = data[2U];
  offset = 3U;

  if (!read_dialog_plain_field(data, bytes, &offset, g_rpc_probe.dialog_title, sizeof(g_rpc_probe.dialog_title))) {
    return false;
  }
  if (!read_dialog_plain_field(data, bytes, &offset, g_rpc_probe.dialog_button1, sizeof(g_rpc_probe.dialog_button1))) {
    trace_netf("dialog-decode: plain missing-button1 dialog=%u style=%u offset=%u bytes=%u",
               static_cast<unsigned int>(g_rpc_probe.last_dialog_id),
               static_cast<unsigned int>(g_rpc_probe.last_dialog_style), offset, bytes);
    return true;
  }
  if (!read_dialog_plain_field(data, bytes, &offset, g_rpc_probe.dialog_button2, sizeof(g_rpc_probe.dialog_button2))) {
    trace_netf("dialog-decode: plain missing-button2 dialog=%u style=%u offset=%u bytes=%u",
               static_cast<unsigned int>(g_rpc_probe.last_dialog_id),
               static_cast<unsigned int>(g_rpc_probe.last_dialog_style), offset, bytes);
    g_rpc_probe.dialog_button2[0] = '\0';
  }

  if (!decode_dialog_compressed_info_tail(data, bytes, offset)) {
    copy_dialog_remaining_info(data, bytes, offset);
    if (!dialog_text_has_printable_content(g_rpc_probe.dialog_info)) {
      g_rpc_probe.dialog_info[0] = '\0';
    }
  }
  sanitize_dialog_text(g_rpc_probe.dialog_title);
  sanitize_dialog_text(g_rpc_probe.dialog_button1);
  sanitize_dialog_text(g_rpc_probe.dialog_button2);

  if (g_rpc_probe.dialog_title[0] == '\0') {
    copy_text(g_rpc_probe.dialog_title, sizeof(g_rpc_probe.dialog_title), "SA-MP Dialog");
  }
  if (g_rpc_probe.dialog_button1[0] == '\0') {
    copy_text(g_rpc_probe.dialog_button1, sizeof(g_rpc_probe.dialog_button1), "OK");
  }
  if (g_rpc_probe.dialog_info[0] == '\0' && std::strstr(g_rpc_probe.dialog_title, "language") != nullptr) {
    copy_text(g_rpc_probe.dialog_info, sizeof(g_rpc_probe.dialog_info), "English\nSpanish\nPortuguese");
  }

  trace_netf("dialog-decode: plain-rest dialog=%u style=%u title='%s' info_len=%u button1='%s' button2='%s' offset=%u bytes=%u",
             static_cast<unsigned int>(g_rpc_probe.last_dialog_id),
             static_cast<unsigned int>(g_rpc_probe.last_dialog_style), g_rpc_probe.dialog_title,
             static_cast<unsigned int>(std::strlen(g_rpc_probe.dialog_info)), g_rpc_probe.dialog_button1,
             g_rpc_probe.dialog_button2, offset, bytes);
  return true;
}

void decode_client_message_payload(const unsigned char *data, unsigned int bytes) {
  unsigned int len = 0U;
  unsigned int copy_len = 0U;
  unsigned int seq = 0U;
  unsigned int color = 0U;
  char text[SAMP_RAKNET_CLIENT_MESSAGE_BYTES] = {0};

  if (data == nullptr || bytes < 8U) {
    return;
  }

  len = read_le32(data + 4U);
  if (len > bytes - 8U) {
    trace_netf("rpc-state id=93 decode failed len=%u bytes=%u", len, bytes);
    return;
  }

  color = read_le32(data);
  copy_len = len;
  if (copy_len >= SAMP_RAKNET_CLIENT_MESSAGE_BYTES) {
    copy_len = SAMP_RAKNET_CLIENT_MESSAGE_BYTES - 1U;
  }
  if (copy_len > 0U) {
    std::memcpy(text, data + 8U, copy_len);
  }
  text[copy_len] = '\0';
  filter_chat_text(text);
  seq = queue_client_message_line(color, text);

  trace_netf("rpc-state id=93 client_message seq=%u color=0x%08x len=%u text='%s'", seq, color, len, text);
}

bool decode_chat_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short player_id = 0U;
  unsigned char text_len = 0U;
  unsigned int color = 0xFFFFFFFFU;
  unsigned int seq = 0U;
  bool local_player = false;
  char text[SAMP_RAKNET_CLIENT_MESSAGE_BYTES] = {0};
  char name[SAMP_RAKNET_PLAYER_NAME_BYTES] = {0};
  char line[SAMP_RAKNET_CLIENT_MESSAGE_BYTES] = {0};
  const samp_raknet_player_pool_event *join_event = nullptr;

  if (data == nullptr || bytes < 3U) {
    return false;
  }

  /*
   * INFERRED + OPENMP_REF + TODO_VERIFY:
   * SA-MP 0.3.7-compatible chat RPC layout is uint16 player id, uint8 text length,
   * then raw text. The client displays local chat only after this
   * server-originated RPC, allowing scripts to filter outgoing player text.
   */
  player_id = read_le16(data);
  text_len = data[2U];
  if (static_cast<unsigned int>(text_len) > bytes - 3U) {
    trace_netf("rpc-state id=101 chat decode_failed player=%u len=%u bytes=%u",
               static_cast<unsigned int>(player_id), static_cast<unsigned int>(text_len), bytes);
    return false;
  }

  (void)copy_bounded_bytes(text, sizeof(text), data + 3U, text_len);
  filter_chat_text(text);

  local_player = player_id_is_local(player_id);
  if (local_player) {
    copy_text(name, sizeof(name), g_rpc_probe.local_nickname[0] != '\0' ? g_rpc_probe.local_nickname : kDefaultNickname);
    if (g_rpc_probe.player_color_seq > 0U && g_rpc_probe.player_color_player_id == player_id) {
      color = g_rpc_probe.player_color;
    }
  } else {
    join_event = find_recent_player_join(player_id);
    if (join_event != nullptr) {
      copy_text(name, sizeof(name), join_event->name);
      color = join_event->color;
    } else {
      std::snprintf(name, sizeof(name), "Player%u", static_cast<unsigned int>(player_id));
    }
  }
  filter_chat_text(name);
  if (name[0] == '\0') {
    copy_text(name, sizeof(name), kDefaultNickname);
  }

  std::snprintf(line, sizeof(line), "%s: %s", name, text);
  seq = queue_client_message_line(color, line);
  trace_netf("rpc-state id=101 chat seq=%u player=%u local=%u color=0x%08x len=%u text='%s'",
             seq, static_cast<unsigned int>(player_id), local_player ? 1U : 0U, color,
             static_cast<unsigned int>(text_len), text);
  return true;
}

bool decode_textdraw_select_payload(const unsigned char *data, unsigned int bytes, unsigned int bits,
                                    unsigned char *out_enabled, unsigned int *out_color) {
  bool enabled = false;
  unsigned int color = 0U;

  if (out_enabled == nullptr || out_color == nullptr) {
    return false;
  }
  *out_enabled = 0U;
  *out_color = 0U;

  if (data == nullptr || bytes == 0U || bits == 0U) {
    return false;
  }

  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
  if (!bs.Read(enabled)) {
    return false;
  }

  if (enabled) {
    /*
     * PROBE_TRACE + INFERRED:
     * 0.3.7/open.mp SelectTextDraw arrives as 33 bits: bool enabled + DWORD hoverColor.
     * A follow-up all-zero payload disables selection and must not be treated as a black hover color.
     */
    color = 0xFFFFFFFFU;
    if (bits >= 33U && !bs.Read(color)) {
      color = 0xFFFFFFFFU;
    }
  }

  *out_enabled = enabled ? 1U : 0U;
  *out_color = enabled ? color : 0U;
  return true;
}

bool decode_dialog_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short dialog_id = 0;
  unsigned char style = 0;
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
  RakNet::StringCompressor *compressor = RakNet::StringCompressor::Instance();

  g_rpc_probe.dialog_title[0] = '\0';
  g_rpc_probe.dialog_info[0] = '\0';
  copy_text(g_rpc_probe.dialog_button1, sizeof(g_rpc_probe.dialog_button1), "OK");
  g_rpc_probe.dialog_button2[0] = '\0';

  if (data == nullptr || bytes < 3U) {
    return false;
  }
  if (decode_dialog_payload_plain(data, bytes)) {
    return true;
  }
  if (compressor == nullptr) {
    return false;
  }
  if (!bs.Read(dialog_id) || !bs.Read(style)) {
    return false;
  }

  g_rpc_probe.last_dialog_id = dialog_id;
  g_rpc_probe.last_dialog_style = style;

  if (!compressor->DecodeString(g_rpc_probe.dialog_title, static_cast<int>(sizeof(g_rpc_probe.dialog_title)), &bs)) {
    return false;
  }
  (void)compressor->DecodeString(g_rpc_probe.dialog_button1, static_cast<int>(sizeof(g_rpc_probe.dialog_button1)), &bs);
  (void)compressor->DecodeString(g_rpc_probe.dialog_button2, static_cast<int>(sizeof(g_rpc_probe.dialog_button2)), &bs);
  (void)compressor->DecodeString(g_rpc_probe.dialog_info, static_cast<int>(sizeof(g_rpc_probe.dialog_info)), &bs);

  if (g_rpc_probe.dialog_button1[0] == '\0') {
    copy_text(g_rpc_probe.dialog_button1, sizeof(g_rpc_probe.dialog_button1), "OK");
  }
  if (g_rpc_probe.dialog_title[0] == '\0') {
    copy_text(g_rpc_probe.dialog_title, sizeof(g_rpc_probe.dialog_title), "SA-MP Dialog");
  }
  if (g_rpc_probe.dialog_info[0] == '\0' && std::strstr(g_rpc_probe.dialog_title, "language") != nullptr) {
    copy_text(g_rpc_probe.dialog_info, sizeof(g_rpc_probe.dialog_info), "English\nSpanish\nPortuguese");
  }
  return true;
}

void queue_dialog_response(unsigned short dialog_id, unsigned char button, std::int16_t listitem, const char *input) {
  g_rpc_probe.dialog_response_id = dialog_id;
  g_rpc_probe.dialog_response_button = button;
  g_rpc_probe.dialog_response_listitem = listitem;
  copy_text(g_rpc_probe.dialog_response_input, sizeof(g_rpc_probe.dialog_response_input), input != nullptr ? input : "");
  g_rpc_probe.pending_dialog_response = 1;
  trace_netf("dialog-ui: queued DialogResponse dialog=%u response=%u listitem=%d input='%s'",
             static_cast<unsigned int>(dialog_id), static_cast<unsigned int>(button), static_cast<int>(listitem),
             g_rpc_probe.dialog_response_input);
}

#ifdef _WIN32
struct ManualDialogContext {
  unsigned short dialog_id;
  unsigned char style;
  char title[kDialogTitleBytes];
  char info[kDialogInfoBytes];
  char button1[kDialogButtonBytes];
  char button2[kDialogButtonBytes];
  HWND list;
  HWND edit;
};

constexpr int kDialogListControl = 1001;
constexpr int kDialogEditControl = 1002;
constexpr int kDialogOkControl = IDOK;
constexpr int kDialogCancelControl = IDCANCEL;

bool dialog_uses_list(unsigned char style, const char *info) {
  return style == 2U || style == 4U || style == 5U || (info != nullptr && std::strchr(info, '\n') != nullptr);
}

void add_dialog_list_lines(HWND list, const char *info) {
  char copy[kDialogInfoBytes];
  char *line = nullptr;
  char *next = nullptr;

  if (list == nullptr) {
    return;
  }

  copy_text(copy, sizeof(copy), info != nullptr ? info : "");
  line = std::strtok(copy, "\n");
  while (line != nullptr) {
    size_t len = std::strlen(line);
    while (len > 0U && (line[len - 1U] == '\r' || line[len - 1U] == '\n')) {
      line[--len] = '\0';
    }
    if (line[0] != '\0') {
      SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line));
    }
    next = std::strtok(nullptr, "\n");
    line = next;
  }
  if (SendMessageA(list, LB_GETCOUNT, 0, 0) == 0) {
    SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>("English"));
    SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Spanish"));
    SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Portuguese"));
  }
}

void queue_dialog_response_from_window(ManualDialogContext *ctx, HWND hwnd, unsigned char button) {
  char input[kDialogInputBytes] = {0};
  std::int16_t listitem = -1;

  if (ctx == nullptr) {
    return;
  }

  if (button != 0U && ctx->list != nullptr) {
    LRESULT sel = SendMessageA(ctx->list, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
      trace_netf("dialog-ui: OK ignored for dialog=%u because no list item is selected",
                 static_cast<unsigned int>(ctx->dialog_id));
      return;
    }
    listitem = static_cast<std::int16_t>(sel);
    if (SendMessageA(ctx->list, LB_GETTEXT, static_cast<WPARAM>(sel), reinterpret_cast<LPARAM>(input)) == LB_ERR) {
      input[0] = '\0';
    }
  } else if (button != 0U && ctx->edit != nullptr) {
    GetWindowTextA(ctx->edit, input, static_cast<int>(sizeof(input)));
    listitem = -1;
  }

  queue_dialog_response(ctx->dialog_id, button, listitem, input);
  DestroyWindow(hwnd);
}

LRESULT CALLBACK manual_dialog_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  ManualDialogContext *ctx = reinterpret_cast<ManualDialogContext *>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_CREATE: {
      CREATESTRUCTA *cs = reinterpret_cast<CREATESTRUCTA *>(lparam);
      ctx = reinterpret_cast<ManualDialogContext *>(cs->lpCreateParams);
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));

      CreateWindowExA(0, "STATIC", ctx->title, WS_CHILD | WS_VISIBLE, 12, 10, 420, 20, hwnd, nullptr, nullptr, nullptr);
      if (dialog_uses_list(ctx->style, ctx->info)) {
        ctx->list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", nullptr,
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 12, 38, 420, 185, hwnd,
                                    reinterpret_cast<HMENU>(kDialogListControl), nullptr, nullptr);
        add_dialog_list_lines(ctx->list, ctx->info);
      } else {
        ctx->edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", ctx->info,
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 12, 38, 420, 185,
                                    hwnd, reinterpret_cast<HMENU>(kDialogEditControl), nullptr, nullptr);
      }

      CreateWindowExA(0, "BUTTON", ctx->button1[0] != '\0' ? ctx->button1 : "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      246, 236, 88, 28, hwnd, reinterpret_cast<HMENU>(kDialogOkControl), nullptr, nullptr);
      if (ctx->button2[0] != '\0') {
        CreateWindowExA(0, "BUTTON", ctx->button2, WS_CHILD | WS_VISIBLE, 344, 236, 88, 28, hwnd,
                        reinterpret_cast<HMENU>(kDialogCancelControl), nullptr, nullptr);
      }
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wparam) == kDialogOkControl) {
        queue_dialog_response_from_window(ctx, hwnd, 1U);
        return 0;
      }
      if (LOWORD(wparam) == kDialogCancelControl) {
        queue_dialog_response_from_window(ctx, hwnd, 0U);
        return 0;
      }
      break;
    case WM_CLOSE:
      queue_dialog_response_from_window(ctx, hwnd, 0U);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcA(hwnd, msg, wparam, lparam);
}

DWORD WINAPI manual_dialog_thread_proc(LPVOID param) {
  ManualDialogContext *ctx = reinterpret_cast<ManualDialogContext *>(param);
  WNDCLASSA wc;
  HWND hwnd = nullptr;
  MSG msg;

  std::memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = manual_dialog_wndproc;
  wc.hInstance = GetModuleHandleA(nullptr);
  wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = "SampDllManualDialog";
  RegisterClassA(&wc);

  hwnd = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, ctx->title,
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 456, 310, nullptr,
                         nullptr, wc.hInstance, ctx);
  if (hwnd != nullptr) {
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
  }

  g_rpc_probe.dialog_window_active = 0;
  delete ctx;
  return 0;
}

void show_manual_dialog_window(void) {
  ManualDialogContext *ctx = nullptr;
  HANDLE thread = nullptr;

  if (g_rpc_probe.dialog_window_active != 0) {
    trace_netf("dialog-ui: window already active, keeping dialog=%u pending",
               static_cast<unsigned int>(g_rpc_probe.last_dialog_id));
    return;
  }

  ctx = new ManualDialogContext();
  std::memset(ctx, 0, sizeof(*ctx));
  ctx->dialog_id = g_rpc_probe.last_dialog_id;
  ctx->style = g_rpc_probe.last_dialog_style;
  copy_text(ctx->title, sizeof(ctx->title), g_rpc_probe.dialog_title);
  copy_text(ctx->info, sizeof(ctx->info), g_rpc_probe.dialog_info);
  copy_text(ctx->button1, sizeof(ctx->button1), g_rpc_probe.dialog_button1);
  copy_text(ctx->button2, sizeof(ctx->button2), g_rpc_probe.dialog_button2);

  g_rpc_probe.dialog_window_active = 1;
  thread = CreateThread(nullptr, 0, manual_dialog_thread_proc, ctx, 0, nullptr);
  if (thread != nullptr) {
    CloseHandle(thread);
    trace_netf("dialog-ui: opened manual dialog window id=%u style=%u title='%s'",
               static_cast<unsigned int>(ctx->dialog_id), static_cast<unsigned int>(ctx->style), ctx->title);
  } else {
    g_rpc_probe.dialog_window_active = 0;
    delete ctx;
    trace_netf("dialog-ui: FAILED to create manual dialog window id=%u",
               static_cast<unsigned int>(g_rpc_probe.last_dialog_id));
  }
}
#else
void show_manual_dialog_window(void) {
  trace_netf("dialog-ui: manual window unavailable on this build id=%u title='%s'",
             static_cast<unsigned int>(g_rpc_probe.last_dialog_id), g_rpc_probe.dialog_title);
}
#endif

int send_spawn_notification_with_seq(RakNet::RakClientInterface *rak_client, unsigned int spawn_info_seq) {
  RakNet::BitStream bs_send;

  if (rak_client == nullptr) {
    return -1;
  }
  if (spawn_info_seq == 0U && g_rpc_probe.sent_spawn_notify) {
    return 0;
  }
  if (spawn_info_seq != 0U && g_rpc_probe.sent_spawn_notify_seq == spawn_info_seq) {
    return 0;
  }

  const int sent = rak_client->RPC(kRpcSpawn, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE_SEQUENCED, 0, false,
                                  RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                       ? 1
                       : 0;
  if (sent != 0) {
    g_rpc_probe.sent_spawn_notify = 1;
    g_rpc_probe.sent_spawn_notify_seq = spawn_info_seq;
  }
  trace_netf("rpc-auto-out id=52 name=SpawnNotify seq=%u sent=%d", spawn_info_seq, sent);
  return sent != 0 ? 0 : -2;
}

void send_spawn_notification(RakNet::RakClientInterface *rak_client) {
  (void)send_spawn_notification_with_seq(rak_client, 0U);
}

void service_rpc_probe_actions(RakNet::RakClientInterface *rak_client) {
  if (rak_client == nullptr) {
    return;
  }

  if (g_rpc_probe.pending_dialog_response) {
    RakNet::BitStream bs_send;
    const unsigned char input_len =
        static_cast<unsigned char>(std::strlen(g_rpc_probe.dialog_response_input) < (kDialogInputBytes - 1U)
                                       ? std::strlen(g_rpc_probe.dialog_response_input)
                                       : (kDialogInputBytes - 1U));
    const unsigned short listitem = static_cast<unsigned short>(g_rpc_probe.dialog_response_listitem);

    bs_send.Write(g_rpc_probe.dialog_response_id);
    bs_send.Write(g_rpc_probe.dialog_response_button);
    bs_send.Write(listitem);
    bs_send.Write(input_len);
    if (input_len > 0U) {
      bs_send.Write(g_rpc_probe.dialog_response_input, input_len);
    }

    const int sent = rak_client->RPC(kRpcDialogResponse, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                    RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                         ? 1
                         : 0;
    g_rpc_probe.pending_dialog_response = 0;
    g_rpc_probe.sent_dialog_response = sent;
    if (sent) {
      g_rpc_probe.saw_dialog = 0;
      if (g_rpc_probe.pending_request_spawn) {
        g_rpc_probe.next_request_spawn_time = RakNet::GetTime() + kPostDialogSpawnDelayMs;
        trace_netf("rpc-state RequestSpawn rescheduled reason=dialog_response_pending delay_ms=%u",
                   static_cast<unsigned int>(kPostDialogSpawnDelayMs));
      } else {
        schedule_request_spawn_if_ready("dialog_response", kPostDialogSpawnDelayMs);
      }
    }
    trace_netf("rpc-manual-out id=62 name=DialogResponse dialog=%u response=%u listitem=%d input='%s' sent=%d",
               static_cast<unsigned int>(g_rpc_probe.dialog_response_id),
               static_cast<unsigned int>(g_rpc_probe.dialog_response_button),
               static_cast<int>(g_rpc_probe.dialog_response_listitem), g_rpc_probe.dialog_response_input, sent);
  }

  if (g_rpc_probe.pending_request_class && !g_rpc_probe.waiting_for_request_class_reply) {
    RakNet::BitStream bs_send;
    const int selected_class = clamp_class_id(g_rpc_probe.pending_request_class_id);
    bs_send.Write(selected_class);
    const int sent = rak_client->RPC(kRpcRequestClass, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                     RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                         ? 1
                         : 0;
    if (sent) {
      g_rpc_probe.sent_request_class = 1;
      g_rpc_probe.pending_request_class = 0;
      g_rpc_probe.waiting_for_request_class_reply = 1;
      g_rpc_probe.selected_class = selected_class;
      g_rpc_probe.last_request_class_time = RakNet::GetTime();
    }
    trace_netf("rpc-class-out id=128 name=RequestClass selected_class=%d sent=%d evidence=INFERRED,OPENMP_REF",
               selected_class, sent);
  }

#ifdef _WIN32
  if (class_selection_manual_ready()) {
    const RakNet::RakNetTime now = RakNet::GetTime();
    const int shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const int left_down = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
    const int right_down = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;
    const RakNet::RakNetTime class_elapsed =
        now >= g_rpc_probe.last_request_class_time ? now - g_rpc_probe.last_request_class_time : 0U;

    if (shift_down && !g_rpc_probe.manual_spawn_shift_down) {
      g_rpc_probe.pending_request_spawn = 1;
      g_rpc_probe.next_request_spawn_time = 0;
      trace_netf("rpc-manual: scheduled RequestSpawn from SHIFT key");
    } else if ((left_down || right_down) && class_elapsed >= kClassSelectionReselectDelayMs) {
      const int next_class = clamp_class_id(g_rpc_probe.selected_class + (left_down ? -1 : 1));
      /*
       * INFERRED + OPENMP_REF + TODO_VERIFY:
       * Compatibility CLocalPlayer::ProcessClassSelection repeats LEFT/RIGHT every 250 ms and sends
       * RequestClass(selected_class); SHIFT is the only spawn confirmation key in that path.
       */
      schedule_request_class(next_class, left_down ? "manual_left" : "manual_right");
    }
    g_rpc_probe.manual_spawn_shift_down = shift_down;
  }
#endif

  if (g_rpc_probe.pending_select_mode_freeroam_click) {
    RakNet::RakNetTime now = RakNet::GetTime();
    if (g_rpc_probe.saw_dialog) {
      return;
    }
    if (g_rpc_probe.select_mode_click_ready_time != 0 && now < g_rpc_probe.select_mode_click_ready_time) {
      return;
    }

    RakNet::BitStream bs_send;
    const unsigned short clicked_textdraw = kDefaultFreeroamTextDrawId;
    bs_send.Write(clicked_textdraw);
    const int sent = rak_client->RPC(kRpcClickTextDraw, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                    RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                         ? 1
                         : 0;
    g_rpc_probe.pending_select_mode_freeroam_click = 0;
    g_rpc_probe.select_mode_click_ready_time = 0;
    if (sent) {
      g_rpc_probe.sent_select_mode_freeroam_click = 1;
      g_rpc_probe.textdraw_select_active = 0U;
      g_rpc_probe.pending_request_spawn = 0;
      g_rpc_probe.next_request_spawn_time = 0;
      g_rpc_probe.request_spawn_retry_count = kMaxSpawnRetries;
    }
    trace_netf("rpc-auto-out id=83 name=ClickTextDraw textdraw=%u purpose=select_freeroam sent=%d",
               static_cast<unsigned int>(clicked_textdraw), sent);
  }

  if (g_rpc_probe.pending_request_spawn) {
    RakNet::RakNetTime now = RakNet::GetTime();
    if (g_rpc_probe.saw_dialog) {
      return;
    }
    if (g_rpc_probe.next_request_spawn_time != 0 && now < g_rpc_probe.next_request_spawn_time) {
      return;
    }
    RakNet::BitStream bs_send;
    const int sent = rak_client->RPC(kRpcRequestSpawn, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                    RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                         ? 1
                         : 0;
    ++g_rpc_probe.request_spawn_send_count;
    g_rpc_probe.pending_request_spawn = 0;
    g_rpc_probe.next_request_spawn_time = 0;
    trace_netf("rpc-auto-out id=129 name=RequestSpawn attempt=%u retry=%u sent=%d",
               g_rpc_probe.request_spawn_send_count, g_rpc_probe.request_spawn_retry_count, sent);
  }

  if (g_rpc_probe.saw_init_game) {
    RakNet::RakNetTime now = RakNet::GetTime();
    if (g_rpc_probe.next_score_ping_update_time == 0U || now >= g_rpc_probe.next_score_ping_update_time) {
      RakNet::BitStream bs_send;
      const int sent = rak_client->RPC(kRpcUpdateScoresPingsIPs, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0,
                                       false, RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                           ? 1
                           : 0;
      g_rpc_probe.next_score_ping_update_time = now + kScorePingUpdateMs;
      trace_netf("rpc-auto-out id=155 name=UpdateScoresPingsIPs sent=%d interval_ms=%u evidence=STATIC_037",
                 sent, static_cast<unsigned int>(kScorePingUpdateMs));
    }
  }
}

int send_chat_rpc_internal(RakNet::RakClientInterface *rak_client, const char *text, int server_command) {
  RakNet::BitStream bs_send;
  std::size_t text_len = 0;
  int sent = 0;

  if (rak_client == nullptr || text == nullptr || text[0] == '\0') {
    return -1;
  }

  text_len = std::strlen(text);
  if (text_len > kChatInputBytes) {
    text_len = kChatInputBytes;
  }
  if (text_len == 0U) {
    return -1;
  }

  if (server_command) {
    const std::int32_t len32 = static_cast<std::int32_t>(text_len);
    bs_send.Write(len32);
    bs_send.Write(text, static_cast<int>(text_len));
    sent = rak_client->RPC(kRpcServerCommand, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                           RakNet::UNASSIGNED_NETWORK_ID, nullptr)
               ? 1
               : 0;
    trace_netf("rpc-user-out id=50 name=ServerCommand len=%d sent=%d text='%.*s'", static_cast<int>(text_len), sent,
               static_cast<int>(text_len), text);
  } else {
    const unsigned char len8 = static_cast<unsigned char>(text_len);
    bs_send.Write(len8);
    bs_send.Write(text, static_cast<int>(text_len));
    sent = rak_client->RPC(kRpcChat, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                           RakNet::UNASSIGNED_NETWORK_ID, nullptr)
               ? 1
               : 0;
    trace_netf("rpc-user-out id=101 name=Chat len=%u sent=%d text='%.*s'", static_cast<unsigned int>(len8), sent,
               static_cast<int>(text_len), text);
  }

  return sent ? 0 : -2;
}

void rpc_observer(RakNet::RPCParameters *rpc_params, void *extra) {
  unsigned int rpc_id = 0;
  unsigned int bytes = 0;
  unsigned int prefix_bytes = 0;
  char prefix[kRpcTraceMaxBytes * 3U + 1U] = {0};

  if (extra != nullptr) {
    rpc_id = static_cast<unsigned int>(*static_cast<unsigned char *>(extra));
  }

  if (rpc_id < 256U) {
    ++g_rpc_probe.counts[rpc_id];
  }

  if (rpc_params != nullptr && rpc_params->numberOfBitsOfData > 0U) {
    bytes = (rpc_params->numberOfBitsOfData + 7U) / 8U;
    prefix_bytes = bytes < kRpcTraceMaxBytes ? bytes : kRpcTraceMaxBytes;
    for (unsigned int i = 0; i < prefix_bytes; ++i) {
      std::snprintf(prefix + (i * 3U), sizeof(prefix) - (i * 3U), "%02x ", rpc_params->input[i]);
    }
    if (prefix_bytes > 0U) {
      prefix[(prefix_bytes * 3U) - 1U] = '\0';
    }
  }

  trace_netf("rpc-in id=%u name=%s local=%s source=%s count=%u bits=%u bytes=%u sender=0x%08x:%u idx=%u first=%s",
             rpc_id, rpc_name(rpc_id), rpc_local_status_name(rpc_local_status(rpc_id)), rpc_source(rpc_id),
             (rpc_id < 256U) ? g_rpc_probe.counts[rpc_id] : 0U,
             rpc_params != nullptr ? rpc_params->numberOfBitsOfData : 0U, bytes,
             rpc_params != nullptr ? rpc_params->sender.binaryAddress : 0U,
             rpc_params != nullptr ? rpc_params->sender.port : 0U,
             rpc_params != nullptr ? static_cast<unsigned int>(rpc_params->senderIndex) : 0U,
             prefix_bytes > 0U ? prefix : "-");

  observe_rpc_handler_surface(rpc_id, bytes, rpc_params != nullptr ? rpc_params->numberOfBitsOfData : 0U);

  if (rpc_id == 93U) {
    if (rpc_params != nullptr && bytes >= 8U) {
      decode_client_message_payload(rpc_params->input, bytes);
    }
  } else if (rpc_id == 101U) {
    if (rpc_params == nullptr || !decode_chat_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=101 chat decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 139U) {
    g_rpc_probe.saw_init_game = 1;
    if (rpc_params != nullptr) {
      if (!decode_init_game_payload(rpc_params->input, bytes)) {
        trace_netf("rpc-state id=139 init_game decode failed bytes=%u", bytes);
      }
    }
    if (!g_rpc_probe.sent_request_class && !g_rpc_probe.pending_request_class &&
        !g_rpc_probe.waiting_for_request_class_reply) {
      schedule_request_class(g_rpc_probe.selected_class, "init_game");
    }
  } else if (rpc_id == 128U) {
    g_rpc_probe.saw_request_class_reply = 1;
    g_rpc_probe.waiting_for_request_class_reply = 0;
    if (rpc_params != nullptr && bytes > 0U) {
      g_rpc_probe.request_class_outcome = rpc_params->input[0];
      trace_netf("rpc-state id=128 request_class_outcome=%u selected_class=%d",
                 static_cast<unsigned int>(g_rpc_probe.request_class_outcome), g_rpc_probe.selected_class);
      if (g_rpc_probe.request_class_outcome != 0U && bytes >= (1U + kObservedPlayerSpawnInfoBytes)) {
        read_spawn_info(rpc_params->input + 1, bytes - 1U, "RequestClass");
        if (g_rpc_probe.class_selection_ready_time == 0U) {
          g_rpc_probe.class_selection_ready_time = RakNet::GetTime() + kClassSelectionManualDelayMs;
        }
      }
    }
    if (g_rpc_probe.request_spawn_send_count == 0U) {
      if (g_rpc_probe.saw_dialog) {
        schedule_request_spawn_if_ready("request_class_pending_dialog", kInitialSpawnDelayMs);
      } else {
        schedule_request_spawn_if_ready("request_class", kInitialSpawnDelayMs);
      }
    }
  } else if (rpc_id == 129U) {
    g_rpc_probe.saw_request_spawn_reply = 1;
    if (rpc_params != nullptr && bytes > 0U) {
      g_rpc_probe.request_spawn_outcome = rpc_params->input[0];
      trace_netf("rpc-state id=129 request_spawn_outcome=%u", static_cast<unsigned int>(g_rpc_probe.request_spawn_outcome));
      if (g_rpc_probe.request_spawn_outcome == 2U || g_rpc_probe.request_spawn_outcome == 1U) {
        g_rpc_probe.pending_request_spawn = 0;
        g_rpc_probe.next_request_spawn_time = 0;
        g_rpc_probe.request_spawn_retry_count = kMaxSpawnRetries;
        trace_netf("rpc-state id=129 spawn allowed outcome=%u waiting_for_local_finalize=1",
                   static_cast<unsigned int>(g_rpc_probe.request_spawn_outcome));
      } else if (!g_rpc_probe.saw_dialog && auto_request_spawn_enabled() &&
                 g_rpc_probe.request_spawn_retry_count < kMaxSpawnRetries) {
        ++g_rpc_probe.request_spawn_retry_count;
        g_rpc_probe.pending_request_spawn = 1;
        g_rpc_probe.next_request_spawn_time = RakNet::GetTime() + kSpawnRetryDelayMs;
        trace_netf("rpc-state id=129 scheduled RequestSpawn retry=%u delay_ms=%u",
                   g_rpc_probe.request_spawn_retry_count, static_cast<unsigned int>(kSpawnRetryDelayMs));
      } else {
        g_rpc_probe.pending_request_spawn = 0;
        g_rpc_probe.next_request_spawn_time = 0;
        g_rpc_probe.request_spawn_send_count = 0U;
        trace_netf("rpc-state id=129 spawn denied outcome=%u manual_retry_enabled=1 evidence=INFERRED",
                   static_cast<unsigned int>(g_rpc_probe.request_spawn_outcome));
      }
    }
  } else if (rpc_id == 83U) {
    unsigned char select_enabled = 0U;
    unsigned int select_color = 0U;
    const unsigned int bits = rpc_params != nullptr ? rpc_params->numberOfBitsOfData : 0U;

    g_rpc_probe.saw_textdraw_select = 1;
    if (rpc_params != nullptr &&
        decode_textdraw_select_payload(rpc_params->input, bytes, bits, &select_enabled, &select_color)) {
      g_rpc_probe.textdraw_select_active = select_enabled;
      g_rpc_probe.textdraw_select_color = select_color;
      trace_netf("rpc-state id=83 select_textdraw active=%u color=0x%08x bits=%u bytes=%u observe_only=1",
                 static_cast<unsigned int>(select_enabled), g_rpc_probe.textdraw_select_color, bits, bytes);
      if (select_enabled != 0U) {
        schedule_select_mode_freeroam_click("select_textdraw");
      }
    } else {
      g_rpc_probe.textdraw_select_active = 0U;
      g_rpc_probe.textdraw_select_color = 0U;
      trace_netf("rpc-state id=83 select_textdraw decode_failed bits=%u bytes=%u active=0", bits, bytes);
    }
  } else if (rpc_id == 134U) {
    if (rpc_params == nullptr || !decode_textdraw_show_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=134 textdraw_show decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 16U) {
    if (rpc_params == nullptr || !decode_play_sound_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=16 play_sound decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 20U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.reset_player_money_seq);
    trace_netf("rpc-state id=20 reset_player_money_seq=%u apply_pending=1", seq);
  } else if (rpc_id == 21U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.reset_player_weapons_seq);
    trace_netf("rpc-state id=21 reset_player_weapons_seq=%u apply_pending=1", seq);
  } else if (rpc_id == 56U) {
    if (rpc_params == nullptr || !decode_set_map_icon_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=56 set_map_icon decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 32U) {
    if (rpc_params == nullptr || !decode_world_player_add_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=32 world_player_add decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 36U) {
    if (rpc_params == nullptr || !decode_create_3d_text_label_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=36 create_3d_text_label decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 42U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.stop_audio_stream_seq);
    trace_netf("rpc-state id=42 stop_audio_stream_seq=%u decoded_only=1", seq);
  } else if (rpc_id == 43U) {
    if (rpc_params == nullptr || !decode_remove_building_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=43 remove_building decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 135U) {
    if (rpc_params == nullptr || !decode_textdraw_hide_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=135 textdraw_hide decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 105U) {
    if (rpc_params == nullptr || !decode_textdraw_edit_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=105 textdraw_set_string decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 136U) {
    if (rpc_params == nullptr || !decode_textdraw_edit_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=136 textdraw_edit decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 137U) {
    if (rpc_params == nullptr || !decode_server_join_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=137 server_join decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 138U) {
    if (rpc_params == nullptr || !decode_server_quit_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=138 server_quit decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 155U) {
    if (rpc_params == nullptr || !decode_update_scores_pings_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=155 score_ping_update decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 163U) {
    if (rpc_params == nullptr ||
        !decode_world_player_remove_payload(rpc_params->input, bytes, SAMP_RAKNET_REMOTE_PLAYER_ACTION_REMOVE, 0U)) {
      trace_netf("rpc-state id=163 world_player_remove decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 103U) {
    if (rpc_params == nullptr || !handle_client_check_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=103 client_check decode_or_send_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 44U) {
    if (rpc_params == nullptr || !decode_object_create_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=44 object_create decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 45U) {
    if (rpc_params == nullptr || !decode_object_set_pos_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=45 object_set_pos decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 46U) {
    if (rpc_params == nullptr || !decode_object_set_rot_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=46 object_set_rot decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 47U) {
    if (rpc_params == nullptr || !decode_object_destroy_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=47 object_destroy decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 75U) {
    trace_netf("rpc-state id=75 object_attach unsupported_yet=1 bytes=%u observe_only=1", bytes);
  } else if (rpc_id == 80U) {
    if (rpc_params == nullptr || !decode_show_name_tag_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=80 show_name_tag decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 99U) {
    if (rpc_params == nullptr || !decode_object_move_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=99 object_move decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 122U) {
    if (rpc_params == nullptr || !decode_object_stop_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=122 object_stop decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 164U) {
    if (rpc_params == nullptr || !decode_vehicle_add_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=164 vehicle_add decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 165U) {
    if (rpc_params == nullptr || !decode_vehicle_remove_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=165 vehicle_remove decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 166U) {
    if (rpc_params == nullptr ||
        !decode_world_player_remove_payload(rpc_params->input, bytes, SAMP_RAKNET_REMOTE_PLAYER_ACTION_DEATH, 0U)) {
      trace_netf("rpc-state id=166 world_player_death decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 147U) {
    if (rpc_params == nullptr || !decode_vehicle_health_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=147 vehicle_health decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 70U) {
    if (rpc_params == nullptr || !decode_put_player_in_vehicle_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=70 put_player_in_vehicle decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 68U) {
    if (rpc_params != nullptr && bytes >= kObservedPlayerSpawnInfoBytes) {
      read_spawn_info(rpc_params->input, bytes, "ScrSetSpawnInfo");
      if (g_rpc_probe.sent_select_mode_freeroam_click && g_rpc_probe.request_spawn_outcome == 0U) {
        g_rpc_probe.saw_request_spawn_reply = 1;
        g_rpc_probe.request_spawn_outcome = 2U;
        g_rpc_probe.pending_request_spawn = 0;
        g_rpc_probe.next_request_spawn_time = 0;
        g_rpc_probe.request_spawn_retry_count = kMaxSpawnRetries;
        trace_netf("rpc-state id=68 spawn_ready inferred_from=select_freeroam_textdraw outcome=2");
      }
    } else {
      g_rpc_probe.saw_spawn_info = 1;
    }
  } else if (rpc_id == 66U) {
    if (rpc_params == nullptr || !decode_player_armour_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=66 player_armour decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 67U) {
    if (rpc_params == nullptr || !decode_player_armed_weapon_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=67 armed_weapon decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 69U) {
    if (rpc_params == nullptr || !decode_player_team_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=69 player_team decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 72U) {
    if (rpc_params == nullptr || !decode_player_color_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=72 player_color decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 84U) {
    if (rpc_params == nullptr || !decode_set_object_material_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=84 set_object_material decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 86U) {
    if (rpc_params == nullptr || !decode_apply_animation_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=86 apply_animation decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 108U) {
    if (rpc_params == nullptr || !decode_gang_zone_create_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=108 gang_zone_create decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 12U) {
    if (rpc_params != nullptr && bytes >= 12U) {
      g_rpc_probe.saw_player_pos = 1;
      read_vec3(rpc_params->input, g_rpc_probe.player_pos);
      ++g_rpc_probe.player_pos_seq;
      if (g_rpc_probe.player_pos_seq == 0U) {
        g_rpc_probe.player_pos_seq = 1U;
      }
      trace_netf("rpc-state id=12 player_pos_seq=%u player_pos=%.3f %.3f %.3f observe_only=1",
                 g_rpc_probe.player_pos_seq,
                 static_cast<double>(g_rpc_probe.player_pos[0]), static_cast<double>(g_rpc_probe.player_pos[1]),
                 static_cast<double>(g_rpc_probe.player_pos[2]));
    }
  } else if (rpc_id == 14U) {
    if (rpc_params != nullptr && bytes >= 4U) {
      const float health = read_le_float(rpc_params->input);
      if (std::isfinite(health) && health >= 0.0f && health <= 10000.0f) {
        g_rpc_probe.saw_player_health = 1;
        g_rpc_probe.player_health = health;
        ++g_rpc_probe.player_health_seq;
        if (g_rpc_probe.player_health_seq == 0U) {
          g_rpc_probe.player_health_seq = 1U;
        }
        trace_netf("rpc-state id=14 player_health_seq=%u health=%.3f observe_only=1",
                   g_rpc_probe.player_health_seq, static_cast<double>(health));
      } else {
        trace_netf("rpc-state id=14 invalid_player_health=%.3f bytes=%u ignored=1",
                   static_cast<double>(health), bytes);
      }
    }
  } else if (rpc_id == 15U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      g_rpc_probe.saw_player_controllable = 1;
      g_rpc_probe.player_controllable = rpc_params->input[0] != 0U ? 1U : 0U;
      ++g_rpc_probe.player_controllable_seq;
      if (g_rpc_probe.player_controllable_seq == 0U) {
        g_rpc_probe.player_controllable_seq = 1U;
      }
      trace_netf("rpc-state id=15 player_controllable_seq=%u controllable=%u observe_only=1",
                 g_rpc_probe.player_controllable_seq,
                 static_cast<unsigned int>(g_rpc_probe.player_controllable));
    }
  } else if (rpc_id == 19U) {
    if (rpc_params != nullptr && bytes >= 4U) {
      g_rpc_probe.saw_player_facing = 1;
      g_rpc_probe.player_facing_angle = read_le_float(rpc_params->input);
      ++g_rpc_probe.player_facing_seq;
      if (g_rpc_probe.player_facing_seq == 0U) {
        g_rpc_probe.player_facing_seq = 1U;
      }
      trace_netf("rpc-state id=19 player_facing_seq=%u player_facing=%.3f observe_only=1",
                 g_rpc_probe.player_facing_seq, static_cast<double>(g_rpc_probe.player_facing_angle));
    }
  } else if (rpc_id == kRpcScrDialogBox) {
    g_rpc_probe.saw_dialog = 1;
    if (rpc_params != nullptr && bytes >= 2U) {
      const bool decoded = decode_dialog_payload(rpc_params->input, bytes);
      if (!decoded) {
        g_rpc_probe.last_dialog_id = read_le16(rpc_params->input);
        g_rpc_probe.last_dialog_style = (bytes >= 3U) ? rpc_params->input[2] : 0U;
        copy_text(g_rpc_probe.dialog_title, sizeof(g_rpc_probe.dialog_title), "SA-MP Dialog");
        copy_text(g_rpc_probe.dialog_button1, sizeof(g_rpc_probe.dialog_button1), "OK");
      }
      trace_netf("rpc-state id=61 dialog_id=%u style=%u title='%s' button1='%s' button2='%s' manual_response=1",
                 static_cast<unsigned int>(g_rpc_probe.last_dialog_id),
                 static_cast<unsigned int>(g_rpc_probe.last_dialog_style), g_rpc_probe.dialog_title,
                 g_rpc_probe.dialog_button1, g_rpc_probe.dialog_button2);
      if (debug_dialog_window_enabled()) {
        show_manual_dialog_window();
      } else {
        trace_netf("dialog-ui: ingame dialog pending id=%u style=%u title='%s'",
                   static_cast<unsigned int>(g_rpc_probe.last_dialog_id),
                   static_cast<unsigned int>(g_rpc_probe.last_dialog_style), g_rpc_probe.dialog_title);
      }
    }
  } else if (rpc_id == 152U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      g_rpc_probe.saw_weather = 1;
      g_rpc_probe.weather = rpc_params->input[0];
      trace_netf("rpc-state id=152 weather=%u observe_only=1", static_cast<unsigned int>(g_rpc_probe.weather));
    }
  } else if (rpc_id == 94U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      const unsigned char hour = rpc_params->input[0];
      if (hour < 24U) {
        g_rpc_probe.saw_world_time = 1;
        g_rpc_probe.world_time_hour = hour;
        trace_netf("rpc-state id=94 world_time=%u observe_only=1", static_cast<unsigned int>(hour));
      } else {
        trace_netf("rpc-state id=94 invalid_world_time=%u bytes=%u observe_only=1",
                   static_cast<unsigned int>(hour), bytes);
      }
    }
  } else if (rpc_id == 29U) {
    if (rpc_params != nullptr && bytes >= 2U) {
      const unsigned char hour = rpc_params->input[0];
      const unsigned char minute = rpc_params->input[1];
      if (hour < 24U && minute < 60U) {
        g_rpc_probe.saw_set_time_ex = 1;
        g_rpc_probe.world_time_hour = hour;
        g_rpc_probe.world_time_minute = minute;
        trace_netf("rpc-state id=29 set_time_ex=%u:%02u observe_only=1", static_cast<unsigned int>(hour),
                   static_cast<unsigned int>(minute));
      } else {
        trace_netf("rpc-state id=29 invalid_set_time_ex=%u:%u bytes=%u observe_only=1",
                   static_cast<unsigned int>(hour), static_cast<unsigned int>(minute), bytes);
      }
    }
  } else if (rpc_id == 144U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      g_rpc_probe.saw_toggle_clock = 1;
      g_rpc_probe.clock_enabled = rpc_params->input[0] != 0U ? 1U : 0U;
      (void)decode_remove_map_icon_payload(rpc_params->input, bytes);
      trace_netf("rpc-state id=144 toggle_clock_compat=%u bytes=%u",
                 static_cast<unsigned int>(g_rpc_probe.clock_enabled), bytes);
    }
  } else if (rpc_id == 156U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      g_rpc_probe.saw_interior = 1;
      g_rpc_probe.interior = rpc_params->input[0];
      trace_netf("rpc-state id=156 interior=%u observe_only=1", static_cast<unsigned int>(g_rpc_probe.interior));
    }
  } else if (rpc_id == 157U) {
    if (rpc_params != nullptr && bytes >= 12U) {
      g_rpc_probe.saw_camera_pos = 1;
      read_vec3(rpc_params->input, g_rpc_probe.camera_pos);
      trace_netf("rpc-state id=157 camera_pos=%.3f %.3f %.3f observe_only=1",
                 static_cast<double>(g_rpc_probe.camera_pos[0]), static_cast<double>(g_rpc_probe.camera_pos[1]),
                 static_cast<double>(g_rpc_probe.camera_pos[2]));
    }
  } else if (rpc_id == 158U) {
    if (rpc_params != nullptr && bytes >= 12U) {
      g_rpc_probe.saw_camera_look_at = 1;
      read_vec3(rpc_params->input, g_rpc_probe.camera_look_at);
      g_rpc_probe.camera_look_at_type = (bytes >= 13U) ? rpc_params->input[12] : 0U;
      trace_netf("rpc-state id=158 camera_look_at=%.3f %.3f %.3f type=%u observe_only=1",
                 static_cast<double>(g_rpc_probe.camera_look_at[0]),
                 static_cast<double>(g_rpc_probe.camera_look_at[1]),
                 static_cast<double>(g_rpc_probe.camera_look_at[2]),
                 static_cast<unsigned int>(g_rpc_probe.camera_look_at_type));
    }
  } else if (rpc_id == 162U) {
    g_rpc_probe.saw_camera_behind = 1;
    ++g_rpc_probe.camera_behind_seq;
    if (g_rpc_probe.camera_behind_seq == 0U) {
      g_rpc_probe.camera_behind_seq = 1U;
    }
    trace_netf("rpc-state id=162 camera_behind_seq=%u observe_only=1", g_rpc_probe.camera_behind_seq);
  }
}

void register_rpc_probe_handlers(RakNet::RakClientInterface *rak_client) {
  if (rak_client == nullptr) {
    return;
  }

  for (unsigned int i = 1U; i <= kRpcRegisterMax; ++i) {
    g_rpc_probe.rpc_ids[i] = static_cast<unsigned char>(i);
    rak_client->RegisterAsRemoteProcedureCall(static_cast<RakNet::RPCID>(g_rpc_probe.rpc_ids[i]), rpc_observer,
                                              &g_rpc_probe.rpc_ids[i]);
  }

  g_rpc_probe.registered = 1;
  trace_netf("rpc-probe registered ids=1..%u known=%u noop=%u mode=catch_all_handler_surface",
             kRpcRegisterMax, rpc_known_meta_count(), rpc_dummy_meta_count());
}

unsigned char get_packet_id(const RakNet::Packet *packet) {
  if (packet == nullptr || packet->data == nullptr || packet->length == 0) {
    return kPacketIdInvalid;
  }

  if (packet->data[0] == RakNet::ID_TIMESTAMP) {
    const unsigned int timestamp_prefix = static_cast<unsigned int>(sizeof(RakNet::RakNetTime)) + 1U;
    if (packet->length <= timestamp_prefix) {
      return kPacketIdInvalid;
    }
    return packet->data[timestamp_prefix];
  }

  return packet->data[0];
}

void copy_profile_string(const char *input, char *output, size_t output_size, const char *fallback) {
  if (output == nullptr || output_size == 0U) {
    return;
  }

  output[0] = '\0';
  if (input != nullptr && input[0] != '\0') {
    std::strncpy(output, input, output_size - 1U);
    output[output_size - 1U] = '\0';
    return;
  }

  if (fallback != nullptr && fallback[0] != '\0') {
    std::strncpy(output, fallback, output_size - 1U);
    output[output_size - 1U] = '\0';
  }
}

void big_num_mul(unsigned long in[5], unsigned long out[6], unsigned long factor) {
  unsigned long src[5] = {0};
  unsigned long long tmp = 0;

  for (int i = 0; i < 5; ++i) {
    src[i] = ((in[4 - i] >> 24) | ((in[4 - i] << 8) & 0x00FF0000UL) | ((in[4 - i] >> 8) & 0x0000FF00UL) |
              (in[4 - i] << 24));
  }

  tmp = static_cast<unsigned long long>(src[0]) * static_cast<unsigned long long>(factor);
  out[0] = static_cast<unsigned long>(tmp & 0xFFFFFFFFULL);
  out[1] = static_cast<unsigned long>(tmp >> 32);

  tmp = static_cast<unsigned long long>(src[1]) * static_cast<unsigned long long>(factor) +
        static_cast<unsigned long long>(out[1]);
  out[1] = static_cast<unsigned long>(tmp & 0xFFFFFFFFULL);
  out[2] = static_cast<unsigned long>(tmp >> 32);

  tmp = static_cast<unsigned long long>(src[2]) * static_cast<unsigned long long>(factor) +
        static_cast<unsigned long long>(out[2]);
  out[2] = static_cast<unsigned long>(tmp & 0xFFFFFFFFULL);
  out[3] = static_cast<unsigned long>(tmp >> 32);

  tmp = static_cast<unsigned long long>(src[3]) * static_cast<unsigned long long>(factor) +
        static_cast<unsigned long long>(out[3]);
  out[3] = static_cast<unsigned long>(tmp & 0xFFFFFFFFULL);
  out[4] = static_cast<unsigned long>(tmp >> 32);

  tmp = static_cast<unsigned long long>(src[4]) * static_cast<unsigned long long>(factor) +
        static_cast<unsigned long long>(out[4]);
  out[4] = static_cast<unsigned long>(tmp & 0xFFFFFFFFULL);
  out[5] = static_cast<unsigned long>(tmp >> 32);

  for (int i = 0; i < 12; ++i) {
    unsigned char temp = reinterpret_cast<unsigned char *>(out)[i];
    reinterpret_cast<unsigned char *>(out)[i] = reinterpret_cast<unsigned char *>(out)[23 - i];
    reinterpret_cast<unsigned char *>(out)[23 - i] = temp;
  }
}

int gen_gpci(char buf[64], unsigned long factor) {
  static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  unsigned char out[6 * 4 + 1] = {0};
  unsigned int notzero = 0;
  int pos = 0;

  if (buf == nullptr) {
    return 0;
  }

  buf[0] = '0';
  buf[1] = '\0';
  if (factor == 0UL) {
    return 1;
  }

  for (int i = 0; i < 6 * 4; ++i) {
    out[i] = static_cast<unsigned char>(alphanum[std::rand() % (sizeof(alphanum) - 1U)]);
  }
  out[6 * 4] = 0;

  big_num_mul(reinterpret_cast<unsigned long *>(out), reinterpret_cast<unsigned long *>(out), factor);

  for (int i = 0; i < 24; ++i) {
    unsigned char upper = out[i] >> 4;
    unsigned char lower = out[i] & 0x0F;

    if (notzero || upper != 0) {
      buf[pos++] = static_cast<char>((upper > 9) ? (upper + 55) : (upper + 48));
      if (!notzero) {
        notzero = 1;
      }
    }
    if (notzero || lower != 0) {
      buf[pos++] = static_cast<char>((lower > 9) ? (lower + 55) : (lower + 48));
      if (!notzero) {
        notzero = 1;
      }
    }
  }
  buf[pos] = '\0';
  return pos;
}

const char *find_auth_response(const char *challenge) {
  if (challenge == nullptr || challenge[0] == '\0') {
    return nullptr;
  }

  // OPENMP_REF + INFERRED:
  // The SA-MP auth challenge/response table is generated from vendored
  // Knogle/RakNet at configure time so public builds do not depend on local
  // reverse-engineering workspaces. TODO_VERIFY against a fresh 0.3.7 trace.
  return samp_raknet_knogle_auth_response(challenge);
}

int send_auth_key_response(RakNet::RakClientInterface *rak_client, const RakNet::Packet *packet) {
  unsigned char challenge_len = 0;
  char challenge[128] = {0};
  const char *response = nullptr;
  unsigned char response_len = 0;
  RakNet::BitStream bs_send;

  if (rak_client == nullptr || packet == nullptr || packet->data == nullptr || packet->length <= 2U) {
    return 0;
  }

  RakNet::BitStream bs_auth(packet->data, packet->length, false);
  bs_auth.IgnoreBits(8);
  if (!bs_auth.Read(challenge_len) || challenge_len == 0U || challenge_len >= sizeof(challenge)) {
    return 0;
  }

  if (packet->length < 2U + static_cast<unsigned int>(challenge_len)) {
    return 0;
  }

  std::memcpy(challenge, packet->data + 2, challenge_len);
  challenge[sizeof(challenge) - 1U] = '\0';

  response = find_auth_response(challenge);
  if (response == nullptr) {
    return 0;
  }

  response_len = static_cast<unsigned char>(std::strlen(response));
  bs_send.Write(static_cast<unsigned char>(RakNet::ID_AUTH_KEY));
  bs_send.Write(response_len);
  bs_send.Write(response, response_len);

  const int sent = rak_client->Send(&bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0) ? 1 : 0;
  trace_netf("auth-key challenge=%s response_len=%u sent=%d", challenge, static_cast<unsigned int>(response_len),
             sent);
  return sent;
}

int send_client_join(RakNet::RakClientInterface *rak_client, const RakNet::Packet *packet,
                     const samp_raknet_join_profile *profile) {
  RakNet::BitStream bs_send;
  RakNet::PlayerIndex my_player_id = RakNet::UNASSIGNED_PLAYER_INDEX;
  unsigned int server_challenge = 0;
  unsigned int netgame_version = kDefaultNetgameVersion;
  unsigned int challenge_response = 0;
  unsigned char mod_byte = kDefaultModByte;
  char nickname[sizeof(profile->nickname)] = {0};
  char client_version[sizeof(profile->client_version)] = {0};
  char auth_bs[64] = {0};
  unsigned char name_len = 0;
  unsigned char auth_len = 0;
  unsigned char version_len = 0;

  if (rak_client == nullptr || packet == nullptr || packet->data == nullptr || packet->length == 0) {
    return 0;
  }

  if (profile != nullptr) {
    if (profile->netgame_version != 0U) {
      netgame_version = profile->netgame_version;
    }
    if (profile->mod_byte != 0U) {
      mod_byte = profile->mod_byte;
    }
    copy_profile_string(profile->nickname, nickname, sizeof(nickname), kDefaultNickname);
    copy_profile_string(profile->client_version, client_version, sizeof(client_version), kDefaultClientVersion);
  } else {
    copy_profile_string(nullptr, nickname, sizeof(nickname), kDefaultNickname);
    copy_profile_string(nullptr, client_version, sizeof(client_version), kDefaultClientVersion);
  }
  copy_text(g_rpc_probe.local_nickname, sizeof(g_rpc_probe.local_nickname), nickname);

  if (gen_gpci(auth_bs, kGpciFactor) <= 0) {
    std::strcpy(auth_bs, "0");
  }

  RakNet::BitStream bs_succ_auth(packet->data, packet->length, false);
  bs_succ_auth.IgnoreBits(8);
  bs_succ_auth.IgnoreBits(32);
  bs_succ_auth.IgnoreBits(16);
  if (!bs_succ_auth.Read(my_player_id)) {
    return 0;
  }
  if (!bs_succ_auth.Read(server_challenge)) {
    return 0;
  }
  if (static_cast<unsigned int>(my_player_id) < SAMP_RAKNET_MAX_PLAYERS) {
    g_rpc_probe.auth_local_player_id = static_cast<unsigned short>(my_player_id);
    g_rpc_probe.auth_local_player_id_valid = 1U;
  }

  challenge_response = server_challenge ^ netgame_version;
  name_len = static_cast<unsigned char>(std::strlen(nickname));
  auth_len = static_cast<unsigned char>(std::strlen(auth_bs));
  version_len = static_cast<unsigned char>(std::strlen(client_version));

  bs_send.Write(netgame_version);
  bs_send.Write(mod_byte);
  bs_send.Write(name_len);
  bs_send.Write(nickname, name_len);
  bs_send.Write(challenge_response);
  bs_send.Write(auth_len);
  bs_send.Write(auth_bs, auth_len);
  bs_send.Write(version_len);
  bs_send.Write(client_version, version_len);

  const int sent = rak_client->RPC(kRpcClientJoin, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                  RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                       ? 1
                       : 0;
  trace_netf("rpc-auto-out id=25 name=ClientJoin nickname=%s version=%s player_index=%u server_challenge=%u sent=%d",
             nickname, client_version, static_cast<unsigned int>(my_player_id), server_challenge, sent);
  return sent;
}

int drain_packets_internal(void *client, int max_packets, const samp_raknet_join_profile *profile, int autojoin,
                           int *out_connected, int *out_join_sent, int *out_last_packet_id) {
  int drained = 0;
  int join_sent = 0;
  int last_packet_id = -1;
  RakNet::RakClientInterface *rak_client = nullptr;

  if (client == nullptr || max_packets < 0) {
    return -1;
  }

  rak_client = static_cast<RakNet::RakClientInterface *>(client);
  while (drained < max_packets) {
    RakNet::Packet *packet = rak_client->Receive();
    if (packet == nullptr) {
      service_rpc_probe_actions(rak_client);
      break;
    }

    const unsigned char packet_id = get_packet_id(packet);
    const bool reset_session = packet_resets_session_state(packet_id);
    last_packet_id = static_cast<int>(packet_id);
    if (autojoin && last_packet_id == static_cast<int>(RakNet::ID_AUTH_KEY)) {
      send_auth_key_response(rak_client, packet);
    } else if (autojoin && !join_sent && last_packet_id == static_cast<int>(RakNet::ID_CONNECTION_REQUEST_ACCEPTED)) {
      join_sent = send_client_join(rak_client, packet, profile);
    }
    if (packet_id == kPacketPlayerSync && packet->data != nullptr && packet->length > 0U) {
      unsigned int sync_offset = 0U;
      if (packet->data[0] == RakNet::ID_TIMESTAMP) {
        sync_offset = static_cast<unsigned int>(sizeof(RakNet::RakNetTime)) + 1U;
      }
      if (sync_offset >= packet->length ||
          !decode_remote_onfoot_sync_packet(packet->data + sync_offset, packet->length - sync_offset)) {
        trace_netf("packet-state id=207 remote_onfoot decode_failed bytes=%u offset=%u",
                   static_cast<unsigned int>(packet->length), sync_offset);
      }
    }

    rak_client->DeallocatePacket(packet);
    if (reset_session) {
      trace_netf("packet-state id=%d reset_rpc_probe reason=disconnect", last_packet_id);
      reset_rpc_probe_runtime(rak_client);
    } else {
      service_rpc_probe_actions(rak_client);
    }
    ++drained;
  }

  if (out_connected != nullptr) {
    *out_connected = rak_client->IsConnected() ? 1 : 0;
  }
  if (out_join_sent != nullptr) {
    *out_join_sent = join_sent;
  }
  if (out_last_packet_id != nullptr) {
    *out_last_packet_id = last_packet_id;
  }
  return drained;
}
}  // namespace

int samp_raknet_client_available(void) { return 1; }

int samp_raknet_client_create(void **out_client) {
  RakNet::RakClientInterface *client = nullptr;

  if (out_client == nullptr) {
    return -1;
  }
  *out_client = nullptr;

  client = RakNet::RakNetworkFactory::GetRakClientInterface();
  if (client == nullptr) {
    return -1;
  }

  reset_rpc_probe_runtime(client);
  register_rpc_probe_handlers(client);
  *out_client = client;
  return 0;
}

int samp_raknet_client_destroy(void *client) {
  if (client == nullptr) {
    return 0;
  }

  RakNet::RakNetworkFactory::DestroyRakClientInterface(static_cast<RakNet::RakClientInterface *>(client));
  return 0;
}

int samp_raknet_client_connect(void *client, const char *host, uint16_t server_port, uint16_t client_port,
                               int thread_sleep_timer) {
  if (client == nullptr || host == nullptr || host[0] == '\0') {
    return -1;
  }

  reset_rpc_probe_runtime(static_cast<RakNet::RakClientInterface *>(client));
  trace_netf("connect host=%s server_port=%u client_port=%u thread_sleep=%d", host, static_cast<unsigned int>(server_port),
             static_cast<unsigned int>(client_port), thread_sleep_timer);
  return static_cast<RakNet::RakClientInterface *>(client)->Connect(host, server_port, client_port, 0U, thread_sleep_timer)
             ? 0
             : -1;
}

void samp_raknet_client_disconnect(void *client, unsigned int block_duration, unsigned char ordering_channel) {
  if (client == nullptr) {
    return;
  }

  static_cast<RakNet::RakClientInterface *>(client)->Disconnect(block_duration, ordering_channel);
}

int samp_raknet_client_is_connected(void *client) {
  if (client == nullptr) {
    return 0;
  }

  return static_cast<RakNet::RakClientInterface *>(client)->IsConnected() ? 1 : 0;
}

int samp_raknet_client_mark_logged_on(void *client) {
  if (client == nullptr) {
    return -1;
  }

  RakNet::RakClient *rak_client = static_cast<RakNet::RakClient *>(static_cast<RakNet::RakClientInterface *>(client));
  const bool before = rak_client->IsServerLogonForSamp();
  const bool marked = rak_client->MarkServerLogonForSamp();
  trace_netf("packet-state mark-logon before=%d marked=%d", before ? 1 : 0, marked ? 1 : 0);
  return marked ? 0 : -1;
}

int samp_raknet_client_send_chat(void *client, const char *text) {
  if (client == nullptr) {
    return -1;
  }
  return send_chat_rpc_internal(static_cast<RakNet::RakClientInterface *>(client), text, 0);
}

int samp_raknet_client_send_server_command(void *client, const char *command) {
  if (client == nullptr) {
    return -1;
  }
  return send_chat_rpc_internal(static_cast<RakNet::RakClientInterface *>(client), command, 1);
}

int samp_raknet_client_send_textdraw_click(void *client, uint16_t textdraw_id) {
  RakNet::BitStream bs_send;
  unsigned short id = textdraw_id;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }
  if (textdraw_id >= SAMP_RAKNET_MAX_TEXTDRAWS && textdraw_id != 0xFFFFu) {
    return -1;
  }

  bs_send.Write(id);
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->RPC(kRpcClickTextDraw, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                   RakNet::UNASSIGNED_NETWORK_ID, nullptr)
             ? 1
             : 0;
  if (sent) {
    g_rpc_probe.textdraw_select_active = 0U;
  }
  trace_netf("rpc-user-out id=83 name=ClickTextDraw textdraw=%u sent=%d",
             static_cast<unsigned int>(textdraw_id), sent);
  return sent ? 0 : -2;
}

int samp_raknet_client_send_onfoot_sync(void *client, const samp_raknet_onfoot_sync *sync) {
  RakNet::BitStream bs_send;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client || sync == nullptr) {
    return -1;
  }

  /*
   * OPENMP_REF + STATIC_037 + INFERRED:
   * Client -> server player sync is packet 207 followed by the 0.3.7 on-foot payload. open.mp's read path
   * consumes the packet id before PlayerFootSync::read(); SAMPFUNCS documents the 68-byte 0.3.7 payload layout.
   */
  bs_send.Write(kPacketPlayerSync);
  bs_send.Write(reinterpret_cast<const char *>(sync), static_cast<int>(sizeof(*sync)));
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->Send(&bs_send, RakNet::HIGH_PRIORITY, RakNet::UNRELIABLE_SEQUENCED, 0)
             ? 1
             : 0;
  trace_netf("packet-user-out id=%u name=PlayerFootSync sent=%d pos=%.3f %.3f %.3f health=%u weapon=%u",
             static_cast<unsigned int>(kPacketPlayerSync), sent, static_cast<double>(sync->position[0]),
             static_cast<double>(sync->position[1]), static_cast<double>(sync->position[2]),
             static_cast<unsigned int>(sync->health), static_cast<unsigned int>(sync->current_weapon));
  return sent ? 0 : -2;
}

int samp_raknet_client_send_incar_sync(void *client, const samp_raknet_incar_sync *sync) {
  RakNet::BitStream bs_send;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client || sync == nullptr) {
    return -1;
  }

  /*
   * INFERRED + OPENMP_REF + TODO_VERIFY:
   * The compatibility local-player driver path periodically sends ID_VEHICLE_SYNC while the ped is the driver.
   * open.mp's 0.3.7 read path consumes packet 200, then this 63-byte client->server payload starting with VehicleID.
   */
  bs_send.Write(kPacketVehicleSync);
  bs_send.Write(reinterpret_cast<const char *>(sync), static_cast<int>(sizeof(*sync)));
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->Send(&bs_send, RakNet::HIGH_PRIORITY, RakNet::UNRELIABLE_SEQUENCED, 0)
             ? 1
             : 0;
  trace_netf(
      "packet-user-out id=%u name=VehicleSync sent=%d vehicle=%u pos=%.3f %.3f %.3f speed=%.3f %.3f %.3f "
      "vehicle_health=%.3f player_health=%u weapon=%u evidence=INFERRED,OPENMP_REF,TODO_VERIFY",
      static_cast<unsigned int>(kPacketVehicleSync), sent, static_cast<unsigned int>(sync->vehicle_id),
      static_cast<double>(sync->position[0]), static_cast<double>(sync->position[1]),
      static_cast<double>(sync->position[2]), static_cast<double>(sync->move_speed[0]),
      static_cast<double>(sync->move_speed[1]), static_cast<double>(sync->move_speed[2]),
      static_cast<double>(sync->vehicle_health), static_cast<unsigned int>(sync->player_health),
      static_cast<unsigned int>(sync->additional_key_weapon));
  return sent ? 0 : -2;
}

int samp_raknet_client_drain_packets(void *client, int max_packets) {
  return drain_packets_internal(client, max_packets, nullptr, 0, nullptr, nullptr, nullptr);
}

int samp_raknet_client_drain_packets_autojoin(void *client, int max_packets, const samp_raknet_join_profile *profile,
                                              int *out_connected, int *out_join_sent, int *out_last_packet_id) {
  return drain_packets_internal(client, max_packets, profile, 1, out_connected, out_join_sent, out_last_packet_id);
}

int samp_raknet_client_get_rpc_probe_snapshot(void *client, samp_raknet_rpc_probe_snapshot *out_snapshot) {
  uint32_t flags = 0U;

  if (client == nullptr || out_snapshot == nullptr || client != g_rpc_probe.client) {
    return -1;
  }

  if (g_rpc_probe.saw_init_game) {
    flags |= SAMP_RAKNET_RPC_FLAG_INIT_GAME;
  }
  if (g_rpc_probe.saw_request_class_reply) {
    flags |= SAMP_RAKNET_RPC_FLAG_REQUEST_CLASS_REPLY;
  }
  if (g_rpc_probe.saw_request_spawn_reply) {
    flags |= SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_REPLY;
  }
  if (g_rpc_probe.saw_spawn_info) {
    flags |= SAMP_RAKNET_RPC_FLAG_SPAWN_INFO;
  }
  if (g_rpc_probe.saw_dialog) {
    flags |= SAMP_RAKNET_RPC_FLAG_DIALOG;
  }
  if (g_rpc_probe.sent_dialog_response) {
    flags |= SAMP_RAKNET_RPC_FLAG_DIALOG_RESPONSE_SENT;
  }
  if (g_rpc_probe.saw_player_pos) {
    flags |= SAMP_RAKNET_RPC_FLAG_PLAYER_POS;
  }
  if (g_rpc_probe.saw_player_facing) {
    flags |= SAMP_RAKNET_RPC_FLAG_PLAYER_FACING;
  }
  if (g_rpc_probe.saw_player_health) {
    flags |= SAMP_RAKNET_RPC_FLAG_PLAYER_HEALTH;
  }
  if (g_rpc_probe.saw_player_controllable) {
    flags |= SAMP_RAKNET_RPC_FLAG_PLAYER_CONTROLLABLE;
  }
  if (g_rpc_probe.saw_weather) {
    flags |= SAMP_RAKNET_RPC_FLAG_WEATHER;
  }
  if (g_rpc_probe.saw_world_time) {
    flags |= SAMP_RAKNET_RPC_FLAG_WORLD_TIME;
  }
  if (g_rpc_probe.saw_set_time_ex) {
    flags |= SAMP_RAKNET_RPC_FLAG_SET_TIME_EX;
  }
  if (g_rpc_probe.saw_toggle_clock) {
    flags |= SAMP_RAKNET_RPC_FLAG_TOGGLE_CLOCK;
  }
  if (g_rpc_probe.saw_interior) {
    flags |= SAMP_RAKNET_RPC_FLAG_INTERIOR;
  }
  if (g_rpc_probe.saw_camera_pos) {
    flags |= SAMP_RAKNET_RPC_FLAG_CAMERA_POS;
  }
  if (g_rpc_probe.saw_camera_look_at) {
    flags |= SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT;
  }
  if (g_rpc_probe.saw_camera_behind) {
    flags |= SAMP_RAKNET_RPC_FLAG_CAMERA_BEHIND;
  }
  if (g_rpc_probe.player_armour_seq > 0U || g_rpc_probe.player_armed_weapon_seq > 0U ||
      g_rpc_probe.reset_player_weapons_seq > 0U || g_rpc_probe.reset_player_money_seq > 0U ||
      g_rpc_probe.play_sound_seq > 0U || g_rpc_probe.stop_audio_stream_seq > 0U ||
      g_rpc_probe.player_color_seq > 0U || g_rpc_probe.player_team_seq > 0U ||
      g_rpc_probe.apply_animation_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_PLAYER_SCRIPT_EVENT;
  }
  if (g_rpc_probe.world_visual_event_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_WORLD_VISUAL_EVENT;
  }
  if (g_rpc_probe.saw_client_message) {
    flags |= SAMP_RAKNET_RPC_FLAG_CLIENT_MESSAGE;
  }
  if (g_rpc_probe.textdraw_event_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_TEXTDRAW_EVENT;
  }
  if (g_rpc_probe.saw_textdraw_select && g_rpc_probe.textdraw_select_active) {
    flags |= SAMP_RAKNET_RPC_FLAG_TEXTDRAW_SELECT;
  }
  if (g_rpc_probe.object_event_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_OBJECT_EVENT;
  }
  if (g_rpc_probe.vehicle_event_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_VEHICLE_EVENT;
  }
  if (g_rpc_probe.remote_player_event_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_REMOTE_PLAYER_EVENT;
  }
  if (g_rpc_probe.remote_player_sync_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_REMOTE_PLAYER_SYNC;
  }
  if (g_rpc_probe.player_pool_event_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_PLAYER_POOL_EVENT;
  }
  if (g_rpc_probe.score_ping_seq > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_SCOREBOARD_UPDATE;
  }
  if (g_rpc_probe.auth_local_player_id_valid) {
    flags |= SAMP_RAKNET_RPC_FLAG_AUTH_LOCAL_PLAYER;
  }
  if (g_rpc_probe.sent_request_class) {
    flags |= SAMP_RAKNET_RPC_FLAG_REQUEST_CLASS_SENT;
  }
  if (g_rpc_probe.request_spawn_send_count > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_SENT;
  }

  out_snapshot->flags = flags;
  out_snapshot->client_message_count = 0U;
  out_snapshot->textdraw_event_count = 0U;
  out_snapshot->object_event_count = 0U;
  out_snapshot->vehicle_event_count = 0U;
  out_snapshot->remote_player_event_count = 0U;
  out_snapshot->remote_player_sync_count = 0U;
  out_snapshot->map_icon_event_count = 0U;
  out_snapshot->name_tag_event_count = 0U;
  out_snapshot->player_pool_event_count = 0U;
  out_snapshot->score_ping_count = 0U;
  out_snapshot->textdraw_select_active = g_rpc_probe.textdraw_select_active;
  out_snapshot->textdraw_select_color = g_rpc_probe.textdraw_select_color;
  out_snapshot->init_spawns_available = g_rpc_probe.init_spawns_available;
  out_snapshot->init_local_player_id = g_rpc_probe.init_local_player_id;
  out_snapshot->auth_local_player_id_valid = g_rpc_probe.auth_local_player_id_valid;
  out_snapshot->auth_local_player_id = g_rpc_probe.auth_local_player_id;
  out_snapshot->init_show_player_tags = g_rpc_probe.init_show_player_tags;
  out_snapshot->init_show_player_markers = g_rpc_probe.init_show_player_markers;
  out_snapshot->init_tire_popping = g_rpc_probe.init_tire_popping;
  out_snapshot->init_world_time = g_rpc_probe.init_world_time;
  out_snapshot->init_weather = g_rpc_probe.init_weather;
  out_snapshot->init_gravity = g_rpc_probe.init_gravity;
  out_snapshot->init_lan_mode = g_rpc_probe.init_lan_mode;
  out_snapshot->init_death_drop_money = g_rpc_probe.init_death_drop_money;
  out_snapshot->init_instagib = g_rpc_probe.init_instagib;
  out_snapshot->init_zone_names = g_rpc_probe.init_zone_names;
  out_snapshot->init_use_cj_walk = g_rpc_probe.init_use_cj_walk;
  out_snapshot->init_allow_weapons = g_rpc_probe.init_allow_weapons;
  out_snapshot->init_limit_global_chat_radius = g_rpc_probe.init_limit_global_chat_radius;
  out_snapshot->init_global_chat_radius = g_rpc_probe.init_global_chat_radius;
  out_snapshot->init_stunt_bonus = g_rpc_probe.init_stunt_bonus;
  out_snapshot->init_name_tag_draw_distance = g_rpc_probe.init_name_tag_draw_distance;
  out_snapshot->init_disable_enter_exits = g_rpc_probe.init_disable_enter_exits;
  out_snapshot->init_name_tag_los = g_rpc_probe.init_name_tag_los;
  std::memcpy(out_snapshot->init_send_rates, g_rpc_probe.init_send_rates, sizeof(out_snapshot->init_send_rates));
  std::memcpy(out_snapshot->init_hostname, g_rpc_probe.init_hostname, sizeof(out_snapshot->init_hostname));
  std::memcpy(out_snapshot->init_vehicle_models, g_rpc_probe.init_vehicle_models,
              sizeof(out_snapshot->init_vehicle_models));
  out_snapshot->request_class_outcome = g_rpc_probe.request_class_outcome;
  out_snapshot->request_spawn_outcome = g_rpc_probe.request_spawn_outcome;
  out_snapshot->last_dialog_id = g_rpc_probe.last_dialog_id;
  out_snapshot->last_dialog_style = g_rpc_probe.last_dialog_style;
  std::memcpy(out_snapshot->dialog_title, g_rpc_probe.dialog_title, sizeof(out_snapshot->dialog_title));
  std::memcpy(out_snapshot->dialog_info, g_rpc_probe.dialog_info, sizeof(out_snapshot->dialog_info));
  std::memcpy(out_snapshot->dialog_button1, g_rpc_probe.dialog_button1, sizeof(out_snapshot->dialog_button1));
  std::memcpy(out_snapshot->dialog_button2, g_rpc_probe.dialog_button2, sizeof(out_snapshot->dialog_button2));
  std::memcpy(out_snapshot->player_pos, g_rpc_probe.player_pos, sizeof(out_snapshot->player_pos));
  out_snapshot->player_facing_angle = g_rpc_probe.player_facing_angle;
  out_snapshot->player_health = g_rpc_probe.player_health;
  out_snapshot->player_pos_seq = g_rpc_probe.player_pos_seq;
  out_snapshot->player_facing_seq = g_rpc_probe.player_facing_seq;
  out_snapshot->player_health_seq = g_rpc_probe.player_health_seq;
  out_snapshot->player_controllable_seq = g_rpc_probe.player_controllable_seq;
  out_snapshot->camera_behind_seq = g_rpc_probe.camera_behind_seq;
  out_snapshot->player_armour_seq = g_rpc_probe.player_armour_seq;
  out_snapshot->player_armed_weapon_seq = g_rpc_probe.player_armed_weapon_seq;
  out_snapshot->reset_player_weapons_seq = g_rpc_probe.reset_player_weapons_seq;
  out_snapshot->reset_player_money_seq = g_rpc_probe.reset_player_money_seq;
  out_snapshot->play_sound_seq = g_rpc_probe.play_sound_seq;
  out_snapshot->stop_audio_stream_seq = g_rpc_probe.stop_audio_stream_seq;
  out_snapshot->player_color_seq = g_rpc_probe.player_color_seq;
  out_snapshot->player_team_seq = g_rpc_probe.player_team_seq;
  out_snapshot->apply_animation_seq = g_rpc_probe.apply_animation_seq;
  out_snapshot->world_visual_event_seq = g_rpc_probe.world_visual_event_seq;
  out_snapshot->player_pool_event_seq = g_rpc_probe.player_pool_event_seq;
  out_snapshot->score_ping_seq = g_rpc_probe.score_ping_seq;
  out_snapshot->player_controllable = g_rpc_probe.player_controllable;
  out_snapshot->player_armour = g_rpc_probe.player_armour;
  out_snapshot->player_armed_weapon = g_rpc_probe.player_armed_weapon;
  out_snapshot->play_sound_id = g_rpc_probe.play_sound_id;
  std::memcpy(out_snapshot->play_sound_pos, g_rpc_probe.play_sound_pos, sizeof(out_snapshot->play_sound_pos));
  out_snapshot->player_color_player_id = g_rpc_probe.player_color_player_id;
  out_snapshot->player_color = g_rpc_probe.player_color;
  out_snapshot->player_team_player_id = g_rpc_probe.player_team_player_id;
  out_snapshot->player_team = g_rpc_probe.player_team;
  out_snapshot->apply_animation_player_id = g_rpc_probe.apply_animation_player_id;
  std::memcpy(out_snapshot->apply_animation_lib, g_rpc_probe.apply_animation_lib,
              sizeof(out_snapshot->apply_animation_lib));
  std::memcpy(out_snapshot->apply_animation_name, g_rpc_probe.apply_animation_name,
              sizeof(out_snapshot->apply_animation_name));
  out_snapshot->apply_animation_delta = g_rpc_probe.apply_animation_delta;
  out_snapshot->apply_animation_loop = g_rpc_probe.apply_animation_loop;
  out_snapshot->apply_animation_lock_x = g_rpc_probe.apply_animation_lock_x;
  out_snapshot->apply_animation_lock_y = g_rpc_probe.apply_animation_lock_y;
  out_snapshot->apply_animation_freeze = g_rpc_probe.apply_animation_freeze;
  out_snapshot->apply_animation_time = g_rpc_probe.apply_animation_time;
  out_snapshot->world_visual_event_type = g_rpc_probe.world_visual_event_type;
  out_snapshot->world_visual_id = g_rpc_probe.world_visual_id;
  out_snapshot->world_visual_model = g_rpc_probe.world_visual_model;
  out_snapshot->world_visual_color = g_rpc_probe.world_visual_color;
  out_snapshot->world_visual_material_background = g_rpc_probe.world_visual_material_background;
  std::memcpy(out_snapshot->world_visual_pos, g_rpc_probe.world_visual_pos,
              sizeof(out_snapshot->world_visual_pos));
  out_snapshot->world_visual_material_type = g_rpc_probe.world_visual_material_type;
  out_snapshot->world_visual_material_slot = g_rpc_probe.world_visual_material_slot;
  out_snapshot->world_visual_material_size = g_rpc_probe.world_visual_material_size;
  out_snapshot->world_visual_material_font_size = g_rpc_probe.world_visual_material_font_size;
  out_snapshot->world_visual_material_bold = g_rpc_probe.world_visual_material_bold;
  out_snapshot->world_visual_material_alignment = g_rpc_probe.world_visual_material_alignment;
  std::memcpy(out_snapshot->world_visual_text, g_rpc_probe.world_visual_text,
              sizeof(out_snapshot->world_visual_text));
  std::memcpy(out_snapshot->world_visual_material_txd, g_rpc_probe.world_visual_material_txd,
              sizeof(out_snapshot->world_visual_material_txd));
  std::memcpy(out_snapshot->world_visual_material_texture, g_rpc_probe.world_visual_material_texture,
              sizeof(out_snapshot->world_visual_material_texture));
  out_snapshot->weather = g_rpc_probe.weather;
  out_snapshot->world_time_hour = g_rpc_probe.world_time_hour;
  out_snapshot->world_time_minute = g_rpc_probe.world_time_minute;
  out_snapshot->clock_enabled = g_rpc_probe.clock_enabled;
  out_snapshot->interior = g_rpc_probe.interior;
  std::memcpy(out_snapshot->camera_pos, g_rpc_probe.camera_pos, sizeof(out_snapshot->camera_pos));
  std::memcpy(out_snapshot->camera_look_at, g_rpc_probe.camera_look_at, sizeof(out_snapshot->camera_look_at));
  out_snapshot->camera_look_at_type = g_rpc_probe.camera_look_at_type;
  out_snapshot->spawn_team = g_rpc_probe.spawn_team;
  out_snapshot->spawn_info_seq = g_rpc_probe.spawn_info_seq;
  out_snapshot->spawn_skin = g_rpc_probe.spawn_skin;
  std::memcpy(out_snapshot->spawn_pos, g_rpc_probe.spawn_pos, sizeof(out_snapshot->spawn_pos));
  out_snapshot->spawn_rotation = g_rpc_probe.spawn_rotation;
  std::memcpy(out_snapshot->spawn_weapons, g_rpc_probe.spawn_weapons, sizeof(out_snapshot->spawn_weapons));
  std::memcpy(out_snapshot->spawn_weapon_ammo, g_rpc_probe.spawn_weapon_ammo, sizeof(out_snapshot->spawn_weapon_ammo));
  std::memset(out_snapshot->client_messages, 0, sizeof(out_snapshot->client_messages));
  if (g_rpc_probe.client_message_seq > 0U) {
    const unsigned int available = g_rpc_probe.client_message_seq < SAMP_RAKNET_CLIENT_MESSAGE_RING
                                       ? g_rpc_probe.client_message_seq
                                       : SAMP_RAKNET_CLIENT_MESSAGE_RING;
    const unsigned int first_seq = g_rpc_probe.client_message_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_CLIENT_MESSAGE_RING;
      if (g_rpc_probe.client_messages[slot].seq == seq) {
        out_snapshot->client_messages[out_snapshot->client_message_count++] = g_rpc_probe.client_messages[slot];
      }
    }
  }
  std::memset(out_snapshot->textdraw_events, 0, sizeof(out_snapshot->textdraw_events));
  if (g_rpc_probe.textdraw_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.textdraw_event_seq < SAMP_RAKNET_TEXTDRAW_EVENT_RING
                                       ? g_rpc_probe.textdraw_event_seq
                                       : SAMP_RAKNET_TEXTDRAW_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.textdraw_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_TEXTDRAW_EVENT_RING;
      if (g_rpc_probe.textdraw_events[slot].seq == seq) {
        out_snapshot->textdraw_events[out_snapshot->textdraw_event_count++] = g_rpc_probe.textdraw_events[slot];
      }
    }
  }
  std::memset(out_snapshot->object_events, 0, sizeof(out_snapshot->object_events));
  if (g_rpc_probe.object_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.object_event_seq < SAMP_RAKNET_OBJECT_EVENT_RING
                                       ? g_rpc_probe.object_event_seq
                                       : SAMP_RAKNET_OBJECT_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.object_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_OBJECT_EVENT_RING;
      if (g_rpc_probe.object_events[slot].seq == seq) {
        out_snapshot->object_events[out_snapshot->object_event_count++] = g_rpc_probe.object_events[slot];
      }
    }
  }
  std::memset(out_snapshot->vehicle_events, 0, sizeof(out_snapshot->vehicle_events));
  if (g_rpc_probe.vehicle_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.vehicle_event_seq < SAMP_RAKNET_VEHICLE_EVENT_RING
                                       ? g_rpc_probe.vehicle_event_seq
                                       : SAMP_RAKNET_VEHICLE_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.vehicle_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_VEHICLE_EVENT_RING;
      if (g_rpc_probe.vehicle_events[slot].seq == seq) {
        out_snapshot->vehicle_events[out_snapshot->vehicle_event_count++] = g_rpc_probe.vehicle_events[slot];
      }
    }
  }
  std::memset(out_snapshot->remote_player_events, 0, sizeof(out_snapshot->remote_player_events));
  if (g_rpc_probe.remote_player_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.remote_player_event_seq < SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING
                                       ? g_rpc_probe.remote_player_event_seq
                                       : SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.remote_player_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING;
      if (g_rpc_probe.remote_player_events[slot].seq == seq) {
        out_snapshot->remote_player_events[out_snapshot->remote_player_event_count++] =
            g_rpc_probe.remote_player_events[slot];
      }
    }
  }
  std::memset(out_snapshot->map_icon_events, 0, sizeof(out_snapshot->map_icon_events));
  if (g_rpc_probe.map_icon_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.map_icon_event_seq < SAMP_RAKNET_MAP_ICON_EVENT_RING
                                       ? g_rpc_probe.map_icon_event_seq
                                       : SAMP_RAKNET_MAP_ICON_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.map_icon_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_MAP_ICON_EVENT_RING;
      if (g_rpc_probe.map_icon_events[slot].seq == seq) {
        out_snapshot->map_icon_events[out_snapshot->map_icon_event_count++] = g_rpc_probe.map_icon_events[slot];
      }
    }
  }
  std::memset(out_snapshot->name_tag_events, 0, sizeof(out_snapshot->name_tag_events));
  if (g_rpc_probe.name_tag_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.name_tag_event_seq < SAMP_RAKNET_NAME_TAG_EVENT_RING
                                       ? g_rpc_probe.name_tag_event_seq
                                       : SAMP_RAKNET_NAME_TAG_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.name_tag_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_NAME_TAG_EVENT_RING;
      if (g_rpc_probe.name_tag_events[slot].seq == seq) {
        out_snapshot->name_tag_events[out_snapshot->name_tag_event_count++] = g_rpc_probe.name_tag_events[slot];
      }
    }
  }
  std::memset(out_snapshot->remote_player_syncs, 0, sizeof(out_snapshot->remote_player_syncs));
  if (g_rpc_probe.remote_player_sync_seq > 0U) {
    const unsigned int available = g_rpc_probe.remote_player_sync_seq < SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING
                                       ? g_rpc_probe.remote_player_sync_seq
                                       : SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING;
    const unsigned int first_seq = g_rpc_probe.remote_player_sync_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING;
      if (g_rpc_probe.remote_player_syncs[slot].seq == seq) {
        out_snapshot->remote_player_syncs[out_snapshot->remote_player_sync_count++] =
            g_rpc_probe.remote_player_syncs[slot];
      }
    }
  }
  std::memset(out_snapshot->player_pool_events, 0, sizeof(out_snapshot->player_pool_events));
  if (g_rpc_probe.player_pool_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.player_pool_event_seq < SAMP_RAKNET_PLAYER_POOL_EVENT_RING
                                       ? g_rpc_probe.player_pool_event_seq
                                       : SAMP_RAKNET_PLAYER_POOL_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.player_pool_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_PLAYER_POOL_EVENT_RING;
      if (g_rpc_probe.player_pool_events[slot].seq == seq) {
        out_snapshot->player_pool_events[out_snapshot->player_pool_event_count++] =
            g_rpc_probe.player_pool_events[slot];
      }
    }
  }
  std::memset(out_snapshot->score_ping_entries, 0, sizeof(out_snapshot->score_ping_entries));
  if (g_rpc_probe.score_ping_count > 0U) {
    out_snapshot->score_ping_count = g_rpc_probe.score_ping_count < SAMP_RAKNET_SCORE_PING_MAX_ENTRIES
                                         ? g_rpc_probe.score_ping_count
                                         : SAMP_RAKNET_SCORE_PING_MAX_ENTRIES;
    std::memcpy(out_snapshot->score_ping_entries, g_rpc_probe.score_ping_entries,
                out_snapshot->score_ping_count * sizeof(out_snapshot->score_ping_entries[0]));
  }
  return 0;
}

int samp_raknet_client_send_spawn_notification(void *client) {
  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }

  return send_spawn_notification_with_seq(static_cast<RakNet::RakClientInterface *>(client), 0U);
}

int samp_raknet_client_send_spawn_notification_for_seq(void *client, uint32_t spawn_info_seq) {
  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }

  return send_spawn_notification_with_seq(static_cast<RakNet::RakClientInterface *>(client),
                                          static_cast<unsigned int>(spawn_info_seq));
}

int samp_raknet_client_request_class_delta(void *client, int delta) {
  RakNet::RakNetTime now = 0;
  RakNet::RakNetTime class_elapsed = 0;
  int step = 0;
  int next_class = 0;

  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }
  if (delta == 0 || !class_selection_manual_ready()) {
    return -1;
  }
  now = RakNet::GetTime();
  class_elapsed = now >= g_rpc_probe.last_request_class_time ? now - g_rpc_probe.last_request_class_time : 0U;
  if (class_elapsed < kClassSelectionReselectDelayMs) {
    return -2;
  }

  /*
   * INFERRED + OPENMP_REF:
   * Compatibility CLocalPlayer::ProcessClassSelection only changes class by one slot
   * per accepted LEFT/RIGHT action and then sends RequestClass(selected_class).
   */
  step = delta < 0 ? -1 : 1;
  next_class = clamp_class_id(g_rpc_probe.selected_class + step);
  schedule_request_class(next_class, step < 0 ? "ui_left" : "ui_right");
  return 0;
}

int samp_raknet_client_queue_dialog_response(void *client, uint16_t dialog_id, uint8_t button, int16_t listitem,
                                             const char *input) {
  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }
  if (!g_rpc_probe.saw_dialog && g_rpc_probe.last_dialog_id != dialog_id) {
    return -1;
  }

  queue_dialog_response(dialog_id, button, static_cast<std::int16_t>(listitem), input);
  return 0;
}
