#include "mini_gnb_c/radio/mock_radio_frontend.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/metrics/metrics_trace.h"

static int mini_gnb_c_ensure_directory_recursive(const char* path) {
  char temp[MINI_GNB_C_MAX_PATH];
  size_t i = 0;

  if (path == NULL) {
    return -1;
  }

  (void)snprintf(temp, sizeof(temp), "%s", path);
  for (i = 1; temp[i] != '\0'; ++i) {
    if (temp[i] == '/' || temp[i] == '\\') {
      char saved = temp[i];
      temp[i] = '\0';
      if (strlen(temp) > 0U && mkdir(temp, 0777) != 0 && errno != EEXIST) {
        temp[i] = saved;
        return -1;
      }
      temp[i] = saved;
    }
  }

  if (mkdir(temp, 0777) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_write_cf32(const char* path, const mini_gnb_c_tx_grid_patch_t* patch) {
  FILE* file = NULL;
  size_t i = 0;

  if (path == NULL || patch == NULL) {
    return -1;
  }

  file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }

  for (i = 0; i < patch->sample_count; ++i) {
    float pair[2];
    pair[0] = patch->samples[i].real;
    pair[1] = patch->samples[i].imag;
    if (fwrite(pair, sizeof(float), 2U, file) != 2U) {
      fclose(file);
      return -1;
    }
  }

  fclose(file);
  return 0;
}

static int mini_gnb_c_write_iq_metadata(const char* path,
                                        const char* cf32_path,
                                        const mini_gnb_c_slot_indication_t* slot,
                                        const mini_gnb_c_tx_grid_patch_t* patch,
                                        uint64_t tx_index) {
  FILE* file = NULL;

  if (path == NULL || cf32_path == NULL || slot == NULL || patch == NULL) {
    return -1;
  }

  file = fopen(path, "wb");
  if (file == NULL) {
    return -1;
  }

  fprintf(file,
          "{"
          "\"abs_slot\":%d,"
          "\"sfn\":%u,"
          "\"slot\":%u,"
          "\"type\":\"%s\","
          "\"rnti\":%u,"
          "\"prb_start\":%u,"
          "\"prb_len\":%u,"
          "\"payload_len\":%zu,"
          "\"fft_size\":%u,"
          "\"cp_length\":%u,"
          "\"sample_count\":%zu,"
          "\"tx_index\":%llu,"
          "\"cf32_path\":\"%s\""
          "}\n",
          slot->abs_slot,
          slot->sfn,
          slot->slot,
          mini_gnb_c_dl_object_type_to_string(patch->type),
          patch->rnti,
          patch->prb_start,
          patch->prb_len,
          patch->payload_len,
          patch->fft_size,
          patch->cp_length,
          patch->sample_count,
          (unsigned long long)tx_index,
          cf32_path);

  fclose(file);
  return 0;
}

static int mini_gnb_c_read_cf32_samples(const char* path,
                                        mini_gnb_c_complexf_t* out_samples,
                                        const size_t max_samples,
                                        size_t* out_sample_count) {
  FILE* file = NULL;
  long size_bytes = 0;
  size_t sample_count = 0;
  size_t i = 0;

  if (path == NULL || path[0] == '\0' || out_samples == NULL || out_sample_count == NULL || max_samples == 0U) {
    return -1;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    return -1;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return -1;
  }
  size_bytes = ftell(file);
  if (size_bytes <= 0 || (size_bytes % (long)(sizeof(float) * 2U)) != 0) {
    fclose(file);
    return -1;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return -1;
  }

  sample_count = (size_t)size_bytes / (sizeof(float) * 2U);
  if (sample_count > max_samples) {
    fclose(file);
    return -1;
  }

  for (i = 0; i < sample_count; ++i) {
    float pair[2];
    if (fread(pair, sizeof(float), 2U, file) != 2U) {
      fclose(file);
      return -1;
    }
    out_samples[i].real = pair[0];
    out_samples[i].imag = pair[1];
  }

  fclose(file);
  *out_sample_count = sample_count;
  return 0;
}

static void mini_gnb_c_generate_ul_waveform(mini_gnb_c_complexf_t* out_samples,
                                            size_t* out_sample_count,
                                            const size_t sample_count,
                                            const double tone_step,
                                            const double amplitude) {
  size_t i = 0;

  if (out_samples == NULL || out_sample_count == NULL) {
    return;
  }

  for (i = 0; i < sample_count; ++i) {
    const double phase = tone_step * (double)i;
    out_samples[i].real = (float)(amplitude * cos(phase));
    out_samples[i].imag = (float)(amplitude * sin(phase));
  }
  *out_sample_count = sample_count;
}

static void mini_gnb_c_build_msg3_mac_pdu(const mini_gnb_c_sim_config_t* sim,
                                          const uint16_t tc_rnti,
                                          mini_gnb_c_buffer_t* out_mac_pdu) {
  uint8_t contention_id[16];
  uint8_t ue_identity[16];
  size_t contention_id_len = 0;
  size_t ue_identity_len = 0;
  mini_gnb_c_buffer_t ccch;

  if (sim == NULL || out_mac_pdu == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_mac_pdu);
  if (mini_gnb_c_hex_to_bytes(sim->contention_id_hex,
                              contention_id,
                              sizeof(contention_id),
                              &contention_id_len) != 0) {
    return;
  }
  if (mini_gnb_c_hex_to_bytes(sim->ue_identity_hex,
                              ue_identity,
                              sizeof(ue_identity),
                              &ue_identity_len) != 0) {
    return;
  }

  mini_gnb_c_buffer_reset(&ccch);
  memcpy(ccch.bytes, contention_id, contention_id_len);
  ccch.bytes[contention_id_len] = sim->establishment_cause;
  ccch.bytes[contention_id_len + 1U] = sim->ue_identity_type;
  memcpy(&ccch.bytes[contention_id_len + 2U], ue_identity, ue_identity_len);
  ccch.len = contention_id_len + 2U + ue_identity_len;

  if (sim->include_crnti_ce) {
    out_mac_pdu->bytes[out_mac_pdu->len++] = 2U;
    out_mac_pdu->bytes[out_mac_pdu->len++] = 2U;
    out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)(tc_rnti & 0xFFU);
    out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)((tc_rnti >> 8U) & 0xFFU);
  }

  out_mac_pdu->bytes[out_mac_pdu->len++] = 1U;
  out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)ccch.len;
  memcpy(&out_mac_pdu->bytes[out_mac_pdu->len], ccch.bytes, ccch.len);
  out_mac_pdu->len += ccch.len;
}

static void mini_gnb_c_fill_ul_burst(mini_gnb_c_radio_burst_t* out_burst,
                                     const mini_gnb_c_ul_burst_type_t type,
                                     const mini_gnb_c_complexf_t* samples,
                                     const size_t sample_count) {
  if (out_burst == NULL || samples == NULL || sample_count > MINI_GNB_C_MAX_IQ_SAMPLES) {
    return;
  }

  out_burst->ul_type = type;
  out_burst->nof_samples = (uint32_t)sample_count;
  memcpy(out_burst->samples, samples, sample_count * sizeof(samples[0]));
}

void mini_gnb_c_mock_radio_frontend_init(mini_gnb_c_mock_radio_frontend_t* radio,
                                         const mini_gnb_c_rf_config_t* rf_config,
                                         const mini_gnb_c_sim_config_t* sim_config) {
  if (radio == NULL || rf_config == NULL || sim_config == NULL) {
    return;
  }

  memset(radio, 0, sizeof(*radio));
  memcpy(&radio->config, rf_config, sizeof(*rf_config));
  memcpy(&radio->sim, sim_config, sizeof(*sim_config));
  radio->retry_prach_abs_slot = -1;

  if (mini_gnb_c_read_cf32_samples(sim_config->ul_prach_cf32_path,
                                   radio->prach_samples,
                                   MINI_GNB_C_MAX_IQ_SAMPLES,
                                   &radio->prach_sample_count) != 0) {
    mini_gnb_c_generate_ul_waveform(radio->prach_samples,
                                    &radio->prach_sample_count,
                                    256U,
                                    0.0975,
                                    0.35);
  }

  if (mini_gnb_c_read_cf32_samples(sim_config->ul_msg3_cf32_path,
                                   radio->msg3_samples,
                                   MINI_GNB_C_MAX_IQ_SAMPLES,
                                   &radio->msg3_sample_count) != 0) {
    mini_gnb_c_generate_ul_waveform(radio->msg3_samples,
                                    &radio->msg3_sample_count,
                                    320U,
                                    0.1425,
                                    0.25);
  }
}

void mini_gnb_c_mock_radio_frontend_receive(mini_gnb_c_mock_radio_frontend_t* radio,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            mini_gnb_c_radio_burst_t* out_burst) {
  if (radio == NULL || slot == NULL || out_burst == NULL) {
    return;
  }

  memset(out_burst, 0, sizeof(*out_burst));
  radio->last_hw_time_ns = slot->slot_start_ns;
  out_burst->hw_time_ns = slot->slot_start_ns;
  out_burst->sfn = slot->sfn;
  out_burst->slot = slot->slot;
  out_burst->status.hw_time_ns = slot->slot_start_ns;

  if (!radio->initial_prach_emitted && slot->abs_slot == radio->sim.prach_trigger_abs_slot) {
    radio->initial_prach_emitted = true;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_PRACH,
                             radio->prach_samples,
                             radio->prach_sample_count);
    out_burst->preamble_id = radio->sim.preamble_id;
    out_burst->ta_est = radio->sim.ta_est;
    out_burst->peak_metric = radio->sim.peak_metric;
    out_burst->snr_db = 20.0;
    return;
  }

  if (radio->msg3_armed && slot->abs_slot == radio->msg3_abs_slot && radio->sim.msg3_present) {
    radio->msg3_armed = false;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_MSG3,
                             radio->msg3_samples,
                             radio->msg3_sample_count);
    out_burst->rnti = radio->msg3_rnti;
    out_burst->snr_db = radio->sim.msg3_snr_db;
    out_burst->evm = radio->sim.msg3_evm;
    out_burst->mac_pdu = radio->msg3_mac_pdu;
    return;
  }

  if (radio->msg3_armed && slot->abs_slot > radio->msg3_abs_slot) {
    radio->msg3_armed = false;
  }

  if (radio->retry_prach_armed && slot->abs_slot == radio->retry_prach_abs_slot) {
    radio->retry_prach_armed = false;
    mini_gnb_c_fill_ul_burst(out_burst,
                             MINI_GNB_C_UL_BURST_PRACH,
                             radio->prach_samples,
                             radio->prach_sample_count);
    out_burst->preamble_id = radio->sim.preamble_id;
    out_burst->ta_est = radio->sim.ta_est;
    out_burst->peak_metric = radio->sim.peak_metric;
    out_burst->snr_db = 20.0;
  }
}

void mini_gnb_c_mock_radio_frontend_arm_msg3(mini_gnb_c_mock_radio_frontend_t* radio,
                                             const mini_gnb_c_ul_grant_for_msg3_t* ul_grant) {
  if (radio == NULL || ul_grant == NULL) {
    return;
  }

  radio->msg3_armed = true;
  radio->msg3_abs_slot = ul_grant->abs_slot;
  radio->msg3_rnti = ul_grant->tc_rnti;
  mini_gnb_c_build_msg3_mac_pdu(&radio->sim, radio->msg3_rnti, &radio->msg3_mac_pdu);

  if (!radio->sim.msg3_present && radio->sim.prach_retry_delay_slots >= 0) {
    radio->retry_prach_armed = true;
    radio->retry_prach_abs_slot = ul_grant->abs_slot + radio->sim.prach_retry_delay_slots;
  } else {
    radio->retry_prach_armed = false;
    radio->retry_prach_abs_slot = -1;
  }
}

void mini_gnb_c_mock_radio_frontend_submit_tx(mini_gnb_c_mock_radio_frontend_t* radio,
                                              const mini_gnb_c_slot_indication_t* slot,
                                              const mini_gnb_c_tx_grid_patch_t* patches,
                                              const size_t patch_count,
                                              mini_gnb_c_metrics_trace_t* metrics) {
  size_t i = 0;
  char iq_dir[MINI_GNB_C_MAX_PATH];
  char details[MINI_GNB_C_MAX_EVENT_TEXT];

  if (radio == NULL || slot == NULL || patches == NULL || metrics == NULL) {
    return;
  }

  if (mini_gnb_c_join_path(metrics->output_dir, "iq", iq_dir, sizeof(iq_dir)) == 0) {
    (void)mini_gnb_c_ensure_directory_recursive(iq_dir);
  } else {
    iq_dir[0] = '\0';
  }

  for (i = 0; i < patch_count; ++i) {
    const mini_gnb_c_tx_grid_patch_t* patch = &patches[i];
    char cf32_path[MINI_GNB_C_MAX_PATH];
    char json_path[MINI_GNB_C_MAX_PATH];
    int export_ok = 0;

    ++radio->tx_burst_count;

    export_ok = (iq_dir[0] != '\0') &&
                (snprintf(cf32_path,
                          sizeof(cf32_path),
                          "%s/slot_%d_%s_rnti_%u.cf32",
                          iq_dir,
                          slot->abs_slot,
                          mini_gnb_c_dl_object_type_to_string(patch->type),
                          patch->rnti) < (int)sizeof(cf32_path)) &&
                (snprintf(json_path,
                          sizeof(json_path),
                          "%s/slot_%d_%s_rnti_%u.json",
                          iq_dir,
                          slot->abs_slot,
                          mini_gnb_c_dl_object_type_to_string(patch->type),
                          patch->rnti) < (int)sizeof(json_path)) &&
                (mini_gnb_c_write_cf32(cf32_path, patch) == 0) &&
                (mini_gnb_c_write_iq_metadata(json_path,
                                              cf32_path,
                                              slot,
                                              patch,
                                              radio->tx_burst_count) == 0);

    (void)snprintf(details,
                   sizeof(details),
                   "type=%s,rnti=%u,prb_start=%u,prb_len=%u,payload_len=%zu,sample_count=%zu%s%s",
                   mini_gnb_c_dl_object_type_to_string(patch->type),
                   patch->rnti,
                   patch->prb_start,
                   patch->prb_len,
                   patch->payload_len,
                   patch->sample_count,
                   export_ok ? ",iq_path=" : "",
                   export_ok ? cf32_path : "");
    mini_gnb_c_metrics_trace_event(metrics, "radio_tx", "Submitted DL burst.", slot->abs_slot, "%s", details);
  }
}
