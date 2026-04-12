#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/common/json_utils.h"
#include "mini_gnb_c/config/config_loader.h"
#include "mini_gnb_c/radio/b210_app_runtime.h"
#include "mini_gnb_c/timing/slot_engine.h"

void test_config_loads(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");
  mini_gnb_c_require(config.cell.pci == 1U, "expected pci=1");
  mini_gnb_c_require(config.cell.band == 78U, "expected band n78");
  mini_gnb_c_require(config.prach.ra_resp_window == 4U, "expected ra response window");
  mini_gnb_c_require(strcmp(config.rf.device_driver, "mock") == 0, "expected mock radio frontend");
  mini_gnb_c_require(config.rf.subdev[0] == '\0', "expected empty RF subdevice by default");
  mini_gnb_c_require(config.rf.runtime_mode == MINI_GNB_C_RF_RUNTIME_MODE_SIMULATOR,
                     "expected simulator RF runtime mode by default");
  mini_gnb_c_require(config.rf.freq_hz == 2462000000.0, "expected default RF center frequency");
  mini_gnb_c_require(config.rf.rx_freq_hz == 2462000000.0, "expected default RF RX center frequency");
  mini_gnb_c_require(config.rf.tx_freq_hz == 2462000000.0, "expected default RF TX center frequency");
  mini_gnb_c_require(config.rf.bandwidth_hz == 20000000.0, "expected default RF bandwidth");
  mini_gnb_c_require(config.rf.duration_sec == 1.0, "expected default RF duration");
  mini_gnb_c_require(config.rf.duration_mode == MINI_GNB_C_RF_DURATION_MODE_SAMPLES,
                     "expected sample-target RF duration mode");
  mini_gnb_c_require(config.rf.channel == 0u, "expected default RF starting channel");
  mini_gnb_c_require(config.rf.channel_count == 1u, "expected default single-channel RF runtime");
  mini_gnb_c_require(config.rf.rx_cpu_core == -1, "expected RX CPU pinning disabled by default");
  mini_gnb_c_require(config.rf.tx_cpu_core == -1, "expected TX CPU pinning disabled by default");
  mini_gnb_c_require(config.rf.apply_host_tuning, "expected host tuning enabled by default");
  mini_gnb_c_require(config.rf.require_ref_lock, "expected reference lock required by default");
  mini_gnb_c_require(config.rf.require_lo_lock, "expected LO lock required by default");
  mini_gnb_c_require(config.rf.rx_output_file[0] == '\0', "expected no default RX output override");
  mini_gnb_c_require(config.rf.tx_input_file[0] == '\0', "expected no default TX input override");
  mini_gnb_c_require(config.rf.rx_ring_map[0] == '\0', "expected no default RX ring override");
  mini_gnb_c_require(config.rf.tx_ring_map[0] == '\0', "expected no default TX ring override");
  mini_gnb_c_require(config.rf.ring_block_samples == 4096u, "expected default ring block size");
  mini_gnb_c_require(config.rf.ring_block_count == 1024u, "expected default ring block count");
  mini_gnb_c_require(config.rf.tx_prefetch_samples == 0u, "expected TX prefetch override disabled by default");
  mini_gnb_c_require(!mini_gnb_c_rf_app_runtime_requested(&config),
                     "expected default config to stay on simulator/shared-slot path");
  mini_gnb_c_require(!config.real_cell.enabled, "expected real-cell target disabled by default");
  mini_gnb_c_require(strcmp(config.real_cell.profile_name, "b210_n78_demo") == 0,
                     "expected default B210 target profile");
  mini_gnb_c_require(strcmp(config.real_cell.target_backend, "uhd-b210") == 0,
                     "expected default B210 target backend");
  mini_gnb_c_require(config.real_cell.band == 78U, "expected default B210 target band");
  mini_gnb_c_require(config.real_cell.common_scs_khz == 30U, "expected default B210 target SCS");
  mini_gnb_c_require(config.real_cell.channel_bandwidth_mhz == 20U, "expected default B210 target bandwidth");
  mini_gnb_c_require(config.real_cell.dl_arfcn == 627334U, "expected default B210 target arfcn");
  mini_gnb_c_require(strcmp(config.real_cell.plmn, "00101") == 0, "expected default B210 target PLMN");
  mini_gnb_c_require(config.real_cell.tac == 7U, "expected default B210 target TAC");
  mini_gnb_c_require(!config.core.enabled, "expected core bridge disabled by default");
  mini_gnb_c_require(strcmp(config.core.amf_ip, "127.0.0.5") == 0, "expected default AMF IP");
  mini_gnb_c_require(config.core.amf_port == 38412u, "expected default AMF port");
  mini_gnb_c_require(config.core.upf_port == 2152u, "expected default UPF port");
  mini_gnb_c_require(config.core.timeout_ms == 5000u, "expected default AMF timeout");
  mini_gnb_c_require(config.core.ran_ue_ngap_id_base == 1u, "expected default RAN UE NGAP ID base");
  mini_gnb_c_require(config.core.default_pdu_session_id == 1u, "expected default requested PDU session ID");
  mini_gnb_c_require(config.core.ngap_trace_pcap[0] == '\0', "expected auto NGAP trace path");
  mini_gnb_c_require(config.core.gtpu_trace_pcap[0] == '\0', "expected auto GTP-U trace path");
  mini_gnb_c_require(config.sim.total_slots == 20, "expected total slot count");
  mini_gnb_c_require(config.sim.slot_sleep_ms == 0u, "expected default slot pacing disabled");
  mini_gnb_c_require(config.sim.prach_retry_delay_slots == 4, "expected PRACH retry delay");
  mini_gnb_c_require(config.sim.msg3_present, "expected Msg3 burst enabled");
  mini_gnb_c_require(strcmp(config.sim.ul_input_dir, "input") == 0, "expected default slot input directory");
  mini_gnb_c_require(strcmp(config.sim.local_exchange_dir, "out/local_exchange") == 0,
                     "expected default local exchange directory");
  mini_gnb_c_require(config.sim.shared_slot_path[0] == '\0', "expected shared slot path disabled by default");
  mini_gnb_c_require(config.sim.shared_slot_timeout_ms == 100u, "expected default shared slot timeout");
  mini_gnb_c_require(!config.sim.ue_tun_enabled, "expected UE TUN disabled by default");
  mini_gnb_c_require(strcmp(config.sim.ue_tun_name, "miniue0") == 0, "expected default UE TUN name");
  mini_gnb_c_require(config.sim.ue_tun_mtu == 1400u, "expected default UE TUN mtu");
  mini_gnb_c_require(config.sim.ue_tun_prefix_len == 16u, "expected default UE TUN prefix length");
  mini_gnb_c_require(config.sim.ue_tun_isolate_netns, "expected UE TUN netns isolation enabled by default");
  mini_gnb_c_require(config.sim.ue_tun_add_default_route, "expected UE TUN default route enabled by default");
  mini_gnb_c_require(config.sim.ue_tun_netns_name[0] == '\0', "expected UE TUN netns name disabled by default");
  mini_gnb_c_require(config.sim.ue_tun_dns_server_ipv4[0] == '\0',
                     "expected UE TUN DNS override disabled by default");
  mini_gnb_c_require(config.sim.post_msg4_traffic_enabled, "expected post-Msg4 traffic enabled");
  mini_gnb_c_require(config.sim.post_msg4_dl_pdcch_delay_slots == 1, "expected DL PDCCH delay");
  mini_gnb_c_require(config.sim.post_msg4_dl_time_indicator == 1, "expected DL time indicator");
  mini_gnb_c_require(config.sim.post_msg4_dl_data_to_ul_ack_slots == 1, "expected DL ACK timing");
  mini_gnb_c_require(config.sim.post_msg4_sr_period_slots == 10, "expected SR period");
  mini_gnb_c_require(config.sim.post_msg4_sr_offset_slot == 2, "expected SR offset");
  mini_gnb_c_require(config.sim.post_msg4_ul_grant_delay_slots == 1, "expected UL grant delay");
  mini_gnb_c_require(config.sim.post_msg4_ul_time_indicator == 2, "expected UL time indicator");
  mini_gnb_c_require(config.sim.post_msg4_dl_harq_process_count == 2, "expected DL HARQ process count");
  mini_gnb_c_require(config.sim.post_msg4_ul_harq_process_count == 2, "expected UL HARQ process count");
  mini_gnb_c_require(config.sim.ul_data_present, "expected synthetic UL data enabled");
  mini_gnb_c_require(config.sim.ul_bsr_buffer_size_bytes == 384, "expected default BSR size");
  mini_gnb_c_require(config.sim.include_crnti_ce, "expected simulated C-RNTI CE");
  mini_gnb_c_require(config.broadcast.sib1_period_slots == 8, "expected SIB1 period");
  mini_gnb_c_require(config.broadcast.sib1_offset_slot == 4, "expected SIB1 offset");
  mini_gnb_c_require(config.broadcast.prach_period_slots == 10, "expected PRACH period");
  mini_gnb_c_require(config.broadcast.prach_offset_slot == 2, "expected PRACH offset");
}

void test_sib1_schedule_uses_period_and_offset(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_slot_indication_t slot0;
  mini_gnb_c_slot_indication_t slot4;
  mini_gnb_c_slot_indication_t slot12;
  mini_gnb_c_slot_indication_t slot20;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");

  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 0, &slot0);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 4, &slot4);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 12, &slot12);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 20, &slot20);

  mini_gnb_c_require(!slot0.has_sib1, "expected no SIB1 at abs_slot 0 when offset is 4");
  mini_gnb_c_require(slot4.has_sib1, "expected SIB1 at abs_slot 4");
  mini_gnb_c_require(slot12.has_sib1, "expected SIB1 at abs_slot 12");
  mini_gnb_c_require(slot20.has_sib1, "expected SIB1 at abs_slot 20");
}

void test_prach_schedule_uses_period_and_offset(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  mini_gnb_c_slot_engine_t slot_engine;
  mini_gnb_c_slot_indication_t slot1;
  mini_gnb_c_slot_indication_t slot2;
  mini_gnb_c_slot_indication_t slot12;

  mini_gnb_c_default_config_path(config_path, sizeof(config_path));
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected config to load");

  mini_gnb_c_slot_engine_init(&slot_engine, &config);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 1, &slot1);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 2, &slot2);
  mini_gnb_c_slot_engine_make_slot(&slot_engine, 12, &slot12);

  mini_gnb_c_require(!slot1.has_prach_occasion, "expected no PRACH occasion at abs_slot 1");
  mini_gnb_c_require(slot2.has_prach_occasion, "expected PRACH occasion at abs_slot 2");
  mini_gnb_c_require(slot12.has_prach_occasion, "expected periodic PRACH occasion at abs_slot 12");
}

void test_tbsize_lookup_table(void) {
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(8U, 4U) == 16U, "expected compact UL BSR tbsize");
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(24U, 8U) == 96U, "expected large UL payload tbsize");
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(24U, 9U) == 120U, "expected DL data tbsize");
  mini_gnb_c_require(mini_gnb_c_lookup_tbsize(7U, 9U) == 0U, "expected unknown PRB/MCS pair to map to 0");
}

void test_open5gs_end_to_end_ue_config_loads_tun_internet_settings(void) {
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;

  (void)snprintf(config_path,
                 sizeof(config_path),
                 "%s/config/example_open5gs_end_to_end_ue.yml",
                 MINI_GNB_C_SOURCE_DIR);
  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected Open5GS UE config to load");
  mini_gnb_c_require(config.sim.ue_tun_enabled, "expected end-to-end UE TUN enabled");
  mini_gnb_c_require(config.sim.ue_tun_add_default_route, "expected end-to-end UE default route enabled");
  mini_gnb_c_require(strcmp(config.sim.ue_tun_netns_name, "miniue-demo") == 0,
                     "expected end-to-end UE netns name");
  mini_gnb_c_require(strcmp(config.sim.ue_tun_dns_server_ipv4, "223.5.5.5") == 0,
                     "expected end-to-end UE DNS server");
}

void test_config_loads_b210_runtime_overrides(void) {
  char output_dir[MINI_GNB_C_MAX_PATH];
  char config_path[MINI_GNB_C_MAX_PATH];
  char error_message[256];
  mini_gnb_c_config_t config;
  FILE* file = NULL;

  mini_gnb_c_make_output_dir("config_b210_runtime", output_dir, sizeof(output_dir));
  mini_gnb_c_require(mini_gnb_c_join_path(output_dir, "runtime.yml", config_path, sizeof(config_path)) == 0,
                     "expected runtime config path");
  file = fopen(config_path, "wb");
  mini_gnb_c_require(file != NULL, "expected runtime config file to open");
  fprintf(file,
          "cell:\n"
          "  dl_arfcn: 627334\n"
          "  band: 78\n"
          "  channel_bandwidth_MHz: 20\n"
          "  common_scs_khz: 30\n"
          "  pci: 1\n"
          "  plmn: \"00101\"\n"
          "  tac: 7\n"
          "  ss0_index: 0\n"
          "  coreset0_index: 6\n"
          "\n"
          "prach:\n"
          "  prach_config_index: 159\n"
          "  prach_root_seq_index: 1\n"
          "  zero_correlation_zone: 8\n"
          "  ra_resp_window: 4\n"
          "  msg3_delta_preamble: 0\n"
          "\n"
          "rf:\n"
          "  device_driver: \"uhd-b210\"\n"
          "  device_args: \"serial=8000963\"\n"
          "  clock_src: \"external\"\n"
          "  runtime_mode: \"trx\"\n"
          "  srate: 23040000.0\n"
          "  rx_freq_hz: 2462000000.0\n"
          "  tx_freq_hz: 2461000000.0\n"
          "  rx_gain: 33.0\n"
          "  tx_gain: 44.0\n"
          "  bandwidth_hz: 20000000.0\n"
          "  duration_sec: 2.0\n"
          "  duration_mode: \"wallclock\"\n"
          "  channel: 1\n"
          "  channel_count: 2\n"
          "  rx_cpu_core: 2\n"
          "  tx_cpu_core: 3\n"
          "  apply_host_tuning: false\n"
          "  require_ref_lock: false\n"
          "  require_lo_lock: true\n"
          "  rx_output_file: \"out/runtime_rx.dat\"\n"
          "  tx_input_file: \"/dev/shm/runtime_tx.dat\"\n"
          "  rx_ring_map: \"/dev/shm/runtime_rx.map\"\n"
          "  tx_ring_map: \"/dev/shm/runtime_tx.map\"\n"
          "  ring_block_samples: 2048\n"
          "  ring_block_count: 12800\n"
          "  tx_prefetch_samples: 65536\n"
          "\n"
          "broadcast:\n"
          "  ssb_period_slots: 2\n"
          "  sib1_period_slots: 8\n"
          "  sib1_offset_slot: 4\n"
          "  prach_period_slots: 10\n"
          "  prach_offset_slot: 2\n"
          "\n"
          "sim:\n"
          "  total_slots: 20\n"
          "  slots_per_frame: 10\n"
          "  msg3_delay_slots: 4\n"
          "  msg4_delay_slots: 1\n"
          "  prach_trigger_abs_slot: 2\n"
          "  prach_retry_delay_slots: 4\n"
          "  preamble_id: 27\n"
          "  ta_est: 11\n"
          "  peak_metric: 18.5\n"
          "  msg3_present: true\n"
          "  msg3_crc_ok: true\n"
          "  msg3_snr_db: 18.2\n"
          "  msg3_evm: 2.1\n"
          "  ul_prach_cf32_path: \"\"\n"
          "  ul_msg3_cf32_path: \"\"\n"
          "  contention_id_hex: \"A1B2C3D4E5F6\"\n"
          "  establishment_cause: 3\n"
          "  ue_identity_type: 1\n"
          "  ue_identity_hex: \"1122334455667788\"\n"
          "  include_crnti_ce: true\n");
  fclose(file);

  mini_gnb_c_require(mini_gnb_c_load_config(config_path, &config, error_message, sizeof(error_message)) == 0,
                     "expected B210 runtime override config to load");
  mini_gnb_c_require(strcmp(config.rf.device_driver, "uhd-b210") == 0, "expected B210 driver override");
  mini_gnb_c_require(config.rf.runtime_mode == MINI_GNB_C_RF_RUNTIME_MODE_TRX, "expected TRX runtime mode");
  mini_gnb_c_require(config.rf.srate == 23040000.0, "expected RF sample rate override");
  mini_gnb_c_require(config.rf.rx_freq_hz == 2462000000.0, "expected RX frequency override");
  mini_gnb_c_require(config.rf.tx_freq_hz == 2461000000.0, "expected TX frequency override");
  mini_gnb_c_require(config.rf.rx_gain == 33.0, "expected RX gain override");
  mini_gnb_c_require(config.rf.tx_gain == 44.0, "expected TX gain override");
  mini_gnb_c_require(config.rf.bandwidth_hz == 20000000.0, "expected RF bandwidth override");
  mini_gnb_c_require(config.rf.duration_sec == 2.0, "expected RF duration override");
  mini_gnb_c_require(config.rf.duration_mode == MINI_GNB_C_RF_DURATION_MODE_WALLCLOCK,
                     "expected wallclock duration mode");
  mini_gnb_c_require(config.rf.channel == 1u, "expected RF channel override");
  mini_gnb_c_require(config.rf.channel_count == 2u, "expected dual-channel override");
  mini_gnb_c_require(config.rf.rx_cpu_core == 2, "expected RX CPU core override");
  mini_gnb_c_require(config.rf.tx_cpu_core == 3, "expected TX CPU core override");
  mini_gnb_c_require(!config.rf.apply_host_tuning, "expected host tuning override");
  mini_gnb_c_require(!config.rf.require_ref_lock, "expected ref-lock override");
  mini_gnb_c_require(config.rf.require_lo_lock, "expected LO-lock override");
  mini_gnb_c_require(strcmp(config.rf.rx_output_file, "out/runtime_rx.dat") == 0,
                     "expected RX output file override");
  mini_gnb_c_require(strcmp(config.rf.tx_input_file, "/dev/shm/runtime_tx.dat") == 0,
                     "expected TX input file override");
  mini_gnb_c_require(strcmp(config.rf.rx_ring_map, "/dev/shm/runtime_rx.map") == 0,
                     "expected RX ring override");
  mini_gnb_c_require(strcmp(config.rf.tx_ring_map, "/dev/shm/runtime_tx.map") == 0,
                     "expected TX ring override");
  mini_gnb_c_require(config.rf.ring_block_samples == 2048u, "expected ring block sample override");
  mini_gnb_c_require(config.rf.ring_block_count == 12800u, "expected ring block count override");
  mini_gnb_c_require(config.rf.tx_prefetch_samples == 65536u, "expected TX prefetch override");
  mini_gnb_c_require(mini_gnb_c_rf_app_runtime_requested(&config),
                     "expected non-simulator RF mode to request app runtime");
}
