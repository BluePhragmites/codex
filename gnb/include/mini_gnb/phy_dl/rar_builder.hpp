#pragma once

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class RarBuilder {
 public:
  ByteVector build(const RaScheduleRequest& request) const;
};

}  // namespace mini_gnb
