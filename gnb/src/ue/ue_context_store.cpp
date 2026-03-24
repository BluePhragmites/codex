#include "mini_gnb/ue/ue_context_store.hpp"

namespace mini_gnb {

const MiniUeContext& UeContextStore::promote(const RaContext& ra_context,
                                             const RrcSetupRequestInfo& request_info,
                                             const int create_abs_slot) {
  MiniUeContext context;
  context.tc_rnti = ra_context.tc_rnti;
  context.c_rnti = ra_context.tc_rnti;
  context.contention_id48 = request_info.contention_id48;
  context.create_abs_slot = create_abs_slot;
  context.rrc_setup_sent = false;
  context.sent_abs_slot = -1;

  if (contexts_.empty()) {
    contexts_.push_back(context);
  } else {
    contexts_.front() = context;
  }

  return contexts_.front();
}

void UeContextStore::mark_rrc_setup_sent(const std::uint16_t tc_rnti, const int sent_abs_slot) {
  for (auto& context : contexts_) {
    if (context.tc_rnti == tc_rnti) {
      context.rrc_setup_sent = true;
      context.sent_abs_slot = sent_abs_slot;
    }
  }
}

const std::vector<MiniUeContext>& UeContextStore::contexts() const {
  return contexts_;
}

}  // namespace mini_gnb
