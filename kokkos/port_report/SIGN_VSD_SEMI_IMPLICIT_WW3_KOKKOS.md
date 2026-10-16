# SIGN_VSD_SEMI_IMPLICIT_WW3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Clips the diagonal part of the source term, `VD = MIN(0., VD)` over the spectrum, before an
implicit integration (`w3srcemd.F90:2630-2632`). `SPEC` and `VS` are dummies it never reads or
writes.

## What was ported

| | |
|---|---|
| WW3 routine | `SIGN_VSD_SEMI_IMPLICIT_WW3`, `model/src/w3srcemd.F90:2559-2633` at NOAA-EMC/WW3 `03adda91`, 75 lines; authors A. Roland, M. Dutour-Sikiric. Its only `#ifdef` block is `W3_S` (trace) |
| Callers | `W3SRCE`, `w3srcemd.F90:1721` and `:1723` (`optionCall` 2 and 3), both under `IF (.not. LSLOC)`; `LSLOC` is `LOGICAL, PARAMETER :: LSLOC = .true.` (`w3parall.F90:89`, outside any `#ifdef`). The calls are dead in every build: the `-O3` object of `W3SRCE` contains no call and no inlined copy (v). Every switch compiles the routine (it is in `w3srcemd`), none runs it. Translated for completeness; it can never be exercised |
| Kokkos code | [`../src/ww_kokkos/srce_sign_vsd_semi_implicit_ww3.hpp`](../src/ww_kokkos/srce_sign_vsd_semi_implicit_ww3.hpp), `ww::srce::sign_vsd_semi_implicit_ww3(spec, vs, vd, nspec)`, arrays 0-based |
| Existing W3SRCE kernel | `srce_kernel.hpp` has no translation of it: it ports the `srce_direct` path of `switch_lab_shrd`, where this branch is dead |
| C shim | none |
| Plan | issue #42, batch of the easy routines left in Fortran |

## Arithmetic

Reference: `w3srcemd.F90.o` of WW3's CMake Release build under `switch_lab_shrd` (gfortran
16.2.0, pinned shell); the module recompiled with `-fdump-tree-optimized-lineno` gives an object
identical to the CMake one (v). The C++ unit compiled with the `openmp-release` flags,
warning-free (v), 2026-10-16.

- (v) No arithmetic other than `MIN(0., VD(IS))`; no libm call, no reduction, no FMA possible.
- (v) MIN operand order, read from the object: a 4-wide `minps` body with `VD` as destination
  (`VD < 0 ? VD : 0`), then an unrolled tail of up to three elements where the first two keep that
  order (`minss %xmm0,%xmm1`) and the third swaps it (`minss (mem),%xmm0`: `0 < VD ? 0 : VD`);
  for `NSPEC <= 3` (or `NSPEC = 2**31-1`) a scalar loop with the swapped order. The two orders
  differ only for a NaN or a `-0.` in `VD` (`+0.` against `VD` itself). The C++ reproduces the
  order per index with two ternaries; its object (AVX2, 8-wide body) computes the same order at
  each index: `vminps %ymm1,%ymm0` and `vcmpnltss`+`vandnps` for the first form, `vminss (mem),%xmm0`
  for the second (v).
- (v) No arithmetic `#ifdef` variant: the body does not depend on the switch.
- ⚠ Not compiled for CUDA. On a device the two ternaries are exact as written.
- (v) Fresh review 2026-10-16: no defect. `w3srcemd.F90` recompiled from `build_lab`'s
  `flags.make` gave an object identical to the CMake one; the routine's assembly and the C++
  unit's (`openmp-release` flags) were compared index by index: the scalar path for `NSPEC` 1 to 3
  and `2**31-1`, the vector body, and the three tail elements keep the same `MIN` operand order,
  so NaN and `-0.` give the same bits. No call to the routine in the object. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can reach it while `LSLOC` is `.true.`.
