#include "test_helpers.hpp"

#include "mini_gnb/common/hex.hpp"
#include "mini_gnb/config/config_loader.hpp"
#include "mini_gnb/mac/mac_ul_demux.hpp"
#include "mini_gnb/phy_dl/msg4_builder.hpp"
#include "mini_gnb/phy_ul/pusch_msg3_receiver.hpp"
#include "mini_gnb/rrc/rrc_ccch_stub.hpp"
#include "mini_gnb/timing/slot_engine.hpp"

void test_mac_rrc_and_msg4_contention_identity() {
  const auto config = mini_gnb::load_config(default_config_path());
  mini_gnb::SlotEngine slot_engine(config);
  mini_gnb::MockMsg3Receiver msg3_receiver(config.sim);
  mini_gnb::MacUlDemux mac_demux;
  mini_gnb::RrcCcchStub rrc_stub;
  mini_gnb::Msg4Builder msg4_builder;

  const auto msg3_slot = slot_engine.make_slot(config.sim.prach_trigger_abs_slot + config.sim.msg3_delay_slots);
  const mini_gnb::UlGrantForMsg3 msg3_grant {
      0x4601,
      msg3_slot.abs_slot,
      48,
      16,
      4,
      4,
      static_cast<std::uint8_t>(config.sim.ta_est),
  };

  const auto msg3 = msg3_receiver.decode(msg3_slot, msg3_grant);
  require(msg3.has_value(), "expected mock Msg3 decode");
  require(msg3->crc_ok, "expected Msg3 CRC success in default config");

  const auto mac_result = mac_demux.parse(msg3->mac_pdu);
  require(mac_result.parse_ok, "expected MAC parse success");
  require(mac_result.has_ul_ccch, "expected UL-CCCH in Msg3");
  require(mac_result.has_crnti_ce, "expected C-RNTI CE in Msg3");

  const auto request_info = rrc_stub.parse_setup_request(mac_result.ul_ccch_sdu);
  require(request_info.valid, "expected valid RRCSetupRequest");
  require(mini_gnb::bytes_to_hex(request_info.contention_id48) == config.sim.contention_id_hex,
          "expected contention identity copied from simulated Msg3");

  const auto rrc_setup = rrc_stub.build_setup(request_info);
  const mini_gnb::Msg4ScheduleRequest msg4_request {
      msg3_grant.tc_rnti,
      msg3_slot.abs_slot + config.sim.msg4_delay_slots,
      request_info.contention_id48,
      rrc_setup,
  };
  const auto msg4 = msg4_builder.build(msg4_request);

  require(msg4.size() >= 8U, "expected non-empty Msg4 payload");
  const mini_gnb::ByteVector msg4_contention_id(msg4.begin() + 2, msg4.begin() + 8);
  require(mini_gnb::bytes_to_hex(msg4_contention_id) == config.sim.contention_id_hex,
          "expected Msg4 contention identity to match Msg3");
}
