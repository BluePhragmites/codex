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

## 3. Known Boundaries

The following items are current prototype boundaries, not accidental omissions:

- the system is still a single-UE, single-cell prototype
- the UE NAS path is still biased toward the Open5GS happy path
- `RLC-lite` is not a full 3GPP RLC AM or UM implementation
- scheduling, HARQ, MAC, and PHY are still prototype-grade mock implementations
- real internet reachability still depends on the host-side Open5GS, NAT, and DNS environment
- there is no real RF, SDR, or commercial-UE attach path yet
- there is no real RRC/MAC/PHY stack for an off-the-shelf handset

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

- pending

Goal:

- make a commercial handset able to detect the cell and read the minimum broadcast information

Main work items:

- introduce an SDR-backed radio adapter boundary without breaking the existing mock-radio path
- implement real-time sample scheduling and timestamp handling
- generate real SSB/PBCH and a valid MIB
- transmit a valid SIB1 on a real carrier configuration
- define a minimal supported deployment profile:
  - one band
  - one numerology
  - one bandwidth
  - one PLMN/TAC profile

Exit criteria:

- a commercial handset can discover the cell in cell search
- the handset can camp on the cell long enough to read MIB and SIB1
- captures from SDR and handset behavior match the configured carrier and broadcast parameters

Notes:

- this stage is still before successful attach
- it is mainly about making the cell visible and believable to real UEs

### Stage 2: Real Random Access and Minimal RRC Setup

Status:

- pending

Goal:

- make a commercial handset complete the first real access steps on the air interface

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

### Stage 3: NAS Transport and Access-Stratum Security for Real UE Attach

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

### Stage 4: Real User Plane for One Commercial Handset

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

### Stage 5: Stability, Interoperability, and Small-Lab Usability

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

### Stage 6: Expansion Beyond the First Real gNB

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

This stage should only start after Stage 4 and Stage 5 are stable enough to serve as a real baseline.

## 5. Immediate Execution Order

If the project starts moving toward commercial handset attach now, the recommended next order is:

1. freeze the current simulator and Open5GS path as the regression baseline
2. define one narrow RF target profile
   - one SDR family
   - one band
   - one bandwidth
   - one handset model
3. build Stage 1 around real SSB, PBCH, and SIB1 first
4. then implement Stage 2 random access and minimal RRC setup
5. only after that, spend effort on AS security and real-user-plane completion

The key planning rule is to avoid trying to build “full gNB” behavior in one jump. The shortest credible path is:

- visible cell
- successful random access and RRC setup
- successful registration
- successful default PDU session
- stable user-plane traffic

## 6. Maintenance Rule

This file should only track:

- implemented features
- known boundaries
- future roadmap items

Do not turn it back into a build guide, a test notebook, or a long architecture narrative.
