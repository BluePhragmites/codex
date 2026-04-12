# gnb_c Test and Feature Validation Guide

This document answers testing questions only:

- which unit tests exist
- which integration tests exist
- which capabilities those tests prove
- how to run manual Open5GS validation

It does not try to describe the full roadmap or provide a broad architecture tutorial.

- Build and run entrypoints: `README.md`
- Feature status and roadmap: `implementation_plan.md`
- Architecture and code-reading path: `architecture.md`

## 1. Quick Start

### 1.1 Run the Full Automated Test Suite

```bash
cd gnb_c
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The automated suite is currently bundled into `mini_gnb_c_tests`, which is invoked through `ctest`.

### 1.2 Run the Standard Manual Open5GS Scenario

```bash
cd gnb_c
./build/mini_ue_c config/example_open5gs_end_to_end_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_open5gs_end_to_end_gnb.yml
wait $UE_PID
```

Then validate traffic from the UE namespace:

```bash
ip netns exec miniue-demo ping -c 4 8.8.8.8
ip netns exec miniue-demo ping -c 4 www.baidu.com
ip netns exec miniue-demo curl -I --max-time 25 http://www.baidu.com
ip netns exec miniue-demo curl --max-time 30 -sS www.baidu.com -o /tmp/miniue_curl_body.html
ip netns exec miniue-demo wc -c /tmp/miniue_curl_body.html
```

## 2. Test Layers

### 2.1 Unit Tests

Unit tests focus on local subsystem behavior.

Main sources include:

- `tests/test_config.c`
- `tests/test_core_session.c`
- `tests/test_air_pdu.c`
- `tests/test_ngap_runtime.c`
- `tests/test_n3_user_plane.c`
- `tests/test_gtpu_tunnel.c`
- `tests/test_nas_5gs_min.c`
- `tests/test_shared_slot_link.c`
- `tests/test_mini_ue_runtime.c`
- `tests/test_rlc_lite.c`
- `tests/test_radio_frontend.c`

### 2.2 Integration Tests

Integration tests focus on multi-module flows.

Main source:

- `tests/test_integration.c`

### 2.3 Manual System Validation

Manual validation covers the most realistic paths that are harder to model fully in automation, such as:

- Open5GS AMF and UPF interoperability
- TUN and network-namespace behavior
- DNS, NAT, and internet traffic
- Wireshark and pcap inspection

## 3. Feature-to-Test Mapping

| Feature | Representative tests | What it proves |
| --- | --- | --- |
| Configuration loading and basic parameters | `test_config_loads`, `test_open5gs_end_to_end_ue_config_loads_tun_internet_settings`, `test_config_loads_b210_runtime_overrides` | configuration files are parsed correctly, including the shared B210 app-harness RF fields |
| Common tables and helpers | `test_tbsize_lookup_table`, `test_shared_slot_link_round_trip`, `test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries` | basic helpers and the shared-slot register path are stable |
| UE context and session state | `test_core_session_tracks_user_plane_state`, `test_ue_context_store_promote_initializes_core_session` | core UE and session state are maintained correctly |
| NGAP builders and parsers | `test_ngap_runtime_builders_encode_expected_headers`, `test_ngap_runtime_extracts_open5gs_user_plane_state` | minimal NGAP encoding and Open5GS session extraction work |
| gNB <-> AMF bridge | `test_gnb_core_bridge_prepares_initial_ue_message`, `test_gnb_core_bridge_relays_followup_uplink_nas`, `test_gnb_core_bridge_parses_session_setup_state`, `test_gnb_core_bridge_relays_post_session_downlink_nas` | the minimal control-plane bridge works |
| Minimal UE NAS helpers | `test_nas_5gs_min_builds_followup_uplinks`, `test_nas_5gs_min_polls_downlink_exchange` | the UE can generate happy-path follow-up NAS messages |
| Stage 2A air-PDU contract | `test_air_pdu_build_and_parse_round_trip`, `test_air_pdu_rejects_crc_mismatch`, `test_air_pdu_rejects_invalid_header_fields` | the first shared over-the-air binary contract is stable enough to become the future gNB/UE IQ payload container |
| GTP-U helpers and N3 | `test_gtpu_builders_encode_expected_headers`, `test_n3_user_plane_activates_and_sends_uplink_gpdu`, `test_n3_user_plane_polls_downlink_packet` | N3 encapsulation, send, and receive are functional |
| Radio frontend boundary and Stage 1 target gating | `test_radio_frontend_initializes_mock_backend`, `test_radio_frontend_rejects_unsupported_backend_name`, `test_config_loads` | the radio boundary still cleanly initializes the mock path, rejects unsupported backends loudly, and keeps `uhd-b210` as the first explicit real-radio target |
| B210 smoke-probe defaults and host tuning | `test_b210_probe_config_defaults`, `test_b210_tx_config_defaults`, `test_b210_trx_config_defaults`, `test_b210_gain_helpers_apply_shared_and_directional_overrides`, `test_host_performance_plan_for_b210_skips_network_buffers`, `test_host_performance_plan_for_mock_is_not_applicable` | the hardware-smoke tool now covers RX, TX, and TRX defaults, separate RX/TX frequency and gain controls, and the B210-relevant host tuning profile without mixing in Ethernet-only steps |
| B210 app-harness configuration handoff | `test_config_loads_b210_runtime_overrides` | `mini_gnb_c_sim` and `mini_ue_c` can now consume the shared `rf:` section for B210 RX/TX/TRX harness runs |
| Single-file `sc16` ring-map layout and export | `test_sc16_ring_map_create_append_and_wrap`, `test_sc16_ring_map_dual_channel_payload_is_channel_major`, `test_sc16_ring_export_range_writes_per_channel_files` | the recent-history ring keeps metadata, wrap state, and pure IQ payload in one file, stores dual-channel payloads in channel-major order, and can export a selected seq range |
| UE runtime uplink queue | `test_mini_ue_runtime_uplink_queue_tracks_bytes_and_bsr_dirty`, `test_mini_ue_runtime_update_uplink_state_rearms_sr_after_grant_consumption`, `test_mini_ue_runtime_builds_bsr_from_current_queue_bytes` | the FIFO, BSR accounting, and SR state work |
| NAS and IPv4 payload kinds | `test_mini_ue_runtime_preserves_payload_kind_for_new_and_retx_grants`, `test_integration_shared_slot_ue_runtime_auto_nas_session_setup` | NAS and IPv4 can share the bearer path without confusion |
| RLC-lite segmentation and reassembly | `test_rlc_lite_builds_and_reassembles_segmented_sdu`, `test_rlc_lite_rejects_out_of_order_segment`, `test_mini_ue_runtime_segments_ipv4_payload_across_multiple_grants` | larger IPv4 SDUs can cross multiple grants |
| Local shared-slot UE/gNB loop | `test_integration_shared_slot_ue_runtime` | `PRACH -> RAR -> MSG3 -> MSG4 -> SR/BSR/UL DATA` works |
| Repeated SR and sustained UL grants | `test_integration_shared_slot_ue_runtime_repeats_sr_for_pending_uplink_queue`, `test_integration_shared_slot_ue_runtime_consumes_uplink_queue_in_order`, `test_integration_slot_text_transport_continues_connected_ul_grants` | sustained connected-mode uplink scheduling works |
| TUN -> N3 forwarding | `test_integration_shared_slot_tun_uplink_reaches_n3`, `test_integration_core_bridge_forwards_ul_ipv4_to_n3` | UE TUN packets reach N3 |
| Local slot text transport | `test_integration_slot_text_transport` | the text-input mock UL scenario still works |

## 4. Key Test Files

### 4.1 `tests/test_mini_ue_runtime.c`

This file mainly validates UE runtime local behavior:

- whether the UL FIFO behaves correctly
- whether `BSR` bytes reflect the real queue
- whether `SR` is re-armed on later occasions
- whether `payload_kind` is preserved
- whether a large IPv4 SDU is segmented across multiple grants

### 4.2 `tests/test_air_pdu.c`

This file validates the Stage 2A binary contract foundation:

- air-PDU header layout
- round-trip encode and parse
- CRC mismatch rejection
- header field validation

### 4.3 `tests/test_rlc_lite.c`

This file validates:

- segment-header construction
- receiver-side reassembly
- out-of-order segment rejection

### 4.4 `tests/test_integration.c`

This file validates cross-module flows:

- the local shared-slot UE/gNB loop
- follow-up NAS over the bearer path
- `TUN -> N3`
- the gNB core bridge
- sustained uplink grants

If you need a quick answer to whether a change broke the main system flow, this is the first file to inspect.

### 4.5 `tests/test_radio_frontend.c`

This file validates the first Stage 1 radio-foundation behavior:

- the current `mock` backend still initializes and runs
- unsupported real-radio configuration fails loudly instead of producing a false-positive simulation run

### 4.6 `tests/test_b210_probe.c`

This file validates the B210 smoke-probe defaults that do not require hardware during `ctest`:

- the default UHD probe profile is pinned to the current Stage 1 bring-up target
- the default output path uses `/dev/shm` to reduce RX overflow risk during high-rate capture
- the default TX input path matches the existing UHD smoke workflow
- the default host-tuning behavior is enabled for both RX and TX smoke paths
- CPU affinity defaults are explicit instead of being left ambiguous
- the TRX defaults explicitly separate `rx_cpu_core` and `tx_cpu_core`

### 4.7 `tests/test_sc16_ring_map.c`

This file validates the single-file recent-history ring:

- ring creation and reopen
- append semantics
- wrap-around behavior
- `oldest_valid_seq`, `next_write_seq`, and `last_committed_seq`
- payload lookup for the latest valid sequence
- dual-channel channel-major payload layout

### 4.8 `tests/test_sc16_ring_export.c`

This file validates the seq-range export path:

- inclusive `seq_start` / `seq_end` handling
- one output file per channel
- exported per-channel sample count
- metadata text generation

## 5. Manual B210 Validation Flow

### 5.1 Run the Simulator-Integrated B210 Slot Backend

The first simulator-facing B210 path now lives under `radio_frontend` itself.

Run the dedicated hybrid example:

```bash
cd gnb_c
sudo ./build/mini_gnb_c_sim config/example_b210_slot_backend_gnb.yml
./build/mini_ring_inspect --show-blocks 2 /dev/shm/mini_gnb_c_sim_slot_backend_rx_ring.map
./build/mini_ring_inspect --show-blocks 2 /dev/shm/mini_gnb_c_sim_slot_backend_tx_ring.map
```

Expected result:

- `mini_gnb_c_sim` reaches the normal simulator summary instead of switching into `mode=rx|tx|trx`
- `/dev/shm/mini_gnb_c_sim_slot_backend_rx_ring.map` exists and reports `role=rx`
- `/dev/shm/mini_gnb_c_sim_slot_backend_tx_ring.map` exists and reports `role=tx`
- the RX ring shows `time_source=uhd_hw`
- the TX ring advances one block per finalized simulator slot

Boundary of this validation:

- it proves the B210 runtime now sits behind `radio_frontend`
- it does not prove real UL burst decoding from B210 IQ
- it still uses the existing mock-radio slot semantics for `PRACH`, `MSG3`, `SR`, and `UL DATA`

### 5.2 Run the Integrated gNB and UE B210 App Harnesses

The main app entrypoints now have a hardware-harness mode driven by YAML `rf.runtime_mode`.

Example gNB-side capture:

```bash
cd gnb_c
sudo ./build/mini_gnb_c_sim config/example_b210_runtime_gnb.yml
./build/mini_ring_inspect --show-blocks 2 /dev/shm/mini_gnb_c_sim_rx_ring.map
```

Expected result:

- `mini_gnb_c_sim` prints `mode=rx`
- the summary shows `output_kind=ring-map(sc16)`
- `/dev/shm/mini_gnb_c_sim_rx_ring.map` exists
- `mini_ring_inspect` reports `channel_count=2`
- the latest descriptors show `time_source=uhd_hw`

Example UE-side replay from that captured ring:

```bash
cd gnb_c
sudo ./build/mini_ue_c config/example_b210_runtime_ue.yml
```

Expected result:

- `mini_ue_c` prints `mode=tx`
- the summary shows `input_kind=ring-map(sc16)`
- `transmitted_samples` is non-zero
- `underflow_observed=false`

Boundary of this validation:

- it proves the main app entrypoints can reuse the Stage 1 B210 runtime directly through shared YAML
- it does not yet prove a slot-aware real-air-interface gNB/UE attach over B210
- in `trx` mode, keep `rf.tx_ring_map` and `rf.rx_ring_map` on different files during one run

### 5.3 Run the Repo-Local B210 RX Smoke Probe

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --config config/default_cell.yml \
  --args "serial=8000963" \
  --subdev "A:A" \
  --ref external \
  --rate 20000000 \
  --freq 2462000000 \
  --gain 60 \
  --bw 20000000 \
  --duration 1 \
  --channel 0 \
  --cpu-core 2
```

Expected result:

- the tool prints `config=config/default_cell.yml` when `--config` is used
- the tool prints a host-tuning summary such as `cpu_governor=applied kms_poll=applied net_buffers=not_applicable`
- the tool prints `ref_locked=true`
- the tool prints `lo_locked=true`
- `received_samples` matches `requested_samples`
- `/dev/shm/b210_probe_rx_fc32.dat` is created

Optional ring-map variant:

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --args "serial=8000963" \
  --subdev "A:A" \
  --ref external \
  --rate 20000000 \
  --freq 2462000000 \
  --gain 60 \
  --bw 20000000 \
  --duration 0.05 \
  --channel 0 \
  --cpu-core 2 \
  --ring-map /dev/shm/rx_ring.map \
  --ring-block-samples 4096 \
  --ring-block-count 256
```

Expected result for the ring-map variant:

- the tool prints `output_kind=ring-map(sc16)`
- `ring_blocks` is non-zero
- `/dev/shm/rx_ring.map` is created
- `./build/mini_ring_inspect /dev/shm/rx_ring.map` prints a valid sequence window
- recent descriptors show `time_source=uhd_hw` when UHD RX metadata time is available

Interpretation rule for `duration` versus ring size:

- `--duration` controls how much data the current run tries to capture
- `--ring-block-samples` and `--ring-block-count` control how much recent history remains in the fixed-size ring
- `--duration-mode samples` stops on successful-sample count
- `--duration-mode wallclock` stops on elapsed wall time instead

Troubleshooting rule for this step:

- if you see `UHD RX overflow (0x8)`, keep the output on `/dev/shm` and pin the RX capture thread with `--cpu-core`
- use `--skip-host-tuning` only when you want an explicit before/after comparison

### 5.4 Run the Repo-Local B210 TX Smoke Probe

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --mode tx \
  --args "serial=8000963" \
  --subdev "A:A" \
  --ref external \
  --rate 20000000 \
  --freq 2462000000 \
  --gain 60 \
  --bw 20000000 \
  --channel 0 \
  --cpu-core 3 \
  --file /dev/shm/test.dat
```

Expected result:

- the tool prints a host-tuning summary such as `cpu_governor=applied kms_poll=applied net_buffers=not_applicable`
- the tool prints `ref_locked=true`
- the tool prints `lo_locked=true`
- `transmitted_samples` matches `requested_samples`
- `burst_ack=true` is preferred, but its absence alone does not invalidate the host-side smoke test

Important interpretation rule for this step:

- if `underflow_observed=true`, treat TX device access as proven but treat stable real-time TX as still incomplete
- the current standalone TX path already uses a larger user-space prefetch window for ring-map replay and refills it when the buffered sample count falls below a low-watermark threshold
- `--tx-prefetch-samples` can override that window when you want to tune replay behavior on a specific host
- the `trx` TX worker uses the same rule and the same override parameter
- the shared YAML/RF layer now also exposes this knob as `rf.tx_prefetch_samples`, even though the current runnable backend is still `mock`
- `--config <yaml>` now loads shared `rf` defaults first, then command-line options override them
- `--config` currently covers shared RF defaults such as rate, frequencies, gains, clock source, CPU cores, and `rf.tx_prefetch_samples`
- probe-local file paths and ring-map paths still stay on the command line
- if `underflow_observed=true` even after that, the next fix is not more YAML tweaking; it is a deeper runtime queue between the future simulator backend and the TX worker
- on this host, applying the built-in tuning plus the current TX prefetch path removed the `20 Msps` TX `underflow` seen in the untuned path

Optional ring replay variant:

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --mode tx \
  --args "serial=8000963" \
  --subdev "A:A" \
  --ref external \
  --rate 20000000 \
  --freq 2462000000 \
  --gain 60 \
  --bw 20000000 \
  --channel 0 \
  --cpu-core 3 \
  --ring-map /dev/shm/rx_ring.map
```

Expected result for the ring replay variant:

- the tool prints `input_kind=ring-map(sc16)`
- `ring_blocks` is non-zero
- `transmitted_samples` matches the sum of ready ring blocks
- `burst_ack=true` is preferred

### 5.5 Run the Repo-Local B210 TRX Smoke Probe

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --mode trx \
  --args "serial=8000963" \
  --subdev "A:A A:B" \
  --ref external \
  --rate 5000000 \
  --rx-freq 2462000000 \
  --tx-freq 2461000000 \
  --gain 50 \
  --bw 5000000 \
  --duration 0.02 \
  --channel 0 \
  --channel-count 2 \
  --rx-cpu-core 2 \
  --tx-cpu-core 3 \
  --rx-ring-map /dev/shm/trx2_rx_ring.map \
  --tx-ring-map /dev/shm/rx2_ring.map \
  --ring-block-samples 2048 \
  --ring-block-count 128
```

Expected result:

- the tool prints `mode=trx`
- the tool prints `channel_count=2`
- `requested_samples`, `received_samples`, and `transmitted_samples` match
- the tool prints `rx_recoverable_events=...`, `rx_overflow_events=...`, and `rx_timeout_events=...`
- the tool prints `rx_gap_events=...` and `rx_lost_samples_estimate=...`
- `rx_ring_blocks` and `tx_ring_blocks` are non-zero
- the tool prints `tx_prefetch_samples=...`
- the tool prints both `actual_rx_freq_hz=...` and `actual_tx_freq_hz=...`
- `burst_ack=true` is preferred
- `underflow_observed=false` is preferred
- `./build/mini_ring_inspect /dev/shm/trx2_rx_ring.map` shows `channel_count=2`

Stress interpretation rule for this step:

- `--duration` now controls the target TRX run length on both RX and TX
- if the TX source ring is shorter than `rate * duration`, the TX side loops the ring automatically
- `tx_ring_wrap_count > 0` confirms that the run actually went past one source-ring pass
- RX `OVERFLOW` and `TIMEOUT` are now treated as recoverable stress events
- `rx_lost_samples_estimate > 0` means hardware-time gaps were observed between successful RX chunks
- broken chains, bad packets, alignment failures, and stream/API failures are still fatal

### 5.6 Export a Seq Range After Capture

If `mini_ring_inspect` shows a valid range such as:

- `seq=123941`
- `seq=123942`
- `seq=123943`
- `seq=123944`

then export that inclusive range like this:

```bash
cd gnb_c
./build/mini_ring_export \
  --seq-start 123941 \
  --seq-end 123944 \
  --output-prefix out/ring_exports/rx2_slice \
  /dev/shm/rx2_ring.map
```

Expected result:

- the tool prints `blocks_exported=4`
- the tool writes `..._ch0.sc16`
- the tool writes `..._ch1.sc16` when `channel_count=2`
- the tool writes `..._meta.txt`

### 5.7 Directional Gain Controls in TRX Mode

The B210 probe keeps `--gain` as a shared shortcut, but `trx` mode also supports:

- `--rx-gain <db>`
- `--tx-gain <db>`

Use them when RX and TX should not share the same value:

```bash
cd gnb_c
./build/mini_b210_probe_c --help
```

Expected result:

- the help text lists `--rx-gain`
- the help text lists `--tx-gain`
- the runtime summary in `trx` mode reports both `actual_rx_gain_db` and `actual_tx_gain_db`

## 6. Manual Open5GS Validation Flow

### 6.1 Check Minimal AMF Reachability First

```bash
./build/ngap_probe 127.0.0.5 38412 5000
```

### 6.2 Then Run the End-to-End UE/gNB Scenario

```bash
./build/mini_ue_c config/example_open5gs_end_to_end_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_open5gs_end_to_end_gnb.yml
wait $UE_PID
```

### 6.3 Validate Step by Step Inside the UE Namespace

Recommended order:

```bash
ip netns exec miniue-demo ping -c 4 8.8.8.8
ip netns exec miniue-demo ping -c 4 www.baidu.com
ip netns exec miniue-demo curl -I --max-time 25 http://www.baidu.com
ip netns exec miniue-demo curl --max-time 30 -sS www.baidu.com -o /tmp/miniue_curl_body.html
```

### 6.4 How to Interpret Failures

- If `ping 8.8.8.8` fails:
  - check the default route, N3, and host NAT first
- If `ping 8.8.8.8` works but `ping www.baidu.com` fails:
  - check DNS first
- If both `ping` commands work but `curl` fails:
  - focus on the large-payload bearer path
  - look for log markers such as:
    - `Queued one DL IPv4 RLC-lite segment`
    - `Buffered one partial UL IPv4 RLC-lite segment`
    - `Reassembled one UL IPv4 SDU`

## 7. Runtime Artifacts Worth Inspecting

Recommended first checks:

- `out/summary.json`
- `out/tx/`
- `out/rx/`
- `out/gnb_core_ngap_runtime.pcap`
- `out/gnb_core_gtpu_runtime.pcap`

A common troubleshooting sequence is:

1. inspect whether the session was established in `out/summary.json`
2. inspect whether the NGAP pcap reached `PDUSessionResourceSetup`
3. inspect whether the GTP-U pcap shows bidirectional traffic
4. inspect whether `out/tx/` and `out/rx/` contain the expected `payload_kind`

For the current Stage 1 radio-foundation work, an additional rule applies:

- if you switch `rf.device_driver` away from `mock`, the current expected result is an explicit startup failure until the B210/UHD backend is actually implemented
- the current hardware validation path for Stage 1 is `mini_b210_probe_c`, not `mini_gnb_c_sim`

## 8. Maintenance Rule

This file should only track:

- the test layers
- the feature-to-test mapping
- manual validation commands
- runtime artifact inspection guidance

Do not turn it back into a roadmap document, an architecture narrative, or a build guide.
