#!/bin/bash
# Orchestrate the full bench: build all variants, run the three sweeps,
# generate plots, and (optionally) collect Apple Instruments counters.
#
# Defaults are sized for ~10-20 minutes of wall time on the M4 Pro.
# Override knobs via env (see each sweep script for its own defaults):
#   VARIANTS       : space-separated list of N values for build_all
#   GRIDS          : grid_sweep RTIOW_GRID_HALF list
#   THREAD_GRIDS   : thread_sweep grid list
#   SAMPLES_LIST   : samples_sweep list
#   SKIP_BUILD=1   : skip build_all
#   SKIP_GRID=1    : skip grid_sweep
#   SKIP_THREAD=1  : skip thread_sweep
#   SKIP_SAMPLES=1 : skip samples_sweep
#   SKIP_PLOTS=1   : skip plot_results
#   WITH_XCTRACE=1 : also run run_xctrace at one anchor (mid grid, t10)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
cd "$REPO_ROOT"

step() { echo; echo "==== $* ===="; }

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
    step "Building variants"
    bash "$SCRIPT_DIR/build_all.sh"
fi

if [[ "${SKIP_GRID:-0}" != "1" ]]; then
    step "Grid sweep (variant=t10, varying grid_half)"
    bash "$SCRIPT_DIR/run_grid_sweep.sh"
fi

if [[ "${SKIP_THREAD:-0}" != "1" ]]; then
    step "Thread sweep (varying variant at fixed grids)"
    bash "$SCRIPT_DIR/run_thread_sweep.sh"
fi

if [[ "${SKIP_SAMPLES:-0}" != "1" ]]; then
    step "Samples sweep (variant=t10, fixed grid, varying samples)"
    bash "$SCRIPT_DIR/run_samples_sweep.sh"
fi

if [[ "${SKIP_PLOTS:-0}" != "1" ]]; then
    step "Plotting"
    if command -v python3 >/dev/null 2>&1; then
        python3 "$SCRIPT_DIR/plot_results.py" || \
            echo "(plot_results.py failed -- install matplotlib if missing)"
    else
        echo "(python3 not found; skipping plots)"
    fi
fi

if [[ "${WITH_XCTRACE:-0}" == "1" ]]; then
    step "xctrace anchor capture (t10 at mid grid)"
    bash "$SCRIPT_DIR/run_xctrace.sh" \
        --variant=t10 --threads=10 --grid-half=30 --samples=20 || \
        echo "(xctrace failed; see message above)"
fi

step "Done. Results under $REPO_ROOT/apps/rtiow/cpu/bench/results/"
ls -la "$REPO_ROOT/apps/rtiow/cpu/bench/results" 2>/dev/null | sed 's/^/  /'
