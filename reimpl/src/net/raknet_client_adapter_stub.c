#include "sampdll/net/raknet_client_adapter.h"

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

int samp_raknet_client_send_textdraw_click(void *client, uint16_t textdraw_id) {
  (void)client;
  (void)textdraw_id;
  return -1;
}

int samp_raknet_client_request_class_delta(void *client, int delta) {
  (void)client;
  (void)delta;
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

int samp_raknet_client_send_spawn_notification(void *client) {
  (void)client;
  return -1;
}

int samp_raknet_client_send_spawn_notification_for_seq(void *client, uint32_t spawn_info_seq) {
  (void)client;
  (void)spawn_info_seq;
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

int samp_raknet_client_queue_dialog_response(void *client, uint16_t dialog_id, uint8_t button, int16_t listitem,
                                             const char *input) {
  (void)client;
  (void)dialog_id;
  (void)button;
  (void)listitem;
  (void)input;
  return -1;
}
