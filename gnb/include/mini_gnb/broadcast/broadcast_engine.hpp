#pragma once

#include <vector>

#include "mini_gnb/broadcast/pbch_ssb_builder.hpp"
#include "mini_gnb/broadcast/sib1_builder.hpp"
#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class BroadcastEngine {
 public:
  BroadcastEngine(CellConfig cell, PrachConfig prach, BroadcastConfig broadcast);

  std::vector<DlGrant> schedule(const SlotIndication& slot) const;

 private:
  CellConfig cell_;
  PrachConfig prach_;
  BroadcastConfig broadcast_;
  PbchSsbBuilder pbch_builder_;
  Sib1Builder sib1_builder_;
};

}  // namespace mini_gnb
