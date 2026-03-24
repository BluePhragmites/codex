#pragma once

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class PbchSsbBuilder {
 public:
  ByteVector build_mib(const CellConfig& cell, const SlotIndication& slot) const;
};

}  // namespace mini_gnb
