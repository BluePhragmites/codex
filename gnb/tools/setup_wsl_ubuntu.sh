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
  pkg-config \
  python3 \
  python3-numpy \
  python3-matplotlib

echo "WSL Ubuntu toolchain setup complete."
