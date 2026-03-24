#include "mini_gnb/phy_dl/rar_builder.hpp"

namespace mini_gnb {

ByteVector RarBuilder::build(const RaScheduleRequest& request) const {
  ByteVector buffer;
  buffer.push_back(request.preamble_id);
  buffer.push_back(request.ta_cmd);
  buffer.push_back(static_cast<std::uint8_t>(request.ul_grant.msg3_prb_start & 0xFFU));
  buffer.push_back(static_cast<std::uint8_t>((request.ul_grant.msg3_prb_start >> 8U) & 0xFFU));
  buffer.push_back(static_cast<std::uint8_t>(request.ul_grant.msg3_prb_len & 0xFFU));
  buffer.push_back(static_cast<std::uint8_t>((request.ul_grant.msg3_prb_len >> 8U) & 0xFFU));
  buffer.push_back(request.ul_grant.msg3_mcs);
  buffer.push_back(request.ul_grant.k2);
  buffer.push_back(static_cast<std::uint8_t>(request.tc_rnti & 0xFFU));
  buffer.push_back(static_cast<std::uint8_t>((request.tc_rnti >> 8U) & 0xFFU));
  return buffer;
}

}  // namespace mini_gnb
