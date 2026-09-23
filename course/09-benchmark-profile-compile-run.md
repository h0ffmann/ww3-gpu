# 09 — Measure first: benchmark, profile, compile options, run configuration

This lesson is steps 1 and 2 of the proposal's ladder (`pubs/proposal/pt/04-scope.md`):
everything you can do to a WW3 installation **without touching a line of Fortran**. It is
also where the instrument for every later lesson is built. Nothing in 10–13 means anything
without a reference run, a metric and a profile.

## The reference run

Freeze five things and write them down: the code revision (`git -C $WW3 rev-parse HEAD`),
the switch file, every namelist, the grid and the forcing. The proposal calls this the
*execução de referência congelada*; every number in the course is relative to it.

The lab's smallest reference is an upstream regtest run step by step (v):

```bash
just rt ww3_tp1.1 PR3_UQ      # build with the test's own switch_PR3_UQ, then run it
```

`just rt` chains `just build` and `just regtest`; the latter is
`scripts/03_run_regtest.sh`, which copies `regtests/<test>/input/` into
`regtests/<test>/work_lab/` and runs `ww3_grid`, `ww3_strt`, `ww3_prnc`, `ww3_bounc`,
`ww3_shel`, `ww3_ounf`, `ww3_ounp` in order, each only if its `.nml`/`.inp` is present (v).
It deliberately does not use `matrix.comp`: the point is to watch the pipeline. The
operational case at LabECO gets the same treatment once the lab captures it.

## The metric

**Wall-clock seconds per forecast hour of `ww3_shel`.** Not CPU time, not the whole
pipeline. `bench/README.md` lists the traps (v): `CPU_TIME` sums across OpenMP threads and
makes threading look like a slowdown, so use `SYSTEM_CLOCK`; `ww3_grid` is serial setup
and is excluded; field output is set to stride `'0'` so you measure compute, not the
filesystem; run more than once and report medians because both an i9 and a 4090 clock
down under sustained load. If you want reproducible numbers, lock the clocks
(`bench/README.md` has the `nvidia-smi -lgc` / `cpupower` lines).

## `bench/`

| What | Program | Measures |
|---|---|---|
| The real WW3, CPU only | `bench/bench_ww3_cpu.sh <build> [case] [max-ranks]` | wall time of `ww3_shel` at 1, 2, 4, … ranks; writes `results_ww3_cpu.csv` with `ranks,wall_s,speedup,efficiency,notes` (v) |
| A WW3-shaped kernel | `bench/kernel_bench.f90` | CPU vs GPU, data resident vs copied every step (v) |
| Everything | `bench/run_all.sh [build]` | the kernel proxies if `nvfortran` exists, then the real-WW3 scaling (v) |

The case itself comes from the C++ generator: `ww_bench_case --size small|medium|large`
(or `--nx --ny --nk --nth --hours`) `-o DIR` writes `depth.inp`, `mask.inp`, the three
namelists and a `case.json`, with the timesteps derived from the CFL condition so a bigger
grid stays stable and does the work it claims to do. `just bench` builds it in
`kokkos/build/openmp-release/tools/bench_case/` first. Requires a build with
`switches/switch_lab_mpi` (`DIST MPI`) (v).

Read `bench_ww3_cpu.sh`'s closing note before believing the curve (v): on an i9, P-cores
and E-cores differ so much that an MPI job runs at the pace of its slowest rank. Rerun
pinned (`mpirun -np 8 --bind-to core --cpu-set 0-15`) and compare; eight P-cores beating
24 mixed cores is normal for a memory-bound spectral model. If efficiency is flat from
rank 1, the case is too small: use a larger `--size`.

## Profile

```bash
just profile ww3_tp1.1          # kokkos/tools/profile/gprof_table.sh <ww3-dir> <regtest>
```

`gprof_table.sh` rebuilds WW3 with `-pg`, runs the regtest, and turns `gprof -b` into a
table `routine | self % | cumulative %` for the top 25 routines, grouped by phase with a
name map: `w3srce*` → source terms, `w3pro*`/`w3uqck*` → propagation,
`w3gath`/`w3scat`/`mpi_` → communication, `w3io*` → I/O. `perf_table.sh` produces the
same table from `perf record -g` where `perf` exists (it is not in the pratico shell).
`exercises/solutions/ex10_profile.sh` is the worked version.

What to expect, as priors only (`docs/AGENTS_KOKKOS_202609.md` §2.1, ⚠ not measured here):

| Bucket | WW3 modules | Typical share |
|---|---|---|
| Source terms | `w3srcemd` (`W3SRCE`), `w3snl1md`, `w3src4md`/`w3src6md`, `w3sbt*`, `w3sdb1md` | 40–65 % |
| Spatial propagation | `w3pro2md`/`w3pro3md`, `w3uqckmd` | 15–30 % |
| Intra-spectral propagation | `W3KTP2/3` | 5–10 % |
| Gather/scatter transposes, MPI | `W3GATH`/`W3SCAT` | 5–25 %, grows with rank count |
| Output and restart I/O | `w3iogomd`, `w3iorsmd`, `w3iopomd` | 5–20 % |

Profile at 1, 4 and 16 ranks (the proposal's methodology). The communication share is
the number that changes, and it is the argument for or against more ranks.

## WW3's own matrix: the bit-for-bit gate

WW3 ships 62 regression cases under `regtests/` (v, counted in the clone at `~/src/WW3`).
`regtests/bin/matrix.base` generates the run script from a list of options that includes
`rstrt_b4b`, `nth_b4b` and `npl_b4b` (v): restart, thread-count and MPI-task-count
reproducibility variants, each of which runs a case twice and demands identical output.
`regtests/bin/matrix.comp` compares two matrix runs file by file with `cmp` for binaries
and `diff` for text, and sorts every case into "identical" or "non-identical" (v). There is
no tolerance anywhere in it: any numerical difference is a failure that a human then
judges. Unit tests exist since 2023 and cover only I/O (`pubs/proposal/pt/05-justification.md`).

That is exactly the right gate for steps 1 and 2. A compiler flag, a switch or a namelist
change that alters no floating-point *ordering* must reproduce the reference bit for bit,
and the matrix will tell you. A change that reorders operations (vectorisation width,
`-Ofast`, a different MPI decomposition of a reduction) is the case for the next section.

## Step 1: the compile-option matrix

Vary one axis at a time. Everything here is baked in at build time
(`scripts/02_build_ww3.sh` wipes `build/` on every run (v)).

| Axis | Values worth trying | Parity expectation |
|---|---|---|
| Compiler | gfortran 15.3 (pinned in pratico (v)); `nvfortran` on the CPU as a stepping stone (lesson 01) | different compilers: rounding-level differences, use `nccmp-tol` |
| Flags | `-DCMAKE_BUILD_TYPE=Release` vs `Debug`; `-O2` vs `-O3 -march=native` | `-O3` may vectorise reductions → check b4b first |
| Parallel switch | `SHRD` (serial), `DIST MPI`, plus `OMPG`/`OMPH` for OpenMP on top (v `switches/README.md`) | `npl_b4b`/`nth_b4b` |
| Output | `FILE%NETCDF` 3 vs 4 in `ww3_ounf.nml` and the output stride; `NC4` in the switch file is inert in 7.14 (not in `switches.json`, no `W3_NC4` guard) (v) | none; it is I/O |

`exercises/solutions/ex09_matrix.sh` runs three of these axes on `ww3_tp1.1` and
tabulates wall-clock and the comparator's verdict; `exercises/ex09_bench.md` is the sheet.

## MPI × OpenMP on one node

With `switch_lab_mpi` you have ranks; with `OMPG OMPH` added you have threads inside
ranks. On one workstation the layout question is: how many ranks, how many threads each,
pinned where.

| Layout | When it wins |
|---|---|
| Pure MPI, one rank per physical core, pinned | almost always on a workstation; WW3's "card deck" decomposition over sea points balances well |
| Fewer ranks, threads inside | when the rank count is so high that the gather/scatter bucket dominates the profile |
| Ranks on hyperthreads | rarely; test one rank per physical core against one per logical core and keep the winner |

Set `OMP_NUM_THREADS` explicitly, set `OMP_PROC_BIND` explicitly (Kokkos warns if you do
not (v `kokkos/CMakeLists.txt`)), and always pass `--bind-to core`. Then run the
`npl_b4b`/`nth_b4b` variants: the layout must not change the answer.

## Step 2: run-configuration knobs, one at a time

These are namelist edits only. Change one, rerun, compare, keep or revert.

| Knob | Where | Notes |
|---|---|---|
| `TIMESTEPS%DTMAX`, `DTXY`, `DTKTH`, `DTMIN` | `ww3_grid.nml` (v) | the template's own guidance: `DTXY ≈ 90 % of the CFL step, DTMAX ≈ 3·DTXY, DTKTH ≈ DTMAX/2 (or /10 with strong currents), DTMIN ≈ 10 s` (v). Lesson 02 derives them. |
| Output frequency and field list | `ww3_shel.nml`: `TYPE%FIELD%LIST`, `*%TIMESTRIDE` (v) | the fastest I/O is the field you do not write |
| Restart stride | `ww3_shel.nml` restart block | `rstrt_b4b` proves a restarted run equals the uninterrupted one |
| Forcing interpolation | `WNT1 WNX1` / `CRT1 CRX1` in the switch (v) | compile-time, so strictly step 1 |

## When reordering breaks b4b: `nccmp-tol`

```bash
nccmp-tol REF.nc TEST.nc                      # default kokkos/tools/nccmp-tol/tolerances.txt
nccmp-tol REF.nc TEST.nc my_tolerances.txt    # just nccmp does the same
```

For every variable listed in the tolerances file (rows `hs`, `fp`, `dir`, `dp`, `t0m1` by
default; format `name abs rel`) it prints max-abs, RMS and max-relative differences over
non-fill values and exits 0 only if every judged variable passes. Unlisted variables are
reported, not judged. The tolerances file is versioned and approved by the co-advisor:
that is the proposal's *comparador por campo*, and it is what lessons 10–13 use once the
matrix's yes/no answer is no longer the right question.

> **The GPU question, short version.** Yes, NVIDIA ships a production Fortran compiler:
> `nvfortran` in the free HPC SDK, with OpenACC (`-acc -gpu=cc89`), OpenMP target
> (`-mp=gpu`), CUDA Fortran (`-cuda`) and `do concurrent` offload (`-stdpar=gpu`). An
> RTX 4090 is compute capability 8.9, so `cc89`, and a consumer card is fine. But
> upstream WW3 has no GPU code path at all, and the one serious attempt, Ikuyajolu et
> al. (2023), *GMD* 16, `W3SRCEMD` under OpenACC on V100 nodes, reached about **1.3×
> against 42 CPU cores**, limited by host↔device transfer and by register pressure from
> the routine's many locals; it is not merged upstream ⚠ check. Your PCIe 4.0 link is
> slower than the NVLink they had, and a 4090 runs FP64 at 1/64 of FP32. So: measure the
> CPU first (this lesson), and when you do go to the GPU, do it with kernels that own
> their data (lessons [11](11-kokkos-and-modern-cpp.md), [12](12-porting-a-kernel-w3snl1.md),
> [13](13-bulk-porting-with-agents.md)), not with directives sprinkled on Fortran
> (lesson [10](10-modern-fortran-refactoring.md) shows why).

## Sources

- Ikuyajolu et al. (2023), *GMD* 16, 1445–1458: https://doi.org/10.5194/gmd-16-1445-2023
- `pubs/proposal/pt/05-justification.md`, "Estado dos testes do WW3" (survey of 2026-09-15)
- `docs/AGENTS_KOKKOS_202609.md` §2.1, §2.4 (the profiling recipe)

→ [`10-modern-fortran-refactoring.md`](10-modern-fortran-refactoring.md)
