#include <windows.h>
#include <winsock.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROBE_LOG_NAME "samp_probe.log"
#define PROBE_NO_HOOKS_FLAG "samp_probe_no_hooks.flag"
#define PROBE_ASSET_PATHS_FLAG "samp_probe_asset_paths.flag"
#define PROBE_FILE_HOOKS_FLAG "samp_probe_file_hooks.flag"
#define PROBE_SAMP_CODE_HOOKS_FLAG "samp_probe_samp_code_hooks.flag"
#define PROBE_GTA_ASSET_HOOKS_FLAG "samp_probe_gta_asset_hooks.flag"
#define PROBE_OBJECT_INFO_FLAG "samp_probe_object_info.flag"
#define PROBE_CUSTOM_OBJECT_HEAVY_FLAG "samp_probe_custom_object_heavy.flag"
#define PROBE_TEXTDRAW_HOOKS_FLAG "samp_probe_textdraw_hooks.flag"
#define PROBE_TEXTDRAW_VERBOSE_FLAG "samp_probe_textdraw_verbose.flag"
#define PROBE_TEXTDRAW_RENDER_FLAG "samp_probe_textdraw_render.flag"
#define PROBE_FONT5_HOOKS_FLAG "samp_probe_font5_hooks.flag"
#define PROBE_MAX_IMPORT_LOGS 4096
#define PROBE_WATCH_INTERVAL_MS 250
#define PROBE_WAIT_FOR_SAMP_MS 30000
#define PROBE_INLINE_HOOK_MAX_COPY 16
#define PROBE_PAYLOAD_PREVIEW_BYTES 256
#define PROBE_STATE_HEARTBEAT_MS 5000
#define PROBE_ASSET_HANDLE_SLOTS 64
#define PROBE_TEXTURE_TRACK_SLOTS 1024
#define PROBE_TEXTDRAW_RENDER_HINT_MS 750u

#define PROBE_GTA_ADDR_MODEL_INFO_PTRS 0x00A9B0C8u
#define PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT 0x00AAE950u
#define PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT 0x00B1C960u
#define PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT 0x00B1E958u
#define PROBE_GTA_ADDR_D3D_DEVICE_PTR 0x00C97C28u
#define PROBE_GTA_MODEL_INFO_MAX 20000u
#define PROBE_GTA_CUSTOM_LOW_MODEL_MIN 11682u
#define PROBE_GTA_CUSTOM_LOW_MODEL_MAX 11753u
#define PROBE_GTA_CUSTOM_MODEL_MIN 18631u
#define PROBE_GTA_CUSTOM_MODEL_MAX 19999u
#define PROBE_GTA_MODEL_INFO_OFFSET_VTABLE 0u
#define PROBE_GTA_MODEL_INFO_OFFSET_KEY 4u
#define PROBE_GTA_MODEL_INFO_OFFSET_TXD_INDEX 10u
#define PROBE_GTA_MODEL_INFO_OFFSET_DRAW_DISTANCE 24u
#define PROBE_GTA_MODEL_INFO_OFFSET_COL_MODEL 20u
#define PROBE_GTA_MODEL_INFO_DUMP_BYTES 64u
#define PROBE_GTA_COL_MODEL_DUMP_BYTES 64u
#define PROBE_GTA_ENTITY_OFFSET_RW_OBJECT 24u
#define PROBE_GTA_ENTITY_OFFSET_MODEL_INDEX 34u
#define PROBE_GTA_ENTITY_OFFSET_STATUS 0x36u
#define PROBE_GTA_PHYSICAL_OFFSET_SECTOR_LIST 0xb0u
#define PROBE_D3DDEV_VTBL_SET_RENDER_STATE 57u
#define PROBE_D3DDEV_VTBL_SET_TEXTURE 65u
#define PROBE_D3DDEV_VTBL_SET_FVF 89u
#define PROBE_D3DDEV_VTBL_CREATE_TEXTURE 23u
#define PROBE_D3DDEV_VTBL_CREATE_RENDER_TARGET 28u
#define PROBE_D3DDEV_VTBL_CREATE_DEPTH_STENCIL_SURFACE 29u
#define PROBE_D3DDEV_VTBL_STRETCH_RECT 34u
#define PROBE_D3DDEV_VTBL_SET_RENDER_TARGET 37u
#define PROBE_D3DDEV_VTBL_GET_RENDER_TARGET 38u
#define PROBE_D3DDEV_VTBL_SET_DEPTH_STENCIL_SURFACE 39u
#define PROBE_D3DDEV_VTBL_GET_DEPTH_STENCIL_SURFACE 40u
#define PROBE_D3DDEV_VTBL_CLEAR 43u
#define PROBE_D3DDEV_VTBL_SET_VIEWPORT 47u
#define PROBE_D3DDEV_VTBL_DRAW_PRIMITIVE 81u
#define PROBE_D3DDEV_VTBL_DRAW_INDEXED_PRIMITIVE 82u
#define PROBE_D3DDEV_VTBL_DRAW_PRIMITIVE_UP 83u
#define PROBE_D3DDEV_VTBL_DRAW_INDEXED_PRIMITIVE_UP 84u
#define PROBE_D3DXSPRITE_VTBL_BEGIN 8u
#define PROBE_D3DXSPRITE_VTBL_DRAW 9u
#define PROBE_D3DXSPRITE_VTBL_END 11u
#define PROBE_D3DTEXTURE_VTBL_GET_SURFACE_LEVEL 18u
#define PROBE_D3DUSAGE_RENDERTARGET 0x00000001u

/* STATIC_037 + PROBE_TRACE: exact local SA-MP 0.3.7-R5 reference identity
 * proxy.  The PE COFF TimeDateStamp is the raw UTC value from the image;
 * do not reconstruct it from objdump's localized display time.
 * SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2. */
#define PROBE_SAMP_R5_TIMESTAMP 0x6372c39eu
#define PROBE_SAMP_R5_ENTRY_RVA 0x000cbc90u
#define PROBE_SAMP_R5_IMAGE_SIZE 0x0027e000u
#define PROBE_SAMP_R5_FONT5_PREPARE_RVA 0x000b34a0u
#define PROBE_SAMP_R5_FONT5_DRAW_DISPATCH_RVA 0x000b3480u

#if defined(__GNUC__)
#define PROBE_ALWAYS_INLINE static __inline__ __attribute__((always_inline))
#define PROBE_THREAD_LOCAL __thread
#define PROBE_THISCALL __attribute__((thiscall))
#else
#define PROBE_ALWAYS_INLINE static __inline
#define PROBE_THREAD_LOCAL __declspec(thread)
#define PROBE_THISCALL __thiscall
#endif

typedef int(WINAPI *probe_WSAStartup_fn)(WORD, LPWSADATA);
typedef int(WINAPI *probe_WSACleanup_fn)(void);
typedef SOCKET(WINAPI *probe_socket_fn)(int, int, int);
typedef int(WINAPI *probe_bind_fn)(SOCKET, const struct sockaddr *, int);
typedef int(WINAPI *probe_connect_fn)(SOCKET, const struct sockaddr *, int);
typedef int(WINAPI *probe_closesocket_fn)(SOCKET);
typedef struct hostent *(WINAPI *probe_gethostbyname_fn)(const char *);
typedef int(WINAPI *probe_gethostname_fn)(char *, int);
typedef unsigned long(WINAPI *probe_inet_addr_fn)(const char *);
typedef char *(WINAPI *probe_inet_ntoa_fn)(struct in_addr);
typedef int(WINAPI *probe_send_fn)(SOCKET, const char *, int, int);
typedef int(WINAPI *probe_recv_fn)(SOCKET, char *, int, int);
typedef int(WINAPI *probe_sendto_fn)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int(WINAPI *probe_recvfrom_fn)(SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int(WINAPI *probe_select_fn)(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
typedef int(WINAPI *probe_WSAGetLastError_fn)(void);
typedef BOOL(WINAPI *probe_VirtualProtect_fn)(LPVOID, SIZE_T, DWORD, PDWORD);
typedef HMODULE(WINAPI *probe_LoadLibraryA_fn)(LPCSTR);
typedef HMODULE(WINAPI *probe_LoadLibraryW_fn)(LPCWSTR);
typedef FARPROC(WINAPI *probe_GetProcAddress_fn)(HMODULE, LPCSTR);
typedef HANDLE(WINAPI *probe_CreateThread_fn)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef HANDLE(WINAPI *probe_CreateFileA_fn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE(WINAPI *probe_CreateFileW_fn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL(WINAPI *probe_ReadFile_fn)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD(WINAPI *probe_SetFilePointer_fn)(HANDLE, LONG, PLONG, DWORD);
typedef DWORD(WINAPI *probe_GetFileSize_fn)(HANDLE, LPDWORD);
typedef BOOL(WINAPI *probe_CloseHandle_fn)(HANDLE);
typedef int(WINAPI *probe_samp_socketlayer_sendto_fn)(SOCKET, const char *, int, unsigned long, unsigned short);
typedef void(WINAPI *probe_samp_process_network_packet_fn)(unsigned long, unsigned short, const char *, int, void *);
typedef void *(__cdecl *probe_gta_add_model_fn)(int);
typedef int(__cdecl *probe_gta_add_image_to_list_fn)(const char *, int);
typedef void(__cdecl *probe_gta_load_cd_directory_fn)(const char *, int);
typedef int(__cdecl *probe_gta_col_add_slot_fn)(const char *);
typedef int(__cdecl *probe_gta_col_load_buffer_fn)(int, void *, int);
typedef void(PROBE_THISCALL *probe_gta_physical_add_fn)(void *);
typedef void(PROBE_THISCALL *probe_samp_font5_method_fn)(void *);
typedef void(__cdecl *probe_gta_font_set_scale_fn)(float, float);
typedef void(__cdecl *probe_gta_font_set_color_fn)(DWORD);
typedef void(__cdecl *probe_gta_font_set_int_fn)(int);
typedef void(__cdecl *probe_gta_font_set_float_fn)(float);
typedef void(__cdecl *probe_gta_font_use_box_fn)(int, int);
typedef void(__cdecl *probe_gta_font_print_string_fn)(float, float, char *);
typedef HRESULT(WINAPI *probe_D3DXCreateSprite_fn)(void *, void **);
typedef HRESULT(WINAPI *probe_D3DXCreateTexture_fn)(void *, UINT, UINT, UINT, DWORD, int, int, void **);
typedef HRESULT(WINAPI *probe_D3DXCreateTextureFromFileA_fn)(void *, LPCSTR, void **);
typedef HRESULT(WINAPI *probe_D3DXCreateTextureFromFileExA_fn)(void *, LPCSTR, UINT, UINT, UINT, DWORD, int, int,
                                                              DWORD, DWORD, DWORD, void *, void *, void **);
typedef HRESULT(WINAPI *probe_D3DXCreateTextureFromFileInMemory_fn)(void *, LPCVOID, UINT, void **);
typedef HRESULT(WINAPI *probe_D3DXCreateTextureFromResourceExA_fn)(void *, HMODULE, LPCSTR, UINT, UINT, UINT, DWORD,
                                                                  int, int, DWORD, DWORD, DWORD, void *, void *,
                                                                  void **);
typedef HRESULT(WINAPI *probe_d3dx_sprite_begin_fn)(void *, DWORD);
typedef HRESULT(WINAPI *probe_d3dx_sprite_end_fn)(void *);
typedef struct probe_d3dx_vec3 {
  float x;
  float y;
  float z;
} probe_d3dx_vec3;
typedef HRESULT(WINAPI *probe_d3dx_sprite_draw_fn)(void *, void *, const RECT *, const probe_d3dx_vec3 *,
                                                  const probe_d3dx_vec3 *, DWORD);
typedef HRESULT(WINAPI *probe_d3d_set_render_state_fn)(void *, DWORD, DWORD);
typedef HRESULT(WINAPI *probe_d3d_set_texture_fn)(void *, DWORD, void *);
typedef HRESULT(WINAPI *probe_d3d_set_fvf_fn)(void *, DWORD);
typedef struct probe_d3d_viewport {
  DWORD x;
  DWORD y;
  DWORD width;
  DWORD height;
  float min_z;
  float max_z;
} probe_d3d_viewport;
typedef HRESULT(WINAPI *probe_d3d_create_texture_fn)(void *, UINT, UINT, UINT, DWORD, int, int, void **, HANDLE *);
typedef HRESULT(WINAPI *probe_d3d_create_render_target_fn)(void *, UINT, UINT, int, int, DWORD, BOOL, void **,
                                                           HANDLE *);
typedef HRESULT(WINAPI *probe_d3d_create_depth_stencil_surface_fn)(void *, UINT, UINT, int, int, DWORD, BOOL,
                                                                  void **, HANDLE *);
typedef HRESULT(WINAPI *probe_d3d_stretch_rect_fn)(void *, void *, const RECT *, void *, const RECT *, int);
typedef HRESULT(WINAPI *probe_d3d_set_render_target_fn)(void *, DWORD, void *);
typedef HRESULT(WINAPI *probe_d3d_get_render_target_fn)(void *, DWORD, void **);
typedef HRESULT(WINAPI *probe_d3d_set_depth_stencil_surface_fn)(void *, void *);
typedef HRESULT(WINAPI *probe_d3d_get_depth_stencil_surface_fn)(void *, void **);
typedef HRESULT(WINAPI *probe_d3d_clear_fn)(void *, DWORD, const void *, DWORD, DWORD, float, DWORD);
typedef HRESULT(WINAPI *probe_d3d_set_viewport_fn)(void *, const probe_d3d_viewport *);
typedef HRESULT(WINAPI *probe_d3d_texture_get_surface_level_fn)(void *, UINT, void **);
typedef HRESULT(WINAPI *probe_d3d_draw_primitive_fn)(void *, DWORD, UINT, UINT);
typedef HRESULT(WINAPI *probe_d3d_draw_indexed_primitive_fn)(void *, DWORD, INT, UINT, UINT, UINT, UINT);
typedef HRESULT(WINAPI *probe_d3d_draw_primitive_up_fn)(void *, DWORD, UINT, const void *, UINT);
typedef HRESULT(WINAPI *probe_d3d_draw_indexed_primitive_up_fn)(void *, DWORD, UINT, UINT, UINT, const void *, DWORD,
                                                               const void *, UINT);

typedef struct probe_iat_hook {
  const char *dll_name;
  const char *symbol_name;
  WORD ordinal;
  void *replacement;
  void **original_slot;
  LONG install_count;
} probe_iat_hook;

typedef struct probe_watchpoint {
  const char *name;
  uintptr_t address;
  size_t size;
  BYTE previous[16];
  int has_previous;
} probe_watchpoint;

typedef struct probe_transition_state {
  DWORD entry_gate;
  DWORD hwnd;
  DWORD id3d9;
  DWORD id3d9_device;
  DWORD game_process_storage;
  DWORD render2d_storage;
  DWORD ped_table;
  DWORD vehicle_table;
  DWORD script_call_target;
  DWORD graphics_call_target;
  DWORD samp_base;
  DWORD samp_size;
  BYTE start_game;
  BYTE game_started;
  BYTE menu;
  BYTE menu2;
  BYTE menu3;
  BYTE hud;
  BYTE time_patch;
  BYTE script_gate0;
  BYTE script_gate1;
  BYTE script_call_opcode;
  BYTE graphics_call_opcode;
  BYTE current_player;
  BYTE camera_mode;
  WORD camera_mode2;
} probe_transition_state;

typedef struct probe_inline_hook {
  const char *dll_name;
  const char *symbol_name;
  void *replacement;
  void **original_slot;
  void *target;
  void *patch_target;
  void *trampoline;
  BYTE saved[PROBE_INLINE_HOOK_MAX_COPY];
  size_t saved_len;
  LONG installed;
} probe_inline_hook;

typedef struct probe_code_hook {
  const char *name;
  DWORD rva;
  void *replacement;
  void **original_slot;
  BYTE expected[16];
  size_t expected_len;
  void *patch_target;
  void *trampoline;
  BYTE saved[PROBE_INLINE_HOOK_MAX_COPY];
  size_t saved_len;
  LONG installed;
} probe_code_hook;

typedef struct probe_asset_handle {
  HANDLE handle;
  char path[MAX_PATH];
  LONG read_count;
} probe_asset_handle;

typedef struct probe_texture_record {
  void *texture;
  void *level0_surface;
  UINT width;
  UINT height;
  DWORD usage;
  char source[192];
  LONG draw_count;
} probe_texture_record;

static HMODULE g_self_module;
static HANDLE g_worker_thread;
static HANDLE g_stop_event;
static CRITICAL_SECTION g_log_lock;
static LONG g_log_ready;
static char g_log_path[MAX_PATH];
static HMODULE g_samp_module;
static uintptr_t g_samp_base;
static DWORD g_samp_size;
static LONG g_import_log_count;
static PVOID g_exception_handler;
static LONG g_exception_logged;
static probe_transition_state g_previous_state;
static LONG g_previous_state_valid;
static DWORD g_previous_state_log_ms;

static void *g_orig_WSAStartup;
static void *g_orig_WSACleanup;
static void *g_orig_socket;
static void *g_orig_bind;
static void *g_orig_connect;
static void *g_orig_closesocket;
static void *g_orig_gethostbyname;
static void *g_orig_gethostname;
static void *g_orig_inet_addr;
static void *g_orig_inet_ntoa;
static void *g_orig_send;
static void *g_orig_recv;
static void *g_orig_sendto;
static void *g_orig_recvfrom;
static void *g_orig_select;
static void *g_orig_VirtualProtect;
static void *g_orig_LoadLibraryA;
static void *g_orig_LoadLibraryW;
static void *g_orig_GetProcAddress;
static void *g_orig_CreateThread;
static void *g_orig_CreateFileA;
static void *g_orig_CreateFileW;
static void *g_orig_ReadFile;
static void *g_orig_SetFilePointer;
static void *g_orig_GetFileSize;
static void *g_orig_CloseHandle;
static void *g_orig_samp_socketlayer_sendto;
static void *g_orig_samp_process_network_packet;
static void *g_orig_gta_add_atomic_model;
static void *g_orig_gta_add_time_model;
static void *g_orig_gta_add_clump_model;
static void *g_orig_gta_add_image_to_list;
static void *g_orig_gta_load_cd_directory;
static void *g_orig_gta_col_add_slot;
static void *g_orig_gta_col_load_buffer;
static void *g_orig_gta_physical_add;
static void *g_orig_samp_font5_prepare;
static void *g_orig_samp_font5_draw_dispatch;
static void *g_orig_gta_font_set_scale;
static void *g_orig_gta_font_set_color;
static void *g_orig_gta_font_set_style;
static void *g_orig_gta_font_set_line_width;
static void *g_orig_gta_font_set_line_height;
static void *g_orig_gta_font_set_drop_color;
static void *g_orig_gta_font_set_shadow;
static void *g_orig_gta_font_set_outline;
static void *g_orig_gta_font_set_proportional;
static void *g_orig_gta_font_use_box;
static void *g_orig_gta_font_use_box_color;
static void *g_orig_gta_font_unk12;
static void *g_orig_gta_font_set_justify;
static void *g_orig_gta_font_print_string;
static void *g_orig_D3DXCreateSprite;
static void *g_orig_D3DXCreateTexture;
static void *g_orig_D3DXCreateTextureFromFileA;
static void *g_orig_D3DXCreateTextureFromFileExA;
static void *g_orig_D3DXCreateTextureFromFileInMemory;
static void *g_orig_D3DXCreateTextureFromResourceExA;
static void *g_orig_d3dx_sprite_begin;
static void *g_orig_d3dx_sprite_draw;
static void *g_orig_d3dx_sprite_end;
static void *g_orig_d3d_set_render_state;
static void *g_orig_d3d_set_texture;
static void *g_orig_d3d_set_fvf;
static void *g_orig_d3d_create_texture;
static void *g_orig_d3d_create_render_target;
static void *g_orig_d3d_create_depth_stencil_surface;
static void *g_orig_d3d_stretch_rect;
static void *g_orig_d3d_set_render_target;
static void *g_orig_d3d_get_render_target;
static void *g_orig_d3d_set_depth_stencil_surface;
static void *g_orig_d3d_get_depth_stencil_surface;
static void *g_orig_d3d_clear;
static void *g_orig_d3d_set_viewport;
static void *g_orig_d3d_texture_get_surface_level;
static void *g_orig_d3d_draw_primitive;
static void *g_orig_d3d_draw_indexed_primitive;
static void *g_orig_d3d_draw_primitive_up;
static void *g_orig_d3d_draw_indexed_primitive_up;
static probe_WSAGetLastError_fn g_WSAGetLastError_ptr;
static probe_asset_handle g_asset_handles[PROBE_ASSET_HANDLE_SLOTS];
static probe_texture_record g_texture_records[PROBE_TEXTURE_TRACK_SLOTS];
static BYTE g_model_info_physical_dumped[PROBE_GTA_MODEL_INFO_MAX];
static PROBE_THREAD_LOCAL int g_file_hook_depth;
static PROBE_THREAD_LOCAL int g_textdraw_render_hook_depth;
static PROBE_THREAD_LOCAL int g_font5_trace_depth;
static PROBE_THREAD_LOCAL LONG g_font5_active_seq;
static LONG g_textdraw_font_call_count;
static LONG g_textdraw_render_call_count;
static LONG g_font5_trace_call_count;
static LONG g_textdraw_current_font_style = -1;
static LONG g_d3d_device_hooks_installed;
static DWORD g_textdraw_render_hint_until_ms;
static void *g_d3d_device_vtable;
static void *g_d3d_stage0_texture;
static void *g_d3d_current_render_target;
static void *g_d3d_current_depth_stencil;

static int env_or_flag_enabled(const char *env_name, const char *flag_name);
static int asset_path_hooks_enabled(void);
static int asset_read_hooks_enabled(void);
static int samp_code_hooks_enabled(void);
static int gta_asset_hooks_enabled(void);
static int object_info_enabled(void);
static int custom_object_heavy_enabled(void);
static int textdraw_hooks_enabled(void);
static int textdraw_verbose_enabled(void);
static int textdraw_render_enabled(void);
static int font5_hooks_enabled(void);
static PIMAGE_NT_HEADERS get_samp_nt_headers(void);
static LONG CALLBACK probe_exception_handler(PEXCEPTION_POINTERS info);
static int WINAPI hook_WSAStartup(WORD version, LPWSADATA data);
static int WINAPI hook_WSACleanup(void);
static SOCKET WINAPI hook_socket(int af, int type, int protocol);
static int WINAPI hook_bind(SOCKET s, const struct sockaddr *name, int namelen);
static int WINAPI hook_connect(SOCKET s, const struct sockaddr *name, int namelen);
static int WINAPI hook_closesocket(SOCKET s);
static struct hostent *WINAPI hook_gethostbyname(const char *name);
static int WINAPI hook_gethostname(char *name, int namelen);
static unsigned long WINAPI hook_inet_addr(const char *cp);
static char *WINAPI hook_inet_ntoa(struct in_addr in);
static int WINAPI hook_send(SOCKET s, const char *buf, int len, int flags);
static int WINAPI hook_recv(SOCKET s, char *buf, int len, int flags);
static int WINAPI hook_sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
static int WINAPI hook_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
static int WINAPI hook_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout);
static BOOL WINAPI hook_VirtualProtect(LPVOID address, SIZE_T size, DWORD new_protect, PDWORD old_protect);
static HMODULE WINAPI hook_LoadLibraryA(LPCSTR file_name);
static HMODULE WINAPI hook_LoadLibraryW(LPCWSTR file_name);
static FARPROC WINAPI hook_GetProcAddress(HMODULE module, LPCSTR proc_name);
static HANDLE WINAPI hook_CreateThread(LPSECURITY_ATTRIBUTES attr, SIZE_T stack_size, LPTHREAD_START_ROUTINE start, LPVOID param,
                                       DWORD flags, LPDWORD thread_id);
static HANDLE WINAPI hook_CreateFileA(LPCSTR file_name, DWORD desired_access, DWORD share_mode,
                                      LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                      DWORD flags_and_attributes, HANDLE template_file);
static HANDLE WINAPI hook_CreateFileW(LPCWSTR file_name, DWORD desired_access, DWORD share_mode,
                                      LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                      DWORD flags_and_attributes, HANDLE template_file);
static BOOL WINAPI hook_ReadFile(HANDLE file, LPVOID buffer, DWORD bytes_to_read, LPDWORD bytes_read, LPOVERLAPPED overlapped);
static DWORD WINAPI hook_SetFilePointer(HANDLE file, LONG distance_to_move, PLONG distance_to_move_high, DWORD move_method);
static DWORD WINAPI hook_GetFileSize(HANDLE file, LPDWORD file_size_high);
static BOOL WINAPI hook_CloseHandle(HANDLE object);
static int WINAPI hook_samp_socketlayer_sendto(SOCKET s, const char *buf, int len, unsigned long binary_address,
                                               unsigned short port);
static void WINAPI hook_samp_process_network_packet(unsigned long binary_address, unsigned short port, const char *data, int len,
                                                    void *rak_peer);
static void *__cdecl hook_gta_add_atomic_model(int model_id);
static void *__cdecl hook_gta_add_time_model(int model_id);
static void *__cdecl hook_gta_add_clump_model(int model_id);
static int __cdecl hook_gta_add_image_to_list(const char *path, int not_player_img);
static void __cdecl hook_gta_load_cd_directory(const char *path, int image_id);
static int __cdecl hook_gta_col_add_slot(const char *name);
static int __cdecl hook_gta_col_load_buffer(int slot, void *buffer, int size);
static void PROBE_THISCALL hook_gta_physical_add(void *entity);
static void PROBE_THISCALL hook_samp_font5_prepare(void *textdraw);
static void PROBE_THISCALL hook_samp_font5_draw_dispatch(void *textdraw);
static void __cdecl hook_gta_font_set_scale(float x, float y);
static void __cdecl hook_gta_font_set_color(DWORD color);
static void __cdecl hook_gta_font_set_style(int style);
static void __cdecl hook_gta_font_set_line_width(float width);
static void __cdecl hook_gta_font_set_line_height(float height);
static void __cdecl hook_gta_font_set_drop_color(DWORD color);
static void __cdecl hook_gta_font_set_shadow(int shadow);
static void __cdecl hook_gta_font_set_outline(int outline);
static void __cdecl hook_gta_font_set_proportional(int proportional);
static void __cdecl hook_gta_font_use_box(int use, int unk);
static void __cdecl hook_gta_font_use_box_color(DWORD color);
static void __cdecl hook_gta_font_unk12(int value);
static void __cdecl hook_gta_font_set_justify(int justify);
static void __cdecl hook_gta_font_print_string(float x, float y, char *text);
static HRESULT WINAPI hook_D3DXCreateSprite(void *device, void **sprite);
static HRESULT WINAPI hook_D3DXCreateTexture(void *device, UINT width, UINT height, UINT mip_levels, DWORD usage,
                                             int format, int pool, void **texture);
static HRESULT WINAPI hook_D3DXCreateTextureFromFileA(void *device, LPCSTR src_file, void **texture);
static HRESULT WINAPI hook_D3DXCreateTextureFromFileExA(void *device, LPCSTR src_file, UINT width, UINT height,
                                                       UINT mip_levels, DWORD usage, int format, int pool,
                                                       DWORD filter, DWORD mip_filter, DWORD color_key,
                                                       void *src_info, void *palette, void **texture);
static HRESULT WINAPI hook_D3DXCreateTextureFromFileInMemory(void *device, LPCVOID src_data, UINT src_data_size,
                                                            void **texture);
static HRESULT WINAPI hook_D3DXCreateTextureFromResourceExA(void *device, HMODULE src_module, LPCSTR src_resource,
                                                           UINT width, UINT height, UINT mip_levels, DWORD usage,
                                                           int format, int pool, DWORD filter, DWORD mip_filter,
                                                           DWORD color_key, void *src_info, void *palette,
                                                           void **texture);
static HRESULT WINAPI hook_d3dx_sprite_begin(void *sprite, DWORD flags);
static HRESULT WINAPI hook_d3dx_sprite_draw(void *sprite, void *texture, const RECT *src_rect,
                                           const probe_d3dx_vec3 *center, const probe_d3dx_vec3 *position,
                                           DWORD color);
static HRESULT WINAPI hook_d3dx_sprite_end(void *sprite);
static HRESULT WINAPI hook_d3d_set_render_state(void *device, DWORD state, DWORD value);
static HRESULT WINAPI hook_d3d_set_texture(void *device, DWORD stage, void *texture);
static HRESULT WINAPI hook_d3d_set_fvf(void *device, DWORD fvf);
static HRESULT WINAPI hook_d3d_create_texture(void *device, UINT width, UINT height, UINT levels, DWORD usage,
                                              int format, int pool, void **texture, HANDLE *shared_handle);
static HRESULT WINAPI hook_d3d_create_render_target(void *device, UINT width, UINT height, int format,
                                                    int multi_sample, DWORD multi_sample_quality, BOOL lockable,
                                                    void **surface, HANDLE *shared_handle);
static HRESULT WINAPI hook_d3d_create_depth_stencil_surface(void *device, UINT width, UINT height, int format,
                                                            int multi_sample, DWORD multi_sample_quality,
                                                            BOOL discard, void **surface, HANDLE *shared_handle);
static HRESULT WINAPI hook_d3d_stretch_rect(void *device, void *source_surface, const RECT *source_rect,
                                            void *dest_surface, const RECT *dest_rect, int filter);
static HRESULT WINAPI hook_d3d_set_render_target(void *device, DWORD index, void *surface);
static HRESULT WINAPI hook_d3d_get_render_target(void *device, DWORD index, void **surface);
static HRESULT WINAPI hook_d3d_set_depth_stencil_surface(void *device, void *surface);
static HRESULT WINAPI hook_d3d_get_depth_stencil_surface(void *device, void **surface);
static HRESULT WINAPI hook_d3d_clear(void *device, DWORD count, const void *rects, DWORD flags, DWORD color,
                                     float z, DWORD stencil);
static HRESULT WINAPI hook_d3d_set_viewport(void *device, const probe_d3d_viewport *viewport);
static HRESULT WINAPI hook_d3d_texture_get_surface_level(void *texture, UINT level, void **surface);
static HRESULT WINAPI hook_d3d_draw_primitive(void *device, DWORD primitive_type, UINT start_vertex,
                                             UINT primitive_count);
static HRESULT WINAPI hook_d3d_draw_indexed_primitive(void *device, DWORD primitive_type, INT base_vertex_index,
                                                     UINT min_vertex_index, UINT num_vertices, UINT start_index,
                                                     UINT primitive_count);
static HRESULT WINAPI hook_d3d_draw_primitive_up(void *device, DWORD primitive_type, UINT primitive_count,
                                                const void *vertex_stream_zero_data,
                                                UINT vertex_stream_zero_stride);
static HRESULT WINAPI hook_d3d_draw_indexed_primitive_up(void *device, DWORD primitive_type, UINT min_vertex_index,
                                                        UINT num_vertices, UINT primitive_count,
                                                        const void *index_data, DWORD index_data_format,
                                                        const void *vertex_stream_zero_data,
                                                        UINT vertex_stream_zero_stride);

static probe_iat_hook g_hooks[] = {
    {"WSOCK32.dll", "WSAStartup", 115, (void *)hook_WSAStartup, &g_orig_WSAStartup, 0},
    {"WSOCK32.dll", "WSACleanup", 116, (void *)hook_WSACleanup, &g_orig_WSACleanup, 0},
    {"WSOCK32.dll", "socket", 23, (void *)hook_socket, &g_orig_socket, 0},
    {"WSOCK32.dll", "bind", 2, (void *)hook_bind, &g_orig_bind, 0},
    {"WSOCK32.dll", "connect", 4, (void *)hook_connect, &g_orig_connect, 0},
    {"WSOCK32.dll", "closesocket", 3, (void *)hook_closesocket, &g_orig_closesocket, 0},
    {"WSOCK32.dll", "gethostbyname", 52, (void *)hook_gethostbyname, &g_orig_gethostbyname, 0},
    {"WSOCK32.dll", "gethostname", 57, (void *)hook_gethostname, &g_orig_gethostname, 0},
    {"WSOCK32.dll", "inet_addr", 10, (void *)hook_inet_addr, &g_orig_inet_addr, 0},
    {"WSOCK32.dll", "inet_ntoa", 11, (void *)hook_inet_ntoa, &g_orig_inet_ntoa, 0},
    {"WSOCK32.dll", "send", 19, (void *)hook_send, &g_orig_send, 0},
    {"WSOCK32.dll", "recv", 16, (void *)hook_recv, &g_orig_recv, 0},
    {"WSOCK32.dll", "sendto", 20, (void *)hook_sendto, &g_orig_sendto, 0},
    {"WSOCK32.dll", "recvfrom", 17, (void *)hook_recvfrom, &g_orig_recvfrom, 0},
    {"WSOCK32.dll", "select", 18, (void *)hook_select, &g_orig_select, 0},
    {"WS2_32.dll", "WSAStartup", 0, (void *)hook_WSAStartup, &g_orig_WSAStartup, 0},
    {"WS2_32.dll", "WSACleanup", 0, (void *)hook_WSACleanup, &g_orig_WSACleanup, 0},
    {"WS2_32.dll", "socket", 0, (void *)hook_socket, &g_orig_socket, 0},
    {"WS2_32.dll", "bind", 0, (void *)hook_bind, &g_orig_bind, 0},
    {"WS2_32.dll", "connect", 0, (void *)hook_connect, &g_orig_connect, 0},
    {"WS2_32.dll", "closesocket", 0, (void *)hook_closesocket, &g_orig_closesocket, 0},
    {"WS2_32.dll", "gethostbyname", 0, (void *)hook_gethostbyname, &g_orig_gethostbyname, 0},
    {"WS2_32.dll", "gethostname", 0, (void *)hook_gethostname, &g_orig_gethostname, 0},
    {"WS2_32.dll", "inet_addr", 0, (void *)hook_inet_addr, &g_orig_inet_addr, 0},
    {"WS2_32.dll", "inet_ntoa", 0, (void *)hook_inet_ntoa, &g_orig_inet_ntoa, 0},
    {"WS2_32.dll", "send", 0, (void *)hook_send, &g_orig_send, 0},
    {"WS2_32.dll", "recv", 0, (void *)hook_recv, &g_orig_recv, 0},
    {"WS2_32.dll", "sendto", 0, (void *)hook_sendto, &g_orig_sendto, 0},
    {"WS2_32.dll", "recvfrom", 0, (void *)hook_recvfrom, &g_orig_recvfrom, 0},
    {"WS2_32.dll", "select", 0, (void *)hook_select, &g_orig_select, 0},
    {"KERNEL32.dll", "VirtualProtect", 0, (void *)hook_VirtualProtect, &g_orig_VirtualProtect, 0},
    {"KERNEL32.dll", "LoadLibraryA", 0, (void *)hook_LoadLibraryA, &g_orig_LoadLibraryA, 0},
    {"KERNEL32.dll", "LoadLibraryW", 0, (void *)hook_LoadLibraryW, &g_orig_LoadLibraryW, 0},
    {"KERNEL32.dll", "GetProcAddress", 0, (void *)hook_GetProcAddress, &g_orig_GetProcAddress, 0},
    {"KERNEL32.dll", "CreateThread", 0, (void *)hook_CreateThread, &g_orig_CreateThread, 0},
};

static probe_iat_hook g_file_path_hooks[] = {
    {"KERNEL32.dll", "CreateFileA", 0, (void *)hook_CreateFileA, &g_orig_CreateFileA, 0},
    {"KERNEL32.dll", "CreateFileW", 0, (void *)hook_CreateFileW, &g_orig_CreateFileW, 0},
    {"KERNEL32.dll", "SetFilePointer", 0, (void *)hook_SetFilePointer, &g_orig_SetFilePointer, 0},
    {"KERNEL32.dll", "GetFileSize", 0, (void *)hook_GetFileSize, &g_orig_GetFileSize, 0},
    {"KERNEL32.dll", "CloseHandle", 0, (void *)hook_CloseHandle, &g_orig_CloseHandle, 0},
};

static probe_iat_hook g_file_read_hooks[] = {
    {"KERNEL32.dll", "ReadFile", 0, (void *)hook_ReadFile, &g_orig_ReadFile, 0},
};

static probe_iat_hook g_textdraw_render_iat_hooks[] = {
    {"d3dx9_25.dll", "D3DXCreateSprite", 0, (void *)hook_D3DXCreateSprite, &g_orig_D3DXCreateSprite, 0},
    {"d3dx9_25.dll", "D3DXCreateTexture", 0, (void *)hook_D3DXCreateTexture, &g_orig_D3DXCreateTexture, 0},
    {"d3dx9_25.dll", "D3DXCreateTextureFromFileA", 0, (void *)hook_D3DXCreateTextureFromFileA,
     &g_orig_D3DXCreateTextureFromFileA, 0},
    {"d3dx9_25.dll", "D3DXCreateTextureFromFileExA", 0, (void *)hook_D3DXCreateTextureFromFileExA,
     &g_orig_D3DXCreateTextureFromFileExA, 0},
    {"d3dx9_25.dll", "D3DXCreateTextureFromFileInMemory", 0, (void *)hook_D3DXCreateTextureFromFileInMemory,
     &g_orig_D3DXCreateTextureFromFileInMemory, 0},
    {"d3dx9_25.dll", "D3DXCreateTextureFromResourceExA", 0, (void *)hook_D3DXCreateTextureFromResourceExA,
     &g_orig_D3DXCreateTextureFromResourceExA, 0},
};

static probe_watchpoint g_watchpoints[] = {
    {"gta_entry_gate", 0x00C8D4C0u, 4, {0}, 0},
    {"start_game_flag", 0x00BA677Bu, 1, {0}, 0},
    {"game_started_flag", 0x00BA6831u, 1, {0}, 0},
    {"menu_flag", 0x00BA67A4u, 1, {0}, 0},
    {"menu2_flag", 0x00BA67A5u, 1, {0}, 0},
    {"menu3_flag", 0x00BA67A6u, 1, {0}, 0},
    {"hud_flag", 0x00BA6769u, 1, {0}, 0},
    {"time_pass_patch", 0x0052CF10u, 1, {0}, 0},
    {"hwnd_ptr", 0x00C97C1Cu, 4, {0}, 0},
    {"id3d9_ptr", 0x00C97C20u, 4, {0}, 0},
    {"id3d9device_ptr", 0x00C97C28u, 4, {0}, 0},
    {"script_process_gate", 0x00469EF5u, 2, {0}, 0},
    {"script_process_call", 0x0053BFC7u, 5, {0}, 0},
    {"graphics_loop_call", 0x0053EB12u, 5, {0}, 0},
    {"game_process_hook_site", 0x0058C246u, 8, {0}, 0},
    {"game_process_hook_storage", 0x0053BED1u, 4, {0}, 0},
    {"render2dstuff_storage", 0x0053E22Cu, 4, {0}, 0},
    {"ped_table_ptr", 0x00B74490u, 4, {0}, 0},
    {"vehicle_table_ptr", 0x00B74494u, 4, {0}, 0},
    {"current_player", 0x00B7CD74u, 1, {0}, 0},
    {"camera_mode", 0x00B6F1A8u, 1, {0}, 0},
    {"camera_mode2", 0x00B6F858u, 2, {0}, 0},
    {"bypass_vids_usa10", 0x00747483u, 6, {0}, 0},
    {"bypass_vids_eu10", 0x007474D3u, 6, {0}, 0},
};

static probe_inline_hook g_inline_hooks[] = {
    {"WSOCK32.dll", "WSAStartup", (void *)hook_WSAStartup, &g_orig_WSAStartup, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "WSACleanup", (void *)hook_WSACleanup, &g_orig_WSACleanup, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "socket", (void *)hook_socket, &g_orig_socket, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "bind", (void *)hook_bind, &g_orig_bind, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "connect", (void *)hook_connect, &g_orig_connect, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "closesocket", (void *)hook_closesocket, &g_orig_closesocket, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "gethostbyname", (void *)hook_gethostbyname, &g_orig_gethostbyname, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "gethostname", (void *)hook_gethostname, &g_orig_gethostname, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "inet_addr", (void *)hook_inet_addr, &g_orig_inet_addr, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "inet_ntoa", (void *)hook_inet_ntoa, &g_orig_inet_ntoa, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "send", (void *)hook_send, &g_orig_send, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "recv", (void *)hook_recv, &g_orig_recv, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "sendto", (void *)hook_sendto, &g_orig_sendto, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "recvfrom", (void *)hook_recvfrom, &g_orig_recvfrom, NULL, NULL, NULL, {0}, 0, 0},
    {"WSOCK32.dll", "select", (void *)hook_select, &g_orig_select, NULL, NULL, NULL, {0}, 0, 0},
};

static probe_inline_hook g_file_path_inline_hooks[] = {
    {"KERNEL32.dll", "CreateFileA", (void *)hook_CreateFileA, &g_orig_CreateFileA, NULL, NULL, NULL, {0}, 0, 0},
    {"KERNEL32.dll", "CreateFileW", (void *)hook_CreateFileW, &g_orig_CreateFileW, NULL, NULL, NULL, {0}, 0, 0},
    {"KERNEL32.dll", "SetFilePointer", (void *)hook_SetFilePointer, &g_orig_SetFilePointer, NULL, NULL, NULL, {0}, 0, 0},
    {"KERNEL32.dll", "GetFileSize", (void *)hook_GetFileSize, &g_orig_GetFileSize, NULL, NULL, NULL, {0}, 0, 0},
    {"KERNEL32.dll", "CloseHandle", (void *)hook_CloseHandle, &g_orig_CloseHandle, NULL, NULL, NULL, {0}, 0, 0},
};

static probe_inline_hook g_file_read_inline_hooks[] = {
    {"KERNEL32.dll", "ReadFile", (void *)hook_ReadFile, &g_orig_ReadFile, NULL, NULL, NULL, {0}, 0, 0},
};

static probe_code_hook g_samp_code_hooks[] = {
    {"samp.SocketLayer.SendTo", 0x0004ffc0u, (void *)hook_samp_socketlayer_sendto, &g_orig_samp_socketlayer_sendto,
     {0x83, 0xec, 0x10, 0x57}, 4, NULL, NULL, {0}, 0, 0},
    {"samp.ProcessNetworkPacket", 0x0003b950u, (void *)hook_samp_process_network_packet, &g_orig_samp_process_network_packet,
     {0x6a, 0xff, 0x68}, 3, NULL, NULL, {0}, 0, 0},
};

static probe_code_hook g_samp_font5_code_hooks[] = {
    /* STATIC_037:
     * Original samp.dll SHA256=b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
     * RVA 0xb34a0 validates style 5 and creates/caches the preview entity.
     * RVA 0xb3480 selects cached-preview drawing vs. the normal CFont path.
     * Both hooks additionally require the PE identity proxy and exact prologue bytes. */
    {"samp.CTextDraw.Font5Prepare", PROBE_SAMP_R5_FONT5_PREPARE_RVA, (void *)hook_samp_font5_prepare,
     &g_orig_samp_font5_prepare, {0x56, 0x8b, 0xf1, 0x83, 0xbe, 0x87, 0x09, 0x00, 0x00, 0x05}, 10,
     NULL, NULL, {0}, 0, 0},
    {"samp.CTextDraw.DrawDispatch", PROBE_SAMP_R5_FONT5_DRAW_DISPATCH_RVA,
     (void *)hook_samp_font5_draw_dispatch, &g_orig_samp_font5_draw_dispatch,
     {0x83, 0xb9, 0xa3, 0x09, 0x00, 0x00, 0xff}, 7, NULL, NULL, {0}, 0, 0},
};

static probe_code_hook g_gta_asset_code_hooks[] = {
    {"gta.CModelInfo.AddAtomicModel", 0x004c6620u, (void *)hook_gta_add_atomic_model, &g_orig_gta_add_atomic_model,
     {0xa1, 0x50, 0xe9, 0xaa, 0x00}, 5, NULL, NULL, {0}, 0, 0},
    {"gta.CModelInfo.AddTimeModel", 0x004c66b0u, (void *)hook_gta_add_time_model, &g_orig_gta_add_time_model,
     {0xa1, 0x60, 0xc9, 0xb1, 0x00}, 5, NULL, NULL, {0}, 0, 0},
    {"gta.CModelInfo.AddClumpModel", 0x004c6740u, (void *)hook_gta_add_clump_model, &g_orig_gta_add_clump_model,
     {0xa1, 0x58, 0xe9, 0xb1, 0x00}, 5, NULL, NULL, {0}, 0, 0},
    {"gta.CStreaming.AddImageToList", 0x00407610u, (void *)hook_gta_add_image_to_list, &g_orig_gta_add_image_to_list,
     {0}, 0, NULL, NULL, {0}, 0, 0},
    {"gta.CStreaming.LoadCdDirectory", 0x005b6170u, (void *)hook_gta_load_cd_directory, &g_orig_gta_load_cd_directory,
     {0x83, 0xec, 0x3c}, 3, NULL, NULL, {0}, 0, 0},
    {"gta.CColStore.AddColSlot", 0x00411140u, (void *)hook_gta_col_add_slot, &g_orig_gta_col_add_slot,
     {0}, 0, NULL, NULL, {0}, 0, 0},
    {"gta.CColStore.LoadCol", 0x004106d0u, (void *)hook_gta_col_load_buffer, &g_orig_gta_col_load_buffer,
     {0}, 0, NULL, NULL, {0}, 0, 0},
    {"gta.CPhysical.Add", 0x00544a30u, (void *)hook_gta_physical_add, &g_orig_gta_physical_add,
     {0x64, 0xa1, 0x00, 0x00, 0x00, 0x00}, 6, NULL, NULL, {0}, 0, 0},
};

static probe_code_hook g_gta_textdraw_code_hooks[] = {
    /* GTA_REVERSED_REF + TODO_VERIFY:
     * Trace-only CFont hooks for GTA SA 1.0 US in the local libsamp prefix.
     * gta_sa.exe SHA256=a559aa772fd136379155efa71f00c47aad34bbfeae6196b0fe1047d0645cbd26.
     * These hooks log only SAMP caller RVAs by default and stay opt-in. */
    {"gta.CFont.SetScale", 0x00719380u, (void *)hook_gta_font_set_scale, &g_orig_gta_font_set_scale,
     {0x8b, 0x44, 0x24, 0x04, 0x8b, 0x4c, 0x24, 0x08}, 8, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.SetColor", 0x00719430u, (void *)hook_gta_font_set_color, &g_orig_gta_font_set_color,
     {0xd9, 0x05, 0x80, 0x1a, 0xc7, 0x00}, 6, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.SetFontStyle", 0x00719490u, (void *)hook_gta_font_set_style, &g_orig_gta_font_set_style,
     {0x8a, 0x4c, 0x24, 0x04, 0x0f, 0xb6, 0xc1}, 7, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.SetLineWidth", 0x007194d0u, (void *)hook_gta_font_set_line_width,
     &g_orig_gta_font_set_line_width, {0x8b, 0x44, 0x24, 0x04, 0xa3, 0x88, 0x1a, 0xc7}, 8, NULL,
     NULL, {0}, 0, 0},
    {"gta.CFont.SetLineHeight", 0x007194e0u, (void *)hook_gta_font_set_line_height,
     &g_orig_gta_font_set_line_height, {0x8b, 0x44, 0x24, 0x04, 0xa3, 0x8c, 0x1a, 0xc7}, 8, NULL,
     NULL, {0}, 0, 0},
    {"gta.CFont.SetDropColor", 0x00719510u, (void *)hook_gta_font_set_drop_color,
     &g_orig_gta_font_set_drop_color, {0x8b, 0x44, 0x24, 0x04, 0xd9, 0x05, 0x80, 0x1a}, 8, NULL,
     NULL, {0}, 0, 0},
    {"gta.CFont.SetShadow", 0x00719570u, (void *)hook_gta_font_set_shadow, &g_orig_gta_font_set_shadow,
     {0x32, 0xc0, 0xa2, 0x9b, 0x1a, 0xc7, 0x00}, 7, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.SetOutline", 0x00719590u, (void *)hook_gta_font_set_outline, &g_orig_gta_font_set_outline,
     {0x8a, 0x44, 0x24, 0x04, 0xc6, 0x05, 0x96, 0x1a}, 8, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.SetProportional", 0x007195b0u, (void *)hook_gta_font_set_proportional,
     &g_orig_gta_font_set_proportional, {0x8a, 0x44, 0x24, 0x04, 0xa2, 0x7d, 0x1a, 0xc7}, 8, NULL,
     NULL, {0}, 0, 0},
    {"gta.CFont.UseBox", 0x007195c0u, (void *)hook_gta_font_use_box, &g_orig_gta_font_use_box,
     {0x8a, 0x44, 0x24, 0x04, 0x8a, 0x4c, 0x24, 0x08}, 8, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.UseBoxColor", 0x007195e0u, (void *)hook_gta_font_use_box_color,
     &g_orig_gta_font_use_box_color, {0x8b, 0x44, 0x24, 0x04, 0xa2, 0x84, 0x1a, 0xc7}, 8, NULL,
     NULL, {0}, 0, 0},
    {"gta.CFont.UnknownState", 0x00719600u, (void *)hook_gta_font_unk12, &g_orig_gta_font_unk12,
     {0x8a, 0x44, 0x24, 0x04, 0xa2, 0x78, 0x1a, 0xc7}, 8, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.SetJustify", 0x00719610u, (void *)hook_gta_font_set_justify, &g_orig_gta_font_set_justify,
     {0x0f, 0xb6, 0x44, 0x24, 0x04, 0x33, 0xc9}, 7, NULL, NULL, {0}, 0, 0},
    {"gta.CFont.PrintString", 0x0071a700u, (void *)hook_gta_font_print_string,
     &g_orig_gta_font_print_string, {0x83, 0xec, 0x10, 0x53, 0x8b, 0x5c, 0x24, 0x20}, 8, NULL,
     NULL, {0}, 0, 0},
};

static void probe_log(const char *fmt, ...) {
  char payload[2048];
  char line[2300];
  DWORD now_ms;
  FILE *fp;
  va_list args;

  if (InterlockedCompareExchange(&g_log_ready, 0, 0) == 0 || fmt == NULL) {
    return;
  }

  va_start(args, fmt);
  vsnprintf(payload, sizeof(payload), fmt, args);
  va_end(args);

  now_ms = GetTickCount();
  snprintf(line, sizeof(line), "[%10lu] %s\r\n", (unsigned long)now_ms, payload);

  EnterCriticalSection(&g_log_lock);
  fp = fopen(g_log_path, "ab");
  if (fp != NULL) {
    fputs(line, fp);
    fclose(fp);
  }
  LeaveCriticalSection(&g_log_lock);

  OutputDebugStringA("[samp_probe] ");
  OutputDebugStringA(payload);
  OutputDebugStringA("\n");
}

static int should_log_call(LONG count) {
  return count <= 64 || (count & 0xff) == 0;
}

static LONG next_call_count(LONG *counter) {
  return InterlockedIncrement(counter);
}

static int ascii_equal_ci(char a, char b) {
  if (a >= 'A' && a <= 'Z') {
    a = (char)(a - 'A' + 'a');
  }
  if (b >= 'A' && b <= 'Z') {
    b = (char)(b - 'A' + 'a');
  }
  return a == b;
}

static int ascii_contains_ci(const char *text, const char *needle) {
  size_t i;
  size_t j;
  size_t text_len;
  size_t needle_len;

  if (text == NULL || needle == NULL || *needle == '\0') {
    return 0;
  }

  text_len = strlen(text);
  needle_len = strlen(needle);
  if (needle_len > text_len) {
    return 0;
  }

  for (i = 0; i + needle_len <= text_len; ++i) {
    for (j = 0; j < needle_len; ++j) {
      if (!ascii_equal_ci(text[i + j], needle[j])) {
        break;
      }
    }
    if (j == needle_len) {
      return 1;
    }
  }

  return 0;
}

static int asset_path_interesting(const char *path) {
  if (path == NULL || *path == '\0') {
    return 0;
  }

  return ascii_contains_ci(path, "\\SAMP\\") || ascii_contains_ci(path, "/SAMP/") ||
         ascii_contains_ci(path, "SAMP.img") || ascii_contains_ci(path, "SAMP.ide") ||
         ascii_contains_ci(path, "SAMPCOL.img") || ascii_contains_ci(path, "CUSTOM.ide") ||
         ascii_contains_ci(path, "custom.img") || ascii_contains_ci(path, "samaps.txd") ||
         ascii_contains_ci(path, "blanktex.txd") || ascii_contains_ci(path, "SAMP.ipl");
}

static int file_hook_enter(void) {
  int nested = g_file_hook_depth;
  ++g_file_hook_depth;
  return nested == 0;
}

static void file_hook_leave(void) {
  --g_file_hook_depth;
}

static void remember_asset_handle(HANDLE handle, const char *path) {
  size_t i;
  size_t empty = PROBE_ASSET_HANDLE_SLOTS;

  if (handle == NULL || handle == INVALID_HANDLE_VALUE || path == NULL || *path == '\0') {
    return;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_ASSET_HANDLE_SLOTS; ++i) {
    if (g_asset_handles[i].handle == handle) {
      snprintf(g_asset_handles[i].path, sizeof(g_asset_handles[i].path), "%s", path);
      InterlockedExchange(&g_asset_handles[i].read_count, 0);
      LeaveCriticalSection(&g_log_lock);
      return;
    }
    if (empty == PROBE_ASSET_HANDLE_SLOTS && g_asset_handles[i].handle == NULL) {
      empty = i;
    }
  }

  if (empty != PROBE_ASSET_HANDLE_SLOTS) {
    g_asset_handles[empty].handle = handle;
    snprintf(g_asset_handles[empty].path, sizeof(g_asset_handles[empty].path), "%s", path);
    InterlockedExchange(&g_asset_handles[empty].read_count, 0);
  }
  LeaveCriticalSection(&g_log_lock);
}

static int find_asset_handle(HANDLE handle, char *path_out, size_t path_out_size, LONG *read_count_out) {
  size_t i;
  int found = 0;

  if (path_out != NULL && path_out_size > 0) {
    path_out[0] = '\0';
  }
  if (read_count_out != NULL) {
    *read_count_out = 0;
  }
  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return 0;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_ASSET_HANDLE_SLOTS; ++i) {
    if (g_asset_handles[i].handle == handle) {
      found = 1;
      if (path_out != NULL && path_out_size > 0) {
        snprintf(path_out, path_out_size, "%s", g_asset_handles[i].path);
      }
      if (read_count_out != NULL) {
        *read_count_out = InterlockedIncrement(&g_asset_handles[i].read_count);
      }
      break;
    }
  }
  LeaveCriticalSection(&g_log_lock);

  return found;
}

static void forget_asset_handle(HANDLE handle) {
  size_t i;

  if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
    return;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_ASSET_HANDLE_SLOTS; ++i) {
    if (g_asset_handles[i].handle == handle) {
      g_asset_handles[i].handle = NULL;
      g_asset_handles[i].path[0] = '\0';
      InterlockedExchange(&g_asset_handles[i].read_count, 0);
      break;
    }
  }
  LeaveCriticalSection(&g_log_lock);
}

static int env_flag_enabled(const char *name) {
  char value[16];
  DWORD len;

  if (name == NULL) {
    return 0;
  }

  len = GetEnvironmentVariableA(name, value, sizeof(value));
  if (len == 0 || len >= sizeof(value)) {
    return 0;
  }

  return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
}

static int asset_path_hooks_enabled(void) {
  return env_or_flag_enabled("SAMP_PROBE_ASSET_PATHS", PROBE_ASSET_PATHS_FLAG) || asset_read_hooks_enabled() ||
         custom_object_heavy_enabled();
}

static int asset_read_hooks_enabled(void) {
  /* PROBE_TRACE + TODO_VERIFY: ReadFile remains a separate opt-in because
   * original 0.3.7 uses large overlapped reads for SAMP asset archives. */
  return env_or_flag_enabled("SAMP_PROBE_FILE_HOOKS", PROBE_FILE_HOOKS_FLAG);
}

static int samp_code_hooks_enabled(void) {
  /* PROBE_TRACE: the older internal hook RVAs are stale for the tested R5 DLL.
   * Keep patching opt-in until we have byte-validated replacement RVAs. */
  return env_or_flag_enabled("SAMP_PROBE_ENABLE_SAMP_CODE_HOOKS", PROBE_SAMP_CODE_HOOKS_FLAG);
}

static int gta_asset_hooks_enabled(void) {
  /* PROBE_TRACE + GTA_REVERSED_REF + TODO_VERIFY:
   * These hooks patch GTA-SA engine functions, not samp.dll. They are only for
   * short original-vs-rebuild custom-object traces because they sit on hot asset
   * loading paths and must not become part of normal gameplay runs. */
  return env_or_flag_enabled("SAMP_PROBE_GTA_ASSET_HOOKS", PROBE_GTA_ASSET_HOOKS_FLAG) ||
         custom_object_heavy_enabled();
}

static int object_info_enabled(void) {
  /* OBSERVED_037 target:
   * Focused custom-object runs need CModelInfo/CColModel field dumps after GTA's
   * native registration path. Keep this separate from the hot GTA asset hook flag
   * because the raw scans are intentionally verbose. */
  return env_or_flag_enabled("SAMP_PROBE_OBJECT_INFO", PROBE_OBJECT_INFO_FLAG) || custom_object_heavy_enabled();
}

static int custom_object_heavy_enabled(void) {
  /* PROBE_TRACE + TODO_VERIFY:
   * Heavy custom-object mode keeps normal runs quiet, but a focused original
   * DLL run can capture stack-attributed model registration, store counters,
   * pointer slots, and collision binding in one pass. */
  return env_or_flag_enabled("SAMP_PROBE_CUSTOM_OBJECT_HEAVY", PROBE_CUSTOM_OBJECT_HEAVY_FLAG);
}

static int textdraw_hooks_enabled(void) {
  /* GTA_REVERSED_REF + TODO_VERIFY:
   * Optional CFont hooks are for focused 0.3.7 textdraw traces only. They are
   * noisy on render paths, so normal probe runs leave them disabled. */
  return env_or_flag_enabled("SAMP_PROBE_TEXTDRAW_HOOKS", PROBE_TEXTDRAW_HOOKS_FLAG) || textdraw_render_enabled();
}

static int textdraw_verbose_enabled(void) {
  return env_or_flag_enabled("SAMP_PROBE_TEXTDRAW_VERBOSE", PROBE_TEXTDRAW_VERBOSE_FLAG);
}

static int textdraw_render_enabled(void) {
  /* PROBE_TRACE + TODO_VERIFY:
   * Focused Font 4 sprite / Font 5 preview traces need D3DX/IDirect3DDevice9
   * hooks in addition to CFont. Keep this deeper render probe separately opt-in
   * because it patches COM vtables and can be noisy during short render runs. */
  return env_or_flag_enabled("SAMP_PROBE_TEXTDRAW_RENDER", PROBE_TEXTDRAW_RENDER_FLAG) || font5_hooks_enabled();
}

static int font5_hooks_enabled(void) {
  /* STATIC_037 + TODO_VERIFY:
   * These internal samp.dll hooks are restricted to an exact R5 PE identity
   * proxy plus validated prologue bytes. The dedicated switch also enables the
   * D3D render tracer so one short run captures dispatch and RTT activity. */
  return env_or_flag_enabled("SAMP_PROBE_FONT5_HOOKS", PROBE_FONT5_HOOKS_FLAG);
}

PROBE_ALWAYS_INLINE void *probe_return_address(void) {
  return __builtin_return_address(0);
}

static int address_in_samp(void *address) {
  uintptr_t value = (uintptr_t)address;
  if (g_samp_base == 0 || g_samp_size == 0 || value == 0) {
    return 0;
  }
  return value >= g_samp_base && value < (g_samp_base + (uintptr_t)g_samp_size);
}

static int should_log_api_from_caller(void *caller) {
  return address_in_samp(caller) || env_flag_enabled("SAMP_PROBE_LOG_ALL_API");
}

static unsigned long samp_rva_from_address(void *address) {
  if (!address_in_samp(address)) {
    return 0;
  }
  return (unsigned long)((uintptr_t)address - g_samp_base);
}

static const char *samp_observed_socket_callsite(void *address) {
  unsigned long rva = samp_rva_from_address(address);

  /* OBSERVED_037 + PROBE_TRACE:
   * R5 safe golden run 2026-06-05, original SHA256
   * b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2.
   * These are socket API return addresses, not verified function entries. */
  switch (rva) {
    case 0x00053b19u:
      return "OBSERVED_037_SOCKET_SENDTO_CALLSITE";
    case 0x00053a57u:
      return "OBSERVED_037_SOCKET_RECVFROM_CALLSITE";
    default:
      return "";
  }
}

static void path_dirname(char *path) {
  size_t len;

  if (path == NULL) {
    return;
  }

  len = strlen(path);
  while (len > 0) {
    char ch = path[len - 1];
    if (ch == '\\' || ch == '/') {
      path[len - 1] = '\0';
      return;
    }
    path[len - 1] = '\0';
    --len;
  }
}

static void join_path(char *out, size_t out_size, const char *dir, const char *file) {
  size_t len;

  if (out == NULL || out_size == 0) {
    return;
  }

  if (dir == NULL || *dir == '\0') {
    snprintf(out, out_size, "%s", file);
    return;
  }

  len = strlen(dir);
  snprintf(out, out_size, "%s%s%s", dir, (len > 0 && dir[len - 1] == '\\') ? "" : "\\", file);
}

static int file_exists(const char *path) {
  DWORD attr;

  if (path == NULL || *path == '\0') {
    return 0;
  }

  attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int get_probe_module_dir(char *out, size_t out_size) {
  char module_path[MAX_PATH];

  if (out == NULL || out_size == 0) {
    return 0;
  }

  out[0] = '\0';
  module_path[0] = '\0';
  if (GetModuleFileNameA(g_self_module, module_path, sizeof(module_path)) == 0) {
    return 0;
  }

  snprintf(out, out_size, "%s", module_path);
  path_dirname(out);
  return out[0] != '\0';
}

static int probe_flag_file_enabled(const char *flag_name) {
  char module_dir[MAX_PATH];
  char flag_path[MAX_PATH];

  if (flag_name == NULL || *flag_name == '\0') {
    return 0;
  }

  module_dir[0] = '\0';
  flag_path[0] = '\0';
  if (!get_probe_module_dir(module_dir, sizeof(module_dir))) {
    return 0;
  }

  join_path(flag_path, sizeof(flag_path), module_dir, flag_name);
  return file_exists(flag_path);
}

static int env_or_flag_enabled(const char *env_name, const char *flag_name) {
  return env_flag_enabled(env_name) || probe_flag_file_enabled(flag_name);
}

static void init_log_path(void) {
  char module_path[MAX_PATH];
  char module_dir[MAX_PATH];

  module_path[0] = '\0';
  module_dir[0] = '\0';
  g_log_path[0] = '\0';

  if (GetModuleFileNameA(g_self_module, module_path, sizeof(module_path)) > 0) {
    snprintf(module_dir, sizeof(module_dir), "%s", module_path);
    path_dirname(module_dir);
    join_path(g_log_path, sizeof(g_log_path), module_dir, PROBE_LOG_NAME);
  } else {
    snprintf(g_log_path, sizeof(g_log_path), "%s", PROBE_LOG_NAME);
  }

  InitializeCriticalSection(&g_log_lock);
  InterlockedExchange(&g_log_ready, 1);
}

static int hooks_disabled_by_flag(void) {
  if (env_flag_enabled("SAMP_PROBE_NO_HOOKS")) {
    return 1;
  }

  return probe_flag_file_enabled(PROBE_NO_HOOKS_FLAG);
}

static void format_sockaddr(const struct sockaddr *addr, int addr_len, char *out, size_t out_size) {
  const struct sockaddr_in *sin;
  const unsigned char *ip;
  const unsigned char *port_bytes;
  unsigned int port;

  if (out == NULL || out_size == 0) {
    return;
  }

  if (addr == NULL) {
    snprintf(out, out_size, "null");
    return;
  }

  if (addr_len < (int)sizeof(struct sockaddr_in) || addr->sa_family != AF_INET) {
    snprintf(out, out_size, "family=%d len=%d", (int)addr->sa_family, addr_len);
    return;
  }

  sin = (const struct sockaddr_in *)addr;
  ip = (const unsigned char *)&sin->sin_addr.s_addr;
  port_bytes = (const unsigned char *)&sin->sin_port;
  port = ((unsigned int)port_bytes[0] << 8u) | (unsigned int)port_bytes[1];
  snprintf(out, out_size, "%u.%u.%u.%u:%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3], port);
}

static void format_binary_address(unsigned long binary_address, unsigned short port, char *out, size_t out_size) {
  const unsigned char *ip = (const unsigned char *)&binary_address;

  if (out == NULL || out_size == 0) {
    return;
  }

  snprintf(out, out_size, "%u.%u.%u.%u:%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
           (unsigned)port);
}

static const char *samp_packet_name(unsigned char packet_id) {
  switch (packet_id) {
    case 6:
      return "ID_INTERNAL_PING";
    case 7:
      return "ID_PING";
    case 8:
      return "ID_PING_OPEN_CONNECTIONS";
    case 9:
      return "ID_CONNECTED_PONG";
    case 10:
      return "ID_REQUEST_STATIC_DATA";
    case 11:
      return "ID_CONNECTION_REQUEST";
    case 12:
      return "ID_AUTH_KEY";
    case 14:
      return "ID_BROADCAST_PINGS";
    case 15:
      return "ID_SECURED_CONNECTION_RESPONSE";
    case 16:
      return "ID_SECURED_CONNECTION_CONFIRMATION";
    case 17:
      return "ID_RPC_MAPPING";
    case 19:
      return "ID_SET_RANDOM_NUMBER_SEED";
    case 20:
      return "ID_RPC";
    case 21:
      return "ID_RPC_REPLY";
    case 23:
      return "ID_DETECT_LOST_CONNECTIONS";
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
    case 39:
      return "ID_PONG";
    case 40:
      return "ID_TIMESTAMP";
    default:
      return "";
  }
}

static const char *known_ordinal_name(const char *dll_name, WORD ordinal) {
  if (dll_name == NULL) {
    return NULL;
  }

  if (lstrcmpiA(dll_name, "WSOCK32.dll") == 0) {
    switch (ordinal) {
      case 2:
        return "bind";
      case 3:
        return "closesocket";
      case 4:
        return "connect";
      case 10:
        return "inet_addr";
      case 11:
        return "inet_ntoa";
      case 16:
        return "recv";
      case 17:
        return "recvfrom";
      case 18:
        return "select";
      case 19:
        return "send";
      case 20:
        return "sendto";
      case 23:
        return "socket";
      case 52:
        return "gethostbyname";
      case 57:
        return "gethostname";
      case 115:
        return "WSAStartup";
      case 116:
        return "WSACleanup";
      default:
        return NULL;
    }
  }

  return NULL;
}

static int last_wsa_error(void) {
  if (g_WSAGetLastError_ptr != NULL) {
    return g_WSAGetLastError_ptr();
  }
  return (int)GetLastError();
}

static int memory_is_readable(uintptr_t address, size_t size) {
  MEMORY_BASIC_INFORMATION mbi;
  DWORD protect;

  if (address == 0 || size == 0) {
    return 0;
  }

  if (VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi)) != sizeof(mbi)) {
    return 0;
  }

  if (mbi.State != MEM_COMMIT) {
    return 0;
  }

  if ((uintptr_t)mbi.BaseAddress + mbi.RegionSize < address + size) {
    return 0;
  }

  protect = mbi.Protect & 0xffu;
  if ((mbi.Protect & PAGE_GUARD) != 0 || protect == PAGE_NOACCESS) {
    return 0;
  }

  return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
         protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

static void bytes_to_hex(const BYTE *bytes, size_t size, char *out, size_t out_size) {
  size_t i;
  size_t used = 0;

  if (out == NULL || out_size == 0) {
    return;
  }

  out[0] = '\0';
  if (bytes == NULL) {
    return;
  }

  for (i = 0; i < size && used + 4 < out_size; ++i) {
    int written = snprintf(out + used, out_size - used, "%s%02x", (i == 0) ? "" : " ", (unsigned)bytes[i]);
    if (written < 0) {
      return;
    }
    used += (size_t)written;
  }
}

static void payload_to_hex(const void *data, int len, char *out, size_t out_size) {
  size_t preview_len;

  if (out == NULL || out_size == 0) {
    return;
  }

  out[0] = '\0';
  if (data == NULL || len <= 0) {
    return;
  }

  preview_len = (size_t)len;
  if (preview_len > PROBE_PAYLOAD_PREVIEW_BYTES) {
    preview_len = PROBE_PAYLOAD_PREVIEW_BYTES;
  }

  if (!memory_is_readable((uintptr_t)data, preview_len)) {
    snprintf(out, out_size, "unreadable");
    return;
  }

  bytes_to_hex((const BYTE *)data, preview_len, out, out_size);
}

static BYTE read_u8_or(uintptr_t address, BYTE fallback) {
  BYTE value;

  if (!memory_is_readable(address, sizeof(value))) {
    return fallback;
  }

  memcpy(&value, (const void *)address, sizeof(value));
  return value;
}

static WORD read_u16_or(uintptr_t address, WORD fallback) {
  WORD value;

  if (!memory_is_readable(address, sizeof(value))) {
    return fallback;
  }

  memcpy(&value, (const void *)address, sizeof(value));
  return value;
}

static DWORD read_u32_or(uintptr_t address, DWORD fallback) {
  DWORD value;

  if (!memory_is_readable(address, sizeof(value))) {
    return fallback;
  }

  memcpy(&value, (const void *)address, sizeof(value));
  return value;
}

static uintptr_t probe_current_stack_pointer(void) {
  uintptr_t sp = 0;

#if defined(__GNUC__) && defined(__i386__)
  __asm__ __volatile__("movl %%esp, %0" : "=r"(sp));
#else
  sp = (uintptr_t)&sp;
#endif
  return sp;
}

static int append_format(char *out, size_t out_size, size_t *used, const char *fmt, ...) {
  va_list ap;
  int written;

  if (out == NULL || out_size == 0 || used == NULL || *used >= out_size) {
    return 0;
  }

  va_start(ap, fmt);
  written = vsnprintf(out + *used, out_size - *used, fmt, ap);
  va_end(ap);
  if (written < 0) {
    return 0;
  }
  if ((size_t)written >= out_size - *used) {
    *used = out_size - 1;
    out[*used] = '\0';
    return 0;
  }
  *used += (size_t)written;
  return 1;
}

static void format_samp_stack_rvas(char *out, size_t out_size, unsigned max_words, unsigned max_hits) {
  uintptr_t sp = probe_current_stack_pointer();
  size_t used = 0;
  unsigned i;
  unsigned hits = 0;

  if (out == NULL || out_size == 0) {
    return;
  }
  out[0] = '\0';
  append_format(out, out_size, &used, "sp=0x%08lx rvas=", (unsigned long)sp);

  for (i = 0; i < max_words && hits < max_hits; ++i) {
    uintptr_t slot = sp + ((uintptr_t)i * sizeof(DWORD));
    DWORD value;
    unsigned long rva;

    if (!memory_is_readable(slot, sizeof(value))) {
      break;
    }

    value = read_u32_or(slot, 0u);
    if (!address_in_samp((void *)(uintptr_t)value)) {
      continue;
    }

    rva = samp_rva_from_address((void *)(uintptr_t)value);
    append_format(out, out_size, &used, "%s[%u]=0x%08lx", hits == 0 ? "" : ",", i, rva);
    ++hits;
  }

  if (hits == 0) {
    append_format(out, out_size, &used, "none");
  }
}

static void safe_cstr_or(const char *value, char *out, size_t out_size, const char *fallback) {
  size_t i;

  if (out == NULL || out_size == 0) {
    return;
  }

  out[0] = '\0';
  if (value == NULL || !memory_is_readable((uintptr_t)value, 1)) {
    snprintf(out, out_size, "%s", fallback != NULL ? fallback : "");
    return;
  }

  for (i = 0; i + 1 < out_size; ++i) {
    char ch;
    if (!memory_is_readable((uintptr_t)(value + i), 1)) {
      break;
    }
    ch = value[i];
    if (ch == '\0') {
      break;
    }
    if ((unsigned char)ch < 0x20u || (unsigned char)ch >= 0x7fu) {
      ch = '?';
    }
    out[i] = ch;
  }
  out[i] = '\0';
}

static void safe_resource_name_or(const char *value, char *out, size_t out_size, const char *fallback) {
  if (out == NULL || out_size == 0) {
    return;
  }
  if (((uintptr_t)value >> 16u) == 0) {
    snprintf(out, out_size, "#%lu", (unsigned long)(uintptr_t)value);
    return;
  }
  safe_cstr_or(value, out, out_size, fallback);
}

static int source_text_interesting(const char *source) {
  if (source == NULL || *source == '\0') {
    return 0;
  }

  return ascii_contains_ci(source, "SAMP") || ascii_contains_ci(source, "LD_") ||
         ascii_contains_ci(source, ".txd") || ascii_contains_ci(source, ".png") ||
         ascii_contains_ci(source, "memory:") || ascii_contains_ci(source, "resource:");
}

static void remember_texture_source(void *texture, const char *source) {
  size_t i;
  size_t empty = PROBE_TEXTURE_TRACK_SLOTS;

  if (texture == NULL || source == NULL || *source == '\0') {
    return;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_TEXTURE_TRACK_SLOTS; ++i) {
    if (g_texture_records[i].texture == texture) {
      snprintf(g_texture_records[i].source, sizeof(g_texture_records[i].source), "%s", source);
      LeaveCriticalSection(&g_log_lock);
      return;
    }
    if (empty == PROBE_TEXTURE_TRACK_SLOTS && g_texture_records[i].texture == NULL) {
      empty = i;
    }
  }

  if (empty != PROBE_TEXTURE_TRACK_SLOTS) {
    g_texture_records[empty].texture = texture;
    snprintf(g_texture_records[empty].source, sizeof(g_texture_records[empty].source), "%s", source);
    InterlockedExchange(&g_texture_records[empty].draw_count, 0);
  }
  LeaveCriticalSection(&g_log_lock);
}

static void remember_texture_details(void *texture, const char *source, UINT width, UINT height, DWORD usage) {
  size_t i;

  remember_texture_source(texture, source);
  if (texture == NULL) {
    return;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_TEXTURE_TRACK_SLOTS; ++i) {
    if (g_texture_records[i].texture == texture) {
      g_texture_records[i].width = width;
      g_texture_records[i].height = height;
      g_texture_records[i].usage = usage;
      break;
    }
  }
  LeaveCriticalSection(&g_log_lock);
}

static void remember_texture_surface(void *texture, UINT level, void *surface) {
  size_t i;

  if (texture == NULL || level != 0 || surface == NULL) {
    return;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_TEXTURE_TRACK_SLOTS; ++i) {
    if (g_texture_records[i].texture == texture) {
      g_texture_records[i].level0_surface = surface;
      break;
    }
  }
  LeaveCriticalSection(&g_log_lock);
}

static int texture_for_surface(void *surface, void **texture_out, char *source, size_t source_size) {
  size_t i;
  int found = 0;

  if (texture_out != NULL) {
    *texture_out = NULL;
  }
  if (source != NULL && source_size > 0) {
    source[0] = '\0';
  }
  if (surface == NULL) {
    return 0;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_TEXTURE_TRACK_SLOTS; ++i) {
    if (g_texture_records[i].level0_surface == surface) {
      found = 1;
      if (texture_out != NULL) {
        *texture_out = g_texture_records[i].texture;
      }
      if (source != NULL && source_size > 0) {
        snprintf(source, source_size, "%s", g_texture_records[i].source);
      }
      break;
    }
  }
  LeaveCriticalSection(&g_log_lock);
  return found;
}

static int texture_source_for(void *texture, char *out, size_t out_size, LONG *draw_count_out) {
  size_t i;
  int found = 0;

  if (out != NULL && out_size > 0) {
    out[0] = '\0';
  }
  if (draw_count_out != NULL) {
    *draw_count_out = 0;
  }
  if (texture == NULL) {
    return 0;
  }

  EnterCriticalSection(&g_log_lock);
  for (i = 0; i < PROBE_TEXTURE_TRACK_SLOTS; ++i) {
    if (g_texture_records[i].texture == texture) {
      found = 1;
      if (out != NULL && out_size > 0) {
        snprintf(out, out_size, "%s", g_texture_records[i].source);
      }
      if (draw_count_out != NULL) {
        *draw_count_out = InterlockedIncrement(&g_texture_records[i].draw_count);
      }
      break;
    }
  }
  LeaveCriticalSection(&g_log_lock);
  return found;
}

static void textdraw_render_mark_hint(const char *reason) {
  DWORD until;

  if (!textdraw_render_enabled()) {
    return;
  }

  until = GetTickCount() + PROBE_TEXTDRAW_RENDER_HINT_MS;
  InterlockedExchange((volatile LONG *)&g_textdraw_render_hint_until_ms, (LONG)until);
  if (textdraw_verbose_enabled()) {
    probe_log("textdraw_render: hint reason=%s until_ms=%lu evidence=PROBE_TRACE,TODO_VERIFY",
              reason != NULL ? reason : "unknown", (unsigned long)until);
  }
}

static int textdraw_render_hint_active(void) {
  DWORD until = (DWORD)InterlockedCompareExchange((volatile LONG *)&g_textdraw_render_hint_until_ms, 0, 0);
  return until != 0 && (LONG)(until - GetTickCount()) > 0;
}

static int should_log_textdraw_render_call(void *caller, LONG count, const char *texture_source) {
  int broad_samp_render_trace;

  if (!textdraw_render_enabled()) {
    return 0;
  }
  if (env_flag_enabled("SAMP_PROBE_TEXTDRAW_RENDER_LOG_ALL")) {
    return 1;
  }
  /* The dedicated Font 5 switch is intentionally narrower than the generic
   * render switch: exact dispatch depth supplies the correlation and avoids a
   * whole-frame SA-MP D3D trace. */
  broad_samp_render_trace = env_or_flag_enabled("SAMP_PROBE_TEXTDRAW_RENDER", PROBE_TEXTDRAW_RENDER_FLAG);
  if (g_font5_trace_depth > 0 ||
      (broad_samp_render_trace && (address_in_samp(caller) || textdraw_render_hint_active() ||
                                  source_text_interesting(texture_source)))) {
    return 1;
  }
  return textdraw_verbose_enabled() && should_log_call(count);
}

static void format_font5_stack(char *out, size_t out_size) {
  format_samp_stack_rvas(out, out_size, 192, 10);
}

static int should_log_font5_rtt_call(LONG count) {
  int broad_samp_render_trace;

  if (!textdraw_render_enabled()) {
    return 0;
  }
  broad_samp_render_trace = env_or_flag_enabled("SAMP_PROBE_TEXTDRAW_RENDER", PROBE_TEXTDRAW_RENDER_FLAG);
  if (g_font5_trace_depth > 0 || env_flag_enabled("SAMP_PROBE_TEXTDRAW_RENDER_LOG_ALL") ||
      (broad_samp_render_trace && textdraw_render_hint_active())) {
    return 1;
  }
  return textdraw_verbose_enabled() && should_log_call(count);
}

static int patch_com_vtable_slot(void *object, unsigned index, void *replacement, void **original_slot,
                                 const char *name) {
  void ***object_vtable_ptr;
  void **vtable;
  void **slot;
  DWORD old_protect;
  DWORD restore_protect;

  if (object == NULL || replacement == NULL || original_slot == NULL) {
    return 0;
  }
  if (!memory_is_readable((uintptr_t)object, sizeof(void *))) {
    return 0;
  }

  object_vtable_ptr = (void ***)object;
  vtable = *object_vtable_ptr;
  if (vtable == NULL || !memory_is_readable((uintptr_t)&vtable[index], sizeof(void *))) {
    return 0;
  }

  slot = &vtable[index];
  if (*slot == replacement) {
    return 1;
  }
  if (*original_slot != NULL && *slot != *original_slot) {
    probe_log("textdraw_render_hook: skip name=%s object=%p index=%u slot=%p current=%p original=%p reason=vtable_changed",
              name != NULL ? name : "unknown", object, index, slot, *slot, *original_slot);
    return 0;
  }
  if (*original_slot == NULL) {
    *original_slot = *slot;
  }

  if (!VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old_protect)) {
    probe_log("textdraw_render_hook: VirtualProtect failed name=%s object=%p index=%u slot=%p err=%lu",
              name != NULL ? name : "unknown", object, index, slot, (unsigned long)GetLastError());
    return 0;
  }

  *slot = replacement;
  FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void *));
  (void)VirtualProtect(slot, sizeof(void *), old_protect, &restore_protect);
  probe_log("textdraw_render_hook: installed name=%s object=%p vtable=%p index=%u slot=%p original=%p replacement=%p "
            "evidence=PROBE_TRACE,TODO_VERIFY",
            name != NULL ? name : "unknown", object, vtable, index, slot, *original_slot, replacement);
  return 1;
}

static void install_d3dx_sprite_hooks(void *sprite) {
  if (!textdraw_render_enabled() || sprite == NULL) {
    return;
  }

  (void)patch_com_vtable_slot(sprite, PROBE_D3DXSPRITE_VTBL_BEGIN, (void *)hook_d3dx_sprite_begin,
                              &g_orig_d3dx_sprite_begin, "ID3DXSprite.Begin");
  (void)patch_com_vtable_slot(sprite, PROBE_D3DXSPRITE_VTBL_DRAW, (void *)hook_d3dx_sprite_draw,
                              &g_orig_d3dx_sprite_draw, "ID3DXSprite.Draw");
  (void)patch_com_vtable_slot(sprite, PROBE_D3DXSPRITE_VTBL_END, (void *)hook_d3dx_sprite_end,
                              &g_orig_d3dx_sprite_end, "ID3DXSprite.End");
}

static void install_d3d_texture_hooks(void *texture) {
  if (!textdraw_render_enabled() || texture == NULL) {
    return;
  }

  (void)patch_com_vtable_slot(texture, PROBE_D3DTEXTURE_VTBL_GET_SURFACE_LEVEL,
                              (void *)hook_d3d_texture_get_surface_level,
                              &g_orig_d3d_texture_get_surface_level,
                              "IDirect3DTexture9.GetSurfaceLevel");
}

static int install_d3d_device_hooks(int log_summary) {
  void *device;
  void **vtable;
  int installed = 0;

  if (!textdraw_render_enabled() || InterlockedCompareExchange(&g_d3d_device_hooks_installed, 0, 0) != 0) {
    return 0;
  }

  device = (void *)(uintptr_t)read_u32_or(PROBE_GTA_ADDR_D3D_DEVICE_PTR, 0u);
  if (device == NULL || !memory_is_readable((uintptr_t)device, sizeof(void *))) {
    return 0;
  }

  vtable = *(void ***)device;
  if (vtable == NULL || !memory_is_readable((uintptr_t)&vtable[PROBE_D3DDEV_VTBL_SET_FVF],
                                            sizeof(void *))) {
    return 0;
  }

  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_CREATE_TEXTURE, (void *)hook_d3d_create_texture,
                                     &g_orig_d3d_create_texture, "IDirect3DDevice9.CreateTexture");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_CREATE_RENDER_TARGET,
                                     (void *)hook_d3d_create_render_target, &g_orig_d3d_create_render_target,
                                     "IDirect3DDevice9.CreateRenderTarget");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_CREATE_DEPTH_STENCIL_SURFACE,
                                     (void *)hook_d3d_create_depth_stencil_surface,
                                     &g_orig_d3d_create_depth_stencil_surface,
                                     "IDirect3DDevice9.CreateDepthStencilSurface");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_STRETCH_RECT, (void *)hook_d3d_stretch_rect,
                                     &g_orig_d3d_stretch_rect, "IDirect3DDevice9.StretchRect");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_SET_RENDER_TARGET,
                                     (void *)hook_d3d_set_render_target, &g_orig_d3d_set_render_target,
                                     "IDirect3DDevice9.SetRenderTarget");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_GET_RENDER_TARGET,
                                     (void *)hook_d3d_get_render_target, &g_orig_d3d_get_render_target,
                                     "IDirect3DDevice9.GetRenderTarget");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_SET_DEPTH_STENCIL_SURFACE,
                                     (void *)hook_d3d_set_depth_stencil_surface,
                                     &g_orig_d3d_set_depth_stencil_surface,
                                     "IDirect3DDevice9.SetDepthStencilSurface");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_GET_DEPTH_STENCIL_SURFACE,
                                     (void *)hook_d3d_get_depth_stencil_surface,
                                     &g_orig_d3d_get_depth_stencil_surface,
                                     "IDirect3DDevice9.GetDepthStencilSurface");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_CLEAR, (void *)hook_d3d_clear,
                                     &g_orig_d3d_clear, "IDirect3DDevice9.Clear");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_SET_VIEWPORT, (void *)hook_d3d_set_viewport,
                                     &g_orig_d3d_set_viewport, "IDirect3DDevice9.SetViewport");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_SET_RENDER_STATE, (void *)hook_d3d_set_render_state,
                                     &g_orig_d3d_set_render_state, "IDirect3DDevice9.SetRenderState");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_SET_TEXTURE, (void *)hook_d3d_set_texture,
                                     &g_orig_d3d_set_texture, "IDirect3DDevice9.SetTexture");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_SET_FVF, (void *)hook_d3d_set_fvf,
                                     &g_orig_d3d_set_fvf, "IDirect3DDevice9.SetFVF");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_DRAW_PRIMITIVE, (void *)hook_d3d_draw_primitive,
                                     &g_orig_d3d_draw_primitive, "IDirect3DDevice9.DrawPrimitive");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_DRAW_INDEXED_PRIMITIVE,
                                     (void *)hook_d3d_draw_indexed_primitive, &g_orig_d3d_draw_indexed_primitive,
                                     "IDirect3DDevice9.DrawIndexedPrimitive");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_DRAW_PRIMITIVE_UP, (void *)hook_d3d_draw_primitive_up,
                                     &g_orig_d3d_draw_primitive_up, "IDirect3DDevice9.DrawPrimitiveUP");
  installed += patch_com_vtable_slot(device, PROBE_D3DDEV_VTBL_DRAW_INDEXED_PRIMITIVE_UP,
                                     (void *)hook_d3d_draw_indexed_primitive_up,
                                     &g_orig_d3d_draw_indexed_primitive_up,
                                     "IDirect3DDevice9.DrawIndexedPrimitiveUP");

  if (installed > 0) {
    g_d3d_device_vtable = vtable;
    InterlockedExchange(&g_d3d_device_hooks_installed, 1);
  }
  if (log_summary && installed > 0) {
    probe_log("textdraw_render_hook: d3d_device_summary installed=%d device=%p vtable=%p", installed, device, vtable);
  }
  return installed;
}

static DWORD gta_model_info_store_count_for_name(const char *name) {
  if (name == NULL) {
    return 0xffffffffu;
  }
  if (strstr(name, "AddAtomicModel") != NULL) {
    return read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu);
  }
  if (strstr(name, "AddTimeModel") != NULL) {
    return read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu);
  }
  if (strstr(name, "AddClumpModel") != NULL) {
    return read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu);
  }
  return 0xffffffffu;
}

static void *gta_model_info_ptr_for_id(int model_id) {
  uintptr_t slot;
  void *value = NULL;

  if (model_id < 0 || (unsigned)model_id >= PROBE_GTA_MODEL_INFO_MAX) {
    return NULL;
  }

  slot = PROBE_GTA_ADDR_MODEL_INFO_PTRS + ((uintptr_t)(unsigned)model_id * sizeof(void *));
  if (!memory_is_readable(slot, sizeof(value))) {
    return NULL;
  }
  memcpy(&value, (const void *)slot, sizeof(value));
  return value;
}

static uintptr_t gta_model_info_slot_address_for_id(int model_id) {
  if (model_id < 0 || (unsigned)model_id >= PROBE_GTA_MODEL_INFO_MAX) {
    return 0;
  }
  return PROBE_GTA_ADDR_MODEL_INFO_PTRS + ((uintptr_t)(unsigned)model_id * sizeof(void *));
}

static float read_f32_or(uintptr_t address, float fallback) {
  float value;

  if (!memory_is_readable(address, sizeof(value))) {
    return fallback;
  }

  memcpy(&value, (const void *)address, sizeof(value));
  return value;
}

static int probe_model_id_is_tracked(int model_id) {
  static const int tracked[] = {
      11682, 11683, 11684, 11692, 18777, 18781, 18784, 18794, 18808, 18809, 18818, 18824, 18826,
      18829, 18842, 18858, 18981, 18997, 19002, 19005, 19071, 19074, 19128,
      19312, 19313, 19316, 19332, 19333, 19378, 19425, 19649, 19667, 19894,
      19905, 19907, 19909,
  };
  size_t i;

  for (i = 0; i < sizeof(tracked) / sizeof(tracked[0]); ++i) {
    if (tracked[i] == model_id) {
      return 1;
    }
  }
  return 0;
}

static int probe_model_id_is_custom(int model_id) {
  if (model_id >= (int)PROBE_GTA_CUSTOM_LOW_MODEL_MIN && model_id <= (int)PROBE_GTA_CUSTOM_LOW_MODEL_MAX) {
    return 1;
  }
  return model_id >= (int)PROBE_GTA_CUSTOM_MODEL_MIN && model_id <= (int)PROBE_GTA_CUSTOM_MODEL_MAX;
}

static int probe_model_id_interesting(int model_id) {
  return probe_model_id_is_custom(model_id) || probe_model_id_is_tracked(model_id);
}

static void probe_bytes_hex_or(uintptr_t address, size_t size, char *out, size_t out_size) {
  BYTE bytes[PROBE_GTA_MODEL_INFO_DUMP_BYTES];

  if (out == NULL || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (size > sizeof(bytes)) {
    size = sizeof(bytes);
  }
  if (address < 0x10000u || size == 0 || !memory_is_readable(address, size)) {
    snprintf(out, out_size, "unreadable");
    return;
  }

  memcpy(bytes, (const void *)address, size);
  bytes_to_hex(bytes, size, out, out_size);
}

static void log_gta_model_info_dump(const char *phase, int model_id, void *caller, int force) {
  void *model_info = gta_model_info_ptr_for_id(model_id);
  uintptr_t model_info_addr = (uintptr_t)model_info;
  DWORD vtable;
  DWORD key;
  WORD txd_index;
  float draw_distance;
  DWORD col_model;
  char raw_hex[PROBE_GTA_MODEL_INFO_DUMP_BYTES * 3 + 8];
  char col_hex[PROBE_GTA_COL_MODEL_DUMP_BYTES * 3 + 8];

  if (!object_info_enabled()) {
    return;
  }
  if (!force && !probe_model_id_interesting(model_id)) {
    return;
  }
  if (model_info_addr < 0x10000u || !memory_is_readable(model_info_addr, 0x20u)) {
    probe_log("gta_object_info: phase=%s caller=%p caller_samp_rva=0x%08lx model=%d model_info=%p "
              "reason=missing_or_unreadable evidence=PROBE_TRACE,TODO_VERIFY",
              phase != NULL ? phase : "unknown", caller, samp_rva_from_address(caller), model_id, model_info);
    return;
  }

  vtable = read_u32_or(model_info_addr + PROBE_GTA_MODEL_INFO_OFFSET_VTABLE, 0xffffffffu);
  key = read_u32_or(model_info_addr + PROBE_GTA_MODEL_INFO_OFFSET_KEY, 0xffffffffu);
  txd_index = read_u16_or(model_info_addr + PROBE_GTA_MODEL_INFO_OFFSET_TXD_INDEX, 0xffffu);
  draw_distance = read_f32_or(model_info_addr + PROBE_GTA_MODEL_INFO_OFFSET_DRAW_DISTANCE, -1.0f);
  col_model = read_u32_or(model_info_addr + PROBE_GTA_MODEL_INFO_OFFSET_COL_MODEL, 0xffffffffu);
  probe_bytes_hex_or(model_info_addr, PROBE_GTA_MODEL_INFO_DUMP_BYTES, raw_hex, sizeof(raw_hex));
  probe_bytes_hex_or((uintptr_t)col_model, PROBE_GTA_COL_MODEL_DUMP_BYTES, col_hex, sizeof(col_hex));

  probe_log("gta_object_info: phase=%s caller=%p caller_samp_rva=0x%08lx model=%d model_info=%p "
            "vtable=0x%08lx key=0x%08lx txd_index=%u draw_distance=%.6f col_model=0x%08lx "
            "raw=%s col_raw=%s evidence=OBSERVED_037,PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
            phase != NULL ? phase : "unknown", caller, samp_rva_from_address(caller), model_id, model_info,
            (unsigned long)vtable, (unsigned long)key, (unsigned)txd_index, draw_distance, (unsigned long)col_model,
            raw_hex, col_hex);
}

static void scan_gta_custom_model_info_range(const char *phase, void *caller, int first_model, int last_model,
                                             LONG *logged, LONG *missing) {
  int model_id;

  for (model_id = first_model; model_id <= last_model; ++model_id) {
    if (gta_model_info_ptr_for_id(model_id) != NULL) {
      log_gta_model_info_dump(phase, model_id, caller, 1);
      if (logged != NULL) {
        ++(*logged);
      }
    } else {
      if (missing != NULL) {
        ++(*missing);
      }
    }
  }
}

static void scan_gta_custom_model_infos(const char *phase, void *caller) {
  LONG logged = 0;
  LONG missing = 0;

  if (!object_info_enabled()) {
    return;
  }

  scan_gta_custom_model_info_range(phase, caller, (int)PROBE_GTA_CUSTOM_LOW_MODEL_MIN,
                                   (int)PROBE_GTA_CUSTOM_LOW_MODEL_MAX, &logged, &missing);
  scan_gta_custom_model_info_range(phase, caller, (int)PROBE_GTA_CUSTOM_MODEL_MIN,
                                   (int)PROBE_GTA_CUSTOM_MODEL_MAX, &logged, &missing);

  probe_log("gta_object_info: scan phase=%s logged=%ld missing=%ld ranges=%u-%u,%u-%u "
            "evidence=OBSERVED_037,PROBE_TRACE,TODO_VERIFY",
            phase != NULL ? phase : "unknown", (long)logged, (long)missing, (unsigned)PROBE_GTA_CUSTOM_LOW_MODEL_MIN,
            (unsigned)PROBE_GTA_CUSTOM_LOW_MODEL_MAX, (unsigned)PROBE_GTA_CUSTOM_MODEL_MIN,
            (unsigned)PROBE_GTA_CUSTOM_MODEL_MAX);
}

static void log_tracked_gta_model_infos(const char *phase, void *caller) {
  static const int tracked[] = {
      11682, 11683, 11684, 11692, 18777, 18818, 18842, 18997, 19313, 19316, 19425, 19894,
      19905, 19907, 19909,
  };
  size_t i;

  if (!object_info_enabled()) {
    return;
  }

  for (i = 0; i < sizeof(tracked) / sizeof(tracked[0]); ++i) {
    log_gta_model_info_dump(phase, tracked[i], caller, 1);
  }
}

static DWORD decode_rel32_target_or(uintptr_t instruction_address, BYTE opcode, DWORD fallback) {
  int32_t rel;
  BYTE actual_opcode;

  if (!memory_is_readable(instruction_address, 5)) {
    return fallback;
  }

  actual_opcode = *(const BYTE *)instruction_address;
  if (actual_opcode != opcode) {
    return fallback;
  }

  memcpy(&rel, (const void *)(instruction_address + 1), sizeof(rel));
  return (DWORD)(instruction_address + 5u + (intptr_t)rel);
}

static void capture_transition_state(probe_transition_state *state) {
  if (state == NULL) {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->entry_gate = read_u32_or(0x00C8D4C0u, 0xffffffffu);
  state->hwnd = read_u32_or(0x00C97C1Cu, 0xffffffffu);
  state->id3d9 = read_u32_or(0x00C97C20u, 0xffffffffu);
  state->id3d9_device = read_u32_or(0x00C97C28u, 0xffffffffu);
  state->game_process_storage = read_u32_or(0x0053BED1u, 0xffffffffu);
  state->render2d_storage = read_u32_or(0x0053E22Cu, 0xffffffffu);
  state->ped_table = read_u32_or(0x00B74490u, 0xffffffffu);
  state->vehicle_table = read_u32_or(0x00B74494u, 0xffffffffu);
  state->script_call_target = decode_rel32_target_or(0x0053BFC7u, 0xe8u, 0xffffffffu);
  state->graphics_call_target = decode_rel32_target_or(0x0053EB12u, 0xe8u, 0xffffffffu);
  state->samp_base = (DWORD)g_samp_base;
  state->samp_size = g_samp_size;
  state->start_game = read_u8_or(0x00BA677Bu, 0xffu);
  state->game_started = read_u8_or(0x00BA6831u, 0xffu);
  state->menu = read_u8_or(0x00BA67A4u, 0xffu);
  state->menu2 = read_u8_or(0x00BA67A5u, 0xffu);
  state->menu3 = read_u8_or(0x00BA67A6u, 0xffu);
  state->hud = read_u8_or(0x00BA6769u, 0xffu);
  state->time_patch = read_u8_or(0x0052CF10u, 0xffu);
  state->script_gate0 = read_u8_or(0x00469EF5u, 0xffu);
  state->script_gate1 = read_u8_or(0x00469EF6u, 0xffu);
  state->script_call_opcode = read_u8_or(0x0053BFC7u, 0xffu);
  state->graphics_call_opcode = read_u8_or(0x0053EB12u, 0xffu);
  state->current_player = read_u8_or(0x00B7CD74u, 0xffu);
  state->camera_mode = read_u8_or(0x00B6F1A8u, 0xffu);
  state->camera_mode2 = read_u16_or(0x00B6F858u, 0xffffu);
}

static const char *probe_mem_state_name(DWORD state) {
  switch (state) {
    case MEM_COMMIT:
      return "COMMIT";
    case MEM_FREE:
      return "FREE";
    case MEM_RESERVE:
      return "RESERVE";
    default:
      return "unknown";
  }
}

static const char *probe_mem_type_name(DWORD type) {
  switch (type) {
    case MEM_IMAGE:
      return "IMAGE";
    case MEM_MAPPED:
      return "MAPPED";
    case MEM_PRIVATE:
      return "PRIVATE";
    default:
      return "unknown";
  }
}

static void probe_log_address_region(const char *label, uintptr_t address) {
  MEMORY_BASIC_INFORMATION mbi;
  char module_path[MAX_PATH];
  DWORD module_len = 0;

  module_path[0] = '\0';
  memset(&mbi, 0, sizeof(mbi));
  if (address != 0 && VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi)) == sizeof(mbi)) {
    if (mbi.AllocationBase != NULL) {
      module_len = GetModuleFileNameA((HMODULE)mbi.AllocationBase, module_path, sizeof(module_path));
      if (module_len >= sizeof(module_path)) {
        module_path[sizeof(module_path) - 1] = '\0';
      }
    }
    probe_log("region: label=%s addr=0x%08lx base=%p alloc=%p size=0x%08lx state=%s protect=0x%08lx type=%s module='%s'",
              label != NULL ? label : "unknown", (unsigned long)address, mbi.BaseAddress, mbi.AllocationBase,
              (unsigned long)mbi.RegionSize, probe_mem_state_name(mbi.State), (unsigned long)mbi.Protect,
              probe_mem_type_name(mbi.Type), module_len != 0 ? module_path : "");
    return;
  }

  probe_log("region: label=%s addr=0x%08lx unreadable", label != NULL ? label : "unknown", (unsigned long)address);
}

static void probe_log_memory_bytes(const char *label, uintptr_t address, size_t before, size_t size) {
  uintptr_t read_address;
  BYTE bytes[96];
  char hex[384];
  size_t readable_size;

  if (address == 0 || size == 0) {
    return;
  }

  read_address = address > before ? address - before : address;
  if (size > sizeof(bytes)) {
    size = sizeof(bytes);
  }
  readable_size = size;
  while (readable_size > 0 && !memory_is_readable(read_address, readable_size)) {
    --readable_size;
  }
  if (readable_size == 0) {
    probe_log("memdump: label=%s addr=0x%08lx read=0x%08lx unreadable", label != NULL ? label : "unknown",
              (unsigned long)address, (unsigned long)read_address);
    return;
  }

  memcpy(bytes, (const void *)read_address, readable_size);
  bytes_to_hex(bytes, readable_size, hex, sizeof(hex));
  probe_log("memdump: label=%s addr=0x%08lx read=0x%08lx size=%lu bytes=%s", label != NULL ? label : "unknown",
            (unsigned long)address, (unsigned long)read_address, (unsigned long)readable_size, hex);
}

static void probe_log_stack_dwords(uintptr_t sp) {
  DWORD words[16];
  char text[512];
  size_t i;
  size_t used = 0;

  if (sp == 0 || !memory_is_readable(sp, sizeof(words))) {
    probe_log("stack: sp=0x%08lx unreadable", (unsigned long)sp);
    return;
  }

  memcpy(words, (const void *)sp, sizeof(words));
  text[0] = '\0';
  for (i = 0; i < sizeof(words) / sizeof(words[0]) && used + 16 < sizeof(text); ++i) {
    int written = snprintf(text + used, sizeof(text) - used, "%s%02lu:0x%08lx", i == 0 ? "" : " ", (unsigned long)i,
                           (unsigned long)words[i]);
    if (written < 0) {
      break;
    }
    used += (size_t)written;
  }
  probe_log("stack: sp=0x%08lx dwords=%s", (unsigned long)sp, text);
}

static void probe_log_transition_snapshot(const char *reason) {
  probe_transition_state state;

  capture_transition_state(&state);
  probe_log("state-snapshot: reason=%s entry=%lu start=%u game_started=%u menu=%u/%u/%u hud=%u time_patch=0x%02x "
            "script_gate=%02x%02x script_target=0x%08lx graphics_target=0x%08lx gp_storage=0x%08lx "
            "render2d_storage=0x%08lx hwnd=0x%08lx d3d=0x%08lx d3ddev=0x%08lx ped_table=0x%08lx "
            "vehicle_table=0x%08lx current_player=%u camera=%u/%u samp=0x%08lx+0x%08lx",
            reason != NULL ? reason : "unknown", (unsigned long)state.entry_gate, (unsigned)state.start_game,
            (unsigned)state.game_started, (unsigned)state.menu, (unsigned)state.menu2, (unsigned)state.menu3,
            (unsigned)state.hud, (unsigned)state.time_patch, (unsigned)state.script_gate0,
            (unsigned)state.script_gate1, (unsigned long)state.script_call_target,
            (unsigned long)state.graphics_call_target, (unsigned long)state.game_process_storage,
            (unsigned long)state.render2d_storage, (unsigned long)state.hwnd, (unsigned long)state.id3d9,
            (unsigned long)state.id3d9_device, (unsigned long)state.ped_table, (unsigned long)state.vehicle_table,
            (unsigned)state.current_player, (unsigned)state.camera_mode, (unsigned)state.camera_mode2,
            (unsigned long)state.samp_base, (unsigned long)state.samp_size);
}

static void sample_transition_state(const char *reason) {
  probe_transition_state state;
  DWORD now_ms;
  int previous_valid;
  int changed;
  int force_log;

  if (env_flag_enabled("SAMP_PROBE_NO_STATE")) {
    return;
  }

  capture_transition_state(&state);
  now_ms = GetTickCount();
  previous_valid = InterlockedCompareExchange(&g_previous_state_valid, 0, 0) != 0;
  changed = !previous_valid || memcmp(&state, &g_previous_state, sizeof(state)) != 0;
  force_log = env_flag_enabled("SAMP_PROBE_STATE_ALWAYS") ||
              (DWORD)(now_ms - g_previous_state_log_ms) >= PROBE_STATE_HEARTBEAT_MS;

  if (!changed && !force_log) {
    return;
  }

  probe_log("state: reason=%s entry=%lu start=%u game_started=%u menu=%u/%u/%u hud=%u time_patch=0x%02x "
            "script_gate=%02x%02x script_call_op=0x%02x script_target=0x%08lx graphics_call_op=0x%02x "
            "graphics_target=0x%08lx gp_storage=0x%08lx render2d_storage=0x%08lx hwnd=0x%08lx d3d=0x%08lx "
            "d3ddev=0x%08lx ped_table=0x%08lx vehicle_table=0x%08lx current_player=%u camera=%u/%u "
            "samp=0x%08lx+0x%08lx",
            changed ? (reason ? reason : "changed") : "heartbeat", (unsigned long)state.entry_gate,
            (unsigned)state.start_game, (unsigned)state.game_started, (unsigned)state.menu, (unsigned)state.menu2,
            (unsigned)state.menu3, (unsigned)state.hud, (unsigned)state.time_patch, (unsigned)state.script_gate0,
            (unsigned)state.script_gate1, (unsigned)state.script_call_opcode, (unsigned long)state.script_call_target,
            (unsigned)state.graphics_call_opcode, (unsigned long)state.graphics_call_target,
            (unsigned long)state.game_process_storage, (unsigned long)state.render2d_storage, (unsigned long)state.hwnd,
            (unsigned long)state.id3d9, (unsigned long)state.id3d9_device, (unsigned long)state.ped_table,
            (unsigned long)state.vehicle_table, (unsigned)state.current_player, (unsigned)state.camera_mode,
            (unsigned)state.camera_mode2, (unsigned long)state.samp_base, (unsigned long)state.samp_size);

  if (changed) {
    memcpy(&g_previous_state, &state, sizeof(g_previous_state));
    InterlockedExchange(&g_previous_state_valid, 1);
  }
  g_previous_state_log_ms = now_ms;
}

static void sample_watchpoints(void) {
  size_t i;

  if (env_flag_enabled("SAMP_PROBE_NO_WATCH")) {
    return;
  }

  for (i = 0; i < sizeof(g_watchpoints) / sizeof(g_watchpoints[0]); ++i) {
    probe_watchpoint *wp = &g_watchpoints[i];
    BYTE current[16];
    char hex[80];

    if (wp->size > sizeof(current)) {
      continue;
    }

    if (!memory_is_readable(wp->address, wp->size)) {
      if (wp->has_previous) {
        probe_log("watch: name=%s addr=0x%08lx unreadable", wp->name, (unsigned long)wp->address);
        wp->has_previous = 0;
      }
      continue;
    }

    memcpy(current, (const void *)wp->address, wp->size);
    if (!wp->has_previous || memcmp(current, wp->previous, wp->size) != 0) {
      bytes_to_hex(current, wp->size, hex, sizeof(hex));
      probe_log("watch: name=%s addr=0x%08lx bytes=%s", wp->name, (unsigned long)wp->address, hex);
      memcpy(wp->previous, current, wp->size);
      wp->has_previous = 1;
    }
  }
}

static size_t modrm_extra_length(const BYTE *code, size_t pos) {
  BYTE modrm;
  BYTE mod;
  BYTE rm;
  BYTE base;
  size_t extra = 1;

  modrm = code[pos];
  mod = (BYTE)((modrm >> 6u) & 3u);
  rm = (BYTE)(modrm & 7u);

  if (mod != 3 && rm == 4) {
    BYTE sib = code[pos + extra];
    ++extra;
    base = (BYTE)(sib & 7u);
    if (mod == 0 && base == 5) {
      extra += 4;
    }
  } else if (mod == 0 && rm == 5) {
    extra += 4;
  }

  if (mod == 1) {
    extra += 1;
  } else if (mod == 2) {
    extra += 4;
  }

  return extra;
}

static size_t x86_instruction_length(const BYTE *code) {
  size_t pos = 0;
  BYTE op;

  while (code[pos] == 0xf0 || code[pos] == 0xf2 || code[pos] == 0xf3 || code[pos] == 0x2e || code[pos] == 0x36 ||
         code[pos] == 0x3e || code[pos] == 0x26 || code[pos] == 0x64 || code[pos] == 0x65 || code[pos] == 0x66 ||
         code[pos] == 0x67) {
    ++pos;
  }

  op = code[pos++];

  if ((op >= 0x50 && op <= 0x5f) || op == 0x90 || op == 0xc3 || op == 0xcc || op == 0x9c || op == 0x9d) {
    return pos;
  }
  if (op == 0xc2) {
    return pos + 2;
  }
  if (op == 0x6a) {
    return pos + 1;
  }
  if (op == 0x68 || (op >= 0xb8 && op <= 0xbf) || op == 0xa1 || op == 0xa2 || op == 0xa3) {
    return pos + 4;
  }
  if (op == 0xe8 || op == 0xe9 || op == 0xeb) {
    return 0;
  }
  if (op == 0x8b || op == 0x8a || op == 0x89 || op == 0x88 || op == 0x8d || op == 0x85 || op == 0x3b ||
      op == 0x33 || op == 0x32 || op == 0x31 || op == 0x2b || op == 0x39 || op == 0x8f || op == 0xff) {
    return pos + modrm_extra_length(code, pos);
  }
  if (op == 0x80 || op == 0x82 || op == 0x83) {
    return pos + modrm_extra_length(code, pos) + 1;
  }
  if (op == 0xc6) {
    return pos + modrm_extra_length(code, pos) + 1;
  }
  if (op == 0x81 || op == 0xc7) {
    return pos + modrm_extra_length(code, pos) + 4;
  }
  if (op == 0x0f) {
    BYTE op2 = code[pos++];
    if (op2 >= 0x80 && op2 <= 0x8f) {
      return 0;
    }
    return pos + modrm_extra_length(code, pos);
  }
  if (op >= 0xd8 && op <= 0xdf) {
    return pos + modrm_extra_length(code, pos);
  }

  return 0;
}

static void *resolve_inline_hook_target(void *target) {
  BYTE *cursor = (BYTE *)target;
  int depth;

  for (depth = 0; depth < 4 && cursor != NULL && memory_is_readable((uintptr_t)cursor, 8); ++depth) {
    if (cursor[0] == 0xe9) {
      int32_t rel = 0;
      memcpy(&rel, cursor + 1, sizeof(rel));
      cursor = cursor + 5 + rel;
      continue;
    }
    if (cursor[0] == 0xff && cursor[1] == 0x25) {
      uint32_t ptr_addr = 0;
      memcpy(&ptr_addr, cursor + 2, sizeof(ptr_addr));
      if (memory_is_readable(ptr_addr, sizeof(void *))) {
        cursor = *(BYTE **)(uintptr_t)ptr_addr;
        continue;
      }
    }
    break;
  }

  return cursor;
}

static size_t calculate_patch_length(void *target) {
  BYTE *cursor = (BYTE *)target;
  size_t total = 0;

  while (total < 5 && total < PROBE_INLINE_HOOK_MAX_COPY) {
    size_t len;
    if (!memory_is_readable((uintptr_t)(cursor + total), 8)) {
      return 0;
    }
    len = x86_instruction_length(cursor + total);
    if (len == 0 || total + len > PROBE_INLINE_HOOK_MAX_COPY) {
      return 0;
    }
    total += len;
  }

  return total >= 5 ? total : 0;
}

static void *create_trampoline(void *target, const BYTE *saved, size_t saved_len) {
  BYTE *trampoline;
  int32_t rel_back;
  DWORD old_protect = 0;

  trampoline = (BYTE *)VirtualAlloc(NULL, saved_len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (trampoline == NULL) {
    return NULL;
  }

  memcpy(trampoline, saved, saved_len);
  trampoline[saved_len] = 0xe9;
  rel_back = (int32_t)((intptr_t)((BYTE *)target + saved_len) - (intptr_t)(trampoline + saved_len + 5));
  memcpy(trampoline + saved_len + 1, &rel_back, sizeof(rel_back));
  FlushInstructionCache(GetCurrentProcess(), trampoline, saved_len + 5);
  (void)VirtualProtect(trampoline, saved_len + 5, PAGE_EXECUTE_READ, &old_protect);
  return trampoline;
}

static int install_inline_hook(probe_inline_hook *hook) {
  HMODULE module;
  FARPROC export_addr;
  void *patch_target;
  size_t patch_len;
  BYTE *target_bytes;
  DWORD old_protect = 0;
  DWORD restore_protect = 0;
  int32_t rel;
  size_t i;

  if (hook == NULL || InterlockedCompareExchange(&hook->installed, 0, 0) != 0) {
    return 0;
  }

  module = GetModuleHandleA(hook->dll_name);
  if (module == NULL) {
    module = LoadLibraryA(hook->dll_name);
  }
  if (module == NULL) {
    return 0;
  }

  export_addr = GetProcAddress(module, hook->symbol_name);
  if (export_addr == NULL) {
    return 0;
  }

  patch_target = resolve_inline_hook_target((void *)export_addr);
  if (patch_target == NULL || patch_target == hook->replacement || !memory_is_readable((uintptr_t)patch_target, 8)) {
    return 0;
  }

  target_bytes = (BYTE *)patch_target;
  if (target_bytes[0] == 0xe9) {
    int32_t existing_rel = 0;
    void *existing_target;
    memcpy(&existing_rel, target_bytes + 1, sizeof(existing_rel));
    existing_target = target_bytes + 5 + existing_rel;
    if (existing_target == hook->replacement) {
      InterlockedExchange(&hook->installed, 1);
      return 1;
    }
  }

  patch_len = calculate_patch_length(patch_target);
  if (patch_len == 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("inline_hook: skip dll=%s name=%s target=%p prologue=%s", hook->dll_name, hook->symbol_name, patch_target, hex);
    return 0;
  }

  memcpy(hook->saved, patch_target, patch_len);
  hook->saved_len = patch_len;
  hook->target = (void *)export_addr;
  hook->patch_target = patch_target;
  hook->trampoline = create_trampoline(patch_target, hook->saved, hook->saved_len);
  if (hook->trampoline == NULL) {
    probe_log("inline_hook: trampoline failed dll=%s name=%s target=%p", hook->dll_name, hook->symbol_name, patch_target);
    return 0;
  }

  if (hook->original_slot != NULL) {
    *hook->original_slot = hook->trampoline;
  }

  if (!VirtualProtect(patch_target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
    probe_log("inline_hook: VirtualProtect failed dll=%s name=%s target=%p error=%lu", hook->dll_name, hook->symbol_name,
              patch_target, (unsigned long)GetLastError());
    return 0;
  }

  target_bytes[0] = 0xe9;
  rel = (int32_t)((intptr_t)hook->replacement - (intptr_t)(target_bytes + 5));
  memcpy(target_bytes + 1, &rel, sizeof(rel));
  for (i = 5; i < patch_len; ++i) {
    target_bytes[i] = 0x90;
  }
  FlushInstructionCache(GetCurrentProcess(), patch_target, patch_len);
  (void)VirtualProtect(patch_target, patch_len, old_protect, &restore_protect);

  InterlockedExchange(&hook->installed, 1);
  probe_log("inline_hook: installed dll=%s name=%s export=%p target=%p trampoline=%p len=%lu", hook->dll_name,
            hook->symbol_name, (void *)export_addr, patch_target, hook->trampoline, (unsigned long)patch_len);
  return 1;
}

static int install_inline_hooks(int log_summary) {
  size_t i;
  int installed = 0;

  if (env_flag_enabled("SAMP_PROBE_NO_INLINE_HOOKS")) {
    if (log_summary) {
      probe_log("inline_hook: disabled by env");
    }
    return 0;
  }

  for (i = 0; i < sizeof(g_inline_hooks) / sizeof(g_inline_hooks[0]); ++i) {
    installed += install_inline_hook(&g_inline_hooks[i]);
  }
  if (asset_path_hooks_enabled()) {
    for (i = 0; i < sizeof(g_file_path_inline_hooks) / sizeof(g_file_path_inline_hooks[0]); ++i) {
      installed += install_inline_hook(&g_file_path_inline_hooks[i]);
    }
  }
  if (asset_read_hooks_enabled()) {
    for (i = 0; i < sizeof(g_file_read_inline_hooks) / sizeof(g_file_read_inline_hooks[0]); ++i) {
      installed += install_inline_hook(&g_file_read_inline_hooks[i]);
    }
  }

  if (log_summary) {
    probe_log("inline_hook: summary installed_or_present=%d requested=%u asset_path_hooks=%d asset_read_hooks=%d",
              installed, (unsigned)(sizeof(g_inline_hooks) / sizeof(g_inline_hooks[0])), asset_path_hooks_enabled(),
              asset_read_hooks_enabled());
  }
  return installed;
}

static int install_samp_code_hook(probe_code_hook *hook) {
  BYTE *target_bytes;
  void *patch_target;
  size_t patch_len;
  size_t readable_len;
  DWORD old_protect = 0;
  DWORD restore_protect = 0;
  int32_t rel;
  size_t i;

  if (hook == NULL || InterlockedCompareExchange(&hook->installed, 0, 0) != 0) {
    return 0;
  }
  if (g_samp_base == 0 || g_samp_size == 0 || hook->rva >= g_samp_size ||
      hook->rva + hook->expected_len > g_samp_size) {
    return 0;
  }

  patch_target = (void *)(g_samp_base + (uintptr_t)hook->rva);
  readable_len = hook->expected_len > 8 ? hook->expected_len : 8;
  if (patch_target == hook->replacement || !memory_is_readable((uintptr_t)patch_target, readable_len)) {
    return 0;
  }

  target_bytes = (BYTE *)patch_target;
  if (target_bytes[0] == 0xe9) {
    int32_t existing_rel = 0;
    void *existing_target;
    memcpy(&existing_rel, target_bytes + 1, sizeof(existing_rel));
    existing_target = target_bytes + 5 + existing_rel;
    if (existing_target == hook->replacement) {
      InterlockedExchange(&hook->installed, 1);
      return 1;
    }
  }

  if (hook->expected_len > 0 && memcmp(patch_target, hook->expected, hook->expected_len) != 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("samp_code_hook: skip name=%s rva=0x%08lx target=%p prologue=%s", hook->name, (unsigned long)hook->rva,
              patch_target, hex);
    InterlockedExchange(&hook->installed, -1);
    return 0;
  }

  patch_len = calculate_patch_length(patch_target);
  if (patch_len == 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("samp_code_hook: skip name=%s rva=0x%08lx target=%p prologue=%s", hook->name, (unsigned long)hook->rva,
              patch_target, hex);
    InterlockedExchange(&hook->installed, -1);
    return 0;
  }

  memcpy(hook->saved, patch_target, patch_len);
  hook->saved_len = patch_len;
  hook->patch_target = patch_target;
  hook->trampoline = create_trampoline(patch_target, hook->saved, hook->saved_len);
  if (hook->trampoline == NULL) {
    probe_log("samp_code_hook: trampoline failed name=%s rva=0x%08lx target=%p", hook->name, (unsigned long)hook->rva,
              patch_target);
    return 0;
  }

  if (hook->original_slot != NULL) {
    *hook->original_slot = hook->trampoline;
  }

  if (!VirtualProtect(patch_target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
    probe_log("samp_code_hook: VirtualProtect failed name=%s rva=0x%08lx target=%p error=%lu", hook->name,
              (unsigned long)hook->rva, patch_target, (unsigned long)GetLastError());
    return 0;
  }

  target_bytes[0] = 0xe9;
  rel = (int32_t)((intptr_t)hook->replacement - (intptr_t)(target_bytes + 5));
  memcpy(target_bytes + 1, &rel, sizeof(rel));
  for (i = 5; i < patch_len; ++i) {
    target_bytes[i] = 0x90;
  }
  FlushInstructionCache(GetCurrentProcess(), patch_target, patch_len);
  (void)VirtualProtect(patch_target, patch_len, old_protect, &restore_protect);

  InterlockedExchange(&hook->installed, 1);
  probe_log("samp_code_hook: installed name=%s rva=0x%08lx target=%p trampoline=%p len=%lu", hook->name,
            (unsigned long)hook->rva, patch_target, hook->trampoline, (unsigned long)patch_len);
  return 1;
}

static int install_samp_code_hooks(int log_summary) {
  size_t i;
  int installed = 0;

  if (env_flag_enabled("SAMP_PROBE_NO_SAMP_CODE_HOOKS")) {
    if (log_summary) {
      probe_log("samp_code_hook: disabled by env");
    }
    return 0;
  }
  if (!samp_code_hooks_enabled()) {
    if (log_summary) {
      probe_log("samp_code_hook: disabled by default; enable with SAMP_PROBE_ENABLE_SAMP_CODE_HOOKS=1 or %s",
                PROBE_SAMP_CODE_HOOKS_FLAG);
    }
    return 0;
  }

  for (i = 0; i < sizeof(g_samp_code_hooks) / sizeof(g_samp_code_hooks[0]); ++i) {
    installed += install_samp_code_hook(&g_samp_code_hooks[i]);
  }

  if (log_summary) {
    probe_log("samp_code_hook: summary installed=%d requested=%u", installed,
              (unsigned)(sizeof(g_samp_code_hooks) / sizeof(g_samp_code_hooks[0])));
  }
  return installed;
}

static int samp_font5_identity_matches(void) {
  PIMAGE_NT_HEADERS nt = get_samp_nt_headers();

  /* PROBE_TRACE (original R5 run 2026-07-15 23:15): get_samp_nt_headers()
   * successfully reported the expected entry RVA and image size while this
   * identity check rejected the same Wine-mapped image.  INFERRED: the
   * redundant VirtualQuery readability check was the differing condition.
   * Reuse the already validated PE parser here, keep checking the timestamp,
   * and require exact target prologues and decoded patch lengths below. */
  return nt != NULL && g_samp_size == PROBE_SAMP_R5_IMAGE_SIZE &&
         nt->FileHeader.TimeDateStamp == PROBE_SAMP_R5_TIMESTAMP &&
         nt->OptionalHeader.AddressOfEntryPoint == PROBE_SAMP_R5_ENTRY_RVA &&
         nt->OptionalHeader.SizeOfImage == PROBE_SAMP_R5_IMAGE_SIZE;
}

static int preflight_samp_font5_code_hooks(void) {
  size_t i;

  /* STATIC_037 + TODO_VERIFY:
   * Treat the two Font 5 hooks as one guarded probe.  Validate every target,
   * exact overwritten prologue, and decoded patch length before the first
   * target is mutated.  This avoids leaving only the prepare hook installed
   * when the dispatch site is already owned by another module. */
  for (i = 0; i < sizeof(g_samp_font5_code_hooks) / sizeof(g_samp_font5_code_hooks[0]); ++i) {
    probe_code_hook *hook = &g_samp_font5_code_hooks[i];
    void *patch_target;
    size_t readable_len;
    size_t patch_len;

    if (hook->expected_len < 5 || hook->expected_len > sizeof(hook->expected) ||
        hook->expected_len > PROBE_INLINE_HOOK_MAX_COPY) {
      probe_log("font5_code_hook: pair_preflight_failed name=%s rva=0x%08lx reason=invalid_expected_len len=%lu",
                hook->name, (unsigned long)hook->rva, (unsigned long)hook->expected_len);
      return 0;
    }

    readable_len = hook->expected_len > 8 ? hook->expected_len : 8;
    if (g_samp_base == 0 || g_samp_size < readable_len || hook->rva >= g_samp_size ||
        hook->rva > g_samp_size - readable_len) {
      probe_log("font5_code_hook: pair_preflight_failed name=%s rva=0x%08lx reason=target_bounds",
                hook->name, (unsigned long)hook->rva);
      return 0;
    }

    patch_target = (void *)(g_samp_base + (uintptr_t)hook->rva);
    if (patch_target == hook->replacement || !memory_is_readable((uintptr_t)patch_target, readable_len)) {
      probe_log("font5_code_hook: pair_preflight_failed name=%s rva=0x%08lx target=%p reason=unreadable_target",
                hook->name, (unsigned long)hook->rva, patch_target);
      return 0;
    }

    if (memcmp(patch_target, hook->expected, hook->expected_len) != 0) {
      BYTE bytes[8];
      char hex[40];
      memcpy(bytes, patch_target, sizeof(bytes));
      bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
      probe_log("font5_code_hook: pair_preflight_failed name=%s rva=0x%08lx target=%p reason=prologue "
                "actual=%s",
                hook->name, (unsigned long)hook->rva, patch_target, hex);
      return 0;
    }

    patch_len = calculate_patch_length(patch_target);
    if (patch_len == 0 || patch_len != hook->expected_len || hook->rva > g_samp_size - patch_len) {
      probe_log("font5_code_hook: pair_preflight_failed name=%s rva=0x%08lx target=%p reason=patch_len "
                "decoded=%lu expected=%lu",
                hook->name, (unsigned long)hook->rva, patch_target, (unsigned long)patch_len,
                (unsigned long)hook->expected_len);
      return 0;
    }
  }

  probe_log("font5_code_hook: pair_preflight_ok hooks=%u evidence=STATIC_037,TODO_VERIFY",
            (unsigned)(sizeof(g_samp_font5_code_hooks) / sizeof(g_samp_font5_code_hooks[0])));
  return 1;
}

static int install_samp_font5_code_hooks(int log_summary) {
  size_t i;
  size_t hook_count = sizeof(g_samp_font5_code_hooks) / sizeof(g_samp_font5_code_hooks[0]);
  size_t already_installed = 0;
  int installed = 0;

  if (!font5_hooks_enabled()) {
    if (log_summary) {
      probe_log("font5_code_hook: disabled by default; enable with SAMP_PROBE_FONT5_HOOKS=1 or %s",
                PROBE_FONT5_HOOKS_FLAG);
    }
    return 0;
  }
  if (env_flag_enabled("SAMP_PROBE_NO_SAMP_CODE_HOOKS")) {
    if (log_summary) {
      probe_log("font5_code_hook: disabled by SAMP_PROBE_NO_SAMP_CODE_HOOKS");
    }
    return 0;
  }
  if (!samp_font5_identity_matches()) {
    if (log_summary) {
      PIMAGE_NT_HEADERS actual_nt = get_samp_nt_headers();
      DWORD actual_timestamp = actual_nt != NULL ? actual_nt->FileHeader.TimeDateStamp : 0u;
      DWORD actual_entry = actual_nt != NULL ? actual_nt->OptionalHeader.AddressOfEntryPoint : 0u;
      DWORD actual_header_size = actual_nt != NULL ? actual_nt->OptionalHeader.SizeOfImage : 0u;
      probe_log("font5_code_hook: skip unsupported_identity headers_valid=%d "
                "timestamp_expected=0x%08lx timestamp_actual=0x%08lx "
                "entry_expected=0x%08lx entry_actual=0x%08lx "
                "size_expected=0x%08lx header_size_actual=0x%08lx module_size_actual=0x%08lx supported_sha256="
                "b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2 "
                "evidence=STATIC_037,TODO_VERIFY",
                actual_nt != NULL, (unsigned long)PROBE_SAMP_R5_TIMESTAMP, (unsigned long)actual_timestamp,
                (unsigned long)PROBE_SAMP_R5_ENTRY_RVA, (unsigned long)actual_entry,
                (unsigned long)PROBE_SAMP_R5_IMAGE_SIZE, (unsigned long)actual_header_size,
                (unsigned long)g_samp_size);
    }
    return 0;
  }

  for (i = 0; i < hook_count; ++i) {
    LONG state = InterlockedCompareExchange(&g_samp_font5_code_hooks[i].installed, 0, 0);
    if (state == 1) {
      ++already_installed;
    } else if (state != 0) {
      return 0;
    }
  }
  if (already_installed == hook_count) {
    return 0;
  }
  if (already_installed != 0) {
    if (log_summary) {
      probe_log("font5_code_hook: skip partial_prior_state installed=%u requested=%u",
                (unsigned)already_installed, (unsigned)hook_count);
    }
    return 0;
  }

  if (!preflight_samp_font5_code_hooks()) {
    for (i = 0; i < hook_count; ++i) {
      InterlockedExchange(&g_samp_font5_code_hooks[i].installed, -1);
    }
    return 0;
  }

  for (i = 0; i < hook_count; ++i) {
    installed += install_samp_code_hook(&g_samp_font5_code_hooks[i]);
  }
  if ((size_t)installed != hook_count) {
    probe_log("font5_code_hook: incomplete_install installed=%d requested=%u; discard run and terminate process",
              installed, (unsigned)hook_count);
  }
  if (log_summary) {
    probe_log("font5_code_hook: summary installed=%d requested=%u supported_sha256="
              "b72b5dbe725f81864ca3f78bc7063bda56cc05fc7188af822fa7a754432553a2 "
              "evidence=STATIC_037,TODO_VERIFY",
              installed, (unsigned)hook_count);
  }
  return installed;
}

static int font5_code_hooks_active(void) {
  size_t i;

  for (i = 0; i < sizeof(g_samp_font5_code_hooks) / sizeof(g_samp_font5_code_hooks[0]); ++i) {
    if (InterlockedCompareExchange(&g_samp_font5_code_hooks[i].installed, 0, 0) != 1) {
      return 0;
    }
  }
  return 1;
}

static int install_absolute_code_hook(probe_code_hook *hook) {
  BYTE *target_bytes;
  void *patch_target;
  void *requested_target;
  size_t patch_len;
  DWORD old_protect = 0;
  DWORD restore_protect = 0;
  int32_t rel;
  size_t i;

  if (hook == NULL || InterlockedCompareExchange(&hook->installed, 0, 0) != 0) {
    return 0;
  }

  requested_target = (void *)(uintptr_t)hook->rva;
  if (requested_target == NULL || requested_target == hook->replacement ||
      !memory_is_readable((uintptr_t)requested_target, 8)) {
    return 0;
  }

  patch_target = resolve_inline_hook_target(requested_target);
  if (patch_target == NULL || patch_target == hook->replacement || !memory_is_readable((uintptr_t)patch_target, 8)) {
    return 0;
  }

  target_bytes = (BYTE *)patch_target;
  if (target_bytes[0] == 0xe9) {
    int32_t existing_rel = 0;
    void *existing_target;
    memcpy(&existing_rel, target_bytes + 1, sizeof(existing_rel));
    existing_target = target_bytes + 5 + existing_rel;
    if (existing_target == hook->replacement) {
      InterlockedExchange(&hook->installed, 1);
      return 1;
    }
  }

  if (hook->expected_len > 0 && patch_target == requested_target &&
      memcmp(patch_target, hook->expected, hook->expected_len) != 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("gta_code_hook: skip name=%s addr=0x%08lx target=%p prologue=%s", hook->name,
              (unsigned long)hook->rva, patch_target, hex);
    InterlockedExchange(&hook->installed, -1);
    return 0;
  }

  patch_len = calculate_patch_length(patch_target);
  if (patch_len == 0) {
    BYTE bytes[8];
    char hex[40];
    memcpy(bytes, patch_target, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), hex, sizeof(hex));
    probe_log("gta_code_hook: skip name=%s addr=0x%08lx target=%p resolved_from=%p prologue=%s", hook->name,
              (unsigned long)hook->rva, patch_target, requested_target, hex);
    InterlockedExchange(&hook->installed, -1);
    return 0;
  }

  memcpy(hook->saved, patch_target, patch_len);
  hook->saved_len = patch_len;
  hook->patch_target = patch_target;
  hook->trampoline = create_trampoline(patch_target, hook->saved, hook->saved_len);
  if (hook->trampoline == NULL) {
    probe_log("gta_code_hook: trampoline failed name=%s addr=0x%08lx target=%p", hook->name,
              (unsigned long)hook->rva, patch_target);
    return 0;
  }

  if (hook->original_slot != NULL) {
    *hook->original_slot = hook->trampoline;
  }

  if (!VirtualProtect(patch_target, patch_len, PAGE_EXECUTE_READWRITE, &old_protect)) {
    probe_log("gta_code_hook: VirtualProtect failed name=%s addr=0x%08lx target=%p error=%lu", hook->name,
              (unsigned long)hook->rva, patch_target, (unsigned long)GetLastError());
    return 0;
  }

  target_bytes[0] = 0xe9;
  rel = (int32_t)((intptr_t)hook->replacement - (intptr_t)(target_bytes + 5));
  memcpy(target_bytes + 1, &rel, sizeof(rel));
  for (i = 5; i < patch_len; ++i) {
    target_bytes[i] = 0x90;
  }
  FlushInstructionCache(GetCurrentProcess(), patch_target, patch_len);
  (void)VirtualProtect(patch_target, patch_len, old_protect, &restore_protect);

  InterlockedExchange(&hook->installed, 1);
  probe_log("gta_code_hook: installed name=%s addr=0x%08lx requested=%p target=%p trampoline=%p len=%lu",
            hook->name, (unsigned long)hook->rva, requested_target, patch_target, hook->trampoline,
            (unsigned long)patch_len);
  return 1;
}

static int install_gta_asset_code_hooks(int log_summary) {
  size_t i;
  int installed = 0;

  if (!gta_asset_hooks_enabled()) {
    if (log_summary) {
      probe_log("gta_code_hook: disabled by default; enable with SAMP_PROBE_GTA_ASSET_HOOKS=1 or %s",
                PROBE_GTA_ASSET_HOOKS_FLAG);
    }
    return 0;
  }

  for (i = 0; i < sizeof(g_gta_asset_code_hooks) / sizeof(g_gta_asset_code_hooks[0]); ++i) {
    installed += install_absolute_code_hook(&g_gta_asset_code_hooks[i]);
  }

  if (log_summary) {
    probe_log("gta_code_hook: summary installed=%d requested=%u", installed,
              (unsigned)(sizeof(g_gta_asset_code_hooks) / sizeof(g_gta_asset_code_hooks[0])));
  }
  return installed;
}

static int install_gta_textdraw_code_hooks(int log_summary) {
  size_t i;
  int installed = 0;

  if (!textdraw_hooks_enabled()) {
    if (log_summary) {
      probe_log("textdraw_code_hook: disabled by default; enable with SAMP_PROBE_TEXTDRAW_HOOKS=1 or %s",
                PROBE_TEXTDRAW_HOOKS_FLAG);
    }
    return 0;
  }

  for (i = 0; i < sizeof(g_gta_textdraw_code_hooks) / sizeof(g_gta_textdraw_code_hooks[0]); ++i) {
    installed += install_absolute_code_hook(&g_gta_textdraw_code_hooks[i]);
  }

  if (log_summary) {
    probe_log("textdraw_code_hook: summary installed=%d requested=%u", installed,
              (unsigned)(sizeof(g_gta_textdraw_code_hooks) / sizeof(g_gta_textdraw_code_hooks[0])));
  }
  return installed;
}

static int rva_is_sane(PIMAGE_NT_HEADERS nt, DWORD rva, DWORD min_size) {
  return rva != 0 && rva + min_size >= rva && rva + min_size <= nt->OptionalHeader.SizeOfImage;
}

static void dump_samp_sections(PIMAGE_NT_HEADERS nt) {
  PIMAGE_SECTION_HEADER section;
  WORD i;

  section = IMAGE_FIRST_SECTION(nt);
  for (i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    char name[9];
    memcpy(name, section[i].Name, 8);
    name[8] = '\0';
    probe_log("section: index=%u name=%s va=0x%08lx vsz=0x%08lx raw=0x%08lx chars=0x%08lx", (unsigned)i, name,
              (unsigned long)section[i].VirtualAddress, (unsigned long)section[i].Misc.VirtualSize,
              (unsigned long)section[i].SizeOfRawData, (unsigned long)section[i].Characteristics);
  }
}

static void dump_samp_imports(PIMAGE_NT_HEADERS nt) {
  DWORD import_rva;
  PIMAGE_IMPORT_DESCRIPTOR desc;
  int logged = 0;

  import_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if (!rva_is_sane(nt, import_rva, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
    probe_log("imports: none");
    return;
  }

  desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE *)g_samp_module + import_rva);
  for (; desc->Name != 0; ++desc) {
    const char *dll_name;
    PIMAGE_THUNK_DATA name_thunk;
    PIMAGE_THUNK_DATA iat_thunk;

    if (!rva_is_sane(nt, desc->Name, 1)) {
      continue;
    }

    dll_name = (const char *)((BYTE *)g_samp_module + desc->Name);
    name_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module +
                                     (desc->OriginalFirstThunk != 0 ? desc->OriginalFirstThunk : desc->FirstThunk));
    iat_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module + desc->FirstThunk);
    probe_log("import_dll: %s first_thunk=0x%08lx", dll_name, (unsigned long)desc->FirstThunk);

    for (; name_thunk->u1.AddressOfData != 0 && iat_thunk->u1.Function != 0; ++name_thunk, ++iat_thunk) {
      const char *symbol = NULL;
      void *target = (void *)(uintptr_t)iat_thunk->u1.Function;

      if (IMAGE_SNAP_BY_ORDINAL(name_thunk->u1.Ordinal)) {
        WORD ordinal = (WORD)IMAGE_ORDINAL(name_thunk->u1.Ordinal);
        const char *known_name = known_ordinal_name(dll_name, ordinal);
        if (known_name != NULL) {
          probe_log("import: dll=%s ordinal=%u known=%s iat=%p target=%p", dll_name, (unsigned)ordinal, known_name,
                    (void *)&iat_thunk->u1.Function, target);
        } else {
          probe_log("import: dll=%s ordinal=%u iat=%p target=%p", dll_name, (unsigned)ordinal,
                    (void *)&iat_thunk->u1.Function, target);
        }
      } else if (rva_is_sane(nt, (DWORD)name_thunk->u1.AddressOfData, sizeof(IMAGE_IMPORT_BY_NAME))) {
        PIMAGE_IMPORT_BY_NAME by_name = (PIMAGE_IMPORT_BY_NAME)((BYTE *)g_samp_module + name_thunk->u1.AddressOfData);
        symbol = (const char *)by_name->Name;
        if (InterlockedIncrement(&g_import_log_count) <= PROBE_MAX_IMPORT_LOGS) {
          probe_log("import: dll=%s name=%s iat=%p target=%p", dll_name, symbol, (void *)&iat_thunk->u1.Function, target);
        }
      }

      ++logged;
    }
  }

  probe_log("imports: logged_entries=%d", logged);
}

static int hook_one_import(PIMAGE_NT_HEADERS nt, probe_iat_hook *hook) {
  DWORD import_rva;
  PIMAGE_IMPORT_DESCRIPTOR desc;

  import_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if (!rva_is_sane(nt, import_rva, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
    return 0;
  }

  desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE *)g_samp_module + import_rva);
  for (; desc->Name != 0; ++desc) {
    const char *dll_name;
    PIMAGE_THUNK_DATA name_thunk;
    PIMAGE_THUNK_DATA iat_thunk;

    if (!rva_is_sane(nt, desc->Name, 1)) {
      continue;
    }

    dll_name = (const char *)((BYTE *)g_samp_module + desc->Name);
    if (lstrcmpiA(dll_name, hook->dll_name) != 0) {
      continue;
    }

    name_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module +
                                     (desc->OriginalFirstThunk != 0 ? desc->OriginalFirstThunk : desc->FirstThunk));
    iat_thunk = (PIMAGE_THUNK_DATA)((BYTE *)g_samp_module + desc->FirstThunk);

    for (; name_thunk->u1.AddressOfData != 0 && iat_thunk->u1.Function != 0; ++name_thunk, ++iat_thunk) {
      PIMAGE_IMPORT_BY_NAME by_name;
      void **slot;
      DWORD old_protect;
      DWORD restore_protect;

      if (IMAGE_SNAP_BY_ORDINAL(name_thunk->u1.Ordinal)) {
        WORD ordinal = (WORD)IMAGE_ORDINAL(name_thunk->u1.Ordinal);
        if (hook->ordinal == 0 || ordinal != hook->ordinal) {
          continue;
        }
      } else {
        if (!rva_is_sane(nt, (DWORD)name_thunk->u1.AddressOfData, sizeof(IMAGE_IMPORT_BY_NAME))) {
          continue;
        }

        by_name = (PIMAGE_IMPORT_BY_NAME)((BYTE *)g_samp_module + name_thunk->u1.AddressOfData);
        if (hook->symbol_name == NULL || lstrcmpA((const char *)by_name->Name, hook->symbol_name) != 0) {
          continue;
        }
      }

      slot = (void **)&iat_thunk->u1.Function;
      if (*slot == hook->replacement) {
        return 1;
      }

      if (hook->original_slot != NULL && (*hook->original_slot == NULL || lstrcmpiA(hook->dll_name, "WS2_32.dll") == 0)) {
        *hook->original_slot = *slot;
      }

      if (!VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old_protect)) {
        probe_log("hook: failed dll=%s name=%s iat=%p error=%lu", hook->dll_name, hook->symbol_name, slot,
                  (unsigned long)GetLastError());
        return 0;
      }

      *slot = hook->replacement;
      FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void *));
      (void)VirtualProtect(slot, sizeof(void *), old_protect, &restore_protect);
      InterlockedIncrement(&hook->install_count);
      if (IMAGE_SNAP_BY_ORDINAL(name_thunk->u1.Ordinal)) {
        probe_log("hook: installed dll=%s ordinal=%u name=%s iat=%p original=%p replacement=%p", hook->dll_name,
                  (unsigned)hook->ordinal, hook->symbol_name ? hook->symbol_name : "", slot,
                  hook->original_slot ? *hook->original_slot : NULL, hook->replacement);
      } else {
        probe_log("hook: installed dll=%s name=%s iat=%p original=%p replacement=%p", hook->dll_name,
                  hook->symbol_name ? hook->symbol_name : "", slot, hook->original_slot ? *hook->original_slot : NULL,
                  hook->replacement);
      }
      return 1;
    }
  }

  return 0;
}

static int install_iat_hooks(PIMAGE_NT_HEADERS nt, int log_summary) {
  size_t i;
  int installed = 0;

  for (i = 0; i < sizeof(g_hooks) / sizeof(g_hooks[0]); ++i) {
    installed += hook_one_import(nt, &g_hooks[i]);
  }
  if (asset_path_hooks_enabled()) {
    for (i = 0; i < sizeof(g_file_path_hooks) / sizeof(g_file_path_hooks[0]); ++i) {
      installed += hook_one_import(nt, &g_file_path_hooks[i]);
    }
  }
  if (asset_read_hooks_enabled()) {
    for (i = 0; i < sizeof(g_file_read_hooks) / sizeof(g_file_read_hooks[0]); ++i) {
      installed += hook_one_import(nt, &g_file_read_hooks[i]);
    }
  }
  if (textdraw_render_enabled()) {
    for (i = 0; i < sizeof(g_textdraw_render_iat_hooks) / sizeof(g_textdraw_render_iat_hooks[0]); ++i) {
      installed += hook_one_import(nt, &g_textdraw_render_iat_hooks[i]);
    }
  }

  if (log_summary) {
    probe_log("hook: summary installed=%d requested=%u asset_path_hooks=%d asset_read_hooks=%d textdraw_render=%d",
              installed, (unsigned)(sizeof(g_hooks) / sizeof(g_hooks[0])), asset_path_hooks_enabled(),
              asset_read_hooks_enabled(), textdraw_render_enabled());
  }
  return installed;
}

static PIMAGE_NT_HEADERS get_samp_nt_headers(void) {
  PIMAGE_DOS_HEADER dos;
  PIMAGE_NT_HEADERS nt;

  if (g_samp_module == NULL) {
    return NULL;
  }

  dos = (PIMAGE_DOS_HEADER)g_samp_module;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return NULL;
  }

  nt = (PIMAGE_NT_HEADERS)((BYTE *)g_samp_module + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    return NULL;
  }

  return nt;
}

static void resolve_helper_exports(void) {
  HMODULE wsock = GetModuleHandleA("WSOCK32.dll");
  if (wsock == NULL) {
    wsock = GetModuleHandleA("wsock32.dll");
  }
  if (wsock != NULL) {
    g_WSAGetLastError_ptr = (probe_WSAGetLastError_fn)GetProcAddress(wsock, "WSAGetLastError");
  }
}

static DWORD WINAPI probe_worker(LPVOID param) {
  DWORD start_ms;
  PIMAGE_NT_HEADERS nt;
  int hooks_disabled;
  (void)param;

  init_log_path();
  probe_log("probe: attached build=%s %s", __DATE__, __TIME__);
  probe_log("probe: options asset_path_hooks=%d asset_read_hooks=%d samp_code_hooks=%d gta_asset_hooks=%d "
            "object_info=%d custom_object_heavy=%d textdraw_hooks=%d textdraw_verbose=%d textdraw_render=%d "
            "font5_hooks=%d",
            asset_path_hooks_enabled(), asset_read_hooks_enabled(), samp_code_hooks_enabled(), gta_asset_hooks_enabled(),
            object_info_enabled(), custom_object_heavy_enabled(), textdraw_hooks_enabled(), textdraw_verbose_enabled(),
            textdraw_render_enabled(), font5_hooks_enabled());
  if (custom_object_heavy_enabled()) {
    probe_log("custom_object_heavy: store_addresses model_info_ptrs=0x%08lx atomic_count=0x%08lx "
              "time_count=0x%08lx clump_count=0x%08lx low_range=%u-%u high_range=%u-%u "
              "evidence=OBSERVED_037,PROBE_TRACE,TODO_VERIFY",
              (unsigned long)PROBE_GTA_ADDR_MODEL_INFO_PTRS,
              (unsigned long)PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT,
              (unsigned long)PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT,
              (unsigned long)PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT,
              (unsigned)PROBE_GTA_CUSTOM_LOW_MODEL_MIN, (unsigned)PROBE_GTA_CUSTOM_LOW_MODEL_MAX,
              (unsigned)PROBE_GTA_CUSTOM_MODEL_MIN, (unsigned)PROBE_GTA_CUSTOM_MODEL_MAX);
  }

  start_ms = GetTickCount();
  while (WaitForSingleObject(g_stop_event, 50) == WAIT_TIMEOUT) {
    g_samp_module = GetModuleHandleA("samp.dll");
    if (g_samp_module != NULL) {
      break;
    }
    if ((DWORD)(GetTickCount() - start_ms) > PROBE_WAIT_FOR_SAMP_MS) {
      probe_log("probe: timed out waiting for samp.dll");
      return 0;
    }
  }

  if (g_samp_module == NULL) {
    return 0;
  }

  nt = get_samp_nt_headers();
  if (nt == NULL) {
    probe_log("samp: invalid PE headers module=%p", (void *)g_samp_module);
    return 0;
  }

  g_samp_base = (uintptr_t)g_samp_module;
  g_samp_size = nt->OptionalHeader.SizeOfImage;
  probe_log("samp: loaded base=0x%08lx size=0x%08lx entry=0x%08lx image_base=0x%08lx sections=%u imports_rva=0x%08lx",
            (unsigned long)g_samp_base, (unsigned long)g_samp_size, (unsigned long)nt->OptionalHeader.AddressOfEntryPoint,
            (unsigned long)nt->OptionalHeader.ImageBase, (unsigned)nt->FileHeader.NumberOfSections,
            (unsigned long)nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

  dump_samp_sections(nt);
  dump_samp_imports(nt);
  resolve_helper_exports();

  hooks_disabled = hooks_disabled_by_flag();
  if (hooks_disabled) {
    probe_log("hook: disabled by flag/env");
  } else {
    install_iat_hooks(nt, 1);
    install_inline_hooks(1);
    install_samp_code_hooks(1);
    install_samp_font5_code_hooks(1);
    install_gta_textdraw_code_hooks(1);
    install_gta_asset_code_hooks(1);
    install_d3d_device_hooks(1);
  }

  sample_transition_state("after_samp_load");

  while (WaitForSingleObject(g_stop_event, PROBE_WATCH_INTERVAL_MS) == WAIT_TIMEOUT) {
    if (!hooks_disabled) {
      (void)install_iat_hooks(nt, 0);
      (void)install_inline_hooks(0);
      (void)install_samp_code_hooks(0);
      (void)install_samp_font5_code_hooks(0);
      (void)install_gta_textdraw_code_hooks(0);
      (void)install_gta_asset_code_hooks(0);
      (void)install_d3d_device_hooks(0);
    }
    sample_watchpoints();
    sample_transition_state("tick");
  }

  probe_log("probe: stopping");
  return 0;
}

static int WINAPI hook_samp_socketlayer_sendto(SOCKET s, const char *buf, int len, unsigned long binary_address,
                                               unsigned short port) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  const unsigned char *bytes = (const unsigned char *)buf;
  const char *name = "";
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  int rc;

  if (bytes != NULL && len > 0) {
    name = samp_packet_name(bytes[0]);
  }
  if (should_log_call(n)) {
    format_binary_address(binary_address, port, addr, sizeof(addr));
    payload_to_hex(buf, len, payload, sizeof(payload));
    probe_log("samp_internal: SocketLayer.SendTo.begin count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d to=%s "
              "plain_id=0x%02x plain_name=%s data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, addr,
              (bytes != NULL && len > 0) ? (unsigned)bytes[0] : 0u, name, payload);
  }

  if (g_orig_samp_socketlayer_sendto == NULL) {
    return -1;
  }

  rc = ((probe_samp_socketlayer_sendto_fn)g_orig_samp_socketlayer_sendto)(s, buf, len, binary_address, port);
  if (should_log_call(n)) {
    probe_log("samp_internal: SocketLayer.SendTo.end count=%ld rc=%d err=%d", (long)n, rc, last_wsa_error());
  }
  return rc;
}

static void WINAPI hook_samp_process_network_packet(unsigned long binary_address, unsigned short port, const char *data, int len,
                                                    void *rak_peer) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  const unsigned char *bytes = (const unsigned char *)data;
  const char *name = "";
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);

  if (bytes != NULL && len > 0) {
    name = samp_packet_name(bytes[0]);
  }
  if (should_log_call(n)) {
    format_binary_address(binary_address, port, addr, sizeof(addr));
    payload_to_hex(data, len, payload, sizeof(payload));
    probe_log("samp_internal: ProcessNetworkPacket count=%ld caller=%p caller_rva=0x%08lx from=%s len=%d rak_peer=%p "
              "plain_id=0x%02x plain_name=%s data=%s",
              (long)n, caller, samp_rva_from_address(caller), addr, len, rak_peer,
              (bytes != NULL && len > 0) ? (unsigned)bytes[0] : 0u, name, payload);
  }

  if (g_orig_samp_process_network_packet != NULL) {
    ((probe_samp_process_network_packet_fn)g_orig_samp_process_network_packet)(binary_address, port, data, len, rak_peer);
  }
}

static void *font5_preview_pointer_for(void *textdraw, LONG *slot_out) {
  LONG slot = -1;
  uintptr_t array_address;
  DWORD value;

  if (slot_out != NULL) {
    *slot_out = -1;
  }
  if (textdraw == NULL || !memory_is_readable((uintptr_t)textdraw + 0x9a3u, sizeof(DWORD))) {
    return NULL;
  }

  slot = (LONG)read_u32_or((uintptr_t)textdraw + 0x9a3u, 0xffffffffu);
  if (slot_out != NULL) {
    *slot_out = slot;
  }
  if (slot < 0 || slot >= 512 || g_samp_base == 0) {
    return NULL;
  }

  array_address = g_samp_base + 0x0026b568u + ((uintptr_t)(unsigned)slot * sizeof(DWORD));
  if (!memory_is_readable(array_address, sizeof(DWORD))) {
    return NULL;
  }
  value = read_u32_or(array_address, 0u);
  return (void *)(uintptr_t)value;
}

static void log_font5_snapshot(const char *phase, LONG seq, void *caller, void *textdraw, int include_raw) {
  uintptr_t base = (uintptr_t)textdraw;
  LONG slot = -1;
  void *preview = font5_preview_pointer_for(textdraw, &slot);
  char text[128];
  char raw[256];
  char stack_rvas[448];

  if (textdraw == NULL || !memory_is_readable(base, 0x9c0u)) {
    probe_log("font5_dispatch: phase=%s seq=%ld caller=%p caller_samp_rva=0x%08lx textdraw=%p readable=0 "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              phase != NULL ? phase : "unknown", (long)seq, caller, samp_rva_from_address(caller), textdraw);
    return;
  }

  safe_cstr_or((const char *)textdraw, text, sizeof(text), "");
  raw[0] = '\0';
  if (include_raw) {
    probe_bytes_hex_or(base + 0x963u, 0x5du, raw, sizeof(raw));
  }
  format_font5_stack(stack_rvas, sizeof(stack_rvas));
  probe_log("font5_dispatch: phase=%s seq=%ld caller=%p caller_samp_rva=0x%08lx textdraw=%p readable=1 "
            "style=%lu model=%d slot=%ld preview=%p text='%s' pos=%.6f,%.6f size=%.6f,%.6f "
            "rot=%.6f,%.6f,%.6f zoom=%.6f colors=%d,%d background=0x%08lx raw=%s stack_samp='%s' "
            "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
            phase != NULL ? phase : "unknown", (long)seq, caller, samp_rva_from_address(caller), textdraw,
            (unsigned long)read_u32_or(base + 0x987u, 0xffffffffu),
            (int)(int16_t)read_u16_or(base + 0x9a8u, 0xffffu), (long)slot, preview, text,
            read_f32_or(base + 0x98bu, -1.0f), read_f32_or(base + 0x98fu, -1.0f),
            read_f32_or(base + 0x972u, -1.0f), read_f32_or(base + 0x976u, -1.0f),
            read_f32_or(base + 0x9aau, -1.0f), read_f32_or(base + 0x9aeu, -1.0f),
            read_f32_or(base + 0x9b2u, -1.0f), read_f32_or(base + 0x9b6u, -1.0f),
            (int)(int16_t)read_u16_or(base + 0x9bau, 0xffffu),
            (int)(int16_t)read_u16_or(base + 0x9bcu, 0xffffu),
            (unsigned long)read_u32_or(base + 0x97fu, 0xffffffffu), raw, stack_rvas);
}

static void PROBE_THISCALL hook_samp_font5_prepare(void *textdraw) {
  void *caller = probe_return_address();
  DWORD style = textdraw != NULL ? read_u32_or((uintptr_t)textdraw + 0x987u, 0xffffffffu) : 0xffffffffu;
  LONG seq;
  LONG previous_seq = g_font5_active_seq;

  if (style != 5u) {
    if (g_orig_samp_font5_prepare != NULL) {
      ((probe_samp_font5_method_fn)g_orig_samp_font5_prepare)(textdraw);
    }
    return;
  }
  seq = next_call_count(&g_font5_trace_call_count);
  log_font5_snapshot("prepare.begin", seq, caller, textdraw, 1);
  ++g_font5_trace_depth;
  g_font5_active_seq = seq;
  if (g_orig_samp_font5_prepare != NULL) {
    ((probe_samp_font5_method_fn)g_orig_samp_font5_prepare)(textdraw);
  }
  g_font5_active_seq = previous_seq;
  --g_font5_trace_depth;
  log_font5_snapshot("prepare.end", seq, caller, textdraw, 1);
}

static void PROBE_THISCALL hook_samp_font5_draw_dispatch(void *textdraw) {
  void *caller = probe_return_address();
  LONG seq;
  LONG previous_seq = g_font5_active_seq;
  DWORD style = textdraw != NULL ? read_u32_or((uintptr_t)textdraw + 0x987u, 0xffffffffu) : 0xffffffffu;
  LONG slot = -1;

  if (style != 5u) {
    if (g_orig_samp_font5_draw_dispatch != NULL) {
      ((probe_samp_font5_method_fn)g_orig_samp_font5_draw_dispatch)(textdraw);
    }
    return;
  }
  seq = next_call_count(&g_font5_trace_call_count);
  (void)font5_preview_pointer_for(textdraw, &slot);
  log_font5_snapshot("draw.begin", seq, caller, textdraw, 0);
  ++g_font5_trace_depth;
  g_font5_active_seq = seq;
  if (g_orig_samp_font5_draw_dispatch != NULL) {
    ((probe_samp_font5_method_fn)g_orig_samp_font5_draw_dispatch)(textdraw);
  }
  g_font5_active_seq = previous_seq;
  --g_font5_trace_depth;
  log_font5_snapshot(slot >= 0 ? "draw.preview.end" : "draw.font.end", seq, caller, textdraw, 0);
}

static int should_log_textdraw_font_call(void *caller, LONG count) {
  if (!address_in_samp(caller) && !env_flag_enabled("SAMP_PROBE_TEXTDRAW_LOG_ALL_FONT")) {
    return 0;
  }
  if (textdraw_verbose_enabled()) {
    return 1;
  }
  return should_log_call(count);
}

static void log_textdraw_font_call(void *caller, LONG count, const char *name, const char *detail) {
  if (!should_log_textdraw_font_call(caller, count)) {
    return;
  }
  probe_log("textdraw_font: seq=%ld caller=%p caller_samp_rva=0x%08lx name=%s %s",
            (long)count, caller, samp_rva_from_address(caller), name != NULL ? name : "unknown",
            detail != NULL ? detail : "");
}

static void __cdecl hook_gta_font_set_scale(float x, float y) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[96];

  snprintf(detail, sizeof(detail), "x=%.6f y=%.6f", x, y);
  log_textdraw_font_call(caller, n, "CFont.SetScale", detail);
  if (g_orig_gta_font_set_scale != NULL) {
    ((probe_gta_font_set_scale_fn)g_orig_gta_font_set_scale)(x, y);
  }
}

static void __cdecl hook_gta_font_set_color(DWORD color) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "color=0x%08lx", (unsigned long)color);
  log_textdraw_font_call(caller, n, "CFont.SetColor", detail);
  if (g_orig_gta_font_set_color != NULL) {
    ((probe_gta_font_set_color_fn)g_orig_gta_font_set_color)(color);
  }
}

static void __cdecl hook_gta_font_set_style(int style) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  InterlockedExchange(&g_textdraw_current_font_style, (LONG)style);
  if (style == 5 && !font5_code_hooks_active()) {
    textdraw_render_mark_hint("font5_style");
  }
  snprintf(detail, sizeof(detail), "style=%d", style);
  log_textdraw_font_call(caller, n, "CFont.SetFontStyle", detail);
  if (g_orig_gta_font_set_style != NULL) {
    ((probe_gta_font_set_int_fn)g_orig_gta_font_set_style)(style);
  }
}

static void __cdecl hook_gta_font_set_line_width(float width) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "width=%.6f", width);
  log_textdraw_font_call(caller, n, "CFont.SetLineWidth", detail);
  if (g_orig_gta_font_set_line_width != NULL) {
    ((probe_gta_font_set_float_fn)g_orig_gta_font_set_line_width)(width);
  }
}

static void __cdecl hook_gta_font_set_line_height(float height) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "height=%.6f", height);
  log_textdraw_font_call(caller, n, "CFont.SetLineHeight", detail);
  if (g_orig_gta_font_set_line_height != NULL) {
    ((probe_gta_font_set_float_fn)g_orig_gta_font_set_line_height)(height);
  }
}

static void __cdecl hook_gta_font_set_drop_color(DWORD color) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "color=0x%08lx", (unsigned long)color);
  log_textdraw_font_call(caller, n, "CFont.SetDropColor", detail);
  if (g_orig_gta_font_set_drop_color != NULL) {
    ((probe_gta_font_set_color_fn)g_orig_gta_font_set_drop_color)(color);
  }
}

static void __cdecl hook_gta_font_set_shadow(int shadow) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "shadow=%d", shadow);
  log_textdraw_font_call(caller, n, "CFont.SetShadow", detail);
  if (g_orig_gta_font_set_shadow != NULL) {
    ((probe_gta_font_set_int_fn)g_orig_gta_font_set_shadow)(shadow);
  }
}

static void __cdecl hook_gta_font_set_outline(int outline) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "outline=%d", outline);
  log_textdraw_font_call(caller, n, "CFont.SetOutline", detail);
  if (g_orig_gta_font_set_outline != NULL) {
    ((probe_gta_font_set_int_fn)g_orig_gta_font_set_outline)(outline);
  }
}

static void __cdecl hook_gta_font_set_proportional(int proportional) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "proportional=%d", proportional);
  log_textdraw_font_call(caller, n, "CFont.SetProportional", detail);
  if (g_orig_gta_font_set_proportional != NULL) {
    ((probe_gta_font_set_int_fn)g_orig_gta_font_set_proportional)(proportional);
  }
}

static void __cdecl hook_gta_font_use_box(int use, int unk) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[80];

  snprintf(detail, sizeof(detail), "use=%d unk=%d", use, unk);
  log_textdraw_font_call(caller, n, "CFont.UseBox", detail);
  if (g_orig_gta_font_use_box != NULL) {
    ((probe_gta_font_use_box_fn)g_orig_gta_font_use_box)(use, unk);
  }
}

static void __cdecl hook_gta_font_use_box_color(DWORD color) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "color=0x%08lx", (unsigned long)color);
  log_textdraw_font_call(caller, n, "CFont.UseBoxColor", detail);
  if (g_orig_gta_font_use_box_color != NULL) {
    ((probe_gta_font_set_color_fn)g_orig_gta_font_use_box_color)(color);
  }
}

static void __cdecl hook_gta_font_unk12(int value) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "value=%d", value);
  log_textdraw_font_call(caller, n, "CFont.UnknownState", detail);
  if (g_orig_gta_font_unk12 != NULL) {
    ((probe_gta_font_set_int_fn)g_orig_gta_font_unk12)(value);
  }
}

static void __cdecl hook_gta_font_set_justify(int justify) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  char detail[64];

  snprintf(detail, sizeof(detail), "justify=%d", justify);
  log_textdraw_font_call(caller, n, "CFont.SetJustify", detail);
  if (g_orig_gta_font_set_justify != NULL) {
    ((probe_gta_font_set_int_fn)g_orig_gta_font_set_justify)(justify);
  }
}

static void __cdecl hook_gta_font_print_string(float x, float y, char *text) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_font_call_count);
  LONG style = InterlockedCompareExchange(&g_textdraw_current_font_style, 0, 0);
  char safe_text[192];
  char detail[320];

  safe_cstr_or(text, safe_text, sizeof(safe_text), "(null)");
  snprintf(detail, sizeof(detail), "x=%.6f y=%.6f style_context=%ld text='%s'", x, y, (long)style, safe_text);
  if (style == 5 && strcmp(safe_text, "_") == 0 && !font5_code_hooks_active()) {
    textdraw_render_mark_hint("font5_preview_underscore");
  } else if (ascii_contains_ci(safe_text, "LD_")) {
    textdraw_render_mark_hint("font4_sprite_text");
  }
  log_textdraw_font_call(caller, n, "CFont.PrintString", detail);
  if (g_orig_gta_font_print_string != NULL) {
    ((probe_gta_font_print_string_fn)g_orig_gta_font_print_string)(x, y, text);
  }
}

static HRESULT WINAPI hook_D3DXCreateSprite(void *device, void **sprite) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;

  if (g_orig_D3DXCreateSprite != NULL) {
    rc = ((probe_D3DXCreateSprite_fn)g_orig_D3DXCreateSprite)(device, sprite);
  }
  if (should_log_textdraw_render_call(caller, n, NULL) || should_log_call(n)) {
    probe_log("textdraw_render: D3DXCreateSprite count=%ld caller=%p caller_samp_rva=0x%08lx device=%p sprite=%p "
              "result=0x%08lx evidence=PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), device, sprite != NULL ? *sprite : NULL,
              (unsigned long)rc);
  }
  if (rc >= 0 && sprite != NULL && *sprite != NULL) {
    install_d3dx_sprite_hooks(*sprite);
  }
  return rc;
}

static HRESULT WINAPI hook_D3DXCreateTexture(void *device, UINT width, UINT height, UINT mip_levels, DWORD usage,
                                             int format, int pool, void **texture) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char source[192];
  char stack_rvas[448];

  if (g_orig_D3DXCreateTexture != NULL) {
    rc = ((probe_D3DXCreateTexture_fn)g_orig_D3DXCreateTexture)(device, width, height, mip_levels, usage, format,
                                                               pool, texture);
  }
  snprintf(source, sizeof(source), "D3DXCreateTexture:%ux%u:usage=0x%08lx:fmt=%d:pool=%d",
           (unsigned)width, (unsigned)height, (unsigned long)usage, format, pool);
  if (rc >= 0 && texture != NULL && *texture != NULL) {
    remember_texture_details(*texture, source, width, height, usage);
    install_d3d_texture_hooks(*texture);
  }
  if ((usage & PROBE_D3DUSAGE_RENDERTARGET) != 0 || should_log_textdraw_render_call(caller, n, source) ||
      should_log_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: D3DXCreateTexture count=%ld font5_seq=%ld caller=%p caller_samp_rva=0x%08lx "
              "device=%p size=%ux%u mip=%u usage=0x%08lx fmt=%d pool=%d texture=%p result=0x%08lx "
              "stack_samp='%s' evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device, (unsigned)width,
              (unsigned)height, (unsigned)mip_levels, (unsigned long)usage, format, pool,
              rc >= 0 && texture != NULL ? *texture : NULL, (unsigned long)rc, stack_rvas);
  }
  return rc;
}

static HRESULT WINAPI hook_D3DXCreateTextureFromFileA(void *device, LPCSTR src_file, void **texture) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char source[MAX_PATH];

  safe_cstr_or(src_file, source, sizeof(source), "(null)");
  if (g_orig_D3DXCreateTextureFromFileA != NULL) {
    rc = ((probe_D3DXCreateTextureFromFileA_fn)g_orig_D3DXCreateTextureFromFileA)(device, src_file, texture);
  }
  if (rc >= 0 && texture != NULL && *texture != NULL) {
    remember_texture_source(*texture, source);
  }
  if (should_log_textdraw_render_call(caller, n, source) || should_log_call(n)) {
    probe_log("textdraw_render: D3DXCreateTextureFromFileA count=%ld caller=%p caller_samp_rva=0x%08lx device=%p "
              "file='%s' texture=%p result=0x%08lx evidence=PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), device, source, texture != NULL ? *texture : NULL,
              (unsigned long)rc);
  }
  return rc;
}

static HRESULT WINAPI hook_D3DXCreateTextureFromFileExA(void *device, LPCSTR src_file, UINT width, UINT height,
                                                       UINT mip_levels, DWORD usage, int format, int pool,
                                                       DWORD filter, DWORD mip_filter, DWORD color_key,
                                                       void *src_info, void *palette, void **texture) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char source[MAX_PATH];

  safe_cstr_or(src_file, source, sizeof(source), "(null)");
  if (g_orig_D3DXCreateTextureFromFileExA != NULL) {
    rc = ((probe_D3DXCreateTextureFromFileExA_fn)g_orig_D3DXCreateTextureFromFileExA)(
        device, src_file, width, height, mip_levels, usage, format, pool, filter, mip_filter, color_key, src_info,
        palette, texture);
  }
  if (rc >= 0 && texture != NULL && *texture != NULL) {
    remember_texture_source(*texture, source);
  }
  if (should_log_textdraw_render_call(caller, n, source) || should_log_call(n)) {
    probe_log("textdraw_render: D3DXCreateTextureFromFileExA count=%ld caller=%p caller_samp_rva=0x%08lx device=%p "
              "file='%s' size=%ux%u mip=%u usage=0x%08lx fmt=%d pool=%d texture=%p result=0x%08lx "
              "evidence=PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), device, source, (unsigned)width, (unsigned)height,
              (unsigned)mip_levels, (unsigned long)usage, format, pool, texture != NULL ? *texture : NULL,
              (unsigned long)rc);
  }
  return rc;
}

static HRESULT WINAPI hook_D3DXCreateTextureFromFileInMemory(void *device, LPCVOID src_data, UINT src_data_size,
                                                            void **texture) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char source[64];

  snprintf(source, sizeof(source), "memory:%lu", (unsigned long)src_data_size);
  if (g_orig_D3DXCreateTextureFromFileInMemory != NULL) {
    rc = ((probe_D3DXCreateTextureFromFileInMemory_fn)g_orig_D3DXCreateTextureFromFileInMemory)(device, src_data,
                                                                                                src_data_size, texture);
  }
  if (rc >= 0 && texture != NULL && *texture != NULL) {
    remember_texture_source(*texture, source);
  }
  if (should_log_textdraw_render_call(caller, n, source) || should_log_call(n)) {
    probe_log("textdraw_render: D3DXCreateTextureFromFileInMemory count=%ld caller=%p caller_samp_rva=0x%08lx "
              "device=%p data=%p size=%lu texture=%p result=0x%08lx evidence=PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), device, src_data, (unsigned long)src_data_size,
              texture != NULL ? *texture : NULL, (unsigned long)rc);
  }
  return rc;
}

static HRESULT WINAPI hook_D3DXCreateTextureFromResourceExA(void *device, HMODULE src_module, LPCSTR src_resource,
                                                           UINT width, UINT height, UINT mip_levels, DWORD usage,
                                                           int format, int pool, DWORD filter, DWORD mip_filter,
                                                           DWORD color_key, void *src_info, void *palette,
                                                           void **texture) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char resource[128];
  char source[192];

  safe_resource_name_or(src_resource, resource, sizeof(resource), "(resource)");
  snprintf(source, sizeof(source), "resource:%p:%s", (void *)src_module, resource);
  if (g_orig_D3DXCreateTextureFromResourceExA != NULL) {
    rc = ((probe_D3DXCreateTextureFromResourceExA_fn)g_orig_D3DXCreateTextureFromResourceExA)(
        device, src_module, src_resource, width, height, mip_levels, usage, format, pool, filter, mip_filter, color_key,
        src_info, palette, texture);
  }
  if (rc >= 0 && texture != NULL && *texture != NULL) {
    remember_texture_source(*texture, source);
  }
  if (should_log_textdraw_render_call(caller, n, source) || should_log_call(n)) {
    probe_log("textdraw_render: D3DXCreateTextureFromResourceExA count=%ld caller=%p caller_samp_rva=0x%08lx "
              "device=%p module=%p resource='%s' size=%ux%u texture=%p result=0x%08lx "
              "evidence=PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), device, (void *)src_module, resource, (unsigned)width,
              (unsigned)height, texture != NULL ? *texture : NULL, (unsigned long)rc);
  }
  return rc;
}

static HRESULT WINAPI hook_d3dx_sprite_begin(void *sprite, DWORD flags) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;

  if (should_log_textdraw_render_call(caller, n, NULL)) {
    probe_log("textdraw_render: ID3DXSprite.Begin seq=%ld caller=%p caller_samp_rva=0x%08lx sprite=%p flags=0x%08lx",
              (long)n, caller, samp_rva_from_address(caller), sprite, (unsigned long)flags);
  }
  if (g_orig_d3dx_sprite_begin != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3dx_sprite_begin_fn)g_orig_d3dx_sprite_begin)(sprite, flags);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3dx_sprite_draw(void *sprite, void *texture, const RECT *src_rect,
                                           const probe_d3dx_vec3 *center, const probe_d3dx_vec3 *position,
                                           DWORD color) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  LONG draw_count = 0;
  HRESULT rc = E_FAIL;
  char source[192];

  (void)texture_source_for(texture, source, sizeof(source), &draw_count);
  if (should_log_textdraw_render_call(caller, n, source)) {
    probe_log("textdraw_render: ID3DXSprite.Draw seq=%ld caller=%p caller_samp_rva=0x%08lx sprite=%p texture=%p "
              "texture_draw=%ld source='%s' rect=%ld,%ld,%ld,%ld center=%.3f,%.3f,%.3f pos=%.3f,%.3f,%.3f "
              "color=0x%08lx evidence=PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), sprite, texture, (long)draw_count, source,
              src_rect != NULL ? (long)src_rect->left : 0l, src_rect != NULL ? (long)src_rect->top : 0l,
              src_rect != NULL ? (long)src_rect->right : 0l, src_rect != NULL ? (long)src_rect->bottom : 0l,
              center != NULL ? center->x : 0.0f, center != NULL ? center->y : 0.0f, center != NULL ? center->z : 0.0f,
              position != NULL ? position->x : 0.0f, position != NULL ? position->y : 0.0f,
              position != NULL ? position->z : 0.0f, (unsigned long)color);
  }
  if (g_orig_d3dx_sprite_draw != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3dx_sprite_draw_fn)g_orig_d3dx_sprite_draw)(sprite, texture, src_rect, center, position, color);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3dx_sprite_end(void *sprite) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;

  if (should_log_textdraw_render_call(caller, n, NULL)) {
    probe_log("textdraw_render: ID3DXSprite.End seq=%ld caller=%p caller_samp_rva=0x%08lx sprite=%p", (long)n,
              caller, samp_rva_from_address(caller), sprite);
  }
  if (g_orig_d3dx_sprite_end != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3dx_sprite_end_fn)g_orig_d3dx_sprite_end)(sprite);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static void describe_surface_texture(void *surface, void **texture_out, char *source, size_t source_size) {
  if (!texture_for_surface(surface, texture_out, source, source_size)) {
    if (texture_out != NULL) {
      *texture_out = NULL;
    }
    if (source != NULL && source_size > 0) {
      source[0] = '\0';
    }
  }
}

static HRESULT WINAPI hook_d3d_create_texture(void *device, UINT width, UINT height, UINT levels, DWORD usage,
                                              int format, int pool, void **texture, HANDLE *shared_handle) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char source[192];
  char stack_rvas[448];

  if (g_orig_d3d_create_texture != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_create_texture_fn)g_orig_d3d_create_texture)(device, width, height, levels, usage, format, pool,
                                                                 texture, shared_handle);
    --g_textdraw_render_hook_depth;
  }
  snprintf(source, sizeof(source), "CreateTexture:%ux%u:usage=0x%08lx:fmt=%d:pool=%d", (unsigned)width,
           (unsigned)height, (unsigned long)usage, format, pool);
  if (rc >= 0 && texture != NULL && *texture != NULL &&
      ((usage & PROBE_D3DUSAGE_RENDERTARGET) != 0 || g_font5_trace_depth > 0)) {
    remember_texture_details(*texture, source, width, height, usage);
    install_d3d_texture_hooks(*texture);
  }
  if ((usage & PROBE_D3DUSAGE_RENDERTARGET) != 0 || should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.CreateTexture seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p size=%ux%u levels=%u usage=0x%08lx fmt=%d pool=%d "
              "texture=%p shared=%p result=0x%08lx stack_samp='%s' "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device, (unsigned)width,
              (unsigned)height, (unsigned)levels, (unsigned long)usage, format, pool,
              rc >= 0 && texture != NULL ? *texture : NULL,
              rc >= 0 && shared_handle != NULL ? *shared_handle : NULL,
              (unsigned long)rc, stack_rvas);
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_create_render_target(void *device, UINT width, UINT height, int format,
                                                    int multi_sample, DWORD multi_sample_quality, BOOL lockable,
                                                    void **surface, HANDLE *shared_handle) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char stack_rvas[448];

  if (g_orig_d3d_create_render_target != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_create_render_target_fn)g_orig_d3d_create_render_target)(
        device, width, height, format, multi_sample, multi_sample_quality, lockable, surface, shared_handle);
    --g_textdraw_render_hook_depth;
  }
  if (should_log_font5_rtt_call(n) || should_log_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.CreateRenderTarget seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p size=%ux%u fmt=%d multisample=%d quality=%lu lockable=%d "
              "surface=%p shared=%p result=0x%08lx stack_samp='%s' "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device, (unsigned)width,
              (unsigned)height, format, multi_sample, (unsigned long)multi_sample_quality, lockable != FALSE,
              rc >= 0 && surface != NULL ? *surface : NULL,
              rc >= 0 && shared_handle != NULL ? *shared_handle : NULL,
              (unsigned long)rc, stack_rvas);
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_create_depth_stencil_surface(void *device, UINT width, UINT height, int format,
                                                            int multi_sample, DWORD multi_sample_quality,
                                                            BOOL discard, void **surface, HANDLE *shared_handle) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char stack_rvas[448];

  if (g_orig_d3d_create_depth_stencil_surface != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_create_depth_stencil_surface_fn)g_orig_d3d_create_depth_stencil_surface)(
        device, width, height, format, multi_sample, multi_sample_quality, discard, surface, shared_handle);
    --g_textdraw_render_hook_depth;
  }
  if (should_log_font5_rtt_call(n) || should_log_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.CreateDepthStencilSurface seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p size=%ux%u fmt=%d multisample=%d quality=%lu discard=%d "
              "surface=%p shared=%p result=0x%08lx stack_samp='%s' "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device, (unsigned)width,
              (unsigned)height, format, multi_sample, (unsigned long)multi_sample_quality, discard != FALSE,
              rc >= 0 && surface != NULL ? *surface : NULL,
              rc >= 0 && shared_handle != NULL ? *shared_handle : NULL,
              (unsigned long)rc, stack_rvas);
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_stretch_rect(void *device, void *source_surface, const RECT *source_rect,
                                            void *dest_surface, const RECT *dest_rect, int filter) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  void *source_texture = NULL;
  void *dest_texture = NULL;
  char source_name[192];
  char dest_name[192];
  char stack_rvas[448];

  describe_surface_texture(source_surface, &source_texture, source_name, sizeof(source_name));
  describe_surface_texture(dest_surface, &dest_texture, dest_name, sizeof(dest_name));
  if (should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.StretchRect.begin seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p source_surface=%p source_texture=%p source='%s' "
              "source_rect=%p dest_surface=%p dest_texture=%p dest='%s' dest_rect=%p filter=%d stack_samp='%s' "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device, source_surface,
              source_texture, source_name, source_rect, dest_surface, dest_texture, dest_name, dest_rect, filter,
              stack_rvas);
  }
  if (g_orig_d3d_stretch_rect != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_stretch_rect_fn)g_orig_d3d_stretch_rect)(device, source_surface, source_rect, dest_surface,
                                                             dest_rect, filter);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_set_render_target(void *device, DWORD index, void *surface) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  void *texture = NULL;
  void *previous = index == 0 ? g_d3d_current_render_target : NULL;
  char source[192];
  char stack_rvas[448];

  describe_surface_texture(surface, &texture, source, sizeof(source));
  if (should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.SetRenderTarget.begin seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p index=%lu previous=%p surface=%p texture=%p source='%s' "
              "stack_samp='%s' evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device,
              (unsigned long)index, previous, surface, texture, source, stack_rvas);
  }
  if (g_orig_d3d_set_render_target != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_set_render_target_fn)g_orig_d3d_set_render_target)(device, index, surface);
    --g_textdraw_render_hook_depth;
  }
  if (rc >= 0 && index == 0) {
    g_d3d_current_render_target = surface;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_get_render_target(void *device, DWORD index, void **surface) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  void *texture = NULL;
  char source[192];
  char stack_rvas[448];

  if (g_orig_d3d_get_render_target != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_get_render_target_fn)g_orig_d3d_get_render_target)(device, index, surface);
    --g_textdraw_render_hook_depth;
  }
  if (rc >= 0 && surface != NULL && index == 0) {
    g_d3d_current_render_target = *surface;
  }
  describe_surface_texture(rc >= 0 && surface != NULL ? *surface : NULL, &texture, source, sizeof(source));
  if (should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.GetRenderTarget seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p index=%lu surface=%p texture=%p source='%s' result=0x%08lx "
              "stack_samp='%s' evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device,
              (unsigned long)index, rc >= 0 && surface != NULL ? *surface : NULL, texture, source, (unsigned long)rc,
              stack_rvas);
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_set_depth_stencil_surface(void *device, void *surface) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char stack_rvas[448];

  if (should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.SetDepthStencilSurface.begin seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p previous=%p surface=%p stack_samp='%s' "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device,
              g_d3d_current_depth_stencil, surface, stack_rvas);
  }
  if (g_orig_d3d_set_depth_stencil_surface != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_set_depth_stencil_surface_fn)g_orig_d3d_set_depth_stencil_surface)(device, surface);
    --g_textdraw_render_hook_depth;
  }
  if (rc >= 0) {
    g_d3d_current_depth_stencil = surface;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_get_depth_stencil_surface(void *device, void **surface) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char stack_rvas[448];

  if (g_orig_d3d_get_depth_stencil_surface != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_get_depth_stencil_surface_fn)g_orig_d3d_get_depth_stencil_surface)(device, surface);
    --g_textdraw_render_hook_depth;
  }
  if (rc >= 0 && surface != NULL) {
    g_d3d_current_depth_stencil = *surface;
  }
  if (should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.GetDepthStencilSurface seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p surface=%p result=0x%08lx stack_samp='%s' "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device,
              rc >= 0 && surface != NULL ? *surface : NULL, (unsigned long)rc, stack_rvas);
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_clear(void *device, DWORD count, const void *rects, DWORD flags, DWORD color,
                                     float z, DWORD stencil) {
  static LONG call_count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&call_count);
  HRESULT rc = E_FAIL;
  char stack_rvas[448];

  if (should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.Clear.begin seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p target=%p depth=%p count=%lu rects=%p flags=0x%08lx "
              "color=0x%08lx z=%.6f stencil=%lu stack_samp='%s' "
              "evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device,
              g_d3d_current_render_target, g_d3d_current_depth_stencil, (unsigned long)count, rects,
              (unsigned long)flags, (unsigned long)color, z, (unsigned long)stencil, stack_rvas);
  }
  if (g_orig_d3d_clear != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_clear_fn)g_orig_d3d_clear)(device, count, rects, flags, color, z, stencil);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_set_viewport(void *device, const probe_d3d_viewport *viewport) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char stack_rvas[448];
  int readable = viewport != NULL && memory_is_readable((uintptr_t)viewport, sizeof(*viewport));

  if (should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DDevice9.SetViewport.begin seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx device=%p viewport=%p readable=%d xy=%lu,%lu size=%lux%lu z=%.6f,%.6f "
              "target=%p stack_samp='%s' evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), device, viewport, readable,
              readable ? (unsigned long)viewport->x : 0ul, readable ? (unsigned long)viewport->y : 0ul,
              readable ? (unsigned long)viewport->width : 0ul, readable ? (unsigned long)viewport->height : 0ul,
              readable ? viewport->min_z : 0.0f, readable ? viewport->max_z : 0.0f,
              g_d3d_current_render_target, stack_rvas);
  }
  if (g_orig_d3d_set_viewport != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_set_viewport_fn)g_orig_d3d_set_viewport)(device, viewport);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_texture_get_surface_level(void *texture, UINT level, void **surface) {
  static LONG count;
  void *caller = probe_return_address();
  LONG n = next_call_count(&count);
  HRESULT rc = E_FAIL;
  char source[192];
  char stack_rvas[448];
  int tracked = texture_source_for(texture, source, sizeof(source), NULL);

  if (g_orig_d3d_texture_get_surface_level != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_texture_get_surface_level_fn)g_orig_d3d_texture_get_surface_level)(texture, level, surface);
    --g_textdraw_render_hook_depth;
  }
  if (rc >= 0 && surface != NULL) {
    remember_texture_surface(texture, level, *surface);
  }
  if (tracked || should_log_font5_rtt_call(n)) {
    format_font5_stack(stack_rvas, sizeof(stack_rvas));
    probe_log("textdraw_rtt: IDirect3DTexture9.GetSurfaceLevel seq=%ld font5_seq=%ld caller=%p "
              "caller_samp_rva=0x%08lx texture=%p source='%s' level=%u surface=%p result=0x%08lx "
              "stack_samp='%s' evidence=STATIC_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, (long)g_font5_active_seq, caller, samp_rva_from_address(caller), texture, source,
              (unsigned)level, rc >= 0 && surface != NULL ? *surface : NULL, (unsigned long)rc, stack_rvas);
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_set_render_state(void *device, DWORD state, DWORD value) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;

  if (should_log_textdraw_render_call(caller, n, NULL)) {
    probe_log("textdraw_render: IDirect3DDevice9.SetRenderState seq=%ld caller=%p caller_samp_rva=0x%08lx "
              "device=%p state=%lu value=0x%08lx",
              (long)n, caller, samp_rva_from_address(caller), device, (unsigned long)state, (unsigned long)value);
  }
  if (g_orig_d3d_set_render_state != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_set_render_state_fn)g_orig_d3d_set_render_state)(device, state, value);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_set_texture(void *device, DWORD stage, void *texture) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;
  char source[192];

  (void)texture_source_for(texture, source, sizeof(source), NULL);
  if (stage == 0) {
    g_d3d_stage0_texture = texture;
  }
  if (should_log_textdraw_render_call(caller, n, source)) {
    probe_log("textdraw_render: IDirect3DDevice9.SetTexture seq=%ld caller=%p caller_samp_rva=0x%08lx device=%p "
              "stage=%lu texture=%p source='%s'",
              (long)n, caller, samp_rva_from_address(caller), device, (unsigned long)stage, texture, source);
  }
  if (g_orig_d3d_set_texture != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_set_texture_fn)g_orig_d3d_set_texture)(device, stage, texture);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_set_fvf(void *device, DWORD fvf) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;

  if (should_log_textdraw_render_call(caller, n, NULL)) {
    probe_log("textdraw_render: IDirect3DDevice9.SetFVF seq=%ld caller=%p caller_samp_rva=0x%08lx device=%p fvf=0x%08lx",
              (long)n, caller, samp_rva_from_address(caller), device, (unsigned long)fvf);
  }
  if (g_orig_d3d_set_fvf != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_set_fvf_fn)g_orig_d3d_set_fvf)(device, fvf);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static void log_d3d_draw_call(const char *name, LONG n, void *caller, void *device, DWORD primitive_type,
                              UINT primitive_count, const char *extra) {
  char source[192];

  (void)texture_source_for(g_d3d_stage0_texture, source, sizeof(source), NULL);
  if (!should_log_textdraw_render_call(caller, n, source)) {
    return;
  }
  probe_log("textdraw_render: %s seq=%ld caller=%p caller_samp_rva=0x%08lx device=%p prim_type=%lu prim_count=%u "
            "stage0_texture=%p source='%s' %s evidence=PROBE_TRACE,TODO_VERIFY",
            name != NULL ? name : "IDirect3DDevice9.Draw", (long)n, caller, samp_rva_from_address(caller), device,
            (unsigned long)primitive_type, (unsigned)primitive_count, g_d3d_stage0_texture, source,
            extra != NULL ? extra : "");
}

static HRESULT WINAPI hook_d3d_draw_primitive(void *device, DWORD primitive_type, UINT start_vertex,
                                             UINT primitive_count) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;
  char extra[80];

  snprintf(extra, sizeof(extra), "start_vertex=%u", (unsigned)start_vertex);
  log_d3d_draw_call("IDirect3DDevice9.DrawPrimitive", n, caller, device, primitive_type, primitive_count, extra);
  if (g_orig_d3d_draw_primitive != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_draw_primitive_fn)g_orig_d3d_draw_primitive)(device, primitive_type, start_vertex,
                                                                  primitive_count);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_draw_indexed_primitive(void *device, DWORD primitive_type, INT base_vertex_index,
                                                     UINT min_vertex_index, UINT num_vertices, UINT start_index,
                                                     UINT primitive_count) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;
  char extra[160];

  snprintf(extra, sizeof(extra), "base=%ld min=%u vertices=%u start_index=%u", (long)base_vertex_index,
           (unsigned)min_vertex_index, (unsigned)num_vertices, (unsigned)start_index);
  log_d3d_draw_call("IDirect3DDevice9.DrawIndexedPrimitive", n, caller, device, primitive_type, primitive_count, extra);
  if (g_orig_d3d_draw_indexed_primitive != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_draw_indexed_primitive_fn)g_orig_d3d_draw_indexed_primitive)(
        device, primitive_type, base_vertex_index, min_vertex_index, num_vertices, start_index, primitive_count);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_draw_primitive_up(void *device, DWORD primitive_type, UINT primitive_count,
                                                const void *vertex_stream_zero_data,
                                                UINT vertex_stream_zero_stride) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;
  char extra[128];

  snprintf(extra, sizeof(extra), "vertices=%p stride=%u", vertex_stream_zero_data, (unsigned)vertex_stream_zero_stride);
  log_d3d_draw_call("IDirect3DDevice9.DrawPrimitiveUP", n, caller, device, primitive_type, primitive_count, extra);
  if (g_orig_d3d_draw_primitive_up != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_draw_primitive_up_fn)g_orig_d3d_draw_primitive_up)(device, primitive_type, primitive_count,
                                                                        vertex_stream_zero_data,
                                                                        vertex_stream_zero_stride);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static HRESULT WINAPI hook_d3d_draw_indexed_primitive_up(void *device, DWORD primitive_type, UINT min_vertex_index,
                                                        UINT num_vertices, UINT primitive_count,
                                                        const void *index_data, DWORD index_data_format,
                                                        const void *vertex_stream_zero_data,
                                                        UINT vertex_stream_zero_stride) {
  void *caller = probe_return_address();
  LONG n = next_call_count(&g_textdraw_render_call_count);
  HRESULT rc = E_FAIL;
  char extra[192];

  snprintf(extra, sizeof(extra), "min=%u vertices=%u indices=%p index_fmt=%lu vertex_data=%p stride=%u",
           (unsigned)min_vertex_index, (unsigned)num_vertices, index_data, (unsigned long)index_data_format,
           vertex_stream_zero_data, (unsigned)vertex_stream_zero_stride);
  log_d3d_draw_call("IDirect3DDevice9.DrawIndexedPrimitiveUP", n, caller, device, primitive_type, primitive_count,
                    extra);
  if (g_orig_d3d_draw_indexed_primitive_up != NULL) {
    ++g_textdraw_render_hook_depth;
    rc = ((probe_d3d_draw_indexed_primitive_up_fn)g_orig_d3d_draw_indexed_primitive_up)(
        device, primitive_type, min_vertex_index, num_vertices, primitive_count, index_data, index_data_format,
        vertex_stream_zero_data, vertex_stream_zero_stride);
    --g_textdraw_render_hook_depth;
  }
  return rc;
}

static void log_custom_heavy_model_add(const char *name, LONG count, void *caller, int model_id, DWORD before_count,
                                       DWORD after_count, void *slot_before, void *slot_after, void *result) {
  char stack_rvas[512];
  uintptr_t slot_address;

  if (!custom_object_heavy_enabled()) {
    return;
  }
  if (!probe_model_id_is_custom(model_id) && !probe_model_id_is_tracked(model_id) && before_count < 13980u &&
      !should_log_call(count)) {
    return;
  }

  stack_rvas[0] = '\0';
  format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
  slot_address = gta_model_info_slot_address_for_id(model_id);
  probe_log("custom_object_heavy: model_add name=%s count=%ld caller=%p caller_samp_rva=0x%08lx model=%d "
            "slot_addr=0x%08lx slot_before=%p slot_after=%p result=%p store_before=%lu store_after=%lu "
            "atomic=%lu time=%lu clump=%lu stack_samp='%s' evidence=OBSERVED_037,PROBE_TRACE,TODO_VERIFY",
            name != NULL ? name : "unknown", (long)count, caller, samp_rva_from_address(caller), model_id,
            (unsigned long)slot_address, slot_before, slot_after, result, (unsigned long)before_count,
            (unsigned long)after_count,
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu), stack_rvas);
}

static void log_gta_model_add(const char *name, LONG count, void *caller, int model_id, DWORD before_count,
                              void *slot_before, void *result) {
  DWORD after_count = gta_model_info_store_count_for_name(name);
  void *model_info = gta_model_info_ptr_for_id(model_id);
  probe_log("gta_asset: %s count=%ld caller=%p caller_samp_rva=0x%08lx model=%d store_before=%lu store_after=%lu "
            "slot_before=%p result=%p model_info_ptr=%p evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
            name, (long)count, caller, samp_rva_from_address(caller), model_id, (unsigned long)before_count,
            (unsigned long)after_count, slot_before, result, model_info);
  log_custom_heavy_model_add(name, count, caller, model_id, before_count, after_count, slot_before, model_info, result);
  log_gta_model_info_dump(name, model_id, caller, should_log_call(count) ||
                                                    (custom_object_heavy_enabled() && probe_model_id_interesting(model_id)));
}

static void log_gta_physical_entity(const char *phase, LONG count, void *caller, void *entity) {
  uintptr_t entity_addr = (uintptr_t)entity;
  int readable = entity_addr != 0 &&
                 memory_is_readable(entity_addr, PROBE_GTA_PHYSICAL_OFFSET_SECTOR_LIST + sizeof(DWORD));
  DWORD vtable = read_u32_or(entity_addr, 0xffffffffu);
  DWORD rw_object = read_u32_or(entity_addr + PROBE_GTA_ENTITY_OFFSET_RW_OBJECT, 0xffffffffu);
  WORD model = read_u16_or(entity_addr + PROBE_GTA_ENTITY_OFFSET_MODEL_INDEX, 0xffffu);
  BYTE status = read_u8_or(entity_addr + PROBE_GTA_ENTITY_OFFSET_STATUS, 0xffu);
  DWORD sector_link = read_u32_or(entity_addr + PROBE_GTA_PHYSICAL_OFFSET_SECTOR_LIST, 0xffffffffu);
  void *model_info = model != 0xffffu ? gta_model_info_ptr_for_id((int)model) : NULL;
  int model_interesting = model != 0xffffu &&
                          (probe_model_id_is_custom((int)model) || probe_model_id_is_tracked((int)model));
  int sample = should_log_call(count);
  int interesting = !readable || model_interesting || model == 1383u || sample;

  if (!interesting) {
    return;
  }

  probe_log("gta_asset: CPhysical.Add.%s count=%ld caller=%p caller_samp_rva=0x%08lx entity=%p readable=%d "
            "vtable=0x%08lx rw_object=0x%08lx model=%u status=0x%02x sector_link=0x%08lx model_info_ptr=%p "
            "atomic=%lu time=%lu clump=%lu evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
            phase != NULL ? phase : "unknown", (long)count, caller, samp_rva_from_address(caller), entity, readable,
            (unsigned long)vtable, (unsigned long)rw_object, (unsigned)model, (unsigned)status,
            (unsigned long)sector_link, model_info,
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu));

  if (custom_object_heavy_enabled() && (model_interesting || !readable || sample)) {
    char stack_rvas[512];
    format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
    probe_log("custom_object_heavy: physical_add phase=%s count=%ld caller=%p caller_samp_rva=0x%08lx "
              "entity=%p readable=%d vtable=0x%08lx rw_object=0x%08lx model=%u status=0x%02x "
              "sector_link=0x%08lx model_info_ptr=%p atomic=%lu time=%lu clump=%lu stack_samp='%s' "
              "evidence=OBSERVED_037,PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
              phase != NULL ? phase : "unknown", (long)count, caller, samp_rva_from_address(caller), entity, readable,
              (unsigned long)vtable, (unsigned long)rw_object, (unsigned)model, (unsigned)status,
              (unsigned long)sector_link, model_info,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu), stack_rvas);
  }

  if (object_info_enabled() && model != 0xffffu && probe_model_id_interesting((int)model) &&
      g_model_info_physical_dumped[model] == 0) {
    g_model_info_physical_dumped[model] = 1;
    log_gta_model_info_dump(phase != NULL && strcmp(phase, "end") == 0 ? "CPhysical.Add.end" : "CPhysical.Add.begin",
                            (int)model, caller, 1);
  }
}

static void PROBE_THISCALL hook_gta_physical_add(void *entity) {
  static LONG count;
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();

  log_gta_physical_entity("begin", n, caller, entity);
  if (g_orig_gta_physical_add != NULL) {
    ((probe_gta_physical_add_fn)g_orig_gta_physical_add)(entity);
  }
  log_gta_physical_entity("end", n, caller, entity);
}

static void *__cdecl hook_gta_add_atomic_model(int model_id) {
  static LONG count;
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();
  DWORD before_count = gta_model_info_store_count_for_name("AddAtomicModel");
  void *slot_before = gta_model_info_ptr_for_id(model_id);
  void *result = NULL;

  if (g_orig_gta_add_atomic_model != NULL) {
    result = ((probe_gta_add_model_fn)g_orig_gta_add_atomic_model)(model_id);
  }
  log_gta_model_add("AddAtomicModel", n, caller, model_id, before_count, slot_before, result);
  return result;
}

static void *__cdecl hook_gta_add_time_model(int model_id) {
  static LONG count;
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();
  DWORD before_count = gta_model_info_store_count_for_name("AddTimeModel");
  void *slot_before = gta_model_info_ptr_for_id(model_id);
  void *result = NULL;

  if (g_orig_gta_add_time_model != NULL) {
    result = ((probe_gta_add_model_fn)g_orig_gta_add_time_model)(model_id);
  }
  log_gta_model_add("AddTimeModel", n, caller, model_id, before_count, slot_before, result);
  return result;
}

static void *__cdecl hook_gta_add_clump_model(int model_id) {
  static LONG count;
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();
  DWORD before_count = gta_model_info_store_count_for_name("AddClumpModel");
  void *slot_before = gta_model_info_ptr_for_id(model_id);
  void *result = NULL;

  if (g_orig_gta_add_clump_model != NULL) {
    result = ((probe_gta_add_model_fn)g_orig_gta_add_clump_model)(model_id);
  }
  log_gta_model_add("AddClumpModel", n, caller, model_id, before_count, slot_before, result);
  return result;
}

static int __cdecl hook_gta_add_image_to_list(const char *path, int not_player_img) {
  static LONG count;
  char safe_path[MAX_PATH];
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();
  int result = -1;

  safe_cstr_or(path, safe_path, sizeof(safe_path), "(null)");
  if (g_orig_gta_add_image_to_list != NULL) {
    result = ((probe_gta_add_image_to_list_fn)g_orig_gta_add_image_to_list)(path, not_player_img);
  }
  probe_log("gta_asset: AddImageToList count=%ld caller=%p caller_samp_rva=0x%08lx path='%s' not_player_img=%d "
            "result=%d evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
            (long)n, caller, samp_rva_from_address(caller), safe_path, not_player_img, result);
  if (custom_object_heavy_enabled() &&
      (ascii_contains_ci(safe_path, "SAMP") || ascii_contains_ci(safe_path, "CUSTOM") ||
       ascii_contains_ci(safe_path, "SAMPCOL") || should_log_call(n))) {
    char stack_rvas[512];
    format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
    probe_log("custom_object_heavy: add_image count=%ld caller=%p caller_samp_rva=0x%08lx path='%s' "
              "not_player_img=%d result=%d atomic=%lu time=%lu clump=%lu stack_samp='%s' "
              "evidence=OBSERVED_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), safe_path, not_player_img, result,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu), stack_rvas);
  }
  return result;
}

static void __cdecl hook_gta_load_cd_directory(const char *path, int image_id) {
  static LONG count;
  char safe_path[MAX_PATH];
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();
  DWORD atomic_before = read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu);
  DWORD time_before = read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu);
  DWORD clump_before = read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu);

  safe_cstr_or(path, safe_path, sizeof(safe_path), "(null)");
  probe_log("gta_asset: LoadCdDirectory.begin count=%ld caller=%p caller_samp_rva=0x%08lx path='%s' image_id=%d "
            "atomic=%lu time=%lu clump=%lu evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
            (long)n, caller, samp_rva_from_address(caller), safe_path, image_id, (unsigned long)atomic_before,
            (unsigned long)time_before, (unsigned long)clump_before);
  if (custom_object_heavy_enabled()) {
    char stack_rvas[512];
    format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
    probe_log("custom_object_heavy: load_cd_directory_begin count=%ld caller=%p caller_samp_rva=0x%08lx "
              "path='%s' image_id=%d atomic=%lu time=%lu clump=%lu stack_samp='%s' "
              "evidence=OBSERVED_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), safe_path, image_id, (unsigned long)atomic_before,
              (unsigned long)time_before, (unsigned long)clump_before, stack_rvas);
  }
  if (g_orig_gta_load_cd_directory != NULL) {
    ((probe_gta_load_cd_directory_fn)g_orig_gta_load_cd_directory)(path, image_id);
  }
  probe_log("gta_asset: LoadCdDirectory.end count=%ld path='%s' image_id=%d atomic=%lu time=%lu clump=%lu",
            (long)n, safe_path, image_id,
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
            (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu));
  if (custom_object_heavy_enabled()) {
    char stack_rvas[512];
    format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
    probe_log("custom_object_heavy: load_cd_directory_end count=%ld path='%s' image_id=%d atomic_before=%lu "
              "atomic_after=%lu time_before=%lu time_after=%lu clump_before=%lu clump_after=%lu stack_samp='%s' "
              "evidence=OBSERVED_037,PROBE_TRACE,TODO_VERIFY",
              (long)n, safe_path, image_id, (unsigned long)atomic_before,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
              (unsigned long)time_before,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
              (unsigned long)clump_before,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu), stack_rvas);
  }
  if (object_info_enabled()) {
    log_tracked_gta_model_infos("LoadCdDirectory.end.tracked", caller);
    if (ascii_contains_ci(safe_path, "CUSTOM.IMG") || ascii_contains_ci(safe_path, "SAMP.IMG") ||
        ascii_contains_ci(safe_path, "SAMPCOL.IMG")) {
      scan_gta_custom_model_infos("LoadCdDirectory.end.scan", caller);
    }
  }
}

static int __cdecl hook_gta_col_add_slot(const char *name) {
  static LONG count;
  char safe_name[128];
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();
  int result = -1;

  safe_cstr_or(name, safe_name, sizeof(safe_name), "(null)");
  if (g_orig_gta_col_add_slot != NULL) {
    result = ((probe_gta_col_add_slot_fn)g_orig_gta_col_add_slot)(name);
  }
  probe_log("gta_asset: ColStore.AddColSlot count=%ld caller=%p caller_samp_rva=0x%08lx name='%s' result=%d "
            "evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
            (long)n, caller, samp_rva_from_address(caller), safe_name, result);
  if (custom_object_heavy_enabled() &&
      (ascii_contains_ci(safe_name, "SAMP") || ascii_contains_ci(safe_name, "CUSTOM") ||
       ascii_contains_ci(safe_name, "SAMPCOL") || should_log_call(n))) {
    char stack_rvas[512];
    format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
    probe_log("custom_object_heavy: col_add_slot count=%ld caller=%p caller_samp_rva=0x%08lx name='%s' result=%d "
              "atomic=%lu time=%lu clump=%lu stack_samp='%s' "
              "evidence=OBSERVED_037,PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), safe_name, result,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu), stack_rvas);
  }
  return result;
}

static int __cdecl hook_gta_col_load_buffer(int slot, void *buffer, int size) {
  static LONG count;
  LONG n = next_call_count(&count);
  void *caller = probe_return_address();
  int result = 0;
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];

  payload[0] = '\0';
  payload_to_hex(buffer, size, payload, sizeof(payload));
  probe_log("gta_asset: ColStore.LoadCol.begin count=%ld caller=%p caller_samp_rva=0x%08lx slot=%d buffer=%p size=%d "
            "evidence=PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
            (long)n, caller, samp_rva_from_address(caller), slot, buffer, size);
  if (custom_object_heavy_enabled()) {
    char stack_rvas[512];
    format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
    probe_log("custom_object_heavy: col_load_begin count=%ld caller=%p caller_samp_rva=0x%08lx slot=%d buffer=%p "
              "size=%d first_bytes=%s atomic=%lu time=%lu clump=%lu stack_samp='%s' "
              "evidence=OBSERVED_037,PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
              (long)n, caller, samp_rva_from_address(caller), slot, buffer, size, payload,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu), stack_rvas);
  }
  if (g_orig_gta_col_load_buffer != NULL) {
    result = ((probe_gta_col_load_buffer_fn)g_orig_gta_col_load_buffer)(slot, buffer, size);
  }
  probe_log("gta_asset: ColStore.LoadCol.end count=%ld slot=%d size=%d result=%d", (long)n, slot, size, result);
  if (custom_object_heavy_enabled()) {
    char stack_rvas[512];
    format_samp_stack_rvas(stack_rvas, sizeof(stack_rvas), 384, 12);
    probe_log("custom_object_heavy: col_load_end count=%ld slot=%d size=%d result=%d atomic=%lu time=%lu clump=%lu "
              "stack_samp='%s' evidence=OBSERVED_037,PROBE_TRACE,GTA_REVERSED_REF,TODO_VERIFY",
              (long)n, slot, size, result,
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_ATOMIC_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_TIME_STORE_COUNT, 0xffffffffu),
              (unsigned long)read_u32_or(PROBE_GTA_ADDR_MODEL_INFO_CLUMP_STORE_COUNT, 0xffffffffu), stack_rvas);
  }
  log_tracked_gta_model_infos("ColStore.LoadCol.end.tracked", caller);
  return result;
}

static int WINAPI hook_WSAStartup(WORD version, LPWSADATA data) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_WSAStartup_fn)g_orig_WSAStartup)(version, data);
  if (log_this && should_log_call(n)) {
    probe_log("call: WSAStartup count=%ld caller=%p caller_rva=0x%08lx version=0x%04x rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), (unsigned)version, rc, last_wsa_error());
  }
  return rc;
}

static int WINAPI hook_WSACleanup(void) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_WSACleanup_fn)g_orig_WSACleanup)();
  if (log_this && should_log_call(n)) {
    probe_log("call: WSACleanup count=%ld caller=%p caller_rva=0x%08lx rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), rc, last_wsa_error());
  }
  return rc;
}

static SOCKET WINAPI hook_socket(int af, int type, int protocol) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  SOCKET s = ((probe_socket_fn)g_orig_socket)(af, type, protocol);
  if (log_this && should_log_call(n)) {
    probe_log("call: socket count=%ld caller=%p caller_rva=0x%08lx af=%d type=%d proto=%d result=0x%lx err=%d",
              (long)n, caller, samp_rva_from_address(caller), af, type, protocol, (unsigned long)s, last_wsa_error());
  }
  return s;
}

static int WINAPI hook_bind(SOCKET s, const struct sockaddr *name, int namelen) {
  static LONG count;
  char addr[96];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_bind_fn)g_orig_bind)(s, name, namelen);
  if (log_this && should_log_call(n)) {
    format_sockaddr(name, namelen, addr, sizeof(addr));
    probe_log("call: bind count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx addr=%s rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)s, addr, rc, last_wsa_error());
  }
  return rc;
}

static int WINAPI hook_connect(SOCKET s, const struct sockaddr *name, int namelen) {
  static LONG count;
  char addr[96];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc;

  if (log_this && should_log_call(n)) {
    format_sockaddr(name, namelen, addr, sizeof(addr));
    probe_log("call: connect.begin count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx addr=%s", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)s, addr);
  }
  rc = ((probe_connect_fn)g_orig_connect)(s, name, namelen);
  if (log_this && should_log_call(n)) {
    format_sockaddr(name, namelen, addr, sizeof(addr));
    probe_log("call: connect.end count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx addr=%s rc=%d err=%d", (long)n,
              caller, samp_rva_from_address(caller), (unsigned long)s, addr, rc, last_wsa_error());
  }
  return rc;
}

static int WINAPI hook_closesocket(SOCKET s) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_closesocket_fn)g_orig_closesocket)(s);
  if (log_this && should_log_call(n)) {
    probe_log("call: closesocket count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)s, rc, last_wsa_error());
  }
  return rc;
}

static struct hostent *WINAPI hook_gethostbyname(const char *name) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  struct hostent *result = ((probe_gethostbyname_fn)g_orig_gethostbyname)(name);
  if (log_this && should_log_call(n)) {
    probe_log("call: gethostbyname count=%ld caller=%p caller_rva=0x%08lx name=%s result=%p err=%d", (long)n, caller,
              samp_rva_from_address(caller), name ? name : "(null)", (void *)result, last_wsa_error());
  }
  return result;
}

static int WINAPI hook_gethostname(char *name, int namelen) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_gethostname_fn)g_orig_gethostname)(name, namelen);
  if (log_this && should_log_call(n)) {
    probe_log("call: gethostname count=%ld caller=%p caller_rva=0x%08lx rc=%d value=%s err=%d", (long)n, caller,
              samp_rva_from_address(caller), rc, (rc == 0 && name != NULL) ? name : "", last_wsa_error());
  }
  return rc;
}

static unsigned long WINAPI hook_inet_addr(const char *cp) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  unsigned long rc = ((probe_inet_addr_fn)g_orig_inet_addr)(cp);
  if (log_this && should_log_call(n)) {
    probe_log("call: inet_addr count=%ld caller=%p caller_rva=0x%08lx text=%s result=0x%08lx", (long)n, caller,
              samp_rva_from_address(caller), cp ? cp : "(null)", rc);
  }
  return rc;
}

static char *WINAPI hook_inet_ntoa(struct in_addr in) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  char *rc = ((probe_inet_ntoa_fn)g_orig_inet_ntoa)(in);
  if (log_this && should_log_call(n)) {
    probe_log("call: inet_ntoa count=%ld caller=%p caller_rva=0x%08lx addr=0x%08lx text=%s", (long)n, caller,
              samp_rva_from_address(caller), (unsigned long)in.s_addr, rc ? rc : "(null)");
  }
  return rc;
}

static int WINAPI hook_send(SOCKET s, const char *buf, int len, int flags) {
  static LONG count;
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_send_fn)g_orig_send)(s, buf, len, flags);
  if (log_this && should_log_call(n)) {
    payload_to_hex(buf, len, payload, sizeof(payload));
    probe_log("call: send count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d flags=0x%x rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, flags, rc, last_wsa_error(), payload);
  }
  return rc;
}

static int WINAPI hook_recv(SOCKET s, char *buf, int len, int flags) {
  static LONG count;
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_recv_fn)g_orig_recv)(s, buf, len, flags);
  if (log_this && should_log_call(n)) {
    payload_to_hex(buf, rc > 0 ? rc : 0, payload, sizeof(payload));
    probe_log("call: recv count=%ld caller=%p caller_rva=0x%08lx socket=0x%lx len=%d flags=0x%x rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), (unsigned long)s, len, flags, rc, last_wsa_error(), payload);
  }
  return rc;
}

static int WINAPI hook_sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_sendto_fn)g_orig_sendto)(s, buf, len, flags, to, tolen);
  if (log_this && should_log_call(n)) {
    const char *site = samp_observed_socket_callsite(caller);
    format_sockaddr(to, tolen, addr, sizeof(addr));
    payload_to_hex(buf, len, payload, sizeof(payload));
    probe_log("call: sendto count=%ld caller=%p caller_rva=0x%08lx site=%s socket=0x%lx len=%d flags=0x%x to=%s "
              "rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), site, (unsigned long)s, len, flags, addr, rc,
              last_wsa_error(), payload);
  }
  return rc;
}

static int WINAPI hook_recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen) {
  static LONG count;
  char addr[96];
  char payload[PROBE_PAYLOAD_PREVIEW_BYTES * 3 + 8];
  int observed_len = fromlen != NULL ? *fromlen : 0;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_recvfrom_fn)g_orig_recvfrom)(s, buf, len, flags, from, fromlen);
  if (log_this && should_log_call(n)) {
    const char *site = samp_observed_socket_callsite(caller);
    format_sockaddr(from, fromlen != NULL ? *fromlen : observed_len, addr, sizeof(addr));
    payload_to_hex(buf, rc > 0 ? rc : 0, payload, sizeof(payload));
    probe_log("call: recvfrom count=%ld caller=%p caller_rva=0x%08lx site=%s socket=0x%lx len=%d flags=0x%x from=%s "
              "rc=%d err=%d data=%s",
              (long)n, caller, samp_rva_from_address(caller), site, (unsigned long)s, len, flags, addr, rc,
              last_wsa_error(), payload);
  }
  return rc;
}

static int WINAPI hook_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout) {
  static LONG count;
  void *caller = probe_return_address();
  int log_this = should_log_api_from_caller(caller);
  LONG n = next_call_count(&count);
  int rc = ((probe_select_fn)g_orig_select)(nfds, readfds, writefds, exceptfds, timeout);
  if (log_this && should_log_call(n)) {
    probe_log("call: select count=%ld caller=%p caller_rva=0x%08lx nfds=%d rc=%d err=%d", (long)n, caller,
              samp_rva_from_address(caller), nfds, rc, last_wsa_error());
  }
  return rc;
}

static BOOL WINAPI hook_VirtualProtect(LPVOID address, SIZE_T size, DWORD new_protect, PDWORD old_protect) {
  static LONG count;
  LONG n = next_call_count(&count);
  BOOL rc = ((probe_VirtualProtect_fn)g_orig_VirtualProtect)(address, size, new_protect, old_protect);
  probe_log("call: VirtualProtect count=%ld addr=%p size=0x%lx new=0x%lx old=0x%lx rc=%d err=%lu", (long)n, address,
            (unsigned long)size, (unsigned long)new_protect, old_protect ? (unsigned long)*old_protect : 0ul, (int)rc,
            (unsigned long)GetLastError());
  return rc;
}

static HMODULE WINAPI hook_LoadLibraryA(LPCSTR file_name) {
  static LONG count;
  LONG n = next_call_count(&count);
  HMODULE rc = ((probe_LoadLibraryA_fn)g_orig_LoadLibraryA)(file_name);
  probe_log("call: LoadLibraryA count=%ld name=%s result=%p err=%lu", (long)n, file_name ? file_name : "(null)", (void *)rc,
            (unsigned long)GetLastError());
  if (g_samp_module != NULL) {
    PIMAGE_NT_HEADERS nt = get_samp_nt_headers();
    if (nt != NULL) {
      (void)install_iat_hooks(nt, 0);
    }
  }
  return rc;
}

static HMODULE WINAPI hook_LoadLibraryW(LPCWSTR file_name) {
  static LONG count;
  char narrowed[260];
  LONG n = next_call_count(&count);
  HMODULE rc = ((probe_LoadLibraryW_fn)g_orig_LoadLibraryW)(file_name);

  narrowed[0] = '\0';
  if (file_name != NULL) {
    WideCharToMultiByte(CP_ACP, 0, file_name, -1, narrowed, sizeof(narrowed), NULL, NULL);
  }
  probe_log("call: LoadLibraryW count=%ld name=%s result=%p err=%lu", (long)n, narrowed[0] ? narrowed : "(null)", (void *)rc,
            (unsigned long)GetLastError());
  return rc;
}

static FARPROC WINAPI hook_GetProcAddress(HMODULE module, LPCSTR proc_name) {
  static LONG count;
  LONG n = next_call_count(&count);
  FARPROC rc = ((probe_GetProcAddress_fn)g_orig_GetProcAddress)(module, proc_name);
  if (((uintptr_t)proc_name >> 16u) == 0) {
    probe_log("call: GetProcAddress count=%ld module=%p ordinal=%lu result=%p err=%lu", (long)n, (void *)module,
              (unsigned long)(uintptr_t)proc_name, (void *)rc, (unsigned long)GetLastError());
  } else {
    probe_log("call: GetProcAddress count=%ld module=%p name=%s result=%p err=%lu", (long)n, (void *)module,
              proc_name ? proc_name : "(null)", (void *)rc, (unsigned long)GetLastError());
  }
  return rc;
}

static HANDLE WINAPI hook_CreateThread(LPSECURITY_ATTRIBUTES attr, SIZE_T stack_size, LPTHREAD_START_ROUTINE start, LPVOID param,
                                       DWORD flags, LPDWORD thread_id) {
  static LONG count;
  LONG n = next_call_count(&count);
  HANDLE rc = ((probe_CreateThread_fn)g_orig_CreateThread)(attr, stack_size, start, param, flags, thread_id);
  probe_log("call: CreateThread count=%ld start=%p param=%p flags=0x%lx tid=%lu handle=%p err=%lu", (long)n, (void *)start,
            param, (unsigned long)flags, thread_id ? (unsigned long)*thread_id : 0ul, rc, (unsigned long)GetLastError());
  return rc;
}

static HANDLE WINAPI hook_CreateFileA(LPCSTR file_name, DWORD desired_access, DWORD share_mode,
                                      LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                      DWORD flags_and_attributes, HANDLE template_file) {
  static LONG count;
  void *caller = probe_return_address();
  int top_level = file_hook_enter();
  HANDLE rc;
  DWORD saved_error;

  if (g_orig_CreateFileA == NULL) {
    file_hook_leave();
    return INVALID_HANDLE_VALUE;
  }

  rc = ((probe_CreateFileA_fn)g_orig_CreateFileA)(file_name, desired_access, share_mode, security_attributes,
                                                  creation_disposition, flags_and_attributes, template_file);
  saved_error = GetLastError();
  if (top_level) {
    int interesting = asset_path_interesting(file_name);
    int log_this = interesting || should_log_api_from_caller(caller);
    LONG n = next_call_count(&count);
    if (interesting && rc != INVALID_HANDLE_VALUE) {
      remember_asset_handle(rc, file_name);
    }
    if (log_this && should_log_call(n)) {
      probe_log("file: CreateFileA count=%ld caller=%p caller_rva=0x%08lx path='%s' access=0x%08lx share=0x%08lx "
                "disp=%lu flags=0x%08lx result=%p err=%lu interesting=%d",
                (long)n, caller, samp_rva_from_address(caller), file_name != NULL ? file_name : "(null)",
                (unsigned long)desired_access, (unsigned long)share_mode, (unsigned long)creation_disposition,
                (unsigned long)flags_and_attributes, rc, (unsigned long)saved_error, interesting);
    }
  }
  file_hook_leave();
  SetLastError(saved_error);
  return rc;
}

static HANDLE WINAPI hook_CreateFileW(LPCWSTR file_name, DWORD desired_access, DWORD share_mode,
                                      LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                      DWORD flags_and_attributes, HANDLE template_file) {
  static LONG count;
  char narrowed[MAX_PATH];
  void *caller = probe_return_address();
  int top_level = file_hook_enter();
  HANDLE rc;
  DWORD saved_error;

  narrowed[0] = '\0';
  if (file_name != NULL) {
    WideCharToMultiByte(CP_ACP, 0, file_name, -1, narrowed, sizeof(narrowed), NULL, NULL);
  }

  if (g_orig_CreateFileW == NULL) {
    file_hook_leave();
    return INVALID_HANDLE_VALUE;
  }

  rc = ((probe_CreateFileW_fn)g_orig_CreateFileW)(file_name, desired_access, share_mode, security_attributes,
                                                  creation_disposition, flags_and_attributes, template_file);
  saved_error = GetLastError();
  if (top_level) {
    int interesting = asset_path_interesting(narrowed);
    int log_this = interesting || should_log_api_from_caller(caller);
    LONG n = next_call_count(&count);
    if (interesting && rc != INVALID_HANDLE_VALUE) {
      remember_asset_handle(rc, narrowed);
    }
    if (log_this && should_log_call(n)) {
      probe_log("file: CreateFileW count=%ld caller=%p caller_rva=0x%08lx path='%s' access=0x%08lx share=0x%08lx "
                "disp=%lu flags=0x%08lx result=%p err=%lu interesting=%d",
                (long)n, caller, samp_rva_from_address(caller), narrowed[0] ? narrowed : "(null)",
                (unsigned long)desired_access, (unsigned long)share_mode, (unsigned long)creation_disposition,
                (unsigned long)flags_and_attributes, rc, (unsigned long)saved_error, interesting);
    }
  }
  file_hook_leave();
  SetLastError(saved_error);
  return rc;
}

static BOOL WINAPI hook_ReadFile(HANDLE file, LPVOID buffer, DWORD bytes_to_read, LPDWORD bytes_read, LPOVERLAPPED overlapped) {
  char path[MAX_PATH];
  LONG read_count = 0;
  int top_level = file_hook_enter();
  BOOL rc;
  DWORD saved_error;

  if (g_orig_ReadFile == NULL) {
    file_hook_leave();
    return FALSE;
  }

  rc = ((probe_ReadFile_fn)g_orig_ReadFile)(file, buffer, bytes_to_read, bytes_read, overlapped);
  saved_error = GetLastError();
  if (top_level && find_asset_handle(file, path, sizeof(path), &read_count)) {
    DWORD observed = bytes_read != NULL ? *bytes_read : 0ul;
    if (read_count <= 32 || (read_count & 0xff) == 0 || (rc && observed == 0)) {
      probe_log("file: ReadFile count=%ld handle=%p path='%s' request=%lu read=%lu overlapped=%p rc=%d err=%lu",
                (long)read_count, file, path, (unsigned long)bytes_to_read, (unsigned long)observed, overlapped, (int)rc,
                (unsigned long)saved_error);
    }
  }
  file_hook_leave();
  SetLastError(saved_error);
  return rc;
}

static DWORD WINAPI hook_SetFilePointer(HANDLE file, LONG distance_to_move, PLONG distance_to_move_high, DWORD move_method) {
  char path[MAX_PATH];
  int top_level = file_hook_enter();
  DWORD rc;
  DWORD saved_error;

  if (g_orig_SetFilePointer == NULL) {
    file_hook_leave();
    return INVALID_SET_FILE_POINTER;
  }

  rc = ((probe_SetFilePointer_fn)g_orig_SetFilePointer)(file, distance_to_move, distance_to_move_high, move_method);
  saved_error = GetLastError();
  if (top_level && find_asset_handle(file, path, sizeof(path), NULL)) {
    probe_log("file: SetFilePointer handle=%p path='%s' distance=%ld high=%ld method=%lu result=%lu err=%lu", file, path,
              (long)distance_to_move, distance_to_move_high != NULL ? (long)*distance_to_move_high : 0l,
              (unsigned long)move_method, (unsigned long)rc, (unsigned long)saved_error);
  }
  file_hook_leave();
  SetLastError(saved_error);
  return rc;
}

static DWORD WINAPI hook_GetFileSize(HANDLE file, LPDWORD file_size_high) {
  char path[MAX_PATH];
  int top_level = file_hook_enter();
  DWORD rc;
  DWORD saved_error;

  if (g_orig_GetFileSize == NULL) {
    file_hook_leave();
    return INVALID_FILE_SIZE;
  }

  rc = ((probe_GetFileSize_fn)g_orig_GetFileSize)(file, file_size_high);
  saved_error = GetLastError();
  if (top_level && find_asset_handle(file, path, sizeof(path), NULL)) {
    probe_log("file: GetFileSize handle=%p path='%s' low=%lu high=%lu err=%lu", file, path, (unsigned long)rc,
              file_size_high != NULL ? (unsigned long)*file_size_high : 0ul, (unsigned long)saved_error);
  }
  file_hook_leave();
  SetLastError(saved_error);
  return rc;
}

static BOOL WINAPI hook_CloseHandle(HANDLE object) {
  char path[MAX_PATH];
  int top_level = file_hook_enter();
  int tracked = 0;
  BOOL rc;
  DWORD saved_error;

  if (top_level) {
    tracked = find_asset_handle(object, path, sizeof(path), NULL);
  }

  if (g_orig_CloseHandle == NULL) {
    file_hook_leave();
    return FALSE;
  }

  rc = ((probe_CloseHandle_fn)g_orig_CloseHandle)(object);
  saved_error = GetLastError();
  if (top_level && tracked) {
    probe_log("file: CloseHandle handle=%p path='%s' rc=%d err=%lu", object, path, (int)rc, (unsigned long)saved_error);
    forget_asset_handle(object);
  }
  file_hook_leave();
  SetLastError(saved_error);
  return rc;
}

static int should_log_exception_code(DWORD code) {
  return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
         code == EXCEPTION_STACK_OVERFLOW || code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
         code == EXCEPTION_PRIV_INSTRUCTION;
}

static LONG CALLBACK probe_exception_handler(PEXCEPTION_POINTERS info) {
  DWORD code;
  void *fault_address;
  uintptr_t ip = 0;
  uintptr_t sp = 0;
  uintptr_t bp = 0;
  DWORD_PTR info0 = 0;
  DWORD_PTR info1 = 0;

  if (info == NULL || info->ExceptionRecord == NULL) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  code = info->ExceptionRecord->ExceptionCode;
  if (!should_log_exception_code(code)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (InterlockedCompareExchange(&g_exception_logged, 1, 0) != 0) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  fault_address = info->ExceptionRecord->ExceptionAddress;
  if (info->ExceptionRecord->NumberParameters > 0) {
    info0 = info->ExceptionRecord->ExceptionInformation[0];
  }
  if (info->ExceptionRecord->NumberParameters > 1) {
    info1 = info->ExceptionRecord->ExceptionInformation[1];
  }

#if defined(_M_IX86) || defined(__i386__)
  if (info->ContextRecord != NULL) {
    ip = (uintptr_t)info->ContextRecord->Eip;
    sp = (uintptr_t)info->ContextRecord->Esp;
    bp = (uintptr_t)info->ContextRecord->Ebp;
  }
#endif

  probe_log("exception: code=0x%08lx fault=%p fault_samp_rva=0x%08lx ip=%p ip_samp_rva=0x%08lx sp=%p bp=%p info0=0x%lx info1=%p",
            (unsigned long)code, fault_address, samp_rva_from_address(fault_address), (void *)ip,
            samp_rva_from_address((void *)ip), (void *)sp, (void *)bp, (unsigned long)info0, (void *)info1);
#if defined(_M_IX86) || defined(__i386__)
  if (info->ContextRecord != NULL) {
    probe_log("registers: eax=0x%08lx ebx=0x%08lx ecx=0x%08lx edx=0x%08lx esi=0x%08lx edi=0x%08lx "
              "eflags=0x%08lx",
              (unsigned long)info->ContextRecord->Eax, (unsigned long)info->ContextRecord->Ebx,
              (unsigned long)info->ContextRecord->Ecx, (unsigned long)info->ContextRecord->Edx,
              (unsigned long)info->ContextRecord->Esi, (unsigned long)info->ContextRecord->Edi,
              (unsigned long)info->ContextRecord->EFlags);
  }
#endif
  probe_log_address_region("fault", (uintptr_t)fault_address);
  probe_log_address_region("ip", ip);
  probe_log_address_region("sp", sp);
  probe_log_memory_bytes("ip", ip, 16, 64);
  probe_log_stack_dwords(sp);
  probe_log_transition_snapshot("exception");
  return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  (void)reserved;

  if (reason == DLL_PROCESS_ATTACH) {
    DWORD thread_id = 0;
    g_self_module = (HMODULE)instance;
    DisableThreadLibraryCalls(instance);
    g_exception_handler = AddVectoredExceptionHandler(1, probe_exception_handler);
    g_stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (g_stop_event != NULL) {
      g_worker_thread = CreateThread(NULL, 0, probe_worker, NULL, 0, &thread_id);
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    if (g_stop_event != NULL) {
      SetEvent(g_stop_event);
    }
    if (g_exception_handler != NULL) {
      RemoveVectoredExceptionHandler(g_exception_handler);
      g_exception_handler = NULL;
    }
  }

  return TRUE;
}
