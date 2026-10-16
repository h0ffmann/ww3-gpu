# FUNCD_FDERIV_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The derivative in the wavenumber k of the dispersion relation for waves under a flexural ice
plate with artificial viscosity (Robinson and Palmer, 1990), `5*C5*k**4 + C1` with
`C5 = D/(rho g)`, `D = E h**3/(12(1-nu**2))` and `C1 = (1 - (sigma**2/g)*draft, -sigma*visc/(rho g))`,
in `COMPLEX(8)`. `FINDROOTS_NR` divides `FUNCD_FVAL` by it in its Newton iteration
(`w3sis2md.F90:1280`), which `W3RPWNICE` calls; switch `IS2`.

## What was ported

| | |
|---|---|
| WW3 routine | `FUNCD_FDERIV`, `model/src/w3sis2md.F90:1523-1611` at NOAA-EMC/WW3 `03adda91`, `PRIVATE` (`:90`); called only by `FINDROOTS_NR` (`:1280`). Compiled by the `IS2` switches (`ww3_tic1.1`, `ww3_tic1.4`, `ww3_tic2.3`, `ww3_ts4`, `mww3_test_03`, `mww3_test_08`, `ww3_tp2.14/input_oasicm`; `model/bin/switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASICM`), by no `switch_NCEP_*` or `switch_NWS_*`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/is2_funcd_fderiv.hpp`](../src/ww_kokkos/is2_funcd_fderiv.hpp), `ww::is2::funcd_fderiv`, and `fderiv_detail::pow_c8_i4_nonneg` for the runtime power |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sis2md.F90` (WW3's CMake Release flags, gfortran 16.2.0,
`switch_lab_shrd` + `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled with its dumps and found
disassembly-identical to the CMake object, against the GIMPLE and object of a C++ unit built with
the `openmp-release` flags (g++ 16.2.0, warning-free), 2026-10-16.

- (v) `WN_GUESS**4` is not expanded inline: the object calls `_gfortran_pow_c8_i4(x, 4)`. Its
  arithmetic was read from the disassembly of the pinned `libgfortran.so.5.0.0`: binary powering
  from `POW = (1,0)`, `X*X` as `(xr*xr - xi*xi, 2*(xr*xi))` and `POW*X` as a full product, no
  `__muldc3` NaN recovery. `pow_c8_i4_nonneg` does the same products; for `n = 4` g++ folds the
  final `(1,0)*X` to `(xr - xi*0, xi + xr*0)` and computes `xr*xi + xi*xr` as `(xr*xi)*2`, both
  exact, so the same bits.
- (v) Folded constants as in the Fortran GIMPLE: `YOUNG = 0x4fa39d60`, `12*(1-POISSON**2) =
  0x412eb852`, `DENS_ICE/DENS = 0x3f666666`, `DENS*GRAV = 0x461d0c99`, `GRAV = 0x411ce560`, each
  with a `static_assert` of how it is built. `ICEH**3` is `(ICEH*ICEH)*ICEH` in both.
- (v) `CMPLX(1-ALP*DRAFT, -1.*GAMMA)` has no `KIND`, so it is `COMPLEX(4)` widened to
  `COMPLEX(8)`: both parts are rounded to float first. `-1.*GAMMA` is folded to a negation.
- (v) `5*C5*WN_GUESS**4` multiplies `(5*C5, 0)` by the power as a full complex product,
  `(k*wr - wi*0, k*wi + wr*0)`; the C++ writes the zero products so the signed zeros and NaN
  propagation match. g++ vectorises these into `VEC_ADDSUB` pairs; each lane is the same operation.
- (v) No FMA in either object, no libm or libmvec call other than the libgfortran power.
- ⚠ NaN payloads when two operands of one commutative operation are both NaN may differ, since
  g++ and gfortran choose the operand order of a commutative `mulsd`/`addsd` independently.
- ⚠ `pow_c8_i4_nonneg` covers `n >= 0` only; libgfortran's `n < 0` branch (Smith division) is
  not translated, as no caller needs it. CUDA not compiled.
- (v) Fresh review 2026-10-16: no arithmetic defect. The `w3sis2md.F90` object, recompiled from
  `build_mixa`'s flags, was disassembly-identical to the CMake object; its GIMPLE, at the
  out-of-line function and at the three copies inlined into `FINDROOTS_NR`, was compared operation
  by operation with the C++ unit's GIMPLE and assembly (no FMA); `_gfortran_pow_c8_i4` was read
  again from the pinned `libgfortran.so.5.0.0`: squaring `(xr*xr - xi*xi, t + t)` with
  `t = xr*xi`, `POW*X` as `(pr*xr - pi*xi, pr*xi + pi*xr)`, which the C++ reproduces for `n = 4`.
  Fixed one comment: `YOUNG` is at `:1590`, not `:1592`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
