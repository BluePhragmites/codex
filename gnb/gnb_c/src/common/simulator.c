#include "mini_gnb_c/common/simulator.h"

#include <stdio.h>
#include <string.h>

#include "mini_gnb_c/common/hex.h"

static const char* mini_gnb_c_bool_string(bool value) {
  return value ? "true" : "false";
}

static void mini_gnb_c_join_lcid_sequence(const mini_gnb_c_mac_ul_parse_result_t* mac_result,
                                          char* out,
                                          size_t out_size) {
  size_t i = 0;
  size_t offset = 0;

  if (out == NULL || out_size == 0U) {
    return;
  }

  out[0] = '\0';
  if (mac_result == NULL) {
    return;
  }

  for (i = 0; i < mac_result->lcid_count; ++i) {
    int written = snprintf(out + offset,
                           out_size - offset,
                           "%s%d",
                           (i == 0U) ? "" : "/",
                           mac_result->lcid_sequence[i]);
    if (written < 0 || (size_t)written >= (out_size - offset)) {
      break;
    }
    offset += (size_t)written;
  }
}

void mini_gnb_c_simulator_init(mini_gnb_c_simulator_t* simulator,
                               const mini_gnb_c_config_t* config,
                               const char* output_dir) {
  if (simulator == NULL || config == NULL) {
    return;
  }

  memset(simulator, 0, sizeof(*simulator));
  simulator->config = *config;
  mini_gnb_c_metrics_trace_init(&simulator->metrics, output_dir);
  mini_gnb_c_slot_engine_init(&simulator->slot_engine, config);
  mini_gnb_c_mock_radio_frontend_init(&simulator->radio, &config->rf, &config->sim);
  mini_gnb_c_broadcast_engine_init(&simulator->broadcast,
                                   &config->cell,
                                   &config->prach,
                                   &config->broadcast);
  mini_gnb_c_mock_prach_detector_init(&simulator->prach_detector, &config->sim);
  mini_gnb_c_ra_manager_init(&simulator->ra_manager, &config->prach, &config->sim);
  mini_gnb_c_initial_access_scheduler_init(&simulator->scheduler);
  mini_gnb_c_mock_msg3_receiver_init(&simulator->msg3_receiver, &config->sim);
  mini_gnb_c_mock_dl_phy_mapper_init(&simulator->dl_mapper);
  mini_gnb_c_ue_context_store_init(&simulator->ue_store);
}

int mini_gnb_c_simulator_run(mini_gnb_c_simulator_t* simulator,
                             mini_gnb_c_run_summary_t* out_summary) {
  int abs_slot = 0;

  if (simulator == NULL || out_summary == NULL) {
    return -1;
  }

  mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                 "main",
                                 "Starting mini gNB Msg1-Msg4 prototype run.",
                                 -1,
                                 "total_slots=%d,slots_per_frame=%d",
                                 simulator->config.sim.total_slots,
                                 simulator->config.sim.slots_per_frame);

  for (abs_slot = 0; abs_slot < simulator->config.sim.total_slots; ++abs_slot) {
    mini_gnb_c_slot_indication_t slot;
    mini_gnb_c_radio_burst_t burst;
    mini_gnb_c_prach_indication_t prach;
    mini_gnb_c_ul_grant_for_msg3_t msg3_grants[MINI_GNB_C_MAX_MSG3_GRANTS];
    mini_gnb_c_dl_grant_t dl_grants[MINI_GNB_C_MAX_GRANTS + 2U];
    mini_gnb_c_tx_grid_patch_t patches[MINI_GNB_C_MAX_GRANTS + 2U];
    size_t msg3_grant_count = 0;
    size_t dl_grant_count = 0;
    size_t patch_count = 0;
    size_t i = 0;

    mini_gnb_c_slot_engine_make_slot(&simulator->slot_engine, abs_slot, &slot);
    mini_gnb_c_mock_radio_frontend_receive(&simulator->radio, &slot, &burst);
    if (burst.ul_type != MINI_GNB_C_UL_BURST_NONE) {
      mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                     "radio_rx",
                                     "Received UL burst.",
                                     slot.abs_slot,
                                     "type=%s,nof_samples=%u,rnti=%u,preamble_id=%u",
                                     mini_gnb_c_ul_burst_type_to_string(burst.ul_type),
                                     burst.nof_samples,
                                     burst.rnti,
                                     burst.preamble_id);
    }
    mini_gnb_c_ra_manager_expire(&simulator->ra_manager, &slot, &simulator->metrics);

    if (mini_gnb_c_mock_prach_detector_detect(&simulator->prach_detector, &slot, &burst, &prach) &&
        prach.valid) {
      mini_gnb_c_ra_schedule_request_t request;
      mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "prach_detect_ok", 1U);
      mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                     "prach_detector",
                                     "Detected PRACH occasion.",
                                     slot.abs_slot,
                                     "preamble_id=%u,ta_est=%d,peak_metric=%.2f",
                                     prach.preamble_id,
                                     prach.ta_est,
                                     prach.peak_metric);
      if (mini_gnb_c_ra_manager_on_prach(&simulator->ra_manager,
                                         &prach,
                                         &slot,
                                         &simulator->metrics,
                                         &request)) {
        mini_gnb_c_initial_access_scheduler_queue_rar(&simulator->scheduler,
                                                      &request,
                                                      &simulator->metrics);
        mini_gnb_c_mock_radio_frontend_arm_msg3(&simulator->radio, &request.ul_grant);
      }
    }

    msg3_grant_count = mini_gnb_c_initial_access_scheduler_pop_due_msg3_grants(&simulator->scheduler,
                                                                                slot.abs_slot,
                                                                                msg3_grants,
                                                                                MINI_GNB_C_MAX_MSG3_GRANTS);
    for (i = 0; i < msg3_grant_count; ++i) {
      mini_gnb_c_msg3_decode_indication_t msg3;
      char mac_pdu_hex[MINI_GNB_C_MAX_PAYLOAD * 2U + 1U];

      if (!mini_gnb_c_mock_msg3_receiver_decode(&simulator->msg3_receiver,
                                                &slot,
                                                &msg3_grants[i],
                                                &burst,
                                                &msg3)) {
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       "pusch_msg3_receiver",
                                       "No Msg3 burst decoded for due UL grant.",
                                       slot.abs_slot,
                                       "rnti=%u,expected_abs_slot=%d,ul_burst=%s",
                                       msg3_grants[i].tc_rnti,
                                       msg3_grants[i].abs_slot,
                                       mini_gnb_c_ul_burst_type_to_string(burst.ul_type));
        continue;
      }

      mini_gnb_c_metrics_trace_increment_named(&simulator->metrics,
                                               msg3.crc_ok ? "msg3_crc_ok" : "msg3_crc_fail",
                                               1U);
      (void)mini_gnb_c_bytes_to_hex(msg3.mac_pdu.bytes,
                                    msg3.mac_pdu.len,
                                    mac_pdu_hex,
                                    sizeof(mac_pdu_hex));
      mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                     "pusch_msg3_receiver",
                                     "Decoded Msg3 candidate.",
                                     slot.abs_slot,
                                     "rnti=%u,crc_ok=%s,snr_db=%.2f,evm=%.2f,mac_pdu=%s",
                                     msg3.rnti,
                                     mini_gnb_c_bool_string(msg3.crc_ok),
                                     msg3.snr_db,
                                     msg3.evm,
                                     mac_pdu_hex);

      if (!msg3.crc_ok) {
        continue;
      }

      {
        mini_gnb_c_mac_ul_parse_result_t mac_result;
        char lcid_sequence[MINI_GNB_C_MAX_TEXT];

        mini_gnb_c_mac_ul_demux_parse(&msg3.mac_pdu, &mac_result);
        mini_gnb_c_join_lcid_sequence(&mac_result, lcid_sequence, sizeof(lcid_sequence));
        mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                       "mac_ul_demux",
                                       "Parsed Msg3 MAC PDU.",
                                       slot.abs_slot,
                                       "parse_ok=%s,has_ul_ccch=%s,has_crnti_ce=%s,lcid_sequence=%s",
                                       mini_gnb_c_bool_string(mac_result.parse_ok),
                                       mini_gnb_c_bool_string(mac_result.has_ul_ccch),
                                       mini_gnb_c_bool_string(mac_result.has_crnti_ce),
                                       lcid_sequence);

        if (!mac_result.parse_ok || !mac_result.has_ul_ccch) {
          continue;
        }

        {
          mini_gnb_c_rrc_setup_request_info_t request_info;
          mini_gnb_c_rrc_setup_blob_t rrc_setup;
          char contention_id_hex[MINI_GNB_C_MAX_TEXT];

          mini_gnb_c_parse_rrc_setup_request(&mac_result.ul_ccch_sdu, &request_info);
          (void)mini_gnb_c_bytes_to_hex(request_info.contention_id48,
                                        6U,
                                        contention_id_hex,
                                        sizeof(contention_id_hex));
          mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                         "rrc_ccch_stub",
                                         "Parsed RRCSetupRequest.",
                                         slot.abs_slot,
                                         "valid=%s,establishment_cause=%u,ue_identity_type=%u,contention_id48=%s",
                                         mini_gnb_c_bool_string(request_info.valid),
                                         request_info.establishment_cause,
                                         request_info.ue_identity_type,
                                         contention_id_hex);

          if (!request_info.valid) {
            continue;
          }

          mini_gnb_c_build_rrc_setup(&request_info, &rrc_setup);
          if (simulator->ra_manager.has_active_context) {
            mini_gnb_c_ue_context_t* ue_context =
                mini_gnb_c_ue_context_store_promote(&simulator->ue_store,
                                                    &simulator->ra_manager.active_context,
                                                    &request_info,
                                                    slot.abs_slot);
            if (ue_context != NULL) {
              mini_gnb_c_metrics_trace_event(&simulator->metrics,
                                             "ue_context_store",
                                             "Promoted RA context into minimal UE context.",
                                             slot.abs_slot,
                                             "tc_rnti=%u,c_rnti=%u,contention_id48=%s",
                                             ue_context->tc_rnti,
                                             ue_context->c_rnti,
                                             contention_id_hex);
            }
          }

          {
            mini_gnb_c_msg4_schedule_request_t msg4_request;
            if (mini_gnb_c_ra_manager_on_msg3_success(&simulator->ra_manager,
                                                      &msg3,
                                                      &mac_result,
                                                      &request_info,
                                                      &rrc_setup,
                                                      &slot,
                                                      &simulator->metrics,
                                                      &msg4_request)) {
              mini_gnb_c_initial_access_scheduler_queue_msg4(&simulator->scheduler,
                                                             &msg4_request,
                                                             &simulator->metrics);
            }
          }
        }
      }
    }

    dl_grant_count = mini_gnb_c_initial_access_scheduler_pop_due_downlink(&simulator->scheduler,
                                                                           slot.abs_slot,
                                                                           dl_grants,
                                                                           MINI_GNB_C_MAX_GRANTS);
    dl_grant_count += mini_gnb_c_broadcast_schedule(&simulator->broadcast,
                                                    &slot,
                                                    &dl_grants[dl_grant_count],
                                                    (MINI_GNB_C_MAX_GRANTS + 2U) - dl_grant_count);

    if (dl_grant_count > 0U) {
      patch_count = mini_gnb_c_mock_dl_phy_mapper_map(&simulator->dl_mapper,
                                                      &slot,
                                                      dl_grants,
                                                      dl_grant_count,
                                                      patches,
                                                      MINI_GNB_C_MAX_GRANTS + 2U);
      mini_gnb_c_mock_radio_frontend_submit_tx(&simulator->radio,
                                               &slot,
                                               patches,
                                               patch_count,
                                               &simulator->metrics);

      for (i = 0; i < dl_grant_count; ++i) {
        if (dl_grants[i].type == MINI_GNB_C_DL_OBJ_RAR) {
          mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "rar_sent", 1U);
          mini_gnb_c_ra_manager_mark_rar_sent(&simulator->ra_manager,
                                              dl_grants[i].rnti,
                                              &slot,
                                              &simulator->metrics);
        } else if (dl_grants[i].type == MINI_GNB_C_DL_OBJ_MSG4) {
          mini_gnb_c_metrics_trace_increment_named(&simulator->metrics, "rrcsetup_sent", 1U);
          mini_gnb_c_ra_manager_mark_msg4_sent(&simulator->ra_manager,
                                               dl_grants[i].rnti,
                                               &slot,
                                               &simulator->metrics);
          mini_gnb_c_ue_context_store_mark_rrc_setup_sent(&simulator->ue_store,
                                                          dl_grants[i].rnti,
                                                          slot.abs_slot);
        }
      }
    }

    {
      mini_gnb_c_slot_perf_t perf;
      memset(&perf, 0, sizeof(perf));
      perf.abs_slot = slot.abs_slot;
      perf.mac_latency_us = 120 + (int)slot.slot;
      perf.dl_build_latency_us = 80 + (int)(dl_grant_count * 5U);
      perf.ul_decode_latency_us = 60 + (int)(msg3_grant_count * 10U);
      mini_gnb_c_metrics_trace_add_slot_perf(&simulator->metrics, &perf);
    }
  }

  return mini_gnb_c_metrics_trace_flush(&simulator->metrics,
                                        simulator->ra_manager.has_active_context,
                                        simulator->ra_manager.has_active_context
                                            ? &simulator->ra_manager.active_context
                                            : NULL,
                                        simulator->ue_store.contexts,
                                        simulator->ue_store.count,
                                        simulator->radio.tx_burst_count,
                                        simulator->radio.last_hw_time_ns,
                                        out_summary);
}
