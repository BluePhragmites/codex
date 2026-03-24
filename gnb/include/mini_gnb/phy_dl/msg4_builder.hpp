#pragma once

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class Msg4Builder {
 public:
  ByteVector build(const Msg4ScheduleRequest& request) const;
};

}  // namespace mini_gnb
