#!/bin/bash
# Build all thread-count variants used by the bench harness.
# Default set: 1, 2, 4, 8, 10, 12. Each must evenly divide image_height
# (720 by default) -- see build_variant.sh's correctness note. We use 12
# (not 14) for "P-cluster oversubscription" because 720 / 14 isn't integer.
#
# Override with: VARIANTS="1 4 10" apps/rtiow/cpu/bench/build_all.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VARIANTS="${VARIANTS:-1 2 4 8 10 12}"

for N in $VARIANTS; do
    "$SCRIPT_DIR/build_variant.sh" "$N"
done

echo
echo "Built variants: $VARIANTS"
