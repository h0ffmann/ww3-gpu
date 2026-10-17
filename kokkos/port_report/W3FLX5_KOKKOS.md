# W3FLX5_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1358` behind `WW_KOKKOS_FLX5` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Friction velocity `UST`, drag coefficient `CD`, roughness length `Z0` and Charnock coefficient
`CHARN` from the total stress `TAUA` of an atmospheric model rather than from the wind alone:
`UST = max(1e-4, sqrt(TAUA/RHOAIR))`, `UNZ = max(0.01, U10)`, `CD = (UST/UNZ)^2`,
`Z0 = ZWND exp(-KAPPA min(UNZ/UST, 100))`, and for `UNZ > 2.5` m/s
`CHARN = (Z0 - 0.11 NU_AIR/UST) GRAV/UST^2` clipped to `[0.0095, 0.035]`, else 0.0095
(`w3flx5md.F90:236-249`). The stress direction `USTD` is `TAUADIR`; `U10D` is not read. Switch
`FLX5`; called once per sea point by `W3SRCE` (`w3srcemd.F90:1153-1154`) and, under ST4, by
`W3SPR4` in place of `CALC_USTAR` (`w3src4md.F90:358-359`); the point post-processors
`ww3_outp`, `ww3_ounp` and `gx_outp` call it too.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLX5`, `model/src/w3flx5md.F90:118-260` at NOAA-EMC/WW3 `03adda91`; compiled by the 4 regtest switches of `ww3_tp2.15` (`input_const` and `input_rho`, each `switch_ST4` and `switch_ST6`), by no `model/bin/switch_*`, no `switch_NCEP_*` or `switch_NWS_*` (`git grep -lw FLX5 -- '*switch*'`) |
| Variants | the routine's only `#ifdef` blocks are `W3_S` (subroutine tracing). Its ST4 and ST6 dependence lives in its callers: the objects of `build_st6f5` (ST6 FLX5) and `build_st4f5` (ST4 FLX5) have the same code and the same `.rodata.cst4`, so one function covers both |
| Kokkos code | [`../src/ww_kokkos/srce_flx5.hpp`](../src/ww_kokkos/srce_flx5.hpp), `ww::srce::flx::w3flx5_point`; returns 1 where the Fortran stops in `EXTCDE(1)` (`:228-231`), outputs not written |
| C shim | none yet; it writes the message of `:229` (format `:254-256`) on `IAPROC == NAPERR` and stops the run on 1 |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3flx5md.F90` (WW3's CMake Release flags, gfortran 16.2), recompiled
from both reference builds with `-fdump-tree-optimized-lineno` and found identical to each CMake
object; compared with a C++ unit under the `openmp-release` flags plus `-ffp-contract=off`,
2026-10-16.

- (v) Same operations in the same order: `|ZWND-10| > 0.01`; `TAUA/RHOAIR`, `sqrtss`, max with
  1E-4; max of `U10` with 0.01; `CD = (UST/UNZ)*(UST/UNZ)`; `UNZ/UST` min 100, `*0.4`, sign
  flip, `expf`, `*ZWND`; then `UNZ > 2.5`, `((Z0 - c/UST)*9.806)/(UST*UST)`. `**2` is one
  product, no `__powisf2`; `EXP` is glibc's `expf` in both; no libmvec, no FMA, no reduction.
- (v) `0.11*NU_AIR` is folded by gfortran to `0x35ceb1ff`; the header writes it as a hex float
  with a `static_assert` of `0.11f*1.4E-5f`. The ten single-precision constants have the same
  bits in `.rodata.cst4` of both objects, in the same order.
- (v) MIN/MAX per site from the object (`maxss`/`minss`, register operand first, so a NaN there
  yields the constant): `MAX(1E-4, SQRT(...))` (`:236`), `MAX(0.01, U10)` (`:237`),
  `MIN(UNZ/UST, 100)` (`:240`), `MAX(CHARN, 0.0095)` (`:244`), `MIN(0.035, CHARN)` (`:245`) are
  written as ternaries of that order. g++ turns the `:244-245` pair into `comiss`/`ja` and a
  `minss`, which gives the same value for every input, NaN included (0.0095).
- (v) A NaN `ZWND` passes the test of `:228` in both objects (`comiss` with `jbe`). g++ keeps a
  branch to `sqrtf` for a negative `TAUA/RHOAIR` (`-fmath-errno`); the NaN is replaced by 1E-4
  in both.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, `#cuda` shell), never run: the SASS holds `FFMA` from
  CUDA's own `expf`, `sqrtf` and division sequences, so the device bits depend on CUDA's math
  library, not glibc's.
- (v) Fresh review 2026-10-16: no defect in the header. Both reference objects recompiled from their
  `flags.make` and found identical to the CMake objects; the C++ unit recompiled (no warning) and read
  instruction by instruction against them: each constant load mapped through the relocations to the
  same `.rodata.cst4` offset and bits, the same operand order of every `divss`/`mulss`/`subss`, every
  `maxss`/`minss` with the same NaN result, `expf` and `sqrtss` as above; CUDA recompiled. Fixed in this
  report: the ST4 caller is `W3SPR4` at `w3src4md.F90:358-359` (it read `W3SIN4`, `:357-358`), and the
  format is `:254-256`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
