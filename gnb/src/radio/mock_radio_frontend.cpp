#include "mini_gnb/radio/radio_frontend.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>

#include "mini_gnb/common/json_utils.hpp"
#include "mini_gnb/metrics/metrics_trace.hpp"

namespace mini_gnb {

namespace {

std::string iq_basename(const TxGridPatch& patch, const std::uint64_t tx_index) {
  return "slot_" + std::to_string(patch.abs_slot) +
         "_" + to_string(patch.type) +
         "_rnti_" + std::to_string(patch.rnti) +
         "_tx_" + std::to_string(tx_index);
}

void write_cf32_file(const std::string& path, const std::vector<ComplexSample>& iq_samples) {
  std::ofstream output(path, std::ios::binary);
  for (const auto& sample : iq_samples) {
    output.write(reinterpret_cast<const char*>(&sample.i), sizeof(sample.i));
    output.write(reinterpret_cast<const char*>(&sample.q), sizeof(sample.q));
  }
}

void write_iq_metadata(const std::string& path, const TxGridPatch& patch) {
  write_text_file(path,
                  json_object({
                      {"abs_slot", std::to_string(patch.abs_slot)},
                      {"sfn", std::to_string(patch.sfn)},
                      {"slot", std::to_string(patch.slot)},
                      {"type", json_quote(to_string(patch.type))},
                      {"rnti", std::to_string(patch.rnti)},
                      {"prb_start", std::to_string(patch.prb_start)},
                      {"prb_len", std::to_string(patch.prb_len)},
                      {"payload_len", std::to_string(patch.payload_len)},
                      {"fft_size", std::to_string(patch.fft_size)},
                      {"cp_len", std::to_string(patch.cp_len)},
                      {"sample_rate_hz", std::to_string(static_cast<long long>(patch.sample_rate_hz))},
                      {"sample_count", std::to_string(patch.iq_samples.size())},
                      {"format", json_quote("cf32_le_interleaved")},
                      {"generator", json_quote("toy_ofdm_mock_dl_phy")},
                  }));
}

}  // namespace

MockRadioFrontend::MockRadioFrontend(RfConfig config) : config_(std::move(config)) {}

RadioBurst MockRadioFrontend::receive(const SlotIndication& slot) {
  last_hw_time_ns_ = slot.slot_start_ns;
  RadioBurst burst;
  burst.hw_time_ns = slot.slot_start_ns;
  burst.sfn = slot.sfn;
  burst.slot = slot.slot;
  burst.nof_samples = 0;
  burst.status = RadioStatus{slot.slot_start_ns, false, false, 0.0};
  return burst;
}

void MockRadioFrontend::submit_tx(const SlotIndication& slot,
                                  const std::vector<TxGridPatch>& patches,
                                  MetricsTrace& metrics) {
  const auto iq_dir = join_path(metrics.output_dir(), "iq");
  std::filesystem::create_directories(iq_dir);

  for (const auto& patch : patches) {
    ++tx_burst_count_;
    const auto base = iq_basename(patch, tx_burst_count_);
    const auto iq_path = join_path(iq_dir, base + ".cf32");
    const auto meta_path = join_path(iq_dir, base + ".json");
    write_cf32_file(iq_path, patch.iq_samples);
    write_iq_metadata(meta_path, patch);

    metrics.trace("radio_tx",
                  "Submitted DL burst.",
                  slot.abs_slot,
                  {
                      {"type", to_string(patch.type)},
                      {"rnti", std::to_string(patch.rnti)},
                      {"prb_start", std::to_string(patch.prb_start)},
                      {"prb_len", std::to_string(patch.prb_len)},
                      {"payload_len", std::to_string(patch.payload_len)},
                      {"sample_count", std::to_string(patch.iq_samples.size())},
                      {"iq_path", iq_path},
                  });
  }
}

std::uint64_t MockRadioFrontend::tx_burst_count() const {
  return tx_burst_count_;
}

std::int64_t MockRadioFrontend::last_hw_time_ns() const {
  return last_hw_time_ns_;
}

}  // namespace mini_gnb
