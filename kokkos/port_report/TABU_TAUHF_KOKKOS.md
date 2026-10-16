# TABU_TAUHF_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: waiting for the ST4 checkpoint
(ADR-0011). Selected by `WW_KOKKOS_ST4_TABLES=1`; every flag defaults to 0, so WW3 runs the Fortran.

The high-frequency stress table `TAUHFT(0:IUSTAR,0:IALPHA)` and its steps `DELUST`, `DELALP`.

## What was ported

| | |
|---|---|
| WW3 routine | `TABU_TAUHF`, `model/src/w3src4md.F90:1427-1596` at `WW3@bb08623a` (the same lines at NOAA-EMC/WW3 `03adda91`) |
| Kokkos code | [`../src/ww_kokkos/st4_tabu_tauhf.cpp`](../src/ww_kokkos/st4_tabu_tauhf.cpp), a host function `tabu_tauhf`, called by `insin4` |
| C shim | [`../src/fortran_iface/st4_shim.cpp`](../src/fortran_iface/st4_shim.cpp); parameters and arrays in [`st4_params.hpp`](../src/ww_kokkos/st4_params.hpp), [`st4_arrays.hpp`](../src/ww_kokkos/st4_arrays.hpp) |
| WW3 side | `w3kokkosmd.F90`, `w3initmd.F90`, `w3srcemd.F90` on the h0ffmann/WW3 branch `ww3-gpu/kokkos-st4` (`3f07a0f2`); the `WW3/` pin is not moved yet |
| Phase | 1: copy in, kernel, copy out, one point per call, inside `!$OMP CRITICAL (WW_KOKKOS_ST4)`; Kokkos::Serial inside an OpenMP region |
| Plan | [`ST4_KOKKOS_PLAN_202610.md`](../../docs/ST4_KOKKOS_PLAN_202610.md), issue #45 |

## Arithmetic

The reference is the `-O3` GIMPLE inventory of `w3src4md.F90` (WW3 CMake Release flags, gfortran 16.2, `switch_lab_shrd`; `-fdump-tree-optimized` on that one file, whose object `.text` was identical to the CMake build's (v)), not the source read by eye.

- (v) `KAPPA**2` folded to 0.16000001; `ZLOG**4` as `(l*l)*(l*l)`; divisions by `GRAV` stay divisions; the integral over J in order.

## Quirks kept

`W(JTOT)` a local array.

## Static parity

Reviewed with `INSIN4` (GIMPLE comparison): equal.

Edge cases, not on finite valid inputs:

- ⚠ The sign of zero at MIN/MAX was not read in the assembly.

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
