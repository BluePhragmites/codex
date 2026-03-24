#include "mini_gnb/phy_dl/dl_phy_mapper.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>
#include <vector>

namespace mini_gnb {

namespace {

constexpr float kPi = 3.14159265358979323846F;

std::uint32_t next_power_of_two(std::uint32_t value) {
  std::uint32_t result = 1;
  while (result < value) {
    result <<= 1U;
  }
  return result;
}

std::vector<std::complex<float>> bytes_to_qpsk(const ByteVector& payload) {
  ByteVector effective_payload = payload;
  if (effective_payload.empty()) {
    effective_payload = {0x5AU, 0xA5U};
  }

  std::vector<std::complex<float>> symbols;
  symbols.reserve(effective_payload.size() * 4U);
  constexpr float kScale = 0.70710678F;

  for (const auto byte : effective_payload) {
    for (int shift = 6; shift >= 0; shift -= 2) {
      const auto bits = static_cast<std::uint8_t>((byte >> shift) & 0x03U);
      float i = ((bits & 0x02U) != 0U) ? 1.0F : -1.0F;
      float q = ((bits & 0x01U) != 0U) ? 1.0F : -1.0F;
      symbols.emplace_back(i * kScale, q * kScale);
    }
  }

  return symbols;
}

std::vector<ComplexSample> synthesize_toy_ofdm(const DlGrant& grant, const double sample_rate_hz) {
  const auto qpsk_symbols = bytes_to_qpsk(grant.payload);
  const auto requested_subcarriers = static_cast<std::uint32_t>(std::max<std::uint16_t>(12U, grant.prb_len * 12U));
  const auto fft_size = next_power_of_two(std::max<std::uint32_t>(128U, requested_subcarriers * 2U));
  const auto usable_subcarriers = std::min<std::uint32_t>(requested_subcarriers, (fft_size / 2U) - 2U);
  const auto cp_len = std::max<std::uint32_t>(8U, fft_size / 8U);
  constexpr std::uint32_t symbols_per_slot = 14U;

  std::vector<ComplexSample> iq_samples;
  iq_samples.reserve(static_cast<std::size_t>(symbols_per_slot) * static_cast<std::size_t>(fft_size + cp_len));

  for (std::uint32_t symbol_index = 0; symbol_index < symbols_per_slot; ++symbol_index) {
    std::vector<std::complex<float>> freq_bins(fft_size, {0.0F, 0.0F});
    const auto half = usable_subcarriers / 2U;
    const auto phase = 2.0F * kPi * static_cast<float>(symbol_index) / static_cast<float>(symbols_per_slot);
    const std::complex<float> rotation(std::cos(phase), std::sin(phase));

    for (std::uint32_t sc = 0; sc < usable_subcarriers; ++sc) {
      const auto symbol = qpsk_symbols[(symbol_index * usable_subcarriers + sc) % qpsk_symbols.size()] * rotation;
      std::uint32_t bin = 0;
      if (sc < half) {
        bin = (fft_size - half) + sc;
      } else {
        bin = 1U + (sc - half);
      }
      freq_bins[bin] = symbol;
    }

    std::vector<std::complex<float>> time_domain(fft_size, {0.0F, 0.0F});
    for (std::uint32_t n = 0; n < fft_size; ++n) {
      std::complex<float> accum(0.0F, 0.0F);
      for (std::uint32_t k = 0; k < fft_size; ++k) {
        const auto angle = 2.0F * kPi * static_cast<float>(k * n) / static_cast<float>(fft_size);
        const std::complex<float> twiddle(std::cos(angle), std::sin(angle));
        accum += freq_bins[k] * twiddle;
      }
      time_domain[n] = accum / std::sqrt(static_cast<float>(fft_size));
    }

    for (std::uint32_t cp_index = 0; cp_index < cp_len; ++cp_index) {
      const auto& sample = time_domain[fft_size - cp_len + cp_index];
      iq_samples.push_back(ComplexSample{sample.real(), sample.imag()});
    }

    for (const auto& sample : time_domain) {
      iq_samples.push_back(ComplexSample{sample.real(), sample.imag()});
    }
  }

  const auto normalization = 1.0F / std::sqrt(static_cast<float>(std::max(1.0, sample_rate_hz / 1.92e6)));
  for (auto& sample : iq_samples) {
    sample.i *= normalization;
    sample.q *= normalization;
  }

  return iq_samples;
}

}  // namespace

MockDlPhyMapper::MockDlPhyMapper(const double sample_rate_hz) : sample_rate_hz_(sample_rate_hz) {}

std::vector<TxGridPatch> MockDlPhyMapper::map(const SlotIndication& slot,
                                              const std::vector<DlGrant>& grants) {
  std::vector<TxGridPatch> patches;
  patches.reserve(grants.size());
  for (const auto& grant : grants) {
    const auto requested_subcarriers = static_cast<std::uint32_t>(std::max<std::uint16_t>(12U, grant.prb_len * 12U));
    const auto fft_size = next_power_of_two(std::max<std::uint32_t>(128U, requested_subcarriers * 2U));
    const auto cp_len = std::max<std::uint32_t>(8U, fft_size / 8U);
    auto iq_samples = synthesize_toy_ofdm(grant, sample_rate_hz_);
    patches.push_back(TxGridPatch{
        slot.sfn,
        slot.slot,
        slot.abs_slot,
        0,
        14,
        grant.prb_start,
        grant.prb_len,
        grant.type,
        grant.rnti,
        grant.payload.size(),
        fft_size,
        cp_len,
        sample_rate_hz_,
        std::move(iq_samples),
    });
  }
  return patches;
}

}  // namespace mini_gnb
