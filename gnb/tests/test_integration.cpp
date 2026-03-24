#include "test_helpers.hpp"

#include "mini_gnb/common/hex.hpp"
#include "mini_gnb/common/simulator.hpp"
#include "mini_gnb/config/config_loader.hpp"

void test_integration_run() {
  const auto config = mini_gnb::load_config(default_config_path());
  mini_gnb::MiniGnbSimulator simulator(config, project_source_dir() + "/out/test_integration");
  const auto summary = simulator.run();

  require(summary.counters.at("prach_detect_ok") >= 1, "expected PRACH detection");
  require(summary.counters.at("rar_sent") >= 1, "expected RAR transmission");
  require(summary.counters.at("msg3_crc_ok") >= 1, "expected Msg3 CRC success");
  require(summary.counters.at("rrcsetup_sent") >= 1, "expected Msg4/RRCSetup transmission");
  require(!summary.ue_contexts.empty(), "expected at least one promoted UE context");
  require(summary.ue_contexts.front().rrc_setup_sent, "expected UE context marked after Msg4");
  require(summary.ra_context.has_value(), "expected RA context in summary");
  require(summary.ra_context->has_contention_id, "expected resolved contention identity");
  require(mini_gnb::bytes_to_hex(summary.ra_context->contention_id48) ==
              mini_gnb::bytes_to_hex(summary.ue_contexts.front().contention_id48),
          "expected RA context and UE context to share the same contention identity");
}
