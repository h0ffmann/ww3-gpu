# W3RPWNICE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Wavenumber `WN_I`, damping rate `DAMPING` and group speed `CG_I` of waves under a continuous ice
sheet, per frequency, from the Robinson and Palmer (1990) dispersion relation with flexural
rigidity D = E h^3 / (12 (1 - nu^2)), E = 5.49e9 Pa, nu = 0.3, rho = 1025 kg m^-3,
rho_ice = 922.5 kg m^-3. Each frequency solves F(k) = 0 twice by the Newton iteration
`FINDROOTS_NR`: once without viscosity (`VISC_RP = 0`, real part kept as `WN_I`), once with
`VISC_RP = 10` (imaginary part kept as `DAMPING`). Then
`CG_I = (5 D k^4 + rho g - rho_ice h sigma^2) / (2 sigma (rho + rho_ice k h))`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3RPWNICE`, `model/src/w3sis2md.F90:1098-1187` at NOAA-EMC/WW3 `03adda91`, and the `PRIVATE` `FINDROOTS_NR` it calls, `:1204-1301`, translated in the same header because no other header holds it ([`FINDROOTS_NR_KOKKOS.md`](FINDROOTS_NR_KOKKOS.md)). Neither has an `#ifdef` |
| Callees | `FINDROOTS_NR` calls `FUNCD_FVAL` and `FUNCD_FDERIV`, which this header includes from [`is2_funcd_fval.hpp`](../src/ww_kokkos/is2_funcd_fval.hpp) and [`is2_funcd_fderiv.hpp`](../src/ww_kokkos/is2_funcd_fderiv.hpp) |
| Caller | `W3SIS2` (`:869`), only when `CICE > 0` and `IS2PARS(6) > 0.5`, that is `IS2DISP = T` (`w3gridmd.F90:5456-5457`); `IS2DISP` defaults to `.FALSE.` (`w3gridmd.F90:2852`) and no regtest input sets it true (`git grep -n IS2DISP -- regtests`: only `IS2DISP = F`, in `ww3_tic2.3`). No regtest reaches this routine |
| Switches that compile it | `IS2`: `ww3_tic1.1` (`input_IS2/switch`, `input_IC2_ifr/switch_IS2`, `switch_IC2IS2`), `ww3_tic1.4`, `ww3_tic2.3`, `ww3_ts4` (rg_multi, rg_shel, ug, each with `_MPI`), `mww3_test_03`, `mww3_test_08`, `ww3_tp2.14` (`switch_OASICM`), and `model/bin/switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASICM` (`git grep -lw IS2 -- '*switch*'`). No `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/is2_w3rpwnice.hpp`](../src/ww_kokkos/is2_w3rpwnice.hpp): `ww::is2::w3rpwnice(iceh, wn_i, damping, cg_i, nk, sig)` and `ww::is2::findroots_nr(guess, visc_rp, x, iceh, sigma)`; arrays 0-based over `IK = 1..NK`, `sig` is `SIG(1:NK)` of `SIG(0:NK+1)`. Both return 0, or 1 where `FINDROOTS_NR` would stop in `EXTCDE(1)` (`:1290-1293`), which cannot happen (below) |
| C shim | none yet |
| Plan | issue #42, batch `goal/42-port-batch-easy` |

## Arithmetic

Reference: the `-O3` object of `w3sis2md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `IC3 IS2 BT8 TIDE UOST FLD2 IG1`, build `build_mixa`), recompiled and checked
identical to the CMake object, compared with a throwaway C++ unit under the `openmp-release`
flags (compiled warning-free), GIMPLE and disassembly, 2026-10-16.

- (v) gfortran inlines `FINDROOTS_NR`, `FUNCD_FVAL` and `FUNCD_FDERIV` twice into
  `__w3sis2md_MOD_w3rpwnice`; g++ inlines the header the same way. Loop-invariant products
  (`DRAFT`, `C5`, `5*C5`, `ALP`, `C1`) are hoisted by both compilers; hoisting does not change
  bits.
- (v) The Newton loop runs at most 299 times in both objects (counter from 299 to 0): `J`
  starts at 1, loops while `J < 300`, and `J > MAXIT` (`:1290`) can never be true, so the
  `EXTCDE(1)` branch is removed by gfortran and by g++. A root that does not converge is
  returned silently after 299 iterations (see the WW3 note below).
- (v) `DX = FVAL/FDERIV` (`:1281`) is inlined by gfortran as Smith's algorithm
  (`-fcx-fortran-rules`, no `__divdc3`): if `|Im D| > |Re D|`, `r = Re D/Im D`,
  `den = r*Re D + Im D`, `DX = ((Im F + r*Re F)/den, (r*Im F - Re F)/den)`, else the mirror
  form. The header writes that branch out explicitly instead of `Kokkos::complex`'s division,
  which scales differently; the C++ GIMPLE shows the same operations in the same order.
- (v) `ABS(DX)` and `ABS(FVAL)` (`:1284`) are calls of libm's `cabs` in the Fortran object; the
  pinned glibc 2.44 `cabs` is a jump to `hypot@@GLIBC_2.35` (disassembled), which is what the
  C++ object calls through `Kokkos::hypot`. `ABS(FVAL)` is evaluated only when `ABS(DX) <= TOL`
  in both. `TOL = 1E-9` is `REAL`, widened to 9.99999971718e-10 for the `DOUBLE` comparison.
- (v) `WN_GUESS**5` and `**4` are libgfortran's `_gfortran_pow_c8_i4` calls in the Fortran
  object (two per Newton iteration); the included headers reproduce its binary powering
  (`FUNCD_FVAL_KOKKOS.md`, `FUNCD_FDERIV_KOKKOS.md`). g++ computes `X**2` and `X**4` once and
  shares them between the two functions; each value is the same product of the same operands as
  in libgfortran, so the bits are the same. `1.0*x` folded to `x` by g++: same bits.
- (v) `FLEX_RIGID = (ICEH*(ICEH*ICEH))*5.49E9/10.92000007629...`; `GS1 = DBLE(SIG(1)**2/GRAV)`
  in `float` then widened (`CMPLX` without `KIND` is single `COMPLEX`); `GS1 = DBLE(WN_I(IK))`
  after both roots (`:1181`); `WN_I`, `DAMPING` are the real and imaginary parts narrowed to
  `float`.
- (v) `CG_I` (`:1182-1183`): numerator `((5*FLEX_RIGID)*(W2*W2) + 10051.1494140625) -
  (922.5*ICEH)*SIG**2` with `W2 = WN_I*WN_I`, denominator `(2*SIG)*((922.5*WN_I)*ICEH + 1025)`,
  one division; `DENS*GRAV` folded to the `REAL` product. Same operations and order in both
  GIMPLE dumps.
- (v) No FMA in either function (no `vfmadd` in the C++ object), no libmvec call, no reduction
  across frequencies other than the `GS1` guess carried from `IK-1` to `IK`, which the loop
  keeps sequential.
- ⚠ When both operands of a commutative product or sum are NaN, the two objects do not always
  put them in the same order, so the NaN payload can differ; every non-NaN result is the same.
- ⚠ The Fortran's bits depend on the pinned libgfortran (`pow_c8_i4`) and glibc (`hypot`). On a
  GPU, `Kokkos::hypot` is CUDA's, not glibc's: the bit-for-bit rule would not hold there.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3sis2md.F90` from `build_mixa`'s
  flags (disassembly identical to the CMake object) and a C++ unit calling `w3rpwnice` and
  `findroots_nr` (warning-free); compared the GIMPLE of `__w3sis2md_MOD_w3rpwnice` and of the
  standalone `__w3sis2md_MOD_findroots_nr` operation by operation: the two Smith branches
  (taken on `|Im D| > |Re D|`, the NaN case in the second branch in both), the zero products of
  the `(C5,0)` and `(1,0)` complex multiplies kept, `GAMMA = SIGMA*0.0/...` kept for the first
  root, `TOL` as `0x3e112e0be0000000`, the 299-trip counter, `CG_I`'s 11 operations and the
  `.rodata` constants; no `vfmadd` in the C++ object; glibc 2.44 `cabs` disassembled as
  `jmp __hypot_finite@GLIBC_2.15` (the `hypot@@GLIBC_2.35` entry the C++ call binds to). The
  second root re-reads `SIG(IK)` and recomputes `ALP` and `Re C1` in the Fortran; g++ reuses
  the first root's values, which are the same bits. Fixed: the header and this report now
  point at `FINDROOTS_NR_KOKKOS.md`.
- ⚠ The C++ reads `WN_I(IK)` back from memory after writing `DAMPING(IK)`; the Fortran object
  keeps it in a register. Same value unless a caller passes the same array for both, which
  `W3SIS2` does not (`:869`, `WN_I`, `WN_RP`, `CG_I`).

WW3 note: `FINDROOTS_NR` documents a stop when the root does not converge (`:1290-1298`,
"ROOT NOT CONVERGED"), but the test `J .GT. MAXIT` sits after `J = J+1` under
`DO WHILE (J .LT. MAXIT ...)`, so `J` never exceeds `MAXIT` and the stop is dead code: a
non-converged root is used without a message. The translation reproduces this (the error path
is kept and is equally unreachable).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest input sets `IS2DISP = T`, so even a wired port would
not be exercised by WW3's regtests as they stand.
