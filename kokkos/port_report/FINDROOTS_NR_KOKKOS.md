# FINDROOTS_NR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Newton-Raphson iteration for a complex root k of the Robinson and Palmer (1990) dispersion
relation for waves under a continuous ice sheet, F(k) = C5 k^5 + C1 k - ALP, starting from the
`DOUBLE COMPLEX` guess `GUESS`: `X_{n+1} = X_n - F(X_n)/F'(X_n)`, with F from `FUNCD_FVAL` and
F' from `FUNCD_FDERIV`, until `|DX| <= TOL = 1E-9` and `|F| <= 1`, or at most 299 iterations.
`VISC_RP` is the artificial viscosity that makes the root complex (0 for the wavenumber, 10 for
the damping rate in `W3RPWNICE`).

## What was ported

| | |
|---|---|
| WW3 routine | `FINDROOTS_NR`, `model/src/w3sis2md.F90:1204-1301` at NOAA-EMC/WW3 `03adda91`, `PRIVATE` to `W3SIS2MD` (`:90`). No `#ifdef` in the routine |
| Callees | `FUNCD_FVAL` (`:1279`) and `FUNCD_FDERIV` (`:1280`), from [`is2_funcd_fval.hpp`](../src/ww_kokkos/is2_funcd_fval.hpp) and [`is2_funcd_fderiv.hpp`](../src/ww_kokkos/is2_funcd_fderiv.hpp); `EXTCDE` (`:1292`) on a path that cannot be reached (below) |
| Callers | `W3RPWNICE` only (`:1177`, `:1179`), called by `W3SIS2` (`:869`) only when `CICE > 0` and `IS2PARS(6) > 0.5`, that is `IS2DISP = T` (`w3gridmd.F90:5456-5457`); `IS2DISP` defaults to `.FALSE.` (`w3gridmd.F90:2852`) and no regtest input sets it true (`git grep -n IS2DISP -- regtests`: only `IS2DISP = F`, in `ww3_tic2.3`). No regtest reaches this routine |
| Switches that compile it | `IS2`: `ww3_tic1.1` (`input_IS2/switch`, `input_IC2_ifr/switch_IS2`, `switch_IC2IS2`), `ww3_tic1.4`, `ww3_tic2.3`, `ww3_ts4` (rg_multi, rg_shel, ug, each with `_MPI`), `mww3_test_03`, `mww3_test_08`, `ww3_tp2.14` (`switch_OASICM`), and `model/bin/switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASICM` (`git grep -lw IS2 -- '*switch*'`). No `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/is2_w3rpwnice.hpp`](../src/ww_kokkos/is2_w3rpwnice.hpp): `ww::is2::findroots_nr(guess, visc_rp, x, iceh, sigma)`, `guess` and `x` as `Kokkos::complex<double>`, `x` written by reference; returns 0, or 1 where the Fortran would stop in `EXTCDE(1)` (`:1290-1293`), which cannot happen. Translated in the header of its only caller, [`W3RPWNICE_KOKKOS.md`](W3RPWNICE_KOKKOS.md) |
| C shim | none yet |
| Plan | issue #42, batch `goal/42-port-batch-easy` |

## Arithmetic

Reference: the `-O3` object of `w3sis2md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `IC3 IS2 BT8 TIDE UOST FLD2 IG1`, build `build_mixa`), recompiled and checked
identical to the CMake object, compared with a throwaway C++ unit under the `openmp-release`
flags (compiled warning-free) that calls `findroots_nr` alone and through `w3rpwnice`, GIMPLE
and disassembly, 2026-10-16.

- (v) The object keeps a standalone `__w3sis2md_MOD_findroots_nr` (never called: its only
  caller inlines it twice) with `FUNCD_FVAL` and `FUNCD_FDERIV` inlined. Its loop-invariant
  terms (`FLEX_RIGID`, `DRAFT`, `ALP`, `GAMMA`, `C1`, `C5`, `5*C5`) are hoisted out of the
  Newton loop; g++ hoists the same ones. Hoisting does not change bits.
- (v) The loop runs at most 299 times (an unsigned counter from 299 down to 0 in both objects):
  `J` starts at 1 (`:1277`), loops while `J < MAXIT = 300` (`:1278`), so `J > MAXIT` (`:1290`)
  is never true and both compilers remove the `EXTCDE(1)` branch. `X` is stored once, after the
  loop, in both.
- (v) `DX = FVAL/FDERIV` (`:1281`) is inlined by gfortran as Smith's algorithm
  (`-fcx-fortran-rules`, no `__divdc3`): when `|Re D| < |Im D|`, `r = Re D/Im D`,
  `den = Re D*r + Im D`, `DX = ((Im F + Re F*r)/den, (Im F*r - Re F)/den)`; otherwise
  `r = Im D/Re D`, `den = Re D + Im D*r`, `DX = ((Re F + Im F*r)/den, (Im F - Re F*r)/den)`.
  A NaN in `|D|` takes the second branch in both. The header writes these two branches out
  instead of `Kokkos::complex`'s division, which scales differently. The standalone Fortran
  function computes the two components with packed `vector(2)` operations (SLP); each lane is
  the same IEEE operation as the scalar C++.
- (v) `X = X0 - DX` (`:1282`) is two subtractions, real and imaginary, in both.
- (v) `ABS(DX)` and `ABS(FVAL)` (`:1284`) are calls of libm's `cabs` in the Fortran; glibc
  2.44's `cabs` is disassembled as `jmp __hypot_finite@GLIBC_2.15`, the address exported as
  `hypot@@GLIBC_2.35`, which the C++ calls through `Kokkos::hypot`. `ABS(FVAL)` is evaluated
  only when `ABS(DX) <= TOL` in both objects. `TOL = 1E-9` is `REAL`, widened to `DOUBLE` for the
  comparison: `0x3e112e0be0000000` (9.99999971718e-10) in both; `1` is compared as `1.0d0`.
- (v) `FUNCD_FVAL` and `FUNCD_FDERIV` inline with the operations of their reports
  ([`FUNCD_FVAL_KOKKOS.md`](FUNCD_FVAL_KOKKOS.md), [`FUNCD_FDERIV_KOKKOS.md`](FUNCD_FDERIV_KOKKOS.md)):
  the two `_gfortran_pow_c8_i4` calls per iteration become products g++ shares between the two
  functions (`X**2`, `X**4`), each the same product of the same operands as libgfortran's binary
  powering; the zero products of the `(1,0)*X` and `(C5,0)*X**n` multiplies are kept, so signed
  zeros and NaNs propagate as in the Fortran.
- (v) No FMA (no `vfmadd` in the C++ object), no libmvec call, no reduction.
- ⚠ When both operands of a commutative product or sum are NaN, the two objects do not always
  put them in the same order, so the NaN payload can differ; every non-NaN result is the same.
- ⚠ The Fortran's bits depend on the pinned libgfortran (`pow_c8_i4`) and glibc (`hypot`). On a
  GPU, `Kokkos::hypot` is CUDA's, not glibc's: the bit-for-bit rule would not hold there.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3sis2md.F90` from `build_mixa`'s flags
  (disassembly identical to the CMake object) and compared the GIMPLE of the standalone
  `__w3sis2md_MOD_findroots_nr` and of its two inlined copies in `__w3sis2md_MOD_w3rpwnice` with
  the C++ unit's `findroots_nr`, operation by operation (branch condition and both Smith
  branches, `X0 - DX`, the two `cabs`/`hypot` tests and their order, the 299-trip counter, the
  `TOL` bits), and the `cabs` entry of the pinned glibc in its disassembly.

WW3 note: the routine documents a stop when the root does not converge (`:1290-1298`, "ROOT
NOT CONVERGED"), but `J .GT. MAXIT` is tested after `J = J+1` inside
`DO WHILE (J .LT. MAXIT ...)`, so `J` never exceeds `MAXIT` and the stop is dead code: a root
that has not converged after 299 iterations is returned without a message. The translation
keeps the error path, equally unreachable.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest input sets `IS2DISP = T`, so even a wired port would
not be exercised by WW3's regtests as they stand.
