#pragma once

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class MacUlDemux {
 public:
  MacUlParseResult parse(const ByteVector& mac_pdu) const;
};

}  // namespace mini_gnb
