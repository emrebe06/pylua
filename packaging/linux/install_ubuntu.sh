#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build python3

cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT_DIR/build"

echo "Lunara built successfully on Ubuntu."
echo "Binary: $ROOT_DIR/build/lunara"
