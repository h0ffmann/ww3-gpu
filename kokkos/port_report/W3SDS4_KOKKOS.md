# W3SDS4_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: waiting for the ST4 checkpoint
(ADR-0011). Selected by `WW_KOKKOS_SDS4=1`; every flag defaults to 0, so WW3 runs the Fortran.

ST4 whitecapping dissipation: saturation-based breaking (`SELECT CASE` 1-3), the cumulative term (both `SSDSC(3)` branches), turbulence, and the whitecap outputs.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SDS4`, `model/src/w3src4md.F90:2043-2659` at `WW3@bb08623a` (the same lines at NOAA-EMC/WW3 `03adda91`) |
| Kokkos code | [`../src/ww_kokkos/st4_sds4.hpp`](../src/ww_kokkos/st4_sds4.hpp), a per-point device function `w3sds4_point`, launched by `ww_sds4` |
| C shim | [`../src/fortran_iface/st4_shim.cpp`](../src/fortran_iface/st4_shim.cpp); parameters and arrays in [`st4_params.hpp`](../src/ww_kokkos/st4_params.hpp), [`st4_arrays.hpp`](../src/ww_kokkos/st4_arrays.hpp) |
| WW3 side | `w3kokkosmd.F90`, `w3initmd.F90`, `w3srcemd.F90` on the h0ffmann/WW3 branch `ww3-gpu/kokkos-st4` (`3f07a0f2`); the `WW3/` pin is not moved yet |
| Phase | 1: copy in, kernel, copy out, one point per call, inside `!$OMP CRITICAL (WW_KOKKOS_ST4)`; Kokkos::Serial inside an OpenMP region |
| Plan | [`ST4_KOKKOS_PLAN_202610.md`](../../docs/ST4_KOKKOS_PLAN_202610.md), issue #45 |

## Arithmetic

The reference is the `-O3` GIMPLE inventory of `w3src4md.F90` (WW3 CMake Release flags, gfortran 16.2, `switch_lab_shrd`; `-fdump-tree-optimized` on that one file, whose object `.text` was identical to the CMake build's (v)), not the source read by eye.

- (v) Three loops are computed by gfortran with glibc libmvec in groups of four from the first element, then scalar: `:2315` and `:2500` (`_ZGVbN4vv_powf`), `:2509` (`_ZGVbN4v_expf`). The port calls the same functions with the same grouping through [`st4_libmvec.hpp`](../src/ww_kokkos/st4_libmvec.hpp) (the owner's choice, 2026-10-15). libmvec and scalar libm differ by up to 3 ULP.
- (v) Folded constants checked by `static_assert` against the dump; integer powers as the dump's multiplication trees; NINT as `lroundf`; FLOOR at `:2590` in the dump's form (`fint`, then correct by one, wrapping); every reduction in source order (the vectorised ones are in-order folds).

## Quirks kept

`DIKCUMUL` (a module variable written at `:2218`) is a local here; the `CASE(2)` expression at `:2473` as written; `CASE(3)` divides by `USTAR**2`; the `DOT_PRODUCT` at `:2283` keeps its full `2*(MTH/2-1)+1` length; the default O(NK² NTH²) cumulative branch `:2554-2563`; `WHITECAP(1)` as a recurrence in IK order; the early return at `:2581` leaves `WHITECAP` as the caller had it and `WHITECAP(3)` is never set. `NSMOOTH` is never read and is not kept.

## Static parity

A fresh reviewer compared the C++ with the dump and the assembly site by site, and compiled the kernel with the `openmp-release` flags: 0 FMA, the same 2 `_ZGVbN4vv_powf` and 1 `_ZGVbN4v_expf` sites, `sincosf` where the Fortran has it. No difference on finite inputs. Not a full statement-by-statement GIMPLE diff ⚠.

Edge cases, not on finite valid inputs:

- ⚠ `FLOOR(AAIRCMIN) <= 0` (an `AIRCMIN` above `GRAV/SIG(1)`) writes `COEF4(0...)` out of bounds in the Fortran too; in the port it lands before the work buffer.
- ⚠ `MAX(1e-20,A)` at `:2518` in a one-element scalar remainder passes a NaN `A` through in the Fortran (`maxss` operand order), not in the port. A NaN spectrum only.
- ⚠ MAX/MIN elsewhere are `a>b?a:b`; they match the assembly at `:2512`, `:2514`, `:2568` and differ elsewhere only for NaN or a signed zero.
- ⚠ On a device the libmvec sites fall back to scalar `powf`/`expf`: a GPU build cannot be bit for bit there.

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
