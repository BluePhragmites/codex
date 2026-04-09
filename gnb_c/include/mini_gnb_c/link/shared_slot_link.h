#ifndef MINI_GNB_C_LINK_SHARED_SLOT_LINK_H
#define MINI_GNB_C_LINK_SHARED_SLOT_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/common/types.h"

#define MINI_GNB_C_SHARED_SLOT_FLAG_SSB 0x0001u
#define MINI_GNB_C_SHARED_SLOT_FLAG_SIB1 0x0002u
#define MINI_GNB_C_SHARED_SLOT_FLAG_RAR 0x0004u
#define MINI_GNB_C_SHARED_SLOT_FLAG_MSG4 0x0008u
#define MINI_GNB_C_SHARED_SLOT_FLAG_DATA 0x0010u
#define MINI_GNB_C_SHARED_SLOT_FLAG_PDCCH 0x0020u
#define MINI_GNB_C_SHARED_SLOT_MAX_UL_EVENTS 16u

typedef struct {
  int abs_slot;
  uint32_t flags;
  uint16_t dl_rnti;
  mini_gnb_c_payload_kind_t dl_data_kind;
  bool ue_ipv4_valid;
  uint8_t ue_ipv4[4];
  bool has_pdcch;
  mini_gnb_c_pdcch_dci_t pdcch;
  mini_gnb_c_buffer_t sib1_payload;
  mini_gnb_c_buffer_t rar_payload;
  mini_gnb_c_buffer_t msg4_payload;
  mini_gnb_c_buffer_t dl_data_payload;
} mini_gnb_c_shared_slot_dl_summary_t;

typedef struct {
  bool valid;
  int abs_slot;
  mini_gnb_c_ul_burst_type_t type;
  uint16_t rnti;
  uint8_t preamble_id;
  int ta_est;
  double peak_metric;
  mini_gnb_c_ul_data_purpose_t purpose;
  uint8_t harq_id;
  bool ndi;
  bool is_new_data;
  mini_gnb_c_payload_kind_t payload_kind;
  mini_gnb_c_buffer_t payload;
} mini_gnb_c_shared_slot_ul_event_t;

typedef struct {
  int fd;
  void* mapping;
  size_t mapping_size;
  char path[MINI_GNB_C_MAX_PATH];
} mini_gnb_c_shared_slot_link_t;

/*
 * Shared-slot register model:
 * - gNB publishes one DL slot summary and advances `gnb_tx_slot`
 * - UE observes that summary, optionally schedules one future UL event, then advances `ue_rx_slot`
 * - gNB consumes a due UL event before publishing the next slot
 * This keeps the UE/gNB loop slot-synchronous without per-slot JSON files.
 */
int mini_gnb_c_shared_slot_link_open(mini_gnb_c_shared_slot_link_t* link, const char* path, bool reset_region);
void mini_gnb_c_shared_slot_link_close(mini_gnb_c_shared_slot_link_t* link);

int mini_gnb_c_shared_slot_link_publish_gnb_slot(mini_gnb_c_shared_slot_link_t* link,
                                                 const mini_gnb_c_shared_slot_dl_summary_t* summary);
int mini_gnb_c_shared_slot_link_wait_for_gnb_slot(mini_gnb_c_shared_slot_link_t* link,
                                                  int target_abs_slot,
                                                  uint32_t timeout_ms,
                                                  mini_gnb_c_shared_slot_dl_summary_t* out_summary);
int mini_gnb_c_shared_slot_link_mark_ue_progress(mini_gnb_c_shared_slot_link_t* link, int abs_slot);
int mini_gnb_c_shared_slot_link_wait_for_ue_progress(mini_gnb_c_shared_slot_link_t* link,
                                                     int target_abs_slot,
                                                     uint32_t timeout_ms);
int mini_gnb_c_shared_slot_link_schedule_ul(mini_gnb_c_shared_slot_link_t* link,
                                            const mini_gnb_c_shared_slot_ul_event_t* event);
int mini_gnb_c_shared_slot_link_consume_ul(mini_gnb_c_shared_slot_link_t* link,
                                           int abs_slot,
                                           mini_gnb_c_shared_slot_ul_event_t* out_event);
int mini_gnb_c_shared_slot_link_mark_gnb_done(mini_gnb_c_shared_slot_link_t* link);
int mini_gnb_c_shared_slot_link_mark_ue_done(mini_gnb_c_shared_slot_link_t* link);
bool mini_gnb_c_shared_slot_link_gnb_done(const mini_gnb_c_shared_slot_link_t* link);

#endif
