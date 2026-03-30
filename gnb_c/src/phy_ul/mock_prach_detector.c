#include "mini_gnb_c/phy_ul/mock_prach_detector.h"

#include <math.h>
#include <string.h>

void mini_gnb_c_mock_prach_detector_init(mini_gnb_c_mock_prach_detector_t* detector,
                                         const mini_gnb_c_sim_config_t* config) {
  if (detector == NULL || config == NULL) {
    return;
  }
  memset(detector, 0, sizeof(*detector));
  memcpy(&detector->config, config, sizeof(*config));
}

bool mini_gnb_c_mock_prach_detector_detect(mini_gnb_c_mock_prach_detector_t* detector,
                                           const mini_gnb_c_slot_indication_t* slot,
                                           const mini_gnb_c_radio_burst_t* burst,
                                           mini_gnb_c_prach_indication_t* out_prach) {
  double energy_acc = 0.0;
  size_t i = 0;

  if (detector == NULL || slot == NULL || burst == NULL || out_prach == NULL) {
    return false;
  }

  if (!slot->has_prach_occasion || burst->ul_type != MINI_GNB_C_UL_BURST_PRACH || burst->nof_samples == 0U) {
    return false;
  }

  for (i = 0; i < burst->nof_samples; ++i) {
    const double real = burst->samples[i].real;
    const double imag = burst->samples[i].imag;
    energy_acc += (real * real) + (imag * imag);
  }

  memset(out_prach, 0, sizeof(*out_prach));
  out_prach->sfn = slot->sfn;
  out_prach->slot = slot->slot;
  out_prach->abs_slot = slot->abs_slot;
  out_prach->preamble_id = burst->preamble_id;
  out_prach->ta_est = burst->ta_est;
  out_prach->peak_metric = burst->peak_metric > 0.0 ? burst->peak_metric : sqrt(energy_acc);
  out_prach->snr_est = burst->snr_db > 0.0 ? burst->snr_db : 20.0;
  out_prach->valid = true;
  return true;
}
