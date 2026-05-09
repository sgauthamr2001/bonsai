#!/bin/bash
# Samples sweep: fix grid + threads, vary RTIOW_SAMPLES.
#
# This is the compute-vs-memory negative control. At a fixed scene the BVH
# bytes are constant; varying samples changes only how many rays per pixel
# we trace. If render_ms scales linearly in samples, the inner traversal
# loop is compute/L1-resident-bound at this grid. Sub-linear scaling means
# BVH lines are being reused across samples (L1D->L2 traffic amortized).
#
# Output: appends rows to apps/rtiow/cpu/bench/results/samples_sweep.csv
# (overwrites at start; pass --append to keep existing rows).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

VARIANT="${VARIANT:-t10}"
THREADS="${THREADS:-10}"
GRID="${GRID:-30}"  # mid grid: BVH > L1D, well within L2
MAX_DEPTH="${MAX_DEPTH:-20}"
TASKPOLICY="${TASKPOLICY:-default}"
SAMPLES_LIST="${SAMPLES_LIST:-1 4 16 64 256}"
REPS="${REPS:-1}"

mode="overwrite"
for arg in "$@"; do
    case "$arg" in
        --append) mode="append" ;;
        *) echo "run_samples_sweep.sh: unknown arg: $arg" >&2; exit 2 ;;
    esac
done

results_dir="$REPO_ROOT/apps/rtiow/cpu/bench/results"
mkdir -p "$results_dir"
csv="$results_dir/samples_sweep.csv"

if [[ "$mode" == "overwrite" || ! -f "$csv" ]]; then
    "$SCRIPT_DIR/parse_run.sh" --header > "$csv"
fi

echo "Samples sweep: variant=$VARIANT threads=$THREADS grid=$GRID max_depth=$MAX_DEPTH taskpolicy=$TASKPOLICY"
echo "  RTIOW_SAMPLES in: $SAMPLES_LIST  (reps=$REPS)"
echo "  Writing to $csv"
echo

for s in $SAMPLES_LIST; do
    for r in $(seq 1 "$REPS"); do
        out_dir="$results_dir/_run/samples/${VARIANT}_g${GRID}_s${s}_r${r}"
        echo "  -> samples=$s rep=$r"
        row="$("$SCRIPT_DIR/run_one.sh" \
            --variant="$VARIANT" \
            --threads="$THREADS" \
            --grid-half="$GRID" \
            --samples="$s" \
            --max-depth="$MAX_DEPTH" \
            --taskpolicy="$TASKPOLICY" \
            --out-dir="$out_dir")"
        echo "$row" >> "$csv"
        echo "     $row"
    done
done

echo
echo "Samples sweep done: $(($(wc -l < "$csv") - 1)) rows in $csv"
