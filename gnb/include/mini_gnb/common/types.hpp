#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mini_gnb {

using ByteVector = std::vector<std::uint8_t>;

struct ComplexSample {
  float i {};
  float q {};
};

enum class DlObjectType {
  ssb,
  sib1,
  rar,
  msg4
};

enum class RaState {
  idle,
  prach_detected,
  tc_rnti_assigned,
  rar_sent,
  msg3_wait,
  msg3_ok,
  msg4_sent,
  done,
  fail
};

inline std::string to_string(DlObjectType type) {
  switch (type) {
    case DlObjectType::ssb:
      return "DL_OBJ_SSB";
    case DlObjectType::sib1:
      return "DL_OBJ_SIB1";
    case DlObjectType::rar:
      return "DL_OBJ_RAR";
    case DlObjectType::msg4:
      return "DL_OBJ_MSG4";
  }
  return "DL_OBJ_UNKNOWN";
}

inline std::string to_string(RaState state) {
  switch (state) {
    case RaState::idle:
      return "IDLE";
    case RaState::prach_detected:
      return "PRACH_DETECTED";
    case RaState::tc_rnti_assigned:
      return "TC_RNTI_ASSIGNED";
    case RaState::rar_sent:
      return "RAR_SENT";
    case RaState::msg3_wait:
      return "MSG3_WAIT";
    case RaState::msg3_ok:
      return "MSG3_OK";
    case RaState::msg4_sent:
      return "MSG4_SENT";
    case RaState::done:
      return "DONE";
    case RaState::fail:
      return "FAIL";
  }
  return "UNKNOWN";
}

struct CellConfig {
  std::uint32_t dl_arfcn {};
  std::uint16_t band {};
  std::uint16_t channel_bandwidth_mhz {};
  std::uint16_t common_scs_khz {};
  std::uint16_t pci {};
  std::string plmn;
  std::uint16_t tac {};
  std::uint8_t ss0_index {};
  std::uint8_t coreset0_index {};
};

struct PrachConfig {
  std::uint16_t prach_config_index {};
  std::uint16_t prach_root_seq_index {};
  std::uint8_t zero_correlation_zone {};
  std::uint8_t ra_resp_window {};
  std::int8_t msg3_delta_preamble {};
};

struct RfConfig {
  std::string device_driver;
  std::string device_args;
  std::string clock_src;
  double srate {};
  double tx_gain {};
  double rx_gain {};
};

struct BroadcastConfig {
  int ssb_period_slots {};
  int sib1_period_slots {};
};

struct SimConfig {
  int total_slots {};
  int slots_per_frame {};
  int msg3_delay_slots {};
  int msg4_delay_slots {};
  int prach_trigger_abs_slot {};
  std::uint8_t preamble_id {};
  int ta_est {};
  double peak_metric {};
  bool msg3_crc_ok {};
  double msg3_snr_db {};
  double msg3_evm {};
  std::string contention_id_hex;
  std::uint8_t establishment_cause {};
  std::uint8_t ue_identity_type {};
  std::string ue_identity_hex;
  bool include_crnti_ce {};
};

struct Config {
  CellConfig cell;
  PrachConfig prach;
  RfConfig rf;
  BroadcastConfig broadcast;
  SimConfig sim;
};

struct RadioStatus {
  std::int64_t hw_time_ns {};
  bool tx_underflow {};
  bool rx_overflow {};
  double cfo_est_hz {};
};

struct RadioBurst {
  std::int64_t hw_time_ns {};
  std::uint32_t sfn {};
  std::uint16_t slot {};
  std::uint32_t nof_samples {};
  RadioStatus status;
};

struct SlotIndication {
  std::uint32_t sfn {};
  std::uint16_t slot {};
  int abs_slot {};
  std::int64_t slot_start_ns {};
  bool has_ssb {};
  bool has_sib1 {};
  bool has_prach_occasion {};
  bool is_ul_slot {};
  bool is_dl_slot {};
};

struct PrachIndication {
  std::uint32_t sfn {};
  std::uint16_t slot {};
  int abs_slot {};
  std::uint8_t preamble_id {};
  int ta_est {};
  double peak_metric {};
  double snr_est {};
  bool valid {};
};

struct UlGrantForMsg3 {
  std::uint16_t tc_rnti {};
  int abs_slot {};
  std::uint16_t msg3_prb_start {};
  std::uint16_t msg3_prb_len {};
  std::uint8_t msg3_mcs {};
  std::uint8_t k2 {};
  std::uint8_t ta_cmd {};
};

struct DlGrant {
  DlObjectType type {DlObjectType::ssb};
  int abs_slot {};
  std::uint16_t rnti {};
  std::uint16_t prb_start {};
  std::uint16_t prb_len {};
  std::uint8_t mcs {};
  std::uint8_t rv {};
  std::uint8_t harq_id {};
  ByteVector payload;
};

struct TxGridPatch {
  std::uint32_t sfn {};
  std::uint16_t slot {};
  int abs_slot {};
  std::uint16_t sym_start {};
  std::uint16_t nof_sym {};
  std::uint16_t prb_start {};
  std::uint16_t prb_len {};
  DlObjectType type {DlObjectType::ssb};
  std::uint16_t rnti {};
  std::size_t payload_len {};
  std::uint32_t fft_size {};
  std::uint32_t cp_len {};
  double sample_rate_hz {};
  std::vector<ComplexSample> iq_samples;
};

struct Msg3DecodeIndication {
  std::uint32_t sfn {};
  std::uint16_t slot {};
  int abs_slot {};
  std::uint16_t rnti {};
  bool crc_ok {};
  double snr_db {};
  double evm {};
  ByteVector mac_pdu;
};

struct MacUlParseResult {
  bool has_crnti_ce {};
  std::uint16_t crnti_ce {};
  bool has_ul_ccch {};
  ByteVector ul_ccch_sdu;
  bool parse_ok {true};
  std::vector<int> lcid_sequence;
};

struct RrcSetupRequestInfo {
  std::uint8_t establishment_cause {};
  std::uint8_t ue_identity_type {};
  ByteVector ue_identity_raw;
  std::array<std::uint8_t, 6> contention_id48 {};
  bool valid {};
};

struct RrcSetupBlob {
  ByteVector asn1_buf;
};

struct MiniUeContext {
  std::uint16_t tc_rnti {};
  std::uint16_t c_rnti {};
  std::array<std::uint8_t, 6> contention_id48 {};
  int create_abs_slot {};
  bool rrc_setup_sent {};
  int sent_abs_slot {-1};
};

struct RaContext {
  int detect_abs_slot {};
  std::uint8_t preamble_id {};
  std::uint16_t tc_rnti {};
  int ta_est {};
  int rar_abs_slot {};
  int msg3_expect_abs_slot {};
  int msg4_abs_slot {-1};
  std::array<std::uint8_t, 6> contention_id48 {};
  bool has_contention_id {};
  RaState state {RaState::idle};
  int msg3_harq_round {};
  bool ue_ctx_promoted {};
  std::string last_failure;
};

struct TraceEvent {
  std::string module;
  std::string message;
  int abs_slot {-1};
  std::map<std::string, std::string> data;
};

struct SlotPerf {
  int abs_slot {};
  int mac_latency_us {};
  int dl_build_latency_us {};
  int ul_decode_latency_us {};
};

struct RaScheduleRequest {
  std::uint16_t tc_rnti {};
  int detect_abs_slot {};
  int rar_abs_slot {};
  std::uint8_t preamble_id {};
  std::uint8_t ta_cmd {};
  UlGrantForMsg3 ul_grant;
};

struct Msg4ScheduleRequest {
  std::uint16_t tc_rnti {};
  int msg4_abs_slot {};
  std::array<std::uint8_t, 6> contention_id48 {};
  RrcSetupBlob rrc_setup;
};

struct RunSummary {
  std::map<std::string, std::uint64_t> counters;
  std::optional<RaContext> ra_context;
  std::vector<MiniUeContext> ue_contexts;
  std::string trace_path;
  std::string metrics_path;
  std::string summary_path;
};

}  // namespace mini_gnb
