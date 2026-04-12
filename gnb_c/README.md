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

The current baseline still remains a teaching prototype, but the simulator path no longer requires `rf.device_driver: "mock"` only. `mini_gnb_c_sim` can now also run through a hybrid B210-backed `radio_frontend` while keeping the existing mock slot semantics for RA, scheduling, and UL burst decisions. A separate optional `real_cell` config section remains available to lock the first real-air-interface target profile around `uhd-b210`.

The codebase also now includes the first Stage 2A protocol foundation for future B210-over-the-air replay:

- a minimal air-PDU binary contract in `src/radio/air_pdu.c`
- matching public declarations in `include/mini_gnb_c/radio/air_pdu.h`

This is an internal foundation step, not a new user-facing run mode yet.

## 1. Directory Overview

- `apps/`
  - executable entrypoints such as `mini_gnb_c_sim`, `mini_ue_c`, `ngap_probe`, and `mini_b210_probe_c`
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
- `pkg-config`
  - required if you want CMake to auto-detect UHD and build `mini_b210_probe_c`

If your distribution packages SCTP headers or libraries separately, install the SCTP development package as well. On Ubuntu, a typical setup is:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config libsctp-dev
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
- UHD plus a USRP B210
  - not required for the current release baseline, but this is the first planned real-air-interface target for Stage 1
  - `mini_b210_probe_c` is built only when `pkg-config --cflags --libs uhd` succeeds
  - for high-rate smoke capture, the current recommended output target is `/dev/shm`

### 2.3 Radio Build Profile

The build now separates the radio-facing compile profile from the top-level CMake build type:

- `MINI_GNB_C_RADIO_FORCE_RELEASE=ON`
  - default
  - builds radio-facing code with release-style optimization even when the main build type is `Debug`
- `MINI_GNB_C_RADIO_FORCE_DEBUG=ON`
  - opt-in
  - forces radio-facing code back to a debug-style build profile

This is meant to keep high-rate UHD paths closer to their real runtime behavior without forcing the whole project out of `Debug`.

## 3. Build

Run the standard build from `gnb_c/`:

```bash
cd gnb_c
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

If you explicitly want the whole tree in `Debug`, including the radio-facing code:

```bash
cd gnb_c
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMINI_GNB_C_RADIO_FORCE_RELEASE=OFF -DMINI_GNB_C_RADIO_FORCE_DEBUG=ON
cmake --build build
```

The main executables are:

- `./build/mini_gnb_c_sim`
  - can now switch from the mock simulator path into a B210/UHD app harness through YAML `rf.runtime_mode`
- `./build/mini_ue_c`
  - can now switch from the shared-slot UE path into the same B210/UHD app harness through YAML `rf.runtime_mode`
- `./build/ngap_probe`
- `./build/mini_b210_probe_c`
  - only present when UHD is detected at configure time
- `./build/mini_ring_inspect`
  - inspects B210 `rx_ring.map` and `tx_ring.map` files
- `./build/mini_ring_export`
  - exports a selected `seq` range from a ring map into per-channel raw files

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

### 4.4 Run the Hybrid B210 Slot Backend Through `mini_gnb_c_sim`

`mini_gnb_c_sim` can now stay on its normal slot-driven simulator path while the `radio_frontend` starts a real B210 RX/TX session in the background.

Use the dedicated example:

```bash
cd gnb_c
sudo ./build/mini_gnb_c_sim config/example_b210_slot_backend_gnb.yml
./build/mini_ring_inspect --show-blocks 2 /dev/shm/mini_gnb_c_sim_slot_backend_rx_ring.map
./build/mini_ring_inspect --show-blocks 2 /dev/shm/mini_gnb_c_sim_slot_backend_tx_ring.map
```

What this path does today:

- keeps the existing slot-driven gNB control flow in `simulator.c`
- keeps the existing mock-radio UL semantics for `PRACH`, `MSG3`, `SR`, and `UL DATA`
- emits the generated downlink slot patches into a B210 TX ring and transmits them over the real radio
- captures real B210 RX IQ into a runtime RX ring

Current boundary of this path:

- it is a hybrid backend, not a full real NR PHY
- DL transmission uses the existing toy slot waveform from the mock DL mapper
- UL bursts are still produced by the existing mock-radio control path, not decoded from B210 IQ yet
- it is intended to move the real-radio runtime under `radio_frontend` without pretending the full PHY is finished

### 4.5 Run the B210/UHD App Harnesses

`mini_gnb_c_sim` and `mini_ue_c` can now enter the same B210/UHD runtime directly from YAML instead of requiring a long probe-only CLI.

This path is intentionally a hardware harness, not a claim that the full slot-driven gNB/UE protocol loop already runs over real RF.

Example gNB-side capture:

```bash
cd gnb_c
sudo ./build/mini_gnb_c_sim config/example_b210_runtime_gnb.yml
```

Example UE-side replay using the ring captured by the gNB harness:

```bash
cd gnb_c
sudo ./build/mini_ue_c config/example_b210_runtime_ue.yml
```

Shared `rf:` keys used by these app harnesses:

- `runtime_mode`
  - `rx`, `tx`, or `trx`
  - `simulator` now stays on the normal `mini_gnb_c_sim` path and is used by the hybrid slot backend in the previous section
- `bandwidth_hz`
- `duration_sec`
- `duration_mode`
  - `samples` or `wallclock`
- `channel`
- `channel_count`
- `rx_output_file`
- `tx_input_file`
- `rx_ring_map`
- `tx_ring_map`
- `ring_block_samples`
- `ring_block_count`
- `tx_prefetch_samples`
- `rx_freq_hz`
- `tx_freq_hz`
- `rx_gain`
- `tx_gain`
- `rx_cpu_core`
- `tx_cpu_core`

Current boundary of this path:

- it reuses the Stage 1 B210 probe/TRX runtime under the application entrypoints
- it is meant for host-side hardware smoke, capture, replay, and stress work
- on a single B210 host, the example gNB and UE harness runs are typically sequential, not simultaneous

### 4.6 Run the B210/UHD RX Smoke Probe

If UHD is installed and CMake detected it, the repo now builds a small Stage 1 hardware-smoke tool:

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --config config/default_cell.yml \
  --args "serial=8000963" \
  --subdev "A:A" \
  --ref external \
  --rate 20000000 \
  --freq 2462000000 \
  --gain 60 \
  --bw 20000000 \
  --duration 1 \
  --channel 0 \
  --cpu-core 2
```

What this tool does today:

- opens the B210 through the UHD C API
- can preload common RF defaults from a YAML file with `--config <yaml>`
  - the intended source is the shared `rf:` section
  - command-line options override YAML-loaded values
- applies the B210-relevant startup tuning that mirrors `srsran_performance`:
  - set the CPU governor to `performance`
  - disable DRM KMS polling
  - skip Ethernet socket-buffer tuning because the B210 uses USB
- applies the requested RX profile
- checks `ref_locked` and `lo_locked` when those sensors are available
- captures one RX burst and writes `fc32` IQ samples to `/dev/shm/b210_probe_rx_fc32.dat` by default
- can alternatively capture into a fixed-size `sc16` single-file ring map
- supports `--channel-count 2` for simultaneous two-channel capture when the output is a ring map
- supports pinning the RX capture thread to one CPU core with `--cpu-core`

Example ring-map capture:

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --args "serial=8000963" \
  --subdev "A:A" \
  --ref external \
  --rate 20000000 \
  --freq 2462000000 \
  --gain 60 \
  --bw 20000000 \
  --duration 0.05 \
  --channel 0 \
  --cpu-core 2 \
  --ring-map /dev/shm/rx_ring.map \
  --ring-block-samples 4096 \
  --ring-block-count 256
```

Example with shared RF defaults loaded from YAML first:

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --config config/example_open5gs_end_to_end_gnb.yml \
  --mode tx \
  --args "serial=8000963" \
  --subdev "A:A A:B" \
  --ref external \
  --channel 0 \
  --channel-count 2 \
  --cpu-core 3 \
  --ring-map /dev/shm/rx2_ring.map
```

In that mode:

- the probe preloads common RF defaults from the YAML `rf:` section first
- command-line options still win when the same field is set in both places
- probe-specific runtime paths such as `--ring-map`, `--rx-ring-map`, and `--tx-ring-map` remain command-line options

Meaning of the capture-length and ring-size controls:

- `--duration`
  - controls how long the current run captures
  - the requested sample count is `rate * duration`
- `--gain`
  - applies one gain value to both RX and TX directions
- `--rx-gain` and `--tx-gain`
  - let `trx` mode use different gain values per direction
  - the last gain-related option on the command line wins for the side it affects
- `--rx-freq` and `--tx-freq`
  - let `trx` mode use different RX and TX center frequencies
  - `--freq` remains the shared shortcut for both sides
- `--ring-block-samples`
  - controls how many complex samples fit into one ring block
- `--ring-block-count`
  - controls how many blocks the ring keeps before older data is overwritten
- `--duration-mode`
  - `samples`
    - stop when the run has collected the target successful sample count
  - `wallclock`
    - stop when the wall-clock duration expires, even if recoverable RX events reduced the received sample count

In other words:

- `duration` controls how much data this run tries to receive
- the ring parameters control how much recent history the fixed-size map can retain
- the YAML `rf` section now also carries:
  - `freq_hz`
  - `rx_freq_hz`
  - `tx_freq_hz`
  - `tx_prefetch_samples`
- for RX stress runs, recoverable RX metadata errors are now handled explicitly:
  - `OVERFLOW` and `TIMEOUT` are counted and the run continues
  - structural metadata errors and stream/API failures still stop the run
- RX stress summaries now also expose:
  - `rx_gap_events`
    - estimated hardware-time discontinuities between successful RX chunks
  - `rx_lost_samples_estimate`
    - estimated lost samples inferred from those discontinuities

What it does not do yet:

- it is still a standalone smoke path and separate from the main simulator path
- it is not the full `uhd-b210` radio backend yet
- use `--skip-host-tuning` if you want to compare behavior before and after the built-in tuning step
- the future real backend will use two dedicated worker cores through:
  - `rf.rx_cpu_core`
  - `rf.tx_cpu_core`

### 4.7 Run the B210/UHD TX Smoke Probe

The same entrypoint can also replay an existing `fc32` IQ file toward the B210:

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --mode tx \
  --args "serial=8000963" \
  --subdev "A:A" \
  --ref external \
  --rate 20000000 \
  --freq 2462000000 \
  --gain 60 \
  --bw 20000000 \
  --channel 0 \
  --cpu-core 3 \
  --file /dev/shm/test.dat
```

What this TX smoke path does today:

- opens the B210 through the UHD C API
- applies the same built-in B210 host tuning before opening the radio, unless `--skip-host-tuning` is used
- applies the requested TX profile
- checks `ref_locked` and `lo_locked` when those sensors are available
- replays one `fc32` file as a finite TX burst
- can also replay a previously captured `sc16` ring map with `--ring-map`
- supports `--channel-count 2` for simultaneous two-channel replay from a ring map
- in ring-map mode, prefetches a larger user-space TX window and refills it when the buffered sample count falls below a low-watermark threshold
- `--tx-prefetch-samples` can override that TX prefetch window explicitly
  - when omitted, the tool uses its built-in default sizing rule
  - the same parameter also applies to the `trx` mode TX side
- the same knob is now also mirrored into YAML as `rf.tx_prefetch_samples`
  - the current `mock` backend ignores it
  - the Stage 1 B210 path and the future `uhd-b210` backend are the intended consumers
- `--config <yaml>` can preload the shared `rf:` section before TX replay starts
  - command-line options still override the YAML-loaded values
- records whether the device reports `burst_ack`, `underflow`, `seq_error`, or `time_error`

Current limitation:

- on this host, the built-in tuning step plus CPU pinning removed the `20 Msps` TX `underflow` seen in the untuned smoke test
- the larger user-space prefetch window improves ring-map replay robustness, but it still does not replace the final simulator-integrated radio backend
- the next real backend step is therefore not “more smoke testing”, but a dedicated two-worker runtime:
  - one RX worker pinned to `rf.rx_cpu_core`
  - one TX worker pinned to `rf.tx_cpu_core`

### 4.8 Run the B210/UHD TRX Smoke Probe

The same probe now also supports simultaneous RX and TX with separate worker cores. The current TRX path is intentionally ring-map based:

```bash
cd gnb_c
sudo ./build/mini_b210_probe_c \
  --mode trx \
  --args "serial=8000963" \
  --subdev "A:A A:B" \
  --ref external \
  --rate 5000000 \
  --rx-freq 2462000000 \
  --tx-freq 2461000000 \
  --rx-gain 45 \
  --tx-gain 55 \
  --bw 5000000 \
  --duration 0.02 \
  --channel 0 \
  --channel-count 2 \
  --rx-cpu-core 2 \
  --tx-cpu-core 3 \
  --rx-ring-map /dev/shm/trx2_rx_ring.map \
  --tx-ring-map /dev/shm/rx2_ring.map \
  --ring-block-samples 2048 \
  --ring-block-count 128
```

What this TRX path does today:

- opens one B210 device and configures both RX and TX
- starts one RX worker and one TX worker at the same time
- pins those workers independently with:
  - `--rx-cpu-core`
  - `--tx-cpu-core`
- supports one or two channels
- uses channel-major ring blocks for two-channel payloads:
  - channel 0 samples first
  - channel 1 samples second
- `trx` can now tune RX and TX to different center frequencies through:
  - `--rx-freq`
  - `--tx-freq`
  - or YAML `rf.rx_freq_hz` and `rf.tx_freq_hz`
- the TX side now uses the same prefetch-window plus low-watermark refill policy as standalone `tx` mode
- `--tx-prefetch-samples` can override that TX-side prefetch window in `trx` mode
- `--duration` now controls the target TRX run length on both sides
  - when the TX ring is shorter than `rate * duration`, the TX side loops that ring until the requested duration is reached
  - the runtime summary reports `tx_ring_wrap_count` so long-run replay is visible
- `--duration-mode wallclock` lets RX-side stress runs stop by elapsed time instead of by successful-sample count
- the RX side now treats UHD `OVERFLOW` and `TIMEOUT` as recoverable stress events
  - they are counted in the runtime summary
  - fatal RX errors still stop the run
- the RX summary also reports:
  - `rx_gap_events`
  - `rx_lost_samples_estimate`
  - `wall_elapsed_sec`
- records `burst_ack`, `underflow`, `seq_error`, and `time_error`

Current limitation:

- TRX mode currently requires ring-map input and output
- do not point `rx_ring_map` and `tx_ring_map` at the same file during one TRX run
- this is still a standalone hardware-smoke path, not yet the final simulator-integrated `uhd-b210` backend
- `--gain` is still supported as a shared shortcut when RX and TX should use the same value

### 4.9 Inspect a Ring Map

Use the ring-map inspector to see where valid data starts and where the next write will land:

```bash
cd gnb_c
./build/mini_ring_inspect --show-blocks 4 /dev/shm/rx_ring.map
```

The ring file keeps three fixed regions in one file:

- superblock
- descriptor ring
- payload ring

That keeps metadata and pure IQ payload together without injecting per-block headers into the transmitted sample stream.

For two-channel captures and replays, each payload block is channel-major:

- all channel 0 samples for the block
- then all channel 1 samples for the same block

How to interpret the printed descriptors in two-channel RX mode:

- each `seq` is one time block, not one antenna
- one `seq` contains both enabled channels for the same capture window
- `slot` is the ring slot index `seq % block_count`, not an NR air-interface slot
- `sample_count` is per channel
  - for example, `sample_count=2048` with `channel_count=2` means:
    - 2048 complex samples for channel 0
    - 2048 complex samples for channel 1
- `payload_bytes` is the total block payload across all enabled channels
- the two channels are synchronous observations of the same time window
  - they are usually not expected to be bit-identical
  - they represent two simultaneous RF channels, for example `A:A` and `A:B`
- `hw_time_ns` is currently one shared time anchor for the whole block
  - when UHD provides RX metadata time, it is the hardware timestamp of the first sample in that block
  - it is in the USRP device time domain, not the host wall-clock domain
  - if a hardware time is unavailable, the code falls back to a host monotonic timestamp
- `flags` and `time_source` show which time source was used
  - `flags=0x1` and `time_source=uhd_hw` mean the block uses a true UHD RX hardware timestamp
  - `flags=0x2` and `time_source=host_fallback` mean the block uses the host fallback timestamp

That means a descriptor line such as:

```text
seq=123941 slot=1 sample_count=2048 payload_bytes=16384 hw_time_ns=1272047558 flags=0x1 time_source=uhd_hw
```

should be read as:

- one block with sequence `123941`
- ring slot `1`
- 2048 complex samples for channel 0
- 2048 complex samples for channel 1
- one shared block timestamp anchor for the first sample of that two-channel block
- a confirmed UHD hardware time source

### 4.10 Export a Seq Range from a Ring Map

After `mini_ring_inspect` shows the valid sequence window, export a selected range like this:

```bash
cd gnb_c
./build/mini_ring_export \
  --seq-start 123941 \
  --seq-end 123944 \
  --output-prefix out/ring_exports/rx2_slice \
  /dev/shm/rx2_ring.map
```

This writes:

- `out/ring_exports/rx2_slice_ch0.sc16`
- `out/ring_exports/rx2_slice_ch1.sc16`
  - only when `channel_count=2`
- `out/ring_exports/rx2_slice_meta.txt`

The export range is inclusive on both ends:

- `--seq-start 123941`
- `--seq-end 123944`

means export blocks `123941`, `123942`, `123943`, and `123944`.

For two-channel exports, the files mean:

- `..._ch0.sc16`
  - the concatenated IQ time series for channel 0 across the selected `seq` range
- `..._ch1.sc16`
  - the concatenated IQ time series for channel 1 across the selected `seq` range

The two exported channel files stay time-aligned block by block. For the same sample index inside the exported range:

- `ch0` is the channel 0 observation
- `ch1` is the channel 1 observation

For `trx` runs that need different RX and TX gains, use:

```bash
./build/mini_b210_probe_c \
  --mode trx \
  --rx-gain 45 \
  --tx-gain 55 \
  ...
```

### 4.11 Run the AMF Reachability Probe

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
- `/dev/shm/rx_ring.map`
  - optional fixed-size `sc16` receive ring for recent B210 samples
- `/dev/shm/tx_ring.map`
  - optional fixed-size `sc16` transmit ring for replay

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
