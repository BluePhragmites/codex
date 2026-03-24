#include "mini_gnb/phy_ul/prach_detector.hpp"

#include <utility>

namespace mini_gnb {

MockPrachDetector::MockPrachDetector(SimConfig config) : config_(std::move(config)) {}

std::optional<PrachIndication> MockPrachDetector::detect(const SlotIndication& slot,
                                                         const RadioBurst& /*burst*/) {
  if (fired_ || !slot.has_prach_occasion || slot.abs_slot != config_.prach_trigger_abs_slot) {
    return std::nullopt;
  }

  fired_ = true;
  return PrachIndication{
      slot.sfn,
      slot.slot,
      slot.abs_slot,
      config_.preamble_id,
      config_.ta_est,
      config_.peak_metric,
      20.0,
      true,
  };
}

}  // namespace mini_gnb
