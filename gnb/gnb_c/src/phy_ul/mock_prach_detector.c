#include "mini_gnb_c/phy_ul/mock_prach_detector.h"

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
  (void)burst;

  if (detector == NULL || slot == NULL || out_prach == NULL) {
    return false;
  }

  if (detector->fired || !slot->has_prach_occasion || slot->abs_slot != detector->config.prach_trigger_abs_slot) {
    return false;
  }

  detector->fired = true;
  memset(out_prach, 0, sizeof(*out_prach));
  out_prach->sfn = slot->sfn;
  out_prach->slot = slot->slot;
  out_prach->abs_slot = slot->abs_slot;
  out_prach->preamble_id = detector->config.preamble_id;
  out_prach->ta_est = detector->config.ta_est;
  out_prach->peak_metric = detector->config.peak_metric;
  out_prach->snr_est = 20.0;
  out_prach->valid = true;
  return true;
}
