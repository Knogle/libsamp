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
int samp_raknet_client_request_class_delta(void *client, int delta);

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

typedef struct samp_raknet_incar_sync {
  uint16_t vehicle_id;
  uint16_t left_right_keys;
  uint16_t up_down_keys;
  uint16_t keys;
  float quaternion[4];
  float position[3];
  float move_speed[3];
  float vehicle_health;
  uint8_t player_health;
  uint8_t player_armour;
  uint8_t additional_key_weapon;
  uint8_t siren;
  uint8_t landing_gear;
  uint16_t trailer_id;
  uint32_t hydra_thrust_angle;
} samp_raknet_incar_sync;

typedef struct samp_raknet_aim_sync {
  uint8_t camera_mode;
  float camera_front[3];
  float camera_position[3];
  float aim_z;
  uint8_t zoom_weapon_state;
  uint8_t aspect_ratio;
} samp_raknet_aim_sync;

typedef struct samp_raknet_bullet_sync {
  uint8_t hit_type;
  uint16_t hit_id;
  float origin[3];
  float hit_position[3];
  float offset[3];
  uint8_t weapon_id;
} samp_raknet_bullet_sync;
#pragma pack(pop)

int samp_raknet_client_send_onfoot_sync(void *client, const samp_raknet_onfoot_sync *sync);
int samp_raknet_client_send_incar_sync(void *client, const samp_raknet_incar_sync *sync);
int samp_raknet_client_send_aim_sync(void *client, const samp_raknet_aim_sync *sync);
int samp_raknet_client_send_bullet_sync(void *client, const samp_raknet_bullet_sync *sync);
int samp_raknet_client_send_give_take_damage(void *client, uint8_t taking, uint16_t player_id, float damage,
                                             uint32_t weapon_id, uint32_t bodypart);

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
#define SAMP_RAKNET_RPC_FLAG_WORLD_TIME 0x00080000u
#define SAMP_RAKNET_RPC_FLAG_SET_TIME_EX 0x00100000u
#define SAMP_RAKNET_RPC_FLAG_TOGGLE_CLOCK 0x00200000u
#define SAMP_RAKNET_RPC_FLAG_PLAYER_HEALTH 0x00400000u
#define SAMP_RAKNET_RPC_FLAG_PLAYER_CONTROLLABLE 0x00800000u
#define SAMP_RAKNET_RPC_FLAG_CAMERA_BEHIND 0x01000000u
#define SAMP_RAKNET_RPC_FLAG_PLAYER_SCRIPT_EVENT 0x02000000u
#define SAMP_RAKNET_RPC_FLAG_WORLD_VISUAL_EVENT 0x04000000u
#define SAMP_RAKNET_RPC_FLAG_PLAYER_POOL_EVENT 0x08000000u
#define SAMP_RAKNET_RPC_FLAG_SCOREBOARD_UPDATE 0x10000000u
#define SAMP_RAKNET_RPC_FLAG_AUTH_LOCAL_PLAYER 0x20000000u
#define SAMP_RAKNET_RPC_FLAG_REMOTE_PLAYER_EVENT 0x40000000u
#define SAMP_RAKNET_RPC_FLAG_REMOTE_PLAYER_SYNC 0x80000000u

#define SAMP_RAKNET_CLIENT_MESSAGE_RING 8u
#define SAMP_RAKNET_GIVE_WEAPON_EVENT_RING 16u
#define SAMP_RAKNET_CLIENT_MESSAGE_BYTES 256u
#define SAMP_RAKNET_HOSTNAME_BYTES 256u
#define SAMP_RAKNET_MAX_PLAYERS 1000u
#define SAMP_RAKNET_PLAYER_NAME_BYTES 25u
#define SAMP_RAKNET_PLAYER_POOL_EVENT_RING 64u
#define SAMP_RAKNET_SCORE_PING_MAX_ENTRIES SAMP_RAKNET_MAX_PLAYERS
#define SAMP_RAKNET_PLAYER_POOL_ACTION_JOIN 1u
#define SAMP_RAKNET_PLAYER_POOL_ACTION_QUIT 2u
#define SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING 64u
#define SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING 128u
#define SAMP_RAKNET_REMOTE_PLAYER_ACTION_ADD 1u
#define SAMP_RAKNET_REMOTE_PLAYER_ACTION_REMOVE 2u
#define SAMP_RAKNET_REMOTE_PLAYER_ACTION_DEATH 3u
#define SAMP_RAKNET_MAP_ICON_EVENT_RING 64u
#define SAMP_RAKNET_MAP_ICON_MAX 32u
#define SAMP_RAKNET_MAP_ICON_ACTION_SET 1u
#define SAMP_RAKNET_MAP_ICON_ACTION_REMOVE 2u
#define SAMP_RAKNET_NAME_TAG_EVENT_RING 64u
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
#define SAMP_RAKNET_OBJECT_ACTION_ATTACH_PLAYER 7u

#define SAMP_RAKNET_OBJECT_ATTACH_NONE 0u
#define SAMP_RAKNET_OBJECT_ATTACH_VEHICLE 1u
#define SAMP_RAKNET_OBJECT_ATTACH_OBJECT 2u
#define SAMP_RAKNET_OBJECT_ATTACH_PLAYER 3u
#define SAMP_RAKNET_VEHICLE_EVENT_RING 128u
#define SAMP_RAKNET_MAX_VEHICLES 2000u
#define SAMP_RAKNET_VEHICLE_MOD_SLOTS 14u
#define SAMP_RAKNET_VEHICLE_ACTION_CREATE 1u
#define SAMP_RAKNET_VEHICLE_ACTION_DESTROY 2u
#define SAMP_RAKNET_VEHICLE_ACTION_SET_HEALTH 3u
#define SAMP_RAKNET_VEHICLE_ACTION_PUT_LOCAL_PLAYER 4u
#define SAMP_RAKNET_ANIM_LIB_BYTES 64u
#define SAMP_RAKNET_ANIM_NAME_BYTES 64u
#define SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES 256u
#define SAMP_RAKNET_WORLD_VISUAL_NAME_BYTES 64u
#define SAMP_RAKNET_WORLD_VISUAL_SET_OBJECT_MATERIAL 1u
#define SAMP_RAKNET_WORLD_VISUAL_CREATE_3D_TEXT_LABEL 2u
#define SAMP_RAKNET_WORLD_VISUAL_GANG_ZONE_CREATE 3u
#define SAMP_RAKNET_WORLD_VISUAL_REMOVE_BUILDING 4u

typedef struct samp_raknet_client_message_probe {
  uint32_t seq;
  uint32_t color;
  char text[SAMP_RAKNET_CLIENT_MESSAGE_BYTES];
} samp_raknet_client_message_probe;

typedef struct samp_raknet_player_pool_event {
  uint32_t seq;
  uint8_t action;
  uint8_t is_npc;
  uint8_t reason;
  uint16_t player_id;
  uint32_t color;
  char name[SAMP_RAKNET_PLAYER_NAME_BYTES];
} samp_raknet_player_pool_event;

typedef struct samp_raknet_score_ping_entry {
  uint16_t player_id;
  int32_t score;
  uint32_t ping;
} samp_raknet_score_ping_entry;

typedef struct samp_raknet_remote_player_event {
  uint32_t seq;
  uint8_t action;
  uint8_t team;
  uint8_t fighting_style;
  uint8_t visible;
  uint8_t death_reason;
  uint16_t player_id;
  int32_t skin;
  uint32_t color;
  float pos[3];
  float rotation;
} samp_raknet_remote_player_event;

typedef struct samp_raknet_remote_onfoot_sync {
  uint32_t seq;
  uint16_t player_id;
  uint16_t left_right_keys;
  uint16_t up_down_keys;
  uint16_t keys;
  uint8_t health;
  uint8_t armour;
  uint8_t current_weapon;
  uint8_t special_action;
  uint16_t surfing_vehicle_id;
  float position[3];
  float rotation;
  float move_speed[3];
  float surfing_offsets[3];
} samp_raknet_remote_onfoot_sync;

typedef struct samp_raknet_map_icon_event {
  uint32_t seq;
  uint8_t action;
  uint8_t index;
  uint8_t icon;
  uint8_t reserved;
  uint32_t color;
  float pos[3];
} samp_raknet_map_icon_event;

typedef struct samp_raknet_name_tag_event {
  uint32_t seq;
  uint16_t player_id;
  uint8_t show;
  uint8_t reserved;
} samp_raknet_name_tag_event;

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
  uint8_t attachment_type;
  uint8_t attachment_sync_rotation;
  uint16_t object_id;
  uint16_t attachment_parent_id;
  int32_t model;
  float pos[3];
  float rot[3];
  float attachment_offset[3];
  float attachment_rot[3];
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
  uint8_t seat_id;
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

typedef struct samp_raknet_given_weapon_event {
  uint32_t seq;
  uint32_t weapon;
  uint32_t ammo;
} samp_raknet_given_weapon_event;

typedef struct samp_raknet_rpc_probe_snapshot {
  uint32_t flags;
  uint32_t client_message_count;
  uint32_t textdraw_event_count;
  uint32_t object_event_count;
  uint32_t vehicle_event_count;
  uint32_t player_pool_event_count;
  uint32_t score_ping_count;
  uint32_t remote_player_event_count;
  uint32_t remote_player_sync_count;
  uint32_t map_icon_event_count;
  uint32_t name_tag_event_count;
  uint32_t given_weapon_event_count;
  uint8_t textdraw_select_active;
  uint32_t textdraw_select_color;
  uint16_t init_spawns_available;
  uint16_t init_local_player_id;
  uint8_t auth_local_player_id_valid;
  uint16_t auth_local_player_id;
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
  float player_health;
  uint32_t player_pos_seq;
  uint32_t player_facing_seq;
  uint32_t player_health_seq;
  uint32_t player_controllable_seq;
  uint32_t camera_behind_seq;
  uint32_t player_armour_seq;
  uint32_t player_armed_weapon_seq;
  uint32_t player_given_weapon_seq;
  uint32_t reset_player_weapons_seq;
  uint32_t reset_player_money_seq;
  uint32_t play_sound_seq;
  uint32_t stop_audio_stream_seq;
  uint32_t player_color_seq;
  uint32_t player_team_seq;
  uint32_t apply_animation_seq;
  uint32_t world_visual_event_seq;
  uint32_t player_pool_event_seq;
  uint32_t score_ping_seq;
  uint8_t player_controllable;
  float player_armour;
  uint32_t player_armed_weapon;
  uint32_t player_given_weapon;
  uint32_t player_given_weapon_ammo;
  uint32_t play_sound_id;
  float play_sound_pos[3];
  uint16_t player_color_player_id;
  uint32_t player_color;
  uint16_t player_team_player_id;
  uint8_t player_team;
  uint16_t apply_animation_player_id;
  char apply_animation_lib[SAMP_RAKNET_ANIM_LIB_BYTES];
  char apply_animation_name[SAMP_RAKNET_ANIM_NAME_BYTES];
  float apply_animation_delta;
  uint8_t apply_animation_loop;
  uint8_t apply_animation_lock_x;
  uint8_t apply_animation_lock_y;
  uint8_t apply_animation_freeze;
  int32_t apply_animation_time;
  uint8_t world_visual_event_type;
  uint16_t world_visual_id;
  int32_t world_visual_model;
  uint32_t world_visual_color;
  uint32_t world_visual_material_background;
  float world_visual_pos[4];
  uint8_t world_visual_material_type;
  uint8_t world_visual_material_slot;
  uint8_t world_visual_material_size;
  uint8_t world_visual_material_font_size;
  uint8_t world_visual_material_bold;
  uint8_t world_visual_material_alignment;
  char world_visual_text[SAMP_RAKNET_WORLD_VISUAL_TEXT_BYTES];
  char world_visual_material_txd[SAMP_RAKNET_WORLD_VISUAL_NAME_BYTES];
  char world_visual_material_texture[SAMP_RAKNET_WORLD_VISUAL_NAME_BYTES];
  uint8_t weather;
  uint8_t world_time_hour;
  uint8_t world_time_minute;
  uint8_t clock_enabled;
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
  samp_raknet_player_pool_event player_pool_events[SAMP_RAKNET_PLAYER_POOL_EVENT_RING];
  samp_raknet_score_ping_entry score_ping_entries[SAMP_RAKNET_SCORE_PING_MAX_ENTRIES];
  samp_raknet_remote_player_event remote_player_events[SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING];
  samp_raknet_remote_onfoot_sync remote_player_syncs[SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING];
  samp_raknet_map_icon_event map_icon_events[SAMP_RAKNET_MAP_ICON_EVENT_RING];
  samp_raknet_name_tag_event name_tag_events[SAMP_RAKNET_NAME_TAG_EVENT_RING];
  samp_raknet_given_weapon_event given_weapon_events[SAMP_RAKNET_GIVE_WEAPON_EVENT_RING];
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
