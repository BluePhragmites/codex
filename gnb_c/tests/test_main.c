#include <stdio.h>

typedef void (*mini_gnb_c_test_fn)(void);

void test_config_loads(void);
void test_sib1_schedule_uses_period_and_offset(void);
void test_tbsize_lookup_table(void);
void test_core_session_tracks_user_plane_state(void);
void test_core_session_rejects_invalid_values(void);
void test_ue_context_store_promote_initializes_core_session(void);
void test_gnb_core_bridge_prepares_initial_ue_message(void);
void test_gnb_core_bridge_ignores_disabled_config(void);
void test_ngap_runtime_builders_encode_expected_headers(void);
void test_ngap_runtime_extracts_open5gs_user_plane_state(void);
void test_gtpu_builders_encode_expected_headers(void);
void test_gtpu_builders_reject_missing_state(void);
void test_json_link_builds_stable_event_path(void);
void test_json_link_emits_atomic_event_file(void);
void test_mini_ue_fsm_generates_default_event_sequence(void);
void test_mini_ue_fsm_skips_connected_traffic_when_disabled(void);
void test_ra_manager_flow(void);
void test_ra_timeout(void);
void test_mac_rrc_and_msg4_contention_identity(void);
void test_integration_run(void);
void test_integration_slot_input_prach(void);
void test_integration_local_exchange_ue_plan(void);
void test_integration_core_bridge_prepares_initial_message(void);
void test_integration_slot_text_transport(void);
void test_integration_msg3_missing_retries_prach(void);
void test_integration_msg3_rnti_mismatch_rejected_after_retry(void);
void test_integration_scripted_schedule_files(void);
void test_integration_scripted_pdcch_files(void);

int main(void) {
  struct {
    const char* name;
    mini_gnb_c_test_fn fn;
  } tests[] = {
      {"test_config_loads", test_config_loads},
      {"test_sib1_schedule_uses_period_and_offset", test_sib1_schedule_uses_period_and_offset},
      {"test_tbsize_lookup_table", test_tbsize_lookup_table},
      {"test_core_session_tracks_user_plane_state", test_core_session_tracks_user_plane_state},
      {"test_core_session_rejects_invalid_values", test_core_session_rejects_invalid_values},
      {"test_ue_context_store_promote_initializes_core_session",
       test_ue_context_store_promote_initializes_core_session},
      {"test_gnb_core_bridge_prepares_initial_ue_message", test_gnb_core_bridge_prepares_initial_ue_message},
      {"test_gnb_core_bridge_ignores_disabled_config", test_gnb_core_bridge_ignores_disabled_config},
      {"test_ngap_runtime_builders_encode_expected_headers",
       test_ngap_runtime_builders_encode_expected_headers},
      {"test_ngap_runtime_extracts_open5gs_user_plane_state",
       test_ngap_runtime_extracts_open5gs_user_plane_state},
      {"test_gtpu_builders_encode_expected_headers", test_gtpu_builders_encode_expected_headers},
      {"test_gtpu_builders_reject_missing_state", test_gtpu_builders_reject_missing_state},
      {"test_json_link_builds_stable_event_path", test_json_link_builds_stable_event_path},
      {"test_json_link_emits_atomic_event_file", test_json_link_emits_atomic_event_file},
      {"test_mini_ue_fsm_generates_default_event_sequence", test_mini_ue_fsm_generates_default_event_sequence},
      {"test_mini_ue_fsm_skips_connected_traffic_when_disabled",
       test_mini_ue_fsm_skips_connected_traffic_when_disabled},
      {"test_ra_manager_flow", test_ra_manager_flow},
      {"test_ra_timeout", test_ra_timeout},
      {"test_mac_rrc_and_msg4_contention_identity", test_mac_rrc_and_msg4_contention_identity},
      {"test_integration_run", test_integration_run},
      {"test_integration_slot_input_prach", test_integration_slot_input_prach},
      {"test_integration_local_exchange_ue_plan", test_integration_local_exchange_ue_plan},
      {"test_integration_core_bridge_prepares_initial_message", test_integration_core_bridge_prepares_initial_message},
      {"test_integration_slot_text_transport", test_integration_slot_text_transport},
      {"test_integration_msg3_missing_retries_prach", test_integration_msg3_missing_retries_prach},
      {"test_integration_msg3_rnti_mismatch_rejected_after_retry",
       test_integration_msg3_rnti_mismatch_rejected_after_retry},
      {"test_integration_scripted_schedule_files", test_integration_scripted_schedule_files},
      {"test_integration_scripted_pdcch_files", test_integration_scripted_pdcch_files},
  };
  size_t i = 0;

  for (i = 0; i < (sizeof(tests) / sizeof(tests[0])); ++i) {
    tests[i].fn();
    printf("[PASS] %s\n", tests[i].name);
  }
  return 0;
}
