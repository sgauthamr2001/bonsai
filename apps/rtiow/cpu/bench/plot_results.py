#!/usr/bin/env python3
"""Plot the three bench sweeps (grid, thread, samples).

Inputs (all under apps/rtiow/cpu/bench/results/):
    grid_sweep.csv       (render_ms vs grid_half / bvh_bytes)
    thread_sweep.csv     (render_ms vs threads at fixed grid sizes)
    samples_sweep.csv    (render_ms vs samples at one fixed grid)

Outputs (under apps/rtiow/cpu/bench/results/plots/):
    grid_sweep_render_vs_bvh.png
    thread_sweep_speedup.png
    samples_sweep.png

Dependencies: matplotlib only. Install with `pip install matplotlib`.

Usage:
    python3 apps/rtiow/cpu/bench/plot_results.py
"""

from __future__ import annotations

import csv
import os
import sys
from collections import defaultdict
from pathlib import Path

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    sys.stderr.write(
        "plot_results.py: matplotlib not installed. Try `pip install matplotlib`.\n"
    )
    sys.exit(1)


SCRIPT_DIR = Path(__file__).resolve().parent
RESULTS_DIR = SCRIPT_DIR / "results"
PLOTS_DIR = RESULTS_DIR / "plots"

# Cache size guides (bytes) for the M4 Pro per `sysctl`. We mark these as
# vertical lines in plots whose x-axis is bvh_bytes or related.
L1D_BYTES = 64 * 1024  # hw.l1dcachesize
L2_BYTES = 4 * 1024 * 1024  # hw.l2cachesize (per cluster on Apple Silicon)


def load_csv(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        sys.stderr.write(f"plot_results.py: missing {path}\n")
        return []
    with path.open() as f:
        return list(csv.DictReader(f))


def to_float(s: str) -> float | None:
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def average(values: list[float]) -> float | None:
    if not values:
        return None
    return sum(values) / len(values)


def group_avg(rows: list[dict[str, str]], key_fields: list[str], value_field: str):
    """Group rows by tuple(key_fields), average the value_field across reps.

    Returns a dict: tuple-of-key-values -> averaged float.
    """
    buckets: dict[tuple, list[float]] = defaultdict(list)
    for row in rows:
        key = tuple(row.get(k, "") for k in key_fields)
        v = to_float(row.get(value_field, ""))
        if v is None:
            continue
        buckets[key].append(v)
    return {k: average(vs) for k, vs in buckets.items() if vs}


def annotate_cache_guides(ax, *, log_x: bool = True, top_label: str = "") -> None:
    """Add vertical L1D and L2 guides to a render-vs-bytes axis."""
    ax.axvline(L1D_BYTES, linestyle="--", linewidth=1, alpha=0.6, label="L1D (64 KB)")
    ax.axvline(L2_BYTES, linestyle="--", linewidth=1, alpha=0.6, label="L2 (4 MB)")
    if top_label:
        ax.text(
            L1D_BYTES,
            ax.get_ylim()[1],
            "  L1D",
            va="top",
            ha="left",
            fontsize=8,
            alpha=0.7,
        )
        ax.text(
            L2_BYTES,
            ax.get_ylim()[1],
            "  L2 (cluster)",
            va="top",
            ha="left",
            fontsize=8,
            alpha=0.7,
        )


def plot_grid_sweep(rows: list[dict[str, str]]) -> Path | None:
    """render_ms vs bvh_bytes, log-log, with L1D/L2 guides."""
    if not rows:
        return None

    avg = group_avg(rows, ["bvh_bytes", "grid_half"], "render_ms")
    if not avg:
        return None

    points = sorted(
        (int(b), int(g), v) for ((b, g), v) in avg.items()
    )
    xs = [p[0] for p in points]
    ys = [p[2] for p in points]
    grid_labels = [p[1] for p in points]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(xs, ys, marker="o")
    for x, y, gh in zip(xs, ys, grid_labels):
        ax.annotate(
            f"g={gh}", (x, y), textcoords="offset points", xytext=(4, 4), fontsize=7
        )
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("bvh_bytes (log)")
    ax.set_ylabel("render_ms (log)")
    ax.set_title("Grid sweep: render_ms vs BVH size (log-log)")
    annotate_cache_guides(ax, top_label="cache")
    ax.legend(loc="best", fontsize=8)
    ax.grid(True, which="both", alpha=0.3)

    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    out = PLOTS_DIR / "grid_sweep_render_vs_bvh.png"
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def plot_thread_sweep(rows: list[dict[str, str]]) -> Path | None:
    """speedup vs N, one line per grid_half, plus the ideal y=x line."""
    if not rows:
        return None

    avg = group_avg(rows, ["grid_half", "threads"], "render_ms")
    if not avg:
        return None

    by_grid: dict[int, dict[int, float]] = defaultdict(dict)
    for (g, t), v in avg.items():
        try:
            by_grid[int(g)][int(t)] = v
        except ValueError:
            continue

    fig, ax = plt.subplots(figsize=(8, 5))
    max_threads = 1
    for grid in sorted(by_grid):
        thread_to_render = by_grid[grid]
        threads_sorted = sorted(thread_to_render)
        if 1 not in thread_to_render:
            sys.stderr.write(
                f"plot_thread_sweep: no t1 baseline for grid={grid}; "
                "skipping speedup line\n"
            )
            continue
        baseline = thread_to_render[1]
        speedups = [baseline / thread_to_render[t] for t in threads_sorted]
        ax.plot(threads_sorted, speedups, marker="o", label=f"g={grid}")
        max_threads = max(max_threads, max(threads_sorted))

    ideal = list(range(1, max_threads + 1))
    ax.plot(ideal, ideal, linestyle="--", alpha=0.5, label="ideal")

    ax.set_xlabel("threads (target chunk concurrency)")
    ax.set_ylabel("speedup over t1")
    ax.set_title("Thread sweep: speedup vs N (per grid size)")
    ax.legend(loc="best", fontsize=8)
    ax.grid(True, alpha=0.3)
    ax.set_xticks(ideal)

    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    out = PLOTS_DIR / "thread_sweep_speedup.png"
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def plot_samples_sweep(rows: list[dict[str, str]]) -> Path | None:
    """render_ms vs samples (log-log)."""
    if not rows:
        return None

    avg = group_avg(rows, ["samples", "grid_half"], "render_ms")
    if not avg:
        return None

    by_grid: dict[int, list[tuple[int, float]]] = defaultdict(list)
    for (s, g), v in avg.items():
        try:
            by_grid[int(g)].append((int(s), v))
        except ValueError:
            continue

    fig, ax = plt.subplots(figsize=(8, 5))
    for grid, pts in by_grid.items():
        pts.sort()
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        ax.plot(xs, ys, marker="o", label=f"g={grid}")
        if len(xs) >= 2 and xs[0] > 0 and ys[0] > 0:
            xs_ref = xs
            ref0 = ys[0] / xs[0]
            ys_ref = [ref0 * x for x in xs_ref]
            ax.plot(
                xs_ref, ys_ref, linestyle="--", alpha=0.4,
                label=f"ideal linear (g={grid})",
            )

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("samples_per_pixel (log)")
    ax.set_ylabel("render_ms (log)")
    ax.set_title("Samples sweep: render_ms vs samples")
    ax.legend(loc="best", fontsize=8)
    ax.grid(True, which="both", alpha=0.3)

    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    out = PLOTS_DIR / "samples_sweep.png"
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out


def main() -> int:
    grid_rows = load_csv(RESULTS_DIR / "grid_sweep.csv")
    thread_rows = load_csv(RESULTS_DIR / "thread_sweep.csv")
    samples_rows = load_csv(RESULTS_DIR / "samples_sweep.csv")

    produced: list[Path] = []
    p = plot_grid_sweep(grid_rows)
    if p:
        produced.append(p)
    p = plot_thread_sweep(thread_rows)
    if p:
        produced.append(p)
    p = plot_samples_sweep(samples_rows)
    if p:
        produced.append(p)

    if not produced:
        sys.stderr.write(
            "plot_results.py: no plots produced. Make sure the sweep CSVs "
            "exist and have data.\n"
        )
        return 1

    print("Produced:")
    for p in produced:
        print(f"  {p}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
