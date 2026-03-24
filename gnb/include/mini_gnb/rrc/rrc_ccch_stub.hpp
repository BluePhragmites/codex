#pragma once

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class RrcCcchStub {
 public:
  RrcSetupRequestInfo parse_setup_request(const ByteVector& ccch_sdu) const;
  RrcSetupBlob build_setup(const RrcSetupRequestInfo& request_info) const;
};

}  // namespace mini_gnb
