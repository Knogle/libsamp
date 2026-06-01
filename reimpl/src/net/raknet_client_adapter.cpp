#include "sampdll/net/raknet_client_adapter.h"

#include <cstdarg>
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
#include "raknet/RakClientInterface.h"
#include "raknet/RakNetworkFactory.h"
#include "raknet/StringCompressor.h"

extern char AuthKeyTable[512][2][128];

namespace {
constexpr unsigned char kPacketIdInvalid = 0xFFu;
constexpr RakNet::RPCID kRpcClientJoin = static_cast<RakNet::RPCID>(25u);
constexpr RakNet::RPCID kRpcDialogResponse = static_cast<RakNet::RPCID>(62u);
constexpr RakNet::RPCID kRpcSpawn = static_cast<RakNet::RPCID>(52u);
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
constexpr RakNet::RakNetTime kSpawnRetryDelayMs = 1600U;
constexpr unsigned int kMaxSpawnRetries = 4U;
constexpr unsigned int kPlayerSpawnInfoBytes = 45U;
constexpr unsigned int kDialogTitleBytes = 256U;
constexpr unsigned int kDialogInfoBytes = 4096U;
constexpr unsigned int kDialogButtonBytes = 64U;
constexpr unsigned int kDialogInputBytes = 256U;

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
  int saw_interior;
  int saw_camera_pos;
  int saw_camera_look_at;
  int saw_client_message;
  int pending_request_class;
  int pending_request_spawn;
  int pending_dialog_response;
  int sent_request_class;
  int sent_dialog_response;
  unsigned int request_spawn_send_count;
  unsigned int request_spawn_retry_count;
  int sent_spawn_notify;
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
  unsigned char request_class_outcome;
  unsigned char request_spawn_outcome;
  float player_pos[3];
  float player_facing_angle;
  unsigned char weather;
  unsigned char interior;
  float camera_pos[3];
  float camera_look_at[3];
  unsigned char camera_look_at_type;
  unsigned char spawn_team;
  std::int32_t spawn_skin;
  float spawn_pos[3];
  float spawn_rotation;
  std::int32_t spawn_weapons[3];
  std::int32_t spawn_weapon_ammo[3];
  unsigned int client_message_seq;
  samp_raknet_client_message_probe client_messages[SAMP_RAKNET_CLIENT_MESSAGE_RING];
  RakNet::RakNetTime class_selection_ready_time;
  RakNet::RakNetTime next_request_spawn_time;
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
    return false;
  }
  return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
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
    case 32:
      return "WorldPlayerAdd";
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
    case 93:
      return "ClientMessage";
    case 94:
      return "WorldTime";
    case 95:
      return "Pickup";
    case 107:
      return "SetCheckpoint";
    case 118:
      return "SetInteriorId";
    case 128:
      return "RequestClass";
    case 129:
      return "RequestSpawn";
    case 137:
      return "ServerJoin";
    case 138:
      return "ServerQuit";
    case 139:
      return "InitGame";
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
  g_rpc_probe.saw_interior = 0;
  g_rpc_probe.saw_camera_pos = 0;
  g_rpc_probe.saw_camera_look_at = 0;
  g_rpc_probe.saw_client_message = 0;
  g_rpc_probe.pending_request_class = 0;
  g_rpc_probe.pending_request_spawn = 0;
  g_rpc_probe.pending_dialog_response = 0;
  g_rpc_probe.sent_request_class = 0;
  g_rpc_probe.sent_dialog_response = 0;
  g_rpc_probe.request_spawn_send_count = 0;
  g_rpc_probe.request_spawn_retry_count = 0;
  g_rpc_probe.sent_spawn_notify = 0;
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
  g_rpc_probe.request_class_outcome = 0;
  g_rpc_probe.request_spawn_outcome = 0;
  std::memset(g_rpc_probe.player_pos, 0, sizeof(g_rpc_probe.player_pos));
  g_rpc_probe.player_facing_angle = 0.0f;
  g_rpc_probe.weather = 0;
  g_rpc_probe.interior = 0;
  std::memset(g_rpc_probe.camera_pos, 0, sizeof(g_rpc_probe.camera_pos));
  std::memset(g_rpc_probe.camera_look_at, 0, sizeof(g_rpc_probe.camera_look_at));
  g_rpc_probe.camera_look_at_type = 0;
  g_rpc_probe.spawn_team = 0;
  g_rpc_probe.spawn_skin = 0;
  std::memset(g_rpc_probe.spawn_pos, 0, sizeof(g_rpc_probe.spawn_pos));
  g_rpc_probe.spawn_rotation = 0.0f;
  std::memset(g_rpc_probe.spawn_weapons, 0, sizeof(g_rpc_probe.spawn_weapons));
  std::memset(g_rpc_probe.spawn_weapon_ammo, 0, sizeof(g_rpc_probe.spawn_weapon_ammo));
  g_rpc_probe.client_message_seq = 0;
  std::memset(g_rpc_probe.client_messages, 0, sizeof(g_rpc_probe.client_messages));
  g_rpc_probe.class_selection_ready_time = 0;
  g_rpc_probe.next_request_spawn_time = 0;
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

void read_spawn_info(const unsigned char *data, unsigned int bytes, const char *source) {
  unsigned int offset = 0;

  if (data == nullptr || bytes < kPlayerSpawnInfoBytes) {
    return;
  }

  g_rpc_probe.spawn_team = data[offset++];
  g_rpc_probe.spawn_skin = read_le_i32(data + offset);
  offset += 4U;
  read_vec3(data + offset, g_rpc_probe.spawn_pos);
  offset += 12U;
  g_rpc_probe.spawn_rotation = read_le_float(data + offset);
  offset += 4U;
  for (unsigned int i = 0; i < 3U; ++i) {
    g_rpc_probe.spawn_weapons[i] = read_le_i32(data + offset);
    offset += 4U;
  }
  for (unsigned int i = 0; i < 3U; ++i) {
    g_rpc_probe.spawn_weapon_ammo[i] = read_le_i32(data + offset);
    offset += 4U;
  }
  g_rpc_probe.saw_spawn_info = 1;

  trace_netf("rpc-state spawn_info source=%s team=%u skin=%d pos=%.3f %.3f %.3f rot=%.3f weapons=%d/%d/%d ammo=%d/%d/%d",
             source != nullptr ? source : "unknown", static_cast<unsigned int>(g_rpc_probe.spawn_team),
             static_cast<int>(g_rpc_probe.spawn_skin), static_cast<double>(g_rpc_probe.spawn_pos[0]),
             static_cast<double>(g_rpc_probe.spawn_pos[1]), static_cast<double>(g_rpc_probe.spawn_pos[2]),
             static_cast<double>(g_rpc_probe.spawn_rotation), static_cast<int>(g_rpc_probe.spawn_weapons[0]),
             static_cast<int>(g_rpc_probe.spawn_weapons[1]), static_cast<int>(g_rpc_probe.spawn_weapons[2]),
             static_cast<int>(g_rpc_probe.spawn_weapon_ammo[0]), static_cast<int>(g_rpc_probe.spawn_weapon_ammo[1]),
             static_cast<int>(g_rpc_probe.spawn_weapon_ammo[2]));
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

bool decode_dialog_payload(const unsigned char *data, unsigned int bytes) {
  unsigned short dialog_id = 0;
  unsigned char style = 0;
  RakNet::BitStream bs(const_cast<unsigned char *>(data), bytes, false);
  RakNet::StringCompressor *compressor = RakNet::StringCompressor::Instance();

  g_rpc_probe.dialog_title[0] = '\0';
  g_rpc_probe.dialog_info[0] = '\0';
  copy_text(g_rpc_probe.dialog_button1, sizeof(g_rpc_probe.dialog_button1), "OK");
  g_rpc_probe.dialog_button2[0] = '\0';

  if (data == nullptr || bytes < 3U || compressor == nullptr) {
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
  SendMessageA(list, LB_SETCURSEL, 0, 0);
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
      sel = 0;
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

void send_spawn_notification(RakNet::RakClientInterface *rak_client) {
  RakNet::BitStream bs_send;

  if (rak_client == nullptr || g_rpc_probe.sent_spawn_notify) {
    return;
  }

  const int sent = rak_client->RPC(kRpcSpawn, &bs_send, RakNet::HIGH_PRIORITY, RakNet::RELIABLE_SEQUENCED, 0, false,
                                  RakNet::UNASSIGNED_NETWORK_ID, nullptr)
                       ? 1
                       : 0;
  g_rpc_probe.sent_spawn_notify = 1;
  trace_netf("rpc-auto-out id=52 name=SpawnNotify sent=%d", sent);
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

  if (g_rpc_probe.pending_request_spawn) {
    RakNet::RakNetTime now = RakNet::GetTime();
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
    if (!g_rpc_probe.sent_request_class) {
      g_rpc_probe.pending_request_class = 1;
    }
  } else if (rpc_id == 128U) {
    g_rpc_probe.saw_request_class_reply = 1;
    if (rpc_params != nullptr && bytes > 0U) {
      g_rpc_probe.request_class_outcome = rpc_params->input[0];
      trace_netf("rpc-state id=128 request_class_outcome=%u", static_cast<unsigned int>(g_rpc_probe.request_class_outcome));
      if (g_rpc_probe.request_class_outcome != 0U && bytes >= (1U + kPlayerSpawnInfoBytes)) {
        read_spawn_info(rpc_params->input + 1, bytes - 1U, "RequestClass");
        g_rpc_probe.class_selection_ready_time = RakNet::GetTime() + kClassSelectionManualDelayMs;
      }
    }
    if (g_rpc_probe.request_spawn_send_count == 0U) {
      if (g_rpc_probe.saw_dialog) {
        trace_netf("rpc-state id=128 auto RequestSpawn suppressed pending_dialog id=%u observe_only=1",
                   static_cast<unsigned int>(g_rpc_probe.last_dialog_id));
      } else if (auto_request_spawn_enabled()) {
        g_rpc_probe.pending_request_spawn = 1;
        g_rpc_probe.next_request_spawn_time = RakNet::GetTime() + kInitialSpawnDelayMs;
        trace_netf("rpc-state id=128 scheduled RequestSpawn delay_ms=%u",
                   static_cast<unsigned int>(kInitialSpawnDelayMs));
      } else {
        trace_netf("rpc-state id=128 auto RequestSpawn disabled observe_only=1");
      }
    }
  } else if (rpc_id == 129U) {
    g_rpc_probe.saw_request_spawn_reply = 1;
    if (rpc_params != nullptr && bytes > 0U) {
      g_rpc_probe.request_spawn_outcome = rpc_params->input[0];
      trace_netf("rpc-state id=129 request_spawn_outcome=%u", static_cast<unsigned int>(g_rpc_probe.request_spawn_outcome));
      if (g_rpc_probe.request_spawn_outcome == 2U || g_rpc_probe.request_spawn_outcome == 1U) {
        send_spawn_notification(g_rpc_probe.client);
      } else if (!g_rpc_probe.saw_dialog && auto_request_spawn_enabled() &&
                 g_rpc_probe.request_spawn_retry_count < kMaxSpawnRetries) {
        ++g_rpc_probe.request_spawn_retry_count;
        g_rpc_probe.pending_request_spawn = 1;
        g_rpc_probe.next_request_spawn_time = RakNet::GetTime() + kSpawnRetryDelayMs;
        trace_netf("rpc-state id=129 scheduled RequestSpawn retry=%u delay_ms=%u",
                   g_rpc_probe.request_spawn_retry_count, static_cast<unsigned int>(kSpawnRetryDelayMs));
      }
    }
  } else if (rpc_id == 68U) {
    if (rpc_params != nullptr && bytes >= kPlayerSpawnInfoBytes) {
      read_spawn_info(rpc_params->input, bytes, "ScrSetSpawnInfo");
    } else {
      g_rpc_probe.saw_spawn_info = 1;
    }
  } else if (rpc_id == 12U) {
    if (rpc_params != nullptr && bytes >= 12U) {
      g_rpc_probe.saw_player_pos = 1;
      read_vec3(rpc_params->input, g_rpc_probe.player_pos);
      trace_netf("rpc-state id=12 player_pos=%.3f %.3f %.3f observe_only=1",
                 static_cast<double>(g_rpc_probe.player_pos[0]), static_cast<double>(g_rpc_probe.player_pos[1]),
                 static_cast<double>(g_rpc_probe.player_pos[2]));
    }
  } else if (rpc_id == 19U) {
    if (rpc_params != nullptr && bytes >= 4U) {
      g_rpc_probe.saw_player_facing = 1;
      g_rpc_probe.player_facing_angle = read_le_float(rpc_params->input);
      trace_netf("rpc-state id=19 player_facing=%.3f observe_only=1",
                 static_cast<double>(g_rpc_probe.player_facing_angle));
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
      show_manual_dialog_window();
    }
  } else if (rpc_id == 152U) {
    if (rpc_params != nullptr && bytes >= 1U) {
      g_rpc_probe.saw_weather = 1;
      g_rpc_probe.weather = rpc_params->input[0];
      trace_netf("rpc-state id=152 weather=%u observe_only=1", static_cast<unsigned int>(g_rpc_probe.weather));
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

    last_packet_id = static_cast<int>(get_packet_id(packet));
    if (autojoin && last_packet_id == static_cast<int>(RakNet::ID_AUTH_KEY)) {
      send_auth_key_response(rak_client, packet);
    } else if (autojoin && !join_sent && last_packet_id == static_cast<int>(RakNet::ID_CONNECTION_REQUEST_ACCEPTED)) {
      join_sent = send_client_join(rak_client, packet, profile);
    }

    rak_client->DeallocatePacket(packet);
    service_rpc_probe_actions(rak_client);
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
  if (g_rpc_probe.sent_request_class) {
    flags |= SAMP_RAKNET_RPC_FLAG_REQUEST_CLASS_SENT;
  }
  if (g_rpc_probe.request_spawn_send_count > 0U) {
    flags |= SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_SENT;
  }

  out_snapshot->flags = flags;
  out_snapshot->client_message_count = 0U;
  out_snapshot->request_class_outcome = g_rpc_probe.request_class_outcome;
  out_snapshot->request_spawn_outcome = g_rpc_probe.request_spawn_outcome;
  out_snapshot->last_dialog_id = g_rpc_probe.last_dialog_id;
  std::memcpy(out_snapshot->player_pos, g_rpc_probe.player_pos, sizeof(out_snapshot->player_pos));
  out_snapshot->player_facing_angle = g_rpc_probe.player_facing_angle;
  out_snapshot->weather = g_rpc_probe.weather;
  out_snapshot->interior = g_rpc_probe.interior;
  std::memcpy(out_snapshot->camera_pos, g_rpc_probe.camera_pos, sizeof(out_snapshot->camera_pos));
  std::memcpy(out_snapshot->camera_look_at, g_rpc_probe.camera_look_at, sizeof(out_snapshot->camera_look_at));
  out_snapshot->camera_look_at_type = g_rpc_probe.camera_look_at_type;
  out_snapshot->spawn_team = g_rpc_probe.spawn_team;
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
  return 0;
}
