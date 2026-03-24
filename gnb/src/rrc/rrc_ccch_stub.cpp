#include "mini_gnb/rrc/rrc_ccch_stub.hpp"

#include <string>

namespace mini_gnb {

RrcSetupRequestInfo RrcCcchStub::parse_setup_request(const ByteVector& ccch_sdu) const {
  RrcSetupRequestInfo info;
  if (ccch_sdu.size() < 8U) {
    info.valid = false;
    return info;
  }

  for (std::size_t i = 0; i < info.contention_id48.size(); ++i) {
    info.contention_id48[i] = ccch_sdu[i];
  }
  info.establishment_cause = ccch_sdu[6];
  info.ue_identity_type = ccch_sdu[7];
  info.ue_identity_raw.assign(ccch_sdu.begin() + 8, ccch_sdu.end());
  info.valid = true;
  return info;
}

RrcSetupBlob RrcCcchStub::build_setup(const RrcSetupRequestInfo& request_info) const {
  const auto text = std::string("RRCSetup|cause=") +
                    std::to_string(request_info.establishment_cause) +
                    "|ue_type=" + std::to_string(request_info.ue_identity_type);
  return RrcSetupBlob{ByteVector(text.begin(), text.end())};
}

}  // namespace mini_gnb
