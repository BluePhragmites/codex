#pragma once

#include <vector>

#include "mini_gnb/common/types.hpp"
#include "mini_gnb/phy_dl/rar_builder.hpp"

namespace mini_gnb {

class MetricsTrace;

class InitialAccessScheduler {
 public:
  InitialAccessScheduler();

  void queue_rar(const RaScheduleRequest& request, MetricsTrace& metrics);
  void queue_msg4(const Msg4ScheduleRequest& request, MetricsTrace& metrics);

  std::vector<DlGrant> pop_due_downlink(int abs_slot);
  std::vector<UlGrantForMsg3> pop_due_msg3_grants(int abs_slot);

 private:
  std::vector<DlGrant> pending_dl_;
  std::vector<UlGrantForMsg3> pending_ul_;
  RarBuilder rar_builder_;
};

}  // namespace mini_gnb
