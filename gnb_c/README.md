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
  - handles the slot-0 and shutdown boundaries explicitly:
    - at slot 0 the gNB only publishes and the UE only reads
    - at the end the UE can still leave one final UL burst for the gNB to consume before both sides mark done

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
  - recognizes later `InitialContextSetup` and `PDUSessionResourceSetup` AMF messages, sends the matching gNB responses, and updates `core_session`
- `config/default_cell.yml`
  - now includes an optional `core:` section with `enabled`, `amf_ip`, `amf_port`, `timeout_ms`, `ran_ue_ngap_id_base`, and `default_pdu_session_id`
- `src/common/simulator.c`
  - calls the core bridge as soon as the UE context is promoted after Msg3
  - seeds `core_session.ran_ue_ngap_id` and the requested `pdu_session_id`
  - when `core.enabled=true`, performs `NGSetup + InitialUEMessage`, stores `amf_ue_ngap_id`, and increments the first uplink/downlink NAS counters
  - if `sim.local_exchange_dir` is configured, writes downlink NAS messages to `gnb_to_ue/seq_<nnnnnn>_gnb_DL_NAS.json`
  - polls `ue_to_gnb_nas/seq_<nnnnnn>_ue_UL_NAS.json` each slot after the first exchange and forwards due events to the AMF
  - parses later session-setup state such as `UE IPv4`, `UPF TEID`, and `QFI` into the promoted UE `core_session`
- `tests/test_gnb_core_bridge.c`
  - verifies the standalone bridge path against an injected fake SCTP/NGAP transport
  - verifies one follow-up `UL_NAS -> UplinkNASTransport -> DL_NAS` exchange after the initial attach message
  - verifies `InitialContextSetupRequest` and `PDUSessionResourceSetupRequest` handling, including automatic NGAP acknowledgements and `core_session` user-plane state extraction
- `tests/test_integration.c`
  - `test_integration_core_bridge_prepares_initial_message`
  - verifies the simulator-side bridge wiring, summary export, and emitted `gnb_to_ue` downlink NAS event
  - `test_integration_core_bridge_relays_followup_ul_nas`
  - verifies that a due `ue_to_gnb_nas/UL_NAS` event is relayed through the simulator bridge and produces a second `gnb_to_ue/DL_NAS` event
  - `test_integration_core_bridge_extracts_session_setup_state`
  - verifies summary export of `upf_ip`, `upf_teid`, `qfi`, and `ue_ipv4` after a fake `PDUSessionResourceSetupRequest`

This is now the first live-control-plane bridge slice. The simulator can open the
configured SCTP association to the AMF, complete `NGSetup`, send one canned
`InitialUEMessage`, parse the first returned `AMF UE NGAP ID` and `NAS-PDU`, and
surface that NAS downlink into the local UE exchange directory. It can also relay
subsequent UE-originated `UL_NAS` event files through `UplinkNASTransport` and
emit the returned follow-up `DL_NAS` events back to the local exchange directory.

That means the repository now uses two local transports in parallel:

- shared-slot register window
  - primary radio-path coupling between `mini_ue_c` and `mini_gnb_c_sim`
- JSON local exchange directory
  - control-plane NAS handoff between the simulator-side bridge and later UE-side NAS work

The current follow-up control-plane event format is:

- `out/local_exchange/ue_to_gnb_nas/seq_000001_ue_UL_NAS.json`
  - envelope fields: `sequence`, `abs_slot`, `channel`, `source`, `type`
  - payload fields: `c_rnti`, `nas_hex`

The current bridge still stops after that first NAS exchange. It does not yet:

- maintain the full UE NAS procedure sequence that `ngap_probe --replay` already exercises
- auto-generate those follow-up `UL_NAS` events from `apps/mini_ue_c.c`; today they are still test-driven or manually emitted
- drive real `InitialContextSetup` or `PDUSessionResourceSetup` semantics on the UE side; today it only captures the resulting session state and sends the required gNB acknowledgements

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

The generated trace pcaps are intended for later Wireshark inspection:

- `out/ngap_probe_ngap_runtime.pcap`
  - same payload-only format as `examples/gnb_ngap.pcap`
  - contains the dynamically encoded uplink NGAP messages plus the AMF responses
  - link type is `Private use 5`
- `out/ngap_probe_gtpu_runtime.pcap`
  - stores synthetic outer IPv4/UDP packets carrying the emitted and received GTP-U payloads
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
