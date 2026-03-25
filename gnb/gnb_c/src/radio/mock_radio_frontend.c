#include "mini_gnb_c/radio/mock_radio_frontend.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

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

void mini_gnb_c_mock_radio_frontend_init(mini_gnb_c_mock_radio_frontend_t* radio,
                                         const mini_gnb_c_rf_config_t* config) {
  if (radio == NULL || config == NULL) {
    return;
  }
  memset(radio, 0, sizeof(*radio));
  memcpy(&radio->config, config, sizeof(*config));
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
  out_burst->nof_samples = 0;
  out_burst->status.hw_time_ns = slot->slot_start_ns;
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
