#include "mini_gnb/phy_dl/dl_phy_mapper.hpp"

namespace mini_gnb {

std::vector<TxGridPatch> MockDlPhyMapper::map(const SlotIndication& slot,
                                              const std::vector<DlGrant>& grants) {
  std::vector<TxGridPatch> patches;
  patches.reserve(grants.size());
  for (const auto& grant : grants) {
    patches.push_back(TxGridPatch{
        slot.sfn,
        slot.slot,
        slot.abs_slot,
        0,
        14,
        grant.prb_start,
        grant.prb_len,
        grant.type,
        grant.rnti,
        grant.payload.size(),
    });
  }
  return patches;
}

}  // namespace mini_gnb
