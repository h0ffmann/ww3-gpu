# FUNCD_FVAL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Residual of the Robinson and Palmer (1990) dispersion relation for waves under a continuous ice
sheet with artificial viscosity, F(k) = C5 k^5 + C1 k - ALP, with C5 = D/(rho g),
D = E h^3 / (12 (1 - nu^2)), C1 = 1 - ALP*DRAFT - i*GAMMA, ALP = sigma^2/g, for a
`DOUBLE COMPLEX` wavenumber guess k. It is the function whose root the Newton-Raphson loop
`FINDROOTS_NR` (`w3sis2md.F90:1204-1301`) seeks, together with `FUNCD_FDERIV`.

## What was ported

| | |
|---|---|
| WW3 routine | `FUNCD_FVAL`, `model/src/w3sis2md.F90:1420-1506` at NOAA-EMC/WW3 `03adda91`, `PRIVATE` to `W3SIS2MD`. No `#ifdef` in the routine |
| Callers | `FINDROOTS_NR` (`:1279`), called twice per frequency by `W3RPWNICE` (`:1177`, `:1179`), called by `W3SIS2` (`:869`) only when `CICE > 0` and `IS2PARS(6) > 0.5` |
| Switches that compile it | `IS2`: `ww3_tic1.1` (`input_IS2/switch`, `input_IC2_ifr/switch_IS2`, `switch_IC2IS2`), `ww3_tic1.4`, `ww3_tic2.3`, `ww3_ts4` (rg_multi, rg_shel, ug, each with `_MPI`), `mww3_test_03`, `mww3_test_08`, `ww3_tp2.14` (`switch_OASICM`), and `model/bin/switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASICM` (`git grep -lw IS2 -- '*switch*'`). No `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/is2_funcd_fval.hpp`](../src/ww_kokkos/is2_funcd_fval.hpp), `ww::is2::funcd_fval(wn_guess, visc_rp, iceh, sigma)` returning `Kokkos::complex<double>`, and `ww::is2::gfortran_pow_c8_i4(xr, xi, n, pr, pi)` for `WN_GUESS**5` |
| C shim | none yet |
| Plan | issue #42, batch `goal/42-port-batch-easy` |

## Arithmetic

Reference: the `-O3` object of `w3sis2md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `IC3 IS2 BT8 TIDE UOST FLD2 IG1`, build `build_mixa`), recompiled and checked
identical to the CMake object, compared with a throwaway C++ unit under the `openmp-release`
flags (compiled warning-free), GIMPLE and disassembly, 2026-10-16.

- (v) The object keeps `FUNCD_FVAL` as a standalone function `__w3sis2md_MOD_funcd_fval`.
- (v) `WN_GUESS**5` is not expanded by gfortran: the object calls libgfortran's
  `_gfortran_pow_c8_i4(WN_GUESS, 5)`. Its code in the pinned `libgfortran.so.5.0.0`
  (gfortran 16.2.0) was disassembled: binary powering from `POW = (1,0)`, `POW = POW*X` as
  `(pr*xr - pi*xi, pr*xi + pi*xr)`, `X = X*X` as `(xr*xr - xi*xi, t + t)` with `t = xr*xi`, no
  `__muldc3` call (no C99 NaN recovery). For n = 5 this is `P1 = (1,0)*X`, `X2 = X*X`,
  `X4 = X2*X2`, `P1*X4`. The header translates that loop for n >= 0; n < 0 (Smith's division)
  is not translated, no caller passes one.
- (v) g++ folds `1.0*xr` to `xr` in the first `POW*X`; libgfortran multiplies by 1.0 at run
  time. The two are the same bits for every input (a signalling NaN is quieted by the following
  subtraction or addition in both). `t + t` becomes `t*2.0` in g++'s GIMPLE and `vaddsd` in its
  object: identical bits.
- (v) Folded constants as the GIMPLE shows them: `YOUNG = 5.49E+9`, `12*(1-POISSON**2)` =
  10.92000007629..., `DENS_ICE/DENS` = 0.89999997615..., `DENS*GRAV` = 10051.1494140625, `GRAV`
  = 9.806 in `REAL`; written as hex literals with a `static_assert` of how each is built.
- (v) `ICEH**3` as `(ICEH*ICEH)*ICEH`; `FLEX_RIGID = (ICEH**3*YOUNG)/10.92...`;
  `ALP = SIGMA*SIGMA/GRAV`; `GAMMA = (VISC_RP*SIGMA)/(DENS*GRAV)`; `C5 = FLEX_RIGID/(DENS*GRAV)`,
  all in `float`. `C1 = CMPLX(1-ALP*DRAFT, -1.*GAMMA)` is a default (single) `COMPLEX`, so both
  parts are computed in `float` and widened to `double` afterwards; `-1.*GAMMA` folds to a sign
  flip in both objects.
- (v) `C5*WN_GUESS**5`: `C5` is widened to `(C5, 0d0)` and multiplied in full, including the
  `x*0.0` products (neither compiler drops them); then `C1*WN_GUESS` as `(ac-bd, ad+bc)`; real
  part `(t1r + t2r) - DBLE(ALP)`, imaginary part `t1i + t2i`. Same operations, same count, no
  FMA, in both objects.
- ⚠ Commutative products whose operands both can be NaN do not always have the same first source
  in the two objects (e.g. `C1r*ar`: gfortran `c1r` first, g++ `ar` first; `X2r*X2i` in the
  second squaring). x86 returns the first source's NaN when both are NaN, so the NaN payload of
  the result can differ when the inputs carry two different NaNs. Every non-NaN result is
  bit-identical.
- ⚠ The Fortran result depends on libgfortran's `pow_c8_i4`; a libgfortran whose build turns on
  the C99 complex multiply (`__muldc3`) would change the Fortran's bits for infinite inputs. The
  translation matches the pinned toolchain only.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3sis2md.F90` from `build_mixa`'s flags (identical to the CMake object), read `__w3sis2md_MOD_funcd_fval` and its inlined copy in `FINDROOTS_NR` against a C++ unit (openmp-release flags, warning-free, no FMA) that includes this header and `is2_funcd_fderiv.hpp` together: no name clash (the latter's helper is `fderiv_detail::pow_c8_i4_nonneg`). Disassembled `_gfortran_pow_c8_i4` in the pinned `libgfortran.so.5.0.0` and matched g++'s unrolled n = 5 sequence to it operation by operation; folded constants match the `.rodata` bits. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
