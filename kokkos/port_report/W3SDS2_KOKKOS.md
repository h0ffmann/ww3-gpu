# W3SDS2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Whitecapping dissipation of Tolman and Chalikov (1996) and the diagonal term of its derivative
(switch `ST2`), a source term `W3SRCE` calls per sea point (`w3srcemd.F90:1296`). The
dissipation is a blend, through a frequency filter `FILT`, of a low-frequency term
`CDIST*K**2` and a high-frequency term `CDSA0*CDISH*SIG**3*(ALFA/SDSALN)**POW`, with
`D(ITH,IK) = D(1,IK)` and `S = D*A`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SDS2`, `model/src/w3src2md.F90:582-825` at NOAA-EMC/WW3 `03adda91`; compiled by the NOAA operational switch `model/bin/switch_NCEP_st2` and by 83 regtest switches (`mww3_test_05` 27, `ww3_ta1` 28, `ww3_ts1` 2, `ww3_ts2` 5, `ww3_ts3` 21; `git grep -lw ST2 -- '*switch*'`). Also called by `ww3_outp.F90:2275`, `ww3_ounp.F90:2447`, `gx_outp.F90:1199` |
| Kokkos code | [`../src/ww_kokkos/srce_st2_sds2.hpp`](../src/ww_kokkos/srce_st2_sds2.hpp), `ww::srce::st2::w3sds2_point`; the Fortran's per-call `EBD(NK)` (automatic array, `malloc` in the object) is a caller-provided work array |
| Variants | none that changes the arithmetic: the routine's only `#ifdef` blocks are `W3_S` (tracing) and `W3_T`, `W3_T0`, `W3_T1`, `W3_T2` (test output, `POWMAX`, `DOUT`), all dropped. `STAB2` and `FLX3` of the reference switch do not reach this routine |
| C shim | none yet; it passes `SIG(1:NK+1)`, `DDEN(1:NK)` and the W3GDATMD scalars `DTH FTE FPIMIN XF1 XF2 XFH SDSALN CDSA0-2 CDSB0-3` of the current grid |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src2md.F90` (WW3's CMake Release flags, gfortran 16.2.0, the
switch of `switch_NCEP_st2`'s physics: `ST2 STAB2 FLX3` on `switch_lab_shrd`), recompiled with
`-fdump-tree-optimized-lineno` and checked identical to the CMake object; compared operation by
operation with the optimised GIMPLE and assembly of a throwaway C++ unit built with the
`openmp-release` flags (g++ 16.2.0, `-ffp-contract=off`, warning-free), 2026-10-16.

- (v) Every expression has the same operands, the same association and the same rounding steps
  in both GIMPLE dumps (commuted operands of one `*` or `+` only): `XHW = FACTI2+FACTI1*LOG(FHW)`,
  `HW`, `EHIGH`, `FPIT`, `PHI`, `BFILT`, `CDIST`, `CDISH`, `CDISP`, `FILT`, `POW` and `D(1,IK)`.
  `AFILT + BFILT*SIG` is `SIG*BFILT - AF1*BFILT` in both, exact by IEEE negation.
- (v) Integer powers as `-O3` expands them (`powmult` in the GIMPLE): `(SIG(NK)/FHW)**5` is
  `x2 = x*x, x3 = x*x2, x5 = x2*x3` (`:730`), `SIG**3` is `SIG*(SIG*SIG)` (`:766`), `K**2` and
  `USTAR**2` are one product. The real powers `FPIT**CDSB3` (`:737`), `(CDISP*SIG)**CDSA2`
  (`:762`) and `(ALFA/SDSALN)**POW` (`:766`) are `powf` in both objects; `LOG` is `logf`.
  No libmvec call in either object.
- (v) Folded constants: `TPIINV` `0x1.45f306p-3`, `G2PI3I` `0x1.5fb1fcp-15`, `G1PI1I`
  `0x1.09eb14p-6`, `GRAV` `9.806f`; the header writes the two `G*PI*I` as hex literals with a
  `static_assert` of `constants.F90:82-83`, and `2.*USTAR` is `USTAR+USTAR` in the object, the
  same bits.
- (v) Reductions: the `EBD` sum over `ITH` (`:716-717`) and the `HW` sum over `IK` (`:725-726`)
  are vectorised in the Fortran object as in-order (fold-left) reductions, lane by lane from the
  running sum; the C++ object keeps the `EBD` loop scalar and vectorises the `HW` loop the same
  fold-left way. Both add in the source order.
- (v) MIN/MAX operand order from the object: `maxss` at `:723` (`XHW-0.5`, then `0`), `:736`
  (`FPI*TPIINV*USTAR/GRAV`, then `FPIMIN`) and `:761` (argument, then `0`), `minss` at `:761`
  (then `1`) and `:762` (then `25`); the C++ `maxss`/`minss` helpers compile to the same
  `a > b ? a : b` selects. `INT(XHW+0.5)` is `cvttss2si`, `ww::st4::fint`.
- (v) `MOD(XHW+0.5, 1.)` is an x87 `fprem` loop in both objects; the C++ one calls `fmodf` only
  when that loop returns a NaN (the `-fmath-errno` path). Both are exact remainders, so the same
  value for finite input.
- ⚠ `SQRT` is `sqrtss` in both; the C++ object adds the `-fmath-errno` call to `sqrtf` for a
  negative argument, which returns the same NaN. `(CDISP*SIG(IK))**CDSA2` is computed every `IK` in the
  C++ object and only where `FILT > 0` in the Fortran one (no effect on `D` or `S`).
- ⚠ `IKHW = MIN(NK, INT(XHW+0.5))` (`:713`) has no lower bound. With `XHW = 1 + ln(FHW/SIG(1))/ln(XFR)`
  (`w3gridmd.F90:3556-3557`), `IKHW < 1` when `XFH*FPI < SIG(1)/SQRT(XFR)`, and then
  `w3src2md.F90:714-726` write `EBD` and read `A`, `DDEN`, `CG` out of bounds (undefined
  behaviour in WW3). The port reproduces the path; whether `FPI` can be that small in `W3SRCE`
  was not checked.
- ⚠ CUDA compiled (`nvcc_wrapper`, `sm_89`, `--fmad=false`, the `#cuda` shell's Kokkos 5.2.0),
  never run. The SASS has 162 `FFMA`, from CUDA's IEEE division and its `powf`, `logf`, `fmodf`
  and `sqrtf`; whether those return glibc's bits is not shown.
- (v) Fresh review 2026-10-16: no defect in the header. The Fortran module was recompiled from
  `build_st2`'s `flags.make` (identical to the CMake object) and the C++ unit with the
  `openmp-release` flags (warning-free); both optimised GIMPLE dumps and both disassemblies were
  read operation by operation for `:711-792`: operand association, the folded constants, the
  `powmult` chains, the four `maxss`/`minss` sites (operand order and NaN result), `cvttss2si`,
  the in-order `EBD` and `HW` sums, `logf`/`powf` with no libmvec, no FMA. The `#ifdef` blocks of
  `:582-825` are only `W3_S` and `W3_T*`. Two sentences of this report were corrected (the C++
  `MOD` is `fprem` too; which power is unconditional).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
