#include "mini_gnb_c/broadcast/broadcast_engine.h"

#include <stdio.h>
#include <string.h>

static void mini_gnb_c_assign_dl_pdcch(mini_gnb_c_dl_grant_t* grant) {
  if (grant == NULL) {
    return;
  }

  memset(&grant->pdcch, 0, sizeof(grant->pdcch));
  grant->pdcch.valid = true;
  grant->pdcch.format = MINI_GNB_C_DCI_FORMAT_1_0;
  grant->pdcch.rnti = grant->rnti;
  grant->pdcch.coreset_prb_start = 0U;
  grant->pdcch.coreset_prb_len = 12U;
  grant->pdcch.scheduled_prb_start = grant->prb_start;
  grant->pdcch.scheduled_prb_len = grant->prb_len;
  grant->pdcch.mcs = grant->mcs;
  grant->pdcch.k2 = -1;
  grant->pdcch.time_indicator = 0;
  grant->pdcch.dl_data_to_ul_ack = 0;
  grant->pdcch.scheduled_abs_slot = grant->abs_slot;
  grant->pdcch.scheduled_dl_type = grant->type;
  grant->pdcch.scheduled_ul_type = MINI_GNB_C_UL_BURST_NONE;
  grant->pdcch.harq_id = 0u;
  grant->pdcch.ndi = true;
  grant->pdcch.is_new_data = true;
}

static void mini_gnb_c_build_mib(const mini_gnb_c_cell_config_t* cell,
                                 const mini_gnb_c_slot_indication_t* slot,
                                 mini_gnb_c_buffer_t* out_payload) {
  char text[MINI_GNB_C_MAX_PAYLOAD];
  (void)snprintf(text,
                 sizeof(text),
                 "MIB|pci=%u|arfcn=%u|scs=%u|sfn=%u",
                 cell->pci,
                 cell->dl_arfcn,
                 cell->common_scs_khz,
                 slot->sfn);
  (void)mini_gnb_c_buffer_set_text(out_payload, text);
}

static void mini_gnb_c_build_sib1(const mini_gnb_c_cell_config_t* cell,
                                  const mini_gnb_c_prach_config_t* prach,
                                  const mini_gnb_c_broadcast_config_t* broadcast,
                                  const mini_gnb_c_sim_config_t* sim,
                                  mini_gnb_c_buffer_t* out_payload) {
  char text[MINI_GNB_C_MAX_PAYLOAD];
  (void)snprintf(text,
                 sizeof(text),
                 "SIB1|plmn=%s|tac=%u|pci=%u|band=%u|prach_cfg=%u|prach_period_slots=%d|prach_offset_slot=%d|ra_resp_window=%u|prach_retry_delay_slots=%d|dl_pdcch_delay_slots=%d|dl_time_indicator=%d|dl_data_to_ul_ack_slots=%d|ul_grant_delay_slots=%d|ul_time_indicator=%d|dl_harq_process_count=%d|ul_harq_process_count=%d",
                 cell->plmn,
                 cell->tac,
                 cell->pci,
                 cell->band,
                 prach->prach_config_index,
                 broadcast != NULL ? broadcast->prach_period_slots : -1,
                 broadcast != NULL ? broadcast->prach_offset_slot : -1,
                 prach->ra_resp_window,
                 sim != NULL ? sim->prach_retry_delay_slots : -1,
                 sim != NULL ? sim->post_msg4_dl_pdcch_delay_slots : -1,
                 sim != NULL ? sim->post_msg4_dl_time_indicator : -1,
                 sim != NULL ? sim->post_msg4_dl_data_to_ul_ack_slots : -1,
                 sim != NULL ? sim->post_msg4_ul_grant_delay_slots : -1,
                 sim != NULL ? sim->post_msg4_ul_time_indicator : -1,
                 sim != NULL ? sim->post_msg4_dl_harq_process_count : -1,
                 sim != NULL ? sim->post_msg4_ul_harq_process_count : -1);
  (void)mini_gnb_c_buffer_set_text(out_payload, text);
}

void mini_gnb_c_broadcast_engine_init(mini_gnb_c_broadcast_engine_t* engine,
                                      const mini_gnb_c_cell_config_t* cell,
                                      const mini_gnb_c_prach_config_t* prach,
                                      const mini_gnb_c_broadcast_config_t* broadcast,
                                      const mini_gnb_c_sim_config_t* sim) {
  if (engine == NULL || cell == NULL || prach == NULL || broadcast == NULL || sim == NULL) {
    return;
  }
  memcpy(&engine->cell, cell, sizeof(*cell));
  memcpy(&engine->prach, prach, sizeof(*prach));
  memcpy(&engine->broadcast, broadcast, sizeof(*broadcast));
  memcpy(&engine->sim, sim, sizeof(*sim));
}

size_t mini_gnb_c_broadcast_schedule(const mini_gnb_c_broadcast_engine_t* engine,
                                     const mini_gnb_c_slot_indication_t* slot,
                                     mini_gnb_c_dl_grant_t* out_grants,
                                     const size_t max_grants) {
  size_t count = 0;

  if (engine == NULL || slot == NULL || out_grants == NULL || max_grants == 0U) {
    return 0;
  }

  if (slot->has_ssb && count < max_grants) {
    mini_gnb_c_dl_grant_t* grant = &out_grants[count++];
    memset(grant, 0, sizeof(*grant));
    grant->type = MINI_GNB_C_DL_OBJ_SSB;
    grant->abs_slot = slot->abs_slot;
    grant->rnti = 0;
    grant->prb_start = 0;
    grant->prb_len = 20;
    grant->mcs = 0;
    mini_gnb_c_build_mib(&engine->cell, slot, &grant->payload);
  }

  if (slot->has_sib1 && count < max_grants) {
    mini_gnb_c_dl_grant_t* grant = &out_grants[count++];
    memset(grant, 0, sizeof(*grant));
    grant->type = MINI_GNB_C_DL_OBJ_SIB1;
    grant->abs_slot = slot->abs_slot;
    grant->rnti = 0xFFFFU;
    grant->prb_start = 20;
    grant->prb_len = 24;
    grant->mcs = 4;
    mini_gnb_c_assign_dl_pdcch(grant);
    mini_gnb_c_build_sib1(&engine->cell, &engine->prach, &engine->broadcast, &engine->sim, &grant->payload);
  }

  return count;
}
