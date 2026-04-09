#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/hex.h"
#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/link/json_link.h"
#include "mini_gnb_c/nas/nas_5gs_min.h"

static void mini_gnb_c_test_build_auth_request(uint8_t* nas_pdu, size_t* nas_pdu_length) {
  static const uint8_t k_rand[16] = {
      0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x15u, 0x16u, 0x17u,
      0x18u, 0x19u, 0x1au, 0x1bu, 0x1cu, 0x1du, 0x1eu, 0x1fu,
  };
  static const uint8_t k_autn[16] = {
      0x20u, 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x27u,
      0x28u, 0x29u, 0x2au, 0x2bu, 0x2cu, 0x2du, 0x2eu, 0x2fu,
  };

  mini_gnb_c_require(nas_pdu != NULL && nas_pdu_length != NULL, "expected auth request output");
  nas_pdu[0] = 0x7eu;
  nas_pdu[1] = 0x00u;
  nas_pdu[2] = 0x56u;
  nas_pdu[3] = 0x01u;
  nas_pdu[4] = 0x02u;
  nas_pdu[5] = 0x00u;
  nas_pdu[6] = 0x00u;
  nas_pdu[7] = 0x21u;
  memcpy(nas_pdu + 8u, k_rand, sizeof(k_rand));
  nas_pdu[24] = 0x20u;
  nas_pdu[25] = 0x10u;
  memcpy(nas_pdu + 26u, k_autn, sizeof(k_autn));
  *nas_pdu_length = 42u;
}

static void mini_gnb_c_require_ul_nas_message_type(const char* exchange_dir,
                                                   uint32_t sequence,
                                                   uint8_t expected_message_type,
                                                   int expected_abs_slot) {
  char event_path[MINI_GNB_C_MAX_PATH];
  char* event_text = NULL;
  char nas_hex[MINI_GNB_C_MAX_PAYLOAD * 2u + 1u];
  uint8_t nas_pdu[MINI_GNB_C_MAX_PAYLOAD];
  size_t nas_pdu_length = 0u;
  int abs_slot = 0;

  mini_gnb_c_require(mini_gnb_c_json_link_build_event_path(exchange_dir,
                                                           "ue_to_gnb_nas",
                                                           "ue",
                                                           sequence,
                                                           "UL_NAS",
                                                           event_path,
                                                           sizeof(event_path)) == 0,
                     "expected UL_NAS event path");
  event_text = mini_gnb_c_read_text_file(event_path);
  mini_gnb_c_require(event_text != NULL, "expected UL_NAS event text");
  mini_gnb_c_require(mini_gnb_c_extract_json_int(event_text, "abs_slot", &abs_slot) == 0,
                     "expected UL_NAS abs_slot");
  if (expected_abs_slot >= 0) {
    mini_gnb_c_require(abs_slot == expected_abs_slot, "expected UL_NAS abs_slot");
  }
  mini_gnb_c_require(mini_gnb_c_extract_json_string(event_text, "nas_hex", nas_hex, sizeof(nas_hex)) == 0,
                     "expected nas_hex field");
  mini_gnb_c_require(mini_gnb_c_hex_to_bytes(nas_hex, nas_pdu, sizeof(nas_pdu), &nas_pdu_length) == 0,
                     "expected UL_NAS decode");
  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_extract_message_type(nas_pdu, nas_pdu_length) == expected_message_type,
                     "expected UL_NAS message type");
  free(event_text);
}

void test_nas_5gs_min_builds_followup_uplinks(void) {
  char output_dir[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_nas_5gs_min_ue_t runtime;
  uint8_t auth_request[MINI_GNB_C_MAX_PAYLOAD];
  size_t auth_request_length = 0u;

  mini_gnb_c_make_output_dir("test_nas_5gs_min", output_dir, sizeof(output_dir));
  mini_gnb_c_nas_5gs_min_ue_init(&runtime);

  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_handle_downlink_nas(&runtime,
                                                                17921u,
                                                                1u,
                                                                true,
                                                                0x1234u,
                                                                true,
                                                                (const uint8_t[]){0x7eu, 0x00u, 0x5bu, 0x01u},
                                                                4u,
                                                                4) == 0,
                     "expected identity request handling");
  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_emit_due_uplinks(&runtime, output_dir, 8) == 0,
                     "expected identity response emission");
  mini_gnb_c_require_ul_nas_message_type(output_dir, 1u, 0x5cu, 9);

  mini_gnb_c_test_build_auth_request(auth_request, &auth_request_length);
  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_handle_downlink_nas(&runtime,
                                                                17921u,
                                                                1u,
                                                                true,
                                                                0x1234u,
                                                                true,
                                                                auth_request,
                                                                auth_request_length,
                                                                5) == 0,
                     "expected auth request handling");
  mini_gnb_c_require(runtime.security_context_valid, "expected derived security context after auth request");
  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_emit_due_uplinks(&runtime, output_dir, 9) == 0,
                     "expected auth response emission");
  mini_gnb_c_require_ul_nas_message_type(output_dir, 2u, 0x57u, 10);

  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_handle_downlink_nas(&runtime,
                                                                17921u,
                                                                1u,
                                                                true,
                                                                0x1234u,
                                                                true,
                                                                (const uint8_t[]){0x7eu, 0x00u, 0x5du, 0x02u},
                                                                4u,
                                                                6) == 0,
                     "expected security mode command handling");
  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_emit_due_uplinks(&runtime, output_dir, 10) == 0,
                     "expected security mode complete emission");
  mini_gnb_c_require_ul_nas_message_type(output_dir, 3u, 0x5eu, 11);
  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_emit_due_uplinks(&runtime, output_dir, 11) == 0,
                     "expected registration complete emission");
  mini_gnb_c_require_ul_nas_message_type(output_dir, 4u, 0x43u, 12);
  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_emit_due_uplinks(&runtime, output_dir, 12) == 0,
                     "expected pdu session request emission");
  mini_gnb_c_require_ul_nas_message_type(output_dir, 5u, 0x67u, 13);
}

void test_nas_5gs_min_polls_downlink_exchange(void) {
  char output_dir[MINI_GNB_C_MAX_PATH];
  mini_gnb_c_nas_5gs_min_ue_t runtime;

  mini_gnb_c_make_output_dir("test_nas_5gs_min_poll", output_dir, sizeof(output_dir));
  mini_gnb_c_nas_5gs_min_ue_init(&runtime);
  mini_gnb_c_require(mini_gnb_c_json_link_emit_event(output_dir,
                                                     "gnb_to_ue",
                                                     "gnb",
                                                     "DL_NAS",
                                                     1u,
                                                     5,
                                                     "{\"c_rnti\":17921,\"ran_ue_ngap_id\":1,\"amf_ue_ngap_id\":4660,"
                                                     "\"nas_hex\":\"7E005B01\"}",
                                                     NULL,
                                                     0u) == 0,
                     "expected seeded DL_NAS event");

  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_poll_exchange(&runtime, output_dir, 4) == 0,
                     "expected no future DL_NAS consumption");
  mini_gnb_c_require(runtime.next_gnb_to_ue_sequence == 1u, "expected DL_NAS sequence to stay pending");

  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_poll_exchange(&runtime, output_dir, 5) == 0,
                     "expected DL_NAS polling");
  mini_gnb_c_require(runtime.next_gnb_to_ue_sequence == 2u, "expected consumed DL_NAS sequence");
  mini_gnb_c_require(runtime.next_ue_to_gnb_nas_sequence == 1u, "expected no same-slot UL_NAS emission");

  mini_gnb_c_require(mini_gnb_c_nas_5gs_min_poll_exchange(&runtime, output_dir, 11) == 0,
                     "expected delayed UL_NAS emission");
  mini_gnb_c_require(runtime.next_ue_to_gnb_nas_sequence == 2u, "expected emitted one UL_NAS event");
  mini_gnb_c_require_ul_nas_message_type(output_dir, 1u, 0x5cu, 12);
}
