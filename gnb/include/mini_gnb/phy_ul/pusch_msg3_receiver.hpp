#pragma once

#include <optional>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class IPuschMsg3Receiver {
 public:
  virtual ~IPuschMsg3Receiver() = default;
  virtual std::optional<Msg3DecodeIndication> decode(const SlotIndication& slot,
                                                     const UlGrantForMsg3& ul_grant) = 0;
};

class MockMsg3Receiver final : public IPuschMsg3Receiver {
 public:
  explicit MockMsg3Receiver(SimConfig config);

  std::optional<Msg3DecodeIndication> decode(const SlotIndication& slot,
                                             const UlGrantForMsg3& ul_grant) override;

 private:
  SimConfig config_;
};

}  // namespace mini_gnb
