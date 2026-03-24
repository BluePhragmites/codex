#include "mini_gnb/phy_ul/pusch_msg3_receiver.hpp"

#include <utility>

#include "mini_gnb/common/hex.hpp"

namespace mini_gnb {

MockMsg3Receiver::MockMsg3Receiver(SimConfig config) : config_(std::move(config)) {}

std::optional<Msg3DecodeIndication> MockMsg3Receiver::decode(const SlotIndication& slot,
                                                             const UlGrantForMsg3& ul_grant) {
  ByteVector ccch;
  const auto contention_id = hex_to_bytes(config_.contention_id_hex);
  const auto ue_identity = hex_to_bytes(config_.ue_identity_hex);
  ccch.insert(ccch.end(), contention_id.begin(), contention_id.end());
  ccch.push_back(config_.establishment_cause);
  ccch.push_back(config_.ue_identity_type);
  ccch.insert(ccch.end(), ue_identity.begin(), ue_identity.end());

  ByteVector mac_pdu;
  if (config_.include_crnti_ce) {
    mac_pdu.push_back(2);
    mac_pdu.push_back(2);
    mac_pdu.push_back(static_cast<std::uint8_t>(ul_grant.tc_rnti & 0xFFU));
    mac_pdu.push_back(static_cast<std::uint8_t>((ul_grant.tc_rnti >> 8U) & 0xFFU));
  }

  mac_pdu.push_back(1);
  mac_pdu.push_back(static_cast<std::uint8_t>(ccch.size()));
  mac_pdu.insert(mac_pdu.end(), ccch.begin(), ccch.end());

  return Msg3DecodeIndication{
      slot.sfn,
      slot.slot,
      slot.abs_slot,
      ul_grant.tc_rnti,
      config_.msg3_crc_ok,
      config_.msg3_snr_db,
      config_.msg3_evm,
      mac_pdu,
  };
}

}  // namespace mini_gnb
