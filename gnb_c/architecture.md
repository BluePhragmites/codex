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

### 2.2 The Three Main Entry Programs

- `apps/mini_gnb_c_sim.c`
  - main gNB entrypoint
- `apps/mini_ue_c.c`
  - local UE runtime entrypoint
- `apps/ngap_probe.c`
  - standalone Open5GS and AMF/UPF probe

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

### 3.3 Radio Access and Scheduling Modules

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

- `src/link/shared_slot_link.c`
- `src/radio/mock_radio_frontend.c`
- `src/ue/mini_ue_runtime.c`

Goal:

- understand how the local gNB and UE stay aligned in time

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
