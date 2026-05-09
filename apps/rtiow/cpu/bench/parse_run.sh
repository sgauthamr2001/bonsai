#!/bin/bash
# Convert one renderer run's outputs into a single CSV row matching the
# schema documented in the bench plan:
#
#   variant, threads, grid_half, samples, max_depth,
#   n_spheres, n_nodes, bvh_bytes, prims_bytes, total_scene_bytes,
#   setup_ms, render_ms, write_ms,
#   real_s, user_s, sys_s,
#   ppm_md5, taskpolicy, host_chip
#
# Usage:
#   parse_run.sh --header
#   parse_run.sh \
#       --variant=t10 --threads=10 --taskpolicy=userinteractive \
#       --stdout=/path/to/stdout.log \
#       --time=/path/to/time.log \
#       --ppm=/path/to/output.ppm
#
# stdout.log must contain the BENCH_STATS line emitted by main_hook.cpp.
# time.log must contain `/usr/bin/time -p` output (real/user/sys lines).
# Pass --ppm=- to skip the md5 (recorded as the literal string "skip").

set -euo pipefail

HEADER_LINE="variant,threads,grid_half,samples,max_depth,n_spheres,n_nodes,bvh_bytes,prims_bytes,total_scene_bytes,setup_ms,render_ms,write_ms,real_s,user_s,sys_s,ppm_md5,taskpolicy,host_chip"

if [[ "${1:-}" == "--header" ]]; then
    echo "$HEADER_LINE"
    exit 0
fi

variant=""
threads=""
taskpolicy=""
stdout_path=""
time_path=""
ppm_path=""

for arg in "$@"; do
    case "$arg" in
        --variant=*)    variant="${arg#--variant=}" ;;
        --threads=*)    threads="${arg#--threads=}" ;;
        --taskpolicy=*) taskpolicy="${arg#--taskpolicy=}" ;;
        --stdout=*)     stdout_path="${arg#--stdout=}" ;;
        --time=*)       time_path="${arg#--time=}" ;;
        --ppm=*)        ppm_path="${arg#--ppm=}" ;;
        --header)       echo "$HEADER_LINE"; exit 0 ;;
        *)
            echo "parse_run.sh: unknown arg: $arg" >&2
            exit 2
            ;;
    esac
done

for required in variant threads taskpolicy stdout_path time_path ppm_path; do
    if [[ -z "${!required}" ]]; then
        echo "parse_run.sh: missing --${required//_path/}=" >&2
        exit 2
    fi
done

if [[ ! -f "$stdout_path" ]]; then
    echo "parse_run.sh: stdout file not found: $stdout_path" >&2
    exit 1
fi
if [[ ! -f "$time_path" ]]; then
    echo "parse_run.sh: time file not found: $time_path" >&2
    exit 1
fi

bench_line="$(grep -E '^BENCH_STATS ' "$stdout_path" | tail -1 || true)"
if [[ -z "$bench_line" ]]; then
    echo "parse_run.sh: BENCH_STATS line not found in $stdout_path" >&2
    exit 1
fi

# Pull a value from the BENCH_STATS line by key (the line is space-separated
# key=value pairs).
bench_get() {
    local key="$1"
    local val
    val="$(printf '%s\n' "$bench_line" | tr ' ' '\n' | awk -F= -v k="$key" '$1==k{print $2; exit}')"
    if [[ -z "$val" ]]; then
        echo "parse_run.sh: missing key '$key' in BENCH_STATS line" >&2
        exit 1
    fi
    printf '%s' "$val"
}

grid_half="$(bench_get grid_half)"
samples="$(bench_get samples)"
max_depth="$(bench_get max_depth)"
n_spheres="$(bench_get n_spheres)"
n_nodes="$(bench_get n_nodes)"
bvh_bytes="$(bench_get bvh_bytes)"
prims_bytes="$(bench_get prims_bytes)"
total_scene_bytes="$(bench_get total_scene_bytes)"
setup_ms="$(bench_get setup_ms)"
render_ms="$(bench_get render_ms)"
write_ms="$(bench_get write_ms)"

# /usr/bin/time -p output:
#   real <seconds>
#   user <seconds>
#   sys  <seconds>
time_get() {
    local key="$1"
    awk -v k="$key" '$1==k{print $2; exit}' "$time_path"
}

real_s="$(time_get real)"
user_s="$(time_get user)"
sys_s="$(time_get sys)"
real_s="${real_s:-NA}"
user_s="${user_s:-NA}"
sys_s="${sys_s:-NA}"

if [[ "$ppm_path" == "-" || ! -f "$ppm_path" ]]; then
    ppm_md5="skip"
else
    # md5 -q on macOS, md5sum elsewhere.
    if command -v md5 >/dev/null 2>&1; then
        ppm_md5="$(md5 -q "$ppm_path")"
    else
        ppm_md5="$(md5sum "$ppm_path" | awk '{print $1}')"
    fi
fi

# Cache the host chip identification per process (cheap, but no need to repeat).
host_chip="${BENCH_HOST_CHIP:-}"
if [[ -z "$host_chip" ]]; then
    host_chip="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo unknown)"
fi
host_chip_csv="${host_chip// /_}"

printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$variant" "$threads" "$grid_half" "$samples" "$max_depth" \
    "$n_spheres" "$n_nodes" "$bvh_bytes" "$prims_bytes" "$total_scene_bytes" \
    "$setup_ms" "$render_ms" "$write_ms" \
    "$real_s" "$user_s" "$sys_s" \
    "$ppm_md5" "$taskpolicy" "$host_chip_csv"
