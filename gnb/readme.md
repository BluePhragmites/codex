# mini-gnb

`mini-gnb` is a single-process, single-cell, single-UE prototype that drives a minimal NR initial access flow from Msg1 through Msg4 with mock PHY/RF components.

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
   ctest --test-dir build
   ./build/mini_gnb_sim
   ```

The simulator writes JSON trace artifacts into `out/`.

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
