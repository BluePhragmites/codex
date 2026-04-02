# gnb_c End-to-End UE/gNB/Core Plan

Last updated: 2026-04-02 (Stage B1 complete)

## Completed Tasks

- [x] 2026-04-02: Reviewed the current `mini_gnb_c_sim` and `ngap_probe` architecture and mapped the staged implementation order.
- [x] 2026-04-02: Created this tracked implementation plan file with explicit completed and pending task sections.
- [x] 2026-04-02: Extracted reusable single-UE core session helpers into `include/mini_gnb_c/core/core_session.h` and `src/core/core_session.c`.
- [x] 2026-04-02: Extracted reusable GTP-U packet builders and validators into `include/mini_gnb_c/n3/gtpu_tunnel.h` and `src/n3/gtpu_tunnel.c`.
- [x] 2026-04-02: Updated `apps/ngap_probe.c` to consume the extracted Stage A session and GTP-U helpers.
- [x] 2026-04-02: Added unit coverage for the new helpers in `tests/test_core_session.c` and `tests/test_gtpu_tunnel.c`, and kept `ctest --test-dir build --output-on-failure` passing.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the Stage A reusable modules and the tracked implementation plan.
- [x] 2026-04-02: Created a local Stage A milestone commit: `Extract reusable core session and GTP-U helpers`.
- [x] 2026-04-02: Added the Stage B1 JSON exchange helper in `include/mini_gnb_c/link/json_link.h` and `src/link/json_link.c`.
- [x] 2026-04-02: Added `tests/test_json_link.c` and kept `ctest --test-dir build --output-on-failure` passing after the Stage B1 JSON exchange work.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the Stage B1 local JSON exchange foundation.

## Pending Tasks

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
