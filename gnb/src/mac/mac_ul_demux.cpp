#include "mini_gnb/mac/mac_ul_demux.hpp"

namespace mini_gnb {

MacUlParseResult MacUlDemux::parse(const ByteVector& mac_pdu) const {
  MacUlParseResult result;
  std::size_t index = 0;
  while (index < mac_pdu.size()) {
    if ((index + 2U) > mac_pdu.size()) {
      result.parse_ok = false;
      break;
    }

    const auto lcid = static_cast<int>(mac_pdu[index]);
    const auto length = static_cast<std::size_t>(mac_pdu[index + 1]);
    result.lcid_sequence.push_back(lcid);
    const auto payload_start = index + 2U;
    const auto payload_end = payload_start + length;
    if (payload_end > mac_pdu.size()) {
      result.parse_ok = false;
      break;
    }

    const ByteVector payload(mac_pdu.begin() + static_cast<std::ptrdiff_t>(payload_start),
                             mac_pdu.begin() + static_cast<std::ptrdiff_t>(payload_end));

    if (lcid == 1) {
      result.has_ul_ccch = true;
      result.ul_ccch_sdu = payload;
    } else if (lcid == 2) {
      if (payload.size() != 2U) {
        result.parse_ok = false;
        break;
      }
      result.has_crnti_ce = true;
      result.crnti_ce = static_cast<std::uint16_t>(payload[0]) |
                        (static_cast<std::uint16_t>(payload[1]) << 8U);
    }

    index = payload_end;
  }
  return result;
}

}  // namespace mini_gnb
