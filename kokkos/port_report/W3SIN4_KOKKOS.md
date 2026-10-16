# W3SIN4_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: waiting for the ST4 checkpoint
(ADR-0011). Selected by `WW_KOKKOS_SIN4=1`; every flag defaults to 0, so WW3 runs the Fortran.

ST4 wind input: swell dissipation, the sequential stress scan over IK, the `W3_STAB3` double pass, the high-frequency tail and the wave-supported stress.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIN4`, `model/src/w3src4md.F90:423-1003` at `WW3@bb08623a` (the same lines at NOAA-EMC/WW3 `03adda91`) |
| Kokkos code | [`../src/ww_kokkos/st4_sin4.hpp`](../src/ww_kokkos/st4_sin4.hpp), a per-point device function `w3sin4_point`, launched by `ww_sin4` |
| C shim | [`../src/fortran_iface/st4_shim.cpp`](../src/fortran_iface/st4_shim.cpp); parameters and arrays in [`st4_params.hpp`](../src/ww_kokkos/st4_params.hpp), [`st4_arrays.hpp`](../src/ww_kokkos/st4_arrays.hpp) |
| WW3 side | `w3kokkosmd.F90`, `w3initmd.F90`, `w3srcemd.F90` on the h0ffmann/WW3 branch `ww3-gpu/kokkos-st4` (`3f07a0f2`); the `WW3/` pin is not moved yet |
| Phase | 1: copy in, kernel, copy out, one point per call, inside `!$OMP CRITICAL (WW_KOKKOS_ST4)`; Kokkos::Serial inside an OpenMP region |
| Plan | [`ST4_KOKKOS_PLAN_202610.md`](../../docs/ST4_KOKKOS_PLAN_202610.md), issue #45 |

## Arithmetic

The reference is the `-O3` GIMPLE inventory of `w3src4md.F90` (WW3 CMake Release flags, gfortran 16.2, `switch_lab_shrd`; `-fdump-tree-optimized` on that one file, whose object `.text` was identical to the CMake build's (v)), not the source read by eye.

- (v) No libmvec in this routine. `COSWIND**SSINTHP` at `:756`, `AORB**(1-0.5*SSWELLF(6))` and both `**0.25` are scalar `powf`; integer powers (`**3`, `**4`, `**5`) as the dump's trees; `LOG(TPI/0.005)` the dump's literal.
- (v) `MAX(ABS(SSWELLF(3)),0.)` at `:663` is dropped by `-O3`; the port drops it too. INT of a REAL goes through `fint` (cvttss2si semantics).

## Quirks kept

The tail at `:852` uses `COSU`/`SINU` of the last `IK=NK` pass, not `USDIRP`; the `LLWS` override at `:769` uses `USTAR`; the IK stress loop `:713-797` is a sequential scan; the analytic tail `:885-930` a recurrence over `J=1..50` (`W(JTOT)` is a local `Real w[50]`). Dead stores (`OMEGACC`, `YC`, `X0`, `OMEGA`) are absent from the dump and from the port.

## Static parity

A fresh reviewer compiled the C++ at `-O3 -ffp-contract=off` and compared its GIMPLE with the Fortran's: the same libm calls (`powf` 6, `logf` 6, `log10f` 1, `tanhf` 2, `atan2f` 3), every reduction in order, the operation order at every line it listed. No difference on the host ⚠ until the checkpoint.

Edge cases, not on finite valid inputs:

- ⚠ The `W3_STAB3` path is translated from the source only: the lab compiles STAB0, so no dump covers it.
- ⚠ MAX/MIN as `a>b?a:b`: they differ from the dump's MIN_EXPR/MAX_EXPR only for NaN or a signed zero.

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
