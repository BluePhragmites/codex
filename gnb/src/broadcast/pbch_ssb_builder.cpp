#include "mini_gnb/broadcast/pbch_ssb_builder.hpp"

#include <string>

namespace mini_gnb {

ByteVector PbchSsbBuilder::build_mib(const CellConfig& cell, const SlotIndication& slot) const {
  const auto text = std::string("MIB|pci=") + std::to_string(cell.pci) +
                    "|arfcn=" + std::to_string(cell.dl_arfcn) +
                    "|scs=" + std::to_string(cell.common_scs_khz) +
                    "|sfn=" + std::to_string(slot.sfn);
  return ByteVector(text.begin(), text.end());
}

}  // namespace mini_gnb
