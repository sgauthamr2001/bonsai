# Benchmarking Harness for RTIOW

Benchmarking harness for the Bonsai-generated RTIOW renderer
in `apps/rtiow/cpu/bench`.  The goal of this benchmark is to setup a harness
to figure out for what kind of workloads at a given hierachy of memory capcity 
distribution and sharding of an acceleration structure could be beneficial. To 
setup the harness, we design it around a M4 Pro CPU  at the L1D <-> L2 level,
with a largest working set of 2.5 MB which is comfortably below the 4 MB L2.

> **Scope limit you should remember**: this bench cannot speak to L2 -> SLC
> or L2 -> DRAM partitioning gains. The largest scene we can build still
> fits in L2. What we *can* measure is the L1D crossing as the BVH grows,
> shared-L2 contention as P-cores are added, and the inner-loop reuse
> behavior (samples sweep).

---

## Layout

```
apps/rtiow/cpu/bench/
  README.md                     <- this file
  build_variant.sh              <- build one bonsai_threaad{N}.out variant
  build_all.sh                  <- build N in {1,2,4,8,10,14}
  run_one.sh                    <- run a single config -> CSV row
  parse_run.sh                  <- stdout + time output -> CSV row
  run_grid_sweep.sh             <- vary RTIOW_GRID_HALF, threads=10
  run_thread_sweep.sh           <- vary variant t{N} at fixed grids
  run_samples_sweep.sh          <- vary RTIOW_SAMPLES at fixed grid+threads
  run_xctrace.sh                <- Apple Instruments counters for one config
  run_all.sh                    <- orchestrate build + 3 sweeps + plots
  plot_results.py               <- matplotlib plots for the three CSVs
  build/t{N}/                   <- per-variant generated artifacts
                                  (main.bonsai, main.h, main.o, main.bir,
                                  main.ll, main_hook.cpp, bonsai.out)
  results/
    grid_sweep.csv              <- one row per grid size
    thread_sweep.csv            <- one row per (grid, thread variant)
    samples_sweep.csv           <- one row per samples value
    plots/                      <- the three .png plots
    _run/                       <- per-iteration stdout/time/output.ppm
    _xctrace/                   <- per-anchor .trace bundles + manifests
```

---

## Quick start

From `bonsai/` repo root, after `cmake --build build` once:

```bash
bash apps/rtiow/cpu/bench/run_all.sh
```

Defaults are sized for ~10-20 minutes on the M4 Pro. Override with env vars
(see top of `run_all.sh`). Examples:

```bash
# Faster smoke run
SAMPLES=4 GRIDS="11 30 60" VARIANTS="1 10" bash apps/rtiow/cpu/bench/run_all.sh

# Just re-plot from existing CSVs
SKIP_BUILD=1 SKIP_GRID=1 SKIP_THREAD=1 SKIP_SAMPLES=1 \
  bash apps/rtiow/cpu/bench/run_all.sh

# Add Apple Instruments counters at the mid-grid t10 anchor
WITH_XCTRACE=1 bash apps/rtiow/cpu/bench/run_all.sh
```

---

## Threading model in Bonsai's: what the variants actually do

Bonsai's schedule lives in [`apps/rtiow/cpu/main.bonsai`](../main.bonsai)
lines 207-208. `image.split(i0, io, ii, CHUNK, false).cpu_thread(io)` lowers
to `dispatch_apply_f` on Apple's Grand Central Dispatch (see
[`bonsai/src/CodeGen/CodeGen_LLVM.cpp:2710`](../../../../src/CodeGen/CodeGen_LLVM.cpp)).
GCD's pool can spawn more workers than the number of chunks, but it cannot
run more chunks in parallel than exist, so chunk count caps concurrency.

### Correctness constraint: chunk_size must divide image_height

Bonsai's `split(...)` schedule has only one mode (`generate_tail = false`);
the inner `forall` runs the full chunk size with no bounds check (see
[`bonsai/src/Lower/LoopTransforms.cpp:145`](../../../../src/Lower/LoopTransforms.cpp)
which `internal_assert`s that the tail strategy is unimplemented). If
`chunk_size` does not exactly divide `image_height`, the renderer writes
past the framebuffer's allocated region.

The bench therefore fixes `RTIOW_IMAGE_WIDTH = 1280` (so image_height = 720
under aspect 16:9) and only allows N values that divide 720. The canonical
set is {1, 2, 4, 8, 10, 12}. We use t12 (not t14) for "P-cluster (there are 10 p-cores)
oversubscription" because 720 / 14 isn't integer. `build_variant.sh`
enforces this with a build-time guard.
---

## P-core "pinning" on macOS (it is soft, and not via taskpolicy)

macOS does not expose strict CPU affinity to userspace. The `taskpolicy(8)`
tool *only* exposes **downgrade** clamps via `-c`: `utility`, `background`,
`maintenance`. All three push work toward E-cores; there is no positive
"prefer P-cores" clamp. So our default is `--taskpolicy=default` (i.e. no
wrapper). With the t10 variant we emit 10 worker chunks; the GCD default
queue runs them on the 10 P-cores in practice on M4 Pro.


---

## Evaluation plots and what they measure

The benchmark is designed to evaluate whether partitioning/sharding is a
promising optimization at the L1D <-> L2 level. Each sweep isolates one axis.

### `plots/grid_sweep_render_vs_bvh.png` (grid sweep)

- **Varies**: `RTIOW_GRID_HALF` (scene/BVH size).
- **Fixed**: samples, thread variant (e.g. `t10`), `max_depth`.
- **Output metrics**: `render_ms`, `bvh_bytes`, `n_nodes`, `real_s`, `user_s`.

This plot measures how runtime changes as BVH footprint grows while
ray count and parallelism are held constant. If runtime steepens
sharply after L1D-sized regions are exceeded, one plausible
interpretation is that threads are making more frequent L2->L1 data
moves. A partitioned/sharded layout could reduce that pressure by
letting workers operate on smaller hot regions that fit better in local
caches. This remains a hypothesis until validated with hardware
counters (L1D misses, L2 hits/misses, and stall cycles), which are a
next step for this benchmark.

### `plots/thread_sweep_speedup.png` (thread sweep)

- **Varies**: thread/chunk variants (`t1 ... t12`).
- **Fixed**: three representative grid sizes (`small`, `mid`, `max`), samples,
  `max_depth`.
- **Output metrics**: `render_ms`, speedup vs `t1`, ideal speedup line.

This plot measures scaling and falloff from ideal. Increasing
falloff at larger grids is a signal that shared-data pressure (including
possible L2 contention) may be rising. Sub-linear scaling alone is not proof
of cache contention, but it identifies where hardware counters should be
collected.

### `plots/samples_sweep.png` (samples sweep)

- **Varies**: `RTIOW_SAMPLES`.
- **Fixed**: scene/grid, thread variant, `max_depth`.
- **Output metrics**: `render_ms` vs samples.

This is a fixed-footprint control experiment. By keeping BVH size and thread
layout fixed, it isolates the marginal cost of tracing more rays per pixel.
Near-linear scaling means each extra sample has roughly constant cost; clear
sub-linear behavior suggests reuse/amortization effects across samples. A
scaling slope greater than 1 can indicate increasing contention.

---

## Acknowlegements: 

Claude was used to generate parts of the scripts. Correctness was verified my checking the rendered images for different benchmarks. 

---