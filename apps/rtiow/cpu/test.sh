#!/bin/bash

set -euo pipefail

# Always run build/run from the Bonsai repo root (parent of apps/), no matter
# where this script was invoked from — avoids writing rtiow-cpu-image.ppm to the wrong cwd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$(cd "$SCRIPT_DIR/../../.." && pwd)"

PREFIX="apps/rtiow/cpu"

# --- Scene / render knobs (optional; exported for ./bonsai.out) ---
# RTIOW_GRID_HALF   — half-extent of random-sphere grid: loops ai,bi in [-N,N).
#                     Default 11 → 22×22 cells (~same as original). Try 15, 20, …
# RTIOW_IMAGE_WIDTH — framebuffer width in pixels (height follows aspect 16:9).
# RTIOW_SAMPLES     — samples_per_pixel (lower while scaling grid for quick tests).
# RTIOW_MAX_DEPTH   — max ray recursion depth.
export RTIOW_GRID_HALF="${RTIOW_GRID_HALF:-11}"
export RTIOW_IMAGE_WIDTH="${RTIOW_IMAGE_WIDTH:-1200}"
export RTIOW_SAMPLES="${RTIOW_SAMPLES:-50}"
export RTIOW_MAX_DEPTH="${RTIOW_MAX_DEPTH:-1}"

# Compile
cmake --build build --config Debug -j
./build/compiler -i $PREFIX/main.bonsai -o $PREFIX/main.bir
./build/compiler -i $PREFIX/main.bonsai -b llvm -o $PREFIX/main.ll
#./build/compiler -i $PREFIX/main.bonsai -b cpp -o $PREFIX/main
clang++ -g -std=c++20 -O3 -I. $PREFIX/main_hook.cpp $PREFIX/main.o -o $PREFIX/bonsai.out
# Run
time ./$PREFIX/bonsai.out $PREFIX/rtiow-cpu-image.ppm

# Clean up
# rm $PREFIX/main.bir
# rm $PREFIX/main.ll
# rm $PREFIX/main.o
# rm $PREFIX/bonsai.out
rm -r $PREFIX/bonsai.out.dSYM

exit 0