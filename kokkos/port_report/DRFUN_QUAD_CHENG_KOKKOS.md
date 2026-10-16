# DRFUN_QUAD_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The residual of the IC3 "Cheng" dispersion relation for waves under a viscoelastic ice layer of
thickness `HICE` over water of depth `DEPTH`, at a complex wavenumber `WN`: the function whose
root the Muller iteration `CMPLX_ROOT_MULLER_CHENG` finds. `F_ZHAO_CHENG` calls it when
`JUDGE == 0` (`w3sic3md.F90:2806`). Every caller of `F_ZHAO_CHENG` passes `JUDGE` = 1, 2 or 3
(`:1939`, `:2284-2338`), so this routine is never reached in WW3: it is translated for
completeness and no regtest can exercise it. Despite its name it computes in `COMPLEX(8)`: the
code comment at `:3031-3033` says the original `COMPLEX(16)` locals were changed to `COMPLEX(8)`.
No `REAL(16)` and no libquadmath call appear in the object.

## What was ported

| | |
|---|---|
| WW3 routine | `DRFUN_QUAD_CHENG`, `model/src/w3sic3md.F90:2967-3088` at NOAA-EMC/WW3 `03adda91` (122 lines); compiled under switch `IC3`, by 46 switch files of the repository (`git grep -lw IC3 -- '*switch*'` lists 47 paths, one of them `model/src/cmake/switches.json`; regtests `ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_drfun_quad_cheng.hpp`](../src/ww_kokkos/ic3_drfun_quad_cheng.hpp), `ww::ic3::drfun_quad_cheng(const Kokkos::complex<double>& wn, double sigma, double es, double nu, double dice, double hice, double depth) -> Kokkos::complex<double>`, the `drfun_quad` callable that `ww::ic3::f_zhao_cheng` (`ic3_f_zhao_cheng.hpp`) takes; `DOUBLE COMPLEX` is `Kokkos::complex<double>`, `DOUBLE PRECISION` is `double` |
| Called by | `F_ZHAO_CHENG` (`:2806`) only, on the `JUDGE == 0` branch that no caller takes |
| C shim | none |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object, 2026-10-16. Its GIMPLE (`-fdump-tree-optimized-lineno`) was
compared with that of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, which compiles warning-free.

- (v) Expression trees: both GIMPLE dumps were expanded, along each of the 40 paths of the
  control-flow graph (six comparisons: the three Smith quotients by `|re| < |im|`, `REAL(WN*DEPTH)
  < 18`, and two more Smith quotients inside it), into one tree per returned part, with the
  operands of `+` and `*` sorted and Fortran parentheses kept as their value. The 40 path
  signatures and the 80 trees are identical, Fortran against C++. The same comparison flags a single
  operation changed by hand in a copy of the C++ dump (`0.0 - x` as `x - 0.0`, `r + 0.0` as
  `r * 1.0`).
- (v) libm calls: the Fortran object calls `cpow` eight times (`WN**2.`, `N**4.`, `VE**4.`,
  `WN**6.`, `ALPHA**2.`, `WN**3.`, `VE**2.`, `N**2.`, with `WN**2.` computed once for `:3062`,
  `:3063`), `csqrt` once and `cexp` three times; the C++ object makes the same twelve calls
  (`std::pow`, `std::sqrt`, `std::exp` of `std::complex<double>` lower to `__builtin_cpow`,
  `__builtin_csqrt`, `__builtin_cexp`). gfortran does not expand `z**2.` into a product for a
  complex `z`: it calls `cpow(z, (2.0, 0.0))`, and so does the header. No libmvec call.
- (v) Complex arithmetic is written component by component as gfortran emits it: a REAL operand
  promoted to `(x, 0.0)` keeps its products with `0.0`, the three reciprocals `1.D0/TEMP` and the
  quotients `SIGMA/VE` and `TH` are Smith's algorithm inlined, `(..)/2.D0` is the product by `0.5`,
  `DBLE(GRAV)**2.` is folded to `0x1.80a16a139p+6`, the exact square of `DBLE(9.806f)`
  (static_assert), `SIGMA**2.` is `SIGMA*SIGMA`, and `TH = 1` multiplies `WN` with the products
  by `1.0` folded, as in the object.
- (v) `CMPLX(NU, ES/DICE/SIGMA)` (`:3061`) has no `KIND=`, so both parts are rounded to the
  default REAL kind (`cvtsd2ss`, `cvtss2sd` in both objects): the viscosity and the shear term
  enter at single precision although the routine is double. Reproduced, not fixed.
- (v) `REAL(TEMP) .LT. 18.D0` with a NaN takes the `TH = 1` branch in both.
- ⚠ The order of the operands of each commutative `+` and `*` was not compared instruction by
  instruction (the Fortran object is SSE2, the C++ AVX, register allocation differs). It changes
  no value except which NaN's payload and sign a NaN result carries when both operands are NaN.
- ⚠ On a device the header calls `Kokkos::exp`, `Kokkos::sqrt` and `Kokkos::pow` of
  `Kokkos::complex<double>`, other algorithms than glibc's: not bit-checked. CUDA not compiled.
- No reduction, no MIN/MAX, no `INT`/`NINT`.
- Op count: Fortran object 177 `mulsd`, 74 `addsd`, 69 `subsd`, 33 `divsd`; C++ object 173, 75,
  70, 33. The difference is block duplication (the Fortran duplicates the `TH*WN` products into
  both arms of the `TH` quotient), not arithmetic: every path's tree is identical.
- (v) Fresh review 2026-10-16: no defect. The Fortran object was recompiled from `build_mixa`'s
  flags and found identical to the CMake object; the Fortran GIMPLE was read statement by
  statement against the header in source order, and every `-`, `/`, `x + 0.0`, `0.0 - x`, Smith
  branch (`|re| < |im|`, NaN to the else arm) and `REAL(WN*DEPTH) < 18` test matches, operands in
  the same order; the per-path trees are identical again (40 paths, commutative operands sorted).
  With operands not sorted, the trees differ only in the order of the operands of `+` and `*`,
  which changes no bit except a NaN payload. The folded `DBLE(GRAV)**2.` is the same double in both
  dumps; both objects call `cpow` 8 times, `cexp` 3, `csqrt` 1, with no FMA, `__muldc3`,
  `__divdc3` or libquadmath call. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: WW3 never calls `F_ZHAO_CHENG` with
`JUDGE == 0`.
