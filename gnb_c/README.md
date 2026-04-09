# mini-gnb-c

`gnb_c` is a minimal C11 gNB prototype for bring-up, debugging, teaching, and incremental feature work. It is not intended to model a production gNB. The project deliberately stays small and explicit: single process, single thread, single cell, single UE, mock PHY/mock RF, and a slot-driven main loop.

This document covers only four things:

- what the project is
- which dependencies it needs
- how to build it
- which run entrypoints are available

For the other topics, use the dedicated documents:

- Current features, known limits, and roadmap: `implementation_plan.md`
- Tests, validation commands, and feature evidence: `feature_test_guide.md`
- Architecture and code-reading path: `architecture.md`

## 1. Directory Overview

- `apps/`
  - executable entrypoints such as `mini_gnb_c_sim`, `mini_ue_c`, and `ngap_probe`
- `include/mini_gnb_c/`
  - public headers grouped by subsystem
- `src/`
  - subsystem implementations
- `config/`
  - YAML configuration examples
- `examples/`
  - sample inputs and reference captures
- `tests/`
  - unit and integration tests
- `build/`
  - local build output
- `out/`
  - runtime artifacts, pcaps, `tx/`, `rx/`, and summaries

## 2. Dependencies

### 2.1 Required Build Dependencies

Recommended environments:

- Linux
- WSL2 Ubuntu

Minimum build dependencies:

- a C11 compiler
  - `gcc` or `clang`
- `cmake`
- `ninja`
  - another generator can be used, but this document assumes Ninja

If your distribution packages SCTP headers or libraries separately, install the SCTP development package as well. On Ubuntu, a typical setup is:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build libsctp-dev
```

### 2.2 Optional Runtime Dependencies

These are not required for compilation, but specific features depend on them:

- `iproute2`
  - needed for the UE TUN and network-namespace demos
- `/dev/net/tun`
  - needed for end-to-end UE TUN traffic
- Open5GS
  - needed for AMF/UPF, N2, and N3 validation
- Wireshark or `tcpdump`
  - useful for NGAP, GTP-U, and `ogstun` analysis

## 3. Build

Run the standard build from `gnb_c/`:

```bash
cd gnb_c
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The main executables are:

- `./build/mini_gnb_c_sim`
- `./build/mini_ue_c`
- `./build/ngap_probe`

## 4. Common Run Modes

### 4.1 Run the gNB Prototype Only

```bash
cd gnb_c
./build/mini_gnb_c_sim
```

This runs the default mock scenario and writes artifacts to `out/`.

### 4.2 Run the Local UE <-> gNB Shared-Slot Loop

This is the recommended local closed-loop path:

```bash
cd gnb_c
./build/mini_ue_c config/example_shared_slot_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_shared_slot_gnb.yml
wait $UE_PID
```

This path is mainly used to validate:

- `PRACH -> RAR -> MSG3 -> MSG4`
- `PUCCH_SR`
- `BSR`
- `UL DATA`
- shared-slot synchronization between UE and gNB

### 4.3 Run the Open5GS End-to-End UE/gNB Scenario

If Open5GS is running locally and the sample configuration matches your AMF and UPF addresses:

```bash
cd gnb_c
./build/mini_ue_c config/example_open5gs_end_to_end_ue.yml &
UE_PID=$!
./build/mini_gnb_c_sim config/example_open5gs_end_to_end_gnb.yml
wait $UE_PID
```

This path exercises:

- minimal gNB <-> AMF NGAP control-plane connectivity
- follow-up NAS over the mock `PDSCH/PUSCH` bearer path
- PDU Session establishment
- gNB <-> UPF N3 user plane
- optional UE TUN integration
- UE-originated internet traffic

### 4.4 Run the AMF Reachability Probe

If you only want a minimal SCTP/NGAP reachability check:

```bash
cd gnb_c
./build/ngap_probe <amf-ip> 38412 5000
```

Example:

```bash
./build/ngap_probe 127.0.0.5 38412 5000
```

## 5. Runtime Outputs

The most useful outputs are:

- `out/summary.json`
  - high-level run summary, usually the best first stop
- `out/tx/`
  - exported gNB downlink objects
- `out/rx/`
  - exported UE uplink objects
- `out/gnb_core_ngap_runtime.pcap`
  - runtime NGAP capture between the gNB and AMF
- `out/gnb_core_gtpu_runtime.pcap`
  - runtime GTP-U capture between the gNB and UPF

For shared-slot plus core mode, the recommended inspection order is:

1. `out/summary.json`
2. `out/tx/` and `out/rx/`
3. `out/gnb_core_ngap_runtime.pcap`
4. `out/gnb_core_gtpu_runtime.pcap`

## 6. Manual Open5GS Validation Commands

If the UE uses a named network namespace such as `miniue-demo`, the common manual checks are:

```bash
ip netns exec miniue-demo ping -c 4 8.8.8.8
ip netns exec miniue-demo ping -c 4 www.baidu.com
ip netns exec miniue-demo curl -I --max-time 25 http://www.baidu.com
ip netns exec miniue-demo curl --max-time 30 -sS www.baidu.com -o /tmp/miniue_curl_body.html
```

If the UE uses an anonymous namespace, use the `nsenter` command printed by the UE runtime.

Recommended validation order:

1. `ping 8.8.8.8`
2. `ping www.baidu.com`
3. `curl -I`
4. `curl` with a response body

That order narrows failures efficiently:

- `ping 8.8.8.8` mainly checks the default route, N3, and host NAT
- `ping www.baidu.com` adds DNS
- `curl` is more likely to exercise larger HTTP and DNS payloads

## 7. Scope Boundary

To avoid duplication across the four `gnb_c` documents, this README intentionally does not carry the full details for:

- the detailed feature list and roadmap
- the test matrix and coverage mapping
- the architecture and reading guide

Use the dedicated files for those topics:

- `implementation_plan.md`
- `feature_test_guide.md`
- `architecture.md`
