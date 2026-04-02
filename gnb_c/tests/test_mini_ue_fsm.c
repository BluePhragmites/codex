#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/ue/mini_ue_fsm.h"

void test_mini_ue_fsm_generates_default_event_sequence(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  char payload_json[2048];
  mini_gnb_c_config_t config;
  mini_gnb_c_mini_ue_fsm_t fsm;
  mini_gnb_c_ue_event_t event;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");

  mini_gnb_c_mini_ue_fsm_init(&fsm, &config.sim);

  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 1, "expected PRACH event");
  mini_gnb_c_require(event.type == MINI_GNB_C_UE_EVENT_PRACH && event.abs_slot == 2,
                     "expected PRACH at configured trigger slot");
  mini_gnb_c_require(mini_gnb_c_ue_event_build_payload_json(&event, payload_json, sizeof(payload_json)) == 0,
                     "expected PRACH payload json");
  mini_gnb_c_require(strstr(payload_json, "\"preamble_id\":27") != NULL, "expected PRACH preamble id");

  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 1, "expected MSG3 event");
  mini_gnb_c_require(event.type == MINI_GNB_C_UE_EVENT_MSG3 && event.abs_slot == 6 && event.rnti == 0x4601u,
                     "expected deterministic MSG3 timing and RNTI");
  mini_gnb_c_require(mini_gnb_c_ue_event_build_payload_json(&event, payload_json, sizeof(payload_json)) == 0,
                     "expected MSG3 payload json");
  mini_gnb_c_require(strstr(payload_json, "\"payload_hex\":\"020201460110A1B2C3D4E5F603011122334455667788\"") != NULL,
                     "expected deterministic MSG3 MAC payload");

  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 1, "expected PUCCH SR event");
  mini_gnb_c_require(event.type == MINI_GNB_C_UE_EVENT_PUCCH_SR && event.abs_slot == 12,
                     "expected SR slot after Msg4 timing");

  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 1, "expected BSR event");
  mini_gnb_c_require(event.type == MINI_GNB_C_UE_EVENT_BSR && event.abs_slot == 15,
                     "expected scheduled BSR slot");
  mini_gnb_c_require(mini_gnb_c_ue_event_build_payload_json(&event, payload_json, sizeof(payload_json)) == 0,
                     "expected BSR payload json");
  mini_gnb_c_require(strstr(payload_json, "\"payload_text\":\"BSR|bytes=384\"") != NULL,
                     "expected BSR payload text");

  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 1, "expected UL DATA event");
  mini_gnb_c_require(event.type == MINI_GNB_C_UE_EVENT_DATA && event.abs_slot == 18,
                     "expected scheduled UL DATA slot");
  mini_gnb_c_require(mini_gnb_c_ue_event_build_payload_json(&event, payload_json, sizeof(payload_json)) == 0,
                     "expected data payload json");
  mini_gnb_c_require(strstr(payload_json, "\"payload_hex\":\"554C5F44415441\"") != NULL,
                     "expected deterministic UL payload");

  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 0, "expected end of UE event stream");
  mini_gnb_c_require(!mini_gnb_c_mini_ue_fsm_has_pending_event(&fsm), "expected FSM completion");
}

void test_mini_ue_fsm_skips_connected_traffic_when_disabled(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_mini_ue_fsm_t fsm;
  mini_gnb_c_ue_event_t event;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");

  config.sim.post_msg4_traffic_enabled = false;
  mini_gnb_c_mini_ue_fsm_init(&fsm, &config.sim);
  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 1, "expected PRACH event");
  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 1, "expected MSG3 event");
  mini_gnb_c_require(event.type == MINI_GNB_C_UE_EVENT_MSG3, "expected MSG3 before completion");
  mini_gnb_c_require(mini_gnb_c_mini_ue_fsm_next_event(&fsm, &event) == 0,
                     "expected no connected traffic when disabled");
}
