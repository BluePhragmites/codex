#pragma once

#include <vector>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class IDlPhyMapper {
 public:
  virtual ~IDlPhyMapper() = default;
  virtual std::vector<TxGridPatch> map(const SlotIndication& slot,
                                       const std::vector<DlGrant>& grants) = 0;
};

class MockDlPhyMapper final : public IDlPhyMapper {
 public:
  explicit MockDlPhyMapper(double sample_rate_hz);

  std::vector<TxGridPatch> map(const SlotIndication& slot,
                               const std::vector<DlGrant>& grants) override;

 private:
  double sample_rate_hz_ {};
};

}  // namespace mini_gnb
