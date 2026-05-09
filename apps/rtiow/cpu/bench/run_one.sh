#!/bin/bash
# Run one configuration of the renderer and emit a single CSV row.
# Wraps execution with /usr/bin/time -p and forwards parse_run.sh.
#
# Usage:
#   run_one.sh \
#       --variant=t10 \
#       --threads=10 \
#       --grid-half=20 \
#       --samples=20 \
#       --max-depth=1 \
#       --image-width=1200 \
#       --seed=42 \
#       --taskpolicy=default           # or utility/background to bias to E-cores
#       [--out-dir=apps/rtiow/cpu/bench/results/_run]
#
# Writes <out-dir>/{stdout.log, time.log, output.ppm} and prints one CSV row.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

variant=""
threads=""
grid_half=""
samples=""
max_depth=1
# 1280x720 (= 1280 / (16/9)) is the canonical bench image size. It's chosen
# so image_height = 720 evenly divides every N in our variant set (1, 2, 4,
# 8, 10, 12). Bonsai's split has no tail strategy, so chunk_size MUST divide
# image_height -- see build_variant.sh.
image_width=1280
seed=42
# macOS `taskpolicy -c` only accepts utility/background/maintenance, all of
# which are *downgrades* (push to E-cores). There is no positive "prefer
# P-cores" QoS clamp via this tool. So our default is to NOT wrap and just
# let GCD's default-QoS global queue schedule -- with 10 worker chunks on
# the M4 Pro, work lands on the 10 P-cores in practice. To explicitly run
# on E-cores (e.g. as a contrast control), pass --taskpolicy=utility.
taskpolicy="default"
out_dir=""

for arg in "$@"; do
    case "$arg" in
        --variant=*)     variant="${arg#--variant=}" ;;
        --threads=*)     threads="${arg#--threads=}" ;;
        --grid-half=*)   grid_half="${arg#--grid-half=}" ;;
        --samples=*)     samples="${arg#--samples=}" ;;
        --max-depth=*)   max_depth="${arg#--max-depth=}" ;;
        --image-width=*) image_width="${arg#--image-width=}" ;;
        --seed=*)        seed="${arg#--seed=}" ;;
        --taskpolicy=*)  taskpolicy="${arg#--taskpolicy=}" ;;
        --out-dir=*)     out_dir="${arg#--out-dir=}" ;;
        *)
            echo "run_one.sh: unknown arg: $arg" >&2
            exit 2
            ;;
    esac
done

for required in variant threads grid_half samples; do
    if [[ -z "${!required}" ]]; then
        echo "run_one.sh: missing --${required//_/-}=" >&2
        exit 2
    fi
done

binary="$REPO_ROOT/apps/rtiow/cpu/bench/build/${variant}/bonsai.out"
if [[ ! -x "$binary" ]]; then
    echo "run_one.sh: binary missing or not executable: $binary" >&2
    echo "  -> build it first:  apps/rtiow/cpu/bench/build_variant.sh ${variant#t}" >&2
    exit 1
fi

if [[ -z "$out_dir" ]]; then
    out_dir="$REPO_ROOT/apps/rtiow/cpu/bench/results/_run/${variant}_g${grid_half}_s${samples}_w${image_width}"
fi
mkdir -p "$out_dir"

stdout_log="$out_dir/stdout.log"
time_log="$out_dir/time.log"
ppm_path="$out_dir/output.ppm"

# Resolve QoS wrapper. taskpolicy=default means: don't wrap.
# Valid `taskpolicy -c` clamps on macOS are utility, background, maintenance
# (all *downgrades* toward E-cores). There is no positive P-core clamp.
qos_prefix=""
if [[ "$taskpolicy" != "default" ]]; then
    if command -v taskpolicy >/dev/null 2>&1; then
        qos_prefix="taskpolicy -c $taskpolicy "
    else
        echo "run_one.sh: taskpolicy not available; running without QoS bias" >&2
        taskpolicy="default"
    fi
fi

# /usr/bin/time -p emits POSIX-format real/user/sys to stderr.
# We redirect stdout from the renderer to stdout_log and time's stderr to time_log.
# Build the command as a single string for `bash -c` so the empty-array case
# is straightforward under `set -u` on macOS bash 3.2.
cmd="env RTIOW_GRID_HALF=$grid_half RTIOW_SAMPLES=$samples RTIOW_MAX_DEPTH=$max_depth"
cmd="$cmd RTIOW_IMAGE_WIDTH=$image_width RTIOW_SEED=$seed"
cmd="$cmd /usr/bin/time -p ${qos_prefix}'$binary' '$ppm_path'"

if ! bash -c "$cmd" >"$stdout_log" 2>"$time_log"; then
    echo "run_one.sh: renderer failed; see $stdout_log and $time_log" >&2
    exit 1
fi

"$SCRIPT_DIR/parse_run.sh" \
    --variant="$variant" \
    --threads="$threads" \
    --taskpolicy="$taskpolicy" \
    --stdout="$stdout_log" \
    --time="$time_log" \
    --ppm="$ppm_path"
