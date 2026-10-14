# W3SNL1_KOKKOS

**Status:** L2 (inside WW3, on the CPU). Not run on a GPU inside WW3 yet.

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
one CPU thread, from `scripts/hoff.sh` on the same workstation (`just smoke-hoff`,
`just full-hoff`).

<!-- bench:start -->
| step | date | what | backend | value | baseline | unit | source |
|---|---|---|---|---|---|---|---|
| 1 | 2026-09-15 | the shim, 1 000 points | Serial (1 thread) | 25.88 | 25.88 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 2 | 2026-09-15 | the shim, 1 000 points | OpenMP, 32 threads | 5.95 | 25.88 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 3 | 2026-09-15 | the shim, 1 000 points | CUDA, RTX 4090 | 0.75 | 25.88 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 4 | 2026-09-15 | the kernel alone, 1 000 points | CUDA, RTX 4090 | 0.047 | 24.96 | ms | bench/results/snl1-i9-14900-rtx4090-2026-09-15.json |
| 5 | 2026-10-14 | ww3_shel, ww3_ts1 (one point) | OpenMP, 1 thread, in WW3 | 2.81 | 2.67 | s | just smoke-hoff |
| 6 | 2026-10-14 | ww3_shel, bench_small (9 520 points) | OpenMP, 1 thread, in WW3 | 217.9 | 152.7 | s | just full-hoff |
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

## How it was regtested

| Level | What | Result | Where |
|---|---|---|---|
| L1 | one call against the Fortran fixture, NK=25 NTH=24, three presets (serial-debug, openmp-release, cuda-release) | bit-identical (max relative error 0.0) | `L1_test_snl1_dia`, `L1_test_snl1_shim` ([`../tests/`](../tests/)), `shim_roundtrip` ([`../tests/fixtures/`](../tests/fixtures/)), CI job `kokkos` |
| L2, smoke | `--hoff --cpu` on regtest `ww3_ts1` (NK=36 NTH=24, 10 days): the Kokkos DIA against the Fortran DIA of the same executable, then pair to pair against `--ww3` | `hs` max rel 9.879e-8, PASS, both comparisons (tolerance 1e-4); not byte-identical | `just smoke-hoff`, 2026-10-14; CI health check h1 |
| L2, full | `--hoff --cpu` on `bench_small` (9 520 points, 12 h) | `hs` max rel 1.832e-7 over 119 652 values, PASS; not byte-identical | `just full-hoff`, 2026-10-14 |
| GPU in WW3 | `--hoff --gpu` | not run: the workstation's driver was not loaded on 2026-10-14, and the run needs the owner | `just smoke-hoff gpu` |

Why L2 is not byte-identical while L1 is: the L1 fixture comes from gfortran at `-g`, and WW3 runs
at `-O3`; `-ffp-contract=off` on the Fortran changed nothing. The cause is open
([`../../docs/PORT_ASSESSMENT_202610.md`](../../docs/PORT_ASSESSMENT_202610.md) §4).

## GoogleTest, the WW4 way

WW4 tests its C++ with GoogleTest, in files named `L1_test_*` (one unit) and `L2_test_*` (the
program, end to end) under `tests/`, run by CTest.

- **L1 already is GoogleTest.** `L1_test_snl1_dia.cpp` and `L1_test_snl1_shim.cpp` use `gtest`, and
  CTest runs them on every preset, as WW4 does.
- **L2 is not.** The replay inside WW3 is a shell script, `scripts/hoff.sh`, with `nccmp-tol` as
  the judge. It can become a GoogleTest the WW4 way: an `L2_test_snl1_replay.cpp` that runs the two
  `ww3_shel` builds of the smoke case and asserts on `nccmp-tol`'s verdict. That needs the WW3
  build inside the CTest tree, and it is not done.
