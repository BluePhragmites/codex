#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/link/shared_slot_link.h"

void test_shared_slot_link_round_trip(void) {
  char output_dir[MINI_GNB_C_MAX_PATH];
  char link_path[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_shared_slot_link_t gnb_link;
  mini_gnb_c_shared_slot_link_t ue_link;
  mini_gnb_c_shared_slot_dl_summary_t dl_summary;
  mini_gnb_c_shared_slot_dl_summary_t read_summary;
  mini_gnb_c_shared_slot_ul_event_t ul_event;
  mini_gnb_c_shared_slot_ul_event_t consumed_ul;

  mini_gnb_c_make_output_dir("test_shared_slot_link", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "shared_slot_link.bin", link_path, sizeof(link_path)) == 0,
                     "expected shared slot link path");

  mini_gnb_c_require(mini_gnb_c_shared_slot_link_open(&gnb_link, link_path, true) == 0,
                     "expected gNB shared slot link open");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_open(&ue_link, link_path, false) == 0,
                     "expected UE shared slot link open");

  memset(&dl_summary, 0, sizeof(dl_summary));
  dl_summary.abs_slot = 3;
  dl_summary.flags = MINI_GNB_C_SHARED_SLOT_FLAG_SIB1 | MINI_GNB_C_SHARED_SLOT_FLAG_RAR |
                     MINI_GNB_C_SHARED_SLOT_FLAG_PDCCH;
  dl_summary.dl_rnti = 0x4601u;
  dl_summary.has_pdcch = true;
  dl_summary.pdcch.rnti = 0x4601u;
  dl_summary.pdcch.scheduled_abs_slot = 6;
  dl_summary.pdcch.scheduled_ul_type = MINI_GNB_C_UL_BURST_DATA;
  dl_summary.pdcch.scheduled_ul_purpose = MINI_GNB_C_UL_DATA_PURPOSE_BSR;
  dl_summary.pdcch.harq_id = 1u;
  dl_summary.pdcch.ndi = true;
  dl_summary.pdcch.is_new_data = true;
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&dl_summary.sib1_payload,
                                                "SIB1|prach_period_slots=10|prach_offset_slot=2") == 0,
                     "expected SIB1 payload text");
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&dl_summary.rar_payload, "RAR") == 0,
                     "expected RAR payload text");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_publish_gnb_slot(&gnb_link, &dl_summary) == 0,
                     "expected gNB slot publish");

  memset(&read_summary, 0, sizeof(read_summary));
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_wait_for_gnb_slot(&ue_link, 3, 10u, &read_summary) == 0,
                     "expected UE to observe published slot");
  mini_gnb_c_require(read_summary.abs_slot == 3, "expected published abs_slot");
  mini_gnb_c_require((read_summary.flags & MINI_GNB_C_SHARED_SLOT_FLAG_RAR) != 0u, "expected RAR flag");
  mini_gnb_c_require(read_summary.pdcch.scheduled_abs_slot == 6, "expected scheduled UL abs slot");
  mini_gnb_c_require(read_summary.sib1_payload.len > 0u, "expected shared SIB1 payload");
  mini_gnb_c_require(read_summary.rar_payload.len == strlen("RAR"), "expected shared RAR payload");

  memset(&ul_event, 0, sizeof(ul_event));
  ul_event.valid = true;
  ul_event.abs_slot = 6;
  ul_event.type = MINI_GNB_C_UL_BURST_DATA;
  ul_event.rnti = 0x4601u;
  ul_event.purpose = MINI_GNB_C_UL_DATA_PURPOSE_BSR;
  ul_event.harq_id = 1u;
  ul_event.ndi = true;
  ul_event.is_new_data = true;
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&ul_event.payload, "BSR|bytes=384") == 0,
                     "expected UL payload text");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_schedule_ul(&ue_link, &ul_event) == 0,
                     "expected UL event scheduling");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_mark_ue_progress(&ue_link, 3) == 0,
                     "expected UE progress update");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_wait_for_ue_progress(&gnb_link, 3, 10u) == 0,
                     "expected gNB to observe UE progress");

  memset(&consumed_ul, 0, sizeof(consumed_ul));
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_consume_ul(&gnb_link, 6, &consumed_ul) == 1,
                     "expected gNB UL event consumption");
  mini_gnb_c_require(consumed_ul.type == MINI_GNB_C_UL_BURST_DATA, "expected consumed UL type");
  mini_gnb_c_require(consumed_ul.payload.len == strlen("BSR|bytes=384"), "expected consumed payload len");

  mini_gnb_c_require(mini_gnb_c_shared_slot_link_mark_gnb_done(&gnb_link) == 0, "expected gNB done flag");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_gnb_done(&ue_link), "expected UE done visibility");

  mini_gnb_c_shared_slot_link_close(&ue_link);
  mini_gnb_c_shared_slot_link_close(&gnb_link);
}

void test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries(void) {
  char output_dir[MINI_GNB_C_MAX_PATH];
  char link_path[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_shared_slot_link_t gnb_link;
  mini_gnb_c_shared_slot_link_t ue_link;
  mini_gnb_c_shared_slot_dl_summary_t dl_summary;
  mini_gnb_c_shared_slot_dl_summary_t read_summary;
  mini_gnb_c_shared_slot_ul_event_t ul_event;
  mini_gnb_c_shared_slot_ul_event_t consumed_ul;

  mini_gnb_c_make_output_dir("test_shared_slot_link_boundaries", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "shared_slot_link.bin", link_path, sizeof(link_path)) == 0,
                     "expected shared slot link boundary path");

  mini_gnb_c_require(mini_gnb_c_shared_slot_link_open(&gnb_link, link_path, true) == 0,
                     "expected gNB shared slot link boundary open");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_open(&ue_link, link_path, false) == 0,
                     "expected UE shared slot link boundary open");

  memset(&read_summary, 0, sizeof(read_summary));
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_wait_for_gnb_slot(&ue_link, 0, 5u, &read_summary) == 1,
                     "expected slot 0 to remain unreadable before first gNB publish");

  memset(&dl_summary, 0, sizeof(dl_summary));
  dl_summary.abs_slot = 0;
  dl_summary.pdcch.scheduled_abs_slot = -1;
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_publish_gnb_slot(&gnb_link, &dl_summary) == 0,
                     "expected slot 0 publish");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_wait_for_gnb_slot(&ue_link, 0, 10u, &read_summary) == 0,
                     "expected UE to observe slot 0 publish");
  mini_gnb_c_require(read_summary.abs_slot == 0, "expected slot 0 summary");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_mark_ue_progress(&ue_link, 0) == 0,
                     "expected UE slot 0 progress");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_wait_for_ue_progress(&gnb_link, 0, 10u) == 0,
                     "expected gNB slot 0 acknowledgement");

  memset(&ul_event, 0, sizeof(ul_event));
  ul_event.valid = true;
  ul_event.abs_slot = 4;
  ul_event.type = MINI_GNB_C_UL_BURST_DATA;
  ul_event.rnti = 0x4601u;
  ul_event.purpose = MINI_GNB_C_UL_DATA_PURPOSE_PAYLOAD;
  mini_gnb_c_require(mini_gnb_c_buffer_set_text(&ul_event.payload, "UL_DATA") == 0,
                     "expected UL boundary payload");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_schedule_ul(&ue_link, &ul_event) == 0,
                     "expected final UL scheduling without another gNB publish");

  memset(&consumed_ul, 0, sizeof(consumed_ul));
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_consume_ul(&gnb_link, 3, &consumed_ul) == 0,
                     "expected no early UL consumption");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_mark_ue_done(&ue_link) == 0, "expected UE done flag");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_wait_for_ue_progress(&gnb_link, 4, 10u) == 1,
                     "expected gNB wait to stop once UE signals shutdown");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_consume_ul(&gnb_link, 4, &consumed_ul) == 1,
                     "expected gNB to read final UL after UE shutdown");
  mini_gnb_c_require(consumed_ul.abs_slot == 4, "expected final UL slot");
  mini_gnb_c_require(consumed_ul.type == MINI_GNB_C_UL_BURST_DATA, "expected final UL type");

  mini_gnb_c_require(mini_gnb_c_shared_slot_link_mark_gnb_done(&gnb_link) == 0, "expected final gNB done flag");
  mini_gnb_c_require(mini_gnb_c_shared_slot_link_gnb_done(&ue_link), "expected UE to observe final gNB done flag");

  mini_gnb_c_shared_slot_link_close(&ue_link);
  mini_gnb_c_shared_slot_link_close(&gnb_link);
}
