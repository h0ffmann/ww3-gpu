# W3BETA_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3KOKKOS_ST2_SETUP`, behind `WW_KOKKOS_SPR2, SIN2 or SDS2` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

The Chalikov and Belevich (1993) wind-wave interaction parameter, beta * 1e-4, as a piecewise
function of the non-dimensional apparent frequency `OMA` and the drag coefficient `CL` (five
branches, quadratic, linear and quadratic in `OMA`). An internal function of `INPTAB`, called only
to fill `BETATB` (switch `ST2`): a host function.

## What was ported

| | |
|---|---|
| WW3 routine | `W3BETA`, `model/src/w3src2md.F90:1081-1224` at NOAA-EMC/WW3 `03adda91`, contained in `INPTAB` ([`INPTAB_KOKKOS.md`](INPTAB_KOKKOS.md)); called only at :971 (and by the `W3_T1` error estimate, :1005, :1023). `ST2` switches: see `INPTAB_KOKKOS.md` |
| Kokkos code | [`../src/ww_kokkos/srce_st2_inptab.hpp`](../src/ww_kokkos/srce_st2_inptab.hpp), `ww::srce::st2::w3beta(oma, cl)`; the third argument `NDST` is dropped, read only by the `W3_T0` writes (:1160-1162, :1185-1188, :1206-1208) |
| C shim | none; `inptab` calls it |
| Plan | issue #42 |

## Arithmetic

Reference: the same object and C++ unit as `INPTAB_KOKKOS.md` (gfortran and g++ 16.2.0, `-O3`,
`ST2 STAB2 FLX3`), 2026-10-16. gfortran emits no out-of-line `W3BETA`: it exists only inlined in
`__w3src2md_MOD_inptab`, so it was compared there, and also as an out-of-line C++ function.

- (v) Every literal is a default REAL in the Fortran and an `f` literal in the C++, with the same
  bits in both dumps (`1.075`, `-0.06`, `1.E-4`, ...); `(OM2-1)` with the integer 1 is a float
  subtraction of 1.0 in both.
- (v) `A5**2`, `OMA**2`, `(OM2-1)**2`, `(OMA-1.)**2` are one multiply each in the object and in the
  C++ (`x*x`); `0.25*A5**2/A4` is `((A5*A5)*0.25)/A4`, `A3*OMA*(A4*OMA-A5)` is
  `(A3*OMA)*(A4*OMA-A5)`; `OM1/2.` became `OM1*0.5` in both (exact). Operations in the Fortran's
  order, no FMA, no libm call.
- (v) `-A1*OMA**2 - A2` (:1192): the vectorised body computes `(-A2) - A1*OMA**2` and the scalar
  tail `-(A1*OMA**2) - A2`, in both objects; the two forms round the same.
- (v) The five branches with the same strict `<` comparisons (`comiss`) and the same order.
- ⚠ `A3` (:1179) and `A7` (:1181) are divisions computed on every call in the Fortran source;
  both compilers compute each only on the branch that uses it. No trap is enabled, so no bit
  changes.
- ⚠ CUDA compiled (host-only, no device code), never run.
- (v) Fresh review 2026-10-16: no defect. Compared inlined in `inptab` (see `INPTAB_KOKKOS.md`): the four scalar branches and the vectorised `OMA < -1` branch have the same operations, operand order and constant bits in the gfortran and g++ GIMPLE. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
