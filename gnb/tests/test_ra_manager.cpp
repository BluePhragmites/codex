#include "test_helpers.hpp"

#include "mini_gnb/common/types.hpp"
#include "mini_gnb/config/config_loader.hpp"
#include "mini_gnb/metrics/metrics_trace.hpp"
#include "mini_gnb/ra/ra_manager.hpp"
#include "mini_gnb/timing/slot_engine.hpp"

void test_ra_manager_flow() {
  const auto config = mini_gnb::load_config(default_config_path());
  mini_gnb::MetricsTrace metrics(project_source_dir() + "/out/test_ra_manager_flow");
  mini_gnb::SlotEngine slot_engine(config);
  mini_gnb::RaManager ra_manager(config.prach, config.sim);

  const auto prach_slot = slot_engine.make_slot(config.sim.prach_trigger_abs_slot);
  const mini_gnb::PrachIndication prach {
      prach_slot.sfn,
      prach_slot.slot,
      prach_slot.abs_slot,
      config.sim.preamble_id,
      config.sim.ta_est,
      config.sim.peak_metric,
      20.0,
      true,
  };

  const auto rar_request = ra_manager.on_prach(prach, prach_slot, metrics);
  require(rar_request.has_value(), "expected RA schedule request after PRACH");
  require(ra_manager.active_context().has_value(), "expected active RA context");
  require(ra_manager.active_context()->state == mini_gnb::RaState::tc_rnti_assigned,
          "expected TC-RNTI assigned state");

  const auto rar_slot = slot_engine.make_slot(rar_request->rar_abs_slot);
  ra_manager.mark_rar_sent(rar_request->tc_rnti, rar_slot, metrics);
  require(ra_manager.active_context()->state == mini_gnb::RaState::msg3_wait,
          "expected MSG3_WAIT after RAR");

  mini_gnb::Msg3DecodeIndication msg3 {
      rar_slot.sfn,
      rar_slot.slot,
      rar_request->ul_grant.abs_slot,
      rar_request->tc_rnti,
      true,
      18.2,
      2.1,
      {},
  };
  mini_gnb::MacUlParseResult mac_result;
  mac_result.parse_ok = true;
  mac_result.has_ul_ccch = true;
  mini_gnb::RrcSetupRequestInfo request_info;
  request_info.valid = true;
  request_info.establishment_cause = config.sim.establishment_cause;
  request_info.ue_identity_type = config.sim.ue_identity_type;
  request_info.contention_id48 = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
  mini_gnb::RrcSetupBlob rrc_setup{{'R', 'R', 'C'}};

  const auto msg3_slot = slot_engine.make_slot(rar_request->ul_grant.abs_slot);
  const auto msg4_request =
      ra_manager.on_msg3_success(msg3, mac_result, request_info, rrc_setup, msg3_slot, metrics);
  require(msg4_request.has_value(), "expected Msg4 scheduling request");
  require(ra_manager.active_context()->state == mini_gnb::RaState::msg3_ok,
          "expected MSG3_OK after Msg3");

  const auto msg4_slot = slot_engine.make_slot(msg4_request->msg4_abs_slot);
  ra_manager.mark_msg4_sent(msg4_request->tc_rnti, msg4_slot, metrics);
  require(ra_manager.active_context()->state == mini_gnb::RaState::done,
          "expected DONE after Msg4 transmission");
}

void test_ra_timeout() {
  const auto config = mini_gnb::load_config(default_config_path());
  mini_gnb::MetricsTrace metrics(project_source_dir() + "/out/test_ra_timeout");
  mini_gnb::SlotEngine slot_engine(config);
  mini_gnb::RaManager ra_manager(config.prach, config.sim);

  const auto prach_slot = slot_engine.make_slot(config.sim.prach_trigger_abs_slot);
  const mini_gnb::PrachIndication prach {
      prach_slot.sfn,
      prach_slot.slot,
      prach_slot.abs_slot,
      config.sim.preamble_id,
      config.sim.ta_est,
      config.sim.peak_metric,
      20.0,
      true,
  };

  const auto rar_request = ra_manager.on_prach(prach, prach_slot, metrics);
  require(rar_request.has_value(), "expected RA schedule request after PRACH");

  const auto timeout_slot = slot_engine.make_slot(
      rar_request->rar_abs_slot + static_cast<int>(config.prach.ra_resp_window) + 1);
  ra_manager.expire(timeout_slot, metrics);
  require(ra_manager.active_context()->state == mini_gnb::RaState::fail,
          "expected RA failure after RAR timeout");
  require(metrics.counters().at("ra_timeout") == 1, "expected one RA timeout counter");
}
