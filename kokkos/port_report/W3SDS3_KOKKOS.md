# W3SDS3_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1601` behind `WW_KOKKOS_SDS3` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

The WAM4/WAM4+ whitecapping dissipation of the `ST3` source-term package (Bidlot et al. 2005
mean wavenumber), a source term `W3SRCE` calls per sea point (`w3srcemd.F90:1298-1301`);
`ww3_outp.F90:2279`, `ww3_ounp.F90:2451` and `gx_outp.F90:1202` call it too. It computes

D(k,θ) = C_ds 2π f̄ (k̄² E)² (δ₁ k/k̄ + δ₂ (k/k̄)²), S = D·N,

with k̄ bounded below by 10⁻¹⁰ in the ratios only (`:1382`), not in the steepness term (`:1383`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SDS3`, `model/src/w3src3md.F90:1253-1433` at NOAA-EMC/WW3 `03adda91`; compiled by the 47 regtest switch files with `ST3` (`ww3_ts1/input/switch_ST3`, five in `ww3_ts2`, twenty in `ww3_ts3`, twenty in `mww3_test_05`, `ww3_tnc1/input/switch`; `git grep -lw ST3 -- '*switch*'`), by no NOAA operational switch in `model/bin`. Its `#ifdef` blocks are `W3_S`, `W3_T`, `W3_T0`, `W3_T1`, all output; `STAB3` appears only in `W3SIN3` |
| Kokkos code | [`../src/ww_kokkos/srce_st3_sds3.hpp`](../src/ww_kokkos/srce_st3_sds3.hpp), `ww::srce::st3::w3sds3_point` |
| C shim | none yet; it passes `SSDSC1`, `DDELTA1`, `DDELTA2` of the current grid (`MPARS(IMOD)%SRCPS`, `w3gdatmd.F90:2662-2667`). `CG`, `USTAR`, `USDIR`, `DEPTH`, `IX`, `IY` are arguments no statement reads |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src3md.F90` (WW3's CMake Release flags, gfortran 16.2), recompiled
from the reference builds `ST3 STAB0 FLX0` and `ST3 STAB3 FLX0` (both from `switch_lab_shrd`) and
identical to each CMake object; compared with a throwaway C++ unit built with the `openmp-release`
flags, 2026-10-16.

- (v) `STAB3` does not change this routine: the disassembly of `__w3src3md_MOD_w3sds3` is
  instruction for instruction the same in the two builds, so there is no variant to translate.
- (v) Every operation in the object's order, read from the GIMPLE and the assembly of both sides:
  `WNMEAN*WNMEAN*EMEAN`, `((SSDSC1*TPI)*FMEAN)*ALFAMEAN²` (`ALFAMEAN**2` is one multiply), and per
  `IK` `K/WNMEAN2`, its square, `(DDELTA1*K)/WNMEAN2`, the sum and the product with `FACTOR`; two
  `divss` per `IK`, no reciprocal, no FMA, no libm or libmvec call, no reduction.
- (v) Constants: `1.E-10` (`0x2edbe6ff`) and `TPI` (`0x40c90fdb`, 2·`REAL(PI)` of `constants.F90`)
  are the same bits in `.rodata.cst4` of both objects.
- (v) `MAX(1.E-10, WNMEAN)` (`:1382`) is `maxss` with the constant as destination: a NaN `WNMEAN`
  gives the NaN. The header's `maxss(1e-10f, wnmean)` compiles to `vmaxss` with the same operands.
- (v) `D = 0.` (`:1378`) is a `memset` in both; `D` is written fully again only when
  `NSPEC = NK*NTH`, so the zero fill is kept.
- ⚠ `S = D * A` (`:1405`) is vectorised by both compilers (4+2 wide in Fortran, 8+4 wide in C++),
  with the two operands on different sides in the vector body and the tails. Only when both `D`
  and `A` are NaN with different payloads can the payload of `S` differ; every finite or infinite
  value is the same.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the `#cuda` shell), never run: the only `FFMA` in the
  SASS are those of the correctly rounded `div.rn` sequence; `MAX` is `FMNMX.NAN`, which also
  returns NaN for a NaN `WNMEAN`.
- (v) Fresh review 2026-10-16: no defect. `w3src3md.F90` recompiled from `build_st3` and
  `build_st3s` flags, identical to both CMake objects, `W3SDS3` the same instructions in both;
  GIMPLE and assembly compared with the C++ unit operation by operation (every multiply pairs the
  same operands with the same grouping, two `divss` by `MAX(1.E-10,WNMEAN)` per `IK`, `maxss`
  returning `WNMEAN` on NaN in both, `memset` for `D=0.`); argument order against the dummies and
  the four call sites; the cited lines and the 47 `ST3` switch files read at `03adda91`. Nothing
  changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
