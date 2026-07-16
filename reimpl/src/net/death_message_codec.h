#ifndef SAMPDLL_NET_DEATH_MESSAGE_CODEC_H
#define SAMPDLL_NET_DEATH_MESSAGE_CODEC_H

#include <cstddef>
#include <cstdint>

namespace sampdll {
namespace net {

struct DeathMessagePayload {
  std::uint16_t killer_id;
  std::uint16_t killee_id;
  std::uint8_t reason;
};

inline bool decode_death_message(const std::uint8_t *data, std::size_t bytes, DeathMessagePayload *out) {
  if (data == nullptr || out == nullptr || bytes < 5U) {
    return false;
  }

  out->killer_id = static_cast<std::uint16_t>(data[0U]) |
                   static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1U]) << 8U);
  out->killee_id = static_cast<std::uint16_t>(data[2U]) |
                   static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[3U]) << 8U);
  out->reason = data[4U];
  return true;
}

}  // namespace net
}  // namespace sampdll

#endif
