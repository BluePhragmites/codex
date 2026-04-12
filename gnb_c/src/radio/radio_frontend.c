#include "mini_gnb_c/radio/radio_frontend.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/radio/b210_slot_backend.h"

static mini_gnb_c_radio_backend_kind_t mini_gnb_c_radio_backend_from_driver_name(const char* driver_name) {
  if (driver_name == NULL || driver_name[0] == '\0' || strcmp(driver_name, "mock") == 0) {
    return MINI_GNB_C_RADIO_BACKEND_MOCK;
  }
  if (strcmp(driver_name, "b210") == 0 || strcmp(driver_name, "uhd") == 0 ||
      strcmp(driver_name, "uhd-b210") == 0) {
    return MINI_GNB_C_RADIO_BACKEND_B210;
  }
  return MINI_GNB_C_RADIO_BACKEND_UNKNOWN;
}

static const char* mini_gnb_c_radio_backend_default_name(const mini_gnb_c_radio_backend_kind_t kind) {
  switch (kind) {
    case MINI_GNB_C_RADIO_BACKEND_MOCK:
      return "mock";
    case MINI_GNB_C_RADIO_BACKEND_B210:
      return "uhd-b210";
    case MINI_GNB_C_RADIO_BACKEND_UNKNOWN:
      break;
  }
  return "unknown";
}

static void mini_gnb_c_radio_frontend_copy_driver_name(mini_gnb_c_radio_frontend_t* radio, const char* driver_name) {
  const char* effective_name = driver_name;

  if (radio == NULL) {
    return;
  }
  if (effective_name == NULL || effective_name[0] == '\0') {
    effective_name = "mock";
  }
  (void)snprintf(radio->driver_name, sizeof(radio->driver_name), "%s", effective_name);
}

int mini_gnb_c_radio_frontend_init(mini_gnb_c_radio_frontend_t* radio,
                                   const mini_gnb_c_rf_config_t* rf_config,
                                   const mini_gnb_c_sim_config_t* sim_config) {
  const char* driver_name = NULL;

  if (radio == NULL || rf_config == NULL || sim_config == NULL) {
    return -1;
  }

  memset(radio, 0, sizeof(*radio));
  driver_name = rf_config->device_driver;
  radio->kind = mini_gnb_c_radio_backend_from_driver_name(driver_name);
  mini_gnb_c_radio_frontend_copy_driver_name(radio, driver_name);

  switch (radio->kind) {
    case MINI_GNB_C_RADIO_BACKEND_MOCK:
      mini_gnb_c_mock_radio_frontend_init(&radio->mock, rf_config, sim_config);
      radio->ready = true;
      return 0;
    case MINI_GNB_C_RADIO_BACKEND_B210:
      if (mini_gnb_c_b210_slot_backend_create(&radio->b210,
                                              rf_config,
                                              sim_config,
                                              radio->last_error,
                                              sizeof(radio->last_error)) == 0) {
        radio->ready = true;
        return 0;
      }
      break;
    case MINI_GNB_C_RADIO_BACKEND_UNKNOWN:
      (void)snprintf(radio->last_error,
                     sizeof(radio->last_error),
                     "unsupported rf.device_driver \"%s\"; supported values are \"mock\" and the planned \"uhd-b210\" target.",
                     radio->driver_name);
      break;
  }
  radio->ready = false;
  return -1;
}

bool mini_gnb_c_radio_frontend_is_ready(const mini_gnb_c_radio_frontend_t* radio) {
  return radio != NULL && radio->ready;
}

mini_gnb_c_radio_backend_kind_t mini_gnb_c_radio_frontend_kind(const mini_gnb_c_radio_frontend_t* radio) {
  return radio != NULL ? radio->kind : MINI_GNB_C_RADIO_BACKEND_UNKNOWN;
}

const char* mini_gnb_c_radio_frontend_driver_name(const mini_gnb_c_radio_frontend_t* radio) {
  if (radio == NULL) {
    return "unknown";
  }
  return radio->driver_name[0] != '\0' ? radio->driver_name : mini_gnb_c_radio_backend_default_name(radio->kind);
}

const char* mini_gnb_c_radio_frontend_error(const mini_gnb_c_radio_frontend_t* radio) {
  if (radio == NULL) {
    return "";
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210 && radio->b210 != NULL) {
    const char* backend_error = mini_gnb_c_b210_slot_backend_error(radio->b210);

    if (backend_error[0] != '\0') {
      return backend_error;
    }
  }
  if (radio->last_error[0] == '\0') {
    return "";
  }
  return radio->last_error;
}

bool mini_gnb_c_radio_frontend_has_pucch_sr_armed_for(const mini_gnb_c_radio_frontend_t* radio,
                                                      const uint16_t rnti,
                                                      const int abs_slot,
                                                      const int current_abs_slot) {
  if (radio == NULL || !radio->ready) {
    return false;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    return radio->mock.pucch_sr_armed && radio->mock.pucch_sr_abs_slot >= current_abs_slot &&
           radio->mock.pucch_sr_abs_slot == abs_slot && radio->mock.pucch_sr_rnti == rnti;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    return mini_gnb_c_b210_slot_backend_has_pucch_sr_armed_for(radio->b210, rnti, abs_slot, current_abs_slot);
  }
  return false;
}

uint64_t mini_gnb_c_radio_frontend_tx_burst_count(const mini_gnb_c_radio_frontend_t* radio) {
  if (radio == NULL) {
    return 0u;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    return radio->mock.tx_burst_count;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    return mini_gnb_c_b210_slot_backend_tx_burst_count(radio->b210);
  }
  return 0u;
}

int64_t mini_gnb_c_radio_frontend_last_hw_time_ns(const mini_gnb_c_radio_frontend_t* radio) {
  if (radio == NULL) {
    return 0;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    return radio->mock.last_hw_time_ns;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    return mini_gnb_c_b210_slot_backend_last_hw_time_ns(radio->b210);
  }
  return 0;
}

void mini_gnb_c_radio_frontend_receive(mini_gnb_c_radio_frontend_t* radio,
                                       const mini_gnb_c_slot_indication_t* slot,
                                       mini_gnb_c_radio_burst_t* out_burst) {
  if (out_burst != NULL) {
    memset(out_burst, 0, sizeof(*out_burst));
  }
  if (radio == NULL || slot == NULL || out_burst == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_receive(&radio->mock, slot, out_burst);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_receive(radio->b210, slot, out_burst);
  }
}

void mini_gnb_c_radio_frontend_arm_msg3(mini_gnb_c_radio_frontend_t* radio,
                                        const mini_gnb_c_ul_grant_for_msg3_t* ul_grant) {
  if (radio == NULL || ul_grant == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_arm_msg3(&radio->mock, ul_grant);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_arm_msg3(radio->b210, ul_grant);
  }
}

void mini_gnb_c_radio_frontend_arm_pucch_sr(mini_gnb_c_radio_frontend_t* radio, uint16_t rnti, int abs_slot) {
  if (radio == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_arm_pucch_sr(&radio->mock, rnti, abs_slot);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_arm_pucch_sr(radio->b210, rnti, abs_slot);
  }
}

void mini_gnb_c_radio_frontend_arm_dl_ack(mini_gnb_c_radio_frontend_t* radio,
                                          uint16_t rnti,
                                          uint8_t harq_id,
                                          int abs_slot) {
  if (radio == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_arm_dl_ack(&radio->mock, rnti, harq_id, abs_slot);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_arm_dl_ack(radio->b210, rnti, harq_id, abs_slot);
  }
}

void mini_gnb_c_radio_frontend_arm_ul_data(mini_gnb_c_radio_frontend_t* radio,
                                           const mini_gnb_c_ul_data_grant_t* ul_grant) {
  if (radio == NULL || ul_grant == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_arm_ul_data(&radio->mock, ul_grant);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_arm_ul_data(radio->b210, ul_grant);
  }
}

void mini_gnb_c_radio_frontend_stage_ue_ipv4(mini_gnb_c_radio_frontend_t* radio,
                                             const uint8_t ue_ipv4[4],
                                             bool ue_ipv4_valid) {
  if (radio == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_stage_ue_ipv4(&radio->mock, ue_ipv4, ue_ipv4_valid);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_stage_ue_ipv4(radio->b210, ue_ipv4, ue_ipv4_valid);
  }
}

void mini_gnb_c_radio_frontend_submit_tx(mini_gnb_c_radio_frontend_t* radio,
                                         const mini_gnb_c_slot_indication_t* slot,
                                         const mini_gnb_c_tx_grid_patch_t* patches,
                                         size_t patch_count,
                                         struct mini_gnb_c_metrics_trace* metrics) {
  if (radio == NULL || slot == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_submit_tx(&radio->mock, slot, patches, patch_count, metrics);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_submit_tx(radio->b210, slot, patches, patch_count, metrics);
  }
}

void mini_gnb_c_radio_frontend_submit_pdcch(mini_gnb_c_radio_frontend_t* radio,
                                            const mini_gnb_c_slot_indication_t* slot,
                                            const mini_gnb_c_pdcch_dci_t* pdcch,
                                            struct mini_gnb_c_metrics_trace* metrics) {
  if (radio == NULL || slot == NULL || pdcch == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_submit_pdcch(&radio->mock, slot, pdcch, metrics);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_submit_pdcch(radio->b210, slot, pdcch, metrics);
  }
}

void mini_gnb_c_radio_frontend_finalize_slot(mini_gnb_c_radio_frontend_t* radio,
                                             const mini_gnb_c_slot_indication_t* slot,
                                             struct mini_gnb_c_metrics_trace* metrics) {
  if (radio == NULL || slot == NULL || !radio->ready) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK) {
    mini_gnb_c_mock_radio_frontend_finalize_slot(&radio->mock, slot, metrics);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210) {
    mini_gnb_c_b210_slot_backend_finalize_slot(radio->b210, slot, metrics);
  }
}

void mini_gnb_c_radio_frontend_shutdown(mini_gnb_c_radio_frontend_t* radio) {
  if (radio == NULL) {
    return;
  }
  if (radio->kind == MINI_GNB_C_RADIO_BACKEND_MOCK && radio->ready) {
    mini_gnb_c_mock_radio_frontend_shutdown(&radio->mock);
  } else if (radio->kind == MINI_GNB_C_RADIO_BACKEND_B210 && radio->b210 != NULL) {
    mini_gnb_c_b210_slot_backend_destroy(&radio->b210);
  }
  radio->ready = false;
}
