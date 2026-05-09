#!/bin/bash
# Grid sweep: fix threads (10) and samples, vary RTIOW_GRID_HALF (changes scene size and prim count)
#
# Per the bench plan, the chosen RTIOW_GRID_HALF set densely samples the
# region around the L1D crossing (~22-25 in this scene) and goes up to 85 to
# stay below the u16 BVH-offset ceiling. (TOD0: Fix bonsai binary to use u32 for BVH offsets.)
#
# Output: appends rows to apps/rtiow/cpu/bench/results/grid_sweep.csv
# (overwrites the file at start; pass --append to keep existing rows).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

VARIANT="${VARIANT:-t10}"
THREADS="${THREADS:-10}"
SAMPLES="${SAMPLES:-20}"
TASKPOLICY="${TASKPOLICY:-default}"
GRIDS="${GRIDS:-5 8 11 15 20 25 30 40 55 70 85}"
REPS="${REPS:-1}"

mode="overwrite"
for arg in "$@"; do
    case "$arg" in
        --append) mode="append" ;;
        *) echo "run_grid_sweep.sh: unknown arg: $arg" >&2; exit 2 ;;
    esac
done

results_dir="$REPO_ROOT/apps/rtiow/cpu/bench/results"
mkdir -p "$results_dir"
csv="$results_dir/grid_sweep.csv"

if [[ "$mode" == "overwrite" || ! -f "$csv" ]]; then
    "$SCRIPT_DIR/parse_run.sh" --header > "$csv"
fi

echo "Grid sweep: variant=$VARIANT threads=$THREADS samples=$SAMPLES taskpolicy=$TASKPOLICY"
echo "  RTIOW_GRID_HALF in: $GRIDS  (reps=$REPS)"
echo "  Writing to $csv"
echo

for g in $GRIDS; do
    for r in $(seq 1 "$REPS"); do
        out_dir="$results_dir/_run/grid/${VARIANT}_g${g}_s${SAMPLES}_r${r}"
        echo "  -> grid_half=$g  rep=$r"
        row="$("$SCRIPT_DIR/run_one.sh" \
            --variant="$VARIANT" \
            --threads="$THREADS" \
            --grid-half="$g" \
            --samples="$SAMPLES" \
            --taskpolicy="$TASKPOLICY" \
            --out-dir="$out_dir")"
        echo "$row" >> "$csv"
        echo "     $row"
    done
done

echo
echo "Grid sweep done: $(($(wc -l < "$csv") - 1)) rows in $csv"
