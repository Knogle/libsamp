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

extern char AuthKeyTable[512][2][128];

namespace {
constexpr unsigned char kPacketIdInvalid = 0xFFu;
constexpr unsigned char kPacketPlayerSync = 207u;
constexpr RakNet::RPCID kRpcClientJoin = static_cast<RakNet::RPCID>(25u);
constexpr RakNet::RPCID kRpcDialogResponse = static_cast<RakNet::RPCID>(62u);
constexpr RakNet::RPCID kRpcSpawn = static_cast<RakNet::RPCID>(52u);
constexpr RakNet::RPCID kRpcServerCommand = static_cast<RakNet::RPCID>(50u);
constexpr RakNet::RPCID kRpcChat = static_cast<RakNet::RPCID>(101u);
constexpr RakNet::RPCID kRpcClickTextDraw = static_cast<RakNet::RPCID>(83u);
constexpr unsigned int kRpcScrDialogBox = 61U;
constexpr RakNet::RPCID kRpcRequestClass = static_cast<RakNet::RPCID>(128u);
constexpr RakNet::RPCID kRpcRequestSpawn = static_cast<RakNet::RPCID>(129u);
constexpr unsigned int kDefaultNetgameVersion = 4057u;
constexpr unsigned char kDefaultModByte = 1u;
constexpr const char *kDefaultNickname = "Player";
constexpr const char *kDefaultClientVersion = "0.3.7";
constexpr unsigned long kGpciFactor = 0x3e9UL;
constexpr unsigned int kRpcTraceMaxBytes = 32U;
constexpr unsigned int kRpcRegisterMax = 200U;
constexpr RakNet::RakNetTime kInitialSpawnDelayMs = 2200U;
constexpr RakNet::RakNetTime kClassSelectionManualDelayMs = 2000U;
constexpr RakNet::RakNetTime kPostDialogSpawnDelayMs = 450U;
constexpr RakNet::RakNetTime kSpawnRetryDelayMs = 1600U;
constexpr RakNet::RakNetTime kSelectModeClickDelayMs = 250U;
constexpr unsigned int kMaxSpawnRetries = 4U;
constexpr unsigned short kDefaultFreeroamTextDrawId = 24U;
constexpr unsigned int kLegacyPlayerSpawnInfoBytes = 45U;
constexpr unsigned int kPlayerSpawnInfoBytes = 46U;
constexpr unsigned int kDialogTitleBytes = 256U;
constexpr unsigned int kDialogInfoBytes = 4096U;
constexpr unsigned int kDialogButtonBytes = 64U;
constexpr unsigned int kDialogInputBytes = 256U;
constexpr unsigned int kChatInputBytes = 128U;
constexpr unsigned int kLegacyTextDrawTransmitBytes = 40U;
constexpr unsigned int kTextDrawShowMinBytes = 2U + kLegacyTextDrawTransmitBytes;
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

bool packet_resets_session_state(unsigned char packet_id) {
  return packet_id == static_cast<unsigned char>(RakNet::ID_DISCONNECTION_NOTIFICATION) ||
         packet_id == static_cast<unsigned char>(RakNet::ID_CONNECTION_LOST);
}

struct RpcProbeState {
  RakNet::RakClientInterface *client;
  unsigned char rpc_ids[256];
  unsigned int counts[256];
  int registered;
  int saw_init_game;
  int saw_request_class_reply;
  int saw_request_spawn_reply;
  int saw_spawn_info;
  int saw_dialog;
  int saw_player_pos;
  int saw_player_facing;
  int saw_weather;
  int saw_world_time;
  int saw_set_time_ex;
  int saw_toggle_clock;
  int saw_interior;
  int saw_camera_pos;
  int saw_camera_look_at;
  int saw_client_message;
  int saw_textdraw_select;
  int saw_object_event;
  int saw_vehicle_event;
  int pending_request_class;
  int pending_request_spawn;
  int pending_select_mode_freeroam_click;
  int pending_dialog_response;
  int sent_request_class;
  int sent_dialog_response;
  int sent_select_mode_freeroam_click;
  unsigned int request_spawn_send_count;
  unsigned int request_spawn_retry_count;
  int sent_spawn_notify;
  unsigned int sent_spawn_notify_seq;
  int manual_spawn_shift_down;
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
  unsigned int player_pos_seq;
  unsigned int player_facing_seq;
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
  RakNet::RakNetTime class_selection_ready_time;
  RakNet::RakNetTime next_request_spawn_time;
  RakNet::RakNetTime select_mode_click_ready_time;
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

bool auto_request_spawn_enabled() {
  const char *value = std::getenv("SAMPDLL_AUTO_REQUEST_SPAWN");
  if (value == nullptr || value[0] == '\0') {
    return true;
  }
  return value[0] != '0' && value[0] != 'n' && value[0] != 'N' && value[0] != 'f' && value[0] != 'F';
}

bool auto_select_freeroam_enabled() {
  const char *value = std::getenv("SAMPDLL_AUTO_SELECT_FREEROAM");
  if (value == nullptr || value[0] == '\0') {
    return true;
  }
  return value[0] != '0' && value[0] != 'n' && value[0] != 'N' && value[0] != 'f' && value[0] != 'F';
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

const char *rpc_name(unsigned int rpc_id) {
  switch (rpc_id) {
    case 11:
      return "ScrSetPlayerName";
    case 12:
      return "ScrSetPlayerPos";
    case 13:
      return "ScrSetPlayerPosFindZ";
    case 14:
      return "ScrSetPlayerHealth";
    case 15:
      return "ScrTogglePlayerControllable";
    case 16:
      return "ScrPlaySound";
    case 17:
      return "ScrSetWorldBounds";
    case 18:
      return "ScrHaveSomeMoney";
    case 19:
      return "ScrSetPlayerFacingAngle";
    case 20:
      return "ScrResetMoney";
    case 21:
      return "ScrResetPlayerWeapons";
    case 22:
      return "ScrGivePlayerWeapon";
    case 25:
      return "ClientJoin";
    case 29:
      return "SetTimeEx";
    case 32:
      return "WorldPlayerAdd";
    case 44:
      return "ScrCreateObject";
    case 45:
      return "ScrSetObjectPos";
    case 46:
      return "ScrSetObjectRotation";
    case 47:
      return "ScrDestroyObject";
    case 52:
      return "Spawn";
    case 61:
      return "ScrDialogBox";
    case 62:
      return "DialogResponse";
    case 68:
      return "ScrSetSpawnInfo";
    case 69:
      return "ScrSetPlayerTeam";
    case 70:
      return "ScrPutPlayerInVehicle";
    case 71:
      return "ScrRemovePlayerFromVehicle";
    case 72:
      return "ScrSetPlayerColor";
    case 75:
      return "ScrAttachObjectToPlayer";
    case 83:
      return "ClickTextDraw/SelectTextDraw";
    case 93:
      return "ClientMessage";
    case 94:
      return "WorldTime";
    case 95:
      return "Pickup";
    case 99:
      return "ScrMoveObject";
    case 105:
      return "PlayerTextDrawSetString";
    case 107:
      return "SetCheckpoint";
    case 118:
      return "SetInteriorId";
    case 122:
      return "ScrStopObject";
    case 128:
      return "RequestClass";
    case 129:
      return "RequestSpawn";
    case 134:
      return "ScrShowTextDraw";
    case 135:
      return "ScrHideTextDraw";
    case 136:
      return "ScrEditTextDraw";
    case 137:
      return "ServerJoin";
    case 138:
      return "ServerQuit";
    case 139:
      return "InitGame";
    case 144:
      return "ToggleClock";
    case 152:
      return "Weather";
    case 153:
      return "ScrSetPlayerSkin";
    case 155:
      return "UpdateScoresPingsIPs";
    case 156:
      return "ScrSetInterior";
    case 157:
      return "ScrSetCameraPos";
    case 158:
      return "ScrSetCameraLookAt";
    case 164:
      return "WorldVehicleAdd";
    case 165:
      return "WorldVehicleRemove";
    default:
      return "unknown";
  }
}

void reset_rpc_probe_runtime(RakNet::RakClientInterface *client) {
  g_rpc_probe.client = client;
  std::memset(g_rpc_probe.counts, 0, sizeof(g_rpc_probe.counts));
  g_rpc_probe.saw_init_game = 0;
  g_rpc_probe.saw_request_class_reply = 0;
  g_rpc_probe.saw_request_spawn_reply = 0;
  g_rpc_probe.saw_spawn_info = 0;
  g_rpc_probe.saw_dialog = 0;
  g_rpc_probe.saw_player_pos = 0;
  g_rpc_probe.saw_player_facing = 0;
  g_rpc_probe.saw_weather = 0;
  g_rpc_probe.saw_world_time = 0;
  g_rpc_probe.saw_set_time_ex = 0;
  g_rpc_probe.saw_toggle_clock = 0;
  g_rpc_probe.saw_interior = 0;
  g_rpc_probe.saw_camera_pos = 0;
  g_rpc_probe.saw_camera_look_at = 0;
  g_rpc_probe.saw_client_message = 0;
  g_rpc_probe.saw_textdraw_select = 0;
  g_rpc_probe.saw_object_event = 0;
  g_rpc_probe.saw_vehicle_event = 0;
  g_rpc_probe.pending_request_class = 0;
  g_rpc_probe.pending_request_spawn = 0;
  g_rpc_probe.pending_select_mode_freeroam_click = 0;
  g_rpc_probe.pending_dialog_response = 0;
  g_rpc_probe.sent_request_class = 0;
  g_rpc_probe.sent_dialog_response = 0;
  g_rpc_probe.sent_select_mode_freeroam_click = 0;
  g_rpc_probe.request_spawn_send_count = 0;
  g_rpc_probe.request_spawn_retry_count = 0;
  g_rpc_probe.sent_spawn_notify = 0;
  g_rpc_probe.sent_spawn_notify_seq = 0U;
  g_rpc_probe.manual_spawn_shift_down = 0;
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
  g_rpc_probe.player_pos_seq = 0U;
  g_rpc_probe.player_facing_seq = 0U;
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
  g_rpc_probe.class_selection_ready_time = 0;
  g_rpc_probe.next_request_spawn_time = 0;
  g_rpc_probe.select_mode_click_ready_time = 0;
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

  if (data == nullptr || bytes < kLegacyPlayerSpawnInfoBytes) {
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
   * OLD_02X_REF is older and shorter; do not reinterpret that as a vehicle here.
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
  std::memcpy(&transmit, data + 2U, kLegacyTextDrawTransmitBytes);
  std::memset(text, 0, sizeof(text));
  decode_textdraw_tail(data, bytes, 2U + kLegacyTextDrawTransmitBytes, text, sizeof(text));
  queue_textdraw_event(SAMP_RAKNET_TEXTDRAW_ACTION_SHOW, textdraw_id, &transmit, text);
  trace_netf("textdraw-decode: show seq=%u id=%u variant=legacy pos=(%.3f,%.3f) style=%u flags=0x%02x tail=%u text='%s'",
             g_rpc_probe.textdraw_event_seq, static_cast<unsigned int>(textdraw_id),
             static_cast<double>(transmit.x), static_cast<double>(transmit.y),
             static_cast<unsigned int>(transmit.style), static_cast<unsigned int>(transmit.flags),
             bytes - (2U + kLegacyTextDrawTransmitBytes), text);
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
  unsigned int slot = 0U;
  samp_raknet_client_message_probe *message = nullptr;

  if (data == nullptr || bytes < 8U) {
    return;
  }

  len = read_le32(data + 4U);
  if (len > bytes - 8U) {
    trace_netf("rpc-state id=93 decode failed len=%u bytes=%u", len, bytes);
    return;
  }

  seq = ++g_rpc_probe.client_message_seq;
  slot = (seq - 1U) % SAMP_RAKNET_CLIENT_MESSAGE_RING;
  message = &g_rpc_probe.client_messages[slot];
  std::memset(message, 0, sizeof(*message));
  message->seq = seq;
  message->color = read_le32(data);
  copy_len = len;
  if (copy_len >= SAMP_RAKNET_CLIENT_MESSAGE_BYTES) {
    copy_len = SAMP_RAKNET_CLIENT_MESSAGE_BYTES - 1U;
  }
  if (copy_len > 0U) {
    std::memcpy(message->text, data + 8U, copy_len);
  }
  message->text[copy_len] = '\0';
  filter_chat_text(message->text);
  g_rpc_probe.saw_client_message = 1;

  trace_netf("rpc-state id=93 client_message seq=%u color=0x%08x len=%u text='%s'", seq, message->color, len,
             message->text);
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

  if (g_rpc_probe.pending_request_class && !g_rpc_probe.sent_request_class) {
    RakNet::BitStream bs_send;
    int selected_class = 0;
    bs_send.Write(selected_class);
    const int sent = rak_client->RPC(kRpcRequestClass, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE, 0, false,
                                    RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                         ? 1
                         : 0;
    g_rpc_probe.sent_request_class = 1;
    g_rpc_probe.pending_request_class = 0;
    trace_netf("rpc-auto-out id=128 name=RequestClass selected_class=%d sent=%d", selected_class, sent);
  }

#ifdef _WIN32
  if (!g_rpc_probe.saw_dialog && !g_rpc_probe.pending_request_spawn && g_rpc_probe.saw_request_class_reply &&
      g_rpc_probe.request_class_outcome != 0U && g_rpc_probe.request_spawn_send_count == 0U &&
      g_rpc_probe.class_selection_ready_time != 0U && RakNet::GetTime() >= g_rpc_probe.class_selection_ready_time) {
    const int shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (shift_down && !g_rpc_probe.manual_spawn_shift_down) {
      g_rpc_probe.pending_request_spawn = 1;
      g_rpc_probe.next_request_spawn_time = 0;
      trace_netf("rpc-manual: scheduled RequestSpawn from SHIFT key");
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

  trace_netf("rpc-in id=%u name=%s count=%u bits=%u bytes=%u sender=0x%08x:%u idx=%u first=%s", rpc_id,
             rpc_name(rpc_id), (rpc_id < 256U) ? g_rpc_probe.counts[rpc_id] : 0U,
             rpc_params != nullptr ? rpc_params->numberOfBitsOfData : 0U, bytes,
             rpc_params != nullptr ? rpc_params->sender.binaryAddress : 0U,
             rpc_params != nullptr ? rpc_params->sender.port : 0U,
             rpc_params != nullptr ? static_cast<unsigned int>(rpc_params->senderIndex) : 0U,
             prefix_bytes > 0U ? prefix : "-");

  if (rpc_id == 93U) {
    if (rpc_params != nullptr && bytes >= 8U) {
      decode_client_message_payload(rpc_params->input, bytes);
    }
  } else if (rpc_id == 139U) {
    g_rpc_probe.saw_init_game = 1;
    if (rpc_params != nullptr) {
      if (!decode_init_game_payload(rpc_params->input, bytes)) {
        trace_netf("rpc-state id=139 init_game decode failed bytes=%u", bytes);
      }
    }
    if (!g_rpc_probe.sent_request_class) {
      g_rpc_probe.pending_request_class = 1;
    }
  } else if (rpc_id == 128U) {
    g_rpc_probe.saw_request_class_reply = 1;
    if (rpc_params != nullptr && bytes > 0U) {
      g_rpc_probe.request_class_outcome = rpc_params->input[0];
      trace_netf("rpc-state id=128 request_class_outcome=%u", static_cast<unsigned int>(g_rpc_probe.request_class_outcome));
      if (g_rpc_probe.request_class_outcome != 0U && bytes >= (1U + kLegacyPlayerSpawnInfoBytes)) {
        read_spawn_info(rpc_params->input + 1, bytes - 1U, "RequestClass");
        g_rpc_probe.class_selection_ready_time = RakNet::GetTime() + kClassSelectionManualDelayMs;
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
  } else if (rpc_id == 68U) {
    if (rpc_params != nullptr && bytes >= kLegacyPlayerSpawnInfoBytes) {
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
      trace_netf("rpc-state id=144 toggle_clock=%u observe_only=1",
                 static_cast<unsigned int>(g_rpc_probe.clock_enabled));
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
  trace_netf("rpc-probe registered ids=1..%u", kRpcRegisterMax);
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

  for (int i = 0; i < 512; ++i) {
    if (std::strcmp(AuthKeyTable[i][0], challenge) == 0) {
      return AuthKeyTable[i][1];
    }
  }

  return nullptr;
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
   * OPENMP_REF + OLD_02X_REF + INFERRED:
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
  out_snapshot->textdraw_select_active = g_rpc_probe.textdraw_select_active;
  out_snapshot->textdraw_select_color = g_rpc_probe.textdraw_select_color;
  out_snapshot->init_spawns_available = g_rpc_probe.init_spawns_available;
  out_snapshot->init_local_player_id = g_rpc_probe.init_local_player_id;
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
  out_snapshot->player_pos_seq = g_rpc_probe.player_pos_seq;
  out_snapshot->player_facing_seq = g_rpc_probe.player_facing_seq;
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
