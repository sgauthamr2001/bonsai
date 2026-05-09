#!/bin/bash
# Capture Apple Instruments CPU counters for one anchor configuration.
#
# Notes / caveats:
#   * Requires `xctrace` from Xcode (not just Command Line Tools). Check with
#     `xcrun -f xctrace`. If missing, install Xcode and run `xcrun xctrace
#     list templates` to discover available counter templates on your system.
#   * Counter event names on Apple Silicon vary by Xcode version. We do NOT
#     hard-code a list. Instead, after capture, we:
#       (a) write the .trace bundle for manual Instruments review,
#       (b) `xctrace export` to XML and best-effort scrape any totals we
#           recognize (cycles, instructions, *L1D*, *L2*),
#       (c) drop a manifest noting which counters were observed.
#   * Apple Silicon SLC and DRAM bandwidth are NOT scoped here -- the bench
#     is bounded to L1D<->L2 effects (see README.md).
#   * macOS sandboxing / SIP may prompt for developer tool access on first
#     run; that prompt cannot be bypassed in a script.
#
# Usage:
#   run_xctrace.sh \
#       --variant=t10 --threads=10 \
#       --grid-half=30 --samples=20 \
#       [--taskpolicy=userinteractive] \
#       [--template="CPU Counters"] \
#       [--out-dir=apps/rtiow/cpu/bench/results/_xctrace/...]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

variant=""
threads=""
grid_half=""
samples=""
max_depth=1
# Match run_one.sh's canonical bench image (1280x720); see that script for
# why height must equal a known multiple of every variant N.
image_width=1280
seed=42
taskpolicy="userinteractive"
template="CPU Counters"
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
        --template=*)    template="${arg#--template=}" ;;
        --out-dir=*)     out_dir="${arg#--out-dir=}" ;;
        *)
            echo "run_xctrace.sh: unknown arg: $arg" >&2
            exit 2
            ;;
    esac
done

for required in variant threads grid_half samples; do
    if [[ -z "${!required}" ]]; then
        echo "run_xctrace.sh: missing --${required//_/-}=" >&2
        exit 2
    fi
done

if ! xctrace_bin="$(xcrun -f xctrace 2>/dev/null)"; then
    echo "run_xctrace.sh: xctrace not found. Install full Xcode (not just CLT)." >&2
    exit 1
fi

binary="$REPO_ROOT/apps/rtiow/cpu/bench/build/${variant}/bonsai.out"
if [[ ! -x "$binary" ]]; then
    echo "run_xctrace.sh: binary missing: $binary" >&2
    echo "  -> build it: apps/rtiow/cpu/bench/build_variant.sh ${variant#t}" >&2
    exit 1
fi

if [[ -z "$out_dir" ]]; then
    out_dir="$REPO_ROOT/apps/rtiow/cpu/bench/results/_xctrace/${variant}_g${grid_half}_s${samples}_${taskpolicy}"
fi
mkdir -p "$out_dir"

trace_bundle="$out_dir/run.trace"
xml_path="$out_dir/counters.xml"
manifest="$out_dir/counters_manifest.txt"
counters_csv="$out_dir/counters.csv"
ppm_path="$out_dir/output.ppm"

# Clean prior trace bundle (xctrace refuses to overwrite).
rm -rf "$trace_bundle"

echo "xctrace template: $template"
echo "binary:           $binary"
echo "out_dir:          $out_dir"

# Build env for the launched process. xctrace passes env through with --env.
env_args=(
    "RTIOW_GRID_HALF=$grid_half"
    "RTIOW_SAMPLES=$samples"
    "RTIOW_MAX_DEPTH=$max_depth"
    "RTIOW_IMAGE_WIDTH=$image_width"
    "RTIOW_SEED=$seed"
)

# We deliberately don't wrap with taskpolicy here -- xctrace --launch attaches
# directly to the process. To bias QoS, set the QoS class via taskpolicy on
# the parent shell before invoking this script: `taskpolicy -c userinteractive
# bash run_xctrace.sh ...`.
"$xctrace_bin" record \
    --template "$template" \
    --output "$trace_bundle" \
    --launch -- "$binary" "$ppm_path" \
    $(printf -- "--env %s " "${env_args[@]}") \
    || {
        echo "run_xctrace.sh: xctrace record failed; partial trace at $trace_bundle" >&2
        exit 1
    }

echo
echo "Recorded trace: $trace_bundle"

# Best-effort XML export. The exact xctrace export schema differs by Xcode
# version; we just dump the trace's "table" data and grep what we can.
if "$xctrace_bin" export --input "$trace_bundle" --xpath '/trace-toc' \
        > "$xml_path" 2>/dev/null; then
    echo "Exported XML index: $xml_path"
else
    echo "(could not export XML index; open $trace_bundle in Instruments manually)"
fi

# Counter discovery: pull anything that looks like a counter name from the XML.
{
    echo "# Counters seen in $xml_path (best-effort scrape)"
    echo "# Open $trace_bundle in Instruments for the full report."
    if [[ -f "$xml_path" ]]; then
        grep -oE '(CYCLES|INST_[A-Z_]+|L1D[A-Z_]*|L2[A-Z_]*|FIXED_[A-Z_]+|MEM_[A-Z_]+|BRANCH_[A-Z_]+|cache[a-zA-Z_]*|cycles|instructions)' \
            "$xml_path" 2>/dev/null | sort -u || true
    else
        echo "(no XML extracted)"
    fi
} > "$manifest"

echo "Counters manifest: $manifest"

# Emit a single-row CSV with the metadata so this run can be joined to the
# wall-time results. Counter values themselves require manual extraction in
# Instruments (the .trace bundle is the source of truth).
{
    echo "variant,threads,grid_half,samples,max_depth,image_width,seed,taskpolicy,template,trace_bundle,counters_xml,counters_manifest"
    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
        "$variant" "$threads" "$grid_half" "$samples" "$max_depth" \
        "$image_width" "$seed" "$taskpolicy" "${template// /_}" \
        "$trace_bundle" "$xml_path" "$manifest"
} > "$counters_csv"
echo "Counters meta CSV: $counters_csv"
