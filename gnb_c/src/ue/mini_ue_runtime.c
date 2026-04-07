#include "mini_gnb_c/ue/mini_ue_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/link/shared_slot_link.h"
#include "mini_gnb_c/nas/nas_5gs_min.h"
#include "mini_gnb_c/ue/ue_ip_stack_min.h"
#include "mini_gnb_c/ue/ue_tun.h"

typedef struct {
  bool valid;
  uint16_t rnti;
  int pdsch_abs_slot;
  int ack_abs_slot;
  uint8_t harq_id;
  bool ndi;
  bool is_new_data;
} mini_gnb_c_mini_ue_dl_harq_wait_t;

typedef struct {
  bool valid;
  bool last_ndi;
  mini_gnb_c_ul_data_purpose_t last_purpose;
  mini_gnb_c_buffer_t last_payload;
} mini_gnb_c_mini_ue_ul_harq_state_t;

typedef struct {
  mini_gnb_c_sim_config_t sim;
  mini_gnb_c_shared_slot_link_t link;
  bool prach_cfg_valid;
  int prach_period_slots;
  int prach_offset_slot;
  int ra_resp_window_slots;
  int prach_retry_delay_slots;
  int dl_pdcch_delay_slots;
  int dl_time_indicator;
  int dl_data_to_ul_ack_slots;
  int ul_grant_delay_slots;
  int ul_time_indicator;
  int dl_harq_process_count;
  int ul_harq_process_count;
  bool prach_inflight;
  int last_prach_abs_slot;
  int earliest_next_prach_abs_slot;
  bool msg3_pending;
  int msg3_abs_slot;
  uint16_t tc_rnti;
  bool sr_cfg_valid;
  int sr_period_slots;
  int sr_offset_slot;
  bool sr_scheduled;
  mini_gnb_c_mini_ue_dl_harq_wait_t dl_harq_wait[MINI_GNB_C_MAX_HARQ_PROCESSES];
  mini_gnb_c_mini_ue_ul_harq_state_t ul_harq[MINI_GNB_C_MAX_HARQ_PROCESSES];
  mini_gnb_c_ue_ip_stack_min_t ip_stack;
  mini_gnb_c_nas_5gs_min_ue_t nas_5gs;
  mini_gnb_c_ue_tun_t tun;
  bool pending_tun_uplink_valid;
  mini_gnb_c_buffer_t pending_tun_uplink_packet;
  bool tun_network_ready_announced;
} mini_gnb_c_mini_ue_runtime_t;

static int mini_gnb_c_next_periodic_slot_at_or_after(const int min_abs_slot,
                                                     const int period_slots,
                                                     const int offset_slot) {
  int candidate = 0;

  if (period_slots <= 0 || offset_slot < 0) {
    return -1;
  }
  if (min_abs_slot <= offset_slot) {
    return offset_slot;
  }

  candidate = min_abs_slot;
  while (((candidate - offset_slot) % period_slots) != 0) {
    ++candidate;
  }
  return candidate;
}

static int mini_gnb_c_extract_text_int(const char* text, const char* key, int* out_value) {
  char pattern[64];
  const char* value_start = NULL;
  char* end_ptr = NULL;

  if (text == NULL || key == NULL || out_value == NULL) {
    return -1;
  }
  if (snprintf(pattern, sizeof(pattern), "%s=", key) >= (int)sizeof(pattern)) {
    return -1;
  }

  value_start = strstr(text, pattern);
  if (value_start == NULL) {
    return -1;
  }
  value_start += strlen(pattern);
  *out_value = (int)strtol(value_start, &end_ptr, 10);
  if (end_ptr == value_start || (*end_ptr != '\0' && *end_ptr != '|')) {
    return -1;
  }
  return 0;
}

static int mini_gnb_c_clamp_harq_id(const int harq_id, const int configured_count) {
  const int max_count = configured_count > 0 ? configured_count : 1;

  if (harq_id < 0) {
    return 0;
  }
  if (harq_id >= MINI_GNB_C_MAX_HARQ_PROCESSES) {
    return MINI_GNB_C_MAX_HARQ_PROCESSES - 1;
  }
  if (harq_id >= max_count) {
    return max_count - 1;
  }
  return harq_id;
}

static int mini_gnb_c_schedule_ul_event(mini_gnb_c_mini_ue_runtime_t* runtime,
                                        const mini_gnb_c_shared_slot_ul_event_t* event) {
  int schedule_result = 0;

  if (runtime == NULL || event == NULL) {
    return -1;
  }

  schedule_result = mini_gnb_c_shared_slot_link_schedule_ul(&runtime->link, event);
  if (schedule_result != 0) {
    return -1;
  }
  return 0;
}

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

static void mini_gnb_c_mini_ue_build_harq_ul_payload(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                     const uint8_t harq_id,
                                                     const bool ndi,
                                                     const bool is_new_data,
                                                     const mini_gnb_c_ul_data_purpose_t purpose,
                                                     mini_gnb_c_buffer_t* out_payload,
                                                     bool* used_pending_tun_payload,
                                                     bool* used_pending_ip_payload) {
  mini_gnb_c_mini_ue_ul_harq_state_t* harq_state = NULL;

  if (used_pending_tun_payload != NULL) {
    *used_pending_tun_payload = false;
  }
  if (used_pending_ip_payload != NULL) {
    *used_pending_ip_payload = false;
  }
  if (runtime == NULL || out_payload == NULL || harq_id >= MINI_GNB_C_MAX_HARQ_PROCESSES) {
    return;
  }

  harq_state = &runtime->ul_harq[harq_id];
  if (!is_new_data && harq_state->valid && harq_state->last_purpose == purpose && harq_state->last_ndi == ndi) {
    *out_payload = harq_state->last_payload;
    return;
  }

  if (is_new_data && purpose == MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD && runtime->pending_tun_uplink_valid) {
    *out_payload = runtime->pending_tun_uplink_packet;
    if (used_pending_tun_payload != NULL) {
      *used_pending_tun_payload = true;
    }
  } else if (is_new_data && purpose == MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD &&
      mini_gnb_c_ue_ip_stack_min_copy_pending_uplink(&runtime->ip_stack, out_payload) == 0) {
    if (used_pending_ip_payload != NULL) {
      *used_pending_ip_payload = true;
    }
  } else {
    mini_gnb_c_mini_ue_build_ul_payload(&runtime->sim, purpose, out_payload);
  }
  harq_state->valid = true;
  harq_state->last_ndi = ndi;
  harq_state->last_purpose = purpose;
  harq_state->last_payload = *out_payload;
}

static int mini_gnb_c_parse_sib1_payload(mini_gnb_c_mini_ue_runtime_t* runtime,
                                         const mini_gnb_c_buffer_t* sib1_payload) {
  char text[MINI_GNB_C_MAX_PAYLOAD + 1];

  if (runtime == NULL || sib1_payload == NULL || sib1_payload->len == 0u || sib1_payload->len > MINI_GNB_C_MAX_PAYLOAD) {
    return -1;
  }

  memcpy(text, sib1_payload->bytes, sib1_payload->len);
  text[sib1_payload->len] = '\0';
  if (strncmp(text, "SIB1|", 5) != 0) {
    return -1;
  }
  if (mini_gnb_c_extract_text_int(text, "prach_period_slots", &runtime->prach_period_slots) != 0 ||
      mini_gnb_c_extract_text_int(text, "prach_offset_slot", &runtime->prach_offset_slot) != 0 ||
      mini_gnb_c_extract_text_int(text, "ra_resp_window", &runtime->ra_resp_window_slots) != 0 ||
      mini_gnb_c_extract_text_int(text, "prach_retry_delay_slots", &runtime->prach_retry_delay_slots) != 0) {
    return -1;
  }
  (void)mini_gnb_c_extract_text_int(text, "dl_pdcch_delay_slots", &runtime->dl_pdcch_delay_slots);
  (void)mini_gnb_c_extract_text_int(text, "dl_time_indicator", &runtime->dl_time_indicator);
  (void)mini_gnb_c_extract_text_int(text, "dl_data_to_ul_ack_slots", &runtime->dl_data_to_ul_ack_slots);
  (void)mini_gnb_c_extract_text_int(text, "ul_grant_delay_slots", &runtime->ul_grant_delay_slots);
  (void)mini_gnb_c_extract_text_int(text, "ul_time_indicator", &runtime->ul_time_indicator);
  (void)mini_gnb_c_extract_text_int(text, "dl_harq_process_count", &runtime->dl_harq_process_count);
  (void)mini_gnb_c_extract_text_int(text, "ul_harq_process_count", &runtime->ul_harq_process_count);
  runtime->prach_cfg_valid = true;
  if (runtime->earliest_next_prach_abs_slot < 0) {
    runtime->earliest_next_prach_abs_slot = 0;
  }
  return 0;
}

static int mini_gnb_c_parse_rar_payload(mini_gnb_c_mini_ue_runtime_t* runtime,
                                        const mini_gnb_c_buffer_t* rar_payload) {
  if (runtime == NULL || rar_payload == NULL || rar_payload->len < 12u) {
    return -1;
  }
  if (rar_payload->bytes[0] != runtime->sim.preamble_id) {
    return -1;
  }

  runtime->tc_rnti = (uint16_t)rar_payload->bytes[8] | ((uint16_t)rar_payload->bytes[9] << 8u);
  runtime->msg3_abs_slot = (int)rar_payload->bytes[10] | ((int)rar_payload->bytes[11] << 8u);
  runtime->msg3_pending = true;
  runtime->prach_inflight = false;
  runtime->earliest_next_prach_abs_slot = -1;
  return 0;
}

static int mini_gnb_c_parse_msg4_payload(mini_gnb_c_mini_ue_runtime_t* runtime,
                                         const mini_gnb_c_buffer_t* msg4_payload) {
  char text[MINI_GNB_C_MAX_PAYLOAD + 1];
  size_t rrc_len = 0u;

  if (runtime == NULL || msg4_payload == NULL || msg4_payload->len < 10u || msg4_payload->bytes[8] != 17u) {
    return -1;
  }

  rrc_len = (size_t)msg4_payload->bytes[9];
  if (10u + rrc_len > msg4_payload->len || rrc_len > MINI_GNB_C_MAX_PAYLOAD) {
    return -1;
  }

  memcpy(text, &msg4_payload->bytes[10], rrc_len);
  text[rrc_len] = '\0';
  if (strncmp(text, "RRCSetup|", 9) != 0) {
    return -1;
  }
  if (mini_gnb_c_extract_text_int(text, "sr_period_slots", &runtime->sr_period_slots) != 0 ||
      mini_gnb_c_extract_text_int(text, "sr_offset_slot", &runtime->sr_offset_slot) != 0) {
    return -1;
  }
  runtime->sr_cfg_valid = true;
  return 0;
}

static void mini_gnb_c_mini_ue_runtime_schedule_granted_ul(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                           const mini_gnb_c_pdcch_dci_t* pdcch,
                                                           const int current_slot) {
  mini_gnb_c_shared_slot_ul_event_t ul_event;
  const int harq_id = mini_gnb_c_clamp_harq_id((int)pdcch->harq_id, runtime->ul_harq_process_count);
  bool used_pending_tun_payload = false;
  bool used_pending_ip_payload = false;

  if (runtime == NULL || pdcch == NULL || pdcch->scheduled_ul_type != MINI_GNB_C_UL_BURST_DATA ||
      runtime->tc_rnti == 0u || pdcch->rnti != runtime->tc_rnti || pdcch->scheduled_abs_slot <= current_slot) {
    return;
  }

  memset(&ul_event, 0, sizeof(ul_event));
  ul_event.valid = true;
  ul_event.abs_slot = pdcch->scheduled_abs_slot;
  ul_event.type = MINI_GNB_C_UL_BURST_DATA;
  ul_event.rnti = runtime->tc_rnti;
  ul_event.purpose = pdcch->scheduled_ul_purpose;
  ul_event.harq_id = (uint8_t)harq_id;
  ul_event.ndi = pdcch->ndi;
  ul_event.is_new_data = pdcch->is_new_data;
  mini_gnb_c_mini_ue_build_harq_ul_payload(runtime,
                                           ul_event.harq_id,
                                           ul_event.ndi,
                                           ul_event.is_new_data,
                                           ul_event.purpose,
                                           &ul_event.payload,
                                           &used_pending_tun_payload,
                                           &used_pending_ip_payload);
  if (mini_gnb_c_schedule_ul_event(runtime, &ul_event) != 0) {
    return;
  }
  if (used_pending_tun_payload) {
    runtime->pending_tun_uplink_valid = false;
  }
  if (used_pending_ip_payload) {
    mini_gnb_c_ue_ip_stack_min_mark_uplink_consumed(&runtime->ip_stack);
  }

  printf("UE scheduled %s for abs_slot=%d via DCI harq_id=%u ndi=%s is_new_data=%s\n",
         ul_event.purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR ? "BSR" : "DATA",
         ul_event.abs_slot,
         ul_event.harq_id,
         ul_event.ndi ? "true" : "false",
         ul_event.is_new_data ? "true" : "false");
  fflush(stdout);
}

static void mini_gnb_c_mini_ue_runtime_track_dl_harq(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                      const mini_gnb_c_pdcch_dci_t* pdcch,
                                                      const int current_slot) {
  mini_gnb_c_mini_ue_dl_harq_wait_t* wait_state = NULL;
  int harq_id = 0;
  int ack_delay = 0;

  if (runtime == NULL || pdcch == NULL || pdcch->scheduled_dl_type != MINI_GNB_C_DL_OBJ_DATA ||
      runtime->tc_rnti == 0u || pdcch->rnti != runtime->tc_rnti || pdcch->scheduled_abs_slot <= current_slot) {
    return;
  }

  harq_id = mini_gnb_c_clamp_harq_id((int)pdcch->harq_id, runtime->dl_harq_process_count);
  ack_delay = pdcch->dl_data_to_ul_ack > 0 ? pdcch->dl_data_to_ul_ack : runtime->dl_data_to_ul_ack_slots;
  if (ack_delay <= 0) {
    ack_delay = 1;
  }

  wait_state = &runtime->dl_harq_wait[harq_id];
  memset(wait_state, 0, sizeof(*wait_state));
  wait_state->valid = true;
  wait_state->rnti = pdcch->rnti;
  wait_state->pdsch_abs_slot = pdcch->scheduled_abs_slot;
  wait_state->ack_abs_slot = pdcch->scheduled_abs_slot + ack_delay;
  wait_state->harq_id = (uint8_t)harq_id;
  wait_state->ndi = pdcch->ndi;
  wait_state->is_new_data = pdcch->is_new_data;
}

static void mini_gnb_c_mini_ue_runtime_schedule_dl_ack(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                       const int current_slot,
                                                       const uint16_t rnti) {
  size_t i = 0u;

  if (runtime == NULL || rnti == 0u) {
    return;
  }

  for (i = 0u; i < MINI_GNB_C_MAX_HARQ_PROCESSES; ++i) {
    mini_gnb_c_mini_ue_dl_harq_wait_t* wait_state = &runtime->dl_harq_wait[i];
    mini_gnb_c_shared_slot_ul_event_t ul_event;

    if (!wait_state->valid || wait_state->rnti != rnti || wait_state->pdsch_abs_slot != current_slot) {
      continue;
    }

    memset(&ul_event, 0, sizeof(ul_event));
    ul_event.valid = true;
    ul_event.abs_slot = wait_state->ack_abs_slot;
    ul_event.type = MINI_GNB_C_UL_BURST_PUCCH_ACK;
    ul_event.rnti = wait_state->rnti;
    ul_event.harq_id = wait_state->harq_id;
    ul_event.ndi = wait_state->ndi;
    ul_event.is_new_data = wait_state->is_new_data;
    if (mini_gnb_c_schedule_ul_event(runtime, &ul_event) == 0) {
      printf("UE scheduled PUCCH_ACK for abs_slot=%d harq_id=%u ndi=%s is_new_data=%s\n",
             ul_event.abs_slot,
             ul_event.harq_id,
             ul_event.ndi ? "true" : "false",
             ul_event.is_new_data ? "true" : "false");
      fflush(stdout);
    }

    wait_state->valid = false;
  }
}

static void mini_gnb_c_mini_ue_runtime_observe_dl(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                  const mini_gnb_c_shared_slot_dl_summary_t* dl_summary,
                                                  const int current_slot) {
  int ip_handle_result = 0;

  if (runtime == NULL || dl_summary == NULL) {
    return;
  }

  if (dl_summary->ue_ipv4_valid) {
    const bool tun_was_configured = runtime->tun.configured;
    const bool ue_ipv4_changed =
        !runtime->ip_stack.local_ipv4_valid ||
        memcmp(runtime->ip_stack.local_ipv4, dl_summary->ue_ipv4, sizeof(runtime->ip_stack.local_ipv4)) != 0;

    runtime->ip_stack.local_ipv4_valid = true;
    memcpy(runtime->ip_stack.local_ipv4, dl_summary->ue_ipv4, sizeof(runtime->ip_stack.local_ipv4));
    if (runtime->tun.opened && mini_gnb_c_ue_tun_configure_ipv4(&runtime->tun, dl_summary->ue_ipv4) == 0 &&
        (!tun_was_configured || ue_ipv4_changed)) {
      const char* netns_name = mini_gnb_c_ue_tun_netns_name(&runtime->tun);

      printf("UE configured TUN %s with IPv4 %u.%u.%u.%u/%u\n",
             runtime->tun.ifname,
             dl_summary->ue_ipv4[0],
             dl_summary->ue_ipv4[1],
             dl_summary->ue_ipv4[2],
             dl_summary->ue_ipv4[3],
             runtime->tun.prefix_len);
      if (runtime->tun.default_route_configured) {
        printf("UE installed default route via %s inside the UE namespace\n", runtime->tun.ifname);
      }
      if (netns_name != NULL) {
        printf("UE published isolated netns '%s'; try `ip netns exec %s ping -c 4 8.8.8.8`\n",
               netns_name,
               netns_name);
        if (runtime->tun.dns_server_ipv4[0] != '\0') {
          printf("UE wrote /etc/netns/%s/resolv.conf with DNS %s; try `ip netns exec %s ping -c 4 www.baidu.com`\n",
                 netns_name,
                 runtime->tun.dns_server_ipv4,
                 netns_name);
        }
      } else if (runtime->tun.isolate_netns && !runtime->tun_network_ready_announced) {
        printf("UE is running in an anonymous isolated netns; use `nsenter -n -t %ld ping -c 4 8.8.8.8`\n",
               (long)getpid());
      }
      runtime->tun_network_ready_announced = true;
      fflush(stdout);
    }
  }
  if ((dl_summary->flags & MINI_GNB_C_SHARED_SLOT_FLAG_SIB1) != 0u) {
    (void)mini_gnb_c_parse_sib1_payload(runtime, &dl_summary->sib1_payload);
  }
  if ((dl_summary->flags & MINI_GNB_C_SHARED_SLOT_FLAG_RAR) != 0u) {
    (void)mini_gnb_c_parse_rar_payload(runtime, &dl_summary->rar_payload);
  }
  if ((dl_summary->flags & MINI_GNB_C_SHARED_SLOT_FLAG_MSG4) != 0u && dl_summary->dl_rnti == runtime->tc_rnti) {
    (void)mini_gnb_c_parse_msg4_payload(runtime, &dl_summary->msg4_payload);
  }
  if (dl_summary->has_pdcch) {
    mini_gnb_c_mini_ue_runtime_schedule_granted_ul(runtime, &dl_summary->pdcch, current_slot);
    mini_gnb_c_mini_ue_runtime_track_dl_harq(runtime, &dl_summary->pdcch, current_slot);
  }
  if ((dl_summary->flags & MINI_GNB_C_SHARED_SLOT_FLAG_DATA) != 0u && dl_summary->dl_rnti == runtime->tc_rnti) {
    if (runtime->tun.opened && runtime->tun.configured &&
        mini_gnb_c_ue_tun_write_packet(&runtime->tun,
                                       dl_summary->dl_data_payload.bytes,
                                       dl_summary->dl_data_payload.len) == 0) {
      printf("UE injected DL DATA into TUN for abs_slot=%d\n", current_slot);
      fflush(stdout);
    } else {
      ip_handle_result = mini_gnb_c_ue_ip_stack_min_handle_downlink(&runtime->ip_stack, &dl_summary->dl_data_payload);
      if (ip_handle_result > 0) {
        printf("UE generated ICMP echo reply from DL DATA for abs_slot=%d\n", current_slot);
        fflush(stdout);
      }
    }
    mini_gnb_c_mini_ue_runtime_schedule_dl_ack(runtime, current_slot, dl_summary->dl_rnti);
  }
}

static void mini_gnb_c_mini_ue_runtime_poll_tun(mini_gnb_c_mini_ue_runtime_t* runtime, const int current_slot) {
  uint8_t packet[MINI_GNB_C_MAX_PAYLOAD];
  size_t packet_length = 0u;
  int read_result = 0;

  if (runtime == NULL || !runtime->tun.opened || !runtime->tun.configured || runtime->pending_tun_uplink_valid) {
    return;
  }

  read_result = mini_gnb_c_ue_tun_read_packet(&runtime->tun, packet, sizeof(packet), &packet_length);
  if (read_result != 0 || !mini_gnb_c_ue_ip_stack_min_is_ipv4_packet(packet, packet_length) ||
      mini_gnb_c_buffer_set_bytes(&runtime->pending_tun_uplink_packet, packet, packet_length) != 0) {
    return;
  }

  runtime->pending_tun_uplink_valid = true;
  printf("UE read one IPv4 packet from TUN at abs_slot=%d len=%zu\n", current_slot, packet_length);
  fflush(stdout);
}

static void mini_gnb_c_mini_ue_runtime_update_retry_state(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                          const int current_slot) {
  if (runtime == NULL || !runtime->prach_inflight || runtime->msg3_pending || runtime->ra_resp_window_slots < 0) {
    return;
  }

  if (current_slot > runtime->last_prach_abs_slot + runtime->ra_resp_window_slots) {
    runtime->prach_inflight = false;
    if (runtime->prach_retry_delay_slots >= 0) {
      runtime->earliest_next_prach_abs_slot = runtime->last_prach_abs_slot + runtime->prach_retry_delay_slots;
    }
  }
}

static int mini_gnb_c_mini_ue_runtime_schedule_prach(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                     const int current_slot) {
  mini_gnb_c_shared_slot_ul_event_t ul_event;
  int target_abs_slot = 0;

  if (runtime == NULL || !runtime->prach_cfg_valid || runtime->prach_inflight || runtime->msg3_pending ||
      runtime->sr_cfg_valid || runtime->tc_rnti != 0u) {
    return 0;
  }

  target_abs_slot = mini_gnb_c_next_periodic_slot_at_or_after(current_slot + 1,
                                                              runtime->prach_period_slots,
                                                              runtime->prach_offset_slot);
  if (runtime->earliest_next_prach_abs_slot > target_abs_slot) {
    target_abs_slot = mini_gnb_c_next_periodic_slot_at_or_after(runtime->earliest_next_prach_abs_slot,
                                                                runtime->prach_period_slots,
                                                                runtime->prach_offset_slot);
  }
  if (target_abs_slot <= current_slot) {
    return 0;
  }

  memset(&ul_event, 0, sizeof(ul_event));
  ul_event.valid = true;
  ul_event.abs_slot = target_abs_slot;
  ul_event.type = MINI_GNB_C_UL_BURST_PRACH;
  ul_event.preamble_id = runtime->sim.preamble_id;
  ul_event.ta_est = runtime->sim.ta_est;
  ul_event.peak_metric = runtime->sim.peak_metric;
  if (mini_gnb_c_schedule_ul_event(runtime, &ul_event) != 0) {
    return 0;
  }

  runtime->prach_inflight = true;
  runtime->last_prach_abs_slot = target_abs_slot;
  runtime->earliest_next_prach_abs_slot = -1;
  printf("UE scheduled PRACH for abs_slot=%d\n", target_abs_slot);
  fflush(stdout);
  return 1;
}

static int mini_gnb_c_mini_ue_runtime_schedule_msg3(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                    const int current_slot) {
  mini_gnb_c_shared_slot_ul_event_t ul_event;

  if (runtime == NULL || !runtime->msg3_pending || runtime->msg3_abs_slot <= current_slot || runtime->tc_rnti == 0u) {
    return 0;
  }

  memset(&ul_event, 0, sizeof(ul_event));
  ul_event.valid = true;
  ul_event.abs_slot = runtime->msg3_abs_slot;
  ul_event.type = MINI_GNB_C_UL_BURST_MSG3;
  ul_event.rnti = runtime->tc_rnti;
  mini_gnb_c_mini_ue_build_msg3_mac_pdu(&runtime->sim, runtime->tc_rnti, &ul_event.payload);
  if (mini_gnb_c_schedule_ul_event(runtime, &ul_event) != 0) {
    return 0;
  }

  runtime->msg3_pending = false;
  printf("UE scheduled MSG3 for abs_slot=%d\n", ul_event.abs_slot);
  fflush(stdout);
  return 1;
}

static int mini_gnb_c_mini_ue_runtime_schedule_sr(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                  const int current_slot) {
  mini_gnb_c_shared_slot_ul_event_t ul_event;
  int target_abs_slot = 0;

  if (runtime == NULL || !runtime->sr_cfg_valid || runtime->sr_scheduled || runtime->tc_rnti == 0u) {
    return 0;
  }

  target_abs_slot = mini_gnb_c_next_periodic_slot_at_or_after(current_slot + 1,
                                                              runtime->sr_period_slots,
                                                              runtime->sr_offset_slot);
  if (target_abs_slot <= current_slot) {
    return 0;
  }

  memset(&ul_event, 0, sizeof(ul_event));
  ul_event.valid = true;
  ul_event.abs_slot = target_abs_slot;
  ul_event.type = MINI_GNB_C_UL_BURST_PUCCH_SR;
  ul_event.rnti = runtime->tc_rnti;
  if (mini_gnb_c_schedule_ul_event(runtime, &ul_event) != 0) {
    return 0;
  }

  runtime->sr_scheduled = true;
  printf("UE scheduled PUCCH_SR for abs_slot=%d\n", ul_event.abs_slot);
  fflush(stdout);
  return 1;
}

static int mini_gnb_c_mini_ue_runtime_schedule_ready_event(mini_gnb_c_mini_ue_runtime_t* runtime,
                                                           const int current_slot) {
  if (runtime == NULL) {
    return -1;
  }
  if (mini_gnb_c_mini_ue_runtime_schedule_msg3(runtime, current_slot) > 0) {
    return 0;
  }
  if (mini_gnb_c_mini_ue_runtime_schedule_sr(runtime, current_slot) > 0) {
    return 0;
  }
  if (mini_gnb_c_mini_ue_runtime_schedule_prach(runtime, current_slot) > 0) {
    return 0;
  }
  return 0;
}

int mini_gnb_c_run_shared_ue_runtime(const mini_gnb_c_config_t* config) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  int current_slot = 0;

  if (config == NULL || config->sim.shared_slot_path[0] == '\0') {
    return -1;
  }

  memset(&runtime, 0, sizeof(runtime));
  runtime.sim = config->sim;
  runtime.dl_pdcch_delay_slots = config->sim.post_msg4_dl_pdcch_delay_slots;
  runtime.dl_time_indicator = config->sim.post_msg4_dl_time_indicator;
  runtime.dl_data_to_ul_ack_slots = config->sim.post_msg4_dl_data_to_ul_ack_slots;
  runtime.ul_grant_delay_slots = config->sim.post_msg4_ul_grant_delay_slots;
  runtime.ul_time_indicator = config->sim.post_msg4_ul_time_indicator;
  runtime.dl_harq_process_count = config->sim.post_msg4_dl_harq_process_count;
  runtime.ul_harq_process_count = config->sim.post_msg4_ul_harq_process_count;
  runtime.earliest_next_prach_abs_slot = -1;
  runtime.last_prach_abs_slot = -1;
  runtime.msg3_abs_slot = -1;
  mini_gnb_c_ue_ip_stack_min_init(&runtime.ip_stack);
  mini_gnb_c_nas_5gs_min_ue_init(&runtime.nas_5gs);
  mini_gnb_c_ue_tun_init(&runtime.tun);

  if (config->sim.ue_tun_enabled && mini_gnb_c_ue_tun_open(&runtime.tun, &config->sim) != 0) {
    return -1;
  }

  if (mini_gnb_c_shared_slot_link_open(&runtime.link, config->sim.shared_slot_path, false) != 0) {
    mini_gnb_c_ue_tun_close(&runtime.tun);
    return -1;
  }

  while (current_slot < runtime.sim.total_slots) {
    mini_gnb_c_shared_slot_dl_summary_t dl_summary;
    const int wait_result = mini_gnb_c_shared_slot_link_wait_for_gnb_slot(&runtime.link,
                                                                          current_slot,
                                                                          runtime.sim.shared_slot_timeout_ms,
                                                                          &dl_summary);
    if (wait_result < 0) {
      mini_gnb_c_ue_tun_close(&runtime.tun);
      mini_gnb_c_shared_slot_link_close(&runtime.link);
      return -1;
    }
    if (wait_result > 0 && mini_gnb_c_shared_slot_link_gnb_done(&runtime.link)) {
      break;
    }
    if (wait_result > 0) {
      continue;
    }

    if (dl_summary.abs_slot == current_slot) {
      mini_gnb_c_mini_ue_runtime_observe_dl(&runtime, &dl_summary, current_slot);
      mini_gnb_c_mini_ue_runtime_update_retry_state(&runtime, current_slot);
    }
    if (mini_gnb_c_nas_5gs_min_poll_exchange(&runtime.nas_5gs, runtime.sim.local_exchange_dir, current_slot) != 0) {
      mini_gnb_c_ue_tun_close(&runtime.tun);
      mini_gnb_c_shared_slot_link_close(&runtime.link);
      return -1;
    }
    mini_gnb_c_mini_ue_runtime_poll_tun(&runtime, current_slot);
    if (mini_gnb_c_mini_ue_runtime_schedule_ready_event(&runtime, current_slot) < 0) {
      mini_gnb_c_ue_tun_close(&runtime.tun);
      mini_gnb_c_shared_slot_link_close(&runtime.link);
      return -1;
    }
    if (mini_gnb_c_shared_slot_link_mark_ue_progress(&runtime.link, current_slot) != 0) {
      mini_gnb_c_ue_tun_close(&runtime.tun);
      mini_gnb_c_shared_slot_link_close(&runtime.link);
      return -1;
    }
    ++current_slot;
  }

  (void)mini_gnb_c_shared_slot_link_mark_ue_done(&runtime.link);
  mini_gnb_c_ue_tun_close(&runtime.tun);
  mini_gnb_c_shared_slot_link_close(&runtime.link);
  return 0;
}
