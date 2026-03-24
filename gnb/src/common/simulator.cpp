#include "mini_gnb/common/simulator.hpp"

#include <filesystem>
#include <sstream>
#include <utility>

#include "mini_gnb/common/hex.hpp"

namespace mini_gnb {

namespace {

std::string join_lcid_sequence(const std::vector<int>& lcid_sequence) {
  std::ostringstream stream;
  for (std::size_t i = 0; i < lcid_sequence.size(); ++i) {
    if (i != 0U) {
      stream << "/";
    }
    stream << lcid_sequence[i];
  }
  return stream.str();
}

std::string bool_string(const bool value) {
  return value ? "true" : "false";
}

}  // namespace

MiniGnbSimulator::MiniGnbSimulator(Config config, std::string output_dir)
    : config_(std::move(config)),
      metrics_(std::move(output_dir)),
      slot_engine_(config_),
      radio_(config_.rf),
      broadcast_(config_.cell, config_.prach, config_.broadcast),
      prach_detector_(config_.sim),
      ra_manager_(config_.prach, config_.sim),
      scheduler_(),
      msg3_receiver_(config_.sim) {}

RunSummary MiniGnbSimulator::run() {
  metrics_.trace("main",
                 "Starting mini gNB Msg1-Msg4 prototype run.",
                 -1,
                 {
                     {"total_slots", std::to_string(config_.sim.total_slots)},
                     {"slots_per_frame", std::to_string(config_.sim.slots_per_frame)},
                 });

  for (int abs_slot = 0; abs_slot < config_.sim.total_slots; ++abs_slot) {
    const auto slot = slot_engine_.make_slot(abs_slot);
    const auto burst = radio_.receive(slot);

    ra_manager_.expire(slot, metrics_);

    if (const auto prach_indication = prach_detector_.detect(slot, burst);
        prach_indication.has_value() && prach_indication->valid) {
      metrics_.increment("prach_detect_ok");
      metrics_.trace("prach_detector",
                     "Detected PRACH occasion.",
                     slot.abs_slot,
                     {
                         {"preamble_id", std::to_string(prach_indication->preamble_id)},
                         {"ta_est", std::to_string(prach_indication->ta_est)},
                         {"peak_metric", std::to_string(prach_indication->peak_metric)},
                     });
      if (const auto request = ra_manager_.on_prach(*prach_indication, slot, metrics_);
          request.has_value()) {
        scheduler_.queue_rar(*request, metrics_);
      }
    }

    const auto msg3_grants = scheduler_.pop_due_msg3_grants(slot.abs_slot);
    for (const auto& msg3_grant : msg3_grants) {
      const auto msg3 = msg3_receiver_.decode(slot, msg3_grant);
      if (!msg3.has_value()) {
        continue;
      }

      metrics_.increment(msg3->crc_ok ? "msg3_crc_ok" : "msg3_crc_fail");
      metrics_.trace("pusch_msg3_receiver",
                     "Decoded Msg3 candidate.",
                     slot.abs_slot,
                     {
                         {"rnti", std::to_string(msg3->rnti)},
                         {"crc_ok", bool_string(msg3->crc_ok)},
                         {"snr_db", std::to_string(msg3->snr_db)},
                         {"evm", std::to_string(msg3->evm)},
                         {"mac_pdu", bytes_to_hex(msg3->mac_pdu)},
                     });

      if (!msg3->crc_ok) {
        continue;
      }

      const auto mac_result = mac_demux_.parse(msg3->mac_pdu);
      metrics_.trace("mac_ul_demux",
                     "Parsed Msg3 MAC PDU.",
                     slot.abs_slot,
                     {
                         {"parse_ok", bool_string(mac_result.parse_ok)},
                         {"has_ul_ccch", bool_string(mac_result.has_ul_ccch)},
                         {"has_crnti_ce", bool_string(mac_result.has_crnti_ce)},
                         {"lcid_sequence", join_lcid_sequence(mac_result.lcid_sequence)},
                     });

      if (!mac_result.parse_ok || !mac_result.has_ul_ccch) {
        continue;
      }

      const auto request_info = rrc_stub_.parse_setup_request(mac_result.ul_ccch_sdu);
      metrics_.trace("rrc_ccch_stub",
                     "Parsed RRCSetupRequest.",
                     slot.abs_slot,
                     {
                         {"valid", bool_string(request_info.valid)},
                         {"establishment_cause", std::to_string(request_info.establishment_cause)},
                         {"ue_identity_type", std::to_string(request_info.ue_identity_type)},
                         {"contention_id48", bytes_to_hex(request_info.contention_id48)},
                     });

      if (!request_info.valid) {
        continue;
      }

      const auto rrc_setup = rrc_stub_.build_setup(request_info);
      if (ra_manager_.active_context().has_value()) {
        const auto& ue_context = ue_store_.promote(*ra_manager_.active_context(), request_info, slot.abs_slot);
        metrics_.trace("ue_context_store",
                       "Promoted RA context into minimal UE context.",
                       slot.abs_slot,
                       {
                           {"tc_rnti", std::to_string(ue_context.tc_rnti)},
                           {"c_rnti", std::to_string(ue_context.c_rnti)},
                           {"contention_id48", bytes_to_hex(ue_context.contention_id48)},
                       });
      }

      if (const auto msg4_request =
              ra_manager_.on_msg3_success(*msg3, mac_result, request_info, rrc_setup, slot, metrics_);
          msg4_request.has_value()) {
        scheduler_.queue_msg4(*msg4_request, metrics_);
      }
    }

    auto dl_grants = scheduler_.pop_due_downlink(slot.abs_slot);
    const auto broadcast_grants = broadcast_.schedule(slot);
    dl_grants.insert(dl_grants.end(), broadcast_grants.begin(), broadcast_grants.end());

    if (!dl_grants.empty()) {
      const auto patches = dl_mapper_.map(slot, dl_grants);
      radio_.submit_tx(slot, patches, metrics_);

      for (const auto& grant : dl_grants) {
        if (grant.type == DlObjectType::rar) {
          metrics_.increment("rar_sent");
          ra_manager_.mark_rar_sent(grant.rnti, slot, metrics_);
        } else if (grant.type == DlObjectType::msg4) {
          metrics_.increment("rrcsetup_sent");
          ra_manager_.mark_msg4_sent(grant.rnti, slot, metrics_);
          ue_store_.mark_rrc_setup_sent(grant.rnti, slot.abs_slot);
        }
      }
    }

    metrics_.add_slot_perf(SlotPerf{
        slot.abs_slot,
        120 + static_cast<int>(slot.slot),
        80 + static_cast<int>(dl_grants.size() * 5U),
        60 + static_cast<int>(msg3_grants.size() * 10U),
    });
  }

  return metrics_.flush(ra_manager_.active_context(),
                        ue_store_.contexts(),
                        radio_.tx_burst_count(),
                        radio_.last_hw_time_ns());
}

}  // namespace mini_gnb
