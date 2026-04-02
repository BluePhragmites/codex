#include "mini_gnb_c/ue/mini_ue_fsm.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"

static void mini_gnb_c_mini_ue_build_msg3_mac_pdu(const mini_gnb_c_sim_config_t* sim,
                                                  const uint16_t tc_rnti,
                                                  mini_gnb_c_buffer_t* out_mac_pdu) {
  uint8_t contention_id[16];
  uint8_t ue_identity[16];
  size_t contention_id_len = 0u;
  size_t ue_identity_len = 0u;
  mini_gnb_c_buffer_t ccch;

  if (sim == NULL || out_mac_pdu == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_mac_pdu);
  if (mini_gnb_c_hex_to_bytes(sim->contention_id_hex, contention_id, sizeof(contention_id), &contention_id_len) != 0 ||
      mini_gnb_c_hex_to_bytes(sim->ue_identity_hex, ue_identity, sizeof(ue_identity), &ue_identity_len) != 0) {
    return;
  }

  mini_gnb_c_buffer_reset(&ccch);
  memcpy(ccch.bytes, contention_id, contention_id_len);
  ccch.bytes[contention_id_len] = sim->establishment_cause;
  ccch.bytes[contention_id_len + 1u] = sim->ue_identity_type;
  memcpy(&ccch.bytes[contention_id_len + 2u], ue_identity, ue_identity_len);
  ccch.len = contention_id_len + 2u + ue_identity_len;

  if (sim->include_crnti_ce) {
    out_mac_pdu->bytes[out_mac_pdu->len++] = 2u;
    out_mac_pdu->bytes[out_mac_pdu->len++] = 2u;
    out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)(tc_rnti & 0xffu);
    out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)((tc_rnti >> 8u) & 0xffu);
  }

  out_mac_pdu->bytes[out_mac_pdu->len++] = 1u;
  out_mac_pdu->bytes[out_mac_pdu->len++] = (uint8_t)ccch.len;
  memcpy(&out_mac_pdu->bytes[out_mac_pdu->len], ccch.bytes, ccch.len);
  out_mac_pdu->len += ccch.len;
}

static void mini_gnb_c_mini_ue_build_ul_payload(const mini_gnb_c_sim_config_t* sim,
                                                const mini_gnb_c_ul_data_purpose_t purpose,
                                                mini_gnb_c_buffer_t* out_payload) {
  char bsr_text[MINI_GNB_C_MAX_TEXT];
  size_t ul_data_len = 0u;

  if (sim == NULL || out_payload == NULL) {
    return;
  }

  mini_gnb_c_buffer_reset(out_payload);
  if (purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR) {
    if (snprintf(bsr_text, sizeof(bsr_text), "BSR|bytes=%d", sim->ul_bsr_buffer_size_bytes) >= (int)sizeof(bsr_text)) {
      return;
    }
    (void)mini_gnb_c_buffer_set_text(out_payload, bsr_text);
    return;
  }

  if (mini_gnb_c_hex_to_bytes(sim->ul_data_hex, out_payload->bytes, sizeof(out_payload->bytes), &ul_data_len) != 0) {
    return;
  }
  out_payload->len = ul_data_len;
}

static bool mini_gnb_c_mini_ue_has_connected_traffic(const mini_gnb_c_mini_ue_fsm_t* fsm) {
  return fsm != NULL && fsm->sim.post_msg4_traffic_enabled && fsm->sim.ul_data_present;
}

void mini_gnb_c_mini_ue_fsm_init(mini_gnb_c_mini_ue_fsm_t* fsm, const mini_gnb_c_sim_config_t* sim) {
  if (fsm == NULL || sim == NULL) {
    return;
  }

  memset(fsm, 0, sizeof(*fsm));
  memcpy(&fsm->sim, sim, sizeof(*sim));
  fsm->next_type = MINI_GNB_C_UE_EVENT_PRACH;
  fsm->next_sequence = 1u;
  fsm->tc_rnti = 0x4601u;
  fsm->msg3_abs_slot = sim->prach_trigger_abs_slot + sim->msg3_delay_slots;
  fsm->msg4_abs_slot = fsm->msg3_abs_slot + sim->msg4_delay_slots;
  fsm->sr_abs_slot = fsm->msg4_abs_slot + sim->post_msg4_dl_data_delay_slots + sim->post_msg4_ul_grant_delay_slots;
  fsm->bsr_abs_slot = fsm->sr_abs_slot + 1 + sim->post_msg4_ul_data_k2;
  fsm->data_abs_slot = fsm->bsr_abs_slot + 1 + sim->post_msg4_ul_data_k2;
}

bool mini_gnb_c_mini_ue_fsm_has_pending_event(const mini_gnb_c_mini_ue_fsm_t* fsm) {
  return fsm != NULL && fsm->next_type != MINI_GNB_C_UE_EVENT_NONE;
}

int mini_gnb_c_mini_ue_fsm_next_event(mini_gnb_c_mini_ue_fsm_t* fsm, mini_gnb_c_ue_event_t* out_event) {
  if (fsm == NULL || out_event == NULL || fsm->next_type == MINI_GNB_C_UE_EVENT_NONE) {
    return 0;
  }

  memset(out_event, 0, sizeof(*out_event));
  out_event->type = fsm->next_type;
  out_event->sequence = fsm->next_sequence++;

  switch (fsm->next_type) {
    case MINI_GNB_C_UE_EVENT_PRACH:
      out_event->abs_slot = fsm->sim.prach_trigger_abs_slot;
      out_event->preamble_id = fsm->sim.preamble_id;
      out_event->ta_est = fsm->sim.ta_est;
      out_event->peak_metric = fsm->sim.peak_metric;
      fsm->next_type = fsm->sim.msg3_present ? MINI_GNB_C_UE_EVENT_MSG3 : MINI_GNB_C_UE_EVENT_NONE;
      break;
    case MINI_GNB_C_UE_EVENT_MSG3:
      out_event->abs_slot = fsm->msg3_abs_slot;
      out_event->rnti = fsm->tc_rnti;
      mini_gnb_c_mini_ue_build_msg3_mac_pdu(&fsm->sim, fsm->tc_rnti, &out_event->payload);
      fsm->next_type = mini_gnb_c_mini_ue_has_connected_traffic(fsm) ? MINI_GNB_C_UE_EVENT_PUCCH_SR
                                                                      : MINI_GNB_C_UE_EVENT_NONE;
      break;
    case MINI_GNB_C_UE_EVENT_PUCCH_SR:
      out_event->abs_slot = fsm->sr_abs_slot;
      out_event->rnti = fsm->tc_rnti;
      fsm->next_type = MINI_GNB_C_UE_EVENT_BSR;
      break;
    case MINI_GNB_C_UE_EVENT_BSR:
      out_event->abs_slot = fsm->bsr_abs_slot;
      out_event->rnti = fsm->tc_rnti;
      out_event->purpose = MINI_GNB_C_UL_DATA_PURPOSE_BSR;
      mini_gnb_c_mini_ue_build_ul_payload(&fsm->sim, MINI_GNB_C_UL_DATA_PURPOSE_BSR, &out_event->payload);
      fsm->next_type = MINI_GNB_C_UE_EVENT_DATA;
      break;
    case MINI_GNB_C_UE_EVENT_DATA:
      out_event->abs_slot = fsm->data_abs_slot;
      out_event->rnti = fsm->tc_rnti;
      out_event->purpose = MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD;
      mini_gnb_c_mini_ue_build_ul_payload(&fsm->sim, MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD, &out_event->payload);
      fsm->next_type = MINI_GNB_C_UE_EVENT_NONE;
      break;
    case MINI_GNB_C_UE_EVENT_NONE:
      return 0;
  }

  return 1;
}

const char* mini_gnb_c_ue_event_type_to_string(const mini_gnb_c_ue_event_type_t type) {
  switch (type) {
    case MINI_GNB_C_UE_EVENT_NONE:
      return "NONE";
    case MINI_GNB_C_UE_EVENT_PRACH:
      return "PRACH";
    case MINI_GNB_C_UE_EVENT_MSG3:
      return "MSG3";
    case MINI_GNB_C_UE_EVENT_PUCCH_SR:
      return "PUCCH_SR";
    case MINI_GNB_C_UE_EVENT_BSR:
      return "BSR";
    case MINI_GNB_C_UE_EVENT_DATA:
      return "DATA";
  }
  return "UNKNOWN";
}

int mini_gnb_c_ue_event_build_payload_json(const mini_gnb_c_ue_event_t* event, char* out, const size_t out_size) {
  char payload_hex[MINI_GNB_C_MAX_PAYLOAD * 2u + 1u];

  if (event == NULL || out == NULL || out_size == 0u) {
    return -1;
  }

  switch (event->type) {
    case MINI_GNB_C_UE_EVENT_PRACH:
      return snprintf(out,
                      out_size,
                      "{\"preamble_id\":%u,\"ta_est\":%d,\"peak_metric\":%.1f}",
                      event->preamble_id,
                      event->ta_est,
                      event->peak_metric) < (int)out_size
                 ? 0
                 : -1;
    case MINI_GNB_C_UE_EVENT_MSG3:
    case MINI_GNB_C_UE_EVENT_DATA:
      if (mini_gnb_c_bytes_to_hex(event->payload.bytes, event->payload.len, payload_hex, sizeof(payload_hex)) != 0) {
        return -1;
      }
      return snprintf(out,
                      out_size,
                      "{\"rnti\":%u,\"payload_hex\":\"%s\"}",
                      event->rnti,
                      payload_hex) < (int)out_size
                 ? 0
                 : -1;
    case MINI_GNB_C_UE_EVENT_PUCCH_SR:
      return snprintf(out, out_size, "{\"rnti\":%u}", event->rnti) < (int)out_size ? 0 : -1;
    case MINI_GNB_C_UE_EVENT_BSR:
      return snprintf(out,
                      out_size,
                      "{\"rnti\":%u,\"purpose\":\"BSR\",\"payload_text\":\"%.*s\"}",
                      event->rnti,
                      (int)event->payload.len,
                      (const char*)event->payload.bytes) < (int)out_size
                 ? 0
                 : -1;
    case MINI_GNB_C_UE_EVENT_NONE:
      break;
  }

  return -1;
}
