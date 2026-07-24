#include "sampdll/net/raknet_client_adapter.h"

#include "raknet_client_adapter_internal.h"

#include <string.h>

int samp_raknet_client_available(void) { return 0; }

int samp_raknet_client_create(void **out_client) {
  if (out_client != 0) {
    *out_client = 0;
  }
  return -1;
}

int samp_raknet_client_destroy(void *client) {
  (void)client;
  return 0;
}

int samp_raknet_client_connect(void *client, const char *host, uint16_t server_port, uint16_t client_port,
                               int thread_sleep_timer) {
  (void)client;
  (void)host;
  (void)server_port;
  (void)client_port;
  (void)thread_sleep_timer;
  return -1;
}

void samp_raknet_client_disconnect(void *client, unsigned int block_duration, unsigned char ordering_channel) {
  (void)client;
  (void)block_duration;
  (void)ordering_channel;
}

int samp_raknet_client_is_connected(void *client) {
  (void)client;
  return 0;
}

int samp_raknet_client_mark_logged_on(void *client) {
  (void)client;
  return -1;
}

int samp_raknet_client_send_chat(void *client, const char *text) {
  (void)client;
  (void)text;
  return -1;
}

int samp_raknet_client_send_server_command(void *client, const char *command) {
  (void)client;
  (void)command;
  return -1;
}

int samp_raknet_client_send_rcon_command(void *client, const char *command) {
  (void)client;
  (void)command;
  return -1;
}

int samp_raknet_client_send_textdraw_click(void *client, uint16_t textdraw_id) {
  (void)client;
  (void)textdraw_id;
  return -1;
}

int samp_raknet_client_send_player_click(void *client, uint16_t player_id, uint8_t source) {
  (void)client;
  (void)player_id;
  (void)source;
  return -1;
}

int samp_raknet_client_send_menu_select(void *client, uint8_t row) {
  (void)client;
  (void)row;
  return -1;
}

int samp_raknet_client_send_menu_quit(void *client) {
  (void)client;
  return -1;
}

int samp_raknet_client_mark_class_selection_after_death(void *client) {
  (void)client;
  return -1;
}

int samp_raknet_client_request_class_selection_after_death(void *client) {
  (void)client;
  return -1;
}

int samp_raknet_client_request_class_delta(void *client, int delta) {
  (void)client;
  (void)delta;
  return -1;
}

int samp_raknet_client_request_spawn(void *client) {
  (void)client;
  return -1;
}

int samp_raknet_client_drain_packets(void *client, int max_packets) {
  (void)client;
  if (max_packets < 0) {
    return -1;
  }
  return 0;
}

int samp_raknet_client_drain_packets_autojoin(void *client, int max_packets, const samp_raknet_join_profile *profile,
                                              int *out_connected, int *out_join_sent, int *out_last_packet_id) {
  (void)profile;
  if (max_packets < 0) {
    return -1;
  }
  (void)client;
  if (out_connected != 0) {
    *out_connected = 0;
  }
  if (out_join_sent != 0) {
    *out_join_sent = 0;
  }
  if (out_last_packet_id != 0) {
    *out_last_packet_id = -1;
  }
  return 0;
}

int samp_raknet_client_get_rpc_probe_snapshot(void *client, samp_raknet_rpc_probe_snapshot *out_snapshot) {
  (void)client;
  if (out_snapshot == 0) {
    return -1;
  }
  memset(out_snapshot, 0, sizeof(*out_snapshot));
  return -1;
}

int samp_raknet_client_get_actor_state(void *client, uint16_t actor_id,
                                       samp_raknet_actor_state *out_state) {
  (void)client;
  (void)actor_id;
  if (out_state != 0) {
    memset(out_state, 0, sizeof(*out_state));
  }
  return -1;
}

int samp_raknet_client_get_actor_create_rotation_bits(
    void *client, uint16_t actor_id, uint32_t *out_create_revision,
    uint32_t *out_rotation_bits, uint32_t *out_facing_revision) {
  (void)client;
  (void)actor_id;
  if (out_create_revision != 0) {
    *out_create_revision = 0u;
  }
  if (out_rotation_bits != 0) {
    *out_rotation_bits = 0u;
  }
  if (out_facing_revision != 0) {
    *out_facing_revision = 0u;
  }
  return -1;
}

int samp_raknet_client_get_object_material(void *client, uint16_t object_id, uint32_t object_generation,
                                           uint8_t material_slot, uint32_t minimum_revision,
                                           samp_raknet_object_material *out_material,
                                           uint32_t *out_revision) {
  (void)client;
  (void)object_id;
  (void)object_generation;
  (void)material_slot;
  (void)minimum_revision;
  if (out_material != 0) {
    memset(out_material, 0, sizeof(*out_material));
  }
  if (out_revision != 0) {
    *out_revision = 0u;
  }
  return -1;
}

int samp_raknet_client_get_object_lifecycle(void *client, uint16_t object_id, uint8_t *out_alive,
                                            samp_raknet_object_event *out_create_event) {
  (void)client;
  (void)object_id;
  if (out_alive != 0) {
    *out_alive = 0u;
  }
  if (out_create_event != 0) {
    memset(out_create_event, 0, sizeof(*out_create_event));
  }
  return -1;
}

int samp_raknet_client_send_spawn_notification(void *client) {
  (void)client;
  return -1;
}

int samp_raknet_client_send_spawn_notification_for_seq(void *client, uint32_t spawn_info_seq) {
  (void)client;
  (void)spawn_info_seq;
  return -1;
}

int samp_raknet_client_send_death_notification(void *client, uint8_t death_reason, uint8_t responsible_player) {
  (void)client;
  (void)death_reason;
  (void)responsible_player;
  return -1;
}

int samp_raknet_client_send_onfoot_sync(void *client, const samp_raknet_onfoot_sync *sync) {
  (void)client;
  (void)sync;
  return -1;
}

int samp_raknet_client_send_incar_sync(void *client, const samp_raknet_incar_sync *sync) {
  (void)client;
  (void)sync;
  return -1;
}

int samp_raknet_client_send_spectator_sync(void *client, const samp_raknet_spectator_sync *sync) {
  (void)client;
  (void)sync;
  return -1;
}

int samp_raknet_client_send_aim_sync(void *client, const samp_raknet_aim_sync *sync) {
  (void)client;
  (void)sync;
  return -1;
}

int samp_raknet_client_send_bullet_sync(void *client, const samp_raknet_bullet_sync *sync) {
  (void)client;
  (void)sync;
  return -1;
}

int samp_raknet_client_send_give_take_damage(void *client, uint8_t taking, uint16_t player_id, float damage,
                                             uint32_t weapon_id, uint32_t bodypart) {
  (void)client;
  (void)taking;
  (void)player_id;
  (void)damage;
  (void)weapon_id;
  (void)bodypart;
  return -1;
}

int samp_raknet_client_send_actor_damage(void *client, uint16_t actor_id, float damage,
                                         uint32_t weapon_id, uint32_t bodypart) {
  (void)client;
  (void)actor_id;
  (void)damage;
  (void)weapon_id;
  (void)bodypart;
  return -1;
}

int samp_raknet_client_send_edit_object_response(void *client, uint8_t player_object, uint16_t object_id,
                                                 uint32_t response, const float position[3],
                                                 const float rotation[3]) {
  (void)client; (void)player_object; (void)object_id; (void)response; (void)position; (void)rotation;
  return -1;
}

int samp_raknet_client_send_edit_attached_object_response(void *client, uint32_t response, uint32_t index,
                                                          uint32_t model, uint32_t bone,
                                                          const float offset[3], const float rotation[3],
                                                          const float scale[3], uint32_t color1,
                                                          uint32_t color2) {
  (void)client; (void)response; (void)index; (void)model; (void)bone; (void)offset; (void)rotation;
  (void)scale; (void)color1; (void)color2;
  return -1;
}

int samp_raknet_client_queue_dialog_response(void *client, uint16_t dialog_id, uint8_t button, int16_t listitem,
                                             const char *input) {
  (void)client;
  (void)dialog_id;
  (void)button;
  (void)listitem;
  (void)input;
  return -1;
}
