#include "mini_gnb/timing/slot_engine.hpp"

namespace mini_gnb {

SlotEngine::SlotEngine(const Config& config) : config_(config) {}

SlotIndication SlotEngine::make_slot(const int abs_slot) const {
  SlotIndication slot;
  slot.abs_slot = abs_slot;
  slot.slot = static_cast<std::uint16_t>(abs_slot % config_.sim.slots_per_frame);
  slot.sfn = static_cast<std::uint32_t>((abs_slot / config_.sim.slots_per_frame) % 1024);
  slot.slot_start_ns = static_cast<std::int64_t>(abs_slot) * 500000;
  slot.has_ssb = (abs_slot % config_.broadcast.ssb_period_slots) == 0;
  slot.has_sib1 = (abs_slot % config_.broadcast.sib1_period_slots) == 0;
  slot.has_prach_occasion = abs_slot == config_.sim.prach_trigger_abs_slot;
  slot.is_ul_slot = true;
  slot.is_dl_slot = true;
  return slot;
}

}  // namespace mini_gnb
