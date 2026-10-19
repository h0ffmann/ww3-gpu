# Four bit-neutral speed-ups of the W3XYPUG / W3XYPFSN2 kernel, each behind a switch

2026-10-19, Claude (agent), issue #196, after PR #202. Regtest pending ⚠ (ADR-0016).

## Question

Which changes to `kokkos/src/ww_kokkos/ug_xypug.hpp` and `xypug_shim.cpp` make the merged kernel
faster and keep every output bit, in the style of `docs/NOAA_PROGRESS_RECORD_202610.md` section 6
(each change behind a run-time switch, the bit argument written down)?

## What was done

Four commits, one switch each, read once by the shim (`speedup_flag`, default on, `0` gives the
earlier code). Commands per commit: `just kokkos-test serial-debug` (11/11),
`just kokkos-test openmp-release`, `bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --build-only
--kokkos resident` (built, nothing run on a GPU), and
`WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix 4 serial -R '^00[12]:'`.

| Switch | Change | Operations moved | Why the bits are the same |
|---|---|---|---|
| `WW_KOKKOS_XYPUG_FUSE_UT` | the first sub-step's `UTILDE` (`w3profsmd.F90:671`) is computed in the per-triangle pass (`:632-634`) from `FLALL` held in registers; `FLALL` is stored only if a later sub-step reads it (`LCALC` off and `ITER <= 1` skips it) | the dot product and the `NM *` product, once, earlier | same products and sums in the same order on the same operands; `U` is `AC`, complete after the barrier of the W3GATH pass and not written before the node update |
| `WW_KOKKOS_XYPUG_SKIP_H2D` | `MAPSTA, CLATS, ECOS, ESIN, CG, CX, CY, IOBP, IOBPD, IOBDP, IOBPA` are uploaded only when a `memcmp` against a host shadow differs | none | the device array has the bits of this call's host array |
| `WW_KOKKOS_XYPUG_LAYOUT` | `KELEM` is stored at its place in the node list (`pos`), so the `KKSUM` and `ST` gathers read it contiguously; `IOBPD` is `[ITH][IP]`, `CG` is `[IK][ISEA]` | none (addresses only) | each node adds its terms in ascending `IE`, then `K`, as before; `VA` is not touched |
| `WW_KOKKOS_XYPUG_CFL_PAR` | the per-node `SI/MAX(KKSUM*IOBDP, 1e-9)` of `:647` is made in the `KKSUM` pass into `KKSUM`'s slot; the minimum scan (`:648`) stays on one thread in ascending `IP` | the divisions, to the parallel pass | same operands and operations per node; the min chain order and its `x < c ? x : c` form are unchanged, so NaN and +0/-0 ties resolve as before |

No sum, limiter or integration order changed, no atomic, no parallel reduction, contraction stays
off. Static argument is by reading the source against the Fortran's order; a GIMPLE comparison of
the changed loops was not made ⚠.

## Result

Parity (v, 2026-10-19, WW3 `aa382393`, i9-14900, `WW3GPU_STRICT=ON`): the matrix above, run
after each of the four commits with the switches on, gives `ww3_tp2.6` `ST4` 0/36 and `ST4_ASCII`
0/38 files different, `nccmp-tol` PASS, 777 600 components on Kokkos; with all four switches at 0
after the last one, the same. Those runs were made before the commits were placed on top of the
fence after the geometry upload; the matrix run again at `106263d`, switches on, gives 0/36 and
0/38, PASS. Nothing else of the matrix was run.

Timing (v): `xypug.kernel` and `xypug.call` from `WW_KOKKOS_TIMERS=1`, `ww3_tp2.6` `ST4` entry's
Kokkos run (resident preset, 600 calls, OpenMP backend), switches toggled in one executable, three
alternated rounds per configuration, median, the start of each run waiting for 1-minute load below
2 and at most 2 runnable tasks (load at the end of a run is its own). Machine: Intel Core i9-14900,
32 hardware threads. Raw rows: `docs/data/xypug_speedups_tp2.6_2026-10-19.tsv`. Command per run:
`nix develop picasso/labs/pratico#ww3 --command env -C <stage> ... WW_KOKKOS_TIMERS=1
WW_KOKKOS_XYPUG_*=... run_cmake_test -o netcdf -s ST4_KOKKOS -w <dir> WW3/model ww3_tp2.6` with the
`ST4_KOKKOS` entry's environment printed by `run_entry.cmake -DDRY_RUN=ON`.

| Config | 32 threads, kernel s | ratio | 1 thread, kernel s | ratio |
|---|---|---|---|---|
| all off | 7.97 | 1.000 | 37.43 | 1.000 |
| `FUSE_UT` only | 6.79 | 0.852 | 36.14 | 0.966 |
| `SKIP_H2D` only | 8.10 | 1.017 | 37.00 | 0.989 |
| `LAYOUT` only | 7.96 | 0.998 | 36.82 | 0.984 |
| `CFL_PAR` only | 7.81 | 0.980 | 36.92 | 0.986 |
| all on | 6.57 | 0.824 | 36.50 | 0.975 |

The spread of the three runs is about 0.1 to 0.3 s at 32 threads and 0.2 to 2 s at 1 thread (one
all-on run took 46.2 s, an outlier), so only `FUSE_UT` and the sum at 32 threads are outside it.
`SKIP_H2D` takes the `xypug.h2d` total from 0.43 s to 0.06 s at 32 threads (the call timer
8.91 s to 7.09 s with all on) and is nothing at 1 thread. `LAYOUT` and `CFL_PAR` are not separated
from noise on this CPU; their target is the GPU, where gathers and one-thread scans cost more, not
measured ⚠. Only the ST4 entry's timers were read; the whole `ww3_shel` time was not compared ⚠.

## What it changes

All four switches default on, because the two entries pass with them on. The record gets a row
6.6. `bench/results/` gets no record: the timing needs a passed WW3 regtest replay on the commit
(ADR-0016 postpones it). The validation phase runs the whole matrix with the switches on.

## CPU re-measurement, 2026-10-19

Regtest pending ⚠ (ADR-0016). The owner asked for the three switches that were inside the spread
(`SKIP_H2D`, `LAYOUT`, `CFL_PAR`) to be timed again, with the whole `ww3_shel` time, on the CPU only.

**Method (v).** Same run as above: `ww3_tp2.6` `ST4_KOKKOS` entry through `run_cmake_test`, resident
preset, 600 `xypug` calls, OpenMP backend, `WW_KOKKOS_TIMERS=1`, the four switches set per run in the
environment of one executable (`build_1` of the matrix build at `fb31d39`, built 2026-10-19 12:57, not
rebuilt). `CUDA_VISIBLE_DEVICES=""` for every run; no `--gpu`, no CUDA recipe. Machine: Intel Core
i9-14900, 32 hardware threads. Before timing, the executable was checked to honour the switches:
`strings` finds the four names in `ww3_shel`, and `SKIP_H2D=1` takes `xypug.h2d` from 0.28 s to
0.05 s at 8 threads in a one-run check. Each run waited for a 1-minute load below 2 and at most 2
runnable tasks (`/proc/loadavg`), configurations in a random order per round (seed 19), whole-run
wall taken from `Elapsed time` in `ww3_shel.out` (WW3's own clock for the program, the file
`work_*/ww3_shel.out`), the `xypug` timers from the same file. "Real" below means the medians differ
by more than the larger of the two min-max spreads; otherwise "inside the spread".

**What was cut ⚠.** The plan was 5 rounds. Each run waited about a minute for the load to fall, so a
round took about 40 minutes and 5 rounds would have passed the 2-hour budget. First the 8-thread
leave-one-out runs were dropped (they were not run at all), then the sweep was stopped after 3
complete rounds (57 runs: 32 threads 10 configs, 8 threads 6 configs, 1 thread 3 configs). The
medians are of three runs, not five. A first sweep that included the 8-thread leave-one-out runs was
stopped after 3 runs and discarded.

Raw rows: `docs/data/xypug_speedups_cpu_rerun_2026-10-19.tsv`. Command per run:
`env -C <regtests dir> CUDA_VISIBLE_DEVICES= CMAKE_OPTIONS="-DWW_KOKKOS=ON
-DCMAKE_PREFIX_PATH=<repo>/kokkos/build/openmp-release/install" path_build_root=<build_1>
OMP_NUM_THREADS=<n> OMP_PROC_BIND=false WW_KOKKOS_ALL=0 WW_KOKKOS_STRICT=1 WW_KOKKOS_VA_POISON=1
WW_KOKKOS_{CPRT,OUTG,IOPE,XYP3,KTP3,SRCE,UPDV}=1 WW_KOKKOS_PROP3_COUNT=1 WW_KOKKOS_TIMERS=1
WW_KOKKOS_XYPUG_*=0|1 WW3/regtests/bin/run_cmake_test -o netcdf -s ST4_KOKKOS -w <dir>
WW3/model ww3_tp2.6`, inside `nix develop picasso/labs/pratico#ww3`.

Median (min-max) in seconds, `xypug.kernel` / whole `ww3_shel` (`Elapsed time`) / `xypug.h2d`:

| Threads | Config | kernel | `ww3_shel` | h2d |
|---|---|---|---|---|
| 32 | all off | 7.87 (7.80-7.98) | 9.78 (9.71-9.87) | 0.419 |
| 32 | `FUSE_UT` only | 6.88 (6.86-7.02) | 8.81 (8.73-8.89) | 0.415 |
| 32 | `SKIP_H2D` only | 7.84 (7.83-7.91) | 9.35 (9.35-9.42) | 0.053 |
| 32 | `LAYOUT` only | 7.76 (7.68-7.81) | 9.73 (9.62-9.74) | 0.449 |
| 32 | `CFL_PAR` only | 7.86 (7.73-7.89) | 9.78 (9.66-9.82) | 0.422 |
| 32 | all but `FUSE_UT` | 7.89 (7.80-8.04) | 9.49 (9.31-9.58) | 0.055 |
| 32 | all but `SKIP_H2D` | 6.97 (6.96-7.07) | 8.89 (8.89-8.99) | 0.438 |
| 32 | all but `LAYOUT` | 6.54 (6.53-6.55) | 8.09 (8.07-8.11) | 0.059 |
| 32 | all but `CFL_PAR` | 6.50 (6.49-6.60) | 8.03 (8.01-8.14) | 0.058 |
| 32 | all on | 6.51 (6.39-6.57) | 8.05 (7.91-8.20) | 0.056 |
| 8 | all off | 8.75 (8.16-9.04) | 10.46 (9.80-10.73) | 0.273 |
| 8 | `FUSE_UT` only | 7.37 (7.21-7.95) | 9.02 (8.88-9.64) | 0.260 |
| 8 | `SKIP_H2D` only | 8.12 (8.04-8.65) | 9.53 (9.48-10.11) | 0.042 |
| 8 | `LAYOUT` only | 8.28 (8.22-8.74) | 10.00 (9.94-10.52) | 0.326 |
| 8 | `CFL_PAR` only | 8.72 (8.41-8.94) | 10.44 (10.10-10.65) | 0.283 |
| 8 | all on | 7.05 (7.01-7.80) | 8.46 (8.42-9.25) | 0.041 |
| 1 | all off | 36.59 (36.57-36.61) | 38.26 (38.23-38.27) | 0.030 |
| 1 | `FUSE_UT` only | 35.89 (35.85-35.99) | 37.54 (37.52-37.64) | 0.030 |
| 1 | all on | 35.97 (35.92-36.04) | 37.64 (37.59-37.71) | 0.038 |

**Verdict per switch on this CPU (v, three rounds, regtest pending ⚠).**

- `FUSE_UT`: helps. Alone against all off, kernel minus 0.98 s and `ww3_shel` minus 0.97 s at 32
  threads (real), minus 1.38 s and 1.44 s at 8 threads (real), minus 0.70 s and 0.72 s at 1 thread
  (real). Removing it from all-on costs 1.37 s of kernel and 1.44 s of `ww3_shel` at 32 threads.
- `SKIP_H2D`: helps the whole run at 32 threads, not the kernel timer alone. `xypug.h2d` falls from
  0.42 s to 0.05 s. `ww3_shel` alone minus 0.43 s (real), removed from all-on plus 0.84 s (real); the
  kernel timer alone moves 0.03 s (inside the spread), but all-on against all-but-`SKIP_H2D` differs
  by 0.45 s in the kernel timer too (real), which I did not explain ⚠. At 8 threads alone, kernel
  minus 0.63 s (inside the spread) and `ww3_shel` minus 0.93 s, which equals the larger spread, so
  by the rule it is at the edge ⚠. Not measured alone at 1 thread (h2d is 0.03 s there).
- `LAYOUT`: inside the spread at 32 and 8 threads, alone (kernel minus 0.11 s and 0.47 s) and as
  leave-one-out at 32 threads (0.03 s). No gain shown on this CPU.
- `CFL_PAR`: inside the spread at 32 and 8 threads, alone (0.01 s and 0.03 s) and as leave-one-out
  (0.01 s). No gain shown on this CPU. Neither switch hurts.

All on against all off: kernel 7.87 to 6.51 s and `ww3_shel` 9.78 to 8.05 s at 32 threads, 8.75 to
7.05 s and 10.46 to 8.46 s at 8 threads, 36.59 to 35.97 s and 38.26 to 37.64 s at 1 thread (all
real). The earlier table's all-off and all-on figures (7.97 s and 6.57 s) agree within 0.1 s. The
`Elapsed time` of `ww3_shel` includes about 1.5 to 1.9 s outside the `xypug` calls that no switch
changes. Only this entry was timed, the machine was shared (load at the end of a run was 1.2 to
7.3, see the load columns), and `LAYOUT` and `CFL_PAR` were aimed at the GPU, which was not
measured ⚠.
