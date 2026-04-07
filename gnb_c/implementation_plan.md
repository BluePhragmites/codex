# gnb_c End-to-End UE/gNB/Core Plan

Last updated: 2026-04-07 (new outbound UE internet scheduling plan added)

## Completed Milestones

- [x] Stage A: Extracted reusable single-UE core/session, NGAP runtime, SCTP transport, and GTP-U helpers from `ngap_probe` into reusable library modules.
- [x] Stage B: Replaced the old filesystem-prebaked UE/gNB loop with the live shared-slot register loop, while keeping JSON NAS exchange as a fallback control-plane path.
- [x] Stage C: Completed the simulator-side gNB <-> AMF control-plane bridge, including `NGSetup`, `InitialUEMessage`, follow-up NAS relay, session-state extraction, and runtime NGAP tracing.
- [x] Stage D: Added the persistent N3 user-plane socket, minimal UE-side IPv4/ICMP handling, and simulator-side GTP-U uplink/downlink forwarding.
- [x] Stage E: Added the optional UE TUN path, live UE NAS follow-up flow, end-to-end Open5GS demo configs, runtime NGAP/GTP-U pcap tracing, and UE-side default-route/netns/DNS support for manual demos.

## Current Problem Statement

- The current TUN-based UE path can read outbound packets from the UE kernel stack, but active UE-originated traffic such as `ip netns exec <ue-netns> ping -c 4 8.8.8.8` can stall after the first TUN read.
- Root cause: the connected-mode uplink path is still too demo-oriented for sustained UE-originated traffic.
- Specifically:
  - the UE keeps only one pending TUN uplink packet instead of a queue
  - `SR` is effectively one-shot instead of being re-triggered whenever the uplink buffer becomes non-empty
  - the gNB does not yet run a sustained `SR -> BSR -> repeated UL grant` loop for UE-originated traffic

## Pending Tasks

### Stage F: Sustained UE-Originated Uplink Scheduling

- [ ] F1: Replace the single pending TUN uplink packet in `src/ue/mini_ue_runtime.c` with a small FIFO queue.
- [ ] F2: Add UE-side uplink demand state such as `sr_pending`, `bsr_dirty`, and one last reported buffer size.
- [ ] F3: Change the UE SR logic so `SR` can be sent repeatedly on valid SR occasions whenever the UE uplink queue is non-empty and no usable grant is currently available.
- [ ] F4: Keep the current simplified HARQ model, but make new UL grants consume queued packets in order instead of only one pending packet.

### Stage G: gNB-Side Continuous UL Scheduling

- [ ] G1: Add simulator-side connected-mode uplink queue accounting in `src/common/simulator.c`, starting with one `pending_ul_bytes` or equivalent per UE.
- [ ] G2: When the gNB receives `PUCCH_SR`, schedule a small `UL grant` for `BSR` instead of treating SR as a one-shot demo event.
- [ ] G3: When the gNB receives `BSR`, store the reported bytes and keep issuing `DCI0_1 + UL DATA grant` while bytes remain pending.
- [ ] G4: On each successful `UL DATA`, decrement the pending bytes and only stop uplink scheduling when the UE queue is empty.
- [ ] G5: Remove reliance on the current “downlink packet arrived, so auto-queue one follow-up UL grant” shortcut for the UE-originated internet case.

### Stage H: Validation and Regression Coverage

- [ ] H1: Add unit coverage for the new UE uplink queue behavior and repeated-SR state transitions.
- [ ] H2: Add integration coverage showing that one outbound packet read from TUN is eventually forwarded by the gNB into the persistent N3 socket.
- [ ] H3: Add integration coverage for multi-packet UE-originated traffic so the queue and repeated grants do not stop after one packet.
- [ ] H4: Update `README.md`, `architecture.md`, and `feature_test_guide.md` with the new sustained uplink behavior and validation commands.

### Stage I: Manual Open5GS Validation

- [ ] I1: Re-run the manual Open5GS demo with `ip netns exec <ue-netns> ping -c 4 8.8.8.8`.
- [ ] I2: Re-run the same demo with `ip netns exec <ue-netns> ping -c 4 www.baidu.com`.
- [ ] I3: Confirm the NGAP and GTP-U runtime pcaps show outbound UE-originated traffic reaching the UPF and receiving the return path.

## Recommended Implementation Order

1. Implement the UE-side TUN uplink queue and repeated-SR logic.
2. Implement the gNB-side `SR -> BSR -> repeated UL DATA grant` loop.
3. Add tests for single-packet and multi-packet UE-originated uplink traffic.
4. Update the documentation and test guide.
5. Re-run the manual Open5GS end-to-end validation.
