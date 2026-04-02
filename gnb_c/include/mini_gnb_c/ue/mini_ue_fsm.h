#ifndef MINI_GNB_C_UE_MINI_UE_FSM_H
#define MINI_GNB_C_UE_MINI_UE_FSM_H

#include <stdbool.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

typedef enum {
  MINI_GNB_C_UE_EVENT_NONE = 0,
  MINI_GNB_C_UE_EVENT_PRACH = 1,
  MINI_GNB_C_UE_EVENT_MSG3 = 2,
  MINI_GNB_C_UE_EVENT_PUCCH_SR = 3,
  MINI_GNB_C_UE_EVENT_BSR = 4,
  MINI_GNB_C_UE_EVENT_DATA = 5
} mini_gnb_c_ue_event_type_t;

typedef struct {
  mini_gnb_c_ue_event_type_t type;
  uint32_t sequence;
  int abs_slot;
  uint16_t rnti;
  uint8_t preamble_id;
  int ta_est;
  double peak_metric;
  mini_gnb_c_ul_data_purpose_t purpose;
  mini_gnb_c_buffer_t payload;
} mini_gnb_c_ue_event_t;

typedef struct {
  mini_gnb_c_sim_config_t sim;
  mini_gnb_c_ue_event_type_t next_type;
  uint32_t next_sequence;
  uint16_t tc_rnti;
  int msg3_abs_slot;
  int msg4_abs_slot;
  int sr_abs_slot;
  int bsr_abs_slot;
  int data_abs_slot;
} mini_gnb_c_mini_ue_fsm_t;

void mini_gnb_c_mini_ue_fsm_init(mini_gnb_c_mini_ue_fsm_t* fsm, const mini_gnb_c_sim_config_t* sim);
bool mini_gnb_c_mini_ue_fsm_has_pending_event(const mini_gnb_c_mini_ue_fsm_t* fsm);
int mini_gnb_c_mini_ue_fsm_next_event(mini_gnb_c_mini_ue_fsm_t* fsm, mini_gnb_c_ue_event_t* out_event);
const char* mini_gnb_c_ue_event_type_to_string(mini_gnb_c_ue_event_type_t type);
int mini_gnb_c_ue_event_build_payload_json(const mini_gnb_c_ue_event_t* event, char* out, size_t out_size);

#endif
