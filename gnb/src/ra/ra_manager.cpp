#include "mini_gnb/ra/ra_manager.hpp"

#include <utility>

#include "mini_gnb/common/hex.hpp"
#include "mini_gnb/metrics/metrics_trace.hpp"

namespace mini_gnb {

RaManager::RaManager(PrachConfig prach, SimConfig sim) : prach_(std::move(prach)), sim_(std::move(sim)) {}

std::optional<RaScheduleRequest> RaManager::on_prach(const PrachIndication& prach_indication,
                                                     const SlotIndication& slot,
                                                     MetricsTrace& metrics) {
  if (active_context_.has_value() &&
      active_context_->state != RaState::done &&
      active_context_->state != RaState::fail) {
    metrics.trace("ra_manager",
                  "Ignored PRACH because a RA context is already active.",
                  slot.abs_slot,
                  {{"active_tc_rnti", std::to_string(active_context_->tc_rnti)}});
    return std::nullopt;
  }

  const auto tc_rnti = next_tc_rnti_++;
  RaContext context;
  context.detect_abs_slot = slot.abs_slot;
  context.preamble_id = prach_indication.preamble_id;
  context.tc_rnti = tc_rnti;
  context.ta_est = prach_indication.ta_est;
  context.rar_abs_slot = slot.abs_slot + 1;
  context.msg3_expect_abs_slot = slot.abs_slot + sim_.msg3_delay_slots;
  context.state = RaState::tc_rnti_assigned;
  active_context_ = context;

  metrics.trace("ra_manager",
                "Created RA context from PRACH.",
                slot.abs_slot,
                {
                    {"preamble_id", std::to_string(context.preamble_id)},
                    {"tc_rnti", std::to_string(context.tc_rnti)},
                    {"rar_abs_slot", std::to_string(context.rar_abs_slot)},
                    {"msg3_expect_abs_slot", std::to_string(context.msg3_expect_abs_slot)},
                });

  RaScheduleRequest request;
  request.tc_rnti = tc_rnti;
  request.detect_abs_slot = slot.abs_slot;
  request.rar_abs_slot = context.rar_abs_slot;
  request.preamble_id = prach_indication.preamble_id;
  request.ta_cmd = static_cast<std::uint8_t>(prach_indication.ta_est);
  request.ul_grant = UlGrantForMsg3{
      tc_rnti,
      context.msg3_expect_abs_slot,
      48,
      16,
      4,
      4,
      static_cast<std::uint8_t>(prach_indication.ta_est),
  };
  return request;
}

void RaManager::mark_rar_sent(const std::uint16_t tc_rnti,
                              const SlotIndication& slot,
                              MetricsTrace& metrics) {
  if (!active_context_.has_value() || active_context_->tc_rnti != tc_rnti) {
    return;
  }

  active_context_->state = RaState::msg3_wait;
  metrics.trace("ra_manager",
                "RAR sent, waiting for Msg3.",
                slot.abs_slot,
                {
                    {"tc_rnti", std::to_string(tc_rnti)},
                    {"msg3_expect_abs_slot", std::to_string(active_context_->msg3_expect_abs_slot)},
                });
}

std::optional<Msg4ScheduleRequest> RaManager::on_msg3_success(const Msg3DecodeIndication& msg3,
                                                              const MacUlParseResult& /*mac_result*/,
                                                              const RrcSetupRequestInfo& request_info,
                                                              const RrcSetupBlob& rrc_setup,
                                                              const SlotIndication& slot,
                                                              MetricsTrace& metrics) {
  if (!active_context_.has_value() || active_context_->tc_rnti != msg3.rnti) {
    return std::nullopt;
  }

  active_context_->state = RaState::msg3_ok;
  active_context_->msg4_abs_slot = slot.abs_slot + sim_.msg4_delay_slots;
  active_context_->contention_id48 = request_info.contention_id48;
  active_context_->has_contention_id = true;
  active_context_->ue_ctx_promoted = true;

  metrics.trace("ra_manager",
                "Msg3 accepted, scheduling Msg4.",
                slot.abs_slot,
                {
                    {"tc_rnti", std::to_string(active_context_->tc_rnti)},
                    {"contention_id48", bytes_to_hex(active_context_->contention_id48)},
                    {"msg4_abs_slot", std::to_string(active_context_->msg4_abs_slot)},
                });

  return Msg4ScheduleRequest{
      active_context_->tc_rnti,
      active_context_->msg4_abs_slot,
      active_context_->contention_id48,
      rrc_setup,
  };
}

void RaManager::mark_msg4_sent(const std::uint16_t tc_rnti,
                               const SlotIndication& slot,
                               MetricsTrace& metrics) {
  if (!active_context_.has_value() || active_context_->tc_rnti != tc_rnti) {
    return;
  }

  active_context_->state = RaState::done;
  metrics.trace("ra_manager",
                "Msg4 sent, RA flow completed.",
                slot.abs_slot,
                {{"tc_rnti", std::to_string(tc_rnti)}});
}

void RaManager::expire(const SlotIndication& slot, MetricsTrace& metrics) {
  if (!active_context_.has_value()) {
    return;
  }

  std::string timeout_reason;
  if (active_context_->state == RaState::tc_rnti_assigned &&
      slot.abs_slot > (active_context_->rar_abs_slot + static_cast<int>(prach_.ra_resp_window))) {
    timeout_reason = "RAR_TIMEOUT";
  } else if (active_context_->state == RaState::msg3_wait &&
             slot.abs_slot > (active_context_->msg3_expect_abs_slot + 2)) {
    timeout_reason = "MSG3_TIMEOUT";
  }

  if (!timeout_reason.empty()) {
    active_context_->state = RaState::fail;
    active_context_->last_failure = timeout_reason;
    metrics.increment("ra_timeout");
    metrics.trace("ra_manager",
                  "RA context expired.",
                  slot.abs_slot,
                  {
                      {"tc_rnti", std::to_string(active_context_->tc_rnti)},
                      {"reason", timeout_reason},
                  });
  }
}

const std::optional<RaContext>& RaManager::active_context() const {
  return active_context_;
}

}  // namespace mini_gnb
