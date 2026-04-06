#ifndef MINI_GNB_C_COMMON_TYPES_H
#define MINI_GNB_C_COMMON_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mini_gnb_c/core/core_session.h"

#define MINI_GNB_C_MAX_PAYLOAD 512
#define MINI_GNB_C_MAX_TEXT 128
#define MINI_GNB_C_MAX_PATH 260
#define MINI_GNB_C_MAX_EVENTS 128
#define MINI_GNB_C_MAX_EVENT_TEXT 256
#define MINI_GNB_C_MAX_LCID_SEQUENCE 8
#define MINI_GNB_C_MAX_GRANTS 8
#define MINI_GNB_C_MAX_MSG3_GRANTS 4
#define MINI_GNB_C_MAX_UL_DATA_GRANTS 4
#define MINI_GNB_C_MAX_UES 1
#define MINI_GNB_C_MAX_IQ_SAMPLES 2048
#define MINI_GNB_C_MAX_HARQ_PROCESSES 8

typedef enum {
  MINI_GNB_C_DL_OBJ_SSB = 0,
  MINI_GNB_C_DL_OBJ_SIB1 = 1,
  MINI_GNB_C_DL_OBJ_RAR = 2,
  MINI_GNB_C_DL_OBJ_MSG4 = 3,
  MINI_GNB_C_DL_OBJ_DATA = 4,
  MINI_GNB_C_DL_OBJ_PDCCH = 5
} mini_gnb_c_dl_object_type_t;

typedef enum {
  MINI_GNB_C_UL_BURST_NONE = 0,
  MINI_GNB_C_UL_BURST_PRACH = 1,
  MINI_GNB_C_UL_BURST_MSG3 = 2,
  MINI_GNB_C_UL_BURST_PUCCH_SR = 3,
  MINI_GNB_C_UL_BURST_DATA = 4,
  MINI_GNB_C_UL_BURST_PUCCH_ACK = 5
} mini_gnb_c_ul_burst_type_t;

typedef enum {
  MINI_GNB_C_DCI_FORMAT_0_0 = 0,
  MINI_GNB_C_DCI_FORMAT_0_1 = 1,
  MINI_GNB_C_DCI_FORMAT_1_0 = 2,
  MINI_GNB_C_DCI_FORMAT_1_1 = 3
} mini_gnb_c_dci_format_t;

typedef enum {
  MINI_GNB_C_RA_IDLE = 0,
  MINI_GNB_C_RA_PRACH_DETECTED = 1,
  MINI_GNB_C_RA_TC_RNTI_ASSIGNED = 2,
  MINI_GNB_C_RA_RAR_SENT = 3,
  MINI_GNB_C_RA_MSG3_WAIT = 4,
  MINI_GNB_C_RA_MSG3_OK = 5,
  MINI_GNB_C_RA_MSG4_SENT = 6,
  MINI_GNB_C_RA_DONE = 7,
  MINI_GNB_C_RA_FAIL = 8
} mini_gnb_c_ra_state_t;

typedef enum {
  MINI_GNB_C_UL_DATA_PURPOSE_BSR = 0,
  MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD = 1
} mini_gnb_c_ul_data_purpose_t;

typedef struct {
  uint8_t bytes[MINI_GNB_C_MAX_PAYLOAD];
  size_t len;
} mini_gnb_c_buffer_t;

typedef struct {
  float real;
  float imag;
} mini_gnb_c_complexf_t;

typedef struct {
  uint32_t dl_arfcn;
  uint16_t band;
  uint16_t channel_bandwidth_mhz;
  uint16_t common_scs_khz;
  uint16_t pci;
  char plmn[8];
  uint16_t tac;
  uint8_t ss0_index;
  uint8_t coreset0_index;
} mini_gnb_c_cell_config_t;

typedef struct {
  uint16_t prach_config_index;
  uint16_t prach_root_seq_index;
  uint8_t zero_correlation_zone;
  uint8_t ra_resp_window;
  int8_t msg3_delta_preamble;
} mini_gnb_c_prach_config_t;

typedef struct {
  char device_driver[16];
  char device_args[64];
  char clock_src[16];
  double srate;
  double tx_gain;
  double rx_gain;
} mini_gnb_c_rf_config_t;

typedef struct {
  bool enabled;
  char amf_ip[MINI_GNB_C_CORE_MAX_IPV4_TEXT];
  uint32_t amf_port;
  uint16_t upf_port;
  uint32_t timeout_ms;
  uint16_t ran_ue_ngap_id_base;
  uint8_t default_pdu_session_id;
} mini_gnb_c_core_config_t;

typedef struct {
  int ssb_period_slots;
  int sib1_period_slots;
  int sib1_offset_slot;
  int prach_period_slots;
  int prach_offset_slot;
} mini_gnb_c_broadcast_config_t;

typedef struct {
  int total_slots;
  int slots_per_frame;
  uint32_t slot_sleep_ms;
  int msg3_delay_slots;
  int msg4_delay_slots;
  int prach_trigger_abs_slot;
  int prach_retry_delay_slots;
  uint8_t preamble_id;
  int ta_est;
  double peak_metric;
  bool msg3_present;
  bool msg3_crc_ok;
  double msg3_snr_db;
  double msg3_evm;
  bool post_msg4_traffic_enabled;
  int post_msg4_dl_pdcch_delay_slots;
  int post_msg4_dl_time_indicator;
  int post_msg4_dl_data_to_ul_ack_slots;
  int post_msg4_sr_period_slots;
  int post_msg4_sr_offset_slot;
  int post_msg4_ul_grant_delay_slots;
  int post_msg4_ul_time_indicator;
  int post_msg4_dl_harq_process_count;
  int post_msg4_ul_harq_process_count;
  bool ul_data_present;
  bool ul_data_crc_ok;
  double ul_data_snr_db;
  double ul_data_evm;
  int ul_bsr_buffer_size_bytes;
  char ul_prach_cf32_path[MINI_GNB_C_MAX_PATH];
  char ul_msg3_cf32_path[MINI_GNB_C_MAX_PATH];
  char ul_input_dir[MINI_GNB_C_MAX_PATH];
  char local_exchange_dir[MINI_GNB_C_MAX_PATH];
  char shared_slot_path[MINI_GNB_C_MAX_PATH];
  uint32_t shared_slot_timeout_ms;
  bool ue_tun_enabled;
  char ue_tun_name[MINI_GNB_C_MAX_TEXT];
  uint16_t ue_tun_mtu;
  uint8_t ue_tun_prefix_len;
  bool ue_tun_isolate_netns;
  char scripted_schedule_dir[MINI_GNB_C_MAX_PATH];
  char scripted_pdcch_dir[MINI_GNB_C_MAX_PATH];
  char ul_data_hex[MINI_GNB_C_MAX_PAYLOAD * 2 + 1];
  char contention_id_hex[32];
  uint8_t establishment_cause;
  uint8_t ue_identity_type;
  char ue_identity_hex[32];
  bool include_crnti_ce;
} mini_gnb_c_sim_config_t;

typedef struct {
  mini_gnb_c_cell_config_t cell;
  mini_gnb_c_prach_config_t prach;
  mini_gnb_c_rf_config_t rf;
  mini_gnb_c_core_config_t core;
  mini_gnb_c_broadcast_config_t broadcast;
  mini_gnb_c_sim_config_t sim;
} mini_gnb_c_config_t;

typedef struct {
  int64_t hw_time_ns;
  bool tx_underflow;
  bool rx_overflow;
  double cfo_est_hz;
} mini_gnb_c_radio_status_t;

typedef struct {
  int64_t hw_time_ns;
  uint32_t sfn;
  uint16_t slot;
  uint32_t nof_samples;
  mini_gnb_c_ul_burst_type_t ul_type;
  uint16_t rnti;
  uint8_t preamble_id;
  int ta_est;
  double peak_metric;
  double snr_db;
  double evm;
  uint16_t tbsize;
  uint8_t harq_id;
  bool ndi;
  bool is_new_data;
  bool crc_ok_override_valid;
  bool crc_ok_override;
  mini_gnb_c_buffer_t mac_pdu;
  mini_gnb_c_complexf_t samples[MINI_GNB_C_MAX_IQ_SAMPLES];
  mini_gnb_c_radio_status_t status;
} mini_gnb_c_radio_burst_t;

typedef struct {
  uint32_t sfn;
  uint16_t slot;
  int abs_slot;
  int64_t slot_start_ns;
  bool has_ssb;
  bool has_sib1;
  bool has_prach_occasion;
  bool is_ul_slot;
  bool is_dl_slot;
} mini_gnb_c_slot_indication_t;

typedef struct {
  uint32_t sfn;
  uint16_t slot;
  int abs_slot;
  uint8_t preamble_id;
  int ta_est;
  double peak_metric;
  double snr_est;
  bool valid;
} mini_gnb_c_prach_indication_t;

typedef struct {
  uint16_t tc_rnti;
  int abs_slot;
  uint16_t msg3_prb_start;
  uint16_t msg3_prb_len;
  uint8_t msg3_mcs;
  uint8_t k2;
  uint8_t ta_cmd;
} mini_gnb_c_ul_grant_for_msg3_t;

typedef struct {
  bool valid;
  mini_gnb_c_dci_format_t format;
  uint16_t rnti;
  uint16_t coreset_prb_start;
  uint16_t coreset_prb_len;
  uint16_t scheduled_prb_start;
  uint16_t scheduled_prb_len;
  uint8_t mcs;
  int k2;
  int time_indicator;
  int dl_data_to_ul_ack;
  int scheduled_abs_slot;
  mini_gnb_c_dl_object_type_t scheduled_dl_type;
  mini_gnb_c_ul_burst_type_t scheduled_ul_type;
  mini_gnb_c_ul_data_purpose_t scheduled_ul_purpose;
  uint8_t harq_id;
  bool ndi;
  bool is_new_data;
} mini_gnb_c_pdcch_dci_t;

typedef struct {
  mini_gnb_c_dl_object_type_t type;
  int abs_slot;
  uint16_t rnti;
  uint16_t prb_start;
  uint16_t prb_len;
  uint8_t mcs;
  uint8_t rv;
  uint8_t harq_id;
  mini_gnb_c_pdcch_dci_t pdcch;
  mini_gnb_c_buffer_t payload;
} mini_gnb_c_dl_grant_t;

typedef struct {
  uint32_t sfn;
  uint16_t slot;
  int abs_slot;
  uint16_t sym_start;
  uint16_t nof_sym;
  uint16_t prb_start;
  uint16_t prb_len;
  mini_gnb_c_dl_object_type_t type;
  uint16_t rnti;
  size_t payload_len;
  mini_gnb_c_pdcch_dci_t pdcch;
  mini_gnb_c_buffer_t payload;
  uint16_t fft_size;
  uint16_t cp_length;
  size_t sample_count;
  mini_gnb_c_complexf_t samples[MINI_GNB_C_MAX_IQ_SAMPLES];
} mini_gnb_c_tx_grid_patch_t;

typedef struct {
  uint32_t sfn;
  uint16_t slot;
  int abs_slot;
  uint16_t rnti;
  bool crc_ok;
  double snr_db;
  double evm;
  mini_gnb_c_buffer_t mac_pdu;
} mini_gnb_c_msg3_decode_indication_t;

typedef struct {
  bool has_crnti_ce;
  uint16_t crnti_ce;
  bool has_ul_ccch;
  mini_gnb_c_buffer_t ul_ccch_sdu;
  bool parse_ok;
  int lcid_sequence[MINI_GNB_C_MAX_LCID_SEQUENCE];
  size_t lcid_count;
} mini_gnb_c_mac_ul_parse_result_t;

typedef struct {
  uint8_t establishment_cause;
  uint8_t ue_identity_type;
  mini_gnb_c_buffer_t ue_identity_raw;
  uint8_t contention_id48[6];
  bool valid;
} mini_gnb_c_rrc_setup_request_info_t;

typedef struct {
  mini_gnb_c_buffer_t asn1_buf;
} mini_gnb_c_rrc_setup_blob_t;

typedef struct {
  uint16_t tc_rnti;
  uint16_t c_rnti;
  uint8_t contention_id48[6];
  mini_gnb_c_core_session_t core_session;
  int create_abs_slot;
  bool rrc_setup_sent;
  int sent_abs_slot;
  bool traffic_plan_scheduled;
  bool dl_data_sent;
  int dl_data_abs_slot;
  bool pucch_sr_detected;
  int pucch_sr_abs_slot;
  bool ul_bsr_received;
  int ul_bsr_abs_slot;
  int ul_bsr_buffer_size_bytes;
  int small_ul_grant_abs_slot;
  int large_ul_grant_abs_slot;
  bool ul_data_received;
  int ul_data_abs_slot;
} mini_gnb_c_ue_context_t;

typedef struct {
  int detect_abs_slot;
  uint8_t preamble_id;
  uint16_t tc_rnti;
  int ta_est;
  int rar_abs_slot;
  int msg3_expect_abs_slot;
  int msg4_abs_slot;
  uint8_t contention_id48[6];
  bool has_contention_id;
  mini_gnb_c_ra_state_t state;
  int msg3_harq_round;
  bool ue_ctx_promoted;
  char last_failure[32];
} mini_gnb_c_ra_context_t;

typedef struct {
  char module[32];
  char message[96];
  int abs_slot;
  char details[MINI_GNB_C_MAX_EVENT_TEXT];
} mini_gnb_c_trace_event_t;

typedef struct {
  int abs_slot;
  int mac_latency_us;
  int dl_build_latency_us;
  int ul_decode_latency_us;
} mini_gnb_c_slot_perf_t;

typedef struct {
  uint16_t tc_rnti;
  int detect_abs_slot;
  int rar_abs_slot;
  uint8_t preamble_id;
  uint8_t ta_cmd;
  mini_gnb_c_ul_grant_for_msg3_t ul_grant;
} mini_gnb_c_ra_schedule_request_t;

typedef struct {
  uint16_t tc_rnti;
  int msg4_abs_slot;
  uint8_t contention_id48[6];
  mini_gnb_c_rrc_setup_blob_t rrc_setup;
} mini_gnb_c_msg4_schedule_request_t;

typedef struct {
  uint16_t c_rnti;
  int pdcch_abs_slot;
  int abs_slot;
  uint16_t prb_start;
  uint16_t prb_len;
  uint8_t mcs;
  mini_gnb_c_dci_format_t dci_format;
  int time_indicator;
  int dl_data_to_ul_ack;
  uint8_t harq_id;
  bool ndi;
  bool is_new_data;
  mini_gnb_c_buffer_t payload;
} mini_gnb_c_dl_data_schedule_request_t;

typedef struct {
  uint16_t c_rnti;
  int pdcch_abs_slot;
  int abs_slot;
  uint16_t prb_start;
  uint16_t prb_len;
  uint8_t mcs;
  uint8_t k2;
  mini_gnb_c_ul_data_purpose_t purpose;
  uint8_t harq_id;
  bool ndi;
  bool is_new_data;
} mini_gnb_c_ul_data_schedule_request_t;

typedef struct {
  uint16_t c_rnti;
  int pdcch_abs_slot;
  int abs_slot;
  uint16_t prb_start;
  uint16_t prb_len;
  uint8_t mcs;
  uint8_t k2;
  mini_gnb_c_ul_data_purpose_t purpose;
  uint8_t harq_id;
  bool ndi;
  bool is_new_data;
  mini_gnb_c_pdcch_dci_t pdcch;
} mini_gnb_c_ul_data_grant_t;

typedef struct {
  uint64_t prach_detect_ok;
  uint64_t prach_false_alarm;
  uint64_t rar_sent;
  uint64_t msg3_crc_ok;
  uint64_t msg3_crc_fail;
  uint64_t rrcsetup_sent;
  uint64_t pucch_sr_detect_ok;
  uint64_t ul_bsr_rx_ok;
  uint64_t ul_bsr_crc_fail;
  uint64_t dl_data_sent;
  uint64_t ul_data_rx_ok;
  uint64_t ul_data_crc_fail;
  uint64_t ra_timeout;
} mini_gnb_c_counters_t;

typedef struct {
  mini_gnb_c_counters_t counters;
  bool has_ra_context;
  mini_gnb_c_ra_context_t ra_context;
  mini_gnb_c_ue_context_t ue_contexts[MINI_GNB_C_MAX_UES];
  size_t ue_count;
  char trace_path[MINI_GNB_C_MAX_PATH];
  char metrics_path[MINI_GNB_C_MAX_PATH];
  char summary_path[MINI_GNB_C_MAX_PATH];
} mini_gnb_c_run_summary_t;

const char* mini_gnb_c_dl_object_type_to_string(mini_gnb_c_dl_object_type_t type);
const char* mini_gnb_c_ul_burst_type_to_string(mini_gnb_c_ul_burst_type_t type);
const char* mini_gnb_c_dci_format_to_string(mini_gnb_c_dci_format_t format);
const char* mini_gnb_c_ra_state_to_string(mini_gnb_c_ra_state_t state);
uint16_t mini_gnb_c_lookup_tbsize(uint16_t prb_len, uint8_t mcs);

void mini_gnb_c_buffer_reset(mini_gnb_c_buffer_t* buffer);
int mini_gnb_c_buffer_set_bytes(mini_gnb_c_buffer_t* buffer, const uint8_t* data, size_t len);
int mini_gnb_c_buffer_set_text(mini_gnb_c_buffer_t* buffer, const char* text);

#endif
