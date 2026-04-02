# gnb_c End-to-End UE/gNB/Core Plan

Last updated: 2026-04-02 (Stage C5 complete; reference captures moved under examples/)

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
- [x] 2026-04-02: Added the Stage B2 standalone UE app in `apps/mini_ue_c.c`.
- [x] 2026-04-02: Added the reusable UE event FSM in `include/mini_gnb_c/ue/mini_ue_fsm.h` and `src/ue/mini_ue_fsm.c`.
- [x] 2026-04-02: Added `tests/test_mini_ue_fsm.c` and kept `ctest --test-dir build --output-on-failure` passing after the Stage B2 UE process work.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the Stage B2 standalone UE process and FSM.
- [x] 2026-04-02: Connected `mini_gnb_c_sim` and `mock_radio_frontend` to consume ordered `ue_to_gnb/*.json` events from `sim.local_exchange_dir`.
- [x] 2026-04-02: Added the Stage B3 local-loop integration coverage in `tests/test_integration.c` so PRACH, Msg3, SR, BSR, and UL DATA can run from the UE event plan without handcrafted slot input files.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the Stage B3 and Stage B4 filesystem-backed UE/gNB loop.
- [x] 2026-04-02: Created a local Stage B milestone commit for the filesystem-backed UE/gNB loop after validating build, `ctest`, and a manual `mini_ue_c -> mini_gnb_c_sim` run.
- [x] 2026-04-02: Extended `mini_gnb_c_ue_context_t` with embedded `core_session` state so promoted UEs now have a stable place for upcoming AMF/session/N3 bridge data.
- [x] 2026-04-02: Added `tests/test_ue_context_store.c` plus integration assertions for the exported `core_session` summary shape, and kept `ctest --test-dir build --output-on-failure` passing.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the Stage C groundwork for UE context and summary state export.
- [x] 2026-04-02: Extracted reusable NGAP runtime builders and Open5GS session parsers into `include/mini_gnb_c/ngap/ngap_runtime.h` and `src/ngap/ngap_runtime.c`.
- [x] 2026-04-02: Updated `apps/ngap_probe.c` to consume the extracted NGAP runtime helpers instead of owning those builders/parsers inline.
- [x] 2026-04-02: Added `tests/test_ngap_runtime.c` and kept `ctest --test-dir build --output-on-failure` passing after the Stage C1 NGAP runtime extraction.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the Stage C1 NGAP runtime extraction.
- [x] 2026-04-02: Added `core/gnb_core_bridge` and `core:` config wiring so the simulator can prepare a reusable `InitialUEMessage` when a UE is promoted.
- [x] 2026-04-02: Wired `mini_gnb_c_sim` to seed `ran_ue_ngap_id`, requested `pdu_session_id`, and `uplink_nas_count` through the new core bridge when `core.enabled=true`.
- [x] 2026-04-02: Added `tests/test_gnb_core_bridge.c` plus integration coverage for the simulator-side core bridge path, and kept `ctest --test-dir build --output-on-failure` passing.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the simulator-side core bridge groundwork.
- [x] 2026-04-02: Added the reusable SCTP NGAP transport wrapper in `include/mini_gnb_c/ngap/ngap_transport.h` and `src/ngap/ngap_transport.c`.
- [x] 2026-04-02: Extended `core:` config with `timeout_ms` and wired the simulator to pass `sim.local_exchange_dir` into the core bridge.
- [x] 2026-04-02: Extended `ngap/ngap_runtime` with reusable `NGSetupRequest`, `DownlinkNASTransport`, and `NAS-PDU` extraction helpers.
- [x] 2026-04-02: Advanced `core/gnb_core_bridge` from prepared-only state to a real first-hop AMF bridge that runs `NGSetup`, sends one `InitialUEMessage`, parses `AMF UE NGAP ID`, and captures the first downlink NAS.
- [x] 2026-04-02: Emitted the first AMF downlink NAS into `gnb_to_ue/*.json` when `sim.local_exchange_dir` is configured.
- [x] 2026-04-02: Added fake-transport unit and integration coverage for the live bridge path, and kept `ctest --test-dir build --output-on-failure` passing.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the first live SCTP/NGAP bridge slice.
- [x] 2026-04-02: Extracted shared JSON field and event-lookup helpers so the radio path and the core bridge can reuse the same local exchange parsing logic.
- [x] 2026-04-02: Added follow-up `ue_to_gnb_nas/UL_NAS` polling to `core/gnb_core_bridge` and relayed those events as `UplinkNASTransport` beyond the first `InitialUEMessage`.
- [x] 2026-04-02: Emitted subsequent AMF downlink NAS messages back into `gnb_to_ue/*.json` for the local control-plane workflow.
- [x] 2026-04-02: Added unit and integration coverage for the follow-up NAS relay path, and updated `gnb_c/README.md` and `gnb_c/architecture.md` for the Stage C4 control-plane workflow.
- [x] 2026-04-02: Extended `core/gnb_core_bridge` to recognize `InitialContextSetupRequest` and `PDUSessionResourceSetupRequest`, send the matching gNB NGAP responses, and keep the simulator slot loop synchronous.
- [x] 2026-04-02: Parsed `ue_ipv4`, `upf_teid`, and `qfi` from later Open5GS session-setup messages into the simulator-side `core_session` path.
- [x] 2026-04-02: Added unit and integration coverage for session-setup state extraction, summary export, and automatic NGAP acknowledgements.
- [x] 2026-04-02: Updated `gnb_c/README.md` and `gnb_c/architecture.md` to document the Stage C5 session-state extraction workflow.
- [x] 2026-04-02: Moved the checked-in `gnb_ngap.pcap` and `gnb_mac.pcap` reference captures into `gnb_c/examples/` and updated runtime/documentation references.

## Pending Tasks

- [ ] Stage C6: Add the next control-plane milestone tests and docs for session-setup state extraction and later AMF procedures beyond simple NAS relay.
- [ ] Stage C7: Create a local milestone commit after the control plane is stable.
- [ ] Stage D1: Add a persistent N3 user-plane helper that maintains a live GTP-U socket instead of one-shot probe traffic.
- [ ] Stage D2: Add minimal UE-side IPv4/ICMP user-plane handling.
- [ ] Stage D3: Add tests and docs for the minimal user-plane path.
- [ ] Stage D4: Create a local milestone commit after the user-plane path is stable.
- [ ] Stage E1: Add optional TUN-based UE integration for end-to-end `ping` validation.
- [ ] Stage E2: Add end-to-end validation docs and artifact expectations for the full `server -> UPF -> gNB -> UE` path.
- [ ] Stage E3: Create a local milestone commit after the full end-to-end path is stable and only push to GitHub when explicitly approved.
