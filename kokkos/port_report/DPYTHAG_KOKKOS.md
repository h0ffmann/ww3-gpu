# DPYTHAG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`sqrt(a**2+b**2)` computed without overflow, in double precision (the Numerical Recipes
`pythag`), used by the singular value decomposition of the tidal analysis.

## What was ported

| | |
|---|---|
| WW3 routine | `DPYTHAG`, `model/src/w3tidemd.F90:976-991` at NOAA-EMC/WW3 `03adda91` (16 lines). Called only by `dsvdcmp` (`:1175`, `:1208`, `:1218`, `:1232`), which `svd` calls (`:1391`), which `flex_tidana_webpage` calls (`:2308`), which the preprocessor `ww3_prnc` calls (`ww3_prnc.F90:1675`). No `#ifdef` in the routine; `w3tidemd` is compiled when the switch has `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`), `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` ⚠ whether `ww3_tp2.18` reaches the tidal analysis was not checked |
| Kokkos code | [`../src/ww_kokkos/tide_dpythag.hpp`](../src/ww_kokkos/tide_dpythag.hpp), `ww::tide::dpythag` |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the preprocessed source is the same in both, so the routine has one
variant. Compared with a C++ unit built with the `openmp-release` flags, 2026-10-16.

- (v) Same operations in both objects: `|a|`, `|b|` by `andpd`; `comisd` for `absa > absb`; the
  zero test on `b` itself (GIMPLE `_2 == 0.0`), as in the C++; `(x)**2` is one `mulsd`; `+1.0`,
  `sqrtsd` inline, one final `mulsd`. No FMA, no libm call, no reciprocal.
- (v) The quotient is `|b|/|a|` (or `|a|/|b|`) in both objects, operand for operand
  (`divsd` destination, then `sqrt(...)*abs` with the square root as first operand), so a NaN
  input returns the same NaN, sign included. gfortran keeps the Fortran parentheses as a
  `PAREN_EXPR`; without one g++ drops both `fabs` because the quotient is squared and divides the
  raw `b/a`, which flips the sign of the NaN returned for a negative NaN input. The header puts
  the quotient through `__builtin_assoc_barrier` (`dpythag_paren`), the same `PAREN_EXPR`.
- ⚠ A compiler without `__builtin_assoc_barrier` (the guard falls back to the plain quotient)
  may again divide the raw values: same bits for every non-NaN input, sign of a NaN result may
  differ.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: one defect, fixed. Recompiled `w3tidemd.F90` from the `build_mixa`
  and `build_lab` flags (identical to both CMake objects) and compared `dpythag` with a C++ unit
  (`openmp-release` flags, warning-free) in GIMPLE and assembly: the C++ divided `b/a` on the raw
  values (NaN sign differs for a negative NaN input); with `dpythag_paren` it divides `|b|/|a|`
  and the two objects now match operation for operation, operand order included.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
