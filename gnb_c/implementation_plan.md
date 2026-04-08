# gnb_c End-to-End UE/gNB/Core Plan

Last updated: 2026-04-08 (all staged A-I work completed; manual Open5GS end-to-end validation passed)

## Completed Milestones

- [x] Stage A: Extracted reusable single-UE core/session, NGAP runtime, SCTP transport, and GTP-U helpers from `ngap_probe` into reusable library modules.
- [x] Stage B: Replaced the old filesystem-prebaked UE/gNB loop with the live shared-slot register loop, while keeping JSON NAS exchange as a fallback control-plane path.
- [x] Stage C: Completed the simulator-side gNB <-> AMF control-plane bridge, including `NGSetup`, `InitialUEMessage`, follow-up NAS relay, session-state extraction, and runtime NGAP tracing.
- [x] Stage D: Added the persistent N3 user-plane socket, minimal UE-side IPv4/ICMP handling, and simulator-side GTP-U uplink/downlink forwarding.
- [x] Stage E: Added the optional UE TUN path, live UE NAS follow-up flow, end-to-end Open5GS demo configs, runtime NGAP/GTP-U pcap tracing, and UE-side default-route/netns/DNS support for manual demos.
- [x] Stage F: Replaced the single pending UE uplink packet with a FIFO-backed demand queue, added repeated-SR / BSR state, and made new UL grants consume queued payloads in order.
- [x] Stage G: Added gNB-side connected-mode pending-byte accounting, repeated SR acceptance, sequential UL payload grants, and removed the N3-triggered follow-up grant shortcut.
- [x] Stage H1: Added unit coverage for UE queue accounting, repeated-SR state, and the “no queued payload means no fabricated UL DATA” behavior.
- [x] Stage H2: Added integration coverage proving that one packet read from the UE TUN device is forwarded into the persistent N3 socket.
- [x] Stage H3: Added integration coverage for multi-packet UE-originated traffic so the queue and repeated grants do not stop after one packet.
- [x] Stage H4: Updated the user-facing docs and test guide for the sustained connected-mode uplink path.
- [x] Stage I1: Re-ran the manual Open5GS demo and confirmed real UE-side `ping -c 4 8.8.8.8` echo replies.
- [x] Stage I2: Re-ran the DNS-capable UE-side demo and confirmed real `ping -c 4 www.baidu.com` name resolution plus echo replies.
- [x] Stage I3: Confirmed the runtime `NGAP` / `GTP-U` pcaps show both the outbound UE-originated traffic and the returned downlink path.

## Current Problem Statement

- The simulator now completes the sustained connected-mode `SR -> BSR -> repeated UL grant` loop for live UE-originated traffic, advertises a matching gNB N3 endpoint in `PDUSessionResourceSetupResponse`, binds the persistent gNB GTP-U socket on the standard local UDP/2152 endpoint, and receives the returned downlink GTP-U path from Open5GS.
- The staged end-to-end goal is complete for the current local Open5GS topology:
  - on 2026-04-08, the manual re-run confirmed `ping -c 4 8.8.8.8` and `ping -c 4 www.baidu.com` both succeeded inside the UE namespace
  - the runtime `gnb_core_gtpu_runtime.pcap` captured the matching bidirectional `127.0.0.1:2152 <-> 127.0.0.7:2152` GTP-U traffic
  - the companion `ogstun` / `enp4s0` captures showed the same UE-originated ICMP and DNS traffic traversing the host data plane and returning successfully

## Pending Tasks

### Stage F: Sustained UE-Originated Uplink Scheduling

- [x] F1: Replace the single pending TUN uplink packet in `src/ue/mini_ue_runtime.c` with a small FIFO queue.
- [x] F2: Add UE-side uplink demand state such as `sr_pending`, `bsr_dirty`, and one last reported buffer size.
- [x] F3: Change the UE SR logic so `SR` can be sent repeatedly on valid SR occasions whenever the UE uplink queue is non-empty and no usable grant is currently available.
- [x] F4: Keep the current simplified HARQ model, but make new UL grants consume queued packets in order instead of only one pending packet.

### Stage G: gNB-Side Continuous UL Scheduling

- [x] G1: Add simulator-side connected-mode uplink queue accounting in `src/common/simulator.c`, starting with one `pending_ul_bytes` or equivalent per UE.
- [x] G2: When the gNB receives `PUCCH_SR`, schedule a small `UL grant` for `BSR` instead of treating SR as a one-shot demo event.
- [x] G3: When the gNB receives `BSR`, store the reported bytes and keep issuing `DCI0_1 + UL DATA grant` while bytes remain pending.
- [x] G4: On each successful `UL DATA`, decrement the pending bytes and only stop uplink scheduling when the UE queue is empty.
- [x] G5: Remove reliance on the current “downlink packet arrived, so auto-queue one follow-up UL grant” shortcut for the UE-originated internet case.

### Stage H: Validation and Regression Coverage

- [x] H1: Add unit coverage for the new UE uplink queue behavior and repeated-SR state transitions.
- [x] H2: Add integration coverage showing that one outbound packet read from TUN is eventually forwarded by the gNB into the persistent N3 socket.
- [x] H3: Add integration coverage for multi-packet UE-originated traffic so the queue and repeated grants do not stop after one packet.
- [x] H4: Update `README.md`, `architecture.md`, and `feature_test_guide.md` with the new sustained uplink behavior and validation commands.

### Stage I: Manual Open5GS Validation

- [x] I1: Re-run the manual Open5GS demo with `ip netns exec <ue-netns> ping -c 4 8.8.8.8` or the rootless `nsenter --preserve-credentials -S 0 -G 0 -U -n -t <ue-pid> ...` fallback, and get real echo replies back.
- [x] I2: Re-run the same demo with the corresponding DNS-capable namespace command (`ip netns exec ...` or `nsenter --preserve-credentials -S 0 -G 0 -U -n -m -t <ue-pid> ...`) and get real name resolution plus echo replies back.
- [x] I3: Confirm the NGAP and GTP-U runtime pcaps show outbound UE-originated traffic reaching the UPF and receiving the return path.

## Recommended Implementation Order

1. Keep the current local Open5GS demo as the regression reference for future changes.
2. If broader interoperability is needed, generalize the current minimal NAS/user-plane assumptions beyond the present Open5GS happy path.
3. If CI automation becomes necessary later, add a privileged end-to-end harness around the existing manual Open5GS validation flow.
