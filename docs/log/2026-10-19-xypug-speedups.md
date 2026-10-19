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
