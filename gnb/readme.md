# mini-gnb

`mini-gnb` is a single-process, single-cell, single-UE prototype that drives a minimal NR initial access flow from Msg1 through Msg4 with mock PHY/RF components.

This repository has been validated in `WSL2 + Ubuntu 22.04` on this machine:

- `cmake -S . -B build -G Ninja` succeeded
- `cmake --build build` succeeded
- `ctest --test-dir build --output-on-failure` succeeded
- `./build/mini_gnb_sim` succeeded
- the simulated flow reached `Msg1 -> Msg4` and wrote artifacts into `out/`

The repository is intentionally shaped like a future gNB codebase:

- `config/`: static JSON cell and simulation configuration
- `apps/`: runnable entrypoints
- `include/mini_gnb/`: public headers by subsystem
- `src/`: subsystem implementations
- `tests/`: unit and integration tests
- `tools/`: environment bootstrap helpers for WSL Ubuntu

## Scope

This first version keeps only the minimum needed to exercise the access flow:

- single cell, single BWP, single SCS
- SSB/PBCH/MIB and SIB1 broadcast scheduling
- PRACH detection
- RAR scheduling and Msg3 UL grant
- Msg3 UL-SCH mock decode with MAC demux
- `RRCSetupRequest` parsing
- Msg4 generation with contention resolution identity and `RRCSetup`

The following are intentionally out of scope:

- CU-CP, CU-UP, NGAP, core network
- DRB, PDCP, SDAP
- multi-UE scheduling
- security, reconfiguration, handover, paging
- real UHD/USRP integration

## WSL Ubuntu Setup

The intended development environment is WSL2 with Ubuntu 22.04 or newer.

1. Install WSL on Windows:

   ```powershell
   wsl --install -d Ubuntu-22.04
   ```

2. Inside Ubuntu, install the toolchain:

   ```bash
   bash ./tools/setup_wsl_ubuntu.sh
   ```

3. Build and run:

   ```bash
   cmake -S . -B build -G Ninja
   cmake --build build
   ctest --test-dir build --output-on-failure
   ./build/mini_gnb_sim
   ```

The simulator writes JSON trace artifacts into `out/`.

The current mock DL PHY also exports minimal time-domain IQ for each downlink burst:

- `out/iq/*.cf32`: interleaved little-endian float32 IQ samples
- `out/iq/*.json`: sidecar metadata for each IQ file
- `tools/plot_cf32.py`: Python/Matplotlib helper to render time-domain and FFT plots as PNG

## Daily Commands

If you are already inside Ubuntu and in the project directory:

```bash
cd /mnt/d/work/codex/test/codex/gnb
cmake -S . -B build -G Ninja
cmake --build build
```

If you want to run from Windows PowerShell through WSL:

```powershell
wsl bash -lc "cd /mnt/d/work/codex/test/codex/gnb && cmake -S . -B build -G Ninja"
wsl bash -lc "cd /mnt/d/work/codex/test/codex/gnb && cmake --build build"
```

## Run The Simulator

Inside Ubuntu:

```bash
cd /mnt/d/work/codex/test/codex/gnb
./build/mini_gnb_sim
```

From Windows PowerShell:

```powershell
wsl bash -lc "cd /mnt/d/work/codex/test/codex/gnb && ./build/mini_gnb_sim"
```

Expected high-level output:

- broadcast config summary
- PRACH detection
- RAR scheduling and send
- Msg3 decode and MAC/RRC parsing
- Msg4 send
- final line `Run result: Msg1 -> Msg4 simulated successfully.`

## Unit And Integration Tests

This repository currently builds one test executable, `mini_gnb_tests`, and runs it through CTest.

Run all tests with CTest:

```bash
cd /mnt/d/work/codex/test/codex/gnb
ctest --test-dir build --output-on-failure
```

Or run the test binary directly:

```bash
cd /mnt/d/work/codex/test/codex/gnb
./build/mini_gnb_tests
```

From Windows PowerShell:

```powershell
wsl bash -lc "cd /mnt/d/work/codex/test/codex/gnb && ctest --test-dir build --output-on-failure"
wsl bash -lc "cd /mnt/d/work/codex/test/codex/gnb && ./build/mini_gnb_tests"
```

The current tests cover:

- config loading
- RA state progression from PRACH to Msg4
- RA timeout handling
- Msg3 MAC demux, `RRCSetupRequest` parsing, and Msg4 contention identity consistency
- end-to-end simulator integration

## Output Artifacts

After running `./build/mini_gnb_sim`, the simulator writes:

- `out/trace.json`: recent event trace
- `out/metrics.json`: counters and per-slot timing summary
- `out/summary.json`: run summary, RA context, UE context, and artifact paths
- `out/iq/*.cf32`: exported complex IQ samples for each DL burst
- `out/iq/*.json`: IQ export metadata including slot, RNTI, PRB range, FFT size, CP length, and sample count

On this machine the generated paths are:

- `/mnt/d/work/codex/test/codex/gnb/out/trace.json`
- `/mnt/d/work/codex/test/codex/gnb/out/metrics.json`
- `/mnt/d/work/codex/test/codex/gnb/out/summary.json`

## Inspect IQ Data

Each `.cf32` file stores interleaved complex float32 samples:

```text
I0, Q0, I1, Q1, I2, Q2, ...
```

You can inspect them in Python:

```python
import numpy as np

iq = np.fromfile("out/iq/slot_7_DL_OBJ_MSG4_rnti_17921_tx_7.cf32", dtype=np.float32)
iq = iq[0::2] + 1j * iq[1::2]
print(iq[:8])
```

Or from WSL:

```bash
ls out/iq
```

Generate a plot directly from a `.cf32` file:

```bash
python3 tools/plot_cf32.py out/iq/slot_7_DL_OBJ_MSG4_rnti_17921_tx_7.cf32
```

This writes a PNG into `out/plots/` by default.

Note: this is a minimal mock PHY waveform, not a full 3GPP-compliant NR baseband chain yet. The IQ is generated by a toy OFDM exporter so you can observe real complex samples and validate the DL signal path end to end.

## GitHub Bootstrapping

After WSL is ready and `git` is installed:

```bash
git init
git add .
git commit -m "Initial mini gNB Msg1-Msg4 prototype"
```

Then create or attach a remote named `origin` and push `main`.

You can also use:

```bash
bash ./tools/first_push.sh <remote-url>
```
