#pragma once

#include <optional>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class MetricsTrace;

class RaManager {
 public:
  RaManager(PrachConfig prach, SimConfig sim);

  std::optional<RaScheduleRequest> on_prach(const PrachIndication& prach_indication,
                                            const SlotIndication& slot,
                                            MetricsTrace& metrics);
  void mark_rar_sent(std::uint16_t tc_rnti, const SlotIndication& slot, MetricsTrace& metrics);
  std::optional<Msg4ScheduleRequest> on_msg3_success(const Msg3DecodeIndication& msg3,
                                                     const MacUlParseResult& mac_result,
                                                     const RrcSetupRequestInfo& request_info,
                                                     const RrcSetupBlob& rrc_setup,
                                                     const SlotIndication& slot,
                                                     MetricsTrace& metrics);
  void mark_msg4_sent(std::uint16_t tc_rnti, const SlotIndication& slot, MetricsTrace& metrics);
  void expire(const SlotIndication& slot, MetricsTrace& metrics);

  const std::optional<RaContext>& active_context() const;

 private:
  PrachConfig prach_;
  SimConfig sim_;
  std::optional<RaContext> active_context_;
  std::uint16_t next_tc_rnti_ {0x4601};
};

}  // namespace mini_gnb
