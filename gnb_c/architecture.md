# gnb_c Architecture and Reading Guide

This document answers two questions:

1. how the codebase is organized at a high level
2. where a new reader should start

It does not try to duplicate the full build guide or the full test matrix.

- Build and run entrypoints: `README.md`
- Feature status and roadmap: `implementation_plan.md`
- Test matrix and validation commands: `feature_test_guide.md`

## 1. Design Goals

The core design goals of `gnb_c` are:

- single process
- single thread
- single cell
- single UE
- mock PHY and mock RF
- a single slot-driven main loop

In practice, this means the system is not built around multiple threads, message queues, or a large asynchronous runtime. It is a slot-driven prototype designed to be easy to inspect, debug, and extend incrementally.

## 2. Top-Level Layout and Runtime Modes

### 2.1 Top-Level Directories

- `apps/`
  - executable entrypoints
- `include/mini_gnb_c/`
  - public headers by subsystem
- `src/`
  - implementations
- `config/`
  - YAML examples
- `examples/`
  - sample inputs and captures
- `tests/`
  - unit and integration tests

### 2.2 The Main Entry Programs

- `apps/mini_gnb_c_sim.c`
  - main gNB entrypoint
  - now has two runtime branches:
    - the existing slot-driven simulator path, which can now use either the mock radio backend or the hybrid B210 backend under `radio_frontend`
    - a B210/UHD app harness selected by YAML `rf.runtime_mode`
- `apps/mini_ue_c.c`
  - local UE runtime entrypoint
  - now has two runtime branches:
    - the existing JSON/shared-slot UE path
    - the same B210/UHD app harness selected by YAML `rf.runtime_mode`
- `apps/ngap_probe.c`
  - standalone Open5GS and AMF/UPF probe
- `apps/mini_b210_probe_c.c`
  - standalone Stage 1 B210/UHD RX/TX smoke probe
- `apps/mini_ring_inspect.c`
  - inspects single-file `sc16` recent-history rings
- `apps/mini_ring_export.c`
  - exports a selected inclusive `seq` range into per-channel `sc16` files

### 2.3 Radio Backend Boundary

The simulator no longer reaches directly into `mock_radio_frontend` as its public radio abstraction. The active boundary is now:

- `src/radio/radio_frontend.c`
- `include/mini_gnb_c/radio/radio_frontend.h`

Today this boundary routes to:

- the existing mock backend
- a new hybrid B210 backend

The first full real-air-interface target still remains:

- `uhd-b210`

That split is deliberate. It allows Stage 1 work to proceed toward a B210/UHD backend without treating the current simulator path as if it were already a real RF implementation.

The first hardware-facing C module for that roadmap is now:

- `src/radio/air_pdu.c`
- `include/mini_gnb_c/radio/air_pdu.h`
- `src/radio/b210_app_runtime.c`
- `include/mini_gnb_c/radio/b210_app_runtime.h`
- `src/radio/b210_uhd_probe.c`
- `include/mini_gnb_c/radio/b210_uhd_probe.h`
- `src/radio/host_performance.c`
- `include/mini_gnb_c/radio/host_performance.h`
- `src/radio/sc16_ring_export.c`
- `include/mini_gnb_c/radio/sc16_ring_export.h`
- `src/radio/sc16_ring_map.c`
- `include/mini_gnb_c/radio/sc16_ring_map.h`

The standalone smoke path is intentionally still kept available beside the simulator backend. It proves host-side UHD access, stress behavior, and ring-map tooling independently of the simulator.

`air_pdu.c` is the first Stage 2A protocol module. It is intentionally narrower than a waveform mapper:

- it defines the shared binary contract for future gNB/UE over-the-air payloads
- it covers PDU typing, slot tagging, optional preamble context, payload length, and CRC
- it does not yet map those PDUs onto actual IQ waveforms

The `b210_app_runtime` bridge is the layer that connects that smoke/runtime path to the main app entrypoints:

- `mini_gnb_c_sim`
- `mini_ue_c`

It does not turn the simulator into a real-air-interface backend. It only lets the main apps reuse the already-working B210 runtime through the shared YAML `rf:` section.

The new simulator-facing layer for that next step is:

- `src/radio/b210_slot_backend.c`
- `include/mini_gnb_c/radio/b210_slot_backend.h`

This backend is intentionally hybrid:

- it embeds the existing mock slot semantics used by the simulator
- it starts long-lived B210 RX and TX workers inside `radio_frontend`
- it captures real B210 RX IQ into the configured RX ring map
- it turns slot-mapped DL patches into a toy TX waveform and streams that through the configured TX ring map

What it does not do yet:

- decode `PRACH`, `MSG3`, `PUCCH`, or `UL DATA` from B210 IQ
- generate a real NR-compliant DL waveform

Today that smoke path covers:

- one RX burst captured to file
- one TX burst replayed from file
- optional RX capture into a fixed-size single-file `sc16` ring map
- optional TX replay from that same ring-map format
- optional simultaneous TRX with one RX worker and one TX worker
- optional one-channel or two-channel operation
- one shared gain shortcut plus direction-specific TRX gain overrides:
  - `--gain`
  - `--rx-gain`
  - `--tx-gain`
- one built-in host tuning pass before radio startup
- explicit CPU pinning for the active worker thread
- per-block RX time anchors taken from UHD RX metadata when available
- standalone TX replay already uses a larger user-space prefetch window and a low-watermark refill policy for ring-map replay
- that prefetch window can be overridden from the CLI with `--tx-prefetch-samples`
- the `trx` TX worker reuses the same policy instead of replaying directly from mapped ring payload pointers
- the `trx` path now also separates RX and TX center-frequency control
  - `--freq` remains the shared shortcut
  - `--rx-freq` and `--tx-freq` override each side independently
- the `trx` TX side now loops its source ring when `rate * duration` is longer than one ring pass
- the RX side now has an explicit error policy for stress runs
  - `OVERFLOW` and `TIMEOUT` are counted and the worker keeps running
  - structure-breaking metadata errors and API failures still stop the worker
- RX observability now includes gap-derived loss estimation
  - successful RX chunks are compared in hardware time
  - discontinuities increase `rx_gap_events` and `rx_lost_samples_estimate`
- the RX-side duration semantics are selectable
  - `samples` mode targets successful-sample count
  - `wallclock` mode targets elapsed time
- the shared RF config also carries the same future runtime knob as `rf.tx_prefetch_samples`
- the standalone B210 probe can now preload its defaults from the same shared `rf` section through `--config <yaml>`
- the preload order is fixed:
  - shared YAML `rf` defaults first
  - probe CLI overrides second
- the shared RF config now also carries `freq_hz`, `rx_freq_hz`, and `tx_freq_hz` so the probe and future backend do not need probe-only frequency sources

The host tuning step is intentionally narrower than the full `srsran_performance` script:

- it reuses the B210-relevant parts
  - CPU governor to `performance`
  - DRM KMS polling disabled
- it intentionally skips Ethernet socket-buffer tuning
  - because the current target radio is the USB-based B210

The build also treats radio-facing code as a separate performance domain:

- the top-level build may stay in `Debug`
- radio-facing sources default to a release-style compile profile
- that default can be turned off explicitly when radio debugging is the goal

It still does not provide the final handset-capable PHY architecture. The intended next step is to replace the remaining mock UL semantics and toy DL waveform generation with real PHY mapping, synchronization, and decoding.

The ring-map format is a probe-time observability and buffering aid, not the final steady-state runtime queue. Its internal layout is:

- superblock
- descriptor ring
- payload ring

That keeps wrap metadata and pure IQ payload in one file while avoiding per-block headers inside the actual sample stream.

For two-channel operation, each block stores payload in channel-major order:

- channel 0 region first
- channel 1 region second

That matches the current analysis goal of reading one antenna’s block payload before the second antenna’s block payload.

Each ready descriptor also carries one shared block timestamp:

- `hw_time_ns`
  - the first-sample time anchor for that block
  - sourced from UHD RX metadata when available
- `flags`
  - indicate whether that time came from UHD hardware time or a host fallback

Two runtime knobs should not be confused:

- `duration`
  - controls how much data the current probe run attempts to receive
- ring geometry
  - `ring_block_samples`
  - `ring_block_count`
  - controls how much recent history remains in the fixed-size map

## 3. Main Control Flow

### 3.1 The gNB Main Loop

The most important top-level orchestrator is:

- `src/common/simulator.c`

It is responsible for:

- advancing one slot at a time
- driving the mock radio front end
- handling random access
- running scheduling decisions
- advancing UE context state
- driving the core bridge
- driving the N3 user plane

This file is the single-threaded orchestration center of the entire system.

### 3.2 The Local UE <-> gNB Shared-Slot Link

The main local coordination path between the UE and gNB is:

- `include/mini_gnb_c/link/shared_slot_link.h`
- `src/link/shared_slot_link.c`

Its responsibilities are:

- the gNB publishes a downlink summary for each slot
- the UE observes that summary and schedules future uplink actions
- the UE writes its own slot progress back to the link

This path is the core of the local UE/gNB closed loop.

### 3.3 Real-Cell Target Profile

The config layer now carries an explicit `real_cell` section. Its role is not to change the current simulator behavior. Its role is to lock the first real-air-interface target profile in one place:

- backend target
- band
- ARFCN
- numerology
- bandwidth
- PLMN/TAC

For the current roadmap, that first target is the B210/UHD path.

The RF config also now carries the first host-runtime controls needed for real radio work:

- `rf.subdev`
- `rf.rx_cpu_core`
- `rf.tx_cpu_core`

The intended steady-state design is:

- one dedicated RX worker pinned to `rf.rx_cpu_core`
- one dedicated TX worker pinned to `rf.tx_cpu_core`
- the main control thread remaining separate from both real-time workers

The current app-harness path is therefore still an intermediate step:

- shared RF config already lives in the main YAML files
- main app entrypoints can already consume that config for B210 RX/TX/TRX runs
- the simulator now has that first true slot-aware `radio_frontend` B210 integration layer
- the remaining step is to replace the hybrid mock semantics inside that backend with real RF/PHY behavior

### 3.4 Radio Access and Scheduling Modules

The radio side is easiest to understand as a small set of layers:

- broadcast and baseline downlink helpers
  - `src/broadcast/`
  - `src/phy_dl/`
- random access
  - `src/prach/`
  - `src/ra/`
  - Msg3 handling in `src/uplink/`
- scheduling
  - `src/scheduler/`
- UE state
  - `src/ue/ue_context_store.c`

For the minimal gNB prototype, the key loop is:

1. PRACH detection
2. RAR scheduling
3. Msg3 decode
4. UE promotion
5. Msg4 and connected-mode scheduling

## 4. UE-Side Architecture

### 4.1 The Role of `mini_ue_c`

`mini_ue_c` currently serves two purposes:

- running a live UE runtime in shared-slot mode
- preserving compatibility with the older local file-based fallback path

The main UE logic is in:

- `src/ue/mini_ue_runtime.c`

### 4.2 What the UE Runtime Does

The UE runtime is responsible for:

- learning timing from SIB1, RAR, and Msg4
- scheduling `PRACH`, `MSG3`, and `PUCCH_SR`
- maintaining the uplink FIFO
- maintaining `BSR` and `SR` state
- handling `payload_kind=NAS`
- handling `payload_kind=IPV4`
- integrating the optional TUN path

If you want to understand why the local UE transmits a given uplink object in a specific slot, start here.

### 4.3 UE User-Plane Layers

The current UE user plane is easiest to think of as three layers:

- minimal IP and ICMP helpers
  - `src/ue/ue_ip_stack_min.c`
- optional TUN integration
  - `src/ue/ue_tun.c`
- bearer segmentation and reassembly for larger IPv4 payloads
  - `src/rlc/rlc_lite.c`

`RLC-lite` has a narrow role:

- it allows an IPv4 SDU larger than one grant to cross the bearer path
- it is not a full 3GPP RLC AM or UM implementation

## 5. Control Plane and Core-Network Bridge

### 5.1 The gNB Core Bridge

The control-plane center is:

- `src/core/gnb_core_bridge.c`

It is responsible for:

- creating the minimal NGAP connection to the AMF
- sending `NGSetup`
- sending `InitialUEMessage`
- forwarding follow-up `UplinkNASTransport`
- handling `InitialContextSetup` and `PDUSessionResourceSetup`
- storing key session state

If your question is how the prototype talks to Open5GS AMF, start here.

### 5.2 Supporting Control-Plane Modules

The lower-level support modules are:

- `src/ngap/ngap_runtime.c`
- `src/ngap/ngap_transport.c`
- `src/core/core_session.c`

Their roles are:

- minimal NGAP message encoding and parsing
- SCTP transport wrapping
- single-UE session-state storage

## 6. User Plane and N3

### 6.1 N3 User Plane

The most important modules are:

- `src/n3/n3_user_plane.c`
- `src/n3/gtpu_tunnel.c`

They are responsible for:

- creating the persistent N3 UDP socket
- encapsulating uplink IPv4 payloads into GTP-U
- decapsulating downlink GTP-U packets
- feeding the inner IPv4 packets back into the gNB and UE data path

### 6.2 Larger Bearer Payloads

Larger HTTP, DNS, and IPv4 packets are not pushed into one mock `PDSCH` or `PUSCH` object. Instead:

- the sender segments them based on the grant `tbsize`
- the receiver reassembles them
- this applies mainly to larger `payload_kind=IPV4` payloads

The relevant modules are:

- `src/rlc/rlc_lite.c`
- `src/ue/mini_ue_runtime.c`
- `src/common/simulator.c`

## 7. Observability as Part of the Architecture

This project intentionally produces rich runtime artifacts because bring-up and debugging are primary use cases.

The main observation surfaces are:

- `out/summary.json`
- `out/tx/`
- `out/rx/`
- runtime NGAP pcaps
- runtime GTP-U pcaps

When reading the code, treat these outputs as part of the design rather than as optional extras.

## 8. Recommended Reading Order

If you are new to the project, do not start with low-level details. Read in this order instead.

### Step 1: Read the Entrypoints

Start with:

- `apps/mini_gnb_c_sim.c`
- `apps/mini_ue_c.c`
- `apps/ngap_probe.c`
- `apps/mini_b210_probe_c.c`

Goal:

- understand which runtime modes exist

### Step 2: Read the Main Orchestrator

Then read:

- `src/common/simulator.c`

Goal:

- understand the global slot-by-slot execution order
- understand the top-level system flow

### Step 3: Read the Shared-Slot Path

Then read:

- `src/radio/radio_frontend.c`
- `src/link/shared_slot_link.c`
- `src/radio/mock_radio_frontend.c`
- `src/radio/b210_uhd_probe.c`
- `src/ue/mini_ue_runtime.c`

Goal:

- understand how the local gNB and UE stay aligned in time
- understand where the future B210/UHD backend will plug in

### Step 4: Read Random Access and Scheduling

Then read:

- `src/ra/`
- `src/scheduler/`
- `src/uplink/`

Goal:

- understand how the UE is promoted from random access into connected mode

### Step 5: Read the Core-Network Bridge

Then read:

- `src/core/gnb_core_bridge.c`
- `src/ngap/ngap_runtime.c`
- `src/core/core_session.c`

Goal:

- understand how the prototype reaches the AMF and how session state is learned

### Step 6: Read the N3 and User-Plane Path

Then read:

- `src/n3/n3_user_plane.c`
- `src/n3/gtpu_tunnel.c`
- `src/ue/ue_tun.c`
- `src/rlc/rlc_lite.c`

Goal:

- understand how the prototype carries user-plane traffic end to end

### Step 7: Read the Tests Alongside the Modules

Finally, use:

- `tests/test_integration.c`
- `tests/test_mini_ue_runtime.c`
- `tests/test_rlc_lite.c`
- `tests/test_ngap_runtime.c`

Goal:

- confirm the intended behavior of each subsystem with executable evidence

## 9. Maintenance Rule

This file should only maintain:

- the architecture overview
- module responsibilities and boundaries
- the recommended reading order

Do not turn it back into a build guide, a roadmap document, or a generic test notebook.
