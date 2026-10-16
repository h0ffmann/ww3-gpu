# CMPLX_ROOT_MULLER_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Muller's method for the complex root of `F_ZHAO_CHENG` (IC3 "Cheng"), the complex wavenumber of
a wave in a viscoelastic ice layer. `W3IC3WNCG_CHENG` calls it with `JUDGE = 1`
(`w3sic3md.F90:1939`). The routine is identical in structure to `CMPLX_ROOT_MULLER_V1` but uses
`F_ZHAO_CHENG` and the Cheng-specific constants `IMAX = 200`, `EPSI = DLTA = 1.E-8`, and applies
the post-iterate constraints `IMAG(P3) >= 0`, `REAL(P3) >= 0` and `NU == 0 => Im(P3) = 0`
(`:2682-2690`).

## What was ported

| | |
|---|---|
| WW3 routine | `CMPLX_ROOT_MULLER_CHENG`, `model/src/w3sic3md.F90:2540-2710` at NOAA-EMC/WW3 `03adda91` (171 lines); compiled under switch `IC3`, by 46 switch files of the repository (`git grep -lw IC3 -- '*switch*'`), by no NOAA operational switch. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_cmplx_root_muller_cheng.hpp`](../src/ww_kokkos/ic3_cmplx_root_muller_cheng.hpp), `ww::ic3::cmplx_root_muller_cheng(...)` and the callable `ww::ic3::CmplxRootMullerCheng`; `DOUBLE COMPLEX` is `Kokkos::complex<double>`, `INTEGER` is `int`, `DOUBLE PRECISION` is `double` |
| Callees | `F_ZHAO_CHENG` through `ww::ic3::f_zhao_cheng` ([`ic3_f_zhao_cheng.hpp`](../src/ww_kokkos/ic3_f_zhao_cheng.hpp), [F_ZHAO_CHENG_KOKKOS.md](F_ZHAO_CHENG_KOKKOS.md)); `DRFUN_DBLE_CHENG` and `DRFUN_QUAD_CHENG` are passed as callables |
| C shim | none; its caller `W3IC3WNCG_CHENG` is not wired |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object, 2026-10-16. Its GIMPLE (`-fdump-tree-optimized-lineno`) was read
against a throwaway C++ unit compiled with the `openmp-release` flags and `-ffp-contract=off`.
That unit compiles warning-free.

- (v) Structure matches `CMPLX_ROOT_MULLER_V1` ([CMPLX_ROOT_MULLER_V1_KOKKOS.md](CMPLX_ROOT_MULLER_V1_KOKKOS.md)):
  the Muller update `Q = (P2-P1)/(P1-P0)`, `A`, `B`, `C`, the discriminant branch by
  `ABS(DEN1) .LT. ABS(DEN2)`, the `A == 0` then `B == 0` early return, and the convergence test
  `ABS(P3-P2).LT.DLTA .AND. ABS(Y3).LT.EPSI` are the same expression trees in the object and in
  the C++.
- (v) `IMAX = 200`, `EPSI = DLTA = 1.E-8`: default `REAL` literals stored in `REAL(8)`, folded
  to the same double bit pattern (`0x1.5798ee2308c0ap-27`) in the C++ `static_assert` and in the
  Fortran object.
- (v) Post-iterate constraints (`:2682-2690`) are reproduced in the same order: first flip the
  sign of a negative imaginary part, then negate a negative real part, then zero the imaginary
  part when `NU == 0.0`. Each uses component-by-component complex arithmetic with the products
  with `0.0` kept, as in the object.
- (v) Non-convergence fallback `P3 = CMPLX(-100., 0)` is reproduced exactly.
- (v) `F_ZHAO_CHENG` is called through the same templated callable pattern as in
  `ic3_f_zhao_cheng.hpp`; the `DRFUN_DBLE_CHENG` and `DRFUN_QUAD_CHENG` callables are inlined
  with the same arithmetic as the Fortran object.
- ⚠ The host path calls glibc `cabs`, `csqrt`, `cexp`, `cpow` (through the Cheng helpers). A
  device has none of them: the device path falls back to `Kokkos::abs`, `Kokkos::sqrt`,
  `Kokkos::exp`, `Kokkos::pow`, not bit for bit with glibc. CUDA not compiled.
- No reduction, no MIN/MAX on arrays, no `INT`/`NINT`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Its checkpoint (ADR-0011) needs a shim and `W3SIC3`'s caller hunk;
the regtests that compile it are the `IC3` cases of `ww3_tic1.1`.
