#include "../src/net/death_message_codec.h"

#include <cstdint>
#include <cstdio>

namespace {

int assert_true(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  int failed = 0;
  sampdll::net::DeathMessagePayload decoded{};

  // PROBE_TRACE: samp_net_trace.log, run=libsamp-20260715-killlist-ak47.
  const std::uint8_t observed_ak47[] = {0x01U, 0x00U, 0x00U, 0x00U, 0x1eU};
  failed += assert_true(sampdll::net::decode_death_message(observed_ak47, sizeof(observed_ak47), &decoded),
                        "observed AK-47 death message decodes");
  failed += assert_true(decoded.killer_id == 1U, "observed killer ID");
  failed += assert_true(decoded.killee_id == 0U, "observed killee ID");
  failed += assert_true(decoded.reason == 30U, "observed AK-47 reason");

  const std::uint8_t wide_ids[] = {0x34U, 0x12U, 0xcdU, 0xabU, 0x1fU};
  failed += assert_true(sampdll::net::decode_death_message(wide_ids, sizeof(wide_ids), &decoded),
                        "16-bit player IDs decode");
  failed += assert_true(decoded.killer_id == 0x1234U, "16-bit killer ID");
  failed += assert_true(decoded.killee_id == 0xabcdU, "16-bit killee ID");
  failed += assert_true(decoded.reason == 31U, "M4 reason");

  failed += assert_true(!sampdll::net::decode_death_message(observed_ak47, 4U, &decoded),
                        "truncated payload rejected");
  failed += assert_true(!sampdll::net::decode_death_message(nullptr, sizeof(observed_ak47), &decoded),
                        "null payload rejected");
  failed += assert_true(!sampdll::net::decode_death_message(observed_ak47, sizeof(observed_ak47), nullptr),
                        "null output rejected");

  if (failed != 0) {
    std::fprintf(stderr, "%d checks failed\n", failed);
    return 1;
  }

  std::fprintf(stdout, "All death-message codec checks passed\n");
  return 0;
}
