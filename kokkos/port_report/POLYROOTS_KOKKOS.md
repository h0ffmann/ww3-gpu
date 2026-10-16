# POLYROOTS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Roots of a real polynomial `P(x) = c_n x^n + ... + c_0` as the eigenvalues of its companion
(upper Hessenberg) matrix, Numerical Recipes' `zrhqr`: `POLYROOTS` builds the matrix, balances it
with `BALANCING_MATRIX` and hands it to `EIG_HQR` (QR iteration on a Hessenberg matrix). In
`W3SIC5` (switch `IC5`) its only caller is `FSDISP` (`w3sic5md.F90:853`, `CALL POLYROOTS(6, ...)`),
which solves the ice-covered dispersion relation for each frequency.

## What was ported

| | |
|---|---|
| WW3 routine | `POLYROOTS`, `model/src/w3sic5md.F90:1435-1586` at NOAA-EMC/WW3 `03adda91` (arithmetic at `:1563-1578`); compiled by the `IC5` switches of `ww3_tic1.1` (`input_IC5_M1`, `_M2`, `_M3`; `git grep -lw IC5 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_*`). Only `#ifdef W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/ic5_polyroots.hpp`](../src/ww_kokkos/ic5_polyroots.hpp), `ww::ic5::polyroots(npc, pcvec, rtrl, rtim, hess, eig_hqr)`, returns 0 or 2 |
| Callees | `ww::ic5::balancing_matrix` ([`BALANCING_MATRIX_KOKKOS.md`](BALANCING_MATRIX_KOKKOS.md)); `EIG_HQR` (`:1111`) is not translated and is a callable argument `eig_hqr(nmat, hmat, eigr, eigi)` |
| Structural changes | the automatic array `HESS(NPC-1, NPC-1)` (`:1553`, a `malloc`/`free` per call in the object) is the caller-provided work array `hess`, column-major with leading dimension `NPC-1`; `WRITE(NDSE, 1001)` on `NAPERR` and `EXTCDE(2)` (`:1563-1566`) are the return code 2, which the shim turns into the WRITE and the stop |
| C shim | none yet |
| Plan | issue #42, batch `goal/42-port-batch-3-shims` |

## Arithmetic

Reference: the `-O3` object of `w3sic5md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `NL4 IC5 BT9 NLS`, `build_mixb`), recompiled and checked identical to the CMake object,
compared with a throwaway C++ unit under the `openmp-release` flags (g++ 16.2, warning-free),
2026-10-16.

- (v) `ABS(PCVEC(1)) < ERRTOL` (`:1563`): `ERRTOL = 1.E-12` is a default `REAL` parameter
  (`:144`); both GIMPLE dumps carry the same constant `9.99999996e-13` (`1.e-12f`), both objects
  `andps` the sign mask then `comiss` with `ERRTOL` as the greater side (a NaN coefficient fails
  the test and goes on).
- (v) `HESS = 0.` is a `memset` in both. `HESS(1, :) = -1 * PCVEC(2:) / PCVEC(1)` (`:1570`) is,
  in both, the quotient `PCVEC(k)/PCVEC(1)` (`divps`/`divss`) then a sign flip (`xorps`); the
  integer `-1` never becomes a multiply. Element-wise, no reduction: the vector widths (gfortran 4
  and 2 wide, g++ 8 and 4 wide) do not change any bit. The subdiagonal `HESS(J+1, J) = 1.`
  (`:1571-1573`) is a plain store.
- (v) In this object `POLYROOTS` calls `__w3sic5md_MOD_balancing_matrix` out of line (`call` at
  `:1576`), then `__w3sic5md_MOD_eig_hqr` (`:1578`); g++ inlines `balancing_matrix`, whose
  arithmetic was compared against the out-of-line routine in `BALANCING_MATRIX_KOKKOS.md`.
- (v) Fresh review 2026-10-16: no defect. The object of `w3sic5md.F90` was recompiled from
  `build_mixb`'s flags and found identical to the CMake object; its GIMPLE and assembly were compared
  with a new C++ unit (the `ERRTOL` constant and `comiss` order, `memset`, divide then negate in the
  vector bodies and the scalar tail, the relocations `balancing_matrix` and `eig_hqr` out of line);
  `NPC = 1` and a NaN `PCVEC(1)` take the same path in both. Nothing changed in the header; the
  inlining claim of `BALANCING_MATRIX_KOKKOS.md` was corrected.
- ⚠ `EIG_HQR` is not translated, so the roots `RTRL`, `RTIM` are as bit-exact as the callable the
  caller passes.
- ⚠ Not compiled for CUDA. No libm call here.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
