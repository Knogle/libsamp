#include <windows.h>
#include <winsock2.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONTROL_PORT 18737
#define CONTROL_TOKEN "reloop-local-v1"
#define MAX_COMMAND 2048

static HANDLE g_stop_event;
static HANDLE g_thread;
static HMODULE g_module;
static FILE *g_log;

static void log_line(const char *text) {
  SYSTEMTIME now;
  GetLocalTime(&now);
  if (g_log == NULL) {
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(g_module, path, sizeof(path));
    if (length > 0 && length < sizeof(path)) {
      char *slash = strrchr(path, '\\');
      if (slash != NULL) strcpy(slash + 1, "reloop_control.log");
      g_log = fopen(path, "a");
    }
  }
  if (g_log != NULL) {
    fprintf(g_log, "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\n",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
            now.wSecond, now.wMilliseconds, text);
    fflush(g_log);
  }
}

static int readable(uintptr_t address, size_t size) {
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t end;
  if (address == 0 || size == 0 || address > UINTPTR_MAX - size) return 0;
  if (VirtualQuery((const void *)address, &mbi, sizeof(mbi)) != sizeof(mbi)) return 0;
  end = address + size;
  if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS ||
      (mbi.Protect & PAGE_GUARD) != 0) return 0;
  return end <= (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
}

static unsigned read_u8(uintptr_t address, unsigned fallback) {
  return readable(address, 1) ? *(const volatile uint8_t *)address : fallback;
}

static unsigned read_u16(uintptr_t address, unsigned fallback) {
  return readable(address, 2) ? *(const volatile uint16_t *)address : fallback;
}

static float read_f32(uintptr_t address) {
  return readable(address, sizeof(float)) ? *(const volatile float *)address : 0.0f;
}

static int read_player_position(float *x, float *y, float *z) {
  uintptr_t ped;
  uintptr_t matrix;
  if (!readable(0x00B7CD98u, sizeof(uintptr_t))) return 0;
  ped = *(const volatile uintptr_t *)0x00B7CD98u;
  if (!readable(ped + 0x14u, sizeof(uintptr_t))) return 0;
  matrix = *(const volatile uintptr_t *)(ped + 0x14u);
  if (!readable(matrix + 0x30u, 3u * sizeof(float))) return 0;
  *x = *(const volatile float *)(matrix + 0x30u);
  *y = *(const volatile float *)(matrix + 0x34u);
  *z = *(const volatile float *)(matrix + 0x38u);
  return 1;
}

static HWND game_window(void) {
  HWND window = NULL;
  if (readable(0x00C97C1Cu, sizeof(window))) window = *(HWND const volatile *)0x00C97C1Cu;
  return IsWindow(window) ? window : GetForegroundWindow();
}

static int json_int(const char *line, const char *name, int fallback) {
  char needle[64];
  const char *value;
  snprintf(needle, sizeof(needle), "\"%s\"", name);
  value = strstr(line, needle);
  if (value == NULL || (value = strchr(value + strlen(needle), ':')) == NULL) return fallback;
  return (int)strtol(value + 1, NULL, 10);
}

static int json_string(const char *line, const char *name, char *out, size_t size) {
  char needle[64];
  const char *value;
  const char *end;
  size_t length;
  if (out == NULL || size == 0) return 0;
  out[0] = '\0';
  snprintf(needle, sizeof(needle), "\"%s\"", name);
  value = strstr(line, needle);
  if (value == NULL || (value = strchr(value + strlen(needle), ':')) == NULL) return 0;
  value++;
  while (*value == ' ' || *value == '\t') value++;
  if (*value++ != '"') return 0;
  end = strchr(value, '"');
  if (end == NULL) return 0;
  length = (size_t)(end - value);
  if (length >= size) length = size - 1;
  memcpy(out, value, length);
  out[length] = '\0';
  return 1;
}

static void send_json(SOCKET client, const char *text) {
  send(client, text, (int)strlen(text), 0);
  send(client, "\n", 1, 0);
}

static void send_state(SOCKET client) {
  char response[1024];
  char input_bytes[16] = "unreadable";
  HWND hwnd = game_window();
  RECT rect = {0};
  POINT cursor = {0};
  CURSORINFO cursor_info;
  unsigned char *input = (unsigned char *)(uintptr_t)0x00541DF5u;
  float player_x = 0.0f, player_y = 0.0f, player_z = 0.0f;
  int player_position_valid = read_player_position(&player_x, &player_y, &player_z);
  memset(&cursor_info, 0, sizeof(cursor_info));
  cursor_info.cbSize = sizeof(cursor_info);
  GetClientRect(hwnd, &rect);
  GetCursorPos(&cursor);
  GetCursorInfo(&cursor_info);
  ScreenToClient(hwnd, &cursor);
  if (readable((uintptr_t)input, 5)) {
    snprintf(input_bytes, sizeof(input_bytes), "%02x%02x%02x%02x%02x",
             input[0], input[1], input[2], input[3], input[4]);
  }
  snprintf(response, sizeof(response),
           "{\"ok\":true,\"event\":\"state\",\"hwnd\":%lu,\"foreground\":%s,"
           "\"client_w\":%ld,\"client_h\":%ld,\"cursor_x\":%ld,\"cursor_y\":%ld,"
           "\"cursor_showing\":%s,"
           "\"hud\":%u,\"radar_blank\":%u,\"camera_mode\":%u,\"camera_mode2\":%u,"
           "\"camera_use_mouse\":%u,\"aim_x\":%.6f,\"aim_y\":%.6f,\"aim_z\":%.6f,"
           "\"player_position_valid\":%s,\"player_x\":%.6f,\"player_y\":%.6f,\"player_z\":%.6f,"
           "\"tab_down\":%s,\"t_down\":%s,\"w_down\":%s,\"input_call\":\"%s\"}",
           (unsigned long)(uintptr_t)hwnd, GetForegroundWindow() == hwnd ? "true" : "false",
           rect.right - rect.left, rect.bottom - rect.top, cursor.x, cursor.y,
           (cursor_info.flags & CURSOR_SHOWING) != 0 ? "true" : "false",
           read_u8(0x00BA6769u, 255), read_u8(0x00BAA3FBu, 255),
           read_u8(0x00B6F1A8u, 255), read_u16(0x00B6F858u, 65535),
           read_u8(0x00B6EC2Eu, 255), read_f32(0x00B6F338u),
           read_f32(0x00B6F33Cu), read_f32(0x00B6F340u),
           player_position_valid ? "true" : "false", player_x, player_y, player_z,
           (GetAsyncKeyState(VK_TAB) & 0x8000) ? "true" : "false",
           (GetAsyncKeyState('T') & 0x8000) ? "true" : "false",
           (GetAsyncKeyState('W') & 0x8000) ? "true" : "false", input_bytes);
  send_json(client, response);
}

static void post_key(HWND hwnd, int vk, int down) {
  UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
  (void)hwnd;
  keybd_event((BYTE)vk, (BYTE)scan, down ? 0 : KEYEVENTF_KEYUP, 0);
}

static void post_mouse(HWND hwnd, int x, int y, const char *action) {
  POINT screen = {x, y};
  LPARAM point = MAKELPARAM((short)x, (short)y);
  ClientToScreen(hwnd, &screen);
  SetCursorPos(screen.x, screen.y);
  PostMessageA(hwnd, WM_MOUSEMOVE, 0, point);
  if (strcmp(action, "click") == 0) {
    PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point);
    PostMessageA(hwnd, WM_LBUTTONUP, 0, point);
  } else if (strcmp(action, "double_click") == 0) {
    PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point);
    PostMessageA(hwnd, WM_LBUTTONUP, 0, point);
  } else if (strcmp(action, "right_click") == 0) {
    PostMessageA(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, point);
    PostMessageA(hwnd, WM_RBUTTONUP, 0, point);
    PostMessageA(hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, point);
    PostMessageA(hwnd, WM_LBUTTONUP, 0, point);
  } else if (strcmp(action, "move_delta") == 0) {
    SetCursorPos(screen.x + x, screen.y + y);
  }
}

static void handle_command(SOCKET client, const char *line) {
  char command[32];
  char token[64];
  char action[32];
  HWND hwnd = game_window();
  if (!json_string(line, "token", token, sizeof(token)) || strcmp(token, CONTROL_TOKEN) != 0) {
    send_json(client, "{\"ok\":false,\"error\":\"unauthorized\"}");
    return;
  }
  if (!json_string(line, "cmd", command, sizeof(command))) {
    send_json(client, "{\"ok\":false,\"error\":\"missing_cmd\"}");
  } else if (strcmp(command, "ping") == 0) {
    send_json(client, "{\"ok\":true,\"event\":\"pong\",\"api\":1}");
  } else if (strcmp(command, "state") == 0) {
    send_state(client);
  } else if (strcmp(command, "focus") == 0) {
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    send_json(client, "{\"ok\":true,\"event\":\"focus\"}");
  } else if (strcmp(command, "key") == 0) {
    int vk = json_int(line, "vk", 0);
    if (!json_string(line, "action", action, sizeof(action)) || vk < 1 || vk > 255) {
      send_json(client, "{\"ok\":false,\"error\":\"bad_key\"}");
    } else {
      if (strcmp(action, "tap") == 0) { post_key(hwnd, vk, 1); Sleep(35); post_key(hwnd, vk, 0); }
      else post_key(hwnd, vk, strcmp(action, "down") == 0);
      send_json(client, "{\"ok\":true,\"event\":\"key\"}");
    }
  } else if (strcmp(command, "window_key") == 0) {
    int vk = json_int(line, "vk", 0);
    UINT scan = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    PostMessageA(hwnd, WM_KEYDOWN, (WPARAM)vk, 1 | ((LPARAM)scan << 16));
    PostMessageA(hwnd, WM_KEYUP, (WPARAM)vk,
                 1 | ((LPARAM)scan << 16) | (LPARAM)0xC0000000u);
    send_json(client, "{\"ok\":true,\"event\":\"window_key\"}");
  } else if (strcmp(command, "char") == 0) {
    int code = json_int(line, "code", 0);
    PostMessageA(hwnd, WM_CHAR, (WPARAM)code, 1);
    send_json(client, "{\"ok\":true,\"event\":\"char\"}");
  } else if (strcmp(command, "mouse") == 0) {
    if (!json_string(line, "action", action, sizeof(action))) strcpy(action, "move");
    post_mouse(hwnd, json_int(line, "x", 0), json_int(line, "y", 0), action);
    send_json(client, "{\"ok\":true,\"event\":\"mouse\"}");
  } else {
    send_json(client, "{\"ok\":false,\"error\":\"unknown_cmd\"}");
  }
}

static DWORD WINAPI server_thread(void *unused) {
  WSADATA wsa;
  SOCKET listener = INVALID_SOCKET;
  struct sockaddr_in address;
  (void)unused;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) goto done;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(CONTROL_PORT);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listener, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR ||
      listen(listener, 1) == SOCKET_ERROR) goto done;
  log_line("listen=127.0.0.1:18737 protocol=newline-json api=1");
  while (WaitForSingleObject(g_stop_event, 0) == WAIT_TIMEOUT) {
    fd_set reads;
    struct timeval timeout = {0, 200000};
    SOCKET client;
    FD_ZERO(&reads); FD_SET(listener, &reads);
    if (select(0, &reads, NULL, NULL, &timeout) <= 0) continue;
    client = accept(listener, NULL, NULL);
    if (client != INVALID_SOCKET) {
      char buffer[MAX_COMMAND];
      int used = 0;
      int received;
      while ((received = recv(client, buffer + used, (int)sizeof(buffer) - used - 1, 0)) > 0) {
        char *line;
        char *newline;
        used += received; buffer[used] = '\0'; line = buffer;
        while ((newline = strchr(line, '\n')) != NULL) {
          *newline = '\0'; handle_command(client, line); line = newline + 1;
        }
        used -= (int)(line - buffer); memmove(buffer, line, (size_t)used);
        if (used == (int)sizeof(buffer) - 1) used = 0;
      }
      closesocket(client);
    }
  }
done:
  if (listener != INVALID_SOCKET) closesocket(listener);
  WSACleanup();
  return 0;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {
    g_module = module;
    DisableThreadLibraryCalls(module);
    g_stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    g_thread = CreateThread(NULL, 0, server_thread, NULL, 0, NULL);
  } else if (reason == DLL_PROCESS_DETACH) {
    if (g_stop_event != NULL) SetEvent(g_stop_event);
    if (g_thread != NULL) CloseHandle(g_thread);
    if (g_stop_event != NULL) CloseHandle(g_stop_event);
    if (g_log != NULL) fclose(g_log);
  }
  return TRUE;
}
