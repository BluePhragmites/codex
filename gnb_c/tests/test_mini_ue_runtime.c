#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/json_utils.h"

#define mini_gnb_c_run_shared_ue_runtime mini_gnb_c_run_shared_ue_runtime_test_copy
#include "../src/ue/mini_ue_runtime.c"
#undef mini_gnb_c_run_shared_ue_runtime

static void mini_gnb_c_init_test_runtime(mini_gnb_c_mini_ue_runtime_t* runtime) {
  mini_gnb_c_require(runtime != NULL, "expected mini UE runtime state");
  memset(runtime, 0, sizeof(*runtime));
  runtime->earliest_next_prach_abs_slot = -1;
  runtime->pending_sr_abs_slot = -1;
  runtime->pending_bsr_grant_abs_slot = -1;
  runtime->pending_payload_grant_abs_slot = -1;
  runtime->next_rlc_sdu_id = 1u;
  mini_gnb_c_rlc_lite_receiver_init(&runtime->dl_ip_reassembly);
}

void test_mini_ue_runtime_uplink_queue_tracks_bytes_and_bsr_dirty(void) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  mini_gnb_c_buffer_t first_payload;
  mini_gnb_c_buffer_t second_payload;
  mini_gnb_c_buffer_t popped_payload;

  mini_gnb_c_init_test_runtime(&runtime);
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&first_payload, "PING_A") == 0, "expected first payload");
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&second_payload, "PING_BB") == 0, "expected second payload");

  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &first_payload,
                                                                   10,
                                                                   MINI_GNB_C_PAYLOAD_KIND_IPV4,
                                                                   "test-first",
                                                                   5) == 0,
                     "expected first queue insert");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &second_payload,
                                                                   14,
                                                                   MINI_GNB_C_PAYLOAD_KIND_NAS,
                                                                   "test-second",
                                                                   6) == 0,
                     "expected second queue insert");
  mini_gnb_c_require(runtime.ul_payload_queue_count == 2u, "expected two queued payloads");
  mini_gnb_c_require(runtime.ul_payload_queue_bytes == 20, "expected accounted queue bytes");
  mini_gnb_c_require(runtime.sr_pending, "expected SR demand after queue inserts");
  mini_gnb_c_require(runtime.bsr_dirty, "expected BSR dirty after queue inserts");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_current_bsr_bytes(&runtime) == 20,
                     "expected BSR bytes to match queue accounting");

  runtime.last_reported_buffer_size_bytes = 20;
  runtime.bsr_dirty = false;
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_pop_ul_payload(&runtime, &popped_payload, NULL) == 0,
                     "expected queued payload pop");
  mini_gnb_c_require(popped_payload.len == first_payload.len, "expected first payload length");
  mini_gnb_c_require(memcmp(popped_payload.bytes, first_payload.bytes, first_payload.len) == 0,
                     "expected FIFO pop order");
  mini_gnb_c_require(runtime.ul_payload_queue_count == 1u, "expected one queued payload after pop");
  mini_gnb_c_require(runtime.ul_payload_queue_bytes == 14, "expected queue bytes after pop");
  mini_gnb_c_require(runtime.bsr_dirty, "expected BSR dirty when queue bytes changed after pop");
}

void test_mini_ue_runtime_update_uplink_state_rearms_sr_after_grant_consumption(void) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  mini_gnb_c_buffer_t payload;

  mini_gnb_c_init_test_runtime(&runtime);
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&payload, "PING") == 0, "expected payload text");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &payload,
                                                                   (int)payload.len,
                                                                   MINI_GNB_C_PAYLOAD_KIND_IPV4,
                                                                   "test",
                                                                   4) == 0,
                     "expected queued payload");

  runtime.sr_pending = false;
  runtime.bsr_dirty = false;
  runtime.last_reported_buffer_size_bytes = (int)payload.len;
  runtime.pending_payload_grant_abs_slot = 12;
  mini_gnb_c_mini_ue_runtime_update_uplink_state(&runtime, 10);
  mini_gnb_c_require(!runtime.sr_pending, "expected no repeated SR while a future payload grant exists");
  mini_gnb_c_require(runtime.pending_payload_grant_abs_slot == 12, "expected future payload grant to remain armed");

  mini_gnb_c_mini_ue_runtime_update_uplink_state(&runtime, 12);
  mini_gnb_c_require(runtime.pending_payload_grant_abs_slot == -1, "expected consumed payload grant to clear");
  mini_gnb_c_require(runtime.sr_pending, "expected repeated SR once no future UL grant remains");

  runtime.sr_pending = false;
  runtime.pending_bsr_grant_abs_slot = 15;
  mini_gnb_c_mini_ue_runtime_update_uplink_state(&runtime, 14);
  mini_gnb_c_require(!runtime.sr_pending, "expected no repeated SR while a future BSR grant exists");
  mini_gnb_c_mini_ue_runtime_update_uplink_state(&runtime, 15);
  mini_gnb_c_require(runtime.pending_bsr_grant_abs_slot == -1, "expected consumed BSR grant to clear");
  mini_gnb_c_require(runtime.sr_pending, "expected repeated SR after the future BSR grant expires");
}

void test_mini_ue_runtime_builds_bsr_from_current_queue_bytes(void) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  mini_gnb_c_buffer_t first_payload;
  mini_gnb_c_buffer_t second_payload;
  mini_gnb_c_buffer_t bsr_payload;
  bool consumed_payload = false;

  mini_gnb_c_init_test_runtime(&runtime);
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&first_payload, "A") == 0, "expected first payload text");
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&second_payload, "BC") == 0, "expected second payload text");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &first_payload,
                                                                   12,
                                                                   MINI_GNB_C_PAYLOAD_KIND_IPV4,
                                                                   "first",
                                                                   1) == 0,
                     "expected first queue insert");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &second_payload,
                                                                   20,
                                                                   MINI_GNB_C_PAYLOAD_KIND_NAS,
                                                                   "second",
                                                                   2) == 0,
                     "expected second queue insert");

  mini_gnb_c_mini_ue_build_harq_ul_payload(&runtime,
                                           0u,
                                           false,
                                           true,
                                           MINI_GNB_C_UL_DATA_PURPOSE_BSR,
                                           0u,
                                           &bsr_payload,
                                           NULL,
                                           &consumed_payload);
  mini_gnb_c_require(!consumed_payload, "expected BSR generation to keep queued payloads");
  mini_gnb_c_require(bsr_payload.len == strlen("BSR|bytes=21"), "expected BSR payload length");
  mini_gnb_c_require(memcmp(bsr_payload.bytes, "BSR|bytes=21", bsr_payload.len) == 0,
                     "expected BSR payload to match queued byte accounting");
  mini_gnb_c_require(runtime.ul_harq[0].valid, "expected UL HARQ cache after BSR generation");
  mini_gnb_c_require(runtime.ul_harq[0].last_purpose == MINI_GNB_C_UL_DATA_PURPOSE_BSR,
                     "expected cached UL HARQ purpose");
}

void test_mini_ue_runtime_skips_new_payload_grant_without_queue(void) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  mini_gnb_c_buffer_t payload;
  bool consumed_payload = true;

  mini_gnb_c_init_test_runtime(&runtime);
  runtime.sim.ul_data_present = false;

  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_current_bsr_bytes(&runtime) == 0,
                     "expected empty queue to report zero BSR bytes");
  mini_gnb_c_mini_ue_build_harq_ul_payload(&runtime,
                                           0u,
                                           false,
                                           true,
                                           MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD,
                                           96u,
                                           &payload,
                                           NULL,
                                           &consumed_payload);
  mini_gnb_c_require(!consumed_payload, "expected no queued payload consumption");
  mini_gnb_c_require(payload.len == 0u, "expected empty payload when no queued UL data exists");
  mini_gnb_c_require(!runtime.ul_harq[0].valid, "expected no HARQ cache entry for skipped new payload");
}

void test_mini_ue_runtime_preserves_payload_kind_for_new_and_retx_grants(void) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  mini_gnb_c_buffer_t payload;
  mini_gnb_c_buffer_t scheduled_payload;
  mini_gnb_c_payload_kind_t payload_kind = MINI_GNB_C_PAYLOAD_KIND_GENERIC;
  bool consumed_payload = false;

  mini_gnb_c_init_test_runtime(&runtime);
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&payload, "NAS") == 0, "expected NAS payload text");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &payload,
                                                                   (int)payload.len,
                                                                   MINI_GNB_C_PAYLOAD_KIND_NAS,
                                                                   "nas",
                                                                   3) == 0,
                     "expected NAS queue insert");

  mini_gnb_c_mini_ue_build_harq_ul_payload(&runtime,
                                           1u,
                                           true,
                                           true,
                                           MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD,
                                           96u,
                                           &scheduled_payload,
                                           &payload_kind,
                                           &consumed_payload);
  mini_gnb_c_require(consumed_payload, "expected new payload grant to consume queued NAS");
  mini_gnb_c_require(payload_kind == MINI_GNB_C_PAYLOAD_KIND_NAS, "expected NAS payload kind on first send");

  payload_kind = MINI_GNB_C_PAYLOAD_KIND_GENERIC;
  consumed_payload = false;
  mini_gnb_c_mini_ue_build_harq_ul_payload(&runtime,
                                           1u,
                                           true,
                                           false,
                                           MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD,
                                           96u,
                                           &scheduled_payload,
                                           &payload_kind,
                                           &consumed_payload);
  mini_gnb_c_require(!consumed_payload, "expected HARQ retx to reuse cached payload");
  mini_gnb_c_require(payload_kind == MINI_GNB_C_PAYLOAD_KIND_NAS, "expected NAS payload kind on HARQ retx");
}

void test_mini_ue_runtime_segments_ipv4_payload_across_multiple_grants(void) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  mini_gnb_c_rlc_lite_receiver_t receiver;
  mini_gnb_c_buffer_t ipv4_payload;
  mini_gnb_c_buffer_t segment;
  mini_gnb_c_buffer_t reassembled_payload;
  mini_gnb_c_payload_kind_t payload_kind = MINI_GNB_C_PAYLOAD_KIND_GENERIC;
  bool consumed_payload = false;
  int grant_index = 0;

  mini_gnb_c_init_test_runtime(&runtime);
  mini_gnb_c_rlc_lite_receiver_init(&receiver);
  memset(&ipv4_payload, 0, sizeof(ipv4_payload));
  ipv4_payload.len = 180u;
  for (grant_index = 0; grant_index < (int)ipv4_payload.len; ++grant_index) {
    ipv4_payload.bytes[grant_index] = (uint8_t)(grant_index & 0xff);
  }
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &ipv4_payload,
                                                                   999,
                                                                   MINI_GNB_C_PAYLOAD_KIND_IPV4,
                                                                   "segmented-ipv4",
                                                                   9) == 0,
                     "expected IPv4 queue insert");
  mini_gnb_c_require(runtime.ul_payload_queue_bytes == (int)ipv4_payload.len,
                     "expected IPv4 queue bytes to follow raw payload length");

  for (grant_index = 0; grant_index < 3; ++grant_index) {
    size_t consumed_bytes = 0u;

    payload_kind = MINI_GNB_C_PAYLOAD_KIND_GENERIC;
    consumed_payload = false;
    mini_gnb_c_mini_ue_build_harq_ul_payload(&runtime,
                                             (uint8_t)grant_index,
                                             true,
                                             true,
                                             MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD,
                                             80u,
                                             &segment,
                                             &payload_kind,
                                             &consumed_payload);
    mini_gnb_c_require(payload_kind == MINI_GNB_C_PAYLOAD_KIND_IPV4, "expected segmented IPv4 payload kind");
    mini_gnb_c_require(mini_gnb_c_rlc_lite_is_segment(segment.bytes, segment.len), "expected RLC-lite segment");
    mini_gnb_c_require(mini_gnb_c_rlc_lite_receiver_consume(&receiver,
                                                            segment.bytes,
                                                            segment.len,
                                                            &reassembled_payload,
                                                            &consumed_bytes) ==
                           (grant_index == 2 ? 1 : 0),
                       "expected segment reassembly progress");
    mini_gnb_c_require(consumed_bytes > 0u, "expected segment to consume IPv4 bytes");
    if (grant_index < 2) {
      mini_gnb_c_require(!consumed_payload, "expected intermediate segment to retain queued payload entry");
      mini_gnb_c_require(runtime.ul_payload_queue_count == 1u, "expected queued IPv4 entry until final segment");
    }
  }

  mini_gnb_c_require(consumed_payload, "expected final segment to consume queued payload entry");
  mini_gnb_c_require(runtime.ul_payload_queue_count == 0u, "expected IPv4 queue to drain after final segment");
  mini_gnb_c_require(runtime.ul_payload_queue_bytes == 0, "expected IPv4 queue byte count to drain");
  mini_gnb_c_require(reassembled_payload.len == ipv4_payload.len, "expected reassembled IPv4 payload length");
  mini_gnb_c_require(memcmp(reassembled_payload.bytes, ipv4_payload.bytes, ipv4_payload.len) == 0,
                     "expected reassembled IPv4 payload bytes");
}

void test_mini_ue_runtime_exports_ul_event_into_rx_dir(void) {
  mini_gnb_c_mini_ue_runtime_t runtime;
  mini_gnb_c_shared_slot_ul_event_t event;
  char output_dir[MINI_GNB_C_MAX_PATH];
  char event_path[MINI_GNB_C_MAX_PATH];
  char* event_text = NULL;

  mini_gnb_c_init_test_runtime(&runtime);
  mini_gnb_c_make_output_dir("test_mini_ue_runtime_rx_export", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "rx", runtime.rx_dir, sizeof(runtime.rx_dir)) == 0,
                     "expected rx export dir path");
  mini_gnb_c_require(mini_gnb_c_ensure_directory_recursive(runtime.rx_dir) == 0, "expected rx export dir");
  runtime.rx_export_enabled = true;

  memset(&event, 0, sizeof(event));
  event.valid = true;
  event.abs_slot = 12;
  event.type = MINI_GNB_C_UL_BURST_DATA;
  event.rnti = 0x4601u;
  event.purpose = MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD;
  event.payload_kind = MINI_GNB_C_PAYLOAD_KIND_NAS;
  event.harq_id = 1u;
  event.ndi = true;
  event.is_new_data = true;
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&event.payload, "NAS_PAYLOAD") == 0, "expected event payload");

  mini_gnb_c_mini_ue_runtime_export_ul_event(&runtime, &event);

  mini_gnb_c_require(snprintf(event_path, sizeof(event_path), "%s/slot_12_UL_OBJ_DATA.txt", runtime.rx_dir) <
                         (int)sizeof(event_path),
                     "expected exported UL event path");
  mini_gnb_c_require(mini_gnb_c_path_exists(event_path), "expected exported UL event file");
  event_text = mini_gnb_c_read_text_file(event_path);
  mini_gnb_c_require(event_text != NULL, "expected exported UL event text");
  mini_gnb_c_require(strstr(event_text, "channel=PUSCH") != NULL, "expected PUSCH export channel");
  mini_gnb_c_require(strstr(event_text, "payload_kind=NAS") != NULL, "expected NAS payload kind export");
  mini_gnb_c_require(strstr(event_text, "type=UL_OBJ_DATA") != NULL, "expected UL data export type");
  free(event_text);
}
