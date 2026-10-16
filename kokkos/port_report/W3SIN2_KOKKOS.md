# W3SIN2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The Tolman and Chalikov (1996) wind input source term (switch `ST2`), with its input peak
frequency `FPI` and the swell filter on the diagonal term. `W3SRCE` calls it per sea point
(`w3srcemd.F90:1230-1231`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIN2`, `model/src/w3src2md.F90:308-566` at NOAA-EMC/WW3 `03adda91` (259 lines); it reads the module table `BETATB(-400:401,1:21)` and the steps `DSIGA`, `DDRAG` (`:93-99`) that `INPTAB` fills. Its only `#ifdef` blocks are `W3_S`, `W3_T`, `W3_T0`, `W3_T1`, `W3_T2`: tracing and test output, no arithmetic. `ST2` is in the operational `switch_NCEP_st2` and in 83 regtest switch files (`mww3_test_05` 27, `ww3_ta1` 28, `ww3_ts1` 2, `ww3_ts2` 5, `ww3_ts3` 21; `git -C WW3-noaa grep -lw ST2 -- '*switch*'`) |
| Kokkos code | [`../src/ww_kokkos/srce_st2_sin2.hpp`](../src/ww_kokkos/srce_st2_sin2.hpp), `ww::srce::st2::w3sin2_point`; the automatic array `SIN1A(NK)` is caller-provided work |
| Table | `betatb` is `BETATB` flat in Fortran column-major order, element `(i,j)` at `(i+400) + 802*(j-1)`, as `INPTAB`'s port (`srce_st2_inptab.hpp`) builds it; this header only reads it |
| C shim | none yet; it passes `SIG` from `SIG(1)`, `SIG2`, `DDEN`, `ECOS`, `ESIN`, the `W3GDATMD` scalars `XFR`, `FTE`, `FTTR`, `FPIMIN`, `ZWIND`, `FSWELL`, `FACTI1`, `FACTI2`, and the current grid's `BETATB`, `DSIGA`, `DDRAG` |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src2md.F90` (WW3's CMake Release flags, gfortran 16.2, the
switch `ST2 STAB2 FLX3`, the physics of `switch_NCEP_st2`), recompiled in scratch and byte-identical
in its disassembly to the CMake build's object; compared with a throwaway C++ unit built with the
`openmp-release` flags (g++ 16.2, `-O3 -march=x86-64-v3 -ffp-contract=off`), warning-free, by
GIMPLE and assembly, 2026-10-16.

- (v) Every floating-point statement in the Fortran's order: `COSFAC`, `LAMBDA = TPI/(K*|COSFAC|)`,
  `ULAM`, `CLAM = CD*(U/ULAM)**2` as one multiply of the quotient by itself, `OMA = ((K*ULAM)*COSFAC)/SIG2`,
  the bilinear `BETA` as `((t1+t2)+t3)+t4`, `D = BETA*SIG2`, `S = A*D`; `SIG**3` as `(s*s)*s` in both;
  `FP1STR`, `FP1 = ((0.8*FP1STR)*GRAV)/USTAR`; `TRANS = (SIG/FP1-0.75)*4`, the division by `1-0.75`
  folded to an exact multiply by 4 in both objects. Constants with the object's bits (`TPI`
  6.2831855, `GRAV`, 0.0087, 1.1, 1E-20, 3.6E-4, 0.92, 6.3E-10, 0.8, 0.75). No FMA in either object.
- (v) libm: both objects call `sincosf` once for `COS(UDIR)`, `SIN(UDIR)` and `logf` for every `LOG`;
  `SQRT(CD)` is `sqrtss` in both (the C++ falls back to `sqrtf` for a negative `CD`, same NaN). No
  libmvec call.
- (v) Reductions: `SIN1A(IK)` sums `MAX(0,S)` over `ITH` in order from 0. The `M0`, `M1` loop
  (`:494-498`) is vectorised by both compilers (SSE 4 lanes in the Fortran, AVX 8 lanes in the C++)
  but as an in-order fold: each lane is added to the running sum one after another, so the bits
  are those of the scalar loop in both.
- (v) `MAX` operand order from the assembly, `maxss d, s` giving `s` on a NaN in either operand or a tie:
  `MAX(|COSFAC|, 0.0087)` (`:459`), `MAX(S, 0)` (`:488`), `MAX(FPI*USTAR/GRAV, FPIMIN)` (`:512`, FPIMIN
  second), `MAX(FSWELL*D, D)` at `:521` (vector body, epilogue and scalar tail alike) and at `:528`,
  FSWELL*D first in both. The C++ object shows the same order at each site.
- (v) `INT()` at `:465` (twice), `:466`, `:516`, `:517` through `fint` (`st4_params.hpp`), gfortran's
  `cvttss2si`: out of range or NaN gives `INT_MIN`, which the clamps at `:469-470` and the `MIN`/`MAX`
  at `:516-518` then handle as in the Fortran. `SIGN(-1.1,OMA)` is `copysignf(1.1,OMA)` in both, so
  `OMA = -0` gives -1. `RD1`, `RD2` use the unclamped `IOMA`, `ICL`, as the Fortran does.
- (v) Variants: none changes the arithmetic of `W3SIN2`; `STAB2` and `FLX3` act in other routines.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, `nix develop picasso/labs/pratico#cuda`), never run. The
  SASS has `FFMA`, expected inside CUDA's division and `logf`, `sincosf` implementations; CUDA's
  `logf`, `sinf`, `cosf` are not shown to return glibc's bits, so the device path is not claimed bit
  for bit.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3src2md.F90` (identical to the
  `build_st2` CMake object) and the C++ unit, and compared GIMPLE and assembly statement by statement:
  constants' bits, operand order of every `maxss`/`maxps` site (`:459`, `:488`, `:512`, `:521` body,
  epilogue and tail, `:528`), the in-order `M0`/`M1` folds (Fortran 4+2+1 lanes, C++ 8+4+1, each
  `(s*s)*s` or its commuted equal), `cvttss2si` at every `INT`, the `BETATB` offsets (Fortran
  `802*ICL-402+IOMA`, C++ `(IOMA+400)+802*(ICL-1)`, the layout `srce_st2_inptab.hpp` fills), libm calls.
  Fixed: the header restated `NRSIGA`, `NRDRAG` and the table extent instead of taking them from
  `srce_st2_inptab.hpp`; it now includes that header and uses its constants, and the object of
  `w3sin2_point` is unchanged by the edit (disassembly compared); the CUDA unit still compiles.
  ⚠ When `OMA` is NaN with its sign bit set, `INT(OMA/DSIGA) + (-1)` overflows `INT_MIN` in both
  languages (undefined in each); both objects do a plain wrapping add and `RD1` is NaN either way.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
