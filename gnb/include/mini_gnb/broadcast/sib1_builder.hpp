#pragma once

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class Sib1Builder {
 public:
  RrcSetupBlob build_minimal_sib1(const CellConfig& cell, const PrachConfig& prach) const;
};

}  // namespace mini_gnb
