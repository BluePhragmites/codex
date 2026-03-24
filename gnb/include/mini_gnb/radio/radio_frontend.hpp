#pragma once

#include <cstdint>
#include <vector>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class MetricsTrace;

class IRadioFrontend {
 public:
  virtual ~IRadioFrontend() = default;

  virtual RadioBurst receive(const SlotIndication& slot) = 0;
  virtual void submit_tx(const SlotIndication& slot,
                         const std::vector<TxGridPatch>& patches,
                         MetricsTrace& metrics) = 0;
  virtual std::uint64_t tx_burst_count() const = 0;
  virtual std::int64_t last_hw_time_ns() const = 0;
};

class MockRadioFrontend final : public IRadioFrontend {
 public:
  explicit MockRadioFrontend(RfConfig config);

  RadioBurst receive(const SlotIndication& slot) override;
  void submit_tx(const SlotIndication& slot,
                 const std::vector<TxGridPatch>& patches,
                 MetricsTrace& metrics) override;
  std::uint64_t tx_burst_count() const override;
  std::int64_t last_hw_time_ns() const override;

 private:
  RfConfig config_;
  std::uint64_t tx_burst_count_ {};
  std::int64_t last_hw_time_ns_ {};
};

}  // namespace mini_gnb
