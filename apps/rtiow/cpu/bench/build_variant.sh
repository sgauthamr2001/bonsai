#!/bin/bash
# Build one RTIOW binary for a chosen thread parallelism count N 
# Output goes to bench/build/t{N}/.
#
# Usage:
#   apps/rtiow/cpu/bench/build_variant.sh <N>
#
# N is the target maximum concurrency 
# Bonsai lowers parfor to dispatch_apply_f on Apple's Grand Central Dispatch (GCD); 
# by emitting exactly N chunks (chunk_size = image_height / N) we cap concurrency at N because GCD

# Bonsai's `split(...)` schedule runs the inner forall over the FULL chunk
# size with no bounds check (the `generate_tail` strategy is unimplemented;
# see bonsai/src/Lower/LoopTransforms.cpp:145). If chunk_size does not
# evenly divide image_height, the renderer writes past the framebuffer.
# We therefore require N * chunk_size == image_height exactly, and we use
# image_height = 720 (matches RTIOW_IMAGE_WIDTH = 1280) which is divisible
# by every N in our canonical set {1, 2, 4, 8, 10, 12}.
#
# Outputs:
#   apps/rtiow/cpu/bench/build/t<N>/{main.bonsai, main.bir, main.ll, main.o,
#                                    main.h, main_hook.cpp, bonsai.out}

set -euo pipefail

N="${1:?usage: build_variant.sh <N>}"
CHUNK_HEIGHT="${2:-720}"

if ! [[ "$N" =~ ^[0-9]+$ ]] || [ "$N" -lt 1 ]; then
    echo "build_variant.sh: N must be a positive integer (got '$N')" >&2
    exit 1
fi

if (( CHUNK_HEIGHT % N != 0 )); then
    echo "build_variant.sh: ERROR -- N=$N does not evenly divide image_height=$CHUNK_HEIGHT" >&2
    echo "  Bonsai's split has no tail strategy; chunk_size must divide height." >&2
    echo "  Pick an N from height divisors. For 720: 1 2 3 4 5 6 8 9 10 12 15 16 18 20 ..." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
cd "$REPO_ROOT"

PREFIX="apps/rtiow/cpu"
BENCH_DIR="$PREFIX/bench"
SRC_BONSAI="$PREFIX/main.bonsai"
SRC_HOOK="$PREFIX/main_hook.cpp"

VARIANT_DIR="$BENCH_DIR/build/t${N}"
mkdir -p "$VARIANT_DIR"

CHUNK=$(( CHUNK_HEIGHT / N ))

# Rewrite the schedule line in main.bonsai to target N chunks. Original is:
#     image.split(i0, io, ii, 45, false) // make 15 threads
sed -E "s|image\.split\(i0, io, ii, [0-9]+, false\)[^\n]*|image.split(i0, io, ii, ${CHUNK}, false) // bench: target N=${N} chunks|" \
    "$SRC_BONSAI" > "$VARIANT_DIR/main.bonsai"

# Build the bonsai compiler if needed.
cmake --build build --config Debug -j >&2

# Bonsai's `-b cpp -o <prefix>` writes <prefix>.h and <prefix>.o.
./build/compiler -i "$VARIANT_DIR/main.bonsai" -b cpp  -o "$VARIANT_DIR/main" >&2

# *** ABI fix-up ***
# Bonsai's auto-generated main.h declares
#     using vec3_float = vector<float, 3>;
# which resolves to the packed struct `vector<T,N>` in runtime/bonsai_vector.h
# (size 12, ALIGN 1). But the emitted main.o internally uses LLVM <3 x float>
# (size 12, ALIGN 16). The struct layout of any type containing `vec3_float`
# (Sphere, MaterialSphere, Camera, _tree_layout1, ...) therefore differs
# between C++ callers and main.o, and the renderer reads garbage. The
# canonical apps/rtiow/cpu/main.h was hand-patched to use the LLVM-ABI typedef
# instead; we apply the same patch here so each variant's header matches its
# main.o. See apps/rtiow/cpu/main.h:8-9 and runtime/bonsai_vector.h.
if grep -q 'using vec3_float = vector<float, 3>;' "$VARIANT_DIR/main.h"; then
    sed -i.bak \
        's|using vec3_float = vector<float, 3>;|typedef float vec3_float __attribute__((vector_size(12)));|' \
        "$VARIANT_DIR/main.h"
    rm -f "$VARIANT_DIR/main.h.bak"
fi

./build/compiler -i "$VARIANT_DIR/main.bonsai"        -o "$VARIANT_DIR/main.bir" >&2 || true
./build/compiler -i "$VARIANT_DIR/main.bonsai" -b llvm -o "$VARIANT_DIR/main.ll"  >&2 || true

# clang searches the source file's own directory first for #include "main.h",
# so copying main_hook.cpp into the variant directory makes it pick up the
# variant's main.h instead of the canonical apps/rtiow/cpu/main.h.
cp "$SRC_HOOK" "$VARIANT_DIR/main_hook.cpp"

clang++ -g -std=c++20 -O3 -I. \
    "$VARIANT_DIR/main_hook.cpp" "$VARIANT_DIR/main.o" \
    -o "$VARIANT_DIR/bonsai.out"

rm -rf "$VARIANT_DIR/bonsai.out.dSYM"

echo "Built $VARIANT_DIR/bonsai.out  (target_threads=$N, chunk=$CHUNK, height=$CHUNK_HEIGHT)"