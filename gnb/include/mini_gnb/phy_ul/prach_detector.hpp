#pragma once

#include <optional>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class IPrachDetector {
 public:
  virtual ~IPrachDetector() = default;
  virtual std::optional<PrachIndication> detect(const SlotIndication& slot,
                                                const RadioBurst& burst) = 0;
};

class MockPrachDetector final : public IPrachDetector {
 public:
  explicit MockPrachDetector(SimConfig config);

  std::optional<PrachIndication> detect(const SlotIndication& slot,
                                        const RadioBurst& burst) override;

 private:
  SimConfig config_;
  bool fired_ {false};
};

}  // namespace mini_gnb
