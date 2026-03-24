#pragma once

#include <vector>

#include "mini_gnb/common/types.hpp"

namespace mini_gnb {

class UeContextStore {
 public:
  const MiniUeContext& promote(const RaContext& ra_context,
                               const RrcSetupRequestInfo& request_info,
                               int create_abs_slot);
  void mark_rrc_setup_sent(std::uint16_t tc_rnti, int sent_abs_slot);

  const std::vector<MiniUeContext>& contexts() const;

 private:
  std::vector<MiniUeContext> contexts_;
};

}  // namespace mini_gnb
