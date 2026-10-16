# SIGN_VSD_PATANKAR_WW3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Clips the linearised source terms of one spectrum Patankar style, `VD = MIN(0,VD)` and
`VS = MAX(0,VS)`, before the implicit unstructured-grid (PDLIB) solver; the Fortran calls it
experimental.

## What was ported

| | |
|---|---|
| WW3 routine | `SIGN_VSD_PATANKAR_WW3`, `model/src/w3srcemd.F90:2647-2722` at NOAA-EMC/WW3 `03adda91`; authors A. Roland, M. Dutour-Sikiric. Its only `#ifdef` blocks are `W3_S` tracing |
| Caller | `W3SRCE`, `w3srcemd.F90:1718-1719`, inside `#ifdef W3_PDLIB` (`:1599`) and only when `optionCall == 1`. `optionCall` is initialised to 3 (`w3gdatmd.F90:1113`) and assigned nowhere in `model/src` at the pin (`git grep -n optionCall`), so the call is never reached ⚠: translated for completeness |
| Switches that compile the call | `PDLIB`: 23 switch files (`git grep -lw PDLIB -- '*switch*'`), among them `ww3_tp2.6`, `ww3_tp2.17`, `ww3_tp2.19`, `ww3_tp2.21`, `ww3_ufs1.1/input_unstr`, and the operational `model/bin/switch_NWS_rwps`. The routine itself is compiled by every switch |
| Kokkos code | [`../src/ww_kokkos/srce_sign_vsd_patankar_ww3.hpp`](../src/ww_kokkos/srce_sign_vsd_patankar_ww3.hpp), `ww::srce::sign_vsd_patankar_ww3`; `SPEC` is kept in the signature and unused, as in the Fortran |
| C shim | none yet |
| Plan | issue #42, batch goal/42-port-batch-easy |

## Arithmetic

Reference: the `-O3` object of `w3srcemd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd`), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the
CMake object, 2026-10-16.

- (v) `MIN(0.,VD)` and `MAX(0.,VS)` become `MIN_EXPR`/`MAX_EXPR`, whose result for a NaN or a
  signed zero x86 decides by operand order, and the object uses three orders:
  - `NSPEC == 1` or `2147483647`: a scalar loop with 0 as the destination of `minss` and
    `maxss`, so `VD = 0<VD ? 0 : VD`, `VS = 0>VS ? 0 : VS` (a NaN passes through).
  - Otherwise a 4-wide `minps`/`maxps` body and a 2-wide epilogue with the value as
    destination, `VD = VD<0 ? VD : 0`, `VS = VS>0 ? VS : 0` (a NaN or a -0 becomes +0).
  - When `NSPEC` is odd (and not 1), the last element goes through a scalar tail:
    `VD = VD<0 ? VD : 0` but `VS = 0>VS ? 0 : VS`.

  The C++ spells each of these as an explicit ternary per element, so the bits do not depend
  on how g++ vectorises.
- (v) Throwaway unit compiled with the openmp-release flags (`-O3 -march=x86-64-v3
  -ffp-contract=off`), warning-free; its object read instruction by instruction: 8- and 4-wide
  `vminps`/`vmaxps` with the value as first source, `vminss`/`vcmpnltss`+`vandnps`/`vandps`
  selects in the scalar paths, each giving the same result as the Fortran's instruction for that
  element, NaN and -0 included.
- (v) No arithmetic, no libm or libmvec call, no reduction.
- ⚠ CUDA not compiled. The ternaries do not call `fminf`/`fmaxf`.
- (v) Fresh review 2026-10-16: no defect. `w3srcemd.F90` recompiled from `build_lab`'s flags (object identical to the CMake one); every `minss`/`maxss`/`minps`/`maxps` site of `SIGN_VSD_PATANKAR_WW3` (scalar loop for `NSPEC` 1 or 2147483647, 4-wide body, 2-wide epilogue, odd-`NSPEC` scalar tail) mapped to its element range and operand order, and each matched against the throwaway unit's `vminps`/`vmaxps`/`vcmpnltss`+`vandnps`/`vandps` instructions for the same elements, NaN and -0 included. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can reach it while `optionCall` stays 3.
