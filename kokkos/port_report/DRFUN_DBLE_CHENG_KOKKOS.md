# DRFUN_DBLE_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The residual of the IC3 "Cheng" dispersion relation for waves under a viscoelastic ice layer of
thickness `HICE` over water of depth `DEPTH`, at a complex wavenumber `WN`: the function whose
root the Muller iteration `CMPLX_ROOT_MULLER_CHENG` finds. `F_ZHAO_CHENG` calls it when
`JUDGE > 0` (`w3sic3md.F90:2804`). With `ES >= 1.E5` and `JUDGE /= 2`, or `JUDGE == 3`
(`:2924`), it returns the determinant of the 4 x 4 boundary-condition matrix `AA` by `BSDET`
(`:2925-2948`); otherwise the closed form `J1`, `J2` (`:2950-2959`), divided (`JUDGE == 2`) or
multiplied out (`JUDGE == 1`). `F_ZHAO_CHENG` is called by `CMPLX_ROOT_MULLER_CHENG`
(`:2648-2650`, `:2692`), which `W3IC3WNCG_CHENG` (`:1939`) and `WN_PRECALC_CHENG`
(`:2284-2338`) call with `JUDGE` = 1, 2 or 3; `W3IC3WNCG_CHENG` runs when `IC3CHENG` is true, its
default (`w3gridmd.F90:2929`), so this routine is on the path of every IC3 regtest run with
`IC3CHENG = T` (⚠ which of its branches a given regtest takes was not checked).

## What was ported

| | |
|---|---|
| WW3 routine | `DRFUN_DBLE_CHENG`, `model/src/w3sic3md.F90:2812-2962` at NOAA-EMC/WW3 `03adda91` (151 lines); compiled under switch `IC3`, by 46 switch files of the repository (`git grep -lw IC3 -- '*switch*'` lists 47 paths, one of them `model/src/cmake/switches.json`; regtests `ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_drfun_dble_cheng.hpp`](../src/ww_kokkos/ic3_drfun_dble_cheng.hpp), `ww::ic3::drfun_dble_cheng(const Kokkos::complex<double>& wn, double sigma, double es, double nu, double dice, double hice, double depth, int judge) -> Kokkos::complex<double>`, the `drfun_dble` callable that `ww::ic3::f_zhao_cheng` (`ic3_f_zhao_cheng.hpp`) takes; `DOUBLE COMPLEX` is `Kokkos::complex<double>`, `DOUBLE PRECISION` is `double` |
| Callees | `BSDET` (`:1611-1730`) through `ww::ic3::bsdet` (`ic3_bsdet.hpp`); `AA(4,4)` and BSDET's automatic `MAT(4,4)` are local arrays of 16 elements. The `cexp`, `csqrt`, `cpow` and reciprocal helpers of `ic3_drfun_quad_cheng.hpp` are reused |
| C shim | none; its caller `CMPLX_ROOT_MULLER_CHENG` is not ported |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object, 2026-10-16. Its GIMPLE (`-fdump-tree-optimized-lineno`) was
compared with that of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`. That unit compiles warning-free; a second copy redeclares `bsdet`
`noinline` so the 16 elements of `AA` passed to it can be read, as in the Fortran object, where
`BSDET` is a call.

- (v) Expression trees: both GIMPLE dumps were evaluated symbolically along each of the 480
  paths of the control-flow graph (the Smith quotients by `|re| < |im|`, `REAL(WN*DEPTH) < 18`,
  the `ES`/`JUDGE` tests), with vector lanes, `VEC_PERM_EXPR` and stores into `AA` followed,
  operands of `+` and `*` sorted and Fortran parentheses kept as their value. The 480 path
  condition sets match; on 400 paths the returned parts and, on the 160 paths that call `BSDET`,
  the 32 doubles of `AA` are identical, Fortran against C++. The same comparison flags one
  operation changed by hand in a copy of the C++ dump (`a - b` as `b - a`, `x * 0.0` as
  `x * 1.0`) and does not flag a swapped operand of `*`.
- ⚠ The other 80 paths are `JUDGE` neither 1 nor 2 with the `BSDET` branch not taken (`ES < 1.E5`
  and `JUDGE` not 3): the Fortran never sets `FUNC1` (`:2955-2959`) and the object returns
  whatever `%r15` and a stack slot hold. The header returns `(0.0, 0.0)` there. No WW3 caller
  passes such a `JUDGE`.
- (v) libm calls: both objects call `cexp` three times, `csqrt` once and `cpow` nine times
  (`WN**2.` once for `:2905`, `:2906` and both branches; `ALPHA**2.` once in the `AA` branch for
  `AA(1,3)`, `AA(3,3)`, `AA(3,4)`; `N**4.`, `VE**4.`, `WN**6.`, `ALPHA**2.`, `WN**3.`, `VE**2.`,
  `N**2.` in the `J1`/`J2` branch). gfortran does not expand `z**2.` for a complex `z`. No libmvec
  call.
- (v) Complex arithmetic is written component by component as gfortran emits it: a REAL operand
  promoted to `(x, 0.0)` keeps its products with `0.0`; `2 * CMPLX(0.,1.)` folds to `(0, 2)`;
  `SIGMA/VE`, `DWAT/DICE*SIGMA**2/TH`, the reciprocals `1/TEMP`, `TH` and `TH*J1/(J2+1.e-20)` are
  Smith's algorithm inlined; `- N*SIGMA*SK + M*CK` is folded to `M*CK - N*SIGMA*SK` (exact);
  `-X*Y` is `-(X*Y)`; `DBLE(GRAV)**2.` is `0x1.80a16a139p+6`, the exact square of
  `DBLE(9.806f)`; `1.e-20` is the REAL literal widened, `0x1.79ca1p-67` (static_asserts).
- (v) `CMPLX(NU, ES/DICE/SIGMA)` (`:2904`) has no `KIND=`, so both parts are rounded to the
  default REAL kind: the viscosity and the shear term enter at single precision. Reproduced, not
  fixed.
- (v) `REAL(WN*DEPTH) .LT. 18.D0` with a NaN takes `TH = 1`; `ES >= 1.E5` with a NaN `ES` is false
  and leaves `JUDGE == 3` to decide, in both.
- ⚠ The order of the operands of each commutative `+` and `*` was not compared instruction by
  instruction; it changes only which NaN's payload a NaN result carries. The Fortran computes
  `AA(1,2)` and `AA(2,1)` in SSE2 vector lanes, the C++ stores `AA` in 32-byte vectors: per lane
  the trees are identical.
- ⚠ On a device the helpers call `Kokkos::exp`, `Kokkos::sqrt`, `Kokkos::pow` and `Kokkos::abs`
  of `Kokkos::complex<double>`, other algorithms than glibc's: not bit-checked. CUDA not compiled.
- The C++ zero-initialises `AA` and `MAT` (`Kokkos::complex` constructor) before filling them;
  it changes no value.
- No reduction, no MIN/MAX, no `INT`/`NINT` (BSDET's pivot search is its own report).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
