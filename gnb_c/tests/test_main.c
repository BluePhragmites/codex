#include <stdio.h>

typedef void (*mini_gnb_c_test_fn)(void);

void test_config_loads(void);
void test_sib1_schedule_uses_period_and_offset(void);
void test_prach_schedule_uses_period_and_offset(void);
void test_tbsize_lookup_table(void);
void test_open5gs_end_to_end_ue_config_loads_tun_internet_settings(void);
void test_config_loads_b210_runtime_overrides(void);
void test_core_session_tracks_user_plane_state(void);
void test_core_session_rejects_invalid_values(void);
void test_ue_context_store_promote_initializes_core_session(void);
void test_gnb_core_bridge_prepares_initial_ue_message(void);
void test_gnb_core_bridge_relays_followup_uplink_nas(void);
void test_gnb_core_bridge_skips_stale_and_waits_for_future_uplink_nas(void);
void test_gnb_core_bridge_parses_session_setup_state(void);
void test_gnb_core_bridge_relays_post_session_downlink_nas(void);
void test_gnb_core_bridge_ignores_disabled_config(void);
void test_ngap_runtime_builders_encode_expected_headers(void);
void test_ngap_runtime_extracts_open5gs_user_plane_state(void);
void test_air_pdu_build_and_parse_round_trip(void);
void test_air_pdu_rejects_crc_mismatch(void);
void test_air_pdu_rejects_invalid_header_fields(void);
void test_pcap_trace_writes_payload_and_udp_ipv4(void);
void test_nas_5gs_min_builds_followup_uplinks(void);
void test_nas_5gs_min_polls_downlink_exchange(void);
void test_rlc_lite_builds_and_reassembles_segmented_sdu(void);
void test_rlc_lite_rejects_out_of_order_segment(void);
void test_radio_frontend_initializes_mock_backend(void);
void test_radio_frontend_rejects_unsupported_backend_name(void);
void test_b210_probe_config_defaults(void);
void test_b210_tx_config_defaults(void);
void test_b210_trx_config_defaults(void);
void test_b210_gain_helpers_apply_shared_and_directional_overrides(void);
void test_b210_time_helpers_convert_hardware_time_and_sample_offsets(void);
void test_host_performance_plan_for_b210_skips_network_buffers(void);
void test_host_performance_plan_for_mock_is_not_applicable(void);
void test_sc16_ring_map_create_append_and_wrap(void);
void test_sc16_ring_map_dual_channel_payload_is_channel_major(void);
void test_sc16_ring_export_range_writes_per_channel_files(void);
void test_gtpu_builders_encode_expected_headers(void);
void test_gtpu_builders_reject_missing_state(void);
void test_n3_user_plane_activates_and_sends_uplink_gpdu(void);
void test_n3_user_plane_polls_downlink_packet(void);
void test_ue_ip_stack_min_generates_echo_reply(void);
void test_ue_ip_stack_min_ignores_non_ipv4_payload(void);
void test_json_link_builds_stable_event_path(void);
void test_json_link_emits_atomic_event_file(void);
void test_json_link_finds_event_by_sequence(void);
void test_shared_slot_link_round_trip(void);
void test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries(void);
void test_mini_ue_fsm_generates_default_event_sequence(void);
void test_mini_ue_fsm_skips_connected_traffic_when_disabled(void);
void test_mini_ue_runtime_uplink_queue_tracks_bytes_and_bsr_dirty(void);
void test_mini_ue_runtime_update_uplink_state_rearms_sr_after_grant_consumption(void);
void test_mini_ue_runtime_builds_bsr_from_current_queue_bytes(void);
void test_mini_ue_runtime_skips_new_payload_grant_without_queue(void);
void test_mini_ue_runtime_preserves_payload_kind_for_new_and_retx_grants(void);
void test_mini_ue_runtime_segments_ipv4_payload_across_multiple_grants(void);
void test_mini_ue_runtime_exports_ul_event_into_rx_dir(void);
void test_ra_manager_flow(void);
void test_ra_timeout(void);
void test_mac_rrc_and_msg4_contention_identity(void);
void test_integration_run(void);
void test_integration_slot_input_prach(void);
void test_integration_local_exchange_ue_plan(void);
void test_integration_shared_slot_ue_runtime(void);
void test_integration_shared_slot_ue_runtime_auto_nas_session_setup(void);
void test_integration_shared_slot_ue_runtime_generates_icmp_reply_payload(void);
void test_integration_shared_slot_ue_runtime_repeats_sr_for_pending_uplink_queue(void);
void test_integration_shared_slot_ue_runtime_consumes_uplink_queue_in_order(void);
void test_integration_shared_slot_tun_uplink_reaches_n3(void);
void test_integration_core_bridge_prepares_initial_message(void);
void test_integration_core_bridge_relays_followup_ul_nas(void);
void test_integration_core_bridge_extracts_session_setup_state(void);
void test_integration_core_bridge_relays_post_session_nas(void);
void test_integration_core_bridge_forwards_ul_ipv4_to_n3(void);
void test_integration_slot_text_transport(void);
void test_integration_slot_text_transport_continues_connected_ul_grants(void);
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
      {"test_prach_schedule_uses_period_and_offset", test_prach_schedule_uses_period_and_offset},
      {"test_tbsize_lookup_table", test_tbsize_lookup_table},
      {"test_open5gs_end_to_end_ue_config_loads_tun_internet_settings",
       test_open5gs_end_to_end_ue_config_loads_tun_internet_settings},
      {"test_config_loads_b210_runtime_overrides", test_config_loads_b210_runtime_overrides},
      {"test_core_session_tracks_user_plane_state", test_core_session_tracks_user_plane_state},
      {"test_core_session_rejects_invalid_values", test_core_session_rejects_invalid_values},
      {"test_ue_context_store_promote_initializes_core_session",
       test_ue_context_store_promote_initializes_core_session},
      {"test_gnb_core_bridge_prepares_initial_ue_message", test_gnb_core_bridge_prepares_initial_ue_message},
      {"test_gnb_core_bridge_relays_followup_uplink_nas", test_gnb_core_bridge_relays_followup_uplink_nas},
      {"test_gnb_core_bridge_skips_stale_and_waits_for_future_uplink_nas",
       test_gnb_core_bridge_skips_stale_and_waits_for_future_uplink_nas},
      {"test_gnb_core_bridge_parses_session_setup_state", test_gnb_core_bridge_parses_session_setup_state},
      {"test_gnb_core_bridge_relays_post_session_downlink_nas",
       test_gnb_core_bridge_relays_post_session_downlink_nas},
      {"test_gnb_core_bridge_ignores_disabled_config", test_gnb_core_bridge_ignores_disabled_config},
      {"test_ngap_runtime_builders_encode_expected_headers",
       test_ngap_runtime_builders_encode_expected_headers},
      {"test_ngap_runtime_extracts_open5gs_user_plane_state",
       test_ngap_runtime_extracts_open5gs_user_plane_state},
      {"test_air_pdu_build_and_parse_round_trip", test_air_pdu_build_and_parse_round_trip},
      {"test_air_pdu_rejects_crc_mismatch", test_air_pdu_rejects_crc_mismatch},
      {"test_air_pdu_rejects_invalid_header_fields", test_air_pdu_rejects_invalid_header_fields},
      {"test_pcap_trace_writes_payload_and_udp_ipv4", test_pcap_trace_writes_payload_and_udp_ipv4},
      {"test_nas_5gs_min_builds_followup_uplinks", test_nas_5gs_min_builds_followup_uplinks},
      {"test_nas_5gs_min_polls_downlink_exchange", test_nas_5gs_min_polls_downlink_exchange},
      {"test_rlc_lite_builds_and_reassembles_segmented_sdu", test_rlc_lite_builds_and_reassembles_segmented_sdu},
      {"test_rlc_lite_rejects_out_of_order_segment", test_rlc_lite_rejects_out_of_order_segment},
      {"test_radio_frontend_initializes_mock_backend", test_radio_frontend_initializes_mock_backend},
      {"test_radio_frontend_rejects_unsupported_backend_name",
       test_radio_frontend_rejects_unsupported_backend_name},
      {"test_b210_probe_config_defaults", test_b210_probe_config_defaults},
      {"test_b210_tx_config_defaults", test_b210_tx_config_defaults},
      {"test_b210_trx_config_defaults", test_b210_trx_config_defaults},
      {"test_b210_gain_helpers_apply_shared_and_directional_overrides",
       test_b210_gain_helpers_apply_shared_and_directional_overrides},
      {"test_b210_time_helpers_convert_hardware_time_and_sample_offsets",
       test_b210_time_helpers_convert_hardware_time_and_sample_offsets},
      {"test_host_performance_plan_for_b210_skips_network_buffers",
       test_host_performance_plan_for_b210_skips_network_buffers},
      {"test_host_performance_plan_for_mock_is_not_applicable",
       test_host_performance_plan_for_mock_is_not_applicable},
      {"test_sc16_ring_map_create_append_and_wrap", test_sc16_ring_map_create_append_and_wrap},
      {"test_sc16_ring_map_dual_channel_payload_is_channel_major",
       test_sc16_ring_map_dual_channel_payload_is_channel_major},
      {"test_sc16_ring_export_range_writes_per_channel_files",
       test_sc16_ring_export_range_writes_per_channel_files},
      {"test_gtpu_builders_encode_expected_headers", test_gtpu_builders_encode_expected_headers},
      {"test_gtpu_builders_reject_missing_state", test_gtpu_builders_reject_missing_state},
      {"test_n3_user_plane_activates_and_sends_uplink_gpdu",
       test_n3_user_plane_activates_and_sends_uplink_gpdu},
      {"test_n3_user_plane_polls_downlink_packet", test_n3_user_plane_polls_downlink_packet},
      {"test_ue_ip_stack_min_generates_echo_reply", test_ue_ip_stack_min_generates_echo_reply},
      {"test_ue_ip_stack_min_ignores_non_ipv4_payload", test_ue_ip_stack_min_ignores_non_ipv4_payload},
      {"test_json_link_builds_stable_event_path", test_json_link_builds_stable_event_path},
      {"test_json_link_emits_atomic_event_file", test_json_link_emits_atomic_event_file},
      {"test_json_link_finds_event_by_sequence", test_json_link_finds_event_by_sequence},
      {"test_shared_slot_link_round_trip", test_shared_slot_link_round_trip},
      {"test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries",
       test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries},
      {"test_mini_ue_fsm_generates_default_event_sequence", test_mini_ue_fsm_generates_default_event_sequence},
      {"test_mini_ue_fsm_skips_connected_traffic_when_disabled",
       test_mini_ue_fsm_skips_connected_traffic_when_disabled},
      {"test_mini_ue_runtime_uplink_queue_tracks_bytes_and_bsr_dirty",
       test_mini_ue_runtime_uplink_queue_tracks_bytes_and_bsr_dirty},
      {"test_mini_ue_runtime_update_uplink_state_rearms_sr_after_grant_consumption",
       test_mini_ue_runtime_update_uplink_state_rearms_sr_after_grant_consumption},
      {"test_mini_ue_runtime_builds_bsr_from_current_queue_bytes",
       test_mini_ue_runtime_builds_bsr_from_current_queue_bytes},
      {"test_mini_ue_runtime_skips_new_payload_grant_without_queue",
       test_mini_ue_runtime_skips_new_payload_grant_without_queue},
      {"test_mini_ue_runtime_preserves_payload_kind_for_new_and_retx_grants",
       test_mini_ue_runtime_preserves_payload_kind_for_new_and_retx_grants},
      {"test_mini_ue_runtime_segments_ipv4_payload_across_multiple_grants",
       test_mini_ue_runtime_segments_ipv4_payload_across_multiple_grants},
      {"test_mini_ue_runtime_exports_ul_event_into_rx_dir", test_mini_ue_runtime_exports_ul_event_into_rx_dir},
      {"test_ra_manager_flow", test_ra_manager_flow},
      {"test_ra_timeout", test_ra_timeout},
      {"test_mac_rrc_and_msg4_contention_identity", test_mac_rrc_and_msg4_contention_identity},
      {"test_integration_run", test_integration_run},
      {"test_integration_slot_input_prach", test_integration_slot_input_prach},
      {"test_integration_local_exchange_ue_plan", test_integration_local_exchange_ue_plan},
      {"test_integration_shared_slot_ue_runtime", test_integration_shared_slot_ue_runtime},
      {"test_integration_shared_slot_ue_runtime_auto_nas_session_setup",
       test_integration_shared_slot_ue_runtime_auto_nas_session_setup},
      {"test_integration_shared_slot_ue_runtime_generates_icmp_reply_payload",
       test_integration_shared_slot_ue_runtime_generates_icmp_reply_payload},
      {"test_integration_shared_slot_ue_runtime_repeats_sr_for_pending_uplink_queue",
       test_integration_shared_slot_ue_runtime_repeats_sr_for_pending_uplink_queue},
      {"test_integration_shared_slot_ue_runtime_consumes_uplink_queue_in_order",
       test_integration_shared_slot_ue_runtime_consumes_uplink_queue_in_order},
      {"test_integration_shared_slot_tun_uplink_reaches_n3", test_integration_shared_slot_tun_uplink_reaches_n3},
      {"test_integration_core_bridge_prepares_initial_message", test_integration_core_bridge_prepares_initial_message},
      {"test_integration_core_bridge_relays_followup_ul_nas", test_integration_core_bridge_relays_followup_ul_nas},
      {"test_integration_core_bridge_extracts_session_setup_state",
       test_integration_core_bridge_extracts_session_setup_state},
      {"test_integration_core_bridge_relays_post_session_nas",
       test_integration_core_bridge_relays_post_session_nas},
      {"test_integration_core_bridge_forwards_ul_ipv4_to_n3",
       test_integration_core_bridge_forwards_ul_ipv4_to_n3},
      {"test_integration_slot_text_transport", test_integration_slot_text_transport},
      {"test_integration_slot_text_transport_continues_connected_ul_grants",
       test_integration_slot_text_transport_continues_connected_ul_grants},
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
