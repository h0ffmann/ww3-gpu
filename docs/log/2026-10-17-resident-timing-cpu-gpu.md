# `ww3_shel` with the resident Kokkos preset against pure NOAA WW3: one timed run on the CPU

2026-10-17, an agent for M. Hoffmann; issue #42,
[WFIP-0005](../WFIPs/WFIP-0005-device-resident-step.md),
[ADR-0016](../ADRs/ADR-0016-speed-first-regtests-postponed-one-person-loop.md).

## Question

Wiring covers 99.3 % of the profiled run time already (`kokkos/PORT_STATUS.md`, port score), and
the only committed timing is the 2026-09-15 kernel benchmark of `W3SNL1` alone. Is `ww3_shel`
faster or slower end to end with the resident preset on, and are its outputs still the Fortran's
bytes? The expectation before the run was a ratio near 1 on the CPU: the lab switch has `OMPG`,
so the Fortran already runs `W3WAVE`'s sea-point loop in OpenMP threads
(`model/src/w3wavemd.F90:1894`, `!$OMP DO SCHEDULE (DYNAMIC,1)`, at `WW3/` `769c44f0`), and the
port does the same arithmetic with contraction off.

## What was done

- `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident --repeat 1`, in a worktree of
  `main` at `47c7664`, with `WW3/` at `769c44f0`, `WW3-noaa/` at `03adda91` and `picasso/` at
  `be76132`. `--kokkos resident` expands to `SRCE,XYP3,KTP3,OUTG,IOPE,UPDV`. The case is
  `bench_small` (`full`): 120 x 80, 9 520 sea points, NK=24 NTH=24, 12 hours, switch
  `switch_lab_ompg`.
- The script runs one executable twice, every `WW_KOKKOS_*` at 0 (the Fortran) and the preset on
  (Kokkos), then builds and runs pure NOAA WW3 (`--ww3`, from `WW3-noaa/`), one run each.
- Machine: Intel Core i9-14900, 32 threads, OpenMP. gfortran and g++ 16.2.0, Kokkos 5.2.2 OpenMP
  (`kokkos-openmp-5.2.2` from the picasso flake).
- **The machine was shared.** Another session ran its own `coast` and `full` timings at the same
  time: load average 35.9 at the start of the run and 46.5 at its end, on 32 hardware threads.
- After the run, `cmp` of every output file of the three run directories under
  `build/runs/`, and `diff` of `ncdump` of the two NetCDF files that `cmp` found different.

## Result

| `ww3_shel` wall time, `full`, CPU, one run each | s | ratio |
|---|---|---|
| `--ww3`, pure NOAA WW3 | 43.09 | 1.00x |
| `--ww3-gpu`, the Fortran of the same executable | 41.92 | 1.03x |
| `--ww3-gpu`, `--kokkos resident` | 17.98 | 2.40x against `--ww3`, 2.33x against the same executable |

- ⚠ One run each, on a loaded machine: regtest pending (ADR-0016), not a median of three, and not
  for a table (`scripts/results.py check` refuses fewer than three runs). Two things make it more
  than noise: the two Fortran executables, run minutes apart, agree within 3 %, and another
  session measured 1.95x for the same preset and case on its integration branch the same evening.
- (v) Kokkos against the Fortran of the same executable: `out_grd.ww3`, `ww3.field.nc` and
  `mod_def.ww3` byte-identical (`cmp`).
- (v) Kokkos against `--ww3`: `out_grd.ww3` and `mod_def.ww3` byte-identical. `ww3.field.nc`
  differs in one line of `ncdump`: the global attribute `WAVEWATCH_III_switches`, which the fork's
  build ends with `KOKKOS`. Every variable is the same; `nccmp-tol` judged `hs` and found
  `max|d| = 0`.
- The case writes no `restart.ww3` and no `out_pnt.ww3`, so neither was compared.
- The guess before the run (a ratio near 1) is not supported. Where the time goes was not
  measured: no `--timers` run was made.

## What it changes

- Two hypotheses could explain a 2.3x gain with the same arithmetic. Each is ⚠ until measured:
  - the preset moves output (`OUTG`), I/O (`IOPE`) and `UPDV` into OpenMP-parallel Kokkos, so the
    parts of the step that are serial in the Fortran no longer dominate at 32 threads;
  - the source-term kernels add parallelism inside a sea point (one team per point, threads over
    spectral bins), where `OMPG` only splits sea points.
- Next, on a quiet machine:
  - one `--timers` run, and the preset's parts turned on one at a time, to say which part gives
    the gain;
  - `--repeat 3` for a median that can enter `bench/results/`.
- The same case on the GPU (`--gpu`) needs the owner at the terminal. Its result is added below
  as an update.
