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
int samp_raknet_client_send_rcon_command(void *client, const char *command);
int samp_raknet_client_send_spawn_notification(void *client);
int samp_raknet_client_send_spawn_notification_for_seq(void *client, uint32_t spawn_info_seq);
int samp_raknet_client_send_death_notification(void *client, uint8_t death_reason, uint8_t responsible_player);
int samp_raknet_client_send_textdraw_click(void *client, uint16_t textdraw_id);
int samp_raknet_client_send_menu_select(void *client, uint8_t row);
int samp_raknet_client_send_menu_quit(void *client);
int samp_raknet_client_mark_class_selection_after_death(void *client);
int samp_raknet_client_request_class_selection_after_death(void *client);
int samp_raknet_client_request_class_delta(void *client, int delta);
int samp_raknet_client_request_spawn(void *client);

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

typedef struct samp_raknet_spectator_sync {
  uint16_t left_right_keys;
  uint16_t up_down_keys;
  uint16_t keys;
  float position[3];
} samp_raknet_spectator_sync;
#pragma pack(pop)

int samp_raknet_client_send_onfoot_sync(void *client, const samp_raknet_onfoot_sync *sync);
int samp_raknet_client_send_incar_sync(void *client, const samp_raknet_incar_sync *sync);
int samp_raknet_client_send_aim_sync(void *client, const samp_raknet_aim_sync *sync);
int samp_raknet_client_send_bullet_sync(void *client, const samp_raknet_bullet_sync *sync);
int samp_raknet_client_send_spectator_sync(void *client, const samp_raknet_spectator_sync *sync);
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
#define SAMP_RAKNET_GIVE_MONEY_EVENT_RING 16u
#define SAMP_RAKNET_PICKUP_EVENT_RING 64u
#define SAMP_RAKNET_EXPLOSION_EVENT_RING 16u
#define SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING 32u
#define SAMP_RAKNET_CHAT_BUBBLE_TEXT_BYTES 129u
#define SAMP_RAKNET_MAX_MENUS 128u
#define SAMP_RAKNET_MENU_MAX_COLUMNS 2u
#define SAMP_RAKNET_MENU_MAX_ITEMS 12u
#define SAMP_RAKNET_MENU_LINE_BYTES 32u
#define SAMP_RAKNET_MENU_ACTION_INIT 1u
#define SAMP_RAKNET_MENU_ACTION_SHOW 2u
#define SAMP_RAKNET_MENU_ACTION_HIDE 3u
#define SAMP_RAKNET_AUDIO_STREAM_URL_BYTES 129u
#define SAMP_RAKNET_SHOP_NAME_BYTES 33u
#define SAMP_RAKNET_PICKUP_ACTION_CREATE 1u
#define SAMP_RAKNET_PICKUP_ACTION_DESTROY 2u
#define SAMP_RAKNET_CLIENT_MESSAGE_BYTES 256u
#define SAMP_RAKNET_HOSTNAME_BYTES 256u
#define SAMP_RAKNET_MAX_PLAYERS 1000u
#define SAMP_RAKNET_PLAYER_NAME_BYTES 25u
#define SAMP_RAKNET_PLAYER_POOL_EVENT_RING 64u
#define SAMP_RAKNET_SCORE_PING_MAX_ENTRIES SAMP_RAKNET_MAX_PLAYERS
#define SAMP_RAKNET_PLAYER_POOL_ACTION_JOIN 1u
#define SAMP_RAKNET_PLAYER_POOL_ACTION_QUIT 2u
#define SAMP_RAKNET_PLAYER_POOL_ACTION_RENAME 3u
#define SAMP_RAKNET_PLAYER_POOL_ACTION_TEAM 4u
#define SAMP_RAKNET_PLAYER_POOL_ACTION_COLOR 5u
#define SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING 64u
#define SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING 128u
#define SAMP_RAKNET_REMOTE_PLAYER_ACTION_ADD 1u
#define SAMP_RAKNET_REMOTE_PLAYER_ACTION_REMOVE 2u
#define SAMP_RAKNET_REMOTE_PLAYER_ACTION_DEATH 3u
#define SAMP_RAKNET_MAP_ICON_EVENT_RING 128u
#define SAMP_RAKNET_MAP_ICON_MAX 100u
#define SAMP_RAKNET_MAP_ICON_ACTION_SET 1u
#define SAMP_RAKNET_MAP_ICON_ACTION_REMOVE 2u
#define SAMP_RAKNET_GANG_ZONE_EVENT_RING 128u
#define SAMP_RAKNET_MAX_GANG_ZONES 1024u
#define SAMP_RAKNET_GANG_ZONE_ACTION_CREATE 1u
#define SAMP_RAKNET_GANG_ZONE_ACTION_DESTROY 2u
#define SAMP_RAKNET_GANG_ZONE_ACTION_FLASH 3u
#define SAMP_RAKNET_GANG_ZONE_ACTION_STOP_FLASH 4u
#define SAMP_RAKNET_MAX_ACTORS 1000u
#define SAMP_RAKNET_ACTOR_EVENT_RING 128u
#define SAMP_RAKNET_ACTOR_ANIM_LIB_BYTES 256u
#define SAMP_RAKNET_ACTOR_ANIM_NAME_BYTES 256u
/* OBSERVED_037 + PROBE_TRACE + STATIC_037:
 * Incoming actor RPCs 171/172/173/174/175/176/178 are kept in receive order.
 * These action values are internal adapter ABI, not wire protocol IDs. */
#define SAMP_RAKNET_ACTOR_ACTION_CREATE 1u
#define SAMP_RAKNET_ACTOR_ACTION_DESTROY 2u
#define SAMP_RAKNET_ACTOR_ACTION_APPLY_ANIMATION 3u
#define SAMP_RAKNET_ACTOR_ACTION_CLEAR_ANIMATIONS 4u
#define SAMP_RAKNET_ACTOR_ACTION_SET_FACING 5u
#define SAMP_RAKNET_ACTOR_ACTION_SET_POSITION 6u
#define SAMP_RAKNET_ACTOR_ACTION_SET_HEALTH 7u
#define SAMP_RAKNET_NAME_TAG_EVENT_RING 64u
#define SAMP_RAKNET_DEATH_WINDOW_EVENT_RING 16u
#define SAMP_RAKNET_DEATH_WINDOW_MAX_ENTRIES 5u
#define SAMP_RAKNET_DEATH_WINDOW_ACTION_ADD 1u
#define SAMP_RAKNET_DEATH_WINDOW_ACTION_CLEAR 2u
#define SAMP_RAKNET_3D_TEXT_LABEL_EVENT_RING 128u
#define SAMP_RAKNET_MAX_3D_TEXT_LABELS 1024u
#define SAMP_RAKNET_3D_TEXT_LABEL_TEXT_BYTES 512u
#define SAMP_RAKNET_3D_TEXT_LABEL_ACTION_CREATE 1u
#define SAMP_RAKNET_3D_TEXT_LABEL_ACTION_UPDATE 2u
#define SAMP_RAKNET_3D_TEXT_LABEL_ACTION_DELETE 3u
#define SAMP_RAKNET_REQUIRED_VEHICLE_MODELS 212u
#define SAMP_RAKNET_DIALOG_TITLE_BYTES 256u
#define SAMP_RAKNET_DIALOG_INFO_BYTES 4096u
#define SAMP_RAKNET_DIALOG_BUTTON_BYTES 64u
#define SAMP_RAKNET_DIALOG_INPUT_BYTES 256u
#define SAMP_RAKNET_TEXTDRAW_EVENT_RING 64u
#define SAMP_RAKNET_TEXTDRAW_TEXT_BYTES 256u
#define SAMP_RAKNET_GAMETEXT_TEXT_BYTES 512u
#define SAMP_RAKNET_GAMETEXT_EVENT_RING 16u
#define SAMP_RAKNET_GAMETEXT_MAX_STYLES 7u
#define SAMP_RAKNET_GAMETEXT_ACTION_SHOW 1u
#define SAMP_RAKNET_GAMETEXT_ACTION_HIDE 2u
#define SAMP_RAKNET_MAX_TEXTDRAWS 4096u
#define SAMP_RAKNET_TEXTDRAW_ACTION_SHOW 1u
#define SAMP_RAKNET_TEXTDRAW_ACTION_HIDE 2u
#define SAMP_RAKNET_TEXTDRAW_ACTION_EDIT 3u
/* Compact descriptors only. RakPeer::Receive may dispatch more RPCs than the
 * outer packet budget in one call, so lifecycle/material stores remain the
 * authoritative recovery path if this diagnostic/order ring wraps. */
#define SAMP_RAKNET_OBJECT_EVENT_RING 1024u
#define SAMP_RAKNET_MAX_OBJECTS 2000u
#define SAMP_RAKNET_OBJECT_ACTION_CREATE 1u
#define SAMP_RAKNET_OBJECT_ACTION_DESTROY 2u
#define SAMP_RAKNET_OBJECT_ACTION_SET_POS 3u
#define SAMP_RAKNET_OBJECT_ACTION_SET_ROT 4u
#define SAMP_RAKNET_OBJECT_ACTION_MOVE 5u
#define SAMP_RAKNET_OBJECT_ACTION_STOP 6u
#define SAMP_RAKNET_OBJECT_ACTION_ATTACH_PLAYER 7u
#define SAMP_RAKNET_OBJECT_ACTION_SET_MATERIAL 8u

#define SAMP_RAKNET_OBJECT_MATERIAL_SLOTS 16u
#define SAMP_RAKNET_OBJECT_MATERIAL_TYPE_DEFAULT 1u
#define SAMP_RAKNET_OBJECT_MATERIAL_TYPE_TEXT 2u
#define SAMP_RAKNET_OBJECT_MATERIAL_SOURCE_CREATE 1u
#define SAMP_RAKNET_OBJECT_MATERIAL_SOURCE_RPC 2u
#define SAMP_RAKNET_OBJECT_MATERIAL_NAME_BYTES 256u
#define SAMP_RAKNET_OBJECT_MATERIAL_TEXT_BYTES 2049u

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
#define SAMP_RAKNET_VEHICLE_ACTION_SET_POS 5u
#define SAMP_RAKNET_VEHICLE_ACTION_SET_Z_ANGLE 6u
#define SAMP_RAKNET_VEHICLE_ACTION_ATTACH_TRAILER 7u
#define SAMP_RAKNET_VEHICLE_ACTION_DETACH_TRAILER 8u
#define SAMP_RAKNET_VEHICLE_ACTION_SET_PARAMS_EX 9u
#define SAMP_RAKNET_VEHICLE_ACTION_REMOVE_COMPONENT 10u
#define SAMP_RAKNET_VEHICLE_ACTION_LINK_INTERIOR 11u
#define SAMP_RAKNET_VEHICLE_ACTION_SET_NUMBER_PLATE 12u
#define SAMP_RAKNET_VEHICLE_ACTION_SET_PARAMS_FOR_PLAYER 13u
#define SAMP_RAKNET_VEHICLE_PARAM_BYTES 16u
#define SAMP_RAKNET_VEHICLE_NUMBER_PLATE_BYTES 33u
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
  uint8_t style;
  uint32_t color;
  float pos[3];
} samp_raknet_map_icon_event;

typedef struct samp_raknet_gang_zone_event {
  uint32_t seq;
  uint16_t zone_id;
  uint8_t action;
  uint8_t reserved;
  float bounds[4];
  uint32_t color;
} samp_raknet_gang_zone_event;

typedef struct samp_raknet_gang_zone_state {
  uint32_t revision;
  uint8_t active;
  uint8_t reserved[3];
  float bounds[4];
  uint32_t base_color;
  uint32_t alternate_color;
} samp_raknet_gang_zone_state;

typedef struct samp_raknet_actor_event {
  uint32_t seq;
  uint16_t actor_id;
  uint8_t action;
  uint8_t invulnerable;
  int32_t skin;
  float pos[3];
  float rotation;
  float health;
  float animation_delta;
  int32_t animation_time;
  uint8_t animation_loop;
  uint8_t animation_lock_x;
  uint8_t animation_lock_y;
  uint8_t animation_freeze;
  char animation_lib[SAMP_RAKNET_ACTOR_ANIM_LIB_BYTES];
  char animation_name[SAMP_RAKNET_ACTOR_ANIM_NAME_BYTES];
} samp_raknet_actor_event;

/* Authoritative receive-side actor state. Keep this out of the aggregate
 * snapshot: callers use actor_state_seq to detect a ring gap and then query
 * only the 1000 bounded actor slots needed for resynchronization. */
typedef struct samp_raknet_actor_state {
  uint32_t revision;
  uint8_t active;
  uint8_t invulnerable;
  uint8_t has_animation;
  uint8_t reserved;
  int32_t skin;
  float pos[3];
  float rotation;
  float health;
  float animation_delta;
  int32_t animation_time;
  uint8_t animation_loop;
  uint8_t animation_lock_x;
  uint8_t animation_lock_y;
  uint8_t animation_freeze;
  char animation_lib[SAMP_RAKNET_ACTOR_ANIM_LIB_BYTES];
  char animation_name[SAMP_RAKNET_ACTOR_ANIM_NAME_BYTES];
} samp_raknet_actor_state;

typedef struct samp_raknet_name_tag_event {
  uint32_t seq;
  uint16_t player_id;
  uint8_t show;
  uint8_t reserved;
} samp_raknet_name_tag_event;

typedef struct samp_raknet_death_window_event {
  uint32_t seq;
  uint8_t action;
  uint8_t reason;
  uint16_t killer_id;
  uint16_t killee_id;
  uint16_t reserved;
} samp_raknet_death_window_event;

typedef struct samp_raknet_game_text_event {
  uint32_t seq;
  uint8_t action;
  uint8_t reserved[3];
  int32_t style;
  int32_t time_ms;
  char text[SAMP_RAKNET_GAMETEXT_TEXT_BYTES];
} samp_raknet_game_text_event;

typedef struct samp_raknet_3d_text_label_event {
  uint32_t seq;
  uint8_t action;
  uint8_t test_los;
  uint16_t label_id;
  uint16_t attached_player_id;
  uint16_t attached_vehicle_id;
  uint32_t color;
  float pos[3];
  float draw_distance;
  char text[SAMP_RAKNET_3D_TEXT_LABEL_TEXT_BYTES];
} samp_raknet_3d_text_label_event;

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

typedef struct samp_raknet_object_material {
  uint8_t type;
  uint8_t slot;
  uint8_t source;
  uint8_t material_size;
  uint8_t font_size;
  uint8_t bold;
  uint8_t alignment;
  uint8_t reserved;
  int32_t model;
  uint32_t material_color;
  uint32_t background_color;
  char txd[SAMP_RAKNET_OBJECT_MATERIAL_NAME_BYTES];
  char texture[SAMP_RAKNET_OBJECT_MATERIAL_NAME_BYTES];
  char font[SAMP_RAKNET_OBJECT_MATERIAL_NAME_BYTES];
  char text[SAMP_RAKNET_OBJECT_MATERIAL_TEXT_BYTES];
} samp_raknet_object_material;

typedef struct samp_raknet_object_event {
  uint32_t seq;
  uint32_t object_generation;
  uint32_t material_revision;
  uint8_t action;
  uint8_t has_pos;
  uint8_t has_attachment;
  uint8_t materials_count;
  uint8_t attachment_type;
  uint8_t attachment_sync_rotation;
  uint8_t material_type;
  uint8_t material_slot;
  uint8_t material_source;
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
  float draw_distance;
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
  uint16_t related_vehicle_id;
  int32_t model;
  float pos[3];
  float rotation;
  float health;
  uint32_t door_damage;
  uint32_t panel_damage;
  uint8_t mods[SAMP_RAKNET_VEHICLE_MOD_SLOTS];
  uint8_t params[SAMP_RAKNET_VEHICLE_PARAM_BYTES];
  int32_t component;
  int32_t body_color1;
  int32_t body_color2;
  char number_plate[SAMP_RAKNET_VEHICLE_NUMBER_PLATE_BYTES];
} samp_raknet_vehicle_event;

typedef struct samp_raknet_given_weapon_event {
  uint32_t seq;
  uint32_t weapon;
  uint32_t ammo;
} samp_raknet_given_weapon_event;

typedef struct samp_raknet_give_money_event {
  uint32_t seq;
  int32_t amount;
} samp_raknet_give_money_event;

typedef struct samp_raknet_pickup_event {
  uint32_t seq;
  uint8_t action;
  int32_t pickup_id;
  int32_t model;
  int32_t type;
  float pos[3];
} samp_raknet_pickup_event;

typedef struct samp_raknet_explosion_event {
  uint32_t seq;
  int32_t type;
  float pos[3];
  float radius;
} samp_raknet_explosion_event;

typedef struct samp_raknet_chat_bubble_event {
  uint32_t seq;
  uint16_t player_id;
  uint16_t reserved;
  uint32_t color;
  uint32_t duration_ms;
  float draw_distance;
  char text[SAMP_RAKNET_CHAT_BUBBLE_TEXT_BYTES];
} samp_raknet_chat_bubble_event;

typedef struct samp_raknet_menu_state {
  uint8_t valid;
  uint8_t menu_id;
  uint8_t columns;
  uint8_t interaction_enabled;
  uint8_t row_enabled[SAMP_RAKNET_MENU_MAX_ITEMS];
  uint8_t column_item_count[SAMP_RAKNET_MENU_MAX_COLUMNS];
  uint8_t reserved[2];
  float x;
  float y;
  float column_width[SAMP_RAKNET_MENU_MAX_COLUMNS];
  char title[SAMP_RAKNET_MENU_LINE_BYTES];
  char column_header[SAMP_RAKNET_MENU_MAX_COLUMNS][SAMP_RAKNET_MENU_LINE_BYTES];
  char items[SAMP_RAKNET_MENU_MAX_COLUMNS][SAMP_RAKNET_MENU_MAX_ITEMS][SAMP_RAKNET_MENU_LINE_BYTES];
} samp_raknet_menu_state;

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
  uint32_t gang_zone_event_count;
  uint32_t gang_zone_state_seq;
  uint32_t actor_event_count;
  uint32_t actor_state_seq;
  uint32_t name_tag_event_count;
  uint32_t death_window_event_count;
  uint32_t game_text_event_count;
  uint32_t text_label_event_count;
  uint32_t given_weapon_event_count;
  uint32_t give_money_event_count;
  uint32_t pickup_event_count;
  uint32_t explosion_event_count;
  uint32_t chat_bubble_event_count;
  uint32_t menu_event_seq;
  uint8_t menu_event_action;
  uint8_t menu_active;
  uint8_t menu_event_id;
  uint8_t menu_reserved;
  samp_raknet_menu_state menu;
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
  uint32_t give_player_money_seq;
  uint32_t player_ammo_seq;
  uint32_t player_skin_seq;
  uint32_t player_skill_seq;
  uint32_t player_drunk_seq;
  uint32_t player_fighting_style_seq;
  uint32_t player_pos_find_z_seq;
  uint32_t player_velocity_seq;
  uint32_t remove_player_from_vehicle_seq;
  uint32_t clear_animations_seq;
  uint32_t vehicle_velocity_seq;
  uint32_t stunt_bonus_seq;
  uint32_t checkpoint_seq;
  uint32_t disable_checkpoint_seq;
  uint32_t race_checkpoint_seq;
  uint32_t disable_race_checkpoint_seq;
  uint32_t checkpoint_event_seq;
  uint32_t pickup_event_seq;
  uint32_t explosion_event_seq;
  uint32_t chat_bubble_event_seq;
  uint32_t cancel_edit_seq;
  uint32_t shop_name_seq;
  uint32_t play_audio_stream_seq;
  uint32_t play_sound_seq;
  uint32_t stop_audio_stream_seq;
  uint32_t player_color_seq;
  uint32_t player_team_seq;
  uint32_t apply_animation_seq;
  uint32_t player_wanted_level_seq;
  uint32_t gravity_seq;
  uint32_t world_bounds_seq;
  uint32_t game_mode_restart_seq;
  uint32_t force_class_selection_seq;
  uint32_t camera_attach_object_seq;
  uint32_t camera_interpolate_seq;
  uint32_t special_action_seq;
  uint32_t spectate_toggle_seq;
  uint32_t spectate_player_seq;
  uint32_t spectate_vehicle_seq;
  uint32_t world_visual_event_seq;
  uint32_t player_pool_event_seq;
  uint32_t score_ping_seq;
  uint32_t game_text_seq;
  int32_t game_text_style;
  int32_t game_text_time_ms;
  uint8_t player_controllable;
  float player_armour;
  uint32_t player_armed_weapon;
  uint32_t player_given_weapon;
  uint32_t player_given_weapon_ammo;
  int32_t give_player_money;
  uint8_t player_ammo_weapon;
  uint16_t player_ammo;
  uint32_t player_skin_player_id;
  int32_t player_skin;
  uint16_t player_skill_player_id;
  uint32_t player_skill;
  uint16_t player_skill_level;
  uint32_t player_drunk_level;
  uint16_t player_fighting_style_player_id;
  uint8_t player_fighting_style;
  float player_pos_find_z[3];
  float player_velocity[3];
  uint16_t clear_animations_player_id;
  uint8_t vehicle_velocity_turn;
  float vehicle_velocity[3];
  uint8_t stunt_bonus_enabled;
  float checkpoint_pos[3];
  float checkpoint_size;
  uint8_t race_checkpoint_type;
  float race_checkpoint_pos[3];
  float race_checkpoint_next[3];
  float race_checkpoint_size;
  uint8_t checkpoint_event_type;
  char shop_name[SAMP_RAKNET_SHOP_NAME_BYTES];
  char audio_stream_url[SAMP_RAKNET_AUDIO_STREAM_URL_BYTES];
  float audio_stream_pos[3];
  float audio_stream_distance;
  uint8_t audio_stream_use_pos;
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
  uint8_t player_wanted_level;
  float gravity;
  float world_bounds[4];
  uint16_t camera_object_id;
  uint8_t camera_interpolate_set_pos;
  uint8_t camera_interpolate_cut;
  float camera_interpolate_from[3];
  float camera_interpolate_to[3];
  int32_t camera_interpolate_time_ms;
  uint8_t special_action;
  int32_t spectate_toggle;
  uint16_t spectate_player_id;
  uint16_t spectate_vehicle_id;
  uint8_t spectate_player_mode;
  uint8_t spectate_vehicle_mode;
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
  char game_text[SAMP_RAKNET_GAMETEXT_TEXT_BYTES];
  samp_raknet_client_message_probe client_messages[SAMP_RAKNET_CLIENT_MESSAGE_RING];
  samp_raknet_textdraw_event textdraw_events[SAMP_RAKNET_TEXTDRAW_EVENT_RING];
  samp_raknet_object_event object_events[SAMP_RAKNET_OBJECT_EVENT_RING];
  samp_raknet_vehicle_event vehicle_events[SAMP_RAKNET_VEHICLE_EVENT_RING];
  samp_raknet_player_pool_event player_pool_events[SAMP_RAKNET_PLAYER_POOL_EVENT_RING];
  samp_raknet_score_ping_entry score_ping_entries[SAMP_RAKNET_SCORE_PING_MAX_ENTRIES];
  samp_raknet_remote_player_event remote_player_events[SAMP_RAKNET_REMOTE_PLAYER_EVENT_RING];
  samp_raknet_remote_onfoot_sync remote_player_syncs[SAMP_RAKNET_REMOTE_PLAYER_SYNC_RING];
  samp_raknet_map_icon_event map_icon_events[SAMP_RAKNET_MAP_ICON_EVENT_RING];
  samp_raknet_gang_zone_event gang_zone_events[SAMP_RAKNET_GANG_ZONE_EVENT_RING];
  samp_raknet_gang_zone_state gang_zone_states[SAMP_RAKNET_MAX_GANG_ZONES];
  samp_raknet_actor_event actor_events[SAMP_RAKNET_ACTOR_EVENT_RING];
  samp_raknet_name_tag_event name_tag_events[SAMP_RAKNET_NAME_TAG_EVENT_RING];
  samp_raknet_death_window_event death_window_events[SAMP_RAKNET_DEATH_WINDOW_EVENT_RING];
  samp_raknet_game_text_event game_text_events[SAMP_RAKNET_GAMETEXT_EVENT_RING];
  samp_raknet_3d_text_label_event text_label_events[SAMP_RAKNET_3D_TEXT_LABEL_EVENT_RING];
  samp_raknet_given_weapon_event given_weapon_events[SAMP_RAKNET_GIVE_WEAPON_EVENT_RING];
  samp_raknet_give_money_event give_money_events[SAMP_RAKNET_GIVE_MONEY_EVENT_RING];
  samp_raknet_pickup_event pickup_events[SAMP_RAKNET_PICKUP_EVENT_RING];
  samp_raknet_explosion_event explosion_events[SAMP_RAKNET_EXPLOSION_EVENT_RING];
  samp_raknet_chat_bubble_event chat_bubble_events[SAMP_RAKNET_CHAT_BUBBLE_EVENT_RING];
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
int samp_raknet_client_get_actor_state(void *client, uint16_t actor_id, samp_raknet_actor_state *out_state);
int samp_raknet_client_get_object_material(void *client, uint16_t object_id, uint32_t object_generation,
                                           uint8_t material_slot, uint32_t minimum_revision,
                                           samp_raknet_object_material *out_material,
                                           uint32_t *out_revision);
int samp_raknet_client_get_object_lifecycle(void *client, uint16_t object_id, uint8_t *out_alive,
                                            samp_raknet_object_event *out_create_event);
int samp_raknet_client_queue_dialog_response(void *client, uint16_t dialog_id, uint8_t button, int16_t listitem,
                                             const char *input);

#ifdef __cplusplus
}
#endif

#endif
