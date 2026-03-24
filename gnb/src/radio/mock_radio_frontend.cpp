#include "mini_gnb/radio/radio_frontend.hpp"

#include <map>
#include <utility>

#include "mini_gnb/metrics/metrics_trace.hpp"

namespace mini_gnb {

MockRadioFrontend::MockRadioFrontend(RfConfig config) : config_(std::move(config)) {}

RadioBurst MockRadioFrontend::receive(const SlotIndication& slot) {
  last_hw_time_ns_ = slot.slot_start_ns;
  RadioBurst burst;
  burst.hw_time_ns = slot.slot_start_ns;
  burst.sfn = slot.sfn;
  burst.slot = slot.slot;
  burst.nof_samples = 0;
  burst.status = RadioStatus{slot.slot_start_ns, false, false, 0.0};
  return burst;
}

void MockRadioFrontend::submit_tx(const SlotIndication& slot,
                                  const std::vector<TxGridPatch>& patches,
                                  MetricsTrace& metrics) {
  for (const auto& patch : patches) {
    ++tx_burst_count_;
    metrics.trace("radio_tx",
                  "Submitted DL burst.",
                  slot.abs_slot,
                  {
                      {"type", to_string(patch.type)},
                      {"rnti", std::to_string(patch.rnti)},
                      {"prb_start", std::to_string(patch.prb_start)},
                      {"prb_len", std::to_string(patch.prb_len)},
                      {"payload_len", std::to_string(patch.payload_len)},
                  });
  }
}

std::uint64_t MockRadioFrontend::tx_burst_count() const {
  return tx_burst_count_;
}

std::int64_t MockRadioFrontend::last_hw_time_ns() const {
  return last_hw_time_ns_;
}

}  // namespace mini_gnb
