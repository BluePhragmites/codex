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
  grant->pdcch.scheduled_abs_slot = grant->abs_slot;
  grant->pdcch.scheduled_dl_type = grant->type;
  grant->pdcch.scheduled_ul_type = MINI_GNB_C_UL_BURST_NONE;
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
                                  mini_gnb_c_buffer_t* out_payload) {
  char text[MINI_GNB_C_MAX_PAYLOAD];
  (void)snprintf(text,
                 sizeof(text),
                 "SIB1|plmn=%s|tac=%u|pci=%u|band=%u|prach_cfg=%u",
                 cell->plmn,
                 cell->tac,
                 cell->pci,
                 cell->band,
                 prach->prach_config_index);
  (void)mini_gnb_c_buffer_set_text(out_payload, text);
}

void mini_gnb_c_broadcast_engine_init(mini_gnb_c_broadcast_engine_t* engine,
                                      const mini_gnb_c_cell_config_t* cell,
                                      const mini_gnb_c_prach_config_t* prach,
                                      const mini_gnb_c_broadcast_config_t* broadcast) {
  if (engine == NULL || cell == NULL || prach == NULL || broadcast == NULL) {
    return;
  }
  memcpy(&engine->cell, cell, sizeof(*cell));
  memcpy(&engine->prach, prach, sizeof(*prach));
  memcpy(&engine->broadcast, broadcast, sizeof(*broadcast));
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
    mini_gnb_c_build_sib1(&engine->cell, &engine->prach, &grant->payload);
  }

  return count;
}
