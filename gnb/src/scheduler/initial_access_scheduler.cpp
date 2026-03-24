#include "mini_gnb/scheduler/initial_access_scheduler.hpp"

#include <algorithm>

#include "mini_gnb/common/hex.hpp"
#include "mini_gnb/metrics/metrics_trace.hpp"
#include "mini_gnb/phy_dl/msg4_builder.hpp"

namespace mini_gnb {

InitialAccessScheduler::InitialAccessScheduler() = default;

void InitialAccessScheduler::queue_rar(const RaScheduleRequest& request, MetricsTrace& metrics) {
  const auto rar_pdu = rar_builder_.build(request);
  pending_dl_.push_back(DlGrant{
      DlObjectType::rar,
      request.rar_abs_slot,
      request.tc_rnti,
      44,
      12,
      4,
      0,
      0,
      rar_pdu,
  });
  pending_ul_.push_back(request.ul_grant);

  metrics.trace("initial_access_scheduler",
                "Queued RAR and Msg3 grant.",
                request.rar_abs_slot,
                {
                    {"tc_rnti", std::to_string(request.tc_rnti)},
                    {"msg3_abs_slot", std::to_string(request.ul_grant.abs_slot)},
                    {"rar_payload", bytes_to_hex(rar_pdu)},
                });
}

void InitialAccessScheduler::queue_msg4(const Msg4ScheduleRequest& request, MetricsTrace& metrics) {
  Msg4Builder builder;
  const auto msg4_pdu = builder.build(request);
  pending_dl_.push_back(DlGrant{
      DlObjectType::msg4,
      request.msg4_abs_slot,
      request.tc_rnti,
      48,
      16,
      4,
      0,
      0,
      msg4_pdu,
  });

  metrics.trace("initial_access_scheduler",
                "Queued Msg4 transmission.",
                request.msg4_abs_slot,
                {
                    {"tc_rnti", std::to_string(request.tc_rnti)},
                    {"contention_id48", bytes_to_hex(request.contention_id48)},
                    {"msg4_payload", bytes_to_hex(msg4_pdu)},
                });
}

std::vector<DlGrant> InitialAccessScheduler::pop_due_downlink(const int abs_slot) {
  std::vector<DlGrant> due;
  auto it = pending_dl_.begin();
  while (it != pending_dl_.end()) {
    if (it->abs_slot == abs_slot) {
      due.push_back(*it);
      it = pending_dl_.erase(it);
    } else {
      ++it;
    }
  }
  return due;
}

std::vector<UlGrantForMsg3> InitialAccessScheduler::pop_due_msg3_grants(const int abs_slot) {
  std::vector<UlGrantForMsg3> due;
  auto it = pending_ul_.begin();
  while (it != pending_ul_.end()) {
    if (it->abs_slot == abs_slot) {
      due.push_back(*it);
      it = pending_ul_.erase(it);
    } else {
      ++it;
    }
  }
  return due;
}

}  // namespace mini_gnb
