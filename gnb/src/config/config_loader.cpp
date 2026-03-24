#include "mini_gnb/config/config_loader.hpp"

#include <regex>
#include <sstream>
#include <stdexcept>

#include "mini_gnb/common/json_utils.hpp"

namespace mini_gnb {

namespace {

std::string extract_string(const std::string& text, const std::string& key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
  std::smatch match;
  if (!std::regex_search(text, match, pattern)) {
    throw std::runtime_error("missing string config key: " + key);
  }
  return match[1].str();
}

int extract_int(const std::string& text, const std::string& key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
  std::smatch match;
  if (!std::regex_search(text, match, pattern)) {
    throw std::runtime_error("missing integer config key: " + key);
  }
  return std::stoi(match[1].str());
}

double extract_double(const std::string& text, const std::string& key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
  std::smatch match;
  if (!std::regex_search(text, match, pattern)) {
    throw std::runtime_error("missing floating config key: " + key);
  }
  return std::stod(match[1].str());
}

bool extract_bool(const std::string& text, const std::string& key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
  std::smatch match;
  if (!std::regex_search(text, match, pattern)) {
    throw std::runtime_error("missing boolean config key: " + key);
  }
  return match[1].str() == "true";
}

}  // namespace

Config load_config(const std::string& path) {
  const auto text = read_text_file(path);

  Config config;
  config.cell.dl_arfcn = static_cast<std::uint32_t>(extract_int(text, "dl_arfcn"));
  config.cell.band = static_cast<std::uint16_t>(extract_int(text, "band"));
  config.cell.channel_bandwidth_mhz = static_cast<std::uint16_t>(extract_int(text, "channel_bandwidth_MHz"));
  config.cell.common_scs_khz = static_cast<std::uint16_t>(extract_int(text, "common_scs_khz"));
  config.cell.pci = static_cast<std::uint16_t>(extract_int(text, "pci"));
  config.cell.plmn = extract_string(text, "plmn");
  config.cell.tac = static_cast<std::uint16_t>(extract_int(text, "tac"));
  config.cell.ss0_index = static_cast<std::uint8_t>(extract_int(text, "ss0_index"));
  config.cell.coreset0_index = static_cast<std::uint8_t>(extract_int(text, "coreset0_index"));

  config.prach.prach_config_index = static_cast<std::uint16_t>(extract_int(text, "prach_config_index"));
  config.prach.prach_root_seq_index = static_cast<std::uint16_t>(extract_int(text, "prach_root_seq_index"));
  config.prach.zero_correlation_zone = static_cast<std::uint8_t>(extract_int(text, "zero_correlation_zone"));
  config.prach.ra_resp_window = static_cast<std::uint8_t>(extract_int(text, "ra_resp_window"));
  config.prach.msg3_delta_preamble = static_cast<std::int8_t>(extract_int(text, "msg3_delta_preamble"));

  config.rf.device_driver = extract_string(text, "device_driver");
  config.rf.device_args = extract_string(text, "device_args");
  config.rf.clock_src = extract_string(text, "clock_src");
  config.rf.srate = extract_double(text, "srate");
  config.rf.tx_gain = extract_double(text, "tx_gain");
  config.rf.rx_gain = extract_double(text, "rx_gain");

  config.broadcast.ssb_period_slots = extract_int(text, "ssb_period_slots");
  config.broadcast.sib1_period_slots = extract_int(text, "sib1_period_slots");

  config.sim.total_slots = extract_int(text, "total_slots");
  config.sim.slots_per_frame = extract_int(text, "slots_per_frame");
  config.sim.msg3_delay_slots = extract_int(text, "msg3_delay_slots");
  config.sim.msg4_delay_slots = extract_int(text, "msg4_delay_slots");
  config.sim.prach_trigger_abs_slot = extract_int(text, "prach_trigger_abs_slot");
  config.sim.preamble_id = static_cast<std::uint8_t>(extract_int(text, "preamble_id"));
  config.sim.ta_est = extract_int(text, "ta_est");
  config.sim.peak_metric = extract_double(text, "peak_metric");
  config.sim.msg3_crc_ok = extract_bool(text, "msg3_crc_ok");
  config.sim.msg3_snr_db = extract_double(text, "msg3_snr_db");
  config.sim.msg3_evm = extract_double(text, "msg3_evm");
  config.sim.contention_id_hex = extract_string(text, "contention_id_hex");
  config.sim.establishment_cause = static_cast<std::uint8_t>(extract_int(text, "establishment_cause"));
  config.sim.ue_identity_type = static_cast<std::uint8_t>(extract_int(text, "ue_identity_type"));
  config.sim.ue_identity_hex = extract_string(text, "ue_identity_hex");
  config.sim.include_crnti_ce = extract_bool(text, "include_crnti_ce");

  return config;
}

std::string format_config_summary(const Config& config) {
  std::ostringstream stream;
  stream << "Broadcast config summary:\n";
  stream << "  cell pci=" << config.cell.pci
         << " band=n" << config.cell.band
         << " arfcn=" << config.cell.dl_arfcn
         << " scs=" << config.cell.common_scs_khz << "kHz"
         << " bw=" << config.cell.channel_bandwidth_mhz << "MHz"
         << " plmn=" << config.cell.plmn
         << " tac=" << config.cell.tac << "\n";
  stream << "  ss0_index=" << static_cast<int>(config.cell.ss0_index)
         << " coreset0_index=" << static_cast<int>(config.cell.coreset0_index) << "\n";
  stream << "RA config summary:\n";
  stream << "  prach_config_index=" << config.prach.prach_config_index
         << " root_seq=" << config.prach.prach_root_seq_index
         << " zero_corr=" << static_cast<int>(config.prach.zero_correlation_zone)
         << " ra_resp_window=" << static_cast<int>(config.prach.ra_resp_window)
         << " msg3_delta_preamble=" << static_cast<int>(config.prach.msg3_delta_preamble) << "\n";
  stream << "RF config summary:\n";
  stream << "  driver=" << config.rf.device_driver
         << " clock=" << config.rf.clock_src
         << " srate=" << config.rf.srate
         << " tx_gain=" << config.rf.tx_gain
         << " rx_gain=" << config.rf.rx_gain;
  return stream.str();
}

}  // namespace mini_gnb
