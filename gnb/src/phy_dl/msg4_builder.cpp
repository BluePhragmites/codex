#include "mini_gnb/phy_dl/msg4_builder.hpp"

namespace mini_gnb {

ByteVector Msg4Builder::build(const Msg4ScheduleRequest& request) const {
  ByteVector buffer;
  buffer.push_back(16);
  buffer.push_back(static_cast<std::uint8_t>(request.contention_id48.size()));
  buffer.insert(buffer.end(), request.contention_id48.begin(), request.contention_id48.end());
  buffer.push_back(17);
  buffer.push_back(static_cast<std::uint8_t>(request.rrc_setup.asn1_buf.size()));
  buffer.insert(buffer.end(), request.rrc_setup.asn1_buf.begin(), request.rrc_setup.asn1_buf.end());
  return buffer;
}

}  // namespace mini_gnb
