#include "mini_gnb/broadcast/sib1_builder.hpp"

#include <string>

namespace mini_gnb {

RrcSetupBlob Sib1Builder::build_minimal_sib1(const CellConfig& cell, const PrachConfig& prach) const {
  const auto text = std::string("SIB1|plmn=") + cell.plmn +
                    "|tac=" + std::to_string(cell.tac) +
                    "|pci=" + std::to_string(cell.pci) +
                    "|band=" + std::to_string(cell.band) +
                    "|prach_cfg=" + std::to_string(prach.prach_config_index);
  return RrcSetupBlob{ByteVector(text.begin(), text.end())};
}

}  // namespace mini_gnb
