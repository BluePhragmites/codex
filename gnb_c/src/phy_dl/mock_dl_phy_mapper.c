#include "mini_gnb_c/phy_dl/mock_dl_phy_mapper.h"

#include <math.h>
#include <string.h>

enum {
  MINI_GNB_C_TOY_FFT_SIZE = 128,
  MINI_GNB_C_TOY_CP_LENGTH = 16,
  MINI_GNB_C_TOY_NOF_SYMBOLS = 14,
  MINI_GNB_C_TOY_MAX_ACTIVE_SC = 72
};

static mini_gnb_c_complexf_t mini_gnb_c_qpsk_symbol(const uint8_t dibit, const float scale) {
  mini_gnb_c_complexf_t symbol;
  symbol.real = ((dibit & 0x1U) != 0U) ? scale : -scale;
  symbol.imag = ((dibit & 0x2U) != 0U) ? scale : -scale;
  return symbol;
}

static uint8_t mini_gnb_c_payload_dibit(const mini_gnb_c_buffer_t* payload, size_t symbol_index) {
  size_t bit_pair_index = 0;
  size_t byte_index = 0;
  size_t bit_offset = 0;

  if (payload == NULL || payload->len == 0U) {
    return (uint8_t)(symbol_index & 0x3U);
  }

  bit_pair_index = symbol_index % (payload->len * 4U);
  byte_index = bit_pair_index / 4U;
  bit_offset = (bit_pair_index % 4U) * 2U;
  return (uint8_t)((payload->bytes[byte_index] >> bit_offset) & 0x3U);
}

static float mini_gnb_c_object_amplitude(mini_gnb_c_dl_object_type_t type) {
  switch (type) {
    case MINI_GNB_C_DL_OBJ_SSB:
      return 0.10f;
    case MINI_GNB_C_DL_OBJ_SIB1:
      return 0.08f;
    case MINI_GNB_C_DL_OBJ_RAR:
      return 0.12f;
    case MINI_GNB_C_DL_OBJ_MSG4:
      return 0.14f;
    case MINI_GNB_C_DL_OBJ_DATA:
      return 0.11f;
    case MINI_GNB_C_DL_OBJ_PDCCH:
      return 0.06f;
  }
  return 0.08f;
}

static void mini_gnb_c_generate_toy_waveform(const mini_gnb_c_dl_grant_t* grant,
                                             mini_gnb_c_tx_grid_patch_t* patch) {
  mini_gnb_c_complexf_t freq[MINI_GNB_C_TOY_FFT_SIZE];
  mini_gnb_c_complexf_t time_domain[MINI_GNB_C_TOY_FFT_SIZE];
  size_t out_index = 0;
  int symbol = 0;
  int n = 0;
  int k = 0;
  const int active_sc_requested = (int)grant->prb_len * 12;
  const int active_sc =
      (active_sc_requested < MINI_GNB_C_TOY_MAX_ACTIVE_SC) ? active_sc_requested : MINI_GNB_C_TOY_MAX_ACTIVE_SC;
  const float amplitude = mini_gnb_c_object_amplitude(grant->type);
  const double two_pi = 6.28318530717958647692;

  patch->fft_size = MINI_GNB_C_TOY_FFT_SIZE;
  patch->cp_length = MINI_GNB_C_TOY_CP_LENGTH;
  patch->sample_count = 0;
  memset(patch->samples, 0, sizeof(patch->samples));

  for (symbol = 0; symbol < MINI_GNB_C_TOY_NOF_SYMBOLS; ++symbol) {
    memset(freq, 0, sizeof(freq));
    memset(time_domain, 0, sizeof(time_domain));

    for (k = 0; k < active_sc; ++k) {
      const int centered_sc = (k - (active_sc / 2)) + (k >= (active_sc / 2) ? 1 : 0);
      const int bin = (centered_sc < 0) ? (MINI_GNB_C_TOY_FFT_SIZE + centered_sc) : centered_sc;
      const uint8_t dibit = mini_gnb_c_payload_dibit(&grant->payload,
                                                     (size_t)(symbol * active_sc + k + grant->rnti));
      freq[bin] = mini_gnb_c_qpsk_symbol(dibit, amplitude);
    }

    for (n = 0; n < MINI_GNB_C_TOY_FFT_SIZE; ++n) {
      double sum_real = 0.0;
      double sum_imag = 0.0;
      for (k = 0; k < MINI_GNB_C_TOY_FFT_SIZE; ++k) {
        const double angle = two_pi * (double)k * (double)n / (double)MINI_GNB_C_TOY_FFT_SIZE;
        const double cosine = cos(angle);
        const double sine = sin(angle);
        sum_real += (double)freq[k].real * cosine - (double)freq[k].imag * sine;
        sum_imag += (double)freq[k].real * sine + (double)freq[k].imag * cosine;
      }
      time_domain[n].real = (float)(sum_real / (double)MINI_GNB_C_TOY_FFT_SIZE);
      time_domain[n].imag = (float)(sum_imag / (double)MINI_GNB_C_TOY_FFT_SIZE);
    }

    for (n = MINI_GNB_C_TOY_FFT_SIZE - MINI_GNB_C_TOY_CP_LENGTH; n < MINI_GNB_C_TOY_FFT_SIZE; ++n) {
      if (out_index < MINI_GNB_C_MAX_IQ_SAMPLES) {
        patch->samples[out_index++] = time_domain[n];
      }
    }
    for (n = 0; n < MINI_GNB_C_TOY_FFT_SIZE; ++n) {
      if (out_index < MINI_GNB_C_MAX_IQ_SAMPLES) {
        patch->samples[out_index++] = time_domain[n];
      }
    }
  }

  patch->sample_count = out_index;
}

void mini_gnb_c_mock_dl_phy_mapper_init(mini_gnb_c_mock_dl_phy_mapper_t* mapper) {
  if (mapper == NULL) {
    return;
  }
  memset(mapper, 0, sizeof(*mapper));
}

size_t mini_gnb_c_mock_dl_phy_mapper_map(const mini_gnb_c_mock_dl_phy_mapper_t* mapper,
                                         const mini_gnb_c_slot_indication_t* slot,
                                         const mini_gnb_c_dl_grant_t* grants,
                                         const size_t grant_count,
                                         mini_gnb_c_tx_grid_patch_t* out_patches,
                                         const size_t max_patches) {
  size_t i = 0;
  size_t count = 0;
  (void)mapper;

  if (slot == NULL || grants == NULL || out_patches == NULL) {
    return 0;
  }

  for (i = 0; i < grant_count && count < max_patches; ++i) {
    mini_gnb_c_tx_grid_patch_t* patch = &out_patches[count++];
    memset(patch, 0, sizeof(*patch));
    patch->sfn = slot->sfn;
    patch->slot = slot->slot;
    patch->abs_slot = slot->abs_slot;
    patch->sym_start = 0;
    patch->nof_sym = 14;
    patch->prb_start = grants[i].prb_start;
    patch->prb_len = grants[i].prb_len;
    patch->type = grants[i].type;
    patch->rnti = grants[i].rnti;
    patch->payload_len = grants[i].payload.len;
    patch->pdcch = grants[i].pdcch;
    patch->payload_kind = grants[i].payload_kind;
    patch->payload = grants[i].payload;
    mini_gnb_c_generate_toy_waveform(&grants[i], patch);
  }

  return count;
}
