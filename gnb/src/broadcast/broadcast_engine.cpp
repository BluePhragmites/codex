#include "mini_gnb/broadcast/broadcast_engine.hpp"

#include <utility>

namespace mini_gnb {

BroadcastEngine::BroadcastEngine(CellConfig cell, PrachConfig prach, BroadcastConfig broadcast)
    : cell_(std::move(cell)), prach_(std::move(prach)), broadcast_(std::move(broadcast)) {}

std::vector<DlGrant> BroadcastEngine::schedule(const SlotIndication& slot) const {
  std::vector<DlGrant> grants;

  if (slot.has_ssb) {
    grants.push_back(DlGrant{
        DlObjectType::ssb,
        slot.abs_slot,
        0,
        0,
        20,
        0,
        0,
        0,
        pbch_builder_.build_mib(cell_, slot),
    });
  }

  if (slot.has_sib1) {
    const auto sib1 = sib1_builder_.build_minimal_sib1(cell_, prach_);
    grants.push_back(DlGrant{
        DlObjectType::sib1,
        slot.abs_slot,
        0xFFFF,
        20,
        24,
        4,
        0,
        0,
        sib1.asn1_buf,
    });
  }

  return grants;
}

}  // namespace mini_gnb
