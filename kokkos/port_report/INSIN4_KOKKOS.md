# INSIN4_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: waiting for the ST4 checkpoint
(ADR-0011). Selected by `WW_KOKKOS_ST4_TABLES=1`; every flag defaults to 0, so WW3 runs the Fortran.

The ST4 tables of a grid: `SATINDICES`, `SATWEIGHTS`, `IKTAB`, `DCKI`, `QBI`, `CUMULW`, and with `FLTABS` the `TABU_*` stress tables. `WAVNU2` (`w3dispmd.F90:204-346`) is translated beside it. By default the shim uses the tables the Fortran built and registered; this translation runs only with the flag, and then prints how many values differ from the Fortran's.

## What was ported

| | |
|---|---|
| WW3 routine | `INSIN4`, `model/src/w3src4md.F90:1014-1288` at `WW3@bb08623a` (the same lines at NOAA-EMC/WW3 `03adda91`) |
| Kokkos code | [`../src/ww_kokkos/st4_insin4.cpp`](../src/ww_kokkos/st4_insin4.cpp), a host function `insin4`, called by `ww_st4_commit` |
| C shim | [`../src/fortran_iface/st4_shim.cpp`](../src/fortran_iface/st4_shim.cpp); parameters and arrays in [`st4_params.hpp`](../src/ww_kokkos/st4_params.hpp), [`st4_arrays.hpp`](../src/ww_kokkos/st4_arrays.hpp) |
| WW3 side | `w3kokkosmd.F90`, `w3initmd.F90`, `w3srcemd.F90` on the h0ffmann/WW3 branch `ww3-gpu/kokkos-st4` (`3f07a0f2`); the `WW3/` pin is not moved yet |
| Phase | 1: copy in, kernel, copy out, one point per call, inside `!$OMP CRITICAL (WW_KOKKOS_ST4)`; Kokkos::Serial inside an OpenMP region |
| Plan | [`ST4_KOKKOS_PLAN_202610.md`](../../docs/ST4_KOKKOS_PLAN_202610.md), issue #45 |

## Arithmetic

The reference is the `-O3` GIMPLE inventory of `w3src4md.F90` (WW3 CMake Release flags, gfortran 16.2, `switch_lab_shrd`; `-fdump-tree-optimized` on that one file, whose object `.text` was identical to the CMake build's (v)), not the source read by eye.

- (v) For `IKD = 1` gfortran folds `TANH(1.01**-999)` and `SQRT(GRAV/XT)` at compile time to 4.818942034e-5 and 451.0971374, while the runtime `__powisf2` gives 4.8189144e-5: the port hard-codes the dump's two constants. ⚠ They belong to gfortran 16.2's folding and must be re-read when the toolchain moves.
- (v) `**3.` at `:1227` stays `powf(x, 3.0f)`; `powi` is bit-identical to `__powisf2` for exponents -999..300 (reviewer's test program).

## Quirks kept

`ECOS(1+|ITH2-ITH|)` as the cosine; the re-`ALLOCATE` of `DCKI`/`QBI` at `:1209` leaks memory in the Fortran and changes no value: the port writes into the existing arrays. The cache file is not involved here.

## Static parity

A fresh reviewer compared each file's GIMPLE (system g++ 15.2 ⚠, not the pinned 16.2) with the Fortran dump: every operation and libm call matches up to exact operand swaps.

Edge cases, not on finite valid inputs:

- ⚠ With `WW_KOKKOS_ST4_TABLES=1` the C++ builds the tables `ww3_grid` wrote into `mod_def.ww3`; `ww3_grid` may have read `TAUHFT`/`TAUHFT2` from a stale `ST4TABUHF2.bin`, which the port never reads. The init line counts such differences.

## Benchmarks

None. No timing without parity: nothing is timed before the ST4 regtest replay passes on the
same commit (`AGENTS.md`, "No timing without parity").

## How it was regtested

Not yet. ST4 is one unit under [ADR-0011](../../docs/ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md):
its checkpoint runs `just smoke-ww3-gpu`, `just full-ww3-gpu` and `just ww3-gpu-matrix` with
`WW3GPU_PHYSICS=ST4`, one flag at a time, after the CMake changes of
[`ST4_KOKKOS_PLAN_202610.md`](../../docs/ST4_KOKKOS_PLAN_202610.md) §10.3 and the `WW3/` pin bump
to `ww3-gpu/kokkos-st4`. Until then every parity statement above is ⚠. What did run, 2026-10-15
(v): `just kokkos-test serial-debug` and `openmp-release` (10/10 each; those tests are the intro
programs' and tools', the port has none, ADR-0010), the `cuda-release` preset compiled (never run),
and the fork branch `ww3-gpu/kokkos-st4` (`3f07a0f2`) building and linking `ww3_shel` and
`ww3_grid` with `KOKKOS`, not run.
