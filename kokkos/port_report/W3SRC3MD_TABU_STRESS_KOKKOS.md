# W3SRC3MD_TABU_STRESS_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3KOKKOS_ST3_SETUP (INSIN3)`, behind `WW_KOKKOS_SPR3, SIN3 or SDS3` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

The ST3 (WAM4+) friction velocity table `TAUT(0:ITAUMAX,0:JUMAX)` = `SQRT(TAU)` as a function of
the wave-supported stress and the 10 m wind, and its steps `DELU`, `DELTAUW`, built once by
`INSIN3` (`w3src3md.F90:808`) and read by `CALC_USTAR` of the same module. The ST4 routine of the
same name is [`TABU_STRESS_KOKKOS.md`](TABU_STRESS_KOKKOS.md).

## What was ported

| | |
|---|---|
| WW3 routine | `TABU_STRESS` of `W3SRC3MD`, `model/src/w3src3md.F90:822-943` (module parameters and tables `:95-101`) at NOAA-EMC/WW3 `03adda91`; compiled under switch `ST3` by the regtest switches of `ww3_ts1`, `ww3_ts2`, `ww3_ts3`, `ww3_tnc1` and `mww3_test_05` (`git grep -lw ST3 -- '*switch*'`), by no NOAA operational switch. It has no `#ifdef` block |
| Kokkos code | [`../src/ww_kokkos/srce_st3_tabu_stress.hpp`](../src/ww_kokkos/srce_st3_tabu_stress.hpp), host function `ww::srce::st3::tabu_stress(zzwnd, aalpha, zz0max, taut)`: writes `taut[kTautSize]` (201 x 201), `TAUT(I,J)` at `I + 201*J`, and returns `TabuStressSteps{delu, deltauw}`; the module parameters `ITAUMAX`, `JUMAX`, `UMAX`, `TAUWMAX` are in `tabu_stress_detail`, so the header and `srce_st3_calc_ustar.hpp` compile in one translation unit |
| C shim | none yet; `INSIN3`'s port calls it with W3GDATMD's `ZZWND`, `AALPHA`, `ZZ0MAX` |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src3md.F90` (WW3's CMake Release flags, gfortran 16.2) under
`ST3 STAB0 FLX0` and `ST3 STAB3 FLX0`, both recompiled object identical to the CMake build's (v);
the two `TABU_STRESS` disassemblies are identical, so the `STAB` choice does not reach it (v).
Compared with the GIMPLE and the object of a throwaway C++ unit under the `openmp-release` flags
(g++ 16.2, `-ffp-contract=off`), operation by operation, 2026-10-16.

- (v) Folded constants written as hex floats with a `static_assert` of how they are built:
  `DELU = 0.25`, `DELTAUW = 0x1.6e5cd4p-7` (`2.2361/200` in REAL), `WCD = SQRT(0.0012875) =
  0x1.25f17cp-5`, `XM+1 = 1.5`. The same constants appear in both GIMPLE dumps.
- (v) `(1.-X)**XM` with REAL `XM = 0.5` is a `powf` call in both objects, not `sqrtf`;
  `ALOG(ZZWND/ZZ0)` is one `logf` for `F` and `DELF`; `**2` and `**2.` are `x*x`. No libmvec call.
- (v) `DELF` has the same statement sequence in both dumps: `KAPPA*UTOP / LG**2`, `*2`, `/UST`,
  `*(1-1.5X)`, `/(1-X)`. Both objects emit `*2` as `x+x`.
- (v) `MAX(USTOLD**2, ZTAUW+EPS1)`, `MAX(UST**2., ZTAUW+EPS1)` and `MIN(ZZ00, ZZ0MAX)` are
  `maxss`/`minss` with the square, and `ZZ00`, as destination in both objects; the header's
  `maxss`/`minss` reproduce that rule (a NaN or a tie gives the second operand).
- (v) No FMA, no reduction, no vectorised loop in either object. The Fortran precomputes the first
  `SQRT` and `powf` of each `(I,J)` before the Newton loop and folds the `(I,J) = (0,0)` start
  (`SQRT(EPS1)`, `powf(1,0.5) = 1`, both exact or correctly rounded); g++ schedules differently,
  with the same operations on the same values.
- ⚠ CUDA compiled (`nvcc_wrapper`, `sm_89`, `--fmad=false`); the function is host-only, so no
  device code was generated, and it was never run.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3src3md.F90` under `ST3 STAB0`
  and `ST3 STAB3` (object identical to the CMake build's, `TABU_STRESS` identical between the two)
  and the header under `openmp-release`, and compared the disassemblies instruction by instruction:
  the same `.rodata` bits (`0x3e800000`, `0x3c372e6a`, `0x3d12f8be`, `0x3727c5ac`, `0x3ecccccd`,
  `0x411ce560`, `0x3fc00000`, `0x3f000000`), `powf`/`logf`/`sqrtss`, the same `minss`/`maxss`
  operand roles, the `ZZ0MAX` test taken on NaN in both. Fixed: the header's module parameters
  sat in `ww::srce::st3` and made `kItauMax`, `kJuMax`, `kTauwMax` ambiguous inside
  `calc_ustar` when both headers were included (a compile error); moved into
  `tabu_stress_detail`, and a unit including every `srce_st3_*.hpp` now compiles warning-free
  (g++ 16.2, `openmp-release`) and for CUDA (`nvcc_wrapper` of the `#cuda` shell, `sm_89`,
  `--fmad=false`, compile only); the C++ assembly of `tabu_stress` is unchanged by the move.
  Line cites of `DELF` (`:926-927`) and the zero-wind loop (`:937-941`) corrected.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
