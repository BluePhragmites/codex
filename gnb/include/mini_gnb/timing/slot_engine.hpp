#pragma once

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class SlotEngine {
 public:
  explicit SlotEngine(const Config& config);

  SlotIndication make_slot(int abs_slot) const;

 private:
  Config config_;
};

}  // namespace mini_gnb
