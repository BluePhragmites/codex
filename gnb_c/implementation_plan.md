# gnb_c Feature Status and Roadmap

This document answers only two questions:

1. which capabilities are implemented today
2. which capabilities are planned next

It does not try to explain build steps, full test commands, or code-reading details.

- Build and run entrypoints: `README.md`
- Test matrix and validation commands: `feature_test_guide.md`
- Architecture and code-reading path: `architecture.md`

## 1. Current Position

`gnb_c` is no longer just a `Msg4`-only skeleton. It is now a single-UE prototype that covers:

- a local slot-driven UE <-> gNB loop
- minimal gNB <-> AMF NGAP control-plane connectivity
- PDU Session establishment
- minimal gNB <-> UPF N3 user-plane connectivity
- end-to-end UE TUN traffic
- runtime NGAP and GTP-U pcap export

The most accurate way to frame it today is:

A single-UE, single-cell, mock-PHY teaching and validation prototype that can complete minimal registration, session setup, `ping`, and `curl` against a local Open5GS deployment.

For roadmap purposes, this should be treated as a simulation and core-integration baseline, not yet as a real-air-interface gNB.

## 2. Implemented Features

### 2.1 Local Radio-Side Closed Loop

Implemented:

- `PRACH -> RAR -> MSG3 -> MSG4`
- connected-mode `PUCCH_SR`
- `BSR`
- `UL DATA`
- slot-by-slot synchronization between the gNB and UE

Characteristics:

- the UE no longer depends on a one-shot pre-generated event batch
- the gNB and UE can run with different YAML files
- the UE learns timing from runtime downlink information instead of relying on fully mirrored slot schedules

### 2.2 Follow-Up NAS over the Bearer Path

Implemented:

- follow-up NAS from the AMF is emitted by the gNB as `DL_OBJ_DATA`
- UE NAS responses are sent as `UL_OBJ_DATA`
- NAS objects are explicitly tagged with `payload_kind=NAS`

This means:

- follow-up NAS no longer depends on a local JSON exchange path for the main flow
- NAS and later user-plane traffic share the same mock `PDSCH/PUSCH` bearer path

### 2.3 Minimal Open5GS Control-Plane Bridge

Implemented:

- `NGSetup`
- `InitialUEMessage`
- follow-up `UplinkNASTransport`
- `InitialContextSetupResponse`
- `PDUSessionResourceSetupResponse`

Extracted session state includes:

- `RAN UE NGAP ID`
- `AMF UE NGAP ID`
- `UE IPv4`
- `UPF IP`
- `TEID`
- `QFI`

### 2.4 Minimal N3 User Plane

Implemented:

- persistent N3 UDP socket setup on the gNB side
- uplink IPv4 encapsulation into GTP-U toward the UPF
- downlink GTP-U decapsulation back into the mock downlink data path
- runtime GTP-U pcap export

### 2.5 UE TUN Path

Implemented:

- UE TUN configuration using the session-provided UE IPv4
- downlink IP injection into the UE TUN device
- uplink packets read back from the TUN device
- named and anonymous network-namespace modes
- optional default-route and DNS setup

### 2.6 Sustained Connected-Mode Scheduling

Implemented:

- UE-side uplink FIFO
- `sr_pending`
- `bsr_dirty`
- repeated `SR`
- gNB-side `connected_ul_pending_bytes`
- one `BSR` driving multiple consecutive UL grants until the queue is drained

### 2.7 Large-Packet Bearer Support

Implemented:

- a minimal `RLC-lite` segmentation and reassembly layer
- large IPv4 SDUs can cross multiple mock `PUSCH` and `PDSCH` grants
- small NAS and small IPv4 payloads can still fit into a single grant

Its goal is not to be a full RLC stack. Its goal is to let larger and burstier traffic such as `curl` complete over the current bearer path.

### 2.8 Observability and Artifacts

Implemented:

- `summary.json`
- `metrics.json`
- `trace.json`
- gNB-side `out/tx/*.txt`
- UE-side `out/rx/*.txt`
- runtime NGAP pcap export
- runtime GTP-U pcap export

### 2.9 Stage 1 Foundation for the First Real RF Target

Implemented:

- a `radio_frontend` boundary now sits between the simulator and the backend-specific mock radio code
- `mini_gnb_c_sim` can now also run a hybrid B210-backed `radio_frontend` when `rf.device_driver` targets `uhd-b210` and `rf.runtime_mode=simulator`
- the first real-air-interface target profile is explicitly locked in config as `real_cell.target_backend: "uhd-b210"`
- a repo-local `mini_b210_probe_c` smoke tool can open the B210 through the UHD C API, validate lock sensors, and capture one RX burst
- the same smoke tool can also replay one `fc32` IQ file as a finite TX burst and report `burst_ack` plus async TX warnings
- the B210 smoke tool now also supports a fixed-size single-file `sc16` ring map for RX and TX replay:
  - one file per direction such as `/dev/shm/rx_ring.map` or `/dev/shm/tx_ring.map`
  - superblock plus descriptor ring plus payload ring
  - wrap-around tracking through `oldest_valid_seq`, `next_write_seq`, and `last_committed_seq`
- the ring-map payload now supports one or two channels, stored channel-major inside each block:
  - channel 0 region first
  - channel 1 region second
- a repo-local `mini_ring_inspect` tool can inspect ring geometry and the latest ready descriptors without guessing the wrap point
- a repo-local `mini_ring_export` tool can export an inclusive `seq` range from a ring map into one raw `sc16` file per channel plus a metadata text file
- B210 RX ring captures now record a per-block time anchor from UHD RX metadata when available
  - `hw_time_ns` is the first-sample timestamp of the block
  - `flags` distinguish UHD hardware time from host fallback time
- the B210 smoke probe now supports separate `--rx-gain` and `--tx-gain` controls in `trx` mode while keeping `--gain` as a shared shortcut
- the B210 probe now supports:
  - one-channel or two-channel RX ring capture
  - one-channel or two-channel TX ring replay
  - a ring-based `trx` mode with simultaneous RX and TX workers pinned to separate CPU cores
- standalone `tx` mode now uses a larger user-space prefetch window for ring-map replay
  - it copies channel-major `sc16` blocks from the ring map into a staging window ahead of time
  - it refills that window when the buffered sample count falls below a low-watermark threshold
  - the prefetch window is externally overridable through `--tx-prefetch-samples`
- the `trx` mode TX worker now uses the same prefetch-window and low-watermark refill policy
  - `--tx-prefetch-samples` applies to both standalone `tx` and `trx` TX replay
  - `trx` duration now remains meaningful even when the TX ring is shorter than the requested run
  - the TX side loops the source ring until `rate * duration` samples have been replayed
  - `tx_ring_wrap_count` reports how many times that wrap-around happened
- RX stress handling is now explicit instead of implicitly fatal
  - UHD `OVERFLOW` and `TIMEOUT` are counted and the run continues
  - broken chains, bad packets, alignment failures, and API failures are still fatal
  - RX summaries now also estimate lost samples from hardware-time gaps
  - `wallclock` duration mode is available when the goal is timed stress testing instead of successful-sample collection
- the B210 `trx` path now also supports separate RX and TX center frequencies
  - CLI: `--rx-freq` and `--tx-freq`
  - shared YAML: `rf.rx_freq_hz` and `rf.tx_freq_hz`
- the same tuning knob is now also present in shared RF config as `rf.tx_prefetch_samples`
- `mini_b210_probe_c` can now preload its RF defaults from `--config <yaml>` using the shared `rf` section
  - `freq_hz`, `rx_freq_hz`, `tx_freq_hz`, and `tx_prefetch_samples` are now part of that shared RF config
  - command-line probe arguments still override YAML-loaded defaults
- the same shared `rf` section can now also drive the main application entrypoints:
  - `mini_gnb_c_sim`
  - `mini_ue_c`
  - `rf.runtime_mode=rx|tx|trx` switches those apps into a B210/UHD harness path
  - the harness uses the same shared YAML RF fields for rate, frequency, gains, bandwidth, duration, ring geometry, CPU affinity, and TX prefetch
  - this removes the need to repeat long probe CLI argument lists when the goal is hardware smoke, capture, replay, or TRX stress
- the simulator-facing `radio_frontend` now also has a first hybrid B210 backend:
  - it keeps the existing mock slot semantics for `PRACH`, `MSG3`, `SR`, and `UL DATA`
  - it starts long-lived B210 RX and TX workers under `mini_gnb_c_sim`
  - it writes real B210 RX IQ into the configured RX ring map
  - it turns slot-mapped DL patches into a toy TX waveform and streams that through the configured TX ring map
  - this moves the real-radio runtime under the simulator backend boundary without claiming that real NR PHY decoding is finished
- radio-facing code now defaults to a release-style compile profile even when the main build is `Debug`
  - `MINI_GNB_C_RADIO_FORCE_RELEASE=ON` is the default
  - `MINI_GNB_C_RADIO_FORCE_DEBUG=ON` is the explicit opt-in override
- the B210 smoke path now applies the B210-relevant subset of `srsran_performance` before radio startup:
  - set the CPU governor to `performance`
  - disable DRM KMS polling
  - skip Ethernet socket-buffer tuning because the B210 is USB-based
- the RF config now carries explicit Stage 1 CPU-affinity placeholders:
  - `rf.rx_cpu_core`
  - `rf.tx_cpu_core`
- the `real_cell` profile carries the initial B210 bring-up identity:
  - one backend target
  - one band
  - one numerology
  - one bandwidth
  - one PLMN/TAC profile

This is intentionally still a foundation step. The simulator now has a real B210-backed hybrid backend, but it is still not a full real-air-interface PHY.

## 3. Known Boundaries

The following items are current prototype boundaries, not accidental omissions:

- the system is still a single-UE, single-cell prototype
- the UE NAS path is still biased toward the Open5GS happy path
- `RLC-lite` is not a full 3GPP RLC AM or UM implementation
- scheduling, HARQ, MAC, and PHY are still prototype-grade mock implementations
- real internet reachability still depends on the host-side Open5GS, NAT, and DNS environment
- there is no real RF, SDR, or commercial-UE attach path yet
- there is no real RRC/MAC/PHY stack for an off-the-shelf handset
- the current simulator-facing B210 backend is hybrid
  - real B210 RX and TX workers run under `radio_frontend`
  - UL control/data events still come from the existing mock slot logic
  - DL emission still uses the current toy DL waveform, not a real NR PHY waveform
- `mini_gnb_c_sim` and `mini_ue_c` still keep their separate B210 app-harness entry modes for pure hardware smoke, capture, replay, and TRX stress
- the current ring-map path now participates in the simulator-facing B210 backend, but it is still not the final long-term queue design for a real handset-capable PHY
- built-in host tuning plus the TX prefetch window materially improve replay robustness on this host, but they do not replace the remaining PHY and decoding work

## 4. Roadmap Toward Commercial Handset Attach

The long-term target is not just a richer simulator. The long-term target is a minimal real-air-interface gNB that can accept a commercial handset in a controlled environment.

That target should be approached in stages. Each stage below has a concrete goal, a bounded work scope, and an exit criterion.

### Stage 0: Simulation and Core Baseline

Status:

- completed

Goal:

- keep a stable simulation baseline for N2, N3, bearer behavior, and observability

Main work items already done:

- local UE <-> gNB slot loop
- Open5GS control-plane bridge
- N3 user plane
- TUN-based internet traffic
- `RLC-lite` bearer support
- runtime pcap and text artifacts

Exit criteria:

- local Open5GS validation can complete registration, session setup, `ping`, and `curl`
- automated tests cover the main mock-radio paths

Why this stage matters:

- every later real-air-interface step still needs this baseline for comparison and regression control

### Stage 1: Real Cell Bring-Up and Broadcast Channel

Status:

- in progress

Goal:

- make a commercial handset able to detect the cell and read the minimum broadcast information

Main work items:

- introduce a B210/UHD-backed radio adapter boundary without breaking the existing mock-radio path
- implement real-time sample scheduling and timestamp handling
- generate real SSB/PBCH and a valid MIB
- transmit a valid SIB1 on a real carrier configuration
- define a minimal supported deployment profile:
  - one backend target: `uhd-b210`
  - one band
  - one numerology
  - one bandwidth
  - one PLMN/TAC profile

Current Stage 1 progress:

- completed:
  - simulator-facing `radio_frontend` boundary
  - first hybrid B210-backed `radio_frontend` backend under `mini_gnb_c_sim`
  - explicit `real_cell` config section
  - locked first target profile around `uhd-b210`
  - repo-local B210/UHD RX smoke probe through the UHD C API
  - repo-local B210/UHD TX-from-file smoke probe through the UHD C API
  - long-lived B210 RX and TX workers behind the simulator radio boundary
  - basic real-time sample and timestamp handling for the current hybrid backend
  - built-in B210 host tuning before RF startup
  - explicit `rf.subdev`, `rf.rx_cpu_core`, and `rf.tx_cpu_core` configuration knobs
- remaining:
  - real SSB/PBCH/MIB/SIB1 transmission
  - replace the current toy DL waveform with a real or at least standards-shaped broadcast waveform
  - make a handset able to detect and camp on the configured cell profile instead of only proving host-side RF runtime

Exit criteria:

- a commercial handset can discover the cell in cell search
- the handset can camp on the cell long enough to read MIB and SIB1
- captures from SDR and handset behavior match the configured carrier and broadcast parameters

Notes:

- this stage is still before successful attach
- it is mainly about making the cell visible and believable to real UEs
- the practical execution order for RF work is:
  - first prove B210 RX capture and host timing with `mini_b210_probe_c`
  - then add a TX smoke path and separate RX/TX worker cores
  - then integrate those workers into the real `uhd-b210` simulator backend
  - only after that start real SSB/PBCH/MIB/SIB1 transmission

### Stage 2: Custom Over-the-Air Replay of the Current Mock Flow

Status:

- pending

Goal:

- make the gNB and UE complete the current mock/shared-slot interaction flow over B210 using a deliberately simple custom slot PHY

This stage is not yet “commercial handset attach”. It is the second major internal milestone:

- keep the existing mock control logic and payload objects
- move their transport from files/shared-slot events into real IQ over B210
- use a simple self-defined air format with payload plus CRC plus reference signals

Main work items:

- define a minimal air-frame/PDU format shared by gNB and UE:
  - `DL-SSB`
  - `DL-CTRL`
  - `DL-DATA`
  - `UL-PRACH`
  - `UL-DATA`
  - `UL-ACK`
  - `UL-SR`
- define a fixed container format for those PDUs:
  - header
  - type
  - slot or frame index
  - RNTI or preamble context
  - payload length
  - payload
  - CRC
- add a bridge layer between existing mock scheduler/runtime objects and those air PDUs
- keep the current ring-map and B210 runtime as observability and transport infrastructure

### Stage 2A: Framing, Synchronization, and Air-PDU Contract

Status:

- pending

Goal:

- define and prove the minimum common slot timing and framing needed for a custom B210-only air interface

Main work items:

- define the exact binary layout for `DL-SSB`, `DL-CTRL`, `DL-DATA`, `UL-PRACH`, `UL-DATA`, `UL-ACK`, and `UL-SR`
- add CRC validation at the air-PDU level
- define one strong downlink synchronization signal:
  - Zadoff-Chu or equivalent strong sequence inside `DL-SSB`
- define one strong uplink access signal:
  - Zadoff-Chu or equivalent inside `UL-PRACH`
- encode enough timing metadata in `DL-SSB` or `DL-CTRL` for the UE to derive absolute slot timing
- implement the first bridge layer:
  - mock DL grant or PDCCH object -> air PDU
  - decoded air PDU -> existing UE runtime input event

Current Stage 2A progress:

- completed:
  - a minimal shared air-PDU binary contract
  - CRC-backed encode and parse helpers for:
    - `DL-SSB`
    - `DL-CTRL`
    - `DL-DATA`
    - `UL-PRACH`
    - `UL-DATA`
    - `UL-ACK`
    - `UL-SR`
  - unit-test coverage for round-trip encode/decode and CRC rejection
- remaining:
  - slot and frame semantics on top of that binary format
  - the first actual `DL-SSB` and `UL-PRACH` waveform mapping
  - bridge code from scheduler/runtime objects into those PDUs

Exit criteria:

- the gNB continuously emits a decodable `DL-SSB`
- the UE can establish coarse downlink timing from `DL-SSB`
- the UE can emit a timed `UL-PRACH`
- the gNB can detect `UL-PRACH` and recover preamble plus timing offset

### Stage 2B: Downlink Payload Transport Over IQ

Status:

- pending

Goal:

- move the current mock downlink file and object transport onto real B210 IQ while keeping payload semantics unchanged

Main work items:

- map existing simulator objects into air PDUs:
  - `PDCCH`-like scheduling info into `DL-CTRL`
  - `RAR`, `MSG4`, `NAS`, and `DL DATA` payloads into `DL-DATA`
- define a simple modulate and demodulate path with:
  - reference signal or pilot support
  - payload decoding
  - CRC check
- replace the current toy “write patch to TX ring” behavior with “encode air PDU to TX waveform”
- connect decoded UE downlink PDUs back into the existing UE runtime and NAS path

Exit criteria:

- the UE can decode `DL-CTRL`
- the UE can decode `RAR`, `MSG4`, and generic downlink payloads from `DL-DATA`
- the UE runtime can consume those decoded payloads instead of file/shared-slot inputs

### Stage 2C: Uplink Payload Transport and Full Mock-Flow Replay

Status:

- pending

Goal:

- move the current mock uplink file and object transport onto real B210 IQ and complete the old mock flow end to end

Main work items:

- encode UE-side `MSG3`, `SR`, `ACK`, `BSR`, `NAS`, and IPv4 payloads into `UL-DATA` or `UL-ACK`
- implement gNB-side uplink demodulation, CRC validation, and object recovery
- map decoded uplink PDUs back into:
  - `mini_gnb_c_radio_burst_t`
  - existing MAC/RRC/NAS handling
- replace mock UL injection in the main closed-loop flow when the custom air path is enabled
- validate timing advance and simple slot-level scheduling around repeated UL events

Exit criteria:

- gNB and UE can complete `PRACH -> RAR -> MSG3 -> MSG4` through B210 IQ
- `SR -> grant -> UL DATA` also works over B210 IQ
- follow-up NAS can cross the custom air link in both directions
- the previous mock-file interaction logic can be replayed over the air without depending on shared-slot transport

Notes:

- this stage intentionally does not require standards-compliant NR PHY
- it is a custom minimal air interface used to retire file-based mock exchange first
- passing this stage means the control logic has left the file bus and moved onto radio IQ

### Stage 3: Real Random Access and Minimal RRC Setup for Commercial Handsets

Status:

- pending

Goal:

- replace the custom over-the-air replay path with the first real handset-facing access procedures

Main work items:

- implement real PRACH reception and timing handling
- map Msg2, Msg3, and Msg4 onto real PDCCH/PDSCH/PUSCH behavior instead of mock objects
- add a minimal RRC state machine for:
  - `RRCSetupRequest`
  - `RRCSetup`
  - `RRCSetupComplete`
- connect the existing NAS bridge to the real RRC/NAS container path
- add the minimum timers and retry handling needed to survive realistic handset behavior

Exit criteria:

- a commercial handset can trigger random access
- the gNB can complete `RRCSetup`
- `RRCSetupComplete` reaches the core-facing side and produces the first real `InitialUEMessage`

Notes:

- this is the first stage where the mock UE is no longer enough as the primary validation target
- handset traces and SDR captures become required evidence

### Stage 4: NAS Transport and Access-Stratum Security for Real UE Attach

Status:

- pending

Goal:

- make a commercial handset complete registration over the real air interface

Main work items:

- support bidirectional NAS transfer through real RRC signaling
- implement the minimum AS security path needed after security activation:
  - security context setup
  - ciphering and integrity hooks at the right layer boundaries
- add the minimum UE capability and configuration handling that handsets expect during attach
- harden NGAP, session state, and error handling for real handset timing and retransmission behavior

Exit criteria:

- a commercial handset can complete registration with Open5GS through the real air interface
- the handset remains attached long enough to request a default PDU session
- the control-plane trace is stable across repeated attach attempts

Notes:

- this stage is the true threshold between a lab simulator and a minimal real gNB prototype
- until this stage is complete, the project is still primarily a simulator

### Stage 5: Real User Plane for One Commercial Handset

Status:

- pending

Goal:

- make one commercial handset pass real user-plane traffic through the gNB and UPF

Main work items:

- carry real handset IP traffic over the real bearer path
- introduce a cleaner boundary between scheduler, bearer segmentation, and packet forwarding
- replace the most simulator-specific bearer assumptions that cannot survive real handset traffic patterns
- validate downlink buffering, uplink grants, and sustained mixed traffic under a real UE
- keep the existing TUN/Open5GS validation path as a regression oracle

Exit criteria:

- one commercial handset can establish a default PDU session
- the handset can pass `ping`, DNS, and basic HTTP traffic through Open5GS
- captures show stable bidirectional user-plane traffic over repeated runs

Notes:

- this stage does not require full standards-grade MAC/RLC completeness
- it does require a bearer path that is stable enough for real handset traffic rather than just the local mock UE

### Stage 6: Stability, Interoperability, and Small-Lab Usability

Status:

- pending

Goal:

- move from one successful handset demo to a repeatable small-lab system

Main work items:

- improve scheduler realism and backlog handling
- improve bearer reliability and recovery behavior
- support at least a small matrix of handset models and software versions
- add better diagnostics for attach failure classification
- reduce per-run manual tuning in RF and core-network configuration
- define safe operating guidance for shielded or cabled test environments

Exit criteria:

- multiple commercial handset models can attach in the same controlled profile
- repeated attach and data tests work without per-run code changes
- troubleshooting is guided by stable traces and documented failure categories

### Stage 7: Expansion Beyond the First Real gNB

Status:

- future

Goal:

- expand the system beyond the first minimal commercial-UE attach milestone

Possible directions:

- multi-UE support
- multi-cell support
- stronger MAC/RLC/PDCP layering
- more complete RRC procedures
- broader core-network interoperability
- more realistic performance and RF behavior

This stage should only start after Stage 5 and Stage 6 are stable enough to serve as a real baseline.

## 5. Immediate Execution Order

If the project starts moving toward the current next milestone now, the recommended order is:

1. freeze the current simulator and Open5GS path as the regression baseline
2. define one narrow RF target profile
   - one SDR family: USRP B210
   - one carrier profile
   - one sample rate
   - one gNB machine and one UE machine
3. finish the remaining Stage 1 broadcast and timing work that is needed for stable B210 runtime usage
4. execute Stage 2A
   - define the custom air PDU contract
   - define `DL-SSB` and `UL-PRACH` synchronization behavior
5. execute Stage 2B
   - move current downlink mock payload transport onto IQ
6. execute Stage 2C
   - move current uplink mock payload transport onto IQ and replay the old mock interaction flow over the air
7. only after that, move on to handset-facing Stage 3 and later stages

The key planning rule is to avoid trying to build “full gNB” behavior in one jump. The shortest credible path is:

- stable B210 runtime under `radio_frontend`
- custom over-the-air replay of the existing mock flow
- handset-visible cell
- handset random access and `RRCSetup`
- registration
- default PDU session
- stable user-plane traffic

## 6. Maintenance Rule

This file should only track:

- implemented features
- known boundaries
- future roadmap items

Do not turn it back into a build guide, a test notebook, or a long architecture narrative.
