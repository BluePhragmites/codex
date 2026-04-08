# mini-gnb-c

`mini-gnb-c` is a C rewrite of the minimal gNB prototype in this repository. It keeps the same target shape as the C++ version:

- single process
- single cell
- single UE
- mock PHY/RF
- Msg1 through Msg4 simulated initial access closure

The C version intentionally uses fixed-size buffers and plain C interfaces so it can serve as a simpler bring-up baseline.

## Layout

- `config/`: static YAML configuration
- `apps/`: executable entrypoints such as `mini_gnb_c_sim` and `ngap_probe`
- `include/mini_gnb_c/`: public C headers by subsystem
- `src/`: implementation modules
- `tests/`: unit and integration tests
- `examples/`: scripted slot inputs plus reference capture assets such as `examples/gnb_ngap.pcap` and `examples/gnb_mac.pcap`
- `implementation_plan.md`: tracked end-to-end UE/gNB/core delivery plan with completed and pending tasks
- `feature_test_guide.md`: current implemented capabilities and runnable test commands for the UE/gNB/core path

## Build In WSL Ubuntu

```bash
cd /mnt/d/work/codex/gnb_c
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_gnb_c_sim
```

For a minimal N2 reachability check against Open5GS AMF over SCTP:

```bash
./build/ngap_probe 192.168.1.10 38412 5000
```

`ngap_probe` sends one captured `NGSetupRequest` payload with `NGAP PPID=60` and
waits for a single response. A successful probe prints `NGSetupResponse detected.`

For a deeper Open5GS validation against the local AMF and UPF, use replay mode:

```bash
./build/ngap_probe --replay --upf-ip 127.0.0.7 --upf-port 2152 127.0.0.5 38412 5000
```

In replay mode, `ngap_probe` follows the same N2 attach/session step order as
`examples/gnb_ngap.pcap`, but the gNB-originated uplink NGAP messages are built locally
instead of copied from the capture:

- `InitialUEMessage`, all `UplinkNASTransport` messages, `InitialContextSetupResponse`,
  and `PDUSessionResourceSetupResponse` are encoded from NGAP IEs at runtime
- `AuthenticationResponse` is generated from the runtime `RAND/AUTN`
- `SecurityModeComplete`, `RegistrationComplete`, and the PDU session NAS uplinks
  have their NAS MAC recomputed from the runtime `KAMF/KNAS`
- `examples/gnb_ngap.pcap` is now only a reference capture for step alignment and later
  Wireshark comparison; replay still runs if that reference file is absent
- top-level `NAS-PDU` and `PDUSessionResourceSetupRequest` session data are parsed
  through bounded IE decoding instead of whole-frame byte-pattern scans
- after `PDUSessionResourceSetupRequest`, the probe extracts the session N3 tunnel
  parameters from the AMF response, including the UPF IP, UL TEID, QFI, and UE IPv4
- after `PDUSessionResourceSetupResponse`, the probe sends a GTP-U `Echo Request`
  to the configured or parsed UPF and then sends one minimal G-PDU using the parsed
  TEID, QFI, and UE IPv4
- `--gpdu-dst-ip <ipv4>` can be used to change the inner IPv4 destination carried
  inside that session G-PDU; the default is `10.45.0.1`
- by default, the probe also writes runtime exchange traces to
  `out/ngap_probe_ngap_runtime.pcap` and `out/ngap_probe_gtpu_runtime.pcap`
- `--ngap-trace-pcap <path>` and `--gtpu-trace-pcap <path>` can override those outputs
- the checked-in reference captures now live under `examples/`, with `examples/gnb_ngap.pcap`
  for N2 replay alignment and `examples/gnb_mac.pcap` for later MAC/Wireshark inspection

The replay code is also now starting to split into reusable library modules for the
later UE/gNB/core integration work:

- `include/mini_gnb_c/core/core_session.h`
  - stores single-UE AMF/session/N3 state that will later be shared by the simulator bridge
- `include/mini_gnb_c/ngap/ngap_runtime.h`
  - builds the reusable `NGSetupRequest`, `InitialUEMessage`, `UplinkNASTransport`, `DownlinkNASTransport`, `InitialContextSetupResponse`, and `PDUSessionResourceSetupResponse` payloads
  - parses Open5GS-facing NGAP session state such as `AMF UE NGAP ID`, `NAS-PDU`, `UPF tunnel`, `QFI`, and `UE IPv4`
- `include/mini_gnb_c/ngap/ngap_transport.h`
  - owns the reusable SCTP transport wrapper used by the simulator-side bridge and future live AMF integration
  - supports injected transport ops so unit and integration tests can validate the bridge without a live AMF listener
- `include/mini_gnb_c/n3/gtpu_tunnel.h`
  - builds and validates the minimal GTP-U Echo and UL G-PDU packets used by replay mode
- `include/mini_gnb_c/n3/n3_user_plane.h`
  - owns the persistent simulator-side N3 user-plane socket
  - activates a long-lived UDP GTP-U endpoint once `core_session` contains `UPF IP`, `TEID`, and `QFI`
  - reuses the extracted `gtpu_tunnel` helpers for runtime G-PDU encapsulation and one-packet-per-slot downlink polling
- the current extraction is covered by unit tests in `tests/test_core_session.c` and
  `tests/test_gtpu_tunnel.c`

`apps/ngap_probe.c` now uses that extracted NGAP runtime module instead of keeping
those message builders and Open5GS session parsers inline. This is the first real
Stage C reuse point that the future gNB-to-core bridge will share.

For the local UE/gNB loop, the repository now has two separate exchange helpers with
different roles:

- `include/mini_gnb_c/link/json_link.h`
  - emits atomic event files with a stable `seq_<nnnnnn>_<source>_<type>.json` naming pattern
  - writes through `tmp + rename` so a local UE process and the simulator can exchange
    control-plane NAS events through the filesystem without partially written files
- `include/mini_gnb_c/link/shared_slot_link.h`
  - exposes a shared-memory-backed slot register window for the live UE/gNB radio loop
  - the gNB publishes one slot summary and advances `txSlot`
  - the UE observes that slot, optionally schedules one future UL burst, then advances `rxSlot`
  - this is now the primary local UE/gNB interaction path because it keeps both processes
    slot-synchronous instead of pre-writing a whole UE event plan to disk
  - it also lets the gNB and UE use separate YAML files while still coupling only through
    downlink and uplink transport state
  - once session setup has produced a `UE IPv4`, later slot summaries also carry that
    address so the live UE can configure its optional TUN path without reading
    control-plane JSON directly

The UE-side process now has both a reusable event template generator and a live runtime:

- `apps/mini_ue_c.c`
  - loads the existing YAML config
  - when `sim.shared_slot_path` is configured, attaches to the live shared-slot register
    window and runs slot-by-slot with the gNB
  - otherwise, can still emit the older deterministic single-UE JSON event plan into
    `sim.local_exchange_dir/ue_to_gnb/` for regression coverage
- `include/mini_gnb_c/ue/mini_ue_fsm.h`
  - exposes the reusable UE event generator used by `mini_ue_c`
  - currently emits `PRACH`, `MSG3`, `PUCCH_SR`, `BSR`, and `DATA` events using the same timing assumptions as the mock gNB
- `include/mini_gnb_c/ue/mini_ue_runtime.h`
  - runs the live shared-slot UE process
  - waits for each published gNB slot before scheduling the next due UE burst
  - learns the PRACH retry policy and occasion timing from the gNB's SIB1 payload
  - learns the gNB-authored PDCCH timing and HARQ defaults from the same SIB1 payload
  - learns the post-attach SR periodicity and offset from the gNB's Msg4 / `RRCSetup` payload
  - uses the RAR payload to derive the exact Msg3 absolute slot instead of relying on local timing guesses
  - polls `gnb_to_ue/*.json` for later `DL_NAS` messages and feeds them into the minimal UE NAS helper
  - now keeps a small FIFO of pending uplink payloads instead of a single pending packet
  - tracks UE-side uplink demand with `sr_pending`, `bsr_dirty`, and the last reported BSR byte count
  - re-triggers `PUCCH_SR` on later valid SR occasions whenever that queue is still non-empty and no future UL grant is already available
  - consumes queued UL payload grants in FIFO order while still keeping the last HARQ payload for retransmission reuse
  - now also inspects `DL_OBJ_DATA` payloads and, when they carry a minimal IPv4 `ICMP Echo Request`, stages a matching `Echo Reply` for the next granted UL data transmission
  - when `sim.ue_tun_enabled=true`, configures a local TUN interface from the downlink-learned
    `UE IPv4`, injects later downlink IP packets into that TUN device, and prefers packets
    read back from the TUN device over the fallback synthetic `ue_ip_stack_min` reply path
  - handles the slot-0 and shutdown boundaries explicitly:
    - at slot 0 the gNB only publishes and the UE only reads
    - at the end the UE can still leave one final UL burst for the gNB to consume before both sides mark done
- `include/mini_gnb_c/ue/ue_ip_stack_min.h`
  - owns the minimal UE-side IPv4/ICMP behavior used by the shared-slot runtime
  - validates IPv4 packets
  - turns one downlink `ICMP Echo Request` into one pending uplink `ICMP Echo Reply`
- `include/mini_gnb_c/nas/nas_5gs_min.h`
  - owns the minimal UE-side 5GS NAS happy-path logic used by the live shared-slot runtime
  - consumes later `DL_NAS` messages from the local exchange directory
  - auto-emits `IdentityResponse`, `AuthenticationResponse`, `SecurityModeComplete`,
    `RegistrationComplete`, and `PDUSessionEstablishmentRequest` as follow-up
    `ue_to_gnb_nas/*.json` events for the simulator-side bridge
- `include/mini_gnb_c/ue/ue_tun.h`
  - owns the optional live UE TUN device
  - can create that device inside an isolated user+network namespace when
    `sim.ue_tun_isolate_netns=true`, which is the default for the end-to-end demo
  - when the caller can publish `/var/run/netns/<name>`, can optionally expose that
    isolated namespace as `sim.ue_tun_netns_name`
  - when `/var/run/netns` is not writable, now falls back to an anonymous isolated
    namespace and prints the correct rootless `nsenter --preserve-credentials -S 0 -G 0 -U -n ...`
    command for later manual probing
  - can install `default dev <ue_tun_name>` and either write `/etc/netns/<name>/resolv.conf`
    or bind a private `/etc/resolv.conf` inside the anonymous namespace from
    `sim.ue_tun_dns_server_ipv4`, which keeps name-based probes practical on rootless hosts
  - configures the learned `UE IPv4` and MTU with the host `ip` tool so the kernel
    networking stack can generate the real `ICMP Echo Reply`

The simulator-side radio frontend now supports that same live register model:

- `src/radio/mock_radio_frontend.c`
  - resolves `sim.shared_slot_path`
  - consumes one due UE UL burst from the shared slot register before any fallback transport
  - publishes one per-slot DL summary and waits for the UE to acknowledge that slot
  - still keeps the older JSON input path as a fallback for deterministic regression tests

The recommended split-config bring-up uses one gNB file and one UE file:

```bash
./build/mini_ue_c config/example_shared_slot_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_shared_slot_gnb.yml
wait $UE_PID
```

That shared-slot loop completes the current local attach and connected-mode mock flow:

- `PRACH -> RAR -> MSG3 -> MSG4`
- post-attach `PUCCH_SR`
- scheduled `BSR`
- scheduled UL `DATA`

For the live shared-slot path, connected-mode uplink scheduling is now sustained instead
of one-shot:

- the UE keeps a FIFO of pending uplink payloads and reports the real queued byte count
  in its `BSR`
- the gNB keeps per-UE `connected_ul_pending_bytes`, accepts later `PUCCH_SR`
  occasions, and keeps issuing one `DCI0_1 + UL DATA` grant at a time until the
  reported queue drains
- `out/summary.json` now also exports `connected_ul_pending_bytes` and
  `connected_ul_last_reported_bsr_bytes` for that gNB-side accounting state

In that split-config mode, the gNB file is the source of truth for downlink-authored
timing. The UE file no longer needs to duplicate the PDCCH/HARQ timing fields because
the live UE runtime follows the air-interface information published by the gNB instead:

- `SIB1` carries the PRACH period, offset, response window, and retry delay
- `SIB1` also carries the post-Msg4 DL/UL `time_indicator`, DL ACK timing, and DL/UL HARQ process counts
- `RAR` carries the exact mock Msg3 absolute slot
- `Msg4` / `RRCSetup` carries the SR period and offset

So the local loop now models "UE reacts to downlink scheduling information" instead of
"UE and gNB happen to share the same hard-coded slot plan".

Those SIB1 timing/HARQ fields are configured on the gNB side through `sim.*` YAML keys
such as `post_msg4_dl_time_indicator`, `post_msg4_dl_data_to_ul_ack_slots`,
`post_msg4_ul_time_indicator`, `post_msg4_dl_harq_process_count`, and
`post_msg4_ul_harq_process_count`. If they are omitted, the config loader supplies
defaults and the gNB still advertises those default values in SIB1.

The older filesystem-backed UE plan is still available as a fallback path when
`sim.shared_slot_path=""`, but it is no longer the preferred interaction model.

The live shared-slot loop is covered by the integration test:

- `tests/test_integration.c`
  - `test_integration_shared_slot_ue_runtime`
  - forks `mini_ue_c`-equivalent shared-slot UE runtime in one process
  - runs `mini_gnb_c_sim` against the same shared slot register file
  - verifies PRACH, Msg3, SR, BSR, UL DATA, and the promoted UE summary state
  - `test_integration_shared_slot_ue_runtime_repeats_sr_for_pending_uplink_queue`
  - verifies that the UE re-sends `PUCCH_SR` on a later SR occasion when queued UL payloads still have no grant
  - `test_integration_shared_slot_ue_runtime_consumes_uplink_queue_in_order`
  - verifies that multiple queued UL payloads are consumed in FIFO order across successive grants
  - `test_integration_slot_text_transport_continues_connected_ul_grants`
  - verifies that one `BSR` can drive multiple sequential UL payload grants until the gNB-side pending-byte accounting reaches zero
  - `test_integration_shared_slot_tun_uplink_reaches_n3`
  - verifies that one real packet read from the UE TUN device is forwarded onto the persistent N3 socket
- `tests/test_mini_ue_runtime.c`
  - `test_mini_ue_runtime_uplink_queue_tracks_bytes_and_bsr_dirty`
  - `test_mini_ue_runtime_update_uplink_state_rearms_sr_after_grant_consumption`
  - `test_mini_ue_runtime_builds_bsr_from_current_queue_bytes`
  - `test_mini_ue_runtime_skips_new_payload_grant_without_queue`
  - verifies that UE-side BSR and new-data UL payload generation now reflect only the real queued bytes instead of falling back to synthetic payloads after the queue drains
- `tests/test_shared_slot_link.c`
  - verifies the slot-0 boundary, the per-slot handshake, and the shutdown-side final UL consumption semantics of the shared register window

The promoted UE state now also carries the future core-bridge placeholder:

- `include/mini_gnb_c/common/types.h`
  - `mini_gnb_c_ue_context_t` embeds `mini_gnb_c_core_session_t`
  - the embedded session is seeded with the promoted `C-RNTI` even before any AMF bridge exists
- `out/summary.json`
  - each `ue_contexts[]` entry now exports a nested `core_session` object
  - the current local loop leaves NGAP/session/N3 fields as `null`, which gives the next Stage C bridge work a stable summary shape to fill
- `tests/test_ue_context_store.c`
  - verifies that UE promotion initializes the embedded `core_session` state cleanly

The first simulator-side core bridge now exists as well:

- `include/mini_gnb_c/core/gnb_core_bridge.h`
  - owns the single-UE gNB-to-AMF bridge runtime for the simulator
  - opens the reusable SCTP/NGAP transport, runs `NGSetup`, sends one `InitialUEMessage`, and captures the first AMF downlink NAS
  - after that first exchange, polls follow-up UE control-plane NAS events from `ue_to_gnb_nas/` and forwards them as `UplinkNASTransport`
  - recognizes later `InitialContextSetup` and `PDUSessionResourceSetup` AMF messages, sends the matching gNB responses, updates `core_session`, and keeps relaying later top-level `DL_NAS`
- `config/default_cell.yml`
  - now includes an optional `core:` section with `enabled`, `amf_ip`, `amf_port`, `upf_port`, `timeout_ms`, `ran_ue_ngap_id_base`, `default_pdu_session_id`, and optional `ngap_trace_pcap` / `gtpu_trace_pcap`
- `src/common/simulator.c`
  - calls the core bridge as soon as the UE context is promoted after Msg3
  - seeds `core_session.ran_ue_ngap_id` and the requested `pdu_session_id`
  - when `core.enabled=true`, performs `NGSetup + InitialUEMessage`, stores `amf_ue_ngap_id`, and increments the first uplink/downlink NAS counters
  - auto-opens runtime pcap traces for the Open5GS-facing NGAP and GTP-U paths; by default they land under the simulator output directory as `gnb_core_ngap_runtime.pcap` and `gnb_core_gtpu_runtime.pcap`
  - if `sim.local_exchange_dir` is configured, writes downlink NAS messages to `gnb_to_ue/seq_<nnnnnn>_gnb_DL_NAS.json`
  - polls `ue_to_gnb_nas/seq_<nnnnnn>_ue_UL_NAS.json` each slot after the first exchange and forwards due events to the AMF
  - parses later session-setup state such as `UE IPv4`, `UPF TEID`, and `QFI` into the promoted UE `core_session`
  - stages that parsed `UE IPv4` into later shared-slot DL summaries so the live UE can
    reconfigure its user-plane path without touching control-plane files
  - activates a persistent N3 UDP socket as soon as that user-plane session state becomes valid
  - keeps that socket open for the rest of the run and polls one downlink GTP-U packet per slot for later Stage D/E delivery work
  - exports the resolved `ngap_trace_pcap_path` and `gtpu_trace_pcap_path` in `summary.json`, and `apps/mini_gnb_c_sim.c` prints them at the end of the run
- `tests/test_gnb_core_bridge.c`
  - verifies the standalone bridge path against an injected fake SCTP/NGAP transport
  - verifies one follow-up `UL_NAS -> UplinkNASTransport -> DL_NAS` exchange after the initial attach message
  - verifies `InitialContextSetupRequest` and `PDUSessionResourceSetupRequest` handling, including automatic NGAP acknowledgements and `core_session` user-plane state extraction
  - verifies stale-vs-future `UL_NAS` queue handling and post-session `DL_NAS` relay after session setup
- `tests/test_integration.c`
  - `test_integration_core_bridge_prepares_initial_message`
  - verifies the simulator-side bridge wiring, summary export, and emitted `gnb_to_ue` downlink NAS event
  - `test_integration_core_bridge_relays_followup_ul_nas`
  - verifies that a due `ue_to_gnb_nas/UL_NAS` event is relayed through the simulator bridge and produces a second `gnb_to_ue/DL_NAS` event
  - `test_integration_core_bridge_extracts_session_setup_state`
  - verifies summary export of `upf_ip`, `upf_teid`, `qfi`, and `ue_ipv4` after a fake `PDUSessionResourceSetupRequest`
  - `test_integration_core_bridge_relays_post_session_nas`
  - verifies that later top-level `DownlinkNASTransport` messages are still relayed after session setup while the parsed session state remains intact

This is now the first live-control-plane bridge slice. The simulator can open the
configured SCTP association to the AMF, complete `NGSetup`, send one canned
`InitialUEMessage`, parse the first returned `AMF UE NGAP ID` and `NAS-PDU`, and
surface that NAS downlink into the local UE exchange directory. It can also relay
subsequent UE-originated `UL_NAS` event files through `UplinkNASTransport` and
emit the returned follow-up `DL_NAS` events back to the local exchange directory.

The simulator now also includes the first persistent N3 runtime slice:

- after `PDUSessionResourceSetupRequest` populates `core_session.upf_ip`,
  `core_session.upf_teid`, `core_session.qfi`, and `core_session.ue_ipv4`, the
  simulator activates `n3/n3_user_plane`
- that helper resolves the gNB-side local IPv4 toward the chosen UPF, binds one
  non-blocking UDP socket on the standard local GTP-U port `2152`, and keeps the
  endpoint alive across later slots instead of sending one-shot probe traffic
- the matching `PDUSessionResourceSetupResponse` now advertises that same gNB
  N3 IPv4 plus the fixed downlink TEID used by the simulator-side downlink socket
- each slot, `src/common/simulator.c` gives the helper a chance to poll one
  downlink GTP-U packet and emits a trace event when something arrives
- `include/mini_gnb_c/n3/gtpu_tunnel.h`
  - now also exposes a minimal G-PDU extractor so the simulator can decapsulate
    one downlink GTP-U packet back into its inner IPv4 payload
- `src/common/simulator.c`
  - now forwards one valid uplink IPv4 `UL DATA` payload into the persistent N3 socket
  - now decapsulates one polled downlink GTP-U packet and re-queues its inner IPv4 payload as a `DL_OBJ_DATA` transmission for the UE
  - in the unscripted live shared-slot path, no longer relies on a synthetic follow-up
    UL grant after a downlink N3 packet; instead, the UE re-uses its configured SR
    occasions and the gNB keeps the later `SR -> BSR -> repeated UL grant` loop running
  - accepts `sim.slot_sleep_ms` to add wall-clock pacing between slots for the live Open5GS demo
- together with `ue/ue_ip_stack_min` and `ue/ue_tun`, the repository now has two user-plane modes:
  - fallback minimal mode:
    - `downlink GTP-U -> gNB DL DATA -> UE synthetic ICMP echo reply -> gNB uplink GTP-U`
  - live TUN mode:
    - `downlink GTP-U -> gNB DL DATA -> UE TUN inject -> kernel ICMP echo reply -> UE TUN read -> gNB uplink GTP-U`
  - both modes still share the same persistent N3 socket and shared-slot radio transport
- `tests/test_n3_user_plane.c` now verifies:
  - socket activation against a loopback UDP peer
  - runtime uplink G-PDU encapsulation using the extracted session state
  - downlink GTP-U polling on the persistent socket
- `tests/test_ue_ip_stack_min.c` now verifies:
  - IPv4 `ICMP Echo Request -> Echo Reply` generation
  - non-IPv4 `DL_OBJ_DATA` payload ignore behavior
- `tests/test_integration.c` now also verifies:
  - `test_integration_shared_slot_ue_runtime_generates_icmp_reply_payload`
    - the live UE runtime turns one downlink ICMP request into the next granted UL payload
  - `test_integration_core_bridge_forwards_ul_ipv4_to_n3`
    - after session setup, the simulator forwards one valid uplink IPv4 payload to a fake UPF over GTP-U
  - full TUN bring-up remains a manual validation path because it depends on `/dev/net/tun`,
    `ip`, and a live Open5GS deployment instead of only the hermetic test harness

That means the repository now uses two local transports in parallel:

- shared-slot register window
  - primary radio-path coupling between `mini_ue_c` and `mini_gnb_c_sim`
- JSON local exchange directory
  - control-plane NAS handoff between the simulator-side bridge and later UE-side NAS work

The current follow-up control-plane event format is:

- `out/local_exchange/ue_to_gnb_nas/seq_000001_ue_UL_NAS.json`
  - envelope fields: `sequence`, `abs_slot`, `channel`, `source`, `type`
  - payload fields: `c_rnti`, `nas_hex`

The current bridge still does not implement a full UE-side NAS procedure state machine. It does not yet:

- implement a general-purpose UE NAS stack beyond the current Open5GS happy path
- negotiate arbitrary subscriber credentials from YAML; today the minimal NAS helper uses one built-in test subscriber profile that matches the repository defaults
- drive real UE-side `InitialContextSetup` or `PDUSessionResourceSetup` semantics; today it learns the resulting session state, relays later top-level `DL_NAS`, and sends the required gNB acknowledgements

This means the current `--replay` mode validates:

- N2 SCTP + NGAP setup to the AMF
- 5G AKA and protected NAS closure for one UE
- PDU session setup signaling up to `PDUSessionResourceSetupResponse`
- basic N3 UDP/GTP-U reachability to the UPF
- session-level N3 decapsulation in the UPF with one minimal UE IPv4/UDP payload

On the local Open5GS setup used during development, this was verified with:

```bash
./build/ngap_probe --replay --upf-ip 127.0.0.7 --upf-port 2152 127.0.0.5 38412 5000
ip -s link show dev ogstun
```

The replay output showed a parsed session such as:

- `UPF=127.0.0.7`
- `TEID=0x0000ef26`
- `QFI=1`
- `UE IPv4=10.45.0.7`

and `ogstun` RX counters increased by exactly one packet and 43 bytes, matching the
single inner IPv4/UDP probe payload emitted through the parsed GTP-U session tunnel.

The repository now also carries a manual Stage E end-to-end demo for the live
`mini_ue_c + mini_gnb_c_sim` path. The control-plane path is now closed far enough
for one happy-path Open5GS attach plus PDU session establishment: `mini_ue_c`
auto-consumes later `DL_NAS` messages and emits the matching follow-up `UL_NAS`
messages dynamically. The remaining limitation is that this is still a minimal,
hard-coded NAS workflow rather than a general UE NAS implementation.

For a local Open5GS setup matching the default example IPs, the manual bring-up flow is:

```bash
rm -rf out/local_exchange out/shared_slot_link.bin out/summary.json
mkdir -p out/local_exchange

./build/mini_ue_c config/example_open5gs_end_to_end_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_open5gs_end_to_end_gnb.yml &
GNB_PID=$!

wait $GNB_PID
wait $UE_PID
```

Those example configs add:

- `config/example_open5gs_end_to_end_gnb.yml`
  - enables the core bridge and persistent N3 path
  - enables `sim.slot_sleep_ms=10` so the slot loop stays alive long enough for manual traffic
- `config/example_open5gs_end_to_end_ue.yml`
  - enables `sim.ue_tun_enabled=true`
  - keeps `sim.ue_tun_isolate_netns=true`, so the UE-side TUN device and kernel reply path stay isolated from the host network namespace
  - requests publishing that namespace as `miniue-demo` when `/var/run/netns` is writable
  - installs a default route on the UE TUN device
  - uses `223.5.5.5` for name-based reachability checks; on rootless hosts that now falls back to an anonymous namespace with a private bound `/etc/resolv.conf`
- `examples/open5gs_ul_nas_seed/`
  - keeps optional canned follow-up `UL_NAS` fixtures for debugging or manual bridge experiments

Once session setup finishes, inspect `out/summary.json` for the learned `ue_ipv4`,
then validate the real end-to-end data path from the host or server side:

```bash
sed -n '1,240p' out/summary.json
ls -l out/gnb_core_ngap_runtime.pcap out/gnb_core_gtpu_runtime.pcap
ping -c 4 <ue_ipv4_from_summary>
ip -s link show dev ogstun
tcpdump -ni ogstun icmp
```

To originate traffic from the UE side instead of the host side, first make sure the
Open5GS host has internet forwarding enabled for the UE subnet. If the UE namespace
was published under `/var/run/netns`, use the named namespace:

```bash
sysctl -w net.ipv4.ip_forward=1
iptables -t nat -A POSTROUTING -s 10.45.0.0/16 -o <host_uplink_if> -j MASQUERADE
ip netns exec miniue-demo ping -c 4 8.8.8.8
ip netns exec miniue-demo ping -c 4 www.baidu.com
```

On rootless hosts where `/var/run/netns` is not writable, use the anonymous namespace
fallback printed by the UE log:

```bash
nsenter --preserve-credentials -S 0 -G 0 -U -n -t <ue_pid> ping -c 4 8.8.8.8
nsenter --preserve-credentials -S 0 -G 0 -U -n -m -t <ue_pid> ping -c 4 www.baidu.com
```

For debugging, validate in this order:

- first `ip netns exec miniue-demo ping -c 4 8.8.8.8`
- then `ip netns exec miniue-demo ping -c 4 www.baidu.com`

That split is important because public-IP reachability and DNS are separate failure
modes.

With the default Open5GS data plane used during development, the learned UE address is
usually in `10.45.0.0/16`, but the runtime summary and UE log are the source of truth.

On the current host, the 2026-04-08 manual re-run completed end-to-end:

- `nsenter --preserve-credentials -S 0 -G 0 -U -n -t <ue_pid> ping -c 4 8.8.8.8`
  returned `4 received`
- `nsenter --preserve-credentials -S 0 -G 0 -U -n -m -t <ue_pid> ping -c 4 www.baidu.com`
  resolved `www.a.shifen.com` and also returned `4 received`
- `out/gnb_core_gtpu_runtime.pcap` showed the matching bidirectional
  `127.0.0.1:2152 <-> 127.0.0.7:2152` GTP-U traffic
- companion `ogstun` and `enp4s0` captures showed the same ICMP and DNS traffic
  traversing the Open5GS host path and returning successfully

The generated trace pcaps are intended for later Wireshark inspection:

- `out/ngap_probe_ngap_runtime.pcap`
  - same payload-only format as `examples/gnb_ngap.pcap`
  - contains the dynamically encoded uplink NGAP messages plus the AMF responses
  - link type is `Private use 5`
- `out/ngap_probe_gtpu_runtime.pcap`
  - stores synthetic outer IPv4/UDP packets carrying the emitted and received GTP-U payloads
  - link type is `Raw IP`
- `out/gnb_core_ngap_runtime.pcap`
  - stores the simulator-side AMF-facing NGAP exchanges in send/receive order during `mini_gnb_c_sim`
  - link type is `Private use 5`
- `out/gnb_core_gtpu_runtime.pcap`
  - stores the simulator-side UPF-facing GTP-U packets with synthetic outer IPv4/UDP headers during `mini_gnb_c_sim`
  - link type is `Raw IP`

The current WSL validation result is:

- configure: passed
- build: passed
- `ctest --test-dir build --output-on-failure`: passed
- `./build/mini_gnb_c_sim`: passed

On a native Ubuntu host, the observed behavior can differ from WSL networking.
In one non-WSL validation, Open5GS AMF reachability was confirmed with both the
local listener address and the LAN address:

```bash
./build/ngap_probe 127.0.0.5 38412 5000
./build/ngap_probe 192.168.1.10 38412 5000
```

Both probes returned `NGSetupResponse detected.`. The WSL validation above does
not imply that off-box SCTP reachability behaves the same way inside WSL; if
you run this from WSL, verify that the AMF IP is reachable from the WSL guest
network before assuming the same result.

The simulator writes:

- `out/trace.json`
- `out/metrics.json`
- `out/summary.json`
- `out/tx/*.txt`
- `out/tx/*PDCCH*.txt`
- `out/iq/*.cf32`
- `out/iq/*.json`

The mock radio transport is now text-first:

- uplink input can be described with `slot_<abs_slot>_UL_OBJ_*.txt`
- downlink scheduling and payload are exported as `out/tx/slot_<abs_slot>_DL_OBJ_*.txt`
- `.cf32` is still kept as an optional waveform artifact for compatibility

Typical uplink text files are:

- `input/slot_20_UL_OBJ_PRACH.txt`
- `input/slot_24_UL_OBJ_MSG3.txt`
- `input/slot_30_UL_OBJ_PUCCH_SR.txt`
- `input/slot_33_UL_OBJ_DATA.txt`
- `input/slot_36_UL_OBJ_DATA.txt`

Example `PRACH` input:

```text
direction=UL
abs_slot=20
type=UL_OBJ_PRACH
preamble_id=27
ta_est=11
peak_metric=18.5
sample_count=256
```

Example `MSG3` input:

```text
direction=UL
abs_slot=24
type=UL_OBJ_MSG3
rnti=17921
snr_db=18.2
evm=2.1
crc_ok=true
payload_hex=020201460110A1B2C3D4E5F603011122334455667788
```

`payload_hex` in `MSG3` is the mock MAC PDU bytes. The current prototype accepts a
very simple layout:

- optional `C-RNTI CE`: `02 02 <tc-rnti-low> <tc-rnti-high>`
- then one `UL-CCCH` subPDU: `01 <len> <contention-id-48><establishment-cause><ue-identity-type><ue-identity>`

For example:

- `02020146` means `C-RNTI CE` with `TC-RNTI = 0x4601`
- `0110` means the following `UL-CCCH` payload is `0x10 = 16` bytes long
- `DEADBEEFCAFE05020102030405060708` means:
  - contention identity `DEADBEEFCAFE`
  - establishment cause `0x05`
  - UE identity type `0x02`
  - UE identity `0102030405060708`

The repository now includes a compact end-to-end example at
`config/example_custom_msg3_ul.yml` with matching files in
`examples/custom_msg3_ul_input/`.

Example `PUCCH_SR` input:

```text
direction=UL
abs_slot=30
type=UL_OBJ_PUCCH_SR
rnti=17921
crc_ok=true
sample_count=96
```

Example scheduled UL `BSR` input:

```text
direction=UL
abs_slot=33
type=UL_OBJ_DATA
rnti=17921
crc_ok=true
tbsize=16
payload_text=BSR|bytes=384
```

Example scheduled UL `DATA` input:

```text
direction=UL
abs_slot=36
type=UL_OBJ_DATA
rnti=17921
crc_ok=true
tbsize=96
payload_text=UL_DATA_20
```

`tbsize` is in bytes and comes from the mock scheduler's `prb_len + mcs`
lookup table. In the current connected-mode demo:

- `BSR`: `prb_len=8`, `mcs=4`, `tbsize=16`
- UL `DATA`: `prb_len=24`, `mcs=8`, `tbsize=96`
- DL `DATA`: `prb_len=24`, `mcs=9`, `tbsize=120`

To run the custom demo:

```bash
./build/mini_gnb_c_sim config/example_custom_msg3_ul.yml
```

That example uses:

- `slot_24_UL_OBJ_MSG3.txt` to inject a hand-written `payload_hex`
- `slot_33_UL_OBJ_DATA.txt` to inject the scheduled `BSR|bytes=384`
- `slot_36_UL_OBJ_DATA.txt` to inject a later UL payload with `payload_hex`

The repository now also includes two connected-mode scheduling demos:

- `config/example_scripted_schedule.yml`
  - uses `sim.scripted_schedule_dir=examples/scripted_schedule_plan`
  - drives per-slot scheduling directly with `slot_<abs_slot>_SCRIPT_DL.txt` and `slot_<abs_slot>_SCRIPT_UL.txt`
- `config/example_scripted_pdcch.yml`
  - uses `sim.scripted_pdcch_dir=examples/scripted_pdcch_plan`
  - feeds handcrafted `PDCCH/DCI` metadata through `slot_<abs_slot>_SCRIPT_PDCCH_DL.txt` and `slot_<abs_slot>_SCRIPT_PDCCH_UL.txt`

In both modes, built-in post-Msg4 connected scheduling is suppressed so the
script files become the only source of connected-mode grants.

Example direct scheduling files:

```text
type=SCRIPT_DL_DATA
abs_slot=27
rnti=17921
dci_format=DCI1_1
prb_start=40
prb_len=20
mcs=8
payload_text=SCRIPTED_DIRECT_DL
```

```text
type=SCRIPT_UL_GRANT
rnti=17921
scheduled_abs_slot=36
purpose=DATA
prb_start=46
prb_len=16
mcs=8
k2=2
```

Example PDCCH-driven scheduling files:

```text
direction=DL
channel=PDCCH
type=DL_OBJ_PDCCH
rnti=17921
dci_format=DCI1_0
scheduled_abs_slot=27
scheduled_type=DATA
scheduled_prb_start=50
scheduled_prb_len=24
mcs=4
payload_text=SCRIPTED_PDCCH_DL
```

```text
direction=DL
channel=PDCCH
type=DL_OBJ_PDCCH
rnti=17921
dci_format=DCI0_1
scheduled_abs_slot=36
scheduled_type=DATA
scheduled_purpose=DATA
scheduled_prb_start=44
scheduled_prb_len=32
mcs=8
k2=2
```

Run them with:

```bash
./build/mini_gnb_c_sim config/example_scripted_schedule.yml
./build/mini_gnb_c_sim config/example_scripted_pdcch.yml
```

Example downlink transport export:

```text
direction=DL
channel=PDSCH
abs_slot=25
type=DL_OBJ_MSG4
rnti=17921
prb_start=48
prb_len=16
tbsize=32
payload_hex=1006...
payload_text=\x10\x06...RRCSetup|cause=3|ue_type=1
scheduled_by_pdcch=true
dci_format=DCI1_0
```

Example companion PDCCH export:

```text
direction=DL
channel=PDCCH
abs_slot=25
type=DL_OBJ_PDCCH
rnti=17921
dci_format=DCI1_0
dci_direction=DL
scheduled_type=MSG4
scheduled_prb_start=48
scheduled_prb_len=16
```

Example UL grant export:

```text
direction=DL
channel=PDCCH
type=DL_OBJ_PDCCH
dci_format=DCI0_1
dci_direction=UL
scheduled_type=BSR
scheduled_purpose=BSR
scheduled_abs_slot=33
k2=2
```

The IQ export is a minimal mock PHY waveform:

- format: interleaved little-endian `float32`, layout `I0,Q0,I1,Q1,...`
- one file per downlink object such as `SSB`, `SIB1`, `RAR`, and `Msg4`
- sidecar JSON includes slot, RNTI, PRB allocation, FFT size, CP length, and sample count

The uplink mock radio supports slot-driven file input when `sim.ul_input_dir`
points to an existing directory. It checks text files first and falls back to `.cf32`
when a matching text file is not present. Example files:

- `input/slot_20_UL_OBJ_PRACH.txt`
- `input/slot_24_UL_OBJ_MSG3.txt`
- `input/slot_20_UL_OBJ_PRACH.cf32`
- `input/slot_24_UL_OBJ_MSG3.cf32`

In this mode, the simulator checks the directory once per `abs_slot`:

- matching file exists: load the UL burst and pass it to upper layers
- matching file missing: treat the slot as empty UL input
- no input directory present: keep the existing synthetic PRACH / Msg3 injection path

## Current Scope

The current C prototype keeps:

- SSB/PBCH/MIB scheduling
- companion PDCCH/DCI export for scheduled PDSCH objects
- post-Msg4 connected traffic with `PUCCH` config, `SR`, a small `DCI0_1` grant for `BSR`, and a larger `DCI0_1` grant for UL payload
- SIB1 scheduling with `period + offset`
- mock radio RX bursts for PRACH and Msg3
- text transport export for DL scheduling and payload
- text transport input for UL PRACH and Msg3 content
- slot-driven UL burst input from `input/slot_<abs_slot>_UL_OBJ_*.cf32`
- PRACH detection driven by received UL burst rather than direct slot trigger
- RAR generation and Msg3 UL grant
- mock Msg3 decode from the UL grant slot
- retry from Msg3 miss back to a new Msg1/PRACH attempt
- MAC UL demux
- `RRCSetupRequest` parsing
- Msg4 generation with contention resolution identity and `RRCSetup`
- YAML config with comment support
- JSON trace, metrics, and summary artifacts in `out/`

Useful `sim:` configuration keys:

- `prach_trigger_abs_slot`: inject the first UL PRACH burst into the mock radio
- `msg3_delay_slots`: used by Msg2/RAR to place the expected Msg3 slot
- `msg3_present`: whether a UL Msg3 burst actually arrives at the granted slot
- `prach_retry_delay_slots`: if Msg3 is missing, inject a retry PRACH burst after this delay
- `ul_prach_cf32_path` / `ul_msg3_cf32_path`: optional external UL sample files; empty means generated toy bursts
- `ul_input_dir`: optional slot-driven UL input directory; when it exists, missing slots stay silent and `*.txt` files override `*.cf32`
- `scripted_schedule_dir`: optional direct scheduling plan directory with `SCRIPT_DL` / `SCRIPT_UL` files
- `scripted_pdcch_dir`: optional PDCCH-driven scheduling plan directory with `SCRIPT_PDCCH_DL` / `SCRIPT_PDCCH_UL` files
- `ul_bsr_buffer_size_bytes`: default synthetic `BSR|bytes=...` payload size used after `SR`

The current C prototype does not keep:

- core network integration
- real RF/UHD
- multi-UE scheduling
- security, reconfiguration, handover, paging
- HARQ, DRB, PDCP, SDAP
- full NR PDCCH/PDSCH/PUSCH encoding; current PDCCH is metadata/text export only

## PDCCH Model

The current transport model now distinguishes:

- `SSB/MIB`: exported as `PBCH`, no companion `PDCCH`
- `SIB1`, `RAR`, `Msg4`: exported as `PDSCH` plus a companion `PDCCH` text file
- `Msg3`: still does not use `PDCCH`; it is triggered by the UL grant carried inside `Msg2/RAR`
- `PUCCH SR`: uplink only, not scheduled by `PDCCH`

The DCI enum already supports the four mock formats:

- `DCI0_0`
- `DCI0_1`
- `DCI1_0`
- `DCI1_1`

In the current initial-access flow and the minimal connected follow-up:

- `DCI1_0` is used for `SIB1`, `RAR`, and `Msg4`
- `DCI1_1` is used for the post-Msg4 downlink `DATA` PDSCH that carries the mock `PUCCH` config
- `DCI0_1` is used first for the compact `BSR` grant and then again for the larger UL `DATA` grant
- `Msg3` still stays special and is driven by `Msg2/RAR`, not by a standalone PDCCH object
