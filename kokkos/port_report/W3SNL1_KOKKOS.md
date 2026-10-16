# W3SNL1_KOKKOS

**Status:** L2 (inside WW3, on the CPU), byte-identical to the Fortran DIA on all 100 serial `NL1` entries of WW3's regtest matrix (2026-10-15). Not run on a GPU inside WW3 yet.

The discrete interaction approximation (DIA) of the nonlinear four-wave interactions, `W3SNL1`,
ported to Kokkos.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SNL1`, `model/src/w3snl1md.F90:115-473` at NOAA-EMC/WW3 `03adda91` ([`../ports.tsv`](../ports.tsv)) |
| Kokkos kernel | [`../src/ww_kokkos/snl1_dia.cpp`](../src/ww_kokkos/snl1_dia.cpp) |
| C shim | [`../src/fortran_iface/snl1_shim.cpp`](../src/fortran_iface/snl1_shim.cpp) (`ww_snl1`), called from WW3's `w3kokkosmd.F90` on the h0ffmann/WW3 branch `hoff/kokkos-snl1` |
| Tables it needs | `INSNL1`, its own report: [`INSNL1_KOKKOS.md`](INSNL1_KOKKOS.md) |
| Phase | 1: one sea point per call, the spectrum copied in and out, inside `!$OMP CRITICAL` ([`../src/fortran_iface/PATCH.md`](../src/fortran_iface/PATCH.md)) |

## Benchmarks

Steps 1 to 4 time the kernel alone, outside WW3: `ww_bench_snl1`, 1 000 sea points per call,
NK=25, NTH=24, on the owner's workstation (i9-14900, RTX 4090), median of three runs, from
[`../../bench/results/snl1-i9-14900-rtx4090-2026-09-15.json`](../../bench/results/snl1-i9-14900-rtx4090-2026-09-15.json).
Steps 5 and 6 time the whole model, `ww3_shel`, with the Fortran DIA and with the Kokkos DIA on
one CPU thread, from `scripts/ww3-gpu.sh` on the same workstation (`just smoke-ww3-gpu`,
`just full-ww3-gpu`).

<!-- bench:start -->
| step | date | what | backend | value | baseline | unit | source |
|---|---|---|---|---|---|---|---|
| 1 | 2026-09-15 | the shim, 1 000 points | Serial (1 thread) | 25.88 | 25.88 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 2 | 2026-09-15 | the shim, 1 000 points | OpenMP, 32 threads | 5.95 | 25.88 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 3 | 2026-09-15 | the shim, 1 000 points | CUDA, RTX 4090 | 0.75 | 25.88 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 4 | 2026-09-15 | the kernel alone, 1 000 points | CUDA, RTX 4090 | 0.047 | 24.96 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 5 | 2026-10-14 | ww3_shel, ww3_ts1 (one point) | OpenMP, 1 thread, in WW3 | 2.81 | 2.67 | s | just smoke-ww3-gpu |
| 6 | 2026-10-14 | ww3_shel, bench_small (9 520 points) | OpenMP, 1 thread, in WW3 | 217.9 | 152.7 | s | just full-ww3-gpu |
<!-- bench:end -->

What they say, read with care:

- Steps 1 to 3 are the cost of the parity build (`-ffp-contract=off`, `--fmad=false`): the
  arithmetic of the Fortran, not the fastest the kernel can go
  ([`../PORT_STATUS.md`](../PORT_STATUS.md), "Two things these numbers are not").
- Step 3 against step 4 is the phase-1 copy: on CUDA, 94 % of the shim's time is moving the
  spectrum, not computing it.
- Steps 5 and 6 are **slower** than the Fortran. Inside WW3, phase 1 calls the shim once per point,
  inside a critical section, and copies at every call; on one CPU thread that costs more than the
  kernel saves. No speed claim is made for WW3 with the Kokkos DIA until phase 2.
- No step is a GPU run inside WW3: that run is the owner's, supervised (AGENTS.md hard rule).

A GPU DIA kernel proposed upstream, `consigcody94/ww3-dia-gpu@38919d2`,
models part of section 3 on synthetic tables; what it computes and measures against this port is
in [`../../docs/W3SNL1_GPU_COMPARISON_202610.md`](../../docs/W3SNL1_GPU_COMPARISON_202610.md).

## How it was regtested

| Level | What | Result | Where |
|---|---|---|---|
| L1 (history) | one call against the Fortran fixture, NK=25 NTH=24, three presets (serial-debug, openmp-release, cuda-release) | bit-identical (max relative error 0.0), 2026-09-15; the CPU presets re-run 2026-10-14 | `L1_test_snl1_dia`, `L1_test_snl1_shim`, `shim_roundtrip`: removed with the tests, 2026-10-14 |
| L2, smoke | `--ww3-gpu --cpu` on regtest `ww3_ts1` (NK=36 NTH=24, 10 days): the Kokkos DIA against the Fortran DIA of the same executable, then pair to pair against `--ww3` | `hs` max rel 9.879e-8, PASS, both comparisons (tolerance 1e-4); not byte-identical, before the `AF11` fix | `just smoke-ww3-gpu`, 2026-10-14; CI health check h1 |
| L2, full | `--ww3-gpu --cpu` on `bench_small` (9 520 points, 12 h) | `hs` max rel 1.832e-7 over 119 652 values, PASS; not byte-identical, before the `AF11` fix | `just full-ww3-gpu`, 2026-10-14 |
| L2, matrix (before the fix) | `just ww3-gpu-matrix`: the 100 serial `NL1` entries of WW3's `matrix.base` through `run_cmake_test`, the Kokkos DIA against the Fortran DIA of one executable (WFIP-0003) | all 100 built and ran with the Kokkos DIA logged; `nccmp-tol` 66 PASS, 29 n/a (no judged field), 5 FAIL (entries 6, 10, 16, 51, 54); `test.comp` byte-identical on 9 entries (`ww3_tp2.6`, `ww3_ts1`, `ww3_tc1`, `ww3_tnc1`) (v) | [`docs/data/hoff_matrix_2026-10-14.tsv`](../../docs/data/hoff_matrix_2026-10-14.tsv), 2026-10-15; the failures: [entry 16](../../docs/log/2026-10-14-hoff-matrix-entry16.md), [51 and 54](../../docs/log/2026-10-15-hoff-matrix-entries51-54.md): round-off amplified by WW3 itself, reproduced by FMA on the Fortran alone |
| L2, matrix after the `AF11` fix | `just ww3-gpu-matrix --jobs 8` with `pow11()` following gfortran's `-O3` chain (below) | all 100 built and ran with the Kokkos DIA logged; `test.comp` **0 differing files on all 100 entries**; `nccmp-tol` 71 PASS, 29 n/a, 0 FAIL (v) | [`docs/data/hoff_matrix_2026-10-15.tsv`](../../docs/data/hoff_matrix_2026-10-15.tsv), 2026-10-15; [`docs/log/2026-10-14-snl1-bitwise-af11.md`](../../docs/log/2026-10-14-snl1-bitwise-af11.md) |
| L2, OpenMP matrix | the 25 shared-memory `NL1` entries with `OMPG` (all `ww3_ts3`, `ST1` to `ST6`), each with `-f -O -n 8`, so WW3 calls the shim from inside `W3WAVE`'s `!$OMP PARALLEL` region (`w3wavemd.F90:2240-2243`; the call at `w3srcemd.F90:1276`, under `!$OMP CRITICAL`); `OMP_MAX_ACTIVE_LEVELS=2` exported for both runs | all 25 built and ran with the Kokkos DIA logged; `test.comp` **0 of 20 files differing on all 25**; `nccmp-tol` 25 PASS on `hs` (v). Without the variable every Kokkos run aborted at the first DIA call, "Nested parallelism requires the maximum active levels to be larger than 1" (v) | [`docs/data/hoff_matrix_omp_2026-10-15.tsv`](../../docs/data/hoff_matrix_omp_2026-10-15.tsv), 2026-10-15, `OMP_MAX_ACTIVE_LEVELS=2 just ww3-gpu-matrix --omp --jobs 4` with the script of `f32640c`; [`docs/log/2026-10-15-hoff-matrix-omp.md`](../../docs/log/2026-10-15-hoff-matrix-omp.md) |
| L2, smoke after the fix | `just smoke-ww3-gpu` (`ww3_ts1`) | byte-identical outputs; `hs` max rel 0 against the Fortran DIA and pair to pair against `--ww3` (v) | 2026-10-15 |
| L2, full after the fix | `just full-ww3-gpu` (`bench_small`) | byte-identical outputs; `hs` max rel 0 over 119 652 values, both comparisons (v) | 2026-10-15 |
| GPU in WW3 | `--ww3-gpu --gpu` | not run: the workstation's driver was not loaded on 2026-10-14, and the run needs the owner | `just smoke-ww3-gpu gpu` |

The port has no tests of its own since 2026-10-14 (the owner: "kokkos code should NOT have its
own tests, hard rule. All tests should come [from] WW3 regtests."). Its validation is the L2 rows:
`scripts/ww3-gpu.sh --ww3-gpu --cpu` pair to pair with `--ww3`, and the WW3 regtest matrix of WFIP-0003.
The matrix rows above were run by `scripts/hoff_matrix.sh`; since 2026-10-15 the same matrix is a
CMake/CTest driver in [`regtests/`](../../regtests/CMakeLists.txt) (`just ww3-gpu-matrix`), one CTest test per
`matrix.base` entry. CTest there runs WW3's own regtests, the Fortran DIA against the Kokkos DIA;
it is not a test of the port's own.

**Open issue: the shim needs nesting set by its caller.** Under `OMPG` the shim is called inside an
active OpenMP parallel region, and Kokkos's OpenMP backend refuses to open its own unless the
runtime allows two active levels (the OpenMP matrix row). Today the caller provides that,
`OMP_MAX_ACTIVE_LEVELS=2` in the environment, which the `omp` selection of `regtests/` exports for
both runs; a WW3 user who sets `OMPG` and `WW_KOKKOS_SNL1=1`
without it gets an abort. Two fixes, both a change to the shim and both awaiting the owner: the
shim calls `omp_set_max_active_levels(2)` when it initialises, or it runs the kernel on
`Kokkos::Serial` when `omp_in_parallel()` is true. Neither is chosen or written ⚠.

Why L2 was not byte-identical while L1 was, until 2026-10-15: `AF11 = (SIG(IFR)*TPIINV)**11`
(`w3snl1md.F90:741`, `:750` at `6d1bcdfe`). gfortran computes `x**11` with libgcc's `__powisf2`
at `-O0`, whose chain ends `x^3*x^8`, and with GCC's power tree at `-O3`, ending `x^5*x^6`
(`x^2 = x*x`, `x^3 = x^2*x`, `x^5 = x^2*x^3`, `x^6 = x^3*x^3`) (v) (`-fdump-tree-optimized` of
`w3snl1md.F90` at WW3's flags). The two differ in the last bit for 643 427 of 1 000 000 inputs
(v). `pow11()` had the `-O0` chain, which the `-g` fixture confirmed and the `-O3` WW3 did not;
it now has the `-O3` chain. No other line of `INSNL1` or `W3SNL1` differs in the dump: runtime
exponents (`XFR**IFRP`) stay `__powisf2` calls at `-O3`, which `powi()` reproduces. This restores
the Fortran's arithmetic; nothing was reordered relative to the Fortran, and no tolerance, flag or
WW3 file changed. The 1e-7 differences of the earlier rows, and the five `nccmp-tol` failures of
the 2026-10-14 matrix, came from this one table. ⚠ The chain is GCC's: a WW3 built with another
compiler or at `-O0` would need the matching chain.

## GoogleTest, the WW4 way

WW4 tests its C++ with GoogleTest, in files named `L1_test_*` (one unit) and `L2_test_*` (the
program, end to end) under `tests/`, run by CTest.

- **L1 is gone.** `L1_test_snl1_dia.cpp` and `L1_test_snl1_shim.cpp` were GoogleTests that CTest
  ran on every preset, as WW4 does; they were removed on 2026-10-14 under the owner's rule above.
- **L2 is not GoogleTest.** The replay inside WW3 is a shell script, `scripts/ww3-gpu.sh`, with `nccmp-tol` as
  the judge, and the WW3 regtest matrix (`regtests/`, a CTest driver since 2026-10-15, WFIP-0003) runs WW3's own harness.
  Wrapping them in an `L2_test_snl1_replay.cpp` would be a test of the port's own, which
  [ADR-0010](../../docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md) rules out.
