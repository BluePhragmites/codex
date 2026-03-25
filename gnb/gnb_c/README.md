# mini-gnb-c

`mini-gnb-c` is a C rewrite of the minimal gNB prototype in this repository. It keeps the same target shape as the C++ version:

- single process
- single cell
- single UE
- mock PHY/RF
- Msg1 through Msg4 simulated initial access closure

The C version intentionally uses fixed-size buffers and plain C interfaces so it can serve as a simpler bring-up baseline.

## Layout

- `config/`: static JSON configuration
- `apps/`: simulator entrypoint
- `include/mini_gnb_c/`: public C headers by subsystem
- `src/`: implementation modules
- `tests/`: unit and integration tests

## Build In WSL Ubuntu

```bash
cd /mnt/d/work/codex/gnb/gnb_c
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_gnb_c_sim
```

The current WSL validation result is:

- configure: passed
- build: passed
- `ctest --test-dir build --output-on-failure`: passed
- `./build/mini_gnb_c_sim`: passed

The simulator writes:

- `out/trace.json`
- `out/metrics.json`
- `out/summary.json`
- `out/iq/*.cf32`
- `out/iq/*.json`

The IQ export is a minimal mock PHY waveform:

- format: interleaved little-endian `float32`, layout `I0,Q0,I1,Q1,...`
- one file per downlink object such as `SSB`, `SIB1`, `RAR`, and `Msg4`
- sidecar JSON includes slot, RNTI, PRB allocation, FFT size, CP length, and sample count

## Current Scope

The current C prototype keeps:

- SSB/PBCH/MIB scheduling
- SIB1 scheduling
- mock PRACH detection
- RAR generation and Msg3 UL grant
- mock Msg3 decode
- MAC UL demux
- `RRCSetupRequest` parsing
- Msg4 generation with contention resolution identity and `RRCSetup`
- JSON trace, metrics, and summary artifacts in `out/`

The current C prototype does not keep:

- core network integration
- real RF/UHD
- multi-UE scheduling
- security, reconfiguration, handover, paging
- HARQ, PUCCH, DRB, PDCP, SDAP
