#include <string.h>

#include "test_helpers.h"

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
                                                                   "test-first",
                                                                   5) == 0,
                     "expected first queue insert");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &second_payload,
                                                                   14,
                                                                   "test-second",
                                                                   6) == 0,
                     "expected second queue insert");
  mini_gnb_c_require(runtime.ul_payload_queue_count == 2u, "expected two queued payloads");
  mini_gnb_c_require(runtime.ul_payload_queue_bytes == 24, "expected accounted queue bytes");
  mini_gnb_c_require(runtime.sr_pending, "expected SR demand after queue inserts");
  mini_gnb_c_require(runtime.bsr_dirty, "expected BSR dirty after queue inserts");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_current_bsr_bytes(&runtime) == 24,
                     "expected BSR bytes to match queue accounting");

  runtime.last_reported_buffer_size_bytes = 24;
  runtime.bsr_dirty = false;
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_pop_ul_payload(&runtime, &popped_payload) == 0,
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
                                                                   "first",
                                                                   1) == 0,
                     "expected first queue insert");
  mini_gnb_c_require(mini_gnb_c_mini_ue_runtime_enqueue_ul_payload(&runtime,
                                                                   &second_payload,
                                                                   20,
                                                                   "second",
                                                                   2) == 0,
                     "expected second queue insert");

  mini_gnb_c_mini_ue_build_harq_ul_payload(&runtime,
                                           0u,
                                           false,
                                           true,
                                           MINI_GNB_C_UL_DATA_PURPOSE_BSR,
                                           &bsr_payload,
                                           &consumed_payload);
  mini_gnb_c_require(!consumed_payload, "expected BSR generation to keep queued payloads");
  mini_gnb_c_require(bsr_payload.len == strlen("BSR|bytes=32"), "expected BSR payload length");
  mini_gnb_c_require(memcmp(bsr_payload.bytes, "BSR|bytes=32", bsr_payload.len) == 0,
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
                                           &payload,
                                           &consumed_payload);
  mini_gnb_c_require(!consumed_payload, "expected no queued payload consumption");
  mini_gnb_c_require(payload.len == 0u, "expected empty payload when no queued UL data exists");
  mini_gnb_c_require(!runtime.ul_harq[0].valid, "expected no HARQ cache entry for skipped new payload");
}
