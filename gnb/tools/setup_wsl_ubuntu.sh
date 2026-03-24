#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  git \
  gdb \
  clang \
  clang-tidy \
  pkg-config

echo "WSL Ubuntu toolchain setup complete."
