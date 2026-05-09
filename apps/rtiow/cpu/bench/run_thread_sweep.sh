#!/bin/bash
# Thread sweep: at each of three grid sizes (small/mid/max), vary the
# thread count {1,2,4,8,10,14} and record render time.
#
# Speedup-vs-N: when 10 P-cores all walk the same _tree_layout1 array, but since each 
# core has a private L1, the speedup is limited by L2 contention.

# Output: appends rows to apps/rtiow/cpu/bench/results/thread_sweep.csv
# (overwrites at start; pass --append to keep existing rows).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

SAMPLES="${SAMPLES:-20}"
TASKPOLICY="${TASKPOLICY:-default}"
# Default grids: small (BVH < L1D), mid (BVH > L1D, < L2), max (just below u16 cap).
GRIDS="${GRIDS:-11 30 85}"
# Default variants: span single-thread baseline through full-P-cluster
# oversubscription. We use t12 (not t14) because Bonsai's split-without-tail
# requires chunk_size to evenly divide image_height = 720, and 720/14 isn't
# integer (see build_variant.sh).
VARIANTS="${VARIANTS:-t1 t2 t4 t8 t10 t12}"
REPS="${REPS:-1}"

mode="overwrite"
for arg in "$@"; do
    case "$arg" in
        --append) mode="append" ;;
        *) echo "run_thread_sweep.sh: unknown arg: $arg" >&2; exit 2 ;;
    esac
done

results_dir="$REPO_ROOT/apps/rtiow/cpu/bench/results"
mkdir -p "$results_dir"
csv="$results_dir/thread_sweep.csv"

if [[ "$mode" == "overwrite" || ! -f "$csv" ]]; then
    "$SCRIPT_DIR/parse_run.sh" --header > "$csv"
fi

for v in $VARIANTS; do
    if [[ ! -x "$REPO_ROOT/apps/rtiow/cpu/bench/build/${v}/bonsai.out" ]]; then
        echo "thread sweep: missing build/${v}/bonsai.out" >&2
        echo "  -> build it: apps/rtiow/cpu/bench/build_variant.sh ${v#t}" >&2
        exit 1
    fi
done

echo "Thread sweep: variants=$VARIANTS  grids=$GRIDS  samples=$SAMPLES  taskpolicy=$TASKPOLICY"
echo "  Writing to $csv"
echo

for g in $GRIDS; do
    for v in $VARIANTS; do
        threads="${v#t}"
        for r in $(seq 1 "$REPS"); do
            out_dir="$results_dir/_run/threads/g${g}_${v}_s${SAMPLES}_r${r}"
            echo "  -> grid=$g variant=$v threads=$threads rep=$r"
            row="$("$SCRIPT_DIR/run_one.sh" \
                --variant="$v" \
                --threads="$threads" \
                --grid-half="$g" \
                --samples="$SAMPLES" \
                --taskpolicy="$TASKPOLICY" \
                --out-dir="$out_dir")"
            echo "$row" >> "$csv"
            echo "     $row"
        done
    done
done

echo
echo "Thread sweep done: $(($(wc -l < "$csv") - 1)) rows in $csv"
