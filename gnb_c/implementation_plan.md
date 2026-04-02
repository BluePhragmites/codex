# gnb_c End-to-End UE/gNB/Core Plan

Last updated: 2026-04-02

## Completed Tasks

- [x] 2026-04-02: Reviewed the current `mini_gnb_c_sim` and `ngap_probe` architecture and mapped the staged implementation order.
- [x] 2026-04-02: Created this tracked implementation plan file with explicit completed and pending task sections.

## Pending Tasks

- [ ] Stage A1: Extract reusable single-UE core session state helpers from `apps/ngap_probe.c` into `mini_gnb_c_lib`.
- [ ] Stage A2: Extract reusable GTP-U packet builders and validators from `apps/ngap_probe.c` into `mini_gnb_c_lib`.
- [ ] Stage A3: Update `apps/ngap_probe.c` to use the new reusable session and GTP-U helpers without regressing current replay behavior.
- [ ] Stage A4: Add unit tests for the new reusable session and GTP-U helpers and wire them into `mini_gnb_c_tests`.
- [ ] Stage A5: Update `gnb_c/README.md` and `gnb_c/architecture.md` to document the new reusable Stage A modules and the tracked implementation progress.
- [ ] Stage A6: Create a local milestone commit after Stage A code, tests, and docs pass.
- [ ] Stage B1: Add a tracked local exchange layer for UE <-> gNB control/data events with atomic JSON file writes.
- [ ] Stage B2: Implement a minimal single-UE FSM process in a new `apps/mini_ue_c.c`.
- [ ] Stage B3: Connect `mini_gnb_c_sim` to the local UE exchange so Msg1/Msg3/SR/BSR/UL-DATA can run without handcrafted example inputs.
- [ ] Stage B4: Add tests and docs for the local UE <-> gNB loop.
- [ ] Stage B5: Create a local milestone commit after the local UE/gNB loop is stable.
- [ ] Stage C1: Introduce a gNB-to-core bridge that reuses extracted NGAP/NAS helpers and attaches the promoted UE context to AMF session state.
- [ ] Stage C2: Extend the UE context and run summary with AMF/session/N3 state such as `ran_ue_ngap_id`, `amf_ue_ngap_id`, `pdu_session_id`, `ue_ipv4`, `teid`, and `qfi`.
- [ ] Stage C3: Bridge UE NAS uplinks and AMF NAS downlinks through the gNB process.
- [ ] Stage C4: Add tests and docs for the control-plane bridge.
- [ ] Stage C5: Create a local milestone commit after the control plane is stable.
- [ ] Stage D1: Add a persistent N3 user-plane helper that maintains a live GTP-U socket instead of one-shot probe traffic.
- [ ] Stage D2: Add minimal UE-side IPv4/ICMP user-plane handling.
- [ ] Stage D3: Add tests and docs for the minimal user-plane path.
- [ ] Stage D4: Create a local milestone commit after the user-plane path is stable.
- [ ] Stage E1: Add optional TUN-based UE integration for end-to-end `ping` validation.
- [ ] Stage E2: Add end-to-end validation docs and artifact expectations for the full `server -> UPF -> gNB -> UE` path.
- [ ] Stage E3: Create a local milestone commit after the full end-to-end path is stable and only push to GitHub when explicitly approved.
