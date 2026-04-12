#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/radio/b210_uhd_probe.h"

void test_b210_probe_config_defaults(void) {
  mini_gnb_c_b210_probe_config_t config;

  memset(&config, 0, sizeof(config));
  mini_gnb_c_b210_probe_config_init(&config);

  mini_gnb_c_require(config.device_args[0] == '\0', "expected empty device args by default");
  mini_gnb_c_require(config.subdev[0] == '\0', "expected empty subdevice by default");
  mini_gnb_c_require(strcmp(config.ref, "external") == 0, "expected external reference by default");
  mini_gnb_c_require(config.rate_sps == 20e6, "expected 20 Msps default probe rate");
  mini_gnb_c_require(config.freq_hz == 2462e6, "expected 2462 MHz default probe frequency");
  mini_gnb_c_require(config.gain_db == 60.0, "expected 60 dB default probe gain");
  mini_gnb_c_require(config.bandwidth_hz == 20e6, "expected 20 MHz default probe bandwidth");
  mini_gnb_c_require(config.duration_sec == 1.0, "expected 1 second default probe duration");
  mini_gnb_c_require(config.duration_mode == MINI_GNB_C_B210_DURATION_MODE_SAMPLE_TARGET,
                     "expected RX duration mode to default to sample-target semantics");
  mini_gnb_c_require(config.channel == 0u, "expected channel 0 by default");
  mini_gnb_c_require(config.channel_count == 1u, "expected single-channel RX probe by default");
  mini_gnb_c_require(config.cpu_core == -1, "expected CPU affinity disabled by default");
  mini_gnb_c_require(config.apply_host_tuning, "expected host tuning enabled by default for RX");
  mini_gnb_c_require(config.require_ref_lock, "expected reference lock required by default");
  mini_gnb_c_require(config.require_lo_lock, "expected LO lock required by default");
  mini_gnb_c_require(strcmp(config.output_path, "/dev/shm/b210_probe_rx_fc32.dat") == 0,
                     "expected tmpfs default output path");
  mini_gnb_c_require(config.ring_path[0] == '\0', "expected ring map RX path disabled by default");
  mini_gnb_c_require(config.ring_block_samples == 4096u, "expected default RX ring block size");
  mini_gnb_c_require(config.ring_block_count == 1024u, "expected default RX ring block count");
}

void test_b210_tx_config_defaults(void) {
  mini_gnb_c_b210_tx_config_t config;

  memset(&config, 0, sizeof(config));
  mini_gnb_c_b210_tx_config_init(&config);

  mini_gnb_c_require(config.device_args[0] == '\0', "expected empty TX device args by default");
  mini_gnb_c_require(config.subdev[0] == '\0', "expected empty TX subdevice by default");
  mini_gnb_c_require(strcmp(config.ref, "external") == 0, "expected external TX reference by default");
  mini_gnb_c_require(config.rate_sps == 20e6, "expected 20 Msps default TX rate");
  mini_gnb_c_require(config.freq_hz == 2462e6, "expected 2462 MHz default TX frequency");
  mini_gnb_c_require(config.gain_db == 60.0, "expected 60 dB default TX gain");
  mini_gnb_c_require(config.bandwidth_hz == 20e6, "expected 20 MHz default TX bandwidth");
  mini_gnb_c_require(config.channel == 0u, "expected TX channel 0 by default");
  mini_gnb_c_require(config.channel_count == 1u, "expected single-channel TX probe by default");
  mini_gnb_c_require(config.cpu_core == -1, "expected TX CPU affinity disabled by default");
  mini_gnb_c_require(config.apply_host_tuning, "expected host tuning enabled by default for TX");
  mini_gnb_c_require(config.require_ref_lock, "expected TX reference lock required by default");
  mini_gnb_c_require(config.require_lo_lock, "expected TX LO lock required by default");
  mini_gnb_c_require(strcmp(config.input_path, "/dev/shm/test.dat") == 0, "expected default TX input path");
  mini_gnb_c_require(config.ring_path[0] == '\0', "expected ring map TX path disabled by default");
  mini_gnb_c_require(config.ring_block_samples == 4096u, "expected default TX ring block size");
  mini_gnb_c_require(config.ring_block_count == 1024u, "expected default TX ring block count");
  mini_gnb_c_require(config.tx_prefetch_samples == 0u, "expected TX prefetch window to default to auto sizing");
}

void test_b210_trx_config_defaults(void) {
  mini_gnb_c_b210_trx_config_t config;

  memset(&config, 0, sizeof(config));
  mini_gnb_c_b210_trx_config_init(&config);

  mini_gnb_c_require(config.device_args[0] == '\0', "expected empty TRX device args by default");
  mini_gnb_c_require(config.subdev[0] == '\0', "expected empty TRX subdevice by default");
  mini_gnb_c_require(strcmp(config.ref, "external") == 0, "expected external TRX reference by default");
  mini_gnb_c_require(config.rate_sps == 20e6, "expected 20 Msps default TRX rate");
  mini_gnb_c_require(config.rx_freq_hz == 2462e6, "expected 2462 MHz default TRX RX frequency");
  mini_gnb_c_require(config.tx_freq_hz == 2462e6, "expected 2462 MHz default TRX TX frequency");
  mini_gnb_c_require(config.rx_gain_db == 60.0, "expected 60 dB default TRX RX gain");
  mini_gnb_c_require(config.tx_gain_db == 60.0, "expected 60 dB default TRX TX gain");
  mini_gnb_c_require(config.bandwidth_hz == 20e6, "expected 20 MHz default TRX bandwidth");
  mini_gnb_c_require(config.duration_sec == 1.0, "expected 1 second default TRX duration");
  mini_gnb_c_require(config.duration_mode == MINI_GNB_C_B210_DURATION_MODE_SAMPLE_TARGET,
                     "expected TRX duration mode to default to sample-target semantics");
  mini_gnb_c_require(config.channel == 0u, "expected TRX channel 0 by default");
  mini_gnb_c_require(config.channel_count == 1u, "expected single-channel TRX by default");
  mini_gnb_c_require(config.rx_cpu_core == -1, "expected TRX RX CPU affinity disabled by default");
  mini_gnb_c_require(config.tx_cpu_core == -1, "expected TRX TX CPU affinity disabled by default");
  mini_gnb_c_require(config.apply_host_tuning, "expected host tuning enabled by default for TRX");
  mini_gnb_c_require(config.require_ref_lock, "expected TRX reference lock required by default");
  mini_gnb_c_require(config.require_lo_lock, "expected TRX LO lock required by default");
  mini_gnb_c_require(strcmp(config.rx_output_path, "/dev/shm/b210_trx_rx_fc32.dat") == 0,
                     "expected default TRX RX output path");
  mini_gnb_c_require(strcmp(config.tx_input_path, "/dev/shm/test.dat") == 0, "expected default TRX TX input path");
  mini_gnb_c_require(config.rx_ring_path[0] == '\0', "expected TRX RX ring path disabled by default");
  mini_gnb_c_require(config.tx_ring_path[0] == '\0', "expected TRX TX ring path disabled by default");
  mini_gnb_c_require(config.ring_block_samples == 4096u, "expected default TRX ring block size");
  mini_gnb_c_require(config.ring_block_count == 1024u, "expected default TRX ring block count");
  mini_gnb_c_require(config.tx_prefetch_samples == 0u, "expected TRX TX prefetch window to default to auto sizing");
}

void test_b210_gain_helpers_apply_shared_and_directional_overrides(void) {
  mini_gnb_c_b210_probe_config_t rx_config;
  mini_gnb_c_b210_tx_config_t tx_config;
  mini_gnb_c_b210_trx_config_t trx_config;

  memset(&rx_config, 0, sizeof(rx_config));
  memset(&tx_config, 0, sizeof(tx_config));
  memset(&trx_config, 0, sizeof(trx_config));
  mini_gnb_c_b210_probe_config_init(&rx_config);
  mini_gnb_c_b210_tx_config_init(&tx_config);
  mini_gnb_c_b210_trx_config_init(&trx_config);

  mini_gnb_c_b210_apply_shared_gain(&rx_config, &tx_config, &trx_config, 50.0);
  mini_gnb_c_require(rx_config.gain_db == 50.0, "expected shared gain to update RX mode config");
  mini_gnb_c_require(tx_config.gain_db == 50.0, "expected shared gain to update TX mode config");
  mini_gnb_c_require(trx_config.rx_gain_db == 50.0, "expected shared gain to update TRX RX gain");
  mini_gnb_c_require(trx_config.tx_gain_db == 50.0, "expected shared gain to update TRX TX gain");

  mini_gnb_c_b210_apply_rx_gain(&rx_config, &trx_config, 37.5);
  mini_gnb_c_require(rx_config.gain_db == 37.5, "expected RX override to update RX mode config");
  mini_gnb_c_require(tx_config.gain_db == 50.0, "expected RX override to leave TX mode config unchanged");
  mini_gnb_c_require(trx_config.rx_gain_db == 37.5, "expected RX override to update TRX RX gain");
  mini_gnb_c_require(trx_config.tx_gain_db == 50.0, "expected RX override to leave TRX TX gain unchanged");

  mini_gnb_c_b210_apply_tx_gain(&tx_config, &trx_config, 12.0);
  mini_gnb_c_require(rx_config.gain_db == 37.5, "expected TX override to leave RX mode config unchanged");
  mini_gnb_c_require(tx_config.gain_db == 12.0, "expected TX override to update TX mode config");
  mini_gnb_c_require(trx_config.rx_gain_db == 37.5, "expected TX override to leave TRX RX gain unchanged");
  mini_gnb_c_require(trx_config.tx_gain_db == 12.0, "expected TX override to update TRX TX gain");

  mini_gnb_c_b210_apply_shared_freq(&rx_config, &tx_config, &trx_config, 2462e6);
  mini_gnb_c_require(rx_config.freq_hz == 2462e6, "expected shared frequency to update RX mode config");
  mini_gnb_c_require(tx_config.freq_hz == 2462e6, "expected shared frequency to update TX mode config");
  mini_gnb_c_require(trx_config.rx_freq_hz == 2462e6, "expected shared frequency to update TRX RX frequency");
  mini_gnb_c_require(trx_config.tx_freq_hz == 2462e6, "expected shared frequency to update TRX TX frequency");

  mini_gnb_c_b210_apply_rx_freq(&rx_config, &trx_config, 2450e6);
  mini_gnb_c_require(rx_config.freq_hz == 2450e6, "expected RX frequency override to update RX mode config");
  mini_gnb_c_require(tx_config.freq_hz == 2462e6, "expected RX frequency override to leave TX mode config unchanged");
  mini_gnb_c_require(trx_config.rx_freq_hz == 2450e6, "expected RX frequency override to update TRX RX frequency");
  mini_gnb_c_require(trx_config.tx_freq_hz == 2462e6, "expected RX frequency override to leave TRX TX frequency unchanged");

  mini_gnb_c_b210_apply_tx_freq(&tx_config, &trx_config, 2470e6);
  mini_gnb_c_require(rx_config.freq_hz == 2450e6, "expected TX frequency override to leave RX mode config unchanged");
  mini_gnb_c_require(tx_config.freq_hz == 2470e6, "expected TX frequency override to update TX mode config");
  mini_gnb_c_require(trx_config.rx_freq_hz == 2450e6, "expected TX frequency override to leave TRX RX frequency unchanged");
  mini_gnb_c_require(trx_config.tx_freq_hz == 2470e6, "expected TX frequency override to update TRX TX frequency");
}

void test_b210_time_helpers_convert_hardware_time_and_sample_offsets(void) {
  const uint64_t start_ns = mini_gnb_c_b210_time_spec_to_ns(4154, 0.590250482);
  const uint64_t advanced_ns = mini_gnb_c_b210_advance_time_ns(start_ns, 2048u, 30720000.0);

  mini_gnb_c_require(start_ns == 4154590250482ULL, "expected exact UHD time-spec conversion");
  mini_gnb_c_require(advanced_ns == 4154590317149ULL, "expected sample offset to advance hardware time");
}
