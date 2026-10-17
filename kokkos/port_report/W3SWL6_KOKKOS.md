# W3SWL6_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1662` behind `WW_KOKKOS_SWL6` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Turbulent dissipation of narrow-banded swell after Babanin (2011, section 7.5), the swell term of
the `ST6` package. Per wavenumber, `D = -(2/3) B1 sigma sqrt(Bn)` with `Bn` the spectral
saturation of Banner et al. (2002) built from the directionally normalised spectrum, applied to
every direction, and `S = D A`. `W3SRCE` calls it per sea point when `SWL6S6` is true
(`w3srcemd.F90:1330-1334`); `ww3_outp`, `ww3_ounp` and `gx_outp` call it too
(`ww3_outp.F90:2288`, `ww3_ounp.F90:2467`, `gx_outp.F90:1211`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SWL6`, `model/src/w3swldmd.F90:264-439` at NOAA-EMC/WW3 `03adda91`. The module is compiled with `ST6`: 62 switch files (`git grep -lw ST6 -- '*switch*'`), in `ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15`, `mww3_test_05`, and `model/bin/switch_NRL3`, `switch_swin`, `switch_UoM_nl1`, `switch_UoM_nl3`, `switch_UoM_nl3s`; no `switch_NCEP_*` or `switch_NWS_*`. `SWL6S6 = SWLB1 > 0` and the default `SWLB1` is `0.41E-02` (`w3gridmd.F90:2207,2328`), so every `ST6` run calls it unless the namelist sets `SWLB1 <= 0`; `ww3_ts1/input_nl5_*` set `SWLB1 = 6E-3` |
| Variants | No `#ifdef` changes its arithmetic: `W3_S` (`STRACE`) and `W3_T6` (`USE NDST`) only. The one runtime branch, `SWL6CSTB1` (`CSTB1`, default false, `w3gridmd.F90:2206`), is a `bool` argument and both sides are translated: false scales `B1` by the steepness at the spectral peak (`:399-410`), true keeps `SWL6B1` |
| Kokkos code | [`../src/ww_kokkos/srce_swl6.hpp`](../src/ww_kokkos/srce_swl6.hpp), `ww::srce::swl::w3swl6_point`, and `w3swl6_ikn`, the host build of the `SAVE` table `IKN = IRANGE(1,NSPEC,NTH)` with `irange` of [`srce_swl_util.hpp`](../src/ww_kokkos/srce_swl_util.hpp). The kernel takes the table and its count and returns 2 where the Fortran stops in `EXTCDE(2)` on `SIZE(IKN) /= NK` (`:374-377`), with `S` and `D` not written, as in the Fortran; the shim prints `6001` and stops. The automatic arrays `ABAND`, `KMAX`, `ANAR`, `BN`, `DDIS` (NK) and `K` (NTH,NK), which the Fortran mallocs per call, are caller-provided work |
| C shim | none yet |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` object of `w3swldmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4`, reference build `build_st6f4`), recompiled with
`-fdump-tree-optimized-lineno` and checked identical in its disassembly to the CMake object;
compared operation by operation with a throwaway C++ unit compiled with the `openmp-release`
flags (warning-free), 2026-10-16.

- (v) `ABAND = SUM(RESHAPE(A,(/NTH,NK/)),1)` (`:379`): `_gfortran_reshape_r4` copies `A`, then a
  per-column sum from `0.0` in `ITH` order; the vectoriser only loads four lanes and adds them
  one after the other. The C++ sums the same column of `A` in the same order from `0.0`; its GIMPLE
  is the same in-order chain.
- (v) `KMAX = MAXVAL(K,1)` (`:388`) is a call of libgfortran's `_gfortran_maxval_r4`, read in the
  pinned `libgfortran.so.5.0.0` (gfortran-16.2.0-lib): an empty column gives `-HUGE`
  (`0xff7fffff`); otherwise it skips to the first element with `comiss` `>= -Inf`, writes the quiet
  NaN `0x7fc00000` when there is none (all NaN), and keeps the running value with
  `maxss %xmm0,%xmm1` (`x > r ? x : r`) from there. The C++ reproduces those three cases with the
  same constants and comparisons.
- (v) `KMAX < 1.0E-34` (`:390`) and `ABAND > 1.E-30` (`:414`): `comiss` with the constants
  `1.0000000467e-34` and `1.0000000031e-30`, the same float bits in both GIMPLEs; a NaN takes the
  else branch in both.
- (v) `K/KMAX` (`:393`), `ANAR = 1.0/(SUM(K,1)*DTH)` (`:396`), `BN = ANAR*(ABAND*SIG*DTH)*WN**3`
  (`:397`): element-wise, vectorised over `ITH` or `IK` lanes in both objects, so no lane mixes
  with another. `WN**3` is `WN*(WN*WN)` in both; the product order is
  `(ANAR*((ABAND*SIG)*DTH))*WN**3`; `SUM(K,1)` is in order from `0.0`.
- (v) `MAXLOC(ABAND,1)` (`:406`), inline: the first element `>= -Inf`, then a strict
  `limit < x` (`comiss`, `cmova`) keeps the first maximum; all NaN gives `IK = 1`. The C++ does
  the same.
- (v) `B1 = SWL6B1*(2.*SQRT(SUM(ABAND*DDEN/CG))*WN(IK))` (`:408`): `(DDEN*ABAND)/CG` summed in
  `IK` order from `0.0` (vectorised loads and divisions, scalar adds in order, in both objects),
  `sqrtss`, `2*x` as `addss x,x` (the same bits), then `*WN(IK)`, `*SWL6B1`.
- (v) `DDIS = -(2./3.)*B1*SIG*SQRT(BN)` (`:415`): `2./3.` folded to `0x3f2aaaab`
  (`kTwoThirds`, with a `static_assert`), then `*B1`, `*SIG`, `*SQRT(BN)` and a sign flip
  (`xorps`) of the whole product, in both.
- (v) No FMA, no libmvec call, no reduction reordered in either object. The Fortran inlines
  `sqrtss` (`-fno-math-errno`); g++ emits `vsqrtss` and calls glibc's `sqrtf` only for a negative
  argument, for `errno`. glibc 2.44's `sqrtf` returns `(x-x)/(x-x)` there (`__math_invalidf`), the
  default NaN `0xffc00000` that `sqrtss` gives, so the bits are the same.
- (v) `D(IKN+(ITH-1)) = DDIS` (`:420-422`) and `S = D*A` (`:424`): integer index arithmetic
  (the Fortran mallocs the temporary `IKN+(ITH-1)` once per `ITH`) and an element-wise product.
- ⚠ The order of the operands of a commutative `+` or `*` is not always the Fortran's (for
  example `DTH*SUM` against `SUM*DTH`). The result is the same except for the payload of a NaN
  when both operands are NaN.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell): the PTX has `div.rn.f32`,
  `sqrt.rn.f32`, `mul.rn.f32`, `add.rn.f32` and no `fma`; the `FFMA` in the SASS are the
  correctly rounded expansions of `div.rn` and `sqrt.rn`. Never run. On a device `sqrtf` is
  CUDA's.
- (v) Fresh review 2026-10-16: no defect. `w3swldmd.F90` recompiled from `build_st6f4`'s
  `flags.make` and found identical in its disassembly to the CMake object; its GIMPLE read
  against the C++ unit's for `:379-424` (in-order sums at `:379`, `:396`, `:408`, division by
  `KMAX` not a reciprocal, `WN**3` power tree, folded `2./3.`, `1.0E-34` and `1.E-30` bits,
  `MAXLOC` inline with all-NaN giving 1 and empty giving `WN(0)`, both `SWL6CSTB1` sides);
  `_gfortran_maxval_r4` disassembled again from the pinned `libgfortran.so.5.0.0`, its `.rodata`
  read (`0xff800000`, `0xff7fffff`) and its `maxss` operand order matched to the C++ `vmaxss`;
  `srce_swl6.hpp`, `srce_swl4.hpp` and `srce_swl_util.hpp` compiled in one unit, warning-free with
  the `openmp-release` flags and for CUDA (`sm_89`, `--fmad=false`, compile only). Nothing
  changed.

### In WW3 itself (reproduced, not fixed)

- ⚠ `IKN` is `ALLOCATABLE, SAVE` and allocated on the first call (`:359`, `:370-372`). Under
  `OMPG`, `W3SRCE` runs inside `W3WAVE`'s parallel loop over points, so the first calls race on
  the allocation (`AGENTS.md`, "Bit for bit"). It aborts a run; it does not change bits. A table
  built once on the host removes the race on the port side without changing a value.
- ⚠ `IKN` is one table per process, not per grid. In `ww3_multi`, a second grid with the same
  `NK` and another `NTH` passes the `SIZE(IKN) /= NK` test and scatters `D` with the first
  grid's `NTH` (`:421`): past the grid's `NSPEC` when its `NTH` is smaller, leaving elements
  of `D` at zero when it is larger. To reproduce the
  Fortran, the shim keeps one table for the process, built by the first call; a table per grid
  would be a fix and is the owner's decision.
- ⚠ With `NK = 0`, `MAXLOC` of an empty `ABAND` returns 0 and `:408` reads `WN(0)` (the GIMPLE
  indexes `wn[-1]`). `IRANGE(1,0,NTH)` has one element for `NTH > 1`, so the `SIZE(IKN) /= NK`
  test stops first; only a grid with `NK = 0` and `NTH = 1` gets there, where the C++ reads
  `wn[0]`, also out of bounds, and the value is never used (the `IK` loops are empty).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
