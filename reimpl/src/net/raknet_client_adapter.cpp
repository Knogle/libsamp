#include "sampdll/net/raknet_client_adapter.h"

#include "death_message_codec.h"

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
constexpr unsigned char kPacketRconCommand = 201u;
constexpr unsigned char kPacketAimSync = 203u;
constexpr unsigned char kPacketBulletSync = 206u;
constexpr unsigned char kPacketPlayerSync = 207u;
constexpr unsigned char kPacketSpectatorSync = 212u;
constexpr RakNet::RPCID kRpcClientJoin = static_cast<RakNet::RPCID>(25u);
constexpr RakNet::RPCID kRpcDialogResponse = static_cast<RakNet::RPCID>(62u);
constexpr RakNet::RPCID kRpcSpawn = static_cast<RakNet::RPCID>(52u);
constexpr RakNet::RPCID kRpcDeath = static_cast<RakNet::RPCID>(53u);
constexpr RakNet::RPCID kRpcServerCommand = static_cast<RakNet::RPCID>(50u);
constexpr RakNet::RPCID kRpcChat = static_cast<RakNet::RPCID>(101u);
constexpr RakNet::RPCID kRpcClientCheck = static_cast<RakNet::RPCID>(103u);
constexpr RakNet::RPCID kRpcClickTextDraw = static_cast<RakNet::RPCID>(83u);
constexpr RakNet::RPCID kRpcGiveTakeDamage = static_cast<RakNet::RPCID>(115u);
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
constexpr RakNet::RakNetTime kClassSelectionReselectDelayMs = 250U;
constexpr RakNet::RakNetTime kScorePingUpdateMs = 3000U;
constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr unsigned int kGtaModelInfoCount = 20000U;
constexpr unsigned int kObjectMaterialTextBudgetBytes = 8U * 1024U * 1024U;
constexpr unsigned int kObservedPlayerSpawnInfoBytes = 45U;
constexpr unsigned int kPlayerSpawnInfoBytes = 46U;
constexpr unsigned int kDialogTitleBytes = 256U;
constexpr unsigned int kDialogInfoBytes = 4096U;
constexpr unsigned int kDialogButtonBytes = 64U;
constexpr unsigned int kDialogInputBytes = 256U;
constexpr unsigned int kChatInputBytes = 128U;
constexpr unsigned int kCompactTextDrawTransmitBytes = 40U;
constexpr unsigned int kClassicTextDrawPreviewNoSelectableBytes = 22U;
constexpr unsigned int kClassicTextDrawPreviewWithSelectableBytes = 23U;
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
constexpr unsigned int kOpenMpObjectCreateAttachOffsetOffset = 39U;
constexpr unsigned int kOpenMpObjectCreateAttachRotOffset = 51U;
constexpr unsigned int kOpenMpObjectCreateAttachSyncOffset = 63U;
constexpr unsigned int kOpenMpAttachObjectToPlayerMinBytes = 2U + 2U + 6U * 4U;
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
static_assert(sizeof(samp_raknet_aim_sync) == 31U, "SA-MP 0.3.7 aim sync payload must be 31 bytes");
static_assert(sizeof(samp_raknet_bullet_sync) == 40U, "SA-MP 0.3.7 bullet sync payload must be 40 bytes");
static_assert(sizeof(samp_raknet_spectator_sync) == 18U, "SA-MP 0.3.7 spectator sync payload must be 18 bytes");
static_assert(sizeof(samp_raknet_map_icon_event) == 24U, "internal map-icon event ABI must remain stable");

bool packet_resets_session_state(unsigned char packet_id) {
  return packet_id == static_cast<unsigned char>(RakNet::ID_DISCONNECTION_NOTIFICATION) ||
         packet_id == static_cast<unsigned char>(RakNet::ID_CONNECTION_LOST);
}

struct ObjectMaterialSlotState {
  unsigned int generation;
  unsigned int revision;
  unsigned int materialColor;
  unsigned int backgroundColor;
  std::int32_t model;
  unsigned char type;
  unsigned char slot;
  unsigned char source;
  unsigned char materialSize;
  unsigned char fontSize;
  unsigned char bold;
  unsigned char alignment;
  char nameA[SAMP_RAKNET_OBJECT_MATERIAL_NAME_BYTES];
  char nameB[SAMP_RAKNET_OBJECT_MATERIAL_NAME_BYTES];
  char *text;
  unsigned int textBytes;
};

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
  int pending_dialog_response;
  int pending_request_class_id;
  int sent_request_class;
  int waiting_for_request_class_reply;
  int sent_dialog_response;
  unsigned int request_spawn_send_count;
  int sent_spawn_notify;
  unsigned int sent_spawn_notify_seq;
  int manual_spawn_shift_down;
  int class_selection_after_death_requested;
  int class_selection_after_death_consumed;
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
  unsigned int player_given_weapon_seq;
  unsigned int reset_player_weapons_seq;
  unsigned int reset_player_money_seq;
  unsigned int give_player_money_seq;
  unsigned int player_ammo_seq;
  unsigned int player_skin_seq;
  unsigned int player_skill_seq;
  unsigned int player_drunk_seq;
  unsigned int player_fighting_style_seq;
  unsigned int player_pos_find_z_seq;
  unsigned int player_velocity_seq;
  unsigned int remove_player_from_vehicle_seq;
  unsigned int clear_animations_seq;
  unsigned int vehicle_velocity_seq;
  unsigned int stunt_bonus_seq;
  unsigned int checkpoint_seq;
  unsigned int disable_checkpoint_seq;
  unsigned int race_checkpoint_seq;
  unsigned int disable_race_checkpoint_seq;
  unsigned int checkpoint_event_seq;
  unsigned int pickup_event_seq;
  samp_raknet_pickup_event pickup_events[SAMP_RAKNET_PICKUP_EVENT_RING];
  unsigned int explosion_event_seq;
  samp_raknet_explosion_event explosion_events[SAMP_RAKNET_EXPLOSION_EVENT_RING];
  unsigned int chat_bubble_event_seq;
  samp_raknet_chat_bubble_event chat_bubble_events[SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING];
  unsigned int cancel_edit_seq;
  unsigned int shop_name_seq;
  unsigned int play_audio_stream_seq;
  unsigned int play_sound_seq;
  unsigned int stop_audio_stream_seq;
  unsigned int player_color_seq;
  unsigned int player_team_seq;
  unsigned int apply_animation_seq;
  unsigned int player_wanted_level_seq;
  unsigned int gravity_seq;
  unsigned int world_bounds_seq;
  unsigned int game_mode_restart_seq;
  unsigned int force_class_selection_seq;
  unsigned int camera_attach_object_seq;
  unsigned int camera_interpolate_seq;
  unsigned int special_action_seq;
  unsigned int spectate_toggle_seq;
  unsigned int spectate_player_seq;
  unsigned int spectate_vehicle_seq;
  unsigned int world_visual_event_seq;
  unsigned int client_check_response_count;
  unsigned char player_controllable;
  float player_armour;
  unsigned int player_armed_weapon;
  unsigned int player_given_weapon;
  unsigned int player_given_weapon_ammo;
  std::int32_t give_player_money;
  samp_raknet_give_money_event give_money_events[SAMP_RAKNET_GIVE_MONEY_EVENT_RING];
  unsigned char player_ammo_weapon;
  unsigned short player_ammo;
  unsigned int player_skin_player_id;
  std::int32_t player_skin;
  unsigned short player_skill_player_id;
  unsigned int player_skill;
  unsigned short player_skill_level;
  unsigned int player_drunk_level;
  unsigned short player_fighting_style_player_id;
  unsigned char player_fighting_style;
  float player_pos_find_z[3];
  float player_velocity[3];
  unsigned short clear_animations_player_id;
  unsigned char vehicle_velocity_turn;
  float vehicle_velocity[3];
  unsigned char stunt_bonus_enabled;
  float checkpoint_pos[3];
  float checkpoint_size;
  unsigned char race_checkpoint_type;
  float race_checkpoint_pos[3];
  float race_checkpoint_next[3];
  float race_checkpoint_size;
  unsigned char checkpoint_event_type;
  samp_raknet_given_weapon_event player_given_weapon_events[SAMP_RAKNET_GIVE_WEAPON_EVENT_RING];
  unsigned int play_sound_id;
  float play_sound_pos[3];
  char shop_name[SAMP_RAKNET_SHOP_NAME_BYTES];
  char audio_stream_url[SAMP_RAKNET_AUDIO_STREAM_URL_BYTES];
  float audio_stream_pos[3];
  float audio_stream_distance;
  unsigned char audio_stream_use_pos;
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
  unsigned char player_wanted_level;
  float gravity;
  float world_bounds[4];
  unsigned short camera_object_id;
  unsigned char camera_interpolate_set_pos;
  unsigned char camera_interpolate_cut;
  float camera_interpolate_from[3];
  float camera_interpolate_to[3];
  std::int32_t camera_interpolate_time_ms;
  unsigned char special_action;
  std::int32_t spectate_toggle;
  unsigned short spectate_player_id;
  unsigned short spectate_vehicle_id;
  unsigned char spectate_player_mode;
  unsigned char spectate_vehicle_mode;
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
  unsigned int object_generations[SAMP_RAKNET_MAX_OBJECTS];
  unsigned char object_alive[SAMP_RAKNET_MAX_OBJECTS];
  samp_raknet_object_event object_create_states[SAMP_RAKNET_MAX_OBJECTS];
  unsigned int object_generation_seq;
  unsigned int object_material_revision_seq;
  unsigned int object_material_text_bytes;
  ObjectMaterialSlotState *object_material_slots[SAMP_RAKNET_MAX_OBJECTS][SAMP_RAKNET_OBJECT_MATERIAL_SLOTS];
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
  unsigned int death_window_event_seq;
  samp_raknet_death_window_event death_window_events[SAMP_RAKNET_DEATH_WINDOW_EVENT_RING];
  unsigned int text_label_event_seq;
  samp_raknet_3d_text_label_event text_label_events[SAMP_RAKNET_3D_TEXT_LABEL_EVENT_RING];
  unsigned int player_pool_event_seq;
  samp_raknet_player_pool_event player_pool_events[SAMP_RAKNET_PLAYER_POOL_EVENT_RING];
  unsigned int score_ping_seq;
  unsigned int score_ping_count;
  samp_raknet_score_ping_entry score_ping_entries[SAMP_RAKNET_SCORE_PING_MAX_ENTRIES];
  unsigned int game_text_event_seq;
  samp_raknet_game_text_event game_text_events[SAMP_RAKNET_GAMETEXT_EVENT_RING];
  unsigned int game_text_seq;
  std::int32_t game_text_style;
  std::int32_t game_text_time_ms;
  char game_text[SAMP_RAKNET_GAMETEXT_TEXT_BYTES];
  RakNet::RakNetTime last_request_class_time;
  RakNet::RakNetTime next_score_ping_update_time;
#ifdef _WIN32
  int dialog_window_active;
#endif
};

RpcProbeState g_rpc_probe = {};

void release_all_object_material_states(const char *reason);

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
         g_rpc_probe.request_class_outcome != 0U && g_rpc_probe.request_spawn_send_count == 0U;
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
    {11U, "ScrSetPlayerName", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1DFD0"},
    {12U, "ScrSetPlayerPos", kRpcLocalImplemented, "PROBE_TRACE"},
    {13U, "ScrSetPlayerPosFindZ", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19440"},
    {14U, "ScrSetPlayerHealth", kRpcLocalImplemented, "STATIC_037"},
    {15U, "ScrTogglePlayerControllable", kRpcLocalImplemented, "STATIC_037"},
    {16U, "ScrPlaySound", kRpcLocalImplemented, "STATIC_037"},
    {17U, "ScrSetWorldBounds", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1A410"},
    {18U, "ScrGivePlayerMoney", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1A500"},
    {19U, "ScrSetPlayerFacingAngle", kRpcLocalImplemented, "PROBE_TRACE"},
    {20U, "ScrResetPlayerMoney", kRpcLocalImplemented, "STATIC_037"},
    {21U, "ScrResetPlayerWeapons", kRpcLocalImplemented, "STATIC_037"},
    {22U, "ScrGivePlayerWeapon", kRpcLocalImplemented, "STATIC_037,OPENMP_REF,TODO_VERIFY"},
    {23U, "OnPlayerClickPlayer", kRpcLocalOutgoing, "OPENMP_REF"},
    {24U, "ScrSetVehicleParamsEx", kRpcLocalImplemented, "STATIC_037:samp.dll+0x112F0"},
    {25U, "ClientJoin", kRpcLocalOutgoing, "OPENMP_REF"},
    {26U, "EnterVehicle", kRpcLocalOutgoing, "OPENMP_REF"},
    {27U, "EnterEditObject", kRpcLocalOutgoing, "OPENMP_REF"},
    {28U, "ScrCancelEdit", kRpcLocalDecoded, "OPENMP_REF,TODO_VERIFY"},
    {29U, "ScrSetPlayerTime/SetTimeEx", kRpcLocalImplemented, "PROBE_TRACE"},
    {30U, "ScrToggleClock", kRpcLocalImplemented, "STATIC_037:samp.dll+0xF500"},
    {31U, "ScriptCash", kRpcLocalOutgoing, "OPENMP_REF"},
    {32U, "ScrWorldPlayerAdd", kRpcLocalImplemented, "STATIC_037,PROBE_TRACE"},
    {33U, "ScrSetPlayerShopName", kRpcLocalDecoded, "STATIC_037:samp.dll+0x17E50,TODO_VERIFY"},
    {34U, "ScrSetPlayerSkillLevel", kRpcLocalImplemented, "STATIC_037:samp.dll+0xF5E0"},
    {35U, "ScrSetPlayerDrunkLevel", kRpcLocalImplemented, "STATIC_037:samp.dll+0x18DB0"},
    {36U, "ScrCreate3DTextLabel", kRpcLocalImplemented, "OPENMP_REF,TODO_VERIFY"},
    {37U, "ScrDisableCheckpoint", kRpcLocalImplemented, "STATIC_037:samp.dll+0xF080"},
    {38U, "ScrSetRaceCheckpoint", kRpcLocalImplemented, "STATIC_037:samp.dll+0xF140"},
    {39U, "ScrDisableRaceCheckpoint", kRpcLocalImplemented, "STATIC_037:samp.dll+0xF1E0"},
    {40U, "ScrGameModeRestart", kRpcLocalImplemented, "STATIC_037:samp.dll+0xE650"},
    {41U, "ScrPlayAudioStream", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1D3C0,OPENMP_REF,TODO_VERIFY"},
    {42U, "ScrStopAudioStream", kRpcLocalImplemented, "STATIC_037:samp.dll+0x180F0,OPENMP_REF"},
    {43U, "ScrRemoveBuildingForPlayer", kRpcLocalDecoded, "OPENMP_REF"},
    {44U, "ScrCreateObject", kRpcLocalImplemented, "PROBE_TRACE"},
    {45U, "ScrSetObjectPos", kRpcLocalImplemented, "PROBE_TRACE"},
    {46U, "ScrSetObjectRot", kRpcLocalImplemented, "PROBE_TRACE"},
    {47U, "ScrDestroyObject", kRpcLocalImplemented, "PROBE_TRACE"},
    {50U, "ServerCommand", kRpcLocalOutgoing, "OPENMP_REF"},
    {52U, "Spawn", kRpcLocalOutgoing, "OPENMP_REF"},
    {53U, "Death", kRpcLocalOutgoing, "OPENMP_REF"},
    {54U, "NPCJoin", kRpcLocalOutgoing, "OPENMP_REF"},
    {55U, "ScrDeathMessage", kRpcLocalImplemented, "STATIC_037,OPENMP_REF,TODO_VERIFY"},
    {56U, "ScrSetPlayerMapIcon", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1A790"},
    {57U, "ScrRemoveVehicleComponent", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1C5C0"},
    {58U, "ScrUpdate3DTextLabel", kRpcLocalImplemented, "SAMPFUNCS_037,TODO_VERIFY"},
    {59U, "ScrChatBubble", kRpcLocalImplemented, "OPENMP_REF,TODO_VERIFY"},
    {60U, "ScrSomeUpdate", kRpcLocalDummy, "SAMPFUNCS_037"},
    {61U, "ScrShowDialog", kRpcLocalImplemented, "PROBE_TRACE"},
    {62U, "DialogResponse", kRpcLocalOutgoing, "OPENMP_REF"},
    {63U, "ScrDestroyPickup", kRpcLocalImplemented, "STATIC_037:samp.dll+0xF140"},
    {64U, "ScrDelete3DTextLabel", kRpcLocalImplemented, "SAMPFUNCS_037,TODO_VERIFY"},
    {65U, "ScrLinkVehicleToInterior", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19F30"},
    {66U, "ScrSetPlayerArmour", kRpcLocalImplemented, "OPENMP_REF"},
    {67U, "ScrSetPlayerArmedWeapon", kRpcLocalImplemented, "OPENMP_REF"},
    {68U, "ScrSetSpawnInfo", kRpcLocalImplemented, "PROBE_TRACE"},
    {69U, "ScrSetPlayerTeam", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19710,OPENMP_REF"},
    {70U, "ScrPutPlayerInVehicle", kRpcLocalImplemented, "PROBE_TRACE,STATIC_037,TODO_VERIFY"},
    {71U, "ScrRemovePlayerFromVehicle", kRpcLocalImplemented, "STATIC_037:samp.dll+0x17FF0"},
    {72U, "ScrSetPlayerColor", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19800,OPENMP_REF"},
    {73U, "ScrDisplayGameText", kRpcLocalImplemented, "STATIC_037,OPENMP_REF,TODO_VERIFY"},
    {74U, "ScrForceClassSelection", kRpcLocalImplemented, "STATIC_037:samp.dll+0x180D0"},
    {75U, "ScrAttachObjectToPlayer", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1C6A0,PROBE_TRACE,TODO_VERIFY"},
    {76U, "ScrInitMenu", kRpcLocalDummy, "STATIC_037"},
    {77U, "ScrShowMenu", kRpcLocalDummy, "STATIC_037"},
    {78U, "ScrHideMenu", kRpcLocalDummy, "STATIC_037"},
    {79U, "ScrCreateExplosion", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1BD10"},
    {80U, "ScrShowPlayerNameTagForPlayer", kRpcLocalImplemented, "INFERRED,TODO_VERIFY"},
    {81U, "ScrAttachCameraToObject", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19FF0"},
    {82U, "ScrInterpolateCamera", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1A0F0"},
    {83U, "ClickTextDraw/SelectTextDraw", kRpcLocalImplemented, "PROBE_TRACE"},
    {84U, "ScrSetObjectMaterial", kRpcLocalImplemented, "STATIC_037,OPENMP_REF,PROBE_TRACE"},
    {85U, "ScrGangZoneStopFlash", kRpcLocalDummy, "SAMPFUNCS_037"},
    {86U, "ScrApplyAnimation", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1A950"},
    {87U, "ScrClearAnimations", kRpcLocalImplemented, "STATIC_037:samp.dll+0x18580"},
    {88U, "ScrSetPlayerSpecialAction", kRpcLocalImplemented, "STATIC_037:samp.dll+0x18690"},
    {89U, "ScrSetPlayerFightingStyle", kRpcLocalImplemented, "STATIC_037:samp.dll+0x18740"},
    {90U, "ScrSetPlayerVelocity", kRpcLocalImplemented, "STATIC_037:samp.dll+0x18850"},
    {91U, "ScrSetVehicleVelocity", kRpcLocalImplemented, "STATIC_037:samp.dll+0x18950"},
    {93U, "ScrClientMessage", kRpcLocalImplemented, "PROBE_TRACE"},
    {94U, "ScrSetWorldTime", kRpcLocalImplemented, "PROBE_TRACE"},
    {95U, "ScrCreatePickup", kRpcLocalImplemented, "STATIC_037:samp.dll+0xF080"},
    {96U, "ScmEvent", kRpcLocalOutgoing, "OPENMP_REF"},
    {97U, "WeaponPickupDestroy", kRpcLocalOutgoing, "OPENMP_REF"},
    {99U, "ScrMoveObject", kRpcLocalImplemented, "PROBE_TRACE"},
    {101U, "Chat", kRpcLocalImplemented, "INFERRED,OPENMP_REF,TODO_VERIFY"},
    {102U, "ServerNetStats", kRpcLocalOutgoing, "OPENMP_REF"},
    {103U, "ClientCheck", kRpcLocalImplemented, "OPENMP_REF"},
    {104U, "ScrEnableStuntBonusForPlayer", kRpcLocalImplemented, "STATIC_037:samp.dll+0x17D50"},
    {105U, "ScrTextDrawSetString", kRpcLocalImplemented, "PROBE_TRACE"},
    {106U, "DamageVehicle", kRpcLocalOutgoing, "OPENMP_REF"},
    {107U, "ScrSetCheckpoint", kRpcLocalImplemented, "STATIC_037:samp.dll+0xEEF0"},
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
    {123U, "ScrSetNumberPlate", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1C230"},
    {124U, "ScrTogglePlayerSpectating", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1C350"},
    {126U, "ScrPlayerSpectatePlayer", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1C400"},
    {127U, "ScrPlayerSpectateVehicle", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1C4E0"},
    {128U, "RequestClass", kRpcLocalImplemented, "OPENMP_REF"},
    {129U, "RequestSpawn", kRpcLocalImplemented, "OPENMP_REF"},
    {131U, "PickedUpPickup", kRpcLocalOutgoing, "OPENMP_REF"},
    {132U, "MenuSelect", kRpcLocalOutgoing, "OPENMP_REF"},
    {133U, "ScrSetPlayerWantedLevel", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1CCF0"},
    {134U, "ScrShowTextDraw", kRpcLocalImplemented, "PROBE_TRACE"},
    {135U, "ScrHideTextDraw", kRpcLocalImplemented, "PROBE_TRACE"},
    {136U, "ScrEditTextDraw/VehicleDestroyed", kRpcLocalImplemented, "PROBE_TRACE"},
    {137U, "ScrServerJoin", kRpcLocalImplemented, "PROBE_TRACE,OPENMP_REF,TODO_VERIFY"},
    {138U, "ScrServerQuit", kRpcLocalImplemented, "PROBE_TRACE,OPENMP_REF,TODO_VERIFY"},
    {139U, "ScrInitGame", kRpcLocalImplemented, "PROBE_TRACE"},
    {140U, "MenuQuit", kRpcLocalOutgoing, "OPENMP_REF"},
    {144U, "ScrRemovePlayerMapIcon", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1A8B0"},
    {145U, "ScrSetPlayerAmmo", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1AC10"},
    {146U, "ScrSetGravity", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1ACD0"},
    {147U, "ScrSetVehicleHealth", kRpcLocalImplemented, "PROBE_TRACE"},
    {148U, "ScrAttachTrailerToVehicle", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1AE50"},
    {149U, "ScrDetachTrailerFromVehicle", kRpcLocalImplemented, "STATIC_037:samp.dll+0x1AF90"},
    {152U, "ScrSetWeather", kRpcLocalImplemented, "PROBE_TRACE"},
    {153U, "ScrSetPlayerSkin", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19190"},
    {154U, "ExitVehicle", kRpcLocalOutgoing, "OPENMP_REF"},
    {155U, "UpdateScoresPingsIPs", kRpcLocalImplemented, "STATIC_037"},
    {156U, "ScrSetPlayerInterior", kRpcLocalImplemented, "PROBE_TRACE"},
    {157U, "ScrSetPlayerCameraPos", kRpcLocalImplemented, "PROBE_TRACE"},
    {158U, "ScrSetPlayerCameraLookAt", kRpcLocalImplemented, "PROBE_TRACE"},
    {159U, "ScrSetVehiclePos", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19C70"},
    {160U, "ScrSetVehicleZAngle", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19D80"},
    {161U, "ScrSetVehicleParamsForPlayer", kRpcLocalImplemented, "STATIC_037:samp.dll+0x19E60"},
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
    case 11U:
      return 4U;
    case 17U:
      return 16U;
    case 12U:
    case 13U:
    case 90U:
    case 157U:
    case 158U:
      return 12U;
    case 14U:
      return 4U;
    case 18U:
      return 4U;
    case 34U:
      return 8U;
    case 35U:
      return 4U;
    case 89U:
      return 3U;
    case 153U:
      return 8U;
    case 15U:
      return 1U;
    case 16U:
      return 16U;
    case 19U:
      return 4U;
    case 24U:
      return 18U;
    case 36U:
      return 27U;
    case 28U:
    case 37U:
    case 39U:
    case 40U:
    case 42U:
    case 74U:
      return 0U;
    case 33U:
      return 32U;
    case 41U:
      return 18U;
    case 38U:
      return 29U;
    case 58U:
      return 6U;
    case 59U:
      return 15U;
    case 64U:
      return 2U;
    case 43U:
      return 20U;
    case 55U:
      return 5U;
    case 56U:
      return 19U;
    case 57U:
      return 6U;
    case 29U:
      return 2U;
    case 30U:
      return 1U;
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
    case 65U:
      return 3U;
    case 63U:
      return 4U;
    case 66U:
    case 67U:
      return 4U;
    case 68U:
      return kObservedPlayerSpawnInfoBytes;
    case 69U:
      return 3U;
    case 70U:
      return 2U;
    case 71U:
      return 0U;
    case 72U:
      return 6U;
    case 79U:
      return 20U;
    case 81U:
      return 2U;
    case 82U:
      return 30U;
    case 83U:
      return 0U;
    case 84U:
      return 4U;
    case 86U:
      return 16U;
    case 87U:
      return 2U;
    case 88U:
      return 1U;
    case 91U:
      return 13U;
    case 93U:
      return 8U;
    case 95U:
      return 24U;
    case 104U:
      return 1U;
    case 107U:
      return 16U;
    case 94U:
    case 133U:
    case 144U:
    case 152U:
    case 156U:
      return 1U;
    case 146U:
      return 4U;
    case 148U:
      return 4U;
    case 149U:
      return 2U;
    case 145U:
      return 3U;
    case 159U:
      return 14U;
    case 160U:
      return 6U;
    case 161U:
      return 4U;
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
    case 123U:
      return 3U;
    case 124U:
      return 4U;
    case 126U:
    case 127U:
      return 3U;
    case 134U:
      return kTextDrawShowMinBytes;
    case 137U:
      return 8U;
    case 138U:
      return 3U;
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
  release_all_object_material_states("rpc_probe_reset");
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
  g_rpc_probe.pending_dialog_response = 0;
  g_rpc_probe.pending_request_class_id = 0;
  g_rpc_probe.sent_request_class = 0;
  g_rpc_probe.waiting_for_request_class_reply = 0;
  g_rpc_probe.sent_dialog_response = 0;
  g_rpc_probe.request_spawn_send_count = 0;
  g_rpc_probe.sent_spawn_notify = 0;
  g_rpc_probe.sent_spawn_notify_seq = 0U;
  g_rpc_probe.manual_spawn_shift_down = 0;
  g_rpc_probe.class_selection_after_death_requested = 0;
  g_rpc_probe.class_selection_after_death_consumed = 0;
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
  g_rpc_probe.player_given_weapon_seq = 0U;
  g_rpc_probe.reset_player_weapons_seq = 0U;
  g_rpc_probe.reset_player_money_seq = 0U;
  g_rpc_probe.give_player_money_seq = 0U;
  g_rpc_probe.player_ammo_seq = 0U;
  g_rpc_probe.player_skin_seq = 0U;
  g_rpc_probe.player_skill_seq = 0U;
  g_rpc_probe.player_drunk_seq = 0U;
  g_rpc_probe.player_fighting_style_seq = 0U;
  g_rpc_probe.player_pos_find_z_seq = 0U;
  g_rpc_probe.player_velocity_seq = 0U;
  g_rpc_probe.remove_player_from_vehicle_seq = 0U;
  g_rpc_probe.clear_animations_seq = 0U;
  g_rpc_probe.vehicle_velocity_seq = 0U;
  g_rpc_probe.stunt_bonus_seq = 0U;
  g_rpc_probe.checkpoint_seq = 0U;
  g_rpc_probe.disable_checkpoint_seq = 0U;
  g_rpc_probe.race_checkpoint_seq = 0U;
  g_rpc_probe.disable_race_checkpoint_seq = 0U;
  g_rpc_probe.checkpoint_event_seq = 0U;
  g_rpc_probe.pickup_event_seq = 0U;
  g_rpc_probe.explosion_event_seq = 0U;
  g_rpc_probe.chat_bubble_event_seq = 0U;
  g_rpc_probe.cancel_edit_seq = 0U;
  g_rpc_probe.shop_name_seq = 0U;
  g_rpc_probe.play_audio_stream_seq = 0U;
  g_rpc_probe.play_sound_seq = 0U;
  g_rpc_probe.stop_audio_stream_seq = 0U;
  g_rpc_probe.player_color_seq = 0U;
  g_rpc_probe.player_team_seq = 0U;
  g_rpc_probe.apply_animation_seq = 0U;
  g_rpc_probe.player_wanted_level_seq = 0U;
  g_rpc_probe.gravity_seq = 0U;
  g_rpc_probe.world_bounds_seq = 0U;
  g_rpc_probe.game_mode_restart_seq = 0U;
  g_rpc_probe.force_class_selection_seq = 0U;
  g_rpc_probe.camera_attach_object_seq = 0U;
  g_rpc_probe.camera_interpolate_seq = 0U;
  g_rpc_probe.special_action_seq = 0U;
  g_rpc_probe.spectate_toggle_seq = 0U;
  g_rpc_probe.spectate_player_seq = 0U;
  g_rpc_probe.spectate_vehicle_seq = 0U;
  g_rpc_probe.world_visual_event_seq = 0U;
  g_rpc_probe.client_check_response_count = 0U;
  g_rpc_probe.game_text_event_seq = 0U;
  std::memset(g_rpc_probe.game_text_events, 0, sizeof(g_rpc_probe.game_text_events));
  g_rpc_probe.game_text_seq = 0U;
  g_rpc_probe.game_text_style = 0;
  g_rpc_probe.game_text_time_ms = 0;
  g_rpc_probe.game_text[0] = '\0';
  g_rpc_probe.player_controllable = 1U;
  g_rpc_probe.player_armour = 0.0f;
  g_rpc_probe.player_armed_weapon = 0U;
  g_rpc_probe.player_given_weapon = 0U;
  g_rpc_probe.player_given_weapon_ammo = 0U;
  g_rpc_probe.give_player_money = 0;
  g_rpc_probe.player_ammo_weapon = 0U;
  g_rpc_probe.player_ammo = 0U;
  g_rpc_probe.player_skin_player_id = 0U;
  g_rpc_probe.player_skin = 0;
  g_rpc_probe.player_skill_player_id = 0U;
  g_rpc_probe.player_skill = 0U;
  g_rpc_probe.player_skill_level = 0U;
  g_rpc_probe.player_drunk_level = 0U;
  g_rpc_probe.player_fighting_style_player_id = 0U;
  g_rpc_probe.player_fighting_style = 0U;
  std::memset(g_rpc_probe.player_pos_find_z, 0, sizeof(g_rpc_probe.player_pos_find_z));
  std::memset(g_rpc_probe.player_velocity, 0, sizeof(g_rpc_probe.player_velocity));
  g_rpc_probe.clear_animations_player_id = 0U;
  g_rpc_probe.vehicle_velocity_turn = 0U;
  std::memset(g_rpc_probe.vehicle_velocity, 0, sizeof(g_rpc_probe.vehicle_velocity));
  g_rpc_probe.stunt_bonus_enabled = 0U;
  std::memset(g_rpc_probe.checkpoint_pos, 0, sizeof(g_rpc_probe.checkpoint_pos));
  g_rpc_probe.checkpoint_size = 0.0f;
  g_rpc_probe.race_checkpoint_type = 0U;
  std::memset(g_rpc_probe.race_checkpoint_pos, 0, sizeof(g_rpc_probe.race_checkpoint_pos));
  std::memset(g_rpc_probe.race_checkpoint_next, 0, sizeof(g_rpc_probe.race_checkpoint_next));
  g_rpc_probe.race_checkpoint_size = 0.0f;
  g_rpc_probe.checkpoint_event_type = 0U;
  std::memset(g_rpc_probe.player_given_weapon_events, 0, sizeof(g_rpc_probe.player_given_weapon_events));
  std::memset(g_rpc_probe.give_money_events, 0, sizeof(g_rpc_probe.give_money_events));
  std::memset(g_rpc_probe.pickup_events, 0, sizeof(g_rpc_probe.pickup_events));
  std::memset(g_rpc_probe.explosion_events, 0, sizeof(g_rpc_probe.explosion_events));
  std::memset(g_rpc_probe.chat_bubble_events, 0, sizeof(g_rpc_probe.chat_bubble_events));
  g_rpc_probe.shop_name[0] = '\0';
  g_rpc_probe.audio_stream_url[0] = '\0';
  std::memset(g_rpc_probe.audio_stream_pos, 0, sizeof(g_rpc_probe.audio_stream_pos));
  g_rpc_probe.audio_stream_distance = 0.0f;
  g_rpc_probe.audio_stream_use_pos = 0U;
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
  g_rpc_probe.player_wanted_level = 0U;
  g_rpc_probe.gravity = 0.0f;
  std::memset(g_rpc_probe.world_bounds, 0, sizeof(g_rpc_probe.world_bounds));
  g_rpc_probe.camera_object_id = 0U;
  g_rpc_probe.camera_interpolate_set_pos = 0U;
  g_rpc_probe.camera_interpolate_cut = 2U;
  std::memset(g_rpc_probe.camera_interpolate_from, 0, sizeof(g_rpc_probe.camera_interpolate_from));
  std::memset(g_rpc_probe.camera_interpolate_to, 0, sizeof(g_rpc_probe.camera_interpolate_to));
  g_rpc_probe.camera_interpolate_time_ms = 0;
  g_rpc_probe.special_action = 0U;
  g_rpc_probe.spectate_toggle = 0;
  g_rpc_probe.spectate_player_id = 0U;
  g_rpc_probe.spectate_vehicle_id = 0U;
  g_rpc_probe.spectate_player_mode = 0U;
  g_rpc_probe.spectate_vehicle_mode = 0U;
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
  std::memset(g_rpc_probe.object_generations, 0, sizeof(g_rpc_probe.object_generations));
  std::memset(g_rpc_probe.object_alive, 0, sizeof(g_rpc_probe.object_alive));
  std::memset(g_rpc_probe.object_create_states, 0, sizeof(g_rpc_probe.object_create_states));
  g_rpc_probe.object_generation_seq = 0U;
  g_rpc_probe.object_material_revision_seq = 0U;
  g_rpc_probe.object_material_text_bytes = 0U;
  std::memset(g_rpc_probe.object_material_slots, 0, sizeof(g_rpc_probe.object_material_slots));
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
  g_rpc_probe.death_window_event_seq = 0U;
  std::memset(g_rpc_probe.death_window_events, 0, sizeof(g_rpc_probe.death_window_events));
  g_rpc_probe.text_label_event_seq = 0U;
  std::memset(g_rpc_probe.text_label_events, 0, sizeof(g_rpc_probe.text_label_events));
  g_rpc_probe.player_pool_event_seq = 0U;
  std::memset(g_rpc_probe.player_pool_events, 0, sizeof(g_rpc_probe.player_pool_events));
  g_rpc_probe.score_ping_seq = 0U;
  g_rpc_probe.score_ping_count = 0U;
  std::memset(g_rpc_probe.score_ping_entries, 0, sizeof(g_rpc_probe.score_ping_entries));
  g_rpc_probe.last_request_class_time = 0;
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
  event->object_generation = object_id_valid(object_id) ? g_rpc_probe.object_generations[object_id] : 0U;
  event->action = action;
  event->object_id = object_id;
  g_rpc_probe.saw_object_event = 1;
  return event;
}

unsigned int bump_object_material_counter(unsigned int *counter) {
  if (counter == nullptr) {
    return 0U;
  }
  ++(*counter);
  if (*counter == 0U) {
    *counter = 1U;
  }
  return *counter;
}

void copy_object_material_name(char *out, std::size_t out_size, const char *value) {
  if (out == nullptr || out_size == 0U) {
    return;
  }
  out[0] = '\0';
  if (value == nullptr) {
    return;
  }
  std::strncpy(out, value, out_size - 1U);
  out[out_size - 1U] = '\0';
}

unsigned int object_material_text_size(const char *text_value) {
  unsigned int length = 0U;

  if (text_value == nullptr) {
    return 1U;
  }
  while (length + 1U < SAMP_RAKNET_OBJECT_MATERIAL_TEXT_BYTES && text_value[length] != '\0') {
    ++length;
  }
  return length + 1U;
}

void free_object_material_state(ObjectMaterialSlotState *state) {
  if (state == nullptr) {
    return;
  }
  std::free(state->text);
  state->text = nullptr;
  state->textBytes = 0U;
  std::free(state);
}

void release_object_material_slot_state(unsigned short object_id, unsigned int material_slot) {
  ObjectMaterialSlotState *state = nullptr;

  if (!object_id_valid(object_id) || material_slot >= SAMP_RAKNET_OBJECT_MATERIAL_SLOTS) {
    return;
  }
  state = g_rpc_probe.object_material_slots[object_id][material_slot];
  if (state == nullptr) {
    return;
  }
  g_rpc_probe.object_material_slots[object_id][material_slot] = nullptr;
  if (state->textBytes <= g_rpc_probe.object_material_text_bytes) {
    g_rpc_probe.object_material_text_bytes -= state->textBytes;
  } else {
    g_rpc_probe.object_material_text_bytes = 0U;
  }
  free_object_material_state(state);
}

void release_object_material_states(unsigned short object_id, const char *reason) {
  unsigned int released = 0U;
  unsigned int released_text_bytes = 0U;

  if (!object_id_valid(object_id)) {
    return;
  }
  for (unsigned int i = 0U; i < SAMP_RAKNET_OBJECT_MATERIAL_SLOTS; ++i) {
    ObjectMaterialSlotState *state = g_rpc_probe.object_material_slots[object_id][i];
    if (state == nullptr) {
      continue;
    }
    released_text_bytes += state->textBytes;
    release_object_material_slot_state(object_id, i);
    ++released;
  }
  if (released != 0U) {
    trace_netf("object-material-state: release id=%u generation=%u slots=%u text_bytes=%u total_text_bytes=%u "
               "reason=%s",
               static_cast<unsigned int>(object_id), g_rpc_probe.object_generations[object_id], released,
               released_text_bytes, g_rpc_probe.object_material_text_bytes, reason != nullptr ? reason : "unknown");
  }
}

void release_all_object_material_states(const char *reason) {
  for (unsigned int object_id = 0U; object_id < SAMP_RAKNET_MAX_OBJECTS; ++object_id) {
    release_object_material_states(static_cast<unsigned short>(object_id), reason);
  }
  g_rpc_probe.object_material_text_bytes = 0U;
  std::memset(g_rpc_probe.object_material_slots, 0, sizeof(g_rpc_probe.object_material_slots));
}

ObjectMaterialSlotState *create_object_material_state(const samp_raknet_object_material *material,
                                                      unsigned int generation, unsigned int revision) {
  ObjectMaterialSlotState *state = nullptr;

  if (material == nullptr || generation == 0U || revision == 0U ||
      material->slot >= SAMP_RAKNET_OBJECT_MATERIAL_SLOTS ||
      (material->type != SAMP_RAKNET_OBJECT_MATERIAL_TYPE_DEFAULT &&
       material->type != SAMP_RAKNET_OBJECT_MATERIAL_TYPE_TEXT)) {
    return nullptr;
  }
  state = static_cast<ObjectMaterialSlotState *>(std::calloc(1U, sizeof(*state)));
  if (state == nullptr) {
    return nullptr;
  }
  state->generation = generation;
  state->revision = revision;
  state->materialColor = material->material_color;
  state->backgroundColor = material->background_color;
  state->model = material->model;
  state->type = material->type;
  state->slot = material->slot;
  state->source = material->source;
  state->materialSize = material->material_size;
  state->fontSize = material->font_size;
  state->bold = material->bold;
  state->alignment = material->alignment;

  if (material->type == SAMP_RAKNET_OBJECT_MATERIAL_TYPE_DEFAULT) {
    copy_object_material_name(state->nameA, sizeof(state->nameA), material->txd);
    copy_object_material_name(state->nameB, sizeof(state->nameB), material->texture);
  } else {
    state->textBytes = object_material_text_size(material->text);
    state->text = static_cast<char *>(std::malloc(state->textBytes));
    if (state->text == nullptr) {
      free_object_material_state(state);
      return nullptr;
    }
    copy_object_material_name(state->nameA, sizeof(state->nameA), material->font);
    std::memcpy(state->text, material->text, state->textBytes - 1U);
    state->text[state->textBytes - 1U] = '\0';
  }
  return state;
}

unsigned int object_material_text_bytes_for_object(unsigned short object_id) {
  unsigned int bytes = 0U;

  if (!object_id_valid(object_id)) {
    return 0U;
  }
  for (unsigned int i = 0U; i < SAMP_RAKNET_OBJECT_MATERIAL_SLOTS; ++i) {
    const ObjectMaterialSlotState *state = g_rpc_probe.object_material_slots[object_id][i];
    if (state != nullptr) {
      bytes += state->textBytes;
    }
  }
  return bytes;
}

void trace_object_material_state_persisted(unsigned short object_id, const ObjectMaterialSlotState *state) {
  if (state == nullptr) {
    return;
  }
  trace_netf("object-material-state: persist id=%u generation=%u revision=%u source=%s type=%u slot=%u "
             "model=%d name_a='%s' name_b='%s' color_wire=0x%08x text_bytes=%u total_text_bytes=%u "
             "evidence=STATIC_037,OPENMP_REF,PROBE_TRACE",
             static_cast<unsigned int>(object_id), state->generation, state->revision,
             state->source == SAMP_RAKNET_OBJECT_MATERIAL_SOURCE_CREATE ? "create" : "rpc84",
             static_cast<unsigned int>(state->type), static_cast<unsigned int>(state->slot),
             static_cast<int>(state->model), state->nameA, state->nameB, state->materialColor, state->textBytes,
             g_rpc_probe.object_material_text_bytes);
}

bool replace_create_object_material_states(unsigned short object_id, unsigned int generation,
                                           const samp_raknet_object_material *materials,
                                           unsigned int materials_count) {
  ObjectMaterialSlotState *replacement[SAMP_RAKNET_OBJECT_MATERIAL_SLOTS] = {};
  unsigned int retained_text_bytes = g_rpc_probe.object_material_text_bytes;
  unsigned int replacement_text_bytes = 0U;
  unsigned int old_text_bytes = object_material_text_bytes_for_object(object_id);
  bool ok = false;

  if (!object_id_valid(object_id) || generation == 0U ||
      materials_count > SAMP_RAKNET_OBJECT_MATERIAL_SLOTS ||
      (materials_count != 0U && materials == nullptr)) {
    return false;
  }
  if (old_text_bytes <= retained_text_bytes) {
    retained_text_bytes -= old_text_bytes;
  } else {
    retained_text_bytes = 0U;
  }

  for (unsigned int i = 0U; i < materials_count; ++i) {
    const unsigned int revision = bump_object_material_counter(&g_rpc_probe.object_material_revision_seq);
    ObjectMaterialSlotState *state = create_object_material_state(&materials[i], generation, revision);
    const unsigned int slot = materials[i].slot;

    if (state == nullptr || slot >= SAMP_RAKNET_OBJECT_MATERIAL_SLOTS) {
      free_object_material_state(state);
      trace_netf("object-material-state: create_store_failed id=%u generation=%u index=%u count=%u "
                 "reason=allocation_or_invalid",
                 static_cast<unsigned int>(object_id), generation, i, materials_count);
      goto cleanup;
    }
    if (replacement[slot] != nullptr) {
      replacement_text_bytes -= replacement[slot]->textBytes;
      free_object_material_state(replacement[slot]);
      replacement[slot] = nullptr;
    }
    if (retained_text_bytes > kObjectMaterialTextBudgetBytes ||
        replacement_text_bytes + state->textBytes > kObjectMaterialTextBudgetBytes - retained_text_bytes) {
      trace_netf("object-material-state: create_store_failed id=%u generation=%u index=%u count=%u "
                 "reason=text_budget retained=%u replacement=%u candidate=%u budget=%u",
                 static_cast<unsigned int>(object_id), generation, i, materials_count, retained_text_bytes,
                 replacement_text_bytes, state->textBytes, kObjectMaterialTextBudgetBytes);
      free_object_material_state(state);
      goto cleanup;
    }
    replacement_text_bytes += state->textBytes;
    replacement[slot] = state;
  }

  release_object_material_states(object_id, "create_replace");
  g_rpc_probe.object_material_text_bytes = retained_text_bytes;
  for (unsigned int i = 0U; i < SAMP_RAKNET_OBJECT_MATERIAL_SLOTS; ++i) {
    if (replacement[i] == nullptr) {
      continue;
    }
    g_rpc_probe.object_material_slots[object_id][i] = replacement[i];
    replacement[i] = nullptr;
    g_rpc_probe.object_material_text_bytes += g_rpc_probe.object_material_slots[object_id][i]->textBytes;
    trace_object_material_state_persisted(object_id, g_rpc_probe.object_material_slots[object_id][i]);
  }
  ok = true;

cleanup:
  for (unsigned int i = 0U; i < SAMP_RAKNET_OBJECT_MATERIAL_SLOTS; ++i) {
    free_object_material_state(replacement[i]);
  }
  return ok;
}

bool store_object_material_state(unsigned short object_id, const samp_raknet_object_material *material,
                                 unsigned int *out_revision) {
  ObjectMaterialSlotState *state = nullptr;
  ObjectMaterialSlotState *previous = nullptr;
  unsigned int retained_text_bytes = g_rpc_probe.object_material_text_bytes;
  unsigned int generation = 0U;
  unsigned int revision = 0U;

  if (out_revision != nullptr) {
    *out_revision = 0U;
  }
  if (!object_id_valid(object_id) || material == nullptr ||
      material->slot >= SAMP_RAKNET_OBJECT_MATERIAL_SLOTS) {
    return false;
  }
  generation = g_rpc_probe.object_generations[object_id];
  if (generation == 0U) {
    trace_netf("object-material-state: orphan id=%u source=rpc84 type=%u slot=%u dropped=1",
               static_cast<unsigned int>(object_id), static_cast<unsigned int>(material->type),
               static_cast<unsigned int>(material->slot));
    return false;
  }
  previous = g_rpc_probe.object_material_slots[object_id][material->slot];
  if (previous != nullptr) {
    if (previous->textBytes <= retained_text_bytes) {
      retained_text_bytes -= previous->textBytes;
    } else {
      retained_text_bytes = 0U;
    }
  }
  revision = bump_object_material_counter(&g_rpc_probe.object_material_revision_seq);
  state = create_object_material_state(material, generation, revision);
  if (state == nullptr) {
    trace_netf("object-material-state: update_store_failed id=%u generation=%u slot=%u reason=allocation_or_invalid",
               static_cast<unsigned int>(object_id), generation, static_cast<unsigned int>(material->slot));
    return false;
  }
  if (retained_text_bytes > kObjectMaterialTextBudgetBytes ||
      state->textBytes > kObjectMaterialTextBudgetBytes - retained_text_bytes) {
    trace_netf("object-material-state: update_store_failed id=%u generation=%u slot=%u reason=text_budget "
               "retained=%u candidate=%u budget=%u",
               static_cast<unsigned int>(object_id), generation, static_cast<unsigned int>(material->slot),
               retained_text_bytes, state->textBytes, kObjectMaterialTextBudgetBytes);
    free_object_material_state(state);
    return false;
  }

  release_object_material_slot_state(object_id, material->slot);
  g_rpc_probe.object_material_text_bytes = retained_text_bytes + state->textBytes;
  g_rpc_probe.object_material_slots[object_id][material->slot] = state;
  trace_object_material_state_persisted(object_id, state);
  if (out_revision != nullptr) {
    *out_revision = revision;
  }
  return true;
}

bool read_object_material_dynstr8(RakNet::BitStream &bs, char *out, std::size_t out_size) {
  unsigned char length = 0U;
  std::size_t copy_length = 0U;
  char value[256] = {0};

  if (out == nullptr || out_size == 0U || !bs.Read(length)) {
    return false;
  }
  if (length != 0U && !bs.Read(value, static_cast<int>(length))) {
    return false;
  }
  value[length] = '\0';
  copy_length = static_cast<std::size_t>(length);
  if (copy_length >= out_size) {
    copy_length = out_size - 1U;
  }
  if (copy_length != 0U) {
    std::memcpy(out, value, copy_length);
  }
  out[copy_length] = '\0';
  return true;
}

bool decode_object_material_record(RakNet::BitStream &bs, unsigned char source,
                                   samp_raknet_object_material *out_material) {
  unsigned char type = 0U;
  unsigned char slot = 0U;
  unsigned short wire_model = 0U;
  RakNet::StringCompressor *compressor = RakNet::StringCompressor::Instance();

  if (out_material == nullptr || !bs.Read(type) || !bs.Read(slot) ||
      slot >= SAMP_RAKNET_OBJECT_MATERIAL_SLOTS ||
      (type != SAMP_RAKNET_OBJECT_MATERIAL_TYPE_DEFAULT && type != SAMP_RAKNET_OBJECT_MATERIAL_TYPE_TEXT)) {
    return false;
  }

  std::memset(out_material, 0, sizeof(*out_material));
  out_material->type = type;
  out_material->slot = slot;
  out_material->source = source;

  if (type == SAMP_RAKNET_OBJECT_MATERIAL_TYPE_DEFAULT) {
    if (!bs.Read(wire_model) ||
        !read_object_material_dynstr8(bs, out_material->txd, sizeof(out_material->txd)) ||
        !read_object_material_dynstr8(bs, out_material->texture, sizeof(out_material->texture)) ||
        !bs.Read(out_material->material_color)) {
      return false;
    }
    /* STATIC_037:
     * samp.dll+0x1B6A0 maps standalone 0xFFFF to -1. The embedded helper at
     * samp.dll+0x1B070 additionally rejects out-of-range (>20000) source IDs.
     */
    if (wire_model == 0xFFFFU ||
        (source == SAMP_RAKNET_OBJECT_MATERIAL_SOURCE_CREATE && wire_model > kGtaModelInfoCount)) {
      out_material->model = -1;
    } else {
      out_material->model = static_cast<std::int32_t>(wire_model);
    }
    return true;
  }

  if (compressor == nullptr || !bs.Read(out_material->material_size) ||
      !read_object_material_dynstr8(bs, out_material->font, sizeof(out_material->font)) ||
      !bs.Read(out_material->font_size) || !bs.Read(out_material->bold) ||
      !bs.Read(out_material->material_color) || !bs.Read(out_material->background_color) ||
      !bs.Read(out_material->alignment) ||
      !compressor->DecodeString(out_material->text, static_cast<int>(sizeof(out_material->text)), &bs)) {
    return false;
  }
  return true;
}

samp_raknet_object_event *queue_object_material_event(unsigned short object_id,
                                                       const samp_raknet_object_material *material,
                                                       unsigned int material_revision) {
  samp_raknet_object_event *event = nullptr;

  if (material == nullptr || !object_id_valid(object_id) ||
      material->slot >= SAMP_RAKNET_OBJECT_MATERIAL_SLOTS || material_revision == 0U) {
    return nullptr;
  }
  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_SET_MATERIAL, object_id);
  event->material_revision = material_revision;
  event->material_type = material->type;
  event->material_slot = material->slot;
  event->material_source = material->source;
  event->materials_count = 1U;
  trace_netf("object-material-decode: queue_compact seq=%u generation=%u revision=%u id=%u source=%s "
             "type=%u slot=%u evidence=STATIC_037,OPENMP_REF,PROBE_TRACE",
             event->seq, event->object_generation, event->material_revision,
             static_cast<unsigned int>(object_id),
             material->source == SAMP_RAKNET_OBJECT_MATERIAL_SOURCE_CREATE ? "create" : "rpc84",
             static_cast<unsigned int>(material->type), static_cast<unsigned int>(material->slot));
  return event;
}

bool decode_object_create_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  unsigned short attach_vehicle_id = kOpenMpInvalidVehicleId;
  unsigned short attach_object_id = kOpenMpInvalidObjectId;
  unsigned int attachment_type = SAMP_RAKNET_OBJECT_ATTACH_NONE;
  unsigned short attachment_parent_id = kOpenMpInvalidObjectId;
  unsigned char attachment_sync_rotation = 1U;
  unsigned int material_count_offset = kOpenMpObjectCreateMaterialCountOffset;
  unsigned int materials_count = 0U;
  unsigned int object_generation = 0U;
  samp_raknet_object_material *materials = nullptr;
  bool material_scratch_available = true;
  std::int32_t model = 0;
  float draw_distance = 0.0f;
  float pos[3] = {0.0f, 0.0f, 0.0f};
  float rot[3] = {0.0f, 0.0f, 0.0f};
  float attachment_offset[3] = {0.0f, 0.0f, 0.0f};
  float attachment_rot[3] = {0.0f, 0.0f, 0.0f};
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
    if (attach_vehicle_id != kOpenMpInvalidVehicleId) {
      attachment_type = SAMP_RAKNET_OBJECT_ATTACH_VEHICLE;
      attachment_parent_id = attach_vehicle_id;
    } else {
      attachment_type = SAMP_RAKNET_OBJECT_ATTACH_OBJECT;
      attachment_parent_id = attach_object_id;
    }
    read_vec3(data + kOpenMpObjectCreateAttachOffsetOffset, attachment_offset);
    read_vec3(data + kOpenMpObjectCreateAttachRotOffset, attachment_rot);
    attachment_sync_rotation = data[kOpenMpObjectCreateAttachSyncOffset] != 0U ? 1U : 0U;
    if (!object_vec_plausible(attachment_offset) || !object_rot_plausible(attachment_rot)) {
      trace_netf("object-decode: create invalid_attachment_transform id=%u bytes=%u type=%u parent=%u offset=%.3f %.3f %.3f rot=%.3f %.3f %.3f",
                 static_cast<unsigned int>(object_id), bytes, attachment_type,
                 static_cast<unsigned int>(attachment_parent_id), static_cast<double>(attachment_offset[0]),
                 static_cast<double>(attachment_offset[1]), static_cast<double>(attachment_offset[2]),
                 static_cast<double>(attachment_rot[0]), static_cast<double>(attachment_rot[1]),
                 static_cast<double>(attachment_rot[2]));
      return false;
    }
  }
  materials_count = static_cast<unsigned int>(data[material_count_offset]);
  if (materials_count > SAMP_RAKNET_OBJECT_MATERIAL_SLOTS) {
    trace_netf("object-decode: create invalid_material_count id=%u bytes=%u count=%u max=%u",
               static_cast<unsigned int>(object_id), bytes, materials_count,
               static_cast<unsigned int>(SAMP_RAKNET_OBJECT_MATERIAL_SLOTS));
    return false;
  }
  if (materials_count != 0U) {
    const unsigned int material_data_offset = material_count_offset + 1U;
    materials = static_cast<samp_raknet_object_material *>(
        std::calloc(materials_count, sizeof(samp_raknet_object_material)));
    if (materials == nullptr) {
      trace_netf("object-decode: create material_allocation_failed id=%u bytes=%u count=%u bytes_requested=%u",
                 static_cast<unsigned int>(object_id), bytes, materials_count,
                 materials_count * static_cast<unsigned int>(sizeof(samp_raknet_object_material)));
      material_scratch_available = false;
    } else {
      RakNet::BitStream material_bs(const_cast<unsigned char *>(data + material_data_offset),
                                    bytes - material_data_offset, false);
      for (unsigned int i = 0U; i < materials_count; ++i) {
        if (!decode_object_material_record(material_bs, SAMP_RAKNET_OBJECT_MATERIAL_SOURCE_CREATE,
                                           &materials[i])) {
          trace_netf("object-decode: create invalid_material id=%u bytes=%u index=%u count=%u tail_bytes=%u",
                     static_cast<unsigned int>(object_id), bytes, i, materials_count,
                     bytes - material_data_offset);
          std::free(materials);
          return false;
        }
      }
    }
  }
  if (!object_id_valid(object_id) || !object_model_plausible(model) || !object_vec_plausible(pos) ||
      !object_rot_plausible(rot) || !std::isfinite(draw_distance) || draw_distance < 0.0f ||
      draw_distance > 10000.0f) {
    trace_netf("object-decode: create invalid id=%u model=%d bytes=%u draw=%.3f pos=%.3f %.3f %.3f rot=%.3f %.3f %.3f",
               static_cast<unsigned int>(object_id), static_cast<int>(model), bytes,
               static_cast<double>(draw_distance), static_cast<double>(pos[0]), static_cast<double>(pos[1]),
               static_cast<double>(pos[2]),
               static_cast<double>(rot[0]), static_cast<double>(rot[1]), static_cast<double>(rot[2]));
    std::free(materials);
    return false;
  }

  object_generation = bump_object_material_counter(&g_rpc_probe.object_generation_seq);
  if (!material_scratch_available ||
      !replace_create_object_material_states(object_id, object_generation, materials, materials_count)) {
    /* Network object lifetime is authoritative even if optional material state
     * cannot be retained. Never turn a bounded material allocation failure into
     * a missing object or a stale previous generation.
     */
    release_object_material_states(object_id, "create_material_fail_soft");
    trace_netf("object-decode: create material_store_failed id=%u generation=%u count=%u "
               "object_continues=1 materials_dropped=1",
               static_cast<unsigned int>(object_id), object_generation, materials_count);
  }
  g_rpc_probe.object_generations[object_id] = object_generation;
  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_CREATE, object_id);
  event->object_generation = object_generation;
  event->model = model;
  std::memcpy(event->pos, pos, sizeof(event->pos));
  std::memcpy(event->rot, rot, sizeof(event->rot));
  event->has_pos = 1U;
  event->has_attachment = has_attachment ? 1U : 0U;
  event->attachment_type = static_cast<unsigned char>(attachment_type);
  event->attachment_parent_id = attachment_parent_id;
  event->attachment_sync_rotation = attachment_sync_rotation;
  std::memcpy(event->attachment_offset, attachment_offset, sizeof(event->attachment_offset));
  std::memcpy(event->attachment_rot, attachment_rot, sizeof(event->attachment_rot));
  event->materials_count = static_cast<unsigned char>(materials_count);
  event->draw_distance = draw_distance;
  g_rpc_probe.object_alive[object_id] = 1U;
  g_rpc_probe.object_create_states[object_id] = *event;
  trace_netf("object-decode: create seq=%u id=%u model=%d draw=%.3f attach=%u type=%u parent=%u sync=%u materials=%u pos=%.3f %.3f %.3f rot=%.3f %.3f %.3f attach_offset=%.3f %.3f %.3f attach_rot=%.3f %.3f %.3f",
             event->seq, static_cast<unsigned int>(object_id), static_cast<int>(model),
             static_cast<double>(draw_distance), has_attachment ? 1U : 0U, attachment_type,
             static_cast<unsigned int>(attachment_parent_id), static_cast<unsigned int>(attachment_sync_rotation),
             materials_count,
             static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]),
             static_cast<double>(rot[0]), static_cast<double>(rot[1]), static_cast<double>(rot[2]),
             static_cast<double>(attachment_offset[0]), static_cast<double>(attachment_offset[1]),
             static_cast<double>(attachment_offset[2]), static_cast<double>(attachment_rot[0]),
             static_cast<double>(attachment_rot[1]), static_cast<double>(attachment_rot[2]));
  std::free(materials);
  return true;
}

bool decode_object_attach_player_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short object_id = 0U;
  unsigned short player_id = 0U;
  float offset[3] = {0.0f, 0.0f, 0.0f};
  float rot[3] = {0.0f, 0.0f, 0.0f};
  samp_raknet_object_event *event = nullptr;

  if (data == nullptr || bytes < kOpenMpAttachObjectToPlayerMinBytes) {
    return false;
  }
  object_id = read_le16(data);
  player_id = read_le16(data + 2U);
  read_vec3(data + 4U, offset);
  read_vec3(data + 16U, rot);
  if (!object_id_valid(object_id) || !object_vec_plausible(offset) || !object_rot_plausible(rot)) {
    trace_netf("object-decode: attach_player invalid object=%u player=%u bytes=%u offset=%.3f %.3f %.3f rot=%.3f %.3f %.3f",
               static_cast<unsigned int>(object_id), static_cast<unsigned int>(player_id), bytes,
               static_cast<double>(offset[0]), static_cast<double>(offset[1]), static_cast<double>(offset[2]),
               static_cast<double>(rot[0]), static_cast<double>(rot[1]), static_cast<double>(rot[2]));
    return false;
  }

  event = queue_object_event(SAMP_RAKNET_OBJECT_ACTION_ATTACH_PLAYER, object_id);
  event->has_attachment = 1U;
  event->attachment_type = SAMP_RAKNET_OBJECT_ATTACH_PLAYER;
  event->attachment_parent_id = player_id;
  event->attachment_sync_rotation = 1U;
  std::memcpy(event->attachment_offset, offset, sizeof(event->attachment_offset));
  std::memcpy(event->attachment_rot, rot, sizeof(event->attachment_rot));
  trace_netf("object-decode: attach_player seq=%u object=%u player=%u offset=%.3f %.3f %.3f rot=%.3f %.3f %.3f",
             event->seq, static_cast<unsigned int>(object_id), static_cast<unsigned int>(player_id),
             static_cast<double>(offset[0]), static_cast<double>(offset[1]), static_cast<double>(offset[2]),
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
  trace_netf("object-decode: destroy seq=%u generation=%u id=%u", event->seq, event->object_generation,
             static_cast<unsigned int>(object_id));
  release_object_material_states(object_id, "destroy_rpc");
  g_rpc_probe.object_generations[object_id] = 0U;
  g_rpc_probe.object_alive[object_id] = 0U;
  std::memset(&g_rpc_probe.object_create_states[object_id], 0,
              sizeof(g_rpc_probe.object_create_states[object_id]));
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

bool decode_vehicle_position_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 14U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  float pos[3];
  read_vec3(data + 2U, pos);
  if (!vehicle_id_valid(vehicle_id) || !vehicle_vec_plausible(pos)) {
    return false;
  }
  samp_raknet_vehicle_event *event = queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_SET_POS, vehicle_id);
  std::memcpy(event->pos, pos, sizeof(pos));
  trace_netf("vehicle-decode: set_pos seq=%u id=%u pos=%.3f %.3f %.3f evidence=STATIC_037",
             event->seq, static_cast<unsigned int>(vehicle_id), static_cast<double>(pos[0]),
             static_cast<double>(pos[1]), static_cast<double>(pos[2]));
  return true;
}

bool decode_vehicle_z_angle_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 6U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  const float angle = read_le_float(data + 2U);
  if (!vehicle_id_valid(vehicle_id) || !vehicle_rotation_plausible(angle)) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_SET_Z_ANGLE, vehicle_id);
  event->rotation = angle;
  trace_netf("vehicle-decode: set_z_angle seq=%u id=%u angle=%.3f evidence=STATIC_037",
             event->seq, static_cast<unsigned int>(vehicle_id), static_cast<double>(angle));
  return true;
}

bool decode_attach_trailer_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) {
    return false;
  }
  const unsigned short trailer_id = read_le16(data);
  const unsigned short vehicle_id = read_le16(data + 2U);
  if (!vehicle_id_valid(trailer_id) || !vehicle_id_valid(vehicle_id) || trailer_id == vehicle_id) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_ATTACH_TRAILER, vehicle_id);
  event->related_vehicle_id = trailer_id;
  trace_netf("vehicle-decode: attach_trailer seq=%u vehicle=%u trailer=%u evidence=STATIC_037",
             event->seq, static_cast<unsigned int>(vehicle_id), static_cast<unsigned int>(trailer_id));
  return true;
}

bool decode_detach_trailer_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 2U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  if (!vehicle_id_valid(vehicle_id)) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_DETACH_TRAILER, vehicle_id);
  trace_netf("vehicle-decode: detach_trailer seq=%u vehicle=%u evidence=STATIC_037",
             event->seq, static_cast<unsigned int>(vehicle_id));
  return true;
}

bool decode_vehicle_params_ex_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 18U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  if (!vehicle_id_valid(vehicle_id)) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_SET_PARAMS_EX, vehicle_id);
  std::memcpy(event->params, data + 2U, SAMP_RAKNET_VEHICLE_PARAM_BYTES);
  trace_netf("vehicle-decode: params_ex seq=%u id=%u engine=%u lights=%u alarm=%u doors=%u "
             "bonnet=%u boot=%u objective=%u siren=%u evidence=STATIC_037",
             event->seq, static_cast<unsigned int>(vehicle_id), static_cast<unsigned int>(event->params[0]),
             static_cast<unsigned int>(event->params[1]), static_cast<unsigned int>(event->params[2]),
             static_cast<unsigned int>(event->params[3]), static_cast<unsigned int>(event->params[4]),
             static_cast<unsigned int>(event->params[5]), static_cast<unsigned int>(event->params[6]),
             static_cast<unsigned int>(event->params[7]));
  return true;
}

bool decode_remove_vehicle_component_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 6U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  const std::int32_t component = read_le_i32(data + 2U);
  if (!vehicle_id_valid(vehicle_id) || component < 1000 || component > 1193) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_REMOVE_COMPONENT, vehicle_id);
  event->component = component;
  trace_netf("vehicle-decode: remove_component seq=%u id=%u component=%d evidence=STATIC_037", event->seq,
             static_cast<unsigned int>(vehicle_id), static_cast<int>(component));
  return true;
}

bool decode_link_vehicle_interior_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 3U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  if (!vehicle_id_valid(vehicle_id)) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_LINK_INTERIOR, vehicle_id);
  event->interior = data[2U];
  trace_netf("vehicle-decode: link_interior seq=%u id=%u interior=%u evidence=STATIC_037", event->seq,
             static_cast<unsigned int>(vehicle_id), static_cast<unsigned int>(event->interior));
  return true;
}

bool decode_vehicle_number_plate_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 3U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  const unsigned int length = data[2U];
  if (!vehicle_id_valid(vehicle_id) || length > 32U || bytes < 3U + length) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_SET_NUMBER_PLATE, vehicle_id);
  if (length != 0U) {
    std::memcpy(event->number_plate, data + 3U, length);
  }
  event->number_plate[length] = '\0';
  trace_netf("vehicle-decode: number_plate seq=%u id=%u length=%u text=%s evidence=STATIC_037", event->seq,
             static_cast<unsigned int>(vehicle_id), length, event->number_plate);
  return true;
}

bool decode_vehicle_params_for_player_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) {
    return false;
  }
  const unsigned short vehicle_id = read_le16(data);
  if (!vehicle_id_valid(vehicle_id)) {
    return false;
  }
  samp_raknet_vehicle_event *event =
      queue_vehicle_event(SAMP_RAKNET_VEHICLE_ACTION_SET_PARAMS_FOR_PLAYER, vehicle_id);
  event->params[0] = data[2U];
  event->params[1] = data[3U];
  trace_netf("vehicle-decode: params_for_player seq=%u id=%u objective=%u doors=%u evidence=STATIC_037",
             event->seq, static_cast<unsigned int>(vehicle_id), static_cast<unsigned int>(event->params[0]),
             static_cast<unsigned int>(event->params[1]));
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
  unsigned int spawns_available = 0U;
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
  const char *layout = "samp037";

  if (data == nullptr || bytes == 0U) {
    return false;
  }

  {
    bool manual_vehicle_engine_lights = false;
    std::uint32_t spawn_info_count = 0U;
    std::uint32_t player_markers = 0U;
    std::uint32_t death_drop_amount = 0U;
    std::uint32_t onfoot_rate = 0U;
    std::uint32_t incar_rate = 0U;
    std::uint32_t weapon_rate = 0U;
    std::uint32_t multiplier = 0U;
    std::uint32_t lag_compensation = 0U;
    std::uint32_t vehicle_friendly_fire = 0U;
    char openmp_hostname[SAMP_RAKNET_HOSTNAME_BYTES] = {0};
    unsigned char openmp_vehicle_models[SAMP_RAKNET_REQUIRED_VEHICLE_MODELS] = {0};
    RakNet::BitStream openmp_bs(const_cast<unsigned char *>(data), bytes, false);

    /* OPENMP_REF + PROBE_TRACE:
     * open.mp's RPC 139 PlayerInit layout starts with zone/CJ/interior/chat flags and
     * sends SpawnInfoCount/PlayerID later. The 2026-06-10 bare run decoded with the
     * old sequential order shifted every following field: player=33796, tags=0,
     * nametag_dist=0, gravity garbage.
     */
    if (openmp_bs.Read(zone_names) && openmp_bs.Read(use_cj_walk) && openmp_bs.Read(allow_weapons) &&
        openmp_bs.Read(limit_global_chat_radius) && openmp_bs.Read(global_chat_radius) &&
        openmp_bs.Read(stunt_bonus) && openmp_bs.Read(name_tag_draw_distance) &&
        openmp_bs.Read(disable_enter_exits) && openmp_bs.Read(name_tag_los) &&
        openmp_bs.Read(manual_vehicle_engine_lights) && openmp_bs.Read(spawn_info_count) &&
        openmp_bs.Read(local_player_id) && openmp_bs.Read(show_player_tags) && openmp_bs.Read(player_markers) &&
        openmp_bs.Read(world_time) && openmp_bs.Read(weather) && openmp_bs.Read(gravity) &&
        openmp_bs.Read(lan_mode) && openmp_bs.Read(death_drop_amount) && openmp_bs.Read(instagib) &&
        openmp_bs.Read(onfoot_rate) && openmp_bs.Read(incar_rate) && openmp_bs.Read(weapon_rate) &&
        openmp_bs.Read(multiplier) && openmp_bs.Read(lag_compensation) && openmp_bs.Read(hostname_len) &&
        hostname_len < SAMP_RAKNET_HOSTNAME_BYTES &&
        (hostname_len == 0U || openmp_bs.Read(openmp_hostname, static_cast<int>(hostname_len))) &&
        openmp_bs.Read(reinterpret_cast<char *>(openmp_vehicle_models), SAMP_RAKNET_REQUIRED_VEHICLE_MODELS) &&
        openmp_bs.Read(vehicle_friendly_fire) && spawn_info_count < 65536U &&
        static_cast<unsigned int>(local_player_id) < SAMP_RAKNET_MAX_PLAYERS && world_time <= 23U &&
        std::isfinite(gravity) && std::isfinite(global_chat_radius) && std::isfinite(name_tag_draw_distance) &&
        gravity > -10.0f && gravity < 10.0f && global_chat_radius >= 0.0f &&
        name_tag_draw_distance >= 0.0f && name_tag_draw_distance <= 10000.0f) {
      spawns_available = spawn_info_count;
      show_player_markers = player_markers != 0U;
      death_drop_money = static_cast<std::int32_t>(death_drop_amount);
      send_rates[0] = static_cast<std::int32_t>(onfoot_rate);
      send_rates[1] = static_cast<std::int32_t>(onfoot_rate);
      send_rates[2] = static_cast<std::int32_t>(incar_rate);
      send_rates[3] = static_cast<std::int32_t>(incar_rate);
      send_rates[4] = static_cast<std::int32_t>(weapon_rate);
      send_rates[5] = static_cast<std::int32_t>(multiplier);
      std::memcpy(hostname, openmp_hostname, sizeof(hostname));
      std::memcpy(vehicle_models, openmp_vehicle_models, sizeof(vehicle_models));
      layout = "openmp";
      (void)manual_vehicle_engine_lights;
      (void)lag_compensation;
      (void)vehicle_friendly_fire;
    }
  }

  if (std::strcmp(layout, "openmp") != 0) {
    std::int32_t spawns_available_i = 0;
    RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);

    /* STATIC_037 + PROBE_TRACE:
     * The sequential 0.3.7 client path reads the first field as NetGame's int
     * spawn count. Keep this as a fallback for non-open.mp servers.
     */
    if (!bs.Read(spawns_available_i) || !bs.Read(local_player_id) || !bs.Read(show_player_tags) ||
        !bs.Read(show_player_markers) || !bs.Read(tire_popping) || !bs.Read(world_time) || !bs.Read(weather) ||
        !bs.Read(gravity) || !bs.Read(lan_mode) || !bs.Read(death_drop_money) || !bs.Read(instagib) ||
        !bs.Read(zone_names) || !bs.Read(use_cj_walk) || !bs.Read(allow_weapons) ||
        !bs.Read(limit_global_chat_radius) || !bs.Read(global_chat_radius) || !bs.Read(stunt_bonus) ||
        !bs.Read(name_tag_draw_distance) || !bs.Read(disable_enter_exits) || !bs.Read(name_tag_los)) {
      return false;
    }

    if (spawns_available_i < 0 || spawns_available_i >= 65536 || world_time > 23U || !std::isfinite(gravity) ||
        !std::isfinite(global_chat_radius) || !std::isfinite(name_tag_draw_distance)) {
      return false;
    }
    spawns_available = static_cast<unsigned int>(spawns_available_i);

    for (unsigned int i = 0U; i < 6U; ++i) {
      if (!bs.Read(send_rates[i])) {
        return false;
      }
    }

    if (!bs.Read(hostname_len) || hostname_len >= SAMP_RAKNET_HOSTNAME_BYTES) {
      return false;
    }
    if (hostname_len > 0U && !bs.Read(hostname, static_cast<int>(hostname_len))) {
      return false;
    }

    if (!bs.Read(reinterpret_cast<char *>(vehicle_models), SAMP_RAKNET_REQUIRED_VEHICLE_MODELS)) {
      return false;
    }
  }
  hostname[SAMP_RAKNET_HOSTNAME_BYTES - 1U] = '\0';

  g_rpc_probe.init_spawns_available = static_cast<unsigned short>(spawns_available);
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

  trace_netf("rpc-state id=139 init_game layout=%s spawns=%u local_player=%u tags=%u markers=%u tire=%u "
             "time=%u weather=%u "
             "gravity=%.6f lan=%u death_drop=%d instagib=%u zones=%u cj=%u weapons=%u chat_limit=%u "
             "chat_radius=%.3f stunt=%u nametag_dist=%.3f enter_exits_disabled=%u los=%u "
             "send_rates=%d/%d/%d/%d/%d/%d host='%s' veh_nonzero=%u veh_total=%u top_vehicle=%u:%u",
             layout, static_cast<unsigned int>(spawns_available), static_cast<unsigned int>(local_player_id),
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
                          unsigned int color, unsigned char style) {
  const unsigned int seq = bump_seq(&g_rpc_probe.map_icon_event_seq);
  const unsigned int slot = (seq - 1U) % SAMP_RAKNET_MAP_ICON_EVENT_RING;
  samp_raknet_map_icon_event *event = &g_rpc_probe.map_icon_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->action = action;
  event->index = index;
  event->icon = icon;
  event->style = style;
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

samp_raknet_death_window_event *queue_death_window_event(unsigned char action, unsigned short killer_id,
                                                         unsigned short killee_id, unsigned char reason) {
  const unsigned int seq = bump_seq(&g_rpc_probe.death_window_event_seq);
  const unsigned int slot = (seq - 1U) % SAMP_RAKNET_DEATH_WINDOW_EVENT_RING;
  samp_raknet_death_window_event *event = &g_rpc_probe.death_window_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->action = action;
  event->killer_id = killer_id;
  event->killee_id = killee_id;
  event->reason = reason;
  return event;
}

samp_raknet_game_text_event *queue_game_text_event(unsigned char action, std::int32_t style, std::int32_t time_ms,
                                                   const char *text) {
  const unsigned int seq = bump_seq(&g_rpc_probe.game_text_event_seq);
  const unsigned int slot = (seq - 1U) % SAMP_RAKNET_GAMETEXT_EVENT_RING;
  samp_raknet_game_text_event *event = &g_rpc_probe.game_text_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->action = action;
  event->style = style;
  event->time_ms = time_ms;
  copy_text(event->text, sizeof(event->text), text);
  return event;
}

void sanitize_3d_text_label_text(char *text) {
  char *base = text;
  size_t len = 0U;

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
  len = std::strlen(base);
  while (len > 0U) {
    const unsigned char ch = static_cast<unsigned char>(base[len - 1U]);
    if (ch != '\0' && ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t') {
      break;
    }
    base[len - 1U] = '\0';
    --len;
  }
}

bool decode_3d_text_label_compressed_tail(const unsigned char *data, unsigned int bytes, unsigned int offset,
                                          char *out, size_t out_size) {
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
  sanitize_3d_text_label_text(out);
  return true;
}

bool decode_3d_text_label_plain_tail(const unsigned char *data, unsigned int bytes, unsigned int offset, char *out,
                                     size_t out_size) {
  unsigned int copy_len = 0U;

  if (data == nullptr || out == nullptr || out_size == 0U || offset > bytes) {
    return false;
  }
  out[0] = '\0';
  copy_len = bytes - offset;
  if (copy_len >= out_size) {
    copy_len = static_cast<unsigned int>(out_size - 1U);
  }
  if (copy_len > 0U) {
    std::memcpy(out, data + offset, copy_len);
  }
  out[copy_len] = '\0';
  sanitize_3d_text_label_text(out);
  return true;
}

bool decode_3d_text_label_tail(const unsigned char *data, unsigned int bytes, unsigned int offset, char *out,
                               size_t out_size) {
  if (out == nullptr || out_size == 0U) {
    return false;
  }
  out[0] = '\0';
  if (decode_3d_text_label_compressed_tail(data, bytes, offset, out, out_size)) {
    return true;
  }
  return decode_3d_text_label_plain_tail(data, bytes, offset, out, out_size);
}

samp_raknet_3d_text_label_event *queue_3d_text_label_event(unsigned char action, unsigned short label_id) {
  const unsigned int seq = bump_seq(&g_rpc_probe.text_label_event_seq);
  const unsigned int slot = (seq - 1U) % SAMP_RAKNET_3D_TEXT_LABEL_EVENT_RING;
  samp_raknet_3d_text_label_event *event = &g_rpc_probe.text_label_events[slot];

  std::memset(event, 0, sizeof(*event));
  event->seq = seq;
  event->action = action;
  event->label_id = label_id;
  event->attached_player_id = 0xFFFFU;
  event->attached_vehicle_id = 0xFFFFU;
  return event;
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

bool decode_death_message_payload(const unsigned char *data, unsigned int bytes) {
  sampdll::net::DeathMessagePayload decoded{};
  unsigned char action = SAMP_RAKNET_DEATH_WINDOW_ACTION_ADD;
  samp_raknet_death_window_event *event = nullptr;

  if (!sampdll::net::decode_death_message(data, bytes, &decoded)) {
    return false;
  }

  /*
   * PROBE_TRACE + OPENMP_REF:
   * samp_net_trace.log, run=libsamp-20260715-killlist-ak47, payload=01 00 00 00 1e.
   * ScrDeathMessage carries uint16 killer, uint16 killee, then uint8 weapon/reason.
   */
  event = queue_death_window_event(action, decoded.killer_id, decoded.killee_id, decoded.reason);
  trace_netf("rpc-state id=55 death_window seq=%u action=%s killer=%u killee=%u reason=%u bytes=%u "
             "evidence=PROBE_TRACE,OPENMP_REF",
             event->seq, action == SAMP_RAKNET_DEATH_WINDOW_ACTION_CLEAR ? "clear" : "add",
             static_cast<unsigned int>(decoded.killer_id), static_cast<unsigned int>(decoded.killee_id),
             static_cast<unsigned int>(decoded.reason), bytes);
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

float yaw_degrees_from_gta_quat(float w, float x, float y, float z) {
  const float siny_cosp = 2.0f * ((w * z) + (x * y));
  const float cosy_cosp = 1.0f - (2.0f * ((y * y) + (z * z)));
  float yaw = std::atan2(siny_cosp, cosy_cosp) * kRadiansToDegrees;

  if (!std::isfinite(yaw)) {
    return 0.0f;
  }
  while (yaw < 0.0f) {
    yaw += 360.0f;
  }
  while (yaw >= 360.0f) {
    yaw -= 360.0f;
  }
  return yaw;
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
  unsigned char weapon_additional = 0U;
  bool has_lr = false;
  bool has_ud = false;
  bool has_surfing = false;
  bool has_animation = false;
  unsigned short animation_id = 0U;
  unsigned short animation_flags = 0U;
  float quat_w = 1.0f;
  float quat_x = 0.0f;
  float quat_y = 0.0f;
  float quat_z = 0.0f;
  samp_raknet_remote_onfoot_sync sync;
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);

  std::memset(&sync, 0, sizeof(sync));
  sync.surfing_vehicle_id = 0xFFFFU;
  if (data == nullptr || bytes < 2U || !bs.Read(packet_id) || packet_id != kPacketPlayerSync ||
      !bs.Read(sync.player_id)) {
    return false;
  }

  /*
   * OPENMP_REF + PROBE_TRACE + TODO_VERIFY:
   * Server -> client on-foot sync is packet 207 plus the open.mp PlayerFootSync
   * write layout: optional LR/UD keys, raw position, GTA quaternion, one
   * compressed health/armour byte, weapon/additional-key byte, special action,
   * compressed velocity, optional surfing and optional animation. The previous
   * float-rotation/axis-flag decode shifted hp/armour/weapon/speed, visible as
   * impossible remote sync traces like hp=0 armour=100 for a living player.
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
      !bs.ReadNormQuat(quat_w, quat_x, quat_y, quat_z) || !bs.Read(health_armour) ||
      !bs.Read(weapon_additional) || !bs.Read(sync.special_action)) {
    return false;
  }
  sync.rotation = yaw_degrees_from_gta_quat(quat_w, quat_x, quat_y, quat_z);
  sync.armour = decode_health_armour_nibble(static_cast<unsigned char>(health_armour & 0x0FU));
  sync.health = decode_health_armour_nibble(static_cast<unsigned char>(health_armour >> 4U));
  sync.current_weapon = static_cast<unsigned char>(weapon_additional & 0x3FU);

  if (!bs.ReadVector(sync.move_speed[0], sync.move_speed[1], sync.move_speed[2])) {
    return false;
  }
  if (!bs.Read(has_surfing)) {
    return false;
  }
  if (has_surfing && (!bs.Read(sync.surfing_vehicle_id) || !bs.Read(sync.surfing_offsets[0]) ||
                      !bs.Read(sync.surfing_offsets[1]) || !bs.Read(sync.surfing_offsets[2]))) {
    return false;
  }
  if (!bs.Read(has_animation)) {
    return false;
  }
  if (has_animation && (!bs.Read(animation_id) || !bs.Read(animation_flags))) {
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

bool set_object_material_event(unsigned short object_id, const samp_raknet_object_material *material) {
  char summary[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES] = {0};
  float pos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  unsigned int material_revision = 0U;

  if (material == nullptr) {
    return false;
  }
  if (!store_object_material_state(object_id, material, &material_revision)) {
    return false;
  }
  pos[0] = static_cast<float>(material->type);
  pos[1] = static_cast<float>(material->slot);
  pos[2] = static_cast<float>(material->font_size);
  pos[3] = static_cast<float>(material->alignment);
  if (material->type == SAMP_RAKNET_OBJECT_MATERIAL_TYPE_DEFAULT) {
    std::snprintf(summary, sizeof(summary), "type=default slot=%u txd='%s' texture='%s'",
                  static_cast<unsigned int>(material->slot), material->txd, material->texture);
  } else {
    std::snprintf(summary, sizeof(summary), "type=text slot=%u size=%u font='%s' bold=%u text='%.96s'",
                  static_cast<unsigned int>(material->slot), static_cast<unsigned int>(material->material_size),
                  material->font, static_cast<unsigned int>(material->bold), material->text);
  }

  const unsigned int seq = bump_seq(&g_rpc_probe.world_visual_event_seq);
  g_rpc_probe.world_visual_event_type = SAMP_RAKNET_WORLD_VISUAL_SET_OBJECT_MATERIAL;
  g_rpc_probe.world_visual_id = object_id;
  g_rpc_probe.world_visual_model = material->model;
  g_rpc_probe.world_visual_color = material->material_color;
  g_rpc_probe.world_visual_material_background = material->background_color;
  std::memcpy(g_rpc_probe.world_visual_pos, pos, sizeof(g_rpc_probe.world_visual_pos));
  g_rpc_probe.world_visual_material_type = material->type;
  g_rpc_probe.world_visual_material_slot = material->slot;
  g_rpc_probe.world_visual_material_size = material->material_size;
  g_rpc_probe.world_visual_material_font_size = material->font_size;
  g_rpc_probe.world_visual_material_bold = material->bold;
  g_rpc_probe.world_visual_material_alignment = material->alignment;
  copy_text(g_rpc_probe.world_visual_text, sizeof(g_rpc_probe.world_visual_text), summary);
  copy_text(g_rpc_probe.world_visual_material_txd, sizeof(g_rpc_probe.world_visual_material_txd), material->txd);
  copy_text(g_rpc_probe.world_visual_material_texture, sizeof(g_rpc_probe.world_visual_material_texture),
            material->texture);
  if (queue_object_material_event(object_id, material, material_revision) == nullptr) {
    trace_netf("object-material-state: compact_event_failed id=%u generation=%u revision=%u slot=%u",
               static_cast<unsigned int>(object_id), g_rpc_probe.object_generations[object_id], material_revision,
               static_cast<unsigned int>(material->slot));
    return false;
  }
  trace_netf("rpc-world-visual: seq=%u type=set_object_material id=%u revision=%u mat_type=%u slot=%u model=%d "
             "color=0x%08x "
             "bg=0x%08x txd='%s' texture='%s' summary='%s' persistent_object_event=1 "
             "evidence=STATIC_037,OPENMP_REF,PROBE_TRACE",
             seq, static_cast<unsigned int>(object_id), material_revision, static_cast<unsigned int>(material->type),
             static_cast<unsigned int>(material->slot), static_cast<int>(material->model), material->material_color,
             material->background_color,
             g_rpc_probe.world_visual_material_txd, g_rpc_probe.world_visual_material_texture,
             g_rpc_probe.world_visual_text);
  return true;
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

bool decode_give_player_money_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) {
    return false;
  }

  /*
   * STATIC_037:
   * SA-MP 0.3.7-R5 samp.dll+0x1A500 reads one signed 32-bit value and passes it
   * to samp.dll+0xA0F70, which invokes GTA script opcode 0x0109.
   * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  g_rpc_probe.give_player_money = static_cast<std::int32_t>(read_le32(data));
  const unsigned int seq = bump_seq(&g_rpc_probe.give_player_money_seq);
  samp_raknet_give_money_event &event =
      g_rpc_probe.give_money_events[(seq - 1U) % SAMP_RAKNET_GIVE_MONEY_EVENT_RING];
  event.seq = seq;
  event.amount = g_rpc_probe.give_player_money;
  trace_netf("rpc-state id=18 give_money_seq=%u amount=%d apply_pending=1 evidence=STATIC_037",
             seq, static_cast<int>(g_rpc_probe.give_player_money));
  return true;
}

bool decode_player_ammo_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 3U) {
    return false;
  }

  /*
   * STATIC_037:
   * SA-MP 0.3.7-R5 samp.dll+0x1AC10 reads an 8-bit weapon ID followed by a
   * 16-bit ammo count. samp.dll+0xB0080 updates the matching CWeapon total-ammo field.
   * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  const unsigned int weapon = data[0U];
  const unsigned int ammo = read_le16(data + 1U);
  if (weapon > 46U) {
    trace_netf("rpc-state id=145 invalid_player_ammo weapon=%u ammo=%u bytes=%u ignored=1 evidence=STATIC_037",
               weapon, ammo, bytes);
    return false;
  }
  g_rpc_probe.player_ammo_weapon = static_cast<unsigned char>(weapon);
  g_rpc_probe.player_ammo = static_cast<unsigned short>(ammo);
  const unsigned int seq = bump_seq(&g_rpc_probe.player_ammo_seq);
  trace_netf("rpc-state id=145 player_ammo_seq=%u weapon=%u ammo=%u apply_pending=1 evidence=STATIC_037",
             seq, weapon, ammo);
  return true;
}

bool decode_player_skin_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 8U) {
    return false;
  }

  /*
   * STATIC_037 + PROBE_TRACE:
   * SA-MP 0.3.7-R5 samp.dll+0x19190 reads a 32-bit player ID followed by a
   * signed 32-bit model ID, resolves local/remote player state, validates the
   * ped model, then applies it. open.mp run 2026-07-10 observed the same
   * eight-byte layout: 00 00 00 00 1c 01 00 00 for player 0, skin 284.
   * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  const unsigned int player_id = read_le32(data);
  const std::int32_t skin = static_cast<std::int32_t>(read_le32(data + 4U));
  if (player_id >= SAMP_RAKNET_MAX_PLAYERS) {
    trace_netf("rpc-state id=153 invalid_player_skin_target=%u skin=%d bytes=%u ignored=1 evidence=PROBE_TRACE",
               player_id, static_cast<int>(skin), bytes);
    return false;
  }
  if (skin < 0 || skin > 311 || skin == 74) {
    trace_netf("rpc-state id=153 invalid_player_skin target=%u skin=%d bytes=%u ignored=1 evidence=STATIC_037",
               player_id, static_cast<int>(skin), bytes);
    return false;
  }
  g_rpc_probe.player_skin_player_id = player_id;
  g_rpc_probe.player_skin = skin;
  const unsigned int seq = bump_seq(&g_rpc_probe.player_skin_seq);
  trace_netf("rpc-state id=153 player_skin_seq=%u target=%u skin=%d apply_pending=1 evidence=STATIC_037,PROBE_TRACE",
             seq, player_id, static_cast<int>(skin));
  return true;
}

bool decode_player_skill_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 8U) {
    return false;
  }

  /*
   * STATIC_037:
   * SA-MP 0.3.7-R5 samp.dll+0xF5E0 reads uint16 player ID, uint32 skill ID,
   * and uint16 level, then calls samp.dll+0xAE450 on the resolved CPlayerPed.
   * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  const unsigned int player_id = read_le16(data);
  const unsigned int skill = read_le32(data + 2U);
  const unsigned int level = read_le16(data + 6U);
  if (player_id >= SAMP_RAKNET_MAX_PLAYERS || skill > 10U || level > 999U) {
    trace_netf("rpc-state id=34 invalid_player_skill target=%u skill=%u level=%u bytes=%u ignored=1 evidence=STATIC_037",
               player_id, skill, level, bytes);
    return false;
  }
  g_rpc_probe.player_skill_player_id = static_cast<unsigned short>(player_id);
  g_rpc_probe.player_skill = skill;
  g_rpc_probe.player_skill_level = static_cast<unsigned short>(level);
  const unsigned int seq = bump_seq(&g_rpc_probe.player_skill_seq);
  trace_netf("rpc-state id=34 player_skill_seq=%u target=%u skill=%u level=%u apply_pending=1 evidence=STATIC_037",
             seq, player_id, skill, level);
  return true;
}

bool decode_player_drunk_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) {
    return false;
  }

  /*
   * STATIC_037:
   * SA-MP 0.3.7-R5 samp.dll+0x18DB0 reads one uint32 level and stores it via
   * samp.dll+0xADFB0 in the local CPlayerPed compatibility wrapper.
   * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  const unsigned int level = read_le32(data);
  if (level > 50000U) {
    trace_netf("rpc-state id=35 invalid_player_drunk level=%u bytes=%u ignored=1 evidence=STATIC_037,OPENMP_REF",
               level, bytes);
    return false;
  }
  g_rpc_probe.player_drunk_level = level;
  const unsigned int seq = bump_seq(&g_rpc_probe.player_drunk_seq);
  trace_netf("rpc-state id=35 player_drunk_seq=%u level=%u apply_pending=1 evidence=STATIC_037", seq, level);
  return true;
}

bool decode_player_fighting_style_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 3U) {
    return false;
  }

  /*
   * STATIC_037:
   * SA-MP 0.3.7-R5 samp.dll+0x18740 reads uint16 player ID plus uint8 style,
   * resolves the local/remote CPlayerPed, and calls samp.dll+0xAE0D0 (GTA 07FE).
   * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  const unsigned int player_id = read_le16(data);
  const unsigned int style = data[2U];
  const bool valid_style = style == 4U || style == 5U || style == 6U || style == 7U || style == 15U || style == 16U;
  if (player_id >= SAMP_RAKNET_MAX_PLAYERS || !valid_style) {
    trace_netf("rpc-state id=89 invalid_fighting_style target=%u style=%u bytes=%u ignored=1 evidence=STATIC_037,OPENMP_REF",
               player_id, style, bytes);
    return false;
  }
  g_rpc_probe.player_fighting_style_player_id = static_cast<unsigned short>(player_id);
  g_rpc_probe.player_fighting_style = static_cast<unsigned char>(style);
  const unsigned int seq = bump_seq(&g_rpc_probe.player_fighting_style_seq);
  trace_netf("rpc-state id=89 fighting_style_seq=%u target=%u style=%u apply_pending=1 evidence=STATIC_037",
             seq, player_id, style);
  return true;
}

bool decode_player_wanted_level_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 1U) {
    return false;
  }

  /*
   * STATIC_037:
   * SA-MP 0.3.7-R5 samp.dll+0x1CCF0 reads one byte and passes it to
   * samp.dll+0xA1420. SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  g_rpc_probe.player_wanted_level = data[0U];
  const unsigned int seq = bump_seq(&g_rpc_probe.player_wanted_level_seq);
  trace_netf("rpc-state id=133 wanted_level_seq=%u level=%u apply_pending=1 evidence=STATIC_037",
             seq, static_cast<unsigned int>(g_rpc_probe.player_wanted_level));
  return true;
}

bool decode_gravity_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) {
    return false;
  }

  /*
   * STATIC_037:
   * SA-MP 0.3.7-R5 samp.dll+0x1ACD0 reads one float and calls
   * samp.dll+0xA1400, which writes GTA's 0x863984 gravity global.
   * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   */
  const float gravity = read_le_float(data);
  if (!std::isfinite(gravity) || gravity < -1.0f || gravity > 1.0f) {
    trace_netf("rpc-state id=146 invalid_gravity=%.9f bytes=%u ignored=1 evidence=STATIC_037,TODO_VERIFY",
               static_cast<double>(gravity), bytes);
    return false;
  }
  g_rpc_probe.gravity = gravity;
  const unsigned int seq = bump_seq(&g_rpc_probe.gravity_seq);
  trace_netf("rpc-state id=146 gravity_seq=%u gravity=%.9f apply_pending=1 evidence=STATIC_037",
             seq, static_cast<double>(gravity));
  return true;
}

bool decode_set_player_name_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 4U) return false;
  const unsigned short player_id = read_le16(data);
  const unsigned int length = data[2U];
  if (player_id >= SAMP_RAKNET_MAX_PLAYERS || length >= SAMP_RAKNET_PLAYER_NAME_BYTES ||
      4U + length > bytes || data[3U + length] != 1U) return false;
  char name[SAMP_RAKNET_PLAYER_NAME_BYTES] = {0};
  if (length != 0U) std::memcpy(name, data + 3U, length);
  queue_player_pool_event(SAMP_RAKNET_PLAYER_POOL_ACTION_RENAME, player_id, 0U, 0U, 0U, name);
  trace_netf("rpc-state id=11 rename player=%u name='%s' evidence=STATIC_037", player_id, name);
  return true;
}

bool decode_world_bounds_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 16U) return false;
  for (unsigned int i = 0U; i < 4U; ++i) {
    g_rpc_probe.world_bounds[i] = read_le_float(data + i * 4U);
    if (!std::isfinite(g_rpc_probe.world_bounds[i])) return false;
  }
  if (g_rpc_probe.world_bounds[0] < g_rpc_probe.world_bounds[1] ||
      g_rpc_probe.world_bounds[2] < g_rpc_probe.world_bounds[3]) return false;
  const unsigned int seq = bump_seq(&g_rpc_probe.world_bounds_seq);
  trace_netf("rpc-state id=17 world_bounds_seq=%u x=%.3f..%.3f y=%.3f..%.3f evidence=STATIC_037",
             seq, g_rpc_probe.world_bounds[1], g_rpc_probe.world_bounds[0],
             g_rpc_probe.world_bounds[3], g_rpc_probe.world_bounds[2]);
  return true;
}

bool decode_camera_interpolate_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 30U) return false;
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
  bool set_pos = false;
  std::int32_t duration = 0;
  unsigned char cut = 0U;
  if (!bs.Read(set_pos) || !bs.Read(g_rpc_probe.camera_interpolate_from[0]) ||
      !bs.Read(g_rpc_probe.camera_interpolate_from[1]) || !bs.Read(g_rpc_probe.camera_interpolate_from[2]) ||
      !bs.Read(g_rpc_probe.camera_interpolate_to[0]) || !bs.Read(g_rpc_probe.camera_interpolate_to[1]) ||
      !bs.Read(g_rpc_probe.camera_interpolate_to[2]) || !bs.Read(duration) || !bs.Read(cut) || duration < 0) return false;
  for (unsigned int i = 0U; i < 3U; ++i) {
    if (!std::isfinite(g_rpc_probe.camera_interpolate_from[i]) ||
        !std::isfinite(g_rpc_probe.camera_interpolate_to[i])) return false;
  }
  g_rpc_probe.camera_interpolate_set_pos = set_pos ? 1U : 0U;
  g_rpc_probe.camera_interpolate_time_ms = duration;
  g_rpc_probe.camera_interpolate_cut = (cut == 1U || cut == 2U) ? cut : 2U;
  const unsigned int seq = bump_seq(&g_rpc_probe.camera_interpolate_seq);
  trace_netf("rpc-state id=82 camera_interpolate_seq=%u kind=%s time=%d cut=%u evidence=STATIC_037",
             seq, set_pos ? "position" : "look_at", duration, g_rpc_probe.camera_interpolate_cut);
  return true;
}

bool decode_client_control_payload(unsigned int rpc_id, const unsigned char *data, unsigned int bytes) {
  switch (rpc_id) {
    case 40U:
      bump_seq(&g_rpc_probe.game_mode_restart_seq);
      return true;
    case 74U:
      bump_seq(&g_rpc_probe.force_class_selection_seq);
      return true;
    case 81U:
      if (data == nullptr || bytes < 2U) return false;
      g_rpc_probe.camera_object_id = read_le16(data);
      if (g_rpc_probe.camera_object_id >= SAMP_RAKNET_MAX_OBJECTS) return false;
      bump_seq(&g_rpc_probe.camera_attach_object_seq);
      return true;
    case 88U:
      if (data == nullptr || bytes < 1U) return false;
      g_rpc_probe.special_action = data[0U];
      bump_seq(&g_rpc_probe.special_action_seq);
      return true;
    case 124U:
      if (data == nullptr || bytes < 4U) return false;
      g_rpc_probe.spectate_toggle = static_cast<std::int32_t>(read_le32(data));
      bump_seq(&g_rpc_probe.spectate_toggle_seq);
      return true;
    case 126U:
      if (data == nullptr || bytes < 3U) return false;
      g_rpc_probe.spectate_player_id = read_le16(data);
      g_rpc_probe.spectate_player_mode = data[2U];
      if (g_rpc_probe.spectate_player_id >= SAMP_RAKNET_MAX_PLAYERS) return false;
      bump_seq(&g_rpc_probe.spectate_player_seq);
      return true;
    case 127U:
      if (data == nullptr || bytes < 3U) return false;
      g_rpc_probe.spectate_vehicle_id = read_le16(data);
      g_rpc_probe.spectate_vehicle_mode = data[2U];
      if (g_rpc_probe.spectate_vehicle_id >= SAMP_RAKNET_MAX_VEHICLES) return false;
      bump_seq(&g_rpc_probe.spectate_vehicle_seq);
      return true;
    default:
      return false;
  }
}

bool decode_give_player_weapon_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 8U) {
    return false;
  }
  const unsigned int weapon = read_le32(data);
  const unsigned int ammo = read_le32(data + 4U);
  if (weapon > 255U || ammo > 1000000U) {
    trace_netf("rpc-state id=22 invalid_give_weapon weapon=%u ammo=%u bytes=%u ignored=1", weapon, ammo, bytes);
    return false;
  }
  g_rpc_probe.player_given_weapon = weapon;
  g_rpc_probe.player_given_weapon_ammo = ammo;
  const unsigned int give_seq = bump_seq(&g_rpc_probe.player_given_weapon_seq);
  samp_raknet_given_weapon_event &event =
      g_rpc_probe.player_given_weapon_events[(give_seq - 1U) % SAMP_RAKNET_GIVE_WEAPON_EVENT_RING];
  event.seq = give_seq;
  event.weapon = weapon;
  event.ammo = ammo;
  trace_netf("rpc-state id=22 give_weapon_seq=%u weapon=%u ammo=%u apply_pending=1 "
             "evidence=OPENMP_REF,STATIC_037",
             give_seq, weapon, ammo);
  return true;
}

bool decode_shop_name_payload(const unsigned char *data, unsigned int bytes) {
  if (data == nullptr || bytes < 32U) {
    return false;
  }
  std::memcpy(g_rpc_probe.shop_name, data, 32U);
  g_rpc_probe.shop_name[32U] = '\0';
  sanitize_3d_text_label_text(g_rpc_probe.shop_name);
  const unsigned int seq = bump_seq(&g_rpc_probe.shop_name_seq);
  trace_netf("rpc-state id=33 shop_name_seq=%u name='%s' apply_pending=1 "
             "evidence=STATIC_037,TODO_VERIFY",
             seq, g_rpc_probe.shop_name);
  return true;
}

bool decode_play_audio_stream_payload(const unsigned char *data, unsigned int bytes) {
  unsigned int offset = 0U;
  char url[SAMP_RAKNET_AUDIO_STREAM_URL_BYTES] = {0};
  float pos[3] = {0.0f, 0.0f, 0.0f};
  float distance = 0.0f;

  if (data == nullptr || bytes < 18U || data[0] >= sizeof(url) ||
      !read_dynstr8_plain(data, bytes, &offset, url, sizeof(url)) || bytes - offset < 17U) {
    return false;
  }
  read_vec3(data + offset, pos);
  distance = read_le_float(data + offset + 12U);
  const unsigned char use_pos = data[offset + 16U] != 0U ? 1U : 0U;
  if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2]) ||
      !std::isfinite(distance) || distance < 0.0f || distance > 100000.0f) {
    return false;
  }
  sanitize_3d_text_label_text(url);
  copy_text(g_rpc_probe.audio_stream_url, sizeof(g_rpc_probe.audio_stream_url), url);
  std::memcpy(g_rpc_probe.audio_stream_pos, pos, sizeof(pos));
  g_rpc_probe.audio_stream_distance = distance;
  g_rpc_probe.audio_stream_use_pos = use_pos;
  const unsigned int seq = bump_seq(&g_rpc_probe.play_audio_stream_seq);
  trace_netf("rpc-state id=41 play_audio_stream_seq=%u url_len=%u pos=%.3f %.3f %.3f "
             "distance=%.3f use_pos=%u apply_pending=1 evidence=STATIC_037,OPENMP_REF,TODO_VERIFY",
             seq, static_cast<unsigned int>(std::strlen(url)), static_cast<double>(pos[0]),
             static_cast<double>(pos[1]), static_cast<double>(pos[2]), static_cast<double>(distance),
             static_cast<unsigned int>(use_pos));
  return true;
}

bool decode_chat_bubble_payload(const unsigned char *data, unsigned int bytes) {
  unsigned int offset = 14U;
  char text[SAMP_RAKNET_CHAT_BUBBLE_TEXT_BYTES] = {0};
  if (data == nullptr || bytes < 15U || data[offset] >= sizeof(text) ||
      !read_dynstr8_plain(data, bytes, &offset, text, sizeof(text))) {
    return false;
  }
  const unsigned short player_id = read_le16(data);
  const unsigned int color = read_le32(data + 2U);
  const float draw_distance = read_le_float(data + 6U);
  const unsigned int duration_ms = read_le32(data + 10U);
  if (player_id >= SAMP_RAKNET_MAX_PLAYERS || !std::isfinite(draw_distance) || draw_distance < 0.0f ||
      draw_distance > 10000.0f) {
    return false;
  }
  sanitize_3d_text_label_text(text);
  const unsigned int seq = bump_seq(&g_rpc_probe.chat_bubble_event_seq);
  samp_raknet_chat_bubble_event &event =
      g_rpc_probe.chat_bubble_events[(seq - 1U) % SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING];
  std::memset(&event, 0, sizeof(event));
  event.seq = seq;
  event.player_id = player_id;
  event.color = color;
  event.duration_ms = duration_ms;
  event.draw_distance = draw_distance;
  copy_text(event.text, sizeof(event.text), text);
  trace_netf("rpc-state id=59 chat_bubble seq=%u player=%u color=0x%08x distance=%.3f duration=%u "
             "text_len=%u apply_pending=1 evidence=OPENMP_REF,TODO_VERIFY",
             seq, static_cast<unsigned int>(player_id), color, static_cast<double>(draw_distance), duration_ms,
             static_cast<unsigned int>(std::strlen(text)));
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
  if (g_rpc_probe.player_team_player_id >= SAMP_RAKNET_MAX_PLAYERS) {
    return false;
  }
  queue_player_pool_event(SAMP_RAKNET_PLAYER_POOL_ACTION_TEAM, g_rpc_probe.player_team_player_id, 0U, 0U,
                          g_rpc_probe.player_team, "");
  const unsigned int seq = bump_seq(&g_rpc_probe.player_team_seq);
  trace_netf("rpc-state id=69 player_team_seq=%u player=%u team=%u apply_pending=1 evidence=STATIC_037", seq,
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
  if (g_rpc_probe.player_color_player_id >= SAMP_RAKNET_MAX_PLAYERS) {
    return false;
  }
  queue_player_pool_event(SAMP_RAKNET_PLAYER_POOL_ACTION_COLOR, g_rpc_probe.player_color_player_id,
                          g_rpc_probe.player_color, 0U, 0U, "");
  const unsigned int seq = bump_seq(&g_rpc_probe.player_color_seq);
  trace_netf("rpc-state id=72 player_color_seq=%u player=%u color=0x%08x apply_pending=1 evidence=STATIC_037", seq,
             static_cast<unsigned int>(g_rpc_probe.player_color_player_id), g_rpc_probe.player_color);
  return true;
}

bool decode_apply_animation_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short player_id = 0U;
  unsigned char lib_len = 0U;
  unsigned char name_len = 0U;
  char lib[SAMP_RAKNET_ANIM_LIB_BYTES];
  char name[SAMP_RAKNET_ANIM_NAME_BYTES];
  float delta = 0.0f;
  bool loop = false;
  bool lock_x = false;
  bool lock_y = false;
  bool freeze = false;
  std::int32_t time = 0;

  if (data == nullptr || bytes < 16U) {
    return false;
  }
  std::memset(lib, 0, sizeof(lib));
  std::memset(name, 0, sizeof(name));
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
  if (!bs.Read(player_id) || !bs.Read(lib_len) || lib_len == 0U || lib_len >= sizeof(lib) ||
      !bs.Read(lib, static_cast<int>(lib_len)) || !bs.Read(name_len) || name_len == 0U ||
      name_len >= sizeof(name) || !bs.Read(name, static_cast<int>(name_len)) || !bs.Read(delta) ||
      !bs.Read(loop) || !bs.Read(lock_x) || !bs.Read(lock_y) || !bs.Read(freeze) || !bs.Read(time) ||
      !std::isfinite(delta) || delta <= 0.0f || delta > 100.0f) {
    return false;
  }
  g_rpc_probe.apply_animation_player_id = player_id;
  std::memcpy(g_rpc_probe.apply_animation_lib, lib, sizeof(g_rpc_probe.apply_animation_lib));
  std::memcpy(g_rpc_probe.apply_animation_name, name, sizeof(g_rpc_probe.apply_animation_name));
  g_rpc_probe.apply_animation_delta = delta;
  g_rpc_probe.apply_animation_loop = loop ? 1U : 0U;
  g_rpc_probe.apply_animation_lock_x = lock_x ? 1U : 0U;
  g_rpc_probe.apply_animation_lock_y = lock_y ? 1U : 0U;
  g_rpc_probe.apply_animation_freeze = freeze ? 1U : 0U;
  g_rpc_probe.apply_animation_time = time;
  const unsigned int seq = bump_seq(&g_rpc_probe.apply_animation_seq);
  trace_netf("rpc-state id=86 apply_animation_seq=%u player=%u lib='%s' name='%s' delta=%.3f flags=%u/%u/%u/%u time=%d apply_pending=1 evidence=STATIC_037",
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
  unsigned short object_id = 0U;
  samp_raknet_object_material material;

  if (data == nullptr || bytes < 4U) {
    return false;
  }
  object_id = read_le16(data);
  if (!object_id_valid(object_id)) {
    return false;
  }

  std::memset(&material, 0, sizeof(material));
  RakNet::BitStream material_bs(const_cast<unsigned char *>(data + 2U), bytes - 2U, false);
  if (!decode_object_material_record(material_bs, SAMP_RAKNET_OBJECT_MATERIAL_SOURCE_RPC, &material)) {
    trace_netf("object-material-decode: rpc84 invalid id=%u bytes=%u generation=%u",
               static_cast<unsigned int>(object_id), bytes, g_rpc_probe.object_generations[object_id]);
    return false;
  }
  return set_object_material_event(object_id, &material);
}

bool decode_create_3d_text_label_payload(const unsigned char *data, unsigned int bytes) {
  samp_raknet_3d_text_label_event *event = nullptr;

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
  char text[SAMP_RAKNET_3D_TEXT_LABEL_TEXT_BYTES] = {0};

  if (label_id >= SAMP_RAKNET_MAX_3D_TEXT_LABELS || !std::isfinite(pos[0]) || !std::isfinite(pos[1]) ||
      !std::isfinite(pos[2]) || !std::isfinite(pos[3]) || pos[3] < 0.0f || pos[3] > 10000.0f ||
      (player_attach != 0xFFFFU && player_attach >= SAMP_RAKNET_MAX_PLAYERS) ||
      (vehicle_attach != 0xFFFFU && vehicle_attach >= SAMP_RAKNET_MAX_VEHICLES) ||
      !decode_3d_text_label_tail(data, bytes, 27U, text, sizeof(text))) {
    return false;
  }

  event = queue_3d_text_label_event(SAMP_RAKNET_3D_TEXT_LABEL_ACTION_CREATE, label_id);
  event->color = color;
  event->test_los = los != 0U ? 1U : 0U;
  event->attached_player_id = static_cast<unsigned short>(player_attach);
  event->attached_vehicle_id = static_cast<unsigned short>(vehicle_attach);
  event->pos[0] = pos[0];
  event->pos[1] = pos[1];
  event->pos[2] = pos[2];
  event->draw_distance = pos[3];
  copy_text(event->text, sizeof(event->text), text);
  set_world_visual_event(SAMP_RAKNET_WORLD_VISUAL_CREATE_3D_TEXT_LABEL, label_id, 0, color, pos, text);
  trace_netf("rpc-state id=36 create_3d_text_label seq=%u label=%u color=0x%08x pos=%.3f %.3f %.3f draw=%.3f los=%u attach=%u/%u text='%.96s' evidence=OPENMP_REF,TODO_VERIFY",
             g_rpc_probe.text_label_event_seq, static_cast<unsigned int>(label_id), color,
             static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]),
             static_cast<double>(pos[3]), los, player_attach, vehicle_attach, text);
  return true;
}

bool decode_update_3d_text_label_payload(const unsigned char *data, unsigned int bytes) {
  samp_raknet_3d_text_label_event *event = nullptr;
  char text[SAMP_RAKNET_3D_TEXT_LABEL_TEXT_BYTES] = {0};

  if (data == nullptr || bytes < 6U) {
    return false;
  }

  const unsigned short label_id = read_le16(data);
  const unsigned int color = read_le32(data + 2U);
  if (label_id >= SAMP_RAKNET_MAX_3D_TEXT_LABELS ||
      !decode_3d_text_label_tail(data, bytes, 6U, text, sizeof(text))) {
    return false;
  }

  event = queue_3d_text_label_event(SAMP_RAKNET_3D_TEXT_LABEL_ACTION_UPDATE, label_id);
  event->color = color;
  copy_text(event->text, sizeof(event->text), text);
  trace_netf("rpc-state id=58 update_3d_text_label seq=%u label=%u color=0x%08x text='%.96s' evidence=OPENMP_REF,SAMPFUNCS_037,TODO_VERIFY",
             g_rpc_probe.text_label_event_seq, static_cast<unsigned int>(label_id), color, text);
  return true;
}

bool decode_delete_3d_text_label_payload(const unsigned char *data, unsigned int bytes) {
  samp_raknet_3d_text_label_event *event = nullptr;

  if (data == nullptr || bytes < 2U) {
    return false;
  }

  const unsigned short label_id = read_le16(data);
  if (label_id >= SAMP_RAKNET_MAX_3D_TEXT_LABELS) {
    return false;
  }

  event = queue_3d_text_label_event(SAMP_RAKNET_3D_TEXT_LABEL_ACTION_DELETE, label_id);
  trace_netf("rpc-state id=64 delete_3d_text_label seq=%u label=%u evidence=SAMPFUNCS_037,TODO_VERIFY",
             event->seq, static_cast<unsigned int>(label_id));
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
  unsigned char style = 0U;
  unsigned int color = 0U;

  if (data == nullptr || bytes < 19U) {
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
  style = data[18U];
  if (!std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2])) {
    return false;
  }
  queue_map_icon_event(SAMP_RAKNET_MAP_ICON_ACTION_SET, index, pos, icon, color, style);
  trace_netf("rpc-state id=56 set_map_icon seq=%u index=%u icon=%u color=0x%08x style=%u "
             "pos=%.3f %.3f %.3f evidence=STATIC_037:samp.dll+0x1A790",
             g_rpc_probe.map_icon_event_seq, static_cast<unsigned int>(index), static_cast<unsigned int>(icon),
             color, static_cast<unsigned int>(style), static_cast<double>(pos[0]), static_cast<double>(pos[1]),
             static_cast<double>(pos[2]));
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
  queue_map_icon_event(SAMP_RAKNET_MAP_ICON_ACTION_REMOVE, index, nullptr, 0U, 0U, 0U);
  trace_netf("rpc-state id=144 remove_map_icon seq=%u index=%u evidence=STATIC_037:samp.dll+0x1A8B0",
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

bool textdraw_classic_preview_values_plausible(unsigned short model, const float *rotation, float zoom,
                                               short color1, short color2) {
  if (model >= kGtaModelInfoCount) {
    return false;
  }
  if (rotation == nullptr || !std::isfinite(rotation[0]) || !std::isfinite(rotation[1]) ||
      !std::isfinite(rotation[2]) || !std::isfinite(zoom)) {
    return false;
  }
  if (rotation[0] < -100000.0f || rotation[0] > 100000.0f || rotation[1] < -100000.0f ||
      rotation[1] > 100000.0f || rotation[2] < -100000.0f || rotation[2] > 100000.0f) {
    return false;
  }
  if (zoom < 0.0f || zoom > 10000.0f) {
    return false;
  }
  return (color1 >= -1 && color1 <= 255) && (color2 >= -1 && color2 <= 255);
}

bool decode_textdraw_classic_preview_tail(const unsigned char *data, unsigned int bytes,
                                          samp_raknet_textdraw_transmit *transmit,
                                          unsigned int *inout_tail_offset) {
  const unsigned int preview_offset = 2U + kCompactTextDrawTransmitBytes;
  unsigned int field_offset = preview_offset;
  unsigned int tail_offset = preview_offset;
  unsigned short model = 0U;
  float rotation[3] = {0.0f, 0.0f, 0.0f};
  float zoom = 1.0f;
  short color1 = -1;
  short color2 = -1;
  bool has_selectable = false;

  if (data == nullptr || transmit == nullptr || inout_tail_offset == nullptr) {
    return false;
  }
  *inout_tail_offset = preview_offset;
  if (transmit->style != 5U || bytes < preview_offset + kClassicTextDrawPreviewNoSelectableBytes) {
    return false;
  }

  /* PROBE_TRACE + TODO_VERIFY:
   * Classic 0.3.7 ScrShowTextDraw carries preview-model fields after the
   * 40-byte text/box prefix. Some server stacks include a selectable byte
   * before the model id; accept both shapes and keep basic textdraw tails at
   * the legacy 40-byte boundary.
   */
  if (bytes >= preview_offset + kClassicTextDrawPreviewWithSelectableBytes &&
      (data[preview_offset] == 0U || data[preview_offset] == 1U)) {
    const unsigned int selectable_field_offset = preview_offset + 1U;
    const unsigned short selectable_model = read_le16(data + selectable_field_offset);
    const float selectable_rotation[3] = {read_le_float(data + selectable_field_offset + 2U),
                                          read_le_float(data + selectable_field_offset + 6U),
                                          read_le_float(data + selectable_field_offset + 10U)};
    const float selectable_zoom = read_le_float(data + selectable_field_offset + 14U);
    const short selectable_color1 = static_cast<short>(read_le16(data + selectable_field_offset + 18U));
    const short selectable_color2 = static_cast<short>(read_le16(data + selectable_field_offset + 20U));
    if (textdraw_classic_preview_values_plausible(selectable_model, selectable_rotation, selectable_zoom,
                                                  selectable_color1, selectable_color2)) {
      has_selectable = true;
      field_offset = selectable_field_offset;
      tail_offset = preview_offset + kClassicTextDrawPreviewWithSelectableBytes;
    }
  }

  if (!has_selectable) {
    tail_offset = preview_offset + kClassicTextDrawPreviewNoSelectableBytes;
  }

  model = read_le16(data + field_offset);
  rotation[0] = read_le_float(data + field_offset + 2U);
  rotation[1] = read_le_float(data + field_offset + 6U);
  rotation[2] = read_le_float(data + field_offset + 10U);
  zoom = read_le_float(data + field_offset + 14U);
  color1 = static_cast<short>(read_le16(data + field_offset + 18U));
  color2 = static_cast<short>(read_le16(data + field_offset + 20U));
  if (!textdraw_classic_preview_values_plausible(model, rotation, zoom, color1, color2)) {
    return false;
  }

  transmit->selectable = has_selectable ? data[preview_offset] : transmit->selectable;
  transmit->preview_model = model;
  transmit->preview_rotation[0] = rotation[0];
  transmit->preview_rotation[1] = rotation[1];
  transmit->preview_rotation[2] = rotation[2];
  transmit->preview_zoom = zoom;
  transmit->preview_color1 = color1;
  transmit->preview_color2 = color2;
  *inout_tail_offset = tail_offset;
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
  unsigned int tail_offset = 2U + kCompactTextDrawTransmitBytes;
  bool classic_preview = false;

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
  classic_preview = decode_textdraw_classic_preview_tail(data, bytes, &transmit, &tail_offset);
  std::memset(text, 0, sizeof(text));
  decode_textdraw_tail(data, bytes, tail_offset, text, sizeof(text));
  queue_textdraw_event(SAMP_RAKNET_TEXTDRAW_ACTION_SHOW, textdraw_id, &transmit, text);
  trace_netf("textdraw-decode: show seq=%u id=%u variant=compact pos=(%.3f,%.3f) style=%u flags=0x%02x selectable=%u model=%u zoom=%.3f preview=%u tail=%u text='%s'",
             g_rpc_probe.textdraw_event_seq, static_cast<unsigned int>(textdraw_id),
             static_cast<double>(transmit.x), static_cast<double>(transmit.y),
             static_cast<unsigned int>(transmit.style), static_cast<unsigned int>(transmit.flags),
             static_cast<unsigned int>(transmit.selectable), static_cast<unsigned int>(transmit.preview_model),
             static_cast<double>(transmit.preview_zoom), classic_preview ? 1U : 0U,
             bytes >= tail_offset ? bytes - tail_offset : 0U, text);
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

bool decode_game_text_payload(const unsigned char *data, unsigned int bytes) {
  std::int32_t style = 0;
  std::int32_t time_ms = 0;
  std::int32_t text_len = 0;
  unsigned int copy_len = 0U;
  bool hide = false;
  samp_raknet_game_text_event *event = nullptr;
  char text[SAMP_RAKNET_GAMETEXT_TEXT_BYTES];

  if (data == nullptr || bytes < 4U) {
    return false;
  }

  style = read_le_i32(data);
  if (bytes < 12U) {
    if (bytes != 4U && bytes != 8U) {
      return false;
    }
    time_ms = bytes >= 8U ? read_le_i32(data + 4U) : 0;
    if (time_ms < 0) {
      time_ms = 0;
    }
    event = queue_game_text_event(SAMP_RAKNET_GAMETEXT_ACTION_HIDE, style, time_ms, "");
    g_rpc_probe.game_text_seq = event->seq;
    g_rpc_probe.game_text_style = style;
    g_rpc_probe.game_text_time_ms = time_ms;
    g_rpc_probe.game_text[0] = '\0';
    trace_netf("rpc-state id=73 game_text_seq=%u action=hide layout=short style=%d time=%d "
               "bytes=%u evidence=OPENMP_REF,INFERRED,TODO_VERIFY",
               event->seq, static_cast<int>(style), static_cast<int>(time_ms), bytes);
    return true;
  }

  /*
   * STATIC_037 + OPENMP_REF + TODO_VERIFY:
   * ScrDisplayGameText is serialized as int style, int display time, int text
   * length, then raw text bytes. Keep the local renderer isolated from GTA
   * CMessages so this MP HUD path can be compared directly against MTA-style DX
   * text output.
   */
  time_ms = read_le_i32(data + 4U);
  text_len = read_le_i32(data + 8U);
  if (text_len < 0 || text_len > (std::int32_t)SAMP_RAKNET_GAMETEXT_TEXT_BYTES - 1 ||
      (unsigned int)text_len > bytes - 12U) {
    trace_netf("rpc-state id=73 game_text invalid style=%d time=%d len=%d bytes=%u",
               static_cast<int>(style), static_cast<int>(time_ms), static_cast<int>(text_len), bytes);
    return false;
  }
  if (time_ms < 0) {
    time_ms = 0;
  }

  copy_len = static_cast<unsigned int>(text_len);
  std::memset(text, 0, sizeof(text));
  if (copy_len > 0U) {
    std::memcpy(text, data + 12U, copy_len);
  }
  text[sizeof(text) - 1U] = '\0';
  sanitize_textdraw_text(text);
  trim_textdraw_text_tail(text);
  hide = time_ms <= 0 || !textdraw_text_has_printable_content(text);
  event = queue_game_text_event(hide ? SAMP_RAKNET_GAMETEXT_ACTION_HIDE : SAMP_RAKNET_GAMETEXT_ACTION_SHOW,
                                style, time_ms, hide ? "" : text);
  g_rpc_probe.game_text_seq = event->seq;
  g_rpc_probe.game_text_style = style;
  g_rpc_probe.game_text_time_ms = time_ms;
  copy_text(g_rpc_probe.game_text, sizeof(g_rpc_probe.game_text), hide ? "" : text);
  if (hide) {
    trace_netf("rpc-state id=73 game_text_seq=%u action=hide style=%d time=%d len=%d "
               "evidence=STATIC_037,OPENMP_REF,INFERRED,TODO_VERIFY",
               event->seq, static_cast<int>(style), static_cast<int>(time_ms), static_cast<int>(text_len));
    return true;
  }
  trace_netf("rpc-state id=73 game_text_seq=%u action=show style=%d time=%d text='%s' "
             "evidence=STATIC_037,OPENMP_REF,TODO_VERIFY",
             event->seq, static_cast<int>(style), static_cast<int>(time_ms), g_rpc_probe.game_text);
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

  if (g_rpc_probe.pending_request_spawn) {
    unsigned int attempt = 0U;
    if (g_rpc_probe.saw_dialog) {
      return;
    }
    RakNet::BitStream bs_send;
    const int sent = rak_client->RPC(kRpcRequestSpawn, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                    RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                         ? 1
                         : 0;
    attempt = g_rpc_probe.request_spawn_send_count + 1U;
    g_rpc_probe.request_spawn_send_count = sent != 0 ? attempt : 0U;
    g_rpc_probe.pending_request_spawn = 0;
    trace_netf("rpc-manual-out id=129 name=RequestSpawn attempt=%u sent=%d evidence=OPENMP_REF",
               attempt, sent);
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

  if (rpc_id == 11U) {
    if (rpc_params == nullptr || !decode_set_player_name_payload(rpc_params->input, bytes))
      trace_netf("rpc-state id=11 set_player_name decode_failed bytes=%u", bytes);
  } else if (rpc_id == 17U) {
    if (rpc_params == nullptr || !decode_world_bounds_payload(rpc_params->input, bytes))
      trace_netf("rpc-state id=17 world_bounds decode_failed bytes=%u", bytes);
  } else if (rpc_id == 82U) {
    if (rpc_params == nullptr || !decode_camera_interpolate_payload(rpc_params->input, bytes))
      trace_netf("rpc-state id=82 camera_interpolate decode_failed bytes=%u", bytes);
  } else if (rpc_id == 40U || rpc_id == 74U || rpc_id == 81U || rpc_id == 88U || rpc_id == 124U ||
             rpc_id == 126U || rpc_id == 127U) {
    const unsigned char *input = rpc_params != nullptr ? rpc_params->input : nullptr;
    if (!decode_client_control_payload(rpc_id, input, bytes))
      trace_netf("rpc-state id=%u client_control decode_failed bytes=%u", rpc_id, bytes);
  } else if (rpc_id == 18U) {
    if (rpc_params == nullptr || !decode_give_player_money_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=18 give_money decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 34U) {
    if (rpc_params == nullptr || !decode_player_skill_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=34 player_skill decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 35U) {
    if (rpc_params == nullptr || !decode_player_drunk_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=35 player_drunk decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 89U) {
    if (rpc_params == nullptr || !decode_player_fighting_style_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=89 fighting_style decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 93U) {
    if (rpc_params != nullptr && bytes >= 8U) {
      decode_client_message_payload(rpc_params->input, bytes);
    }
  } else if (rpc_id == 101U) {
    if (rpc_params == nullptr || !decode_chat_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=101 chat decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 133U) {
    if (rpc_params == nullptr || !decode_player_wanted_level_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=133 wanted_level decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 146U) {
    if (rpc_params == nullptr || !decode_gravity_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=146 gravity decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 145U) {
    if (rpc_params == nullptr || !decode_player_ammo_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=145 player_ammo decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 153U) {
    if (rpc_params == nullptr || !decode_player_skin_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=153 player_skin decode_failed bytes=%u", bytes);
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
        g_rpc_probe.class_selection_after_death_requested = 0;
        g_rpc_probe.class_selection_after_death_consumed = 0;
        trace_netf("rpc-state class_selection manual_controls_ready=1 auto_spawn=0 evidence=OPENMP_REF");
      }
    }
  } else if (rpc_id == 129U) {
    g_rpc_probe.saw_request_spawn_reply = 1;
    if (rpc_params != nullptr && bytes > 0U) {
      g_rpc_probe.request_spawn_outcome = rpc_params->input[0];
      trace_netf("rpc-state id=129 request_spawn_outcome=%u", static_cast<unsigned int>(g_rpc_probe.request_spawn_outcome));
      if (g_rpc_probe.request_spawn_outcome == 2U || g_rpc_probe.request_spawn_outcome == 1U) {
        g_rpc_probe.pending_request_spawn = 0;
        trace_netf("rpc-state id=129 spawn allowed outcome=%u waiting_for_local_finalize=1",
                   static_cast<unsigned int>(g_rpc_probe.request_spawn_outcome));
      } else {
        g_rpc_probe.pending_request_spawn = 0;
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
    } else {
      g_rpc_probe.textdraw_select_active = 0U;
      g_rpc_probe.textdraw_select_color = 0U;
      trace_netf("rpc-state id=83 select_textdraw decode_failed bits=%u bytes=%u active=0", bits, bytes);
    }
  } else if (rpc_id == 30U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      g_rpc_probe.saw_toggle_clock = 1;
      g_rpc_probe.clock_enabled = rpc_params->input[0] != 0U ? 1U : 0U;
      trace_netf("rpc-state id=30 toggle_clock=%u apply_pending=1 evidence=STATIC_037",
                 static_cast<unsigned int>(g_rpc_probe.clock_enabled));
    }
  } else if (rpc_id == 63U) {
    if (rpc_params != nullptr && bytes >= 4U) {
      const std::int32_t pickup_id = static_cast<std::int32_t>(read_le32(rpc_params->input));
      if (pickup_id >= 0 && pickup_id < 4096) {
        const unsigned int seq = bump_seq(&g_rpc_probe.pickup_event_seq);
        samp_raknet_pickup_event &event =
            g_rpc_probe.pickup_events[(seq - 1U) % SAMP_RAKNET_PICKUP_EVENT_RING];
        std::memset(&event, 0, sizeof(event));
        event.seq = seq;
        event.action = SAMP_RAKNET_PICKUP_ACTION_DESTROY;
        event.pickup_id = pickup_id;
        trace_netf("rpc-state id=63 pickup_destroy seq=%u pickup=%d apply_pending=1 evidence=STATIC_037",
                   seq, static_cast<int>(pickup_id));
      }
    }
  } else if (rpc_id == 79U) {
    if (rpc_params != nullptr && bytes >= 20U) {
      float pos[3];
      read_vec3(rpc_params->input, pos);
      const std::int32_t type = static_cast<std::int32_t>(read_le32(rpc_params->input + 12U));
      const float radius = read_le_float(rpc_params->input + 16U);
      if (std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
          std::isfinite(radius) && radius >= 0.0f && radius <= 1000.0f && type >= 0 && type <= 255) {
        const unsigned int seq = bump_seq(&g_rpc_probe.explosion_event_seq);
        samp_raknet_explosion_event &event =
            g_rpc_probe.explosion_events[(seq - 1U) % SAMP_RAKNET_EXPLOSION_EVENT_RING];
        event.seq = seq;
        event.type = type;
        std::memcpy(event.pos, pos, sizeof(pos));
        event.radius = radius;
        trace_netf("rpc-state id=79 explosion seq=%u type=%d pos=%.3f %.3f %.3f radius=%.3f "
                   "apply_pending=1 evidence=STATIC_037",
                   seq, static_cast<int>(type), static_cast<double>(pos[0]), static_cast<double>(pos[1]),
                   static_cast<double>(pos[2]), static_cast<double>(radius));
      }
    }
  } else if (rpc_id == 95U) {
    if (rpc_params != nullptr && bytes >= 24U) {
      const std::int32_t pickup_id = static_cast<std::int32_t>(read_le32(rpc_params->input));
      const std::int32_t model = static_cast<std::int32_t>(read_le32(rpc_params->input + 4U));
      const std::int32_t type = static_cast<std::int32_t>(read_le32(rpc_params->input + 8U));
      float pos[3];
      read_vec3(rpc_params->input + 12U, pos);
      if (pickup_id >= 0 && pickup_id < 4096 && model > 0 && model <= 20000 && type >= 0 && type <= 255 &&
          std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2])) {
        const unsigned int seq = bump_seq(&g_rpc_probe.pickup_event_seq);
        samp_raknet_pickup_event &event =
            g_rpc_probe.pickup_events[(seq - 1U) % SAMP_RAKNET_PICKUP_EVENT_RING];
        std::memset(&event, 0, sizeof(event));
        event.seq = seq;
        event.action = SAMP_RAKNET_PICKUP_ACTION_CREATE;
        event.pickup_id = pickup_id;
        event.model = model;
        event.type = type;
        std::memcpy(event.pos, pos, sizeof(pos));
        trace_netf("rpc-state id=95 pickup_create seq=%u pickup=%d model=%d type=%d pos=%.3f %.3f %.3f "
                   "apply_pending=1 evidence=STATIC_037",
                   seq, static_cast<int>(pickup_id), static_cast<int>(model), static_cast<int>(type),
                   static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]));
      }
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
  } else if (rpc_id == 58U) {
    if (rpc_params == nullptr || !decode_update_3d_text_label_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=58 update_3d_text_label decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 64U) {
    if (rpc_params == nullptr || !decode_delete_3d_text_label_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=64 delete_3d_text_label decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 28U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.cancel_edit_seq);
    trace_netf("rpc-state id=28 cancel_edit_seq=%u apply_pending=1 evidence=OPENMP_REF,TODO_VERIFY", seq);
  } else if (rpc_id == 33U) {
    if (rpc_params == nullptr || !decode_shop_name_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=33 shop_name decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 41U) {
    if (rpc_params == nullptr || !decode_play_audio_stream_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=41 play_audio_stream decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 42U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.stop_audio_stream_seq);
    trace_netf("rpc-state id=42 stop_audio_stream_seq=%u apply_pending=1 evidence=STATIC_037", seq);
  } else if (rpc_id == 43U) {
    if (rpc_params == nullptr || !decode_remove_building_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=43 remove_building decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 55U) {
    if (rpc_params == nullptr || !decode_death_message_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=55 death_window decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 59U) {
    if (rpc_params == nullptr || !decode_chat_bubble_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=59 chat_bubble decode_failed bytes=%u", bytes);
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
  } else if (rpc_id == 73U) {
    if (rpc_params == nullptr || !decode_game_text_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=73 game_text decode_failed bytes=%u", bytes);
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
    if (rpc_params == nullptr || !decode_object_attach_player_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=75 object_attach_player decode_failed bytes=%u", bytes);
    }
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
  } else if (rpc_id == 148U) {
    if (rpc_params == nullptr || !decode_attach_trailer_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=148 attach_trailer decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 149U) {
    if (rpc_params == nullptr || !decode_detach_trailer_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=149 detach_trailer decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 24U) {
    if (rpc_params == nullptr || !decode_vehicle_params_ex_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=24 vehicle_params_ex decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 57U) {
    if (rpc_params == nullptr || !decode_remove_vehicle_component_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=57 remove_vehicle_component decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 65U) {
    if (rpc_params == nullptr || !decode_link_vehicle_interior_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=65 link_vehicle_interior decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 123U) {
    if (rpc_params == nullptr || !decode_vehicle_number_plate_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=123 vehicle_number_plate decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 161U) {
    if (rpc_params == nullptr || !decode_vehicle_params_for_player_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=161 vehicle_params_for_player decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 159U) {
    if (rpc_params == nullptr || !decode_vehicle_position_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=159 vehicle_position decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 160U) {
    if (rpc_params == nullptr || !decode_vehicle_z_angle_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=160 vehicle_z_angle decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 70U) {
    if (rpc_params == nullptr || !decode_put_player_in_vehicle_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=70 put_player_in_vehicle decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 68U) {
    if (rpc_params != nullptr && bytes >= kObservedPlayerSpawnInfoBytes) {
      read_spawn_info(rpc_params->input, bytes, "ScrSetSpawnInfo");
    } else {
      g_rpc_probe.saw_spawn_info = 1;
    }
  } else if (rpc_id == 66U) {
    if (rpc_params == nullptr || !decode_player_armour_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=66 player_armour decode_failed bytes=%u", bytes);
    }
  } else if (rpc_id == 22U) {
    if (rpc_params == nullptr || !decode_give_player_weapon_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=22 give_player_weapon decode_failed bytes=%u", bytes);
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
  } else if (rpc_id == 13U) {
    if (rpc_params != nullptr && bytes >= 12U) {
      float pos[3];
      read_vec3(rpc_params->input, pos);
      if (std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
          pos[0] > -30000.0f && pos[0] < 30000.0f && pos[1] > -30000.0f && pos[1] < 30000.0f &&
          pos[2] > -5000.0f && pos[2] < 20000.0f) {
        std::memcpy(g_rpc_probe.player_pos_find_z, pos, sizeof(pos));
        const unsigned int seq = bump_seq(&g_rpc_probe.player_pos_find_z_seq);
        trace_netf("rpc-state id=13 pos_find_z_seq=%u pos=%.3f %.3f %.3f apply_pending=1 evidence=STATIC_037",
                   seq, static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]));
      } else {
        trace_netf("rpc-state id=13 invalid_pos_find_z=%.3f %.3f %.3f ignored=1 evidence=STATIC_037",
                   static_cast<double>(pos[0]), static_cast<double>(pos[1]), static_cast<double>(pos[2]));
      }
    }
  } else if (rpc_id == 71U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.remove_player_from_vehicle_seq);
    trace_netf("rpc-state id=71 remove_from_vehicle_seq=%u apply_pending=1 evidence=STATIC_037", seq);
  } else if (rpc_id == 90U) {
    if (rpc_params != nullptr && bytes >= 12U) {
      float velocity[3];
      read_vec3(rpc_params->input, velocity);
      if (std::isfinite(velocity[0]) && std::isfinite(velocity[1]) && std::isfinite(velocity[2]) &&
          std::fabs(velocity[0]) <= 20.0f && std::fabs(velocity[1]) <= 20.0f && std::fabs(velocity[2]) <= 20.0f) {
        std::memcpy(g_rpc_probe.player_velocity, velocity, sizeof(velocity));
        const unsigned int seq = bump_seq(&g_rpc_probe.player_velocity_seq);
        trace_netf("rpc-state id=90 player_velocity_seq=%u velocity=%.4f %.4f %.4f apply_pending=1 evidence=STATIC_037",
                   seq, static_cast<double>(velocity[0]), static_cast<double>(velocity[1]),
                   static_cast<double>(velocity[2]));
      } else {
        trace_netf("rpc-state id=90 invalid_player_velocity=%.4f %.4f %.4f ignored=1 evidence=STATIC_037",
                   static_cast<double>(velocity[0]), static_cast<double>(velocity[1]),
                   static_cast<double>(velocity[2]));
      }
    }
  } else if (rpc_id == 87U) {
    if (rpc_params != nullptr && bytes >= 2U) {
      g_rpc_probe.clear_animations_player_id = read_le16(rpc_params->input);
      const unsigned int seq = bump_seq(&g_rpc_probe.clear_animations_seq);
      trace_netf("rpc-state id=87 clear_animations_seq=%u player=%u apply_pending=1 evidence=STATIC_037",
                 seq, static_cast<unsigned int>(g_rpc_probe.clear_animations_player_id));
    }
  } else if (rpc_id == 91U) {
    if (rpc_params != nullptr && bytes >= 13U) {
      float velocity[3];
      const unsigned int turn = rpc_params->input[0U];
      read_vec3(rpc_params->input + 1U, velocity);
      if (turn <= 1U && std::isfinite(velocity[0]) && std::isfinite(velocity[1]) &&
          std::isfinite(velocity[2]) && std::fabs(velocity[0]) <= 20.0f &&
          std::fabs(velocity[1]) <= 20.0f && std::fabs(velocity[2]) <= 20.0f) {
        g_rpc_probe.vehicle_velocity_turn = static_cast<unsigned char>(turn);
        std::memcpy(g_rpc_probe.vehicle_velocity, velocity, sizeof(velocity));
        const unsigned int seq = bump_seq(&g_rpc_probe.vehicle_velocity_seq);
        trace_netf("rpc-state id=91 vehicle_velocity_seq=%u turn=%u velocity=%.4f %.4f %.4f "
                   "apply_pending=1 evidence=STATIC_037",
                   seq, turn, static_cast<double>(velocity[0]), static_cast<double>(velocity[1]),
                   static_cast<double>(velocity[2]));
      } else {
        trace_netf("rpc-state id=91 invalid_vehicle_velocity turn=%u velocity=%.4f %.4f %.4f ignored=1",
                   turn, static_cast<double>(velocity[0]), static_cast<double>(velocity[1]),
                   static_cast<double>(velocity[2]));
      }
    }
  } else if (rpc_id == 104U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      bool enabled = false;
      RakNet::BitStream bs(rpc_params->input, bytes, false);
      if (bs.Read(enabled)) {
        g_rpc_probe.stunt_bonus_enabled = enabled ? 1U : 0U;
        const unsigned int seq = bump_seq(&g_rpc_probe.stunt_bonus_seq);
        trace_netf("rpc-state id=104 stunt_bonus_seq=%u enabled=%u apply_pending=1 evidence=STATIC_037",
                   seq, enabled ? 1U : 0U);
      }
    }
  } else if (rpc_id == 107U) {
    if (rpc_params != nullptr && bytes >= 16U) {
      float pos[3];
      read_vec3(rpc_params->input, pos);
      const float size = read_le_float(rpc_params->input + 12U);
      if (std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
          std::isfinite(size) && size > 0.0f && size <= 1000.0f) {
        std::memcpy(g_rpc_probe.checkpoint_pos, pos, sizeof(pos));
        g_rpc_probe.checkpoint_size = size;
        const unsigned int seq = bump_seq(&g_rpc_probe.checkpoint_seq);
        g_rpc_probe.checkpoint_event_type = 1U;
        (void)bump_seq(&g_rpc_probe.checkpoint_event_seq);
        trace_netf("rpc-state id=107 checkpoint_seq=%u pos=%.3f %.3f %.3f size=%.3f apply_pending=1 "
                   "evidence=STATIC_037",
                   seq, static_cast<double>(pos[0]), static_cast<double>(pos[1]),
                   static_cast<double>(pos[2]), static_cast<double>(size));
      }
    }
  } else if (rpc_id == 37U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.disable_checkpoint_seq);
    g_rpc_probe.checkpoint_event_type = 2U;
    (void)bump_seq(&g_rpc_probe.checkpoint_event_seq);
    trace_netf("rpc-state id=37 disable_checkpoint_seq=%u apply_pending=1 evidence=STATIC_037", seq);
  } else if (rpc_id == 38U) {
    if (rpc_params != nullptr && bytes >= 29U) {
      float pos[3];
      float next[3];
      const unsigned int type = rpc_params->input[0U];
      read_vec3(rpc_params->input + 1U, pos);
      read_vec3(rpc_params->input + 13U, next);
      const float size = read_le_float(rpc_params->input + 25U);
      if (type <= 4U && std::isfinite(pos[0]) && std::isfinite(pos[1]) && std::isfinite(pos[2]) &&
          std::isfinite(next[0]) && std::isfinite(next[1]) && std::isfinite(next[2]) &&
          std::isfinite(size) && size > 0.0f && size <= 1000.0f) {
        g_rpc_probe.race_checkpoint_type = static_cast<unsigned char>(type);
        std::memcpy(g_rpc_probe.race_checkpoint_pos, pos, sizeof(pos));
        std::memcpy(g_rpc_probe.race_checkpoint_next, next, sizeof(next));
        g_rpc_probe.race_checkpoint_size = size;
        const unsigned int seq = bump_seq(&g_rpc_probe.race_checkpoint_seq);
        g_rpc_probe.checkpoint_event_type = 3U;
        (void)bump_seq(&g_rpc_probe.checkpoint_event_seq);
        trace_netf("rpc-state id=38 race_checkpoint_seq=%u type=%u pos=%.3f %.3f %.3f "
                   "next=%.3f %.3f %.3f size=%.3f apply_pending=1 evidence=STATIC_037",
                   seq, type, static_cast<double>(pos[0]), static_cast<double>(pos[1]),
                   static_cast<double>(pos[2]), static_cast<double>(next[0]),
                   static_cast<double>(next[1]), static_cast<double>(next[2]), static_cast<double>(size));
      }
    }
  } else if (rpc_id == 39U) {
    const unsigned int seq = bump_seq(&g_rpc_probe.disable_race_checkpoint_seq);
    g_rpc_probe.checkpoint_event_type = 4U;
    (void)bump_seq(&g_rpc_probe.checkpoint_event_seq);
    trace_netf("rpc-state id=39 disable_race_checkpoint_seq=%u apply_pending=1 evidence=STATIC_037", seq);
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
    if (rpc_params == nullptr || !decode_remove_map_icon_payload(rpc_params->input, bytes)) {
      trace_netf("rpc-state id=144 remove_map_icon decode_failed bytes=%u", bytes);
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

  if (client == g_rpc_probe.client) {
    release_all_object_material_states("client_destroy");
    std::memset(g_rpc_probe.object_generations, 0, sizeof(g_rpc_probe.object_generations));
    g_rpc_probe.object_generation_seq = 0U;
    g_rpc_probe.object_material_revision_seq = 0U;
    g_rpc_probe.client = nullptr;
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

int samp_raknet_client_send_rcon_command(void *client, const char *command) {
  RakNet::BitStream bs_send;
  std::size_t command_len = 0U;
  std::uint32_t command_len32 = 0U;
  int sent = 0;

  if (client == nullptr || command == nullptr || client != g_rpc_probe.client) {
    return -1;
  }

  command_len = std::strlen(command);
  if (command_len > kChatInputBytes) {
    return -1;
  }
  command_len32 = static_cast<std::uint32_t>(command_len);

  /*
   * STATIC_037:
   * Original SA-MP 0.3.7-R5 cmdRcon at samp.dll+0x00069030 writes packet
   * ID 0xC9, a 32-bit command length, then the command bytes. The send at
   * samp.dll+0x000690DA uses HIGH_PRIORITY, RELIABLE, ordering channel 0.
   */
  bs_send.Write(kPacketRconCommand);
  bs_send.Write(command_len32);
  if (command_len32 != 0U) {
    bs_send.Write(command, static_cast<int>(command_len32));
  }
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->Send(&bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0)
             ? 1
             : 0;

  /* Never place an RCON password or command payload in the network trace. */
  trace_netf("packet-user-out id=%u name=RconCommand len=%u sent=%d payload_redacted=1 evidence=STATIC_037",
             static_cast<unsigned int>(kPacketRconCommand), static_cast<unsigned int>(command_len32), sent);
  return sent ? 0 : -2;
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
  trace_netf("packet-user-out id=%u name=PlayerFootSync sent=%d pos=%.3f %.3f %.3f health=%u keys=0x%04x "
             "lr=0x%04x ud=0x%04x weapon=%u q=%.5f %.5f %.5f %.5f anim=%d anim_flags=0x%04x",
             static_cast<unsigned int>(kPacketPlayerSync), sent, static_cast<double>(sync->position[0]),
             static_cast<double>(sync->position[1]), static_cast<double>(sync->position[2]),
             static_cast<unsigned int>(sync->health), static_cast<unsigned int>(sync->keys),
             static_cast<unsigned int>(sync->left_right_keys), static_cast<unsigned int>(sync->up_down_keys),
             static_cast<unsigned int>(sync->current_weapon), static_cast<double>(sync->quaternion[0]),
             static_cast<double>(sync->quaternion[1]), static_cast<double>(sync->quaternion[2]),
             static_cast<double>(sync->quaternion[3]),
             static_cast<int>(sync->current_animation_id),
             static_cast<unsigned int>(static_cast<std::uint16_t>(sync->animation_flags)));
  return sent ? 0 : -2;
}

int samp_raknet_client_send_give_take_damage(void *client, uint8_t taking, uint16_t player_id, float damage,
                                             uint32_t weapon_id, uint32_t bodypart) {
  RakNet::BitStream bs_send;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client || damage < 0.0f) {
    return -1;
  }
  if (bodypart < 3U || bodypart > 9U) {
    return -1;
  }

  /*
   * OPENMP_REF + TODO_VERIFY:
   * open.mp's OnPlayerGiveTakeDamage RPC reader consumes one bit for Taking,
   * then UINT16 player id, float damage, UINT32 weapon id and UINT32 bodypart.
   */
  bs_send.Write(taking != 0U);
  bs_send.Write(static_cast<unsigned short>(player_id));
  bs_send.Write(damage);
  bs_send.Write(static_cast<unsigned int>(weapon_id));
  bs_send.Write(static_cast<unsigned int>(bodypart));
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->RPC(kRpcGiveTakeDamage, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                   RakNet::UNASSIGNED_NETWORK_ID, nullptr)
             ? 1
             : 0;
  trace_netf(
      "rpc-user-out id=115 name=GiveTakeDamage sent=%d taking=%u player=%u damage=%.3f weapon=%u bodypart=%u "
      "evidence=OPENMP_REF,TODO_VERIFY",
      sent, static_cast<unsigned int>(taking != 0U), static_cast<unsigned int>(player_id),
      static_cast<double>(damage), static_cast<unsigned int>(weapon_id), static_cast<unsigned int>(bodypart));
  return sent ? 0 : -2;
}

int samp_raknet_client_send_aim_sync(void *client, const samp_raknet_aim_sync *sync) {
  RakNet::BitStream bs_send;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client || sync == nullptr) {
    return -1;
  }

  /*
   * OPENMP_REF + STATIC_037 + INFERRED:
   * Client -> server aim sync is packet 203 followed by camMode, camera front,
   * camera position, aimZ, packed zoom/weapon-state and aspect-ratio. The
   * original local player path sends this as high-priority unreliable sequenced.
   */
  bs_send.Write(kPacketAimSync);
  bs_send.Write(reinterpret_cast<const char *>(sync), static_cast<int>(sizeof(*sync)));
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->Send(&bs_send, RakNet::HIGH_PRIORITY, RakNet::UNRELIABLE_SEQUENCED, 0)
             ? 1
             : 0;
  trace_netf("packet-user-out id=%u name=AimSync sent=%d mode=%u front=%.5f %.5f %.5f "
             "pos=%.3f %.3f %.3f aim_z=%.5f zoom_state=0x%02x aspect=%u "
             "evidence=OPENMP_REF,STATIC_037,INFERRED,TODO_VERIFY",
             static_cast<unsigned int>(kPacketAimSync), sent, static_cast<unsigned int>(sync->camera_mode),
             static_cast<double>(sync->camera_front[0]), static_cast<double>(sync->camera_front[1]),
             static_cast<double>(sync->camera_front[2]), static_cast<double>(sync->camera_position[0]),
             static_cast<double>(sync->camera_position[1]), static_cast<double>(sync->camera_position[2]),
             static_cast<double>(sync->aim_z), static_cast<unsigned int>(sync->zoom_weapon_state),
             static_cast<unsigned int>(sync->aspect_ratio));
  return sent ? 0 : -2;
}

int samp_raknet_client_send_bullet_sync(void *client, const samp_raknet_bullet_sync *sync) {
  RakNet::BitStream bs_send;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client || sync == nullptr) {
    return -1;
  }

  /*
   * OPENMP_REF + INFERRED + TODO_VERIFY:
   * Bullet sync is packet 206 with hit type/id, origin, hit position, offset
   * and weapon id. This lets open.mp fire OnPlayerWeaponShot for the local
   * player while GiveTakeDamage remains the damage-RPC path.
   */
  bs_send.Write(kPacketBulletSync);
  bs_send.Write(reinterpret_cast<const char *>(sync), static_cast<int>(sizeof(*sync)));
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->Send(&bs_send, RakNet::HIGH_PRIORITY, RakNet::UNRELIABLE_SEQUENCED, 0)
             ? 1
             : 0;
  trace_netf("packet-user-out id=%u name=BulletSync sent=%d hit_type=%u hit_id=%u weapon=%u "
             "origin=%.3f %.3f %.3f hit=%.3f %.3f %.3f offset=%.3f %.3f %.3f "
             "evidence=OPENMP_REF,INFERRED,TODO_VERIFY",
             static_cast<unsigned int>(kPacketBulletSync), sent, static_cast<unsigned int>(sync->hit_type),
             static_cast<unsigned int>(sync->hit_id), static_cast<unsigned int>(sync->weapon_id),
             static_cast<double>(sync->origin[0]), static_cast<double>(sync->origin[1]),
             static_cast<double>(sync->origin[2]), static_cast<double>(sync->hit_position[0]),
             static_cast<double>(sync->hit_position[1]), static_cast<double>(sync->hit_position[2]),
             static_cast<double>(sync->offset[0]), static_cast<double>(sync->offset[1]),
             static_cast<double>(sync->offset[2]));
  return sent ? 0 : -2;
}

int samp_raknet_client_send_spectator_sync(void *client, const samp_raknet_spectator_sync *sync) {
  RakNet::BitStream bs_send;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client || sync == nullptr) {
    return -1;
  }

  /* STATIC_037 + OPENMP_REF:
   * CLocalPlayer::ProcessSpectating sends ID_SPECTATOR_SYNC followed by the packed
   * 18-byte analog/key/camera-position payload every 200 ms.
   */
  bs_send.Write(kPacketSpectatorSync);
  bs_send.Write(reinterpret_cast<const char *>(sync), static_cast<int>(sizeof(*sync)));
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->Send(&bs_send, RakNet::HIGH_PRIORITY, RakNet::UNRELIABLE, 0)
             ? 1
             : 0;
  trace_netf("packet-user-out id=%u name=SpectatorSync sent=%d pos=%.3f %.3f %.3f keys=0x%04x "
             "evidence=STATIC_037,OPENMP_REF",
             static_cast<unsigned int>(kPacketSpectatorSync), sent, static_cast<double>(sync->position[0]),
             static_cast<double>(sync->position[1]), static_cast<double>(sync->position[2]),
             static_cast<unsigned int>(sync->keys));
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
      g_rpc_probe.player_given_weapon_seq > 0U || g_rpc_probe.reset_player_weapons_seq > 0U ||
      g_rpc_probe.reset_player_money_seq > 0U || g_rpc_probe.give_player_money_seq > 0U ||
      g_rpc_probe.player_ammo_seq > 0U || g_rpc_probe.player_skin_seq > 0U || g_rpc_probe.player_skill_seq > 0U ||
      g_rpc_probe.player_drunk_seq > 0U || g_rpc_probe.player_fighting_style_seq > 0U ||
      g_rpc_probe.player_pos_find_z_seq > 0U || g_rpc_probe.player_velocity_seq > 0U ||
      g_rpc_probe.remove_player_from_vehicle_seq > 0U || g_rpc_probe.clear_animations_seq > 0U ||
      g_rpc_probe.vehicle_velocity_seq > 0U || g_rpc_probe.stunt_bonus_seq > 0U ||
      g_rpc_probe.checkpoint_seq > 0U || g_rpc_probe.disable_checkpoint_seq > 0U ||
      g_rpc_probe.race_checkpoint_seq > 0U || g_rpc_probe.disable_race_checkpoint_seq > 0U ||
      g_rpc_probe.pickup_event_seq > 0U || g_rpc_probe.explosion_event_seq > 0U ||
      g_rpc_probe.chat_bubble_event_seq > 0U || g_rpc_probe.cancel_edit_seq > 0U ||
      g_rpc_probe.shop_name_seq > 0U || g_rpc_probe.play_audio_stream_seq > 0U ||
      g_rpc_probe.play_sound_seq > 0U || g_rpc_probe.stop_audio_stream_seq > 0U ||
      g_rpc_probe.player_color_seq > 0U || g_rpc_probe.player_team_seq > 0U ||
      g_rpc_probe.apply_animation_seq > 0U || g_rpc_probe.world_bounds_seq > 0U ||
      g_rpc_probe.game_mode_restart_seq > 0U || g_rpc_probe.force_class_selection_seq > 0U ||
      g_rpc_probe.camera_attach_object_seq > 0U || g_rpc_probe.camera_interpolate_seq > 0U ||
      g_rpc_probe.special_action_seq > 0U || g_rpc_probe.spectate_toggle_seq > 0U ||
      g_rpc_probe.spectate_player_seq > 0U || g_rpc_probe.spectate_vehicle_seq > 0U) {
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
  out_snapshot->death_window_event_count = 0U;
  out_snapshot->game_text_event_count = 0U;
  out_snapshot->text_label_event_count = 0U;
  out_snapshot->given_weapon_event_count = 0U;
  out_snapshot->give_money_event_count = 0U;
  out_snapshot->pickup_event_count = 0U;
  out_snapshot->explosion_event_count = 0U;
  out_snapshot->chat_bubble_event_count = 0U;
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
  out_snapshot->player_given_weapon_seq = g_rpc_probe.player_given_weapon_seq;
  out_snapshot->reset_player_weapons_seq = g_rpc_probe.reset_player_weapons_seq;
  out_snapshot->reset_player_money_seq = g_rpc_probe.reset_player_money_seq;
  out_snapshot->give_player_money_seq = g_rpc_probe.give_player_money_seq;
  out_snapshot->player_ammo_seq = g_rpc_probe.player_ammo_seq;
  out_snapshot->player_skin_seq = g_rpc_probe.player_skin_seq;
  out_snapshot->player_skill_seq = g_rpc_probe.player_skill_seq;
  out_snapshot->player_drunk_seq = g_rpc_probe.player_drunk_seq;
  out_snapshot->player_fighting_style_seq = g_rpc_probe.player_fighting_style_seq;
  out_snapshot->player_pos_find_z_seq = g_rpc_probe.player_pos_find_z_seq;
  out_snapshot->player_velocity_seq = g_rpc_probe.player_velocity_seq;
  out_snapshot->remove_player_from_vehicle_seq = g_rpc_probe.remove_player_from_vehicle_seq;
  out_snapshot->clear_animations_seq = g_rpc_probe.clear_animations_seq;
  out_snapshot->vehicle_velocity_seq = g_rpc_probe.vehicle_velocity_seq;
  out_snapshot->stunt_bonus_seq = g_rpc_probe.stunt_bonus_seq;
  out_snapshot->checkpoint_seq = g_rpc_probe.checkpoint_seq;
  out_snapshot->disable_checkpoint_seq = g_rpc_probe.disable_checkpoint_seq;
  out_snapshot->race_checkpoint_seq = g_rpc_probe.race_checkpoint_seq;
  out_snapshot->disable_race_checkpoint_seq = g_rpc_probe.disable_race_checkpoint_seq;
  out_snapshot->checkpoint_event_seq = g_rpc_probe.checkpoint_event_seq;
  out_snapshot->pickup_event_seq = g_rpc_probe.pickup_event_seq;
  out_snapshot->explosion_event_seq = g_rpc_probe.explosion_event_seq;
  out_snapshot->chat_bubble_event_seq = g_rpc_probe.chat_bubble_event_seq;
  out_snapshot->cancel_edit_seq = g_rpc_probe.cancel_edit_seq;
  out_snapshot->shop_name_seq = g_rpc_probe.shop_name_seq;
  out_snapshot->play_audio_stream_seq = g_rpc_probe.play_audio_stream_seq;
  out_snapshot->play_sound_seq = g_rpc_probe.play_sound_seq;
  out_snapshot->stop_audio_stream_seq = g_rpc_probe.stop_audio_stream_seq;
  out_snapshot->player_color_seq = g_rpc_probe.player_color_seq;
  out_snapshot->player_team_seq = g_rpc_probe.player_team_seq;
  out_snapshot->apply_animation_seq = g_rpc_probe.apply_animation_seq;
  out_snapshot->player_wanted_level_seq = g_rpc_probe.player_wanted_level_seq;
  out_snapshot->gravity_seq = g_rpc_probe.gravity_seq;
  out_snapshot->world_bounds_seq = g_rpc_probe.world_bounds_seq;
  out_snapshot->game_mode_restart_seq = g_rpc_probe.game_mode_restart_seq;
  out_snapshot->force_class_selection_seq = g_rpc_probe.force_class_selection_seq;
  out_snapshot->camera_attach_object_seq = g_rpc_probe.camera_attach_object_seq;
  out_snapshot->camera_interpolate_seq = g_rpc_probe.camera_interpolate_seq;
  out_snapshot->special_action_seq = g_rpc_probe.special_action_seq;
  out_snapshot->spectate_toggle_seq = g_rpc_probe.spectate_toggle_seq;
  out_snapshot->spectate_player_seq = g_rpc_probe.spectate_player_seq;
  out_snapshot->spectate_vehicle_seq = g_rpc_probe.spectate_vehicle_seq;
  out_snapshot->world_visual_event_seq = g_rpc_probe.world_visual_event_seq;
  out_snapshot->player_pool_event_seq = g_rpc_probe.player_pool_event_seq;
  out_snapshot->score_ping_seq = g_rpc_probe.score_ping_seq;
  out_snapshot->game_text_seq = g_rpc_probe.game_text_seq;
  out_snapshot->game_text_style = g_rpc_probe.game_text_style;
  out_snapshot->game_text_time_ms = g_rpc_probe.game_text_time_ms;
  out_snapshot->player_controllable = g_rpc_probe.player_controllable;
  out_snapshot->player_armour = g_rpc_probe.player_armour;
  out_snapshot->player_armed_weapon = g_rpc_probe.player_armed_weapon;
  out_snapshot->player_given_weapon = g_rpc_probe.player_given_weapon;
  out_snapshot->player_given_weapon_ammo = g_rpc_probe.player_given_weapon_ammo;
  out_snapshot->give_player_money = g_rpc_probe.give_player_money;
  out_snapshot->player_ammo_weapon = g_rpc_probe.player_ammo_weapon;
  out_snapshot->player_ammo = g_rpc_probe.player_ammo;
  out_snapshot->player_skin_player_id = g_rpc_probe.player_skin_player_id;
  out_snapshot->player_skin = g_rpc_probe.player_skin;
  out_snapshot->player_skill_player_id = g_rpc_probe.player_skill_player_id;
  out_snapshot->player_skill = g_rpc_probe.player_skill;
  out_snapshot->player_skill_level = g_rpc_probe.player_skill_level;
  out_snapshot->player_drunk_level = g_rpc_probe.player_drunk_level;
  out_snapshot->player_fighting_style_player_id = g_rpc_probe.player_fighting_style_player_id;
  out_snapshot->player_fighting_style = g_rpc_probe.player_fighting_style;
  std::memcpy(out_snapshot->player_pos_find_z, g_rpc_probe.player_pos_find_z,
              sizeof(out_snapshot->player_pos_find_z));
  std::memcpy(out_snapshot->player_velocity, g_rpc_probe.player_velocity,
              sizeof(out_snapshot->player_velocity));
  out_snapshot->clear_animations_player_id = g_rpc_probe.clear_animations_player_id;
  out_snapshot->vehicle_velocity_turn = g_rpc_probe.vehicle_velocity_turn;
  std::memcpy(out_snapshot->vehicle_velocity, g_rpc_probe.vehicle_velocity,
              sizeof(out_snapshot->vehicle_velocity));
  out_snapshot->stunt_bonus_enabled = g_rpc_probe.stunt_bonus_enabled;
  std::memcpy(out_snapshot->checkpoint_pos, g_rpc_probe.checkpoint_pos,
              sizeof(out_snapshot->checkpoint_pos));
  out_snapshot->checkpoint_size = g_rpc_probe.checkpoint_size;
  out_snapshot->race_checkpoint_type = g_rpc_probe.race_checkpoint_type;
  std::memcpy(out_snapshot->race_checkpoint_pos, g_rpc_probe.race_checkpoint_pos,
              sizeof(out_snapshot->race_checkpoint_pos));
  std::memcpy(out_snapshot->race_checkpoint_next, g_rpc_probe.race_checkpoint_next,
              sizeof(out_snapshot->race_checkpoint_next));
  out_snapshot->race_checkpoint_size = g_rpc_probe.race_checkpoint_size;
  out_snapshot->checkpoint_event_type = g_rpc_probe.checkpoint_event_type;
  std::memcpy(out_snapshot->shop_name, g_rpc_probe.shop_name, sizeof(out_snapshot->shop_name));
  std::memcpy(out_snapshot->audio_stream_url, g_rpc_probe.audio_stream_url,
              sizeof(out_snapshot->audio_stream_url));
  std::memcpy(out_snapshot->audio_stream_pos, g_rpc_probe.audio_stream_pos,
              sizeof(out_snapshot->audio_stream_pos));
  out_snapshot->audio_stream_distance = g_rpc_probe.audio_stream_distance;
  out_snapshot->audio_stream_use_pos = g_rpc_probe.audio_stream_use_pos;
  std::memcpy(out_snapshot->game_text, g_rpc_probe.game_text, sizeof(out_snapshot->game_text));
  if (g_rpc_probe.player_given_weapon_seq > 0U) {
    const unsigned int available = g_rpc_probe.player_given_weapon_seq < SAMP_RAKNET_GIVE_WEAPON_EVENT_RING
                                       ? g_rpc_probe.player_given_weapon_seq
                                       : SAMP_RAKNET_GIVE_WEAPON_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.player_given_weapon_seq - available + 1U;
    for (unsigned int seq = first_seq; seq <= g_rpc_probe.player_given_weapon_seq; ++seq) {
      const samp_raknet_given_weapon_event &event =
          g_rpc_probe.player_given_weapon_events[(seq - 1U) % SAMP_RAKNET_GIVE_WEAPON_EVENT_RING];
      if (event.seq == seq && out_snapshot->given_weapon_event_count < SAMP_RAKNET_GIVE_WEAPON_EVENT_RING) {
        out_snapshot->given_weapon_events[out_snapshot->given_weapon_event_count++] = event;
      }
    }
  }
  if (g_rpc_probe.give_player_money_seq > 0U) {
    const unsigned int available = g_rpc_probe.give_player_money_seq < SAMP_RAKNET_GIVE_MONEY_EVENT_RING
                                       ? g_rpc_probe.give_player_money_seq
                                       : SAMP_RAKNET_GIVE_MONEY_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.give_player_money_seq - available + 1U;
    for (unsigned int seq = first_seq; seq <= g_rpc_probe.give_player_money_seq; ++seq) {
      const samp_raknet_give_money_event &event =
          g_rpc_probe.give_money_events[(seq - 1U) % SAMP_RAKNET_GIVE_MONEY_EVENT_RING];
      if (event.seq == seq && out_snapshot->give_money_event_count < SAMP_RAKNET_GIVE_MONEY_EVENT_RING) {
        out_snapshot->give_money_events[out_snapshot->give_money_event_count++] = event;
      }
    }
  }
  if (g_rpc_probe.pickup_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.pickup_event_seq < SAMP_RAKNET_PICKUP_EVENT_RING
                                       ? g_rpc_probe.pickup_event_seq
                                       : SAMP_RAKNET_PICKUP_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.pickup_event_seq - available + 1U;
    for (unsigned int seq = first_seq; seq <= g_rpc_probe.pickup_event_seq; ++seq) {
      const samp_raknet_pickup_event &event =
          g_rpc_probe.pickup_events[(seq - 1U) % SAMP_RAKNET_PICKUP_EVENT_RING];
      if (event.seq == seq && out_snapshot->pickup_event_count < SAMP_RAKNET_PICKUP_EVENT_RING) {
        out_snapshot->pickup_events[out_snapshot->pickup_event_count++] = event;
      }
    }
  }
  if (g_rpc_probe.explosion_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.explosion_event_seq < SAMP_RAKNET_EXPLOSION_EVENT_RING
                                       ? g_rpc_probe.explosion_event_seq
                                       : SAMP_RAKNET_EXPLOSION_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.explosion_event_seq - available + 1U;
    for (unsigned int seq = first_seq; seq <= g_rpc_probe.explosion_event_seq; ++seq) {
      const samp_raknet_explosion_event &event =
          g_rpc_probe.explosion_events[(seq - 1U) % SAMP_RAKNET_EXPLOSION_EVENT_RING];
      if (event.seq == seq && out_snapshot->explosion_event_count < SAMP_RAKNET_EXPLOSION_EVENT_RING) {
        out_snapshot->explosion_events[out_snapshot->explosion_event_count++] = event;
      }
    }
  }
  if (g_rpc_probe.chat_bubble_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.chat_bubble_event_seq < SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING
                                       ? g_rpc_probe.chat_bubble_event_seq
                                       : SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.chat_bubble_event_seq - available + 1U;
    for (unsigned int seq = first_seq; seq <= g_rpc_probe.chat_bubble_event_seq; ++seq) {
      const samp_raknet_chat_bubble_event &event =
          g_rpc_probe.chat_bubble_events[(seq - 1U) % SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING];
      if (event.seq == seq && out_snapshot->chat_bubble_event_count < SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING) {
        out_snapshot->chat_bubble_events[out_snapshot->chat_bubble_event_count++] = event;
      }
    }
  }
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
  out_snapshot->player_wanted_level = g_rpc_probe.player_wanted_level;
  out_snapshot->gravity = g_rpc_probe.gravity;
  std::memcpy(out_snapshot->world_bounds, g_rpc_probe.world_bounds, sizeof(out_snapshot->world_bounds));
  out_snapshot->camera_object_id = g_rpc_probe.camera_object_id;
  out_snapshot->camera_interpolate_set_pos = g_rpc_probe.camera_interpolate_set_pos;
  out_snapshot->camera_interpolate_cut = g_rpc_probe.camera_interpolate_cut;
  std::memcpy(out_snapshot->camera_interpolate_from, g_rpc_probe.camera_interpolate_from,
              sizeof(out_snapshot->camera_interpolate_from));
  std::memcpy(out_snapshot->camera_interpolate_to, g_rpc_probe.camera_interpolate_to,
              sizeof(out_snapshot->camera_interpolate_to));
  out_snapshot->camera_interpolate_time_ms = g_rpc_probe.camera_interpolate_time_ms;
  out_snapshot->special_action = g_rpc_probe.special_action;
  out_snapshot->spectate_toggle = g_rpc_probe.spectate_toggle;
  out_snapshot->spectate_player_id = g_rpc_probe.spectate_player_id;
  out_snapshot->spectate_vehicle_id = g_rpc_probe.spectate_vehicle_id;
  out_snapshot->spectate_player_mode = g_rpc_probe.spectate_player_mode;
  out_snapshot->spectate_vehicle_mode = g_rpc_probe.spectate_vehicle_mode;
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
  std::memset(out_snapshot->death_window_events, 0, sizeof(out_snapshot->death_window_events));
  if (g_rpc_probe.death_window_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.death_window_event_seq < SAMP_RAKNET_DEATH_WINDOW_EVENT_RING
                                       ? g_rpc_probe.death_window_event_seq
                                       : SAMP_RAKNET_DEATH_WINDOW_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.death_window_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_DEATH_WINDOW_EVENT_RING;
      if (g_rpc_probe.death_window_events[slot].seq == seq) {
        out_snapshot->death_window_events[out_snapshot->death_window_event_count++] =
            g_rpc_probe.death_window_events[slot];
      }
    }
  }
  std::memset(out_snapshot->game_text_events, 0, sizeof(out_snapshot->game_text_events));
  if (g_rpc_probe.game_text_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.game_text_event_seq < SAMP_RAKNET_GAMETEXT_EVENT_RING
                                       ? g_rpc_probe.game_text_event_seq
                                       : SAMP_RAKNET_GAMETEXT_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.game_text_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_GAMETEXT_EVENT_RING;
      if (g_rpc_probe.game_text_events[slot].seq == seq) {
        out_snapshot->game_text_events[out_snapshot->game_text_event_count++] =
            g_rpc_probe.game_text_events[slot];
      }
    }
  }
  std::memset(out_snapshot->text_label_events, 0, sizeof(out_snapshot->text_label_events));
  if (g_rpc_probe.text_label_event_seq > 0U) {
    const unsigned int available = g_rpc_probe.text_label_event_seq < SAMP_RAKNET_3D_TEXT_LABEL_EVENT_RING
                                       ? g_rpc_probe.text_label_event_seq
                                       : SAMP_RAKNET_3D_TEXT_LABEL_EVENT_RING;
    const unsigned int first_seq = g_rpc_probe.text_label_event_seq - available + 1U;
    for (unsigned int i = 0U; i < available; ++i) {
      const unsigned int seq = first_seq + i;
      const unsigned int slot = (seq - 1U) % SAMP_RAKNET_3D_TEXT_LABEL_EVENT_RING;
      if (g_rpc_probe.text_label_events[slot].seq == seq) {
        out_snapshot->text_label_events[out_snapshot->text_label_event_count++] =
            g_rpc_probe.text_label_events[slot];
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

int samp_raknet_client_get_object_material(void *client, uint16_t object_id, uint32_t object_generation,
                                           uint8_t material_slot, uint32_t minimum_revision,
                                           samp_raknet_object_material *out_material,
                                           uint32_t *out_revision) {
  const ObjectMaterialSlotState *state = nullptr;

  if (out_material != nullptr) {
    std::memset(out_material, 0, sizeof(*out_material));
  }
  if (out_revision != nullptr) {
    *out_revision = 0U;
  }
  if (client == nullptr || client != g_rpc_probe.client || out_material == nullptr ||
      !object_id_valid(object_id) || material_slot >= SAMP_RAKNET_OBJECT_MATERIAL_SLOTS) {
    return -1;
  }
  state = g_rpc_probe.object_material_slots[object_id][material_slot];
  if (state == nullptr || object_generation == 0U || state->generation != object_generation) {
    return -2;
  }
  if (out_revision != nullptr) {
    *out_revision = state->revision;
  }
  /* A descriptor may lag behind the authoritative slot when several RPC 84 updates
   * arrive in one RakNet pump. minimum_revision==0 is an explicit full-state resync;
   * otherwise any state at least as new as the compact descriptor is valid.
   */
  if (minimum_revision != 0U && state->revision < minimum_revision) {
    return -3;
  }

  out_material->type = state->type;
  out_material->slot = state->slot;
  out_material->source = state->source;
  out_material->material_size = state->materialSize;
  out_material->font_size = state->fontSize;
  out_material->bold = state->bold;
  out_material->alignment = state->alignment;
  out_material->model = state->model;
  out_material->material_color = state->materialColor;
  out_material->background_color = state->backgroundColor;
  if (state->type == SAMP_RAKNET_OBJECT_MATERIAL_TYPE_DEFAULT) {
    copy_text(out_material->txd, sizeof(out_material->txd), state->nameA);
    copy_text(out_material->texture, sizeof(out_material->texture), state->nameB);
  } else if (state->type == SAMP_RAKNET_OBJECT_MATERIAL_TYPE_TEXT) {
    copy_text(out_material->font, sizeof(out_material->font), state->nameA);
    copy_text(out_material->text, sizeof(out_material->text), state->text);
  }
  return 0;
}

int samp_raknet_client_get_object_lifecycle(void *client, uint16_t object_id, uint8_t *out_alive,
                                            samp_raknet_object_event *out_create_event) {
  if (out_alive != nullptr) {
    *out_alive = 0U;
  }
  if (out_create_event != nullptr) {
    std::memset(out_create_event, 0, sizeof(*out_create_event));
  }
  if (client == nullptr || client != g_rpc_probe.client || !object_id_valid(object_id) ||
      out_alive == nullptr || out_create_event == nullptr) {
    return -1;
  }
  *out_alive = g_rpc_probe.object_alive[object_id] != 0U ? 1U : 0U;
  if (*out_alive != 0U) {
    *out_create_event = g_rpc_probe.object_create_states[object_id];
    if (out_create_event->action != SAMP_RAKNET_OBJECT_ACTION_CREATE ||
        out_create_event->object_id != object_id || out_create_event->object_generation == 0U) {
      std::memset(out_create_event, 0, sizeof(*out_create_event));
      *out_alive = 0U;
      return -2;
    }
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

int samp_raknet_client_send_death_notification(void *client, uint8_t death_reason, uint8_t responsible_player) {
  RakNet::BitStream bs_send;
  int sent = 0;

  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }

  /*
   * STATIC_037 + TODO_VERIFY:
   * 0.3.x localplayer writes byte death reason then byte responsible player to RPC_Death. The exact GTA-side
   * reason discovery is still pending, so callers may pass 255 as a conservative unknown/world value.
   */
  bs_send.Write(static_cast<unsigned char>(death_reason));
  bs_send.Write(static_cast<unsigned char>(responsible_player));
  sent = static_cast<RakNet::RakClientInterface *>(client)
             ->RPC(kRpcDeath, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE_SEQUENCED, 0, false,
                   RakNet::UNASSIGNED_NETWORK_ID, nullptr)
             ? 1
             : 0;
  trace_netf("rpc-auto-out id=53 name=Death reason=%u responsible=%u sent=%d evidence=STATIC_037,TODO_VERIFY",
             static_cast<unsigned int>(death_reason), static_cast<unsigned int>(responsible_player), sent);
  return sent ? 0 : -2;
}

int samp_raknet_client_mark_class_selection_after_death(void *client) {
  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }
  if (!g_rpc_probe.saw_init_game) {
    return -2;
  }
  if (g_rpc_probe.class_selection_after_death_requested && !g_rpc_probe.class_selection_after_death_consumed) {
    return 1;
  }

  /*
   * INFERRED + TODO_VERIFY:
   * The local 0.2x legacy tree keeps F4 as a "wants another class" latch and only calls
   * HandleClassSelection() from the wasted respawn path.
   */
  g_rpc_probe.class_selection_after_death_requested = 1;
  g_rpc_probe.class_selection_after_death_consumed = 0;
  trace_netf("rpc-manual: f4_after_death_latched selected_class=%d evidence=INFERRED,TODO_VERIFY",
             g_rpc_probe.selected_class);
  return 0;
}

int samp_raknet_client_request_class_selection_after_death(void *client) {
  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }
  if (!g_rpc_probe.saw_init_game) {
    return -2;
  }
  if (!g_rpc_probe.class_selection_after_death_requested || g_rpc_probe.class_selection_after_death_consumed) {
    return -3;
  }

  /*
   * INFERRED + TODO_VERIFY:
   * Mirrors legacy HandleClassSelection() after m_bWantsAnotherClass is consumed. Keep auto
   * RequestSpawn disabled until the player explicitly confirms class selection again.
   */
  g_rpc_probe.class_selection_after_death_requested = 0;
  g_rpc_probe.class_selection_after_death_consumed = 1;
  g_rpc_probe.saw_request_class_reply = 0;
  g_rpc_probe.request_class_outcome = 0U;
  g_rpc_probe.waiting_for_request_class_reply = 0;
  g_rpc_probe.saw_request_spawn_reply = 0;
  g_rpc_probe.request_spawn_outcome = 0U;
  g_rpc_probe.pending_request_spawn = 0;
  g_rpc_probe.request_spawn_send_count = 0U;
  g_rpc_probe.manual_spawn_shift_down = 0;
  schedule_request_class(g_rpc_probe.selected_class, "f4_after_death");
  trace_netf("rpc-manual: f4_after_death_request_class selected_class=%d evidence=INFERRED,TODO_VERIFY",
             g_rpc_probe.selected_class);
  return 0;
}

int samp_raknet_client_request_class_delta(void *client, int delta) {
  RakNet::RakNetTime now = 0;
  RakNet::RakNetTime class_elapsed = 0;
  int step = 0;
  int next_class = 0;

  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }
  const bool forced_selection = g_rpc_probe.class_selection_after_death_consumed != 0;
  if (delta == 0 || (!class_selection_manual_ready() && !forced_selection)) {
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
  if (forced_selection) {
    g_rpc_probe.waiting_for_request_class_reply = 0;
    g_rpc_probe.saw_request_class_reply = 0;
    g_rpc_probe.request_class_outcome = 0U;
  }
  schedule_request_class(next_class, step < 0 ? "ui_left" : "ui_right");
  return 0;
}

int samp_raknet_client_request_spawn(void *client) {
  if (client == nullptr || client != g_rpc_probe.client) {
    return -1;
  }
  const bool forced_selection = g_rpc_probe.class_selection_after_death_consumed != 0;
  if (!class_selection_manual_ready() && !forced_selection) {
    return -1;
  }
  if (g_rpc_probe.pending_request_class || g_rpc_probe.waiting_for_request_class_reply ||
      !g_rpc_probe.saw_request_class_reply || g_rpc_probe.request_class_outcome == 0U) {
    return -2;
  }

  /* INFERRED + OPENMP_REF:
   * This is the mouse equivalent of the existing SHIFT confirmation path.
   * The normal service loop remains the single owner that serializes and
   * sends RPC 129.
   */
  g_rpc_probe.pending_request_spawn = 1;
  trace_netf("rpc-manual: scheduled RequestSpawn from class-selection UI evidence=INFERRED,OPENMP_REF");
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
