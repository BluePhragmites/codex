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
- `tests/test_ngap_runtime.c`
- `tests/test_n3_user_plane.c`
- `tests/test_gtpu_tunnel.c`
- `tests/test_nas_5gs_min.c`
- `tests/test_shared_slot_link.c`
- `tests/test_mini_ue_runtime.c`
- `tests/test_rlc_lite.c`

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
| Configuration loading and basic parameters | `test_config_loads`, `test_open5gs_end_to_end_ue_config_loads_tun_internet_settings` | configuration files are parsed correctly |
| Common tables and helpers | `test_tbsize_lookup_table`, `test_shared_slot_link_round_trip`, `test_shared_slot_link_handles_slot_zero_and_shutdown_boundaries` | basic helpers and the shared-slot register path are stable |
| UE context and session state | `test_core_session_tracks_user_plane_state`, `test_ue_context_store_promote_initializes_core_session` | core UE and session state are maintained correctly |
| NGAP builders and parsers | `test_ngap_runtime_builders_encode_expected_headers`, `test_ngap_runtime_extracts_open5gs_user_plane_state` | minimal NGAP encoding and Open5GS session extraction work |
| gNB <-> AMF bridge | `test_gnb_core_bridge_prepares_initial_ue_message`, `test_gnb_core_bridge_relays_followup_uplink_nas`, `test_gnb_core_bridge_parses_session_setup_state`, `test_gnb_core_bridge_relays_post_session_downlink_nas` | the minimal control-plane bridge works |
| Minimal UE NAS helpers | `test_nas_5gs_min_builds_followup_uplinks`, `test_nas_5gs_min_polls_downlink_exchange` | the UE can generate happy-path follow-up NAS messages |
| GTP-U helpers and N3 | `test_gtpu_builders_encode_expected_headers`, `test_n3_user_plane_activates_and_sends_uplink_gpdu`, `test_n3_user_plane_polls_downlink_packet` | N3 encapsulation, send, and receive are functional |
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

### 4.2 `tests/test_rlc_lite.c`

This file validates:

- segment-header construction
- receiver-side reassembly
- out-of-order segment rejection

### 4.3 `tests/test_integration.c`

This file validates cross-module flows:

- the local shared-slot UE/gNB loop
- follow-up NAS over the bearer path
- `TUN -> N3`
- the gNB core bridge
- sustained uplink grants

If you need a quick answer to whether a change broke the main system flow, this is the first file to inspect.

## 5. Manual Open5GS Validation Flow

### 5.1 Check Minimal AMF Reachability First

```bash
./build/ngap_probe 127.0.0.5 38412 5000
```

### 5.2 Then Run the End-to-End UE/gNB Scenario

```bash
./build/mini_ue_c config/example_open5gs_end_to_end_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_open5gs_end_to_end_gnb.yml
wait $UE_PID
```

### 5.3 Validate Step by Step Inside the UE Namespace

Recommended order:

```bash
ip netns exec miniue-demo ping -c 4 8.8.8.8
ip netns exec miniue-demo ping -c 4 www.baidu.com
ip netns exec miniue-demo curl -I --max-time 25 http://www.baidu.com
ip netns exec miniue-demo curl --max-time 30 -sS www.baidu.com -o /tmp/miniue_curl_body.html
```

### 5.4 How to Interpret Failures

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

## 6. Runtime Artifacts Worth Inspecting

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

## 7. Maintenance Rule

This file should only track:

- the test layers
- the feature-to-test mapping
- manual validation commands
- runtime artifact inspection guidance

Do not turn it back into a roadmap document, an architecture narrative, or a build guide.
