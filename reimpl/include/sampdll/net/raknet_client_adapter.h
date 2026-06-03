#ifndef SAMPDLL_NET_RAKNET_CLIENT_ADAPTER_H
#define SAMPDLL_NET_RAKNET_CLIENT_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int samp_raknet_client_available(void);
int samp_raknet_client_create(void **out_client);
int samp_raknet_client_destroy(void *client);

int samp_raknet_client_connect(void *client, const char *host, uint16_t server_port, uint16_t client_port,
                               int thread_sleep_timer);
void samp_raknet_client_disconnect(void *client, unsigned int block_duration, unsigned char ordering_channel);
int samp_raknet_client_is_connected(void *client);
int samp_raknet_client_mark_logged_on(void *client);
int samp_raknet_client_send_chat(void *client, const char *text);
int samp_raknet_client_send_server_command(void *client, const char *command);
int samp_raknet_client_send_spawn_notification(void *client);
int samp_raknet_client_send_spawn_notification_for_seq(void *client, uint32_t spawn_info_seq);
int samp_raknet_client_send_textdraw_click(void *client, uint16_t textdraw_id);

#pragma pack(push, 1)
typedef struct samp_raknet_onfoot_sync {
  uint16_t left_right_keys;
  uint16_t up_down_keys;
  uint16_t keys;
  float position[3];
  float quaternion[4];
  uint8_t health;
  uint8_t armour;
  uint8_t current_weapon;
  uint8_t special_action;
  float move_speed[3];
  float surfing_offsets[3];
  uint16_t surfing_vehicle_id;
  int16_t current_animation_id;
  int16_t animation_flags;
} samp_raknet_onfoot_sync;
#pragma pack(pop)

int samp_raknet_client_send_onfoot_sync(void *client, const samp_raknet_onfoot_sync *sync);

typedef struct samp_raknet_join_profile {
  char nickname[25];
  char client_version[16];
  uint32_t netgame_version;
  uint8_t mod_byte;
} samp_raknet_join_profile;

#define SAMP_RAKNET_RPC_FLAG_INIT_GAME 0x00000001u
#define SAMP_RAKNET_RPC_FLAG_REQUEST_CLASS_REPLY 0x00000002u
#define SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_REPLY 0x00000004u
#define SAMP_RAKNET_RPC_FLAG_SPAWN_INFO 0x00000008u
#define SAMP_RAKNET_RPC_FLAG_DIALOG 0x00000010u
#define SAMP_RAKNET_RPC_FLAG_DIALOG_RESPONSE_SENT 0x00000020u
#define SAMP_RAKNET_RPC_FLAG_REQUEST_CLASS_SENT 0x00000040u
#define SAMP_RAKNET_RPC_FLAG_REQUEST_SPAWN_SENT 0x00000080u
#define SAMP_RAKNET_RPC_FLAG_PLAYER_POS 0x00000100u
#define SAMP_RAKNET_RPC_FLAG_PLAYER_FACING 0x00000200u
#define SAMP_RAKNET_RPC_FLAG_WEATHER 0x00000400u
#define SAMP_RAKNET_RPC_FLAG_INTERIOR 0x00000800u
#define SAMP_RAKNET_RPC_FLAG_CAMERA_POS 0x00001000u
#define SAMP_RAKNET_RPC_FLAG_CAMERA_LOOK_AT 0x00002000u
#define SAMP_RAKNET_RPC_FLAG_CLIENT_MESSAGE 0x00004000u
#define SAMP_RAKNET_RPC_FLAG_TEXTDRAW_EVENT 0x00008000u
#define SAMP_RAKNET_RPC_FLAG_TEXTDRAW_SELECT 0x00010000u
#define SAMP_RAKNET_RPC_FLAG_OBJECT_EVENT 0x00020000u
#define SAMP_RAKNET_RPC_FLAG_VEHICLE_EVENT 0x00040000u

#define SAMP_RAKNET_CLIENT_MESSAGE_RING 8u
#define SAMP_RAKNET_CLIENT_MESSAGE_BYTES 256u
#define SAMP_RAKNET_HOSTNAME_BYTES 256u
#define SAMP_RAKNET_REQUIRED_VEHICLE_MODELS 212u
#define SAMP_RAKNET_DIALOG_TITLE_BYTES 256u
#define SAMP_RAKNET_DIALOG_INFO_BYTES 4096u
#define SAMP_RAKNET_DIALOG_BUTTON_BYTES 64u
#define SAMP_RAKNET_DIALOG_INPUT_BYTES 256u
#define SAMP_RAKNET_TEXTDRAW_EVENT_RING 64u
#define SAMP_RAKNET_TEXTDRAW_TEXT_BYTES 256u
#define SAMP_RAKNET_MAX_TEXTDRAWS 4096u
#define SAMP_RAKNET_TEXTDRAW_ACTION_SHOW 1u
#define SAMP_RAKNET_TEXTDRAW_ACTION_HIDE 2u
#define SAMP_RAKNET_TEXTDRAW_ACTION_EDIT 3u
#define SAMP_RAKNET_OBJECT_EVENT_RING 128u
#define SAMP_RAKNET_MAX_OBJECTS 2000u
#define SAMP_RAKNET_OBJECT_ACTION_CREATE 1u
#define SAMP_RAKNET_OBJECT_ACTION_DESTROY 2u
#define SAMP_RAKNET_OBJECT_ACTION_SET_POS 3u
#define SAMP_RAKNET_OBJECT_ACTION_SET_ROT 4u
#define SAMP_RAKNET_OBJECT_ACTION_MOVE 5u
#define SAMP_RAKNET_OBJECT_ACTION_STOP 6u
#define SAMP_RAKNET_VEHICLE_EVENT_RING 128u
#define SAMP_RAKNET_MAX_VEHICLES 2000u
#define SAMP_RAKNET_VEHICLE_MOD_SLOTS 14u
#define SAMP_RAKNET_VEHICLE_ACTION_CREATE 1u
#define SAMP_RAKNET_VEHICLE_ACTION_DESTROY 2u

typedef struct samp_raknet_client_message_probe {
  uint32_t seq;
  uint32_t color;
  char text[SAMP_RAKNET_CLIENT_MESSAGE_BYTES];
} samp_raknet_client_message_probe;

#pragma pack(push, 1)
typedef struct samp_raknet_textdraw_transmit {
  float letter_width;
  float letter_height;
  uint32_t letter_color;
  float line_width;
  float line_height;
  uint32_t box_color;
  uint8_t flags;
  uint8_t shadow;
  uint8_t outline;
  uint32_t background_color;
  uint8_t style;
  float x;
  float y;
  uint8_t selectable;
  uint16_t preview_model;
  float preview_rotation[3];
  float preview_zoom;
  int16_t preview_color1;
  int16_t preview_color2;
} samp_raknet_textdraw_transmit;
#pragma pack(pop)

typedef struct samp_raknet_textdraw_event {
  uint32_t seq;
  uint8_t action;
  uint16_t textdraw_id;
  samp_raknet_textdraw_transmit transmit;
  char text[SAMP_RAKNET_TEXTDRAW_TEXT_BYTES];
} samp_raknet_textdraw_event;

typedef struct samp_raknet_object_event {
  uint32_t seq;
  uint8_t action;
  uint8_t has_pos;
  uint8_t has_attachment;
  uint8_t materials_count;
  uint16_t object_id;
  int32_t model;
  float pos[3];
  float rot[3];
  float move_from[3];
  float move_to[3];
  float move_speed;
} samp_raknet_object_event;

typedef struct samp_raknet_vehicle_event {
  uint32_t seq;
  uint8_t action;
  uint8_t color1;
  uint8_t color2;
  uint8_t interior;
  uint8_t paintjob;
  uint8_t light_damage;
  uint8_t tyre_damage;
  uint8_t siren;
  uint16_t vehicle_id;
  int32_t model;
  float pos[3];
  float rotation;
  float health;
  uint32_t door_damage;
  uint32_t panel_damage;
  uint8_t mods[SAMP_RAKNET_VEHICLE_MOD_SLOTS];
  int32_t body_color1;
  int32_t body_color2;
} samp_raknet_vehicle_event;

typedef struct samp_raknet_rpc_probe_snapshot {
  uint32_t flags;
  uint32_t client_message_count;
  uint32_t textdraw_event_count;
  uint32_t object_event_count;
  uint32_t vehicle_event_count;
  uint8_t textdraw_select_active;
  uint32_t textdraw_select_color;
  uint16_t init_spawns_available;
  uint16_t init_local_player_id;
  uint8_t init_show_player_tags;
  uint8_t init_show_player_markers;
  uint8_t init_tire_popping;
  uint8_t init_world_time;
  uint8_t init_weather;
  float init_gravity;
  uint8_t init_lan_mode;
  int32_t init_death_drop_money;
  uint8_t init_instagib;
  uint8_t init_zone_names;
  uint8_t init_use_cj_walk;
  uint8_t init_allow_weapons;
  uint8_t init_limit_global_chat_radius;
  float init_global_chat_radius;
  uint8_t init_stunt_bonus;
  float init_name_tag_draw_distance;
  uint8_t init_disable_enter_exits;
  uint8_t init_name_tag_los;
  int32_t init_send_rates[6];
  char init_hostname[SAMP_RAKNET_HOSTNAME_BYTES];
  uint8_t init_vehicle_models[SAMP_RAKNET_REQUIRED_VEHICLE_MODELS];
  uint8_t request_class_outcome;
  uint8_t request_spawn_outcome;
  uint16_t last_dialog_id;
  uint8_t last_dialog_style;
  char dialog_title[SAMP_RAKNET_DIALOG_TITLE_BYTES];
  char dialog_info[SAMP_RAKNET_DIALOG_INFO_BYTES];
  char dialog_button1[SAMP_RAKNET_DIALOG_BUTTON_BYTES];
  char dialog_button2[SAMP_RAKNET_DIALOG_BUTTON_BYTES];
  float player_pos[3];
  float player_facing_angle;
  uint32_t player_pos_seq;
  uint32_t player_facing_seq;
  uint8_t weather;
  uint8_t interior;
  float camera_pos[3];
  float camera_look_at[3];
  uint8_t camera_look_at_type;
  uint8_t spawn_team;
  uint32_t spawn_info_seq;
  int32_t spawn_skin;
  float spawn_pos[3];
  float spawn_rotation;
  int32_t spawn_weapons[3];
  int32_t spawn_weapon_ammo[3];
  samp_raknet_client_message_probe client_messages[SAMP_RAKNET_CLIENT_MESSAGE_RING];
  samp_raknet_textdraw_event textdraw_events[SAMP_RAKNET_TEXTDRAW_EVENT_RING];
  samp_raknet_object_event object_events[SAMP_RAKNET_OBJECT_EVENT_RING];
  samp_raknet_vehicle_event vehicle_events[SAMP_RAKNET_VEHICLE_EVENT_RING];
} samp_raknet_rpc_probe_snapshot;

/*
 * Drains up to max_packets from Receive()/DeallocatePacket.
 * Returns number of drained packets, or -1 on invalid args.
 */
int samp_raknet_client_drain_packets(void *client, int max_packets);

/*
 * Drains up to max_packets and performs minimal SA:MP client-join bootstrap:
 * on ID_CONNECTION_REQUEST_ACCEPTED it sends RPC_ClientJoin once using profile.
 * out_connected/out_join_sent/out_last_packet_id are optional.
 * Returns number of drained packets, or -1 on invalid args.
 */
int samp_raknet_client_drain_packets_autojoin(void *client, int max_packets, const samp_raknet_join_profile *profile,
                                              int *out_connected, int *out_join_sent, int *out_last_packet_id);

int samp_raknet_client_get_rpc_probe_snapshot(void *client, samp_raknet_rpc_probe_snapshot *out_snapshot);
int samp_raknet_client_queue_dialog_response(void *client, uint16_t dialog_id, uint8_t button, int16_t listitem,
                                             const char *input);

#ifdef __cplusplus
}
#endif

#endif
