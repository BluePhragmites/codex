# Repository Guidelines

## Project Structure & Module Organization
This repository has two active work areas. `gnb_c/` contains the C11 mini-gNB simulator: `apps/` for entrypoints, `include/mini_gnb_c/` for public headers, `src/` for subsystem code, `config/` for YAML scenarios, `examples/` for scripted inputs, and `tests/` for unit and integration coverage. `signalAnalysis/` contains MATLAB tooling: `tools/` for shared I/O helpers, `base/` for common analysis scripts, `advance/` for deeper NR workflows, and `data/` for local captures and generated `.mat` files. Generated artifacts belong in `gnb_c/build/`, `gnb_c/out/`, and `signalAnalysis/data/`; do not commit them.

## Build, Test, and Development Commands
There is no single root build. Work from the relevant subdirectory.

```bash
cd gnb_c
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/mini_gnb_c_sim
```

Use `./build/mini_gnb_c_sim config/example_scripted_schedule.yml` or `config/example_scripted_pdcch.yml` for deterministic scheduling demos. Run `./build/ngap_probe <amf-ip> 38412 5000` for a basic Open5GS AMF reachability check. For MATLAB work, start from `signalAnalysis/` and use commands documented in [signalAnalysis/README.md](/home/hzy/codex/test2/codex/signalAnalysis/README.md), for example `addpath('signalAnalysis'); demoSignalAnalysis`.

## Coding Style & Naming Conventions
Follow the existing C style: 2-space indentation, K&R braces, C11, and compile cleanly with `-Wall -Wextra -Wpedantic`. `gnb_c/` is a C implementation; keep code in that directory in C wherever possible and avoid introducing C++-only patterns or files unless explicitly required. Name C source files in `snake_case.c`; keep public symbols and types prefixed with `mini_gnb_c_`. Add headers under `include/mini_gnb_c/` when exporting new interfaces. MATLAB utilities should keep descriptive filenames such as `fftSpectrumAnalysis.m` or `decoder_0001_cellsearch.m`; prefer small, task-focused scripts over monolithic notebooks.

## Testing Guidelines
Add or extend tests in `gnb_c/tests/` alongside the affected subsystem. Follow the existing `test_*.c` pattern and register new cases in `tests/test_main.c`. Run `ctest --test-dir build --output-on-failure` before submitting C changes. `signalAnalysis/` currently relies on manual verification; when changing parsing or visualization flows, document a reproducible MATLAB command and expected output file or figure.

## Commit & Pull Request Guidelines
Recent history uses short imperative subjects, usually in English, such as `Add minimal NGAP probe for Open5GS`. Keep commit titles focused, one change per commit, and avoid noisy prefixes. Pull requests should include a concise summary, affected paths, exact validation commands, and screenshots or sample artifact paths when plots, traces, or exported transport files change. Every update under `gnb_c/` should also record user-visible feature changes in `gnb_c/README.md` and architectural or module-level changes in `gnb_c/architecture.md`. After review, publish approved `gnb_c/` work to the GitHub branch `add-gnb-c` using the `codex` identity.
