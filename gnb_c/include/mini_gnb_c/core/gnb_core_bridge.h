#ifndef MINI_GNB_C_CORE_GNB_CORE_BRIDGE_H
#define MINI_GNB_C_CORE_GNB_CORE_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"
#include "mini_gnb_c/metrics/metrics_trace.h"
#include "mini_gnb_c/ngap/ngap_transport.h"
#include "mini_gnb_c/trace/pcap_trace.h"

#define MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE 2048

typedef struct {
  mini_gnb_c_core_config_t config;
  mini_gnb_c_ngap_transport_t transport;
  char local_exchange_dir[MINI_GNB_C_MAX_PATH];
  uint16_t next_ran_ue_ngap_id;
  uint32_t next_gnb_to_ue_sequence;
  uint32_t next_ue_to_gnb_nas_sequence;
  bool ng_setup_complete;
  bool initial_message_sent;
  uint8_t last_initial_ue_message[MINI_GNB_C_CORE_BRIDGE_MAX_MESSAGE];
  size_t last_initial_ue_message_length;
  int last_initial_ue_message_abs_slot;
  uint8_t last_downlink_nas[MINI_GNB_C_MAX_PAYLOAD];
  size_t last_downlink_nas_length;
  int last_downlink_nas_abs_slot;
  mini_gnb_c_pcap_writer_t ngap_trace_writer;
} mini_gnb_c_gnb_core_bridge_t;

void mini_gnb_c_gnb_core_bridge_init(mini_gnb_c_gnb_core_bridge_t* bridge,
                                     const mini_gnb_c_core_config_t* config,
                                     const char* local_exchange_dir);
int mini_gnb_c_gnb_core_bridge_set_ngap_trace_path(mini_gnb_c_gnb_core_bridge_t* bridge, const char* path);
const char* mini_gnb_c_gnb_core_bridge_get_ngap_trace_path(const mini_gnb_c_gnb_core_bridge_t* bridge);
void mini_gnb_c_gnb_core_bridge_close(mini_gnb_c_gnb_core_bridge_t* bridge);

int mini_gnb_c_gnb_core_bridge_on_ue_promoted(mini_gnb_c_gnb_core_bridge_t* bridge,
                                              mini_gnb_c_ue_context_t* ue_context,
                                              mini_gnb_c_metrics_trace_t* metrics,
                                              int abs_slot);

int mini_gnb_c_gnb_core_bridge_poll_ue_nas(mini_gnb_c_gnb_core_bridge_t* bridge,
                                           mini_gnb_c_ue_context_t* ue_contexts,
                                           size_t ue_context_count,
                                           mini_gnb_c_metrics_trace_t* metrics,
                                           int abs_slot);

#endif
