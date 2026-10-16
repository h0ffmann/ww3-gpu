# AMUXD_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `y = A x` for a `REAL*8` matrix in diagonal (DIA) form: column `j` of `diag(ndiag,idiag)`
holds the diagonal at offset `ioff(j)`, so `diag(k,j)` is `A(k, k+ioff(j))`. A SPARSKIT routine
(Y. Saad) kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `amuxd`, `model/src/w3profsmd.F90:3095-3146` at NOAA-EMC/WW3 `03adda91`, 52 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw amuxd -- model/src` finds only the `PUBLIC` list (`:62`), the index comment (`:2838`) and the routine. Translated for completeness; no WW3 run can exercise it |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_amuxd.hpp`](../src/ww_kokkos/profs_amuxd.hpp), `ww::profs::amuxd`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based, `diag` column-major |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Zeroing loop (`:3131-3133`): both objects store `+0.0`; the Fortran one is vectorised by 2.
- (v) Bounds (`:3136-3137`): `MAX0(1,1-io)` and `MIN0(n,n-io)` are 32-bit integer `MAX_EXPR`/`MIN_EXPR`
  in the GIMPLE; the C++ computes the same values (`cmov`). Integer, so operand order is irrelevant.
- (v) Product loop (`:3138-3140`): the Fortran object vectorises the `k` loop by 2 with unroll 4
  (`mulpd`, `addpd`, gathered `x(k+io)`) and a scalar tail of up to three steps; the C++ object
  keeps it scalar (`vmulsd`, `vaddsd`). Each `y(k) = y(k) + diag(k,j)*x(k+io)` is one rounded
  product and one rounded sum per element, in the order `j = 1..idiag`, so the bits match for
  every input. No FMA, no libm call, no reduction reordered.
- (v) Indices: `k+io` is a 32-bit sum sign-extended to 64 bits, and the leading dimension is
  `MAX(ndiag,0)` multiplied by `j-1` in 64 bits, in both objects.
- (v) Operand order: in the Fortran vector body and every scalar tail step, and in the C++ loop,
  the register that receives the product is loaded with `x(k+io)`, multiplied by `diag(k,j)`, then
  `y(k)` is added to it (`mulpd`/`mulsd`, `addpd`/`addsd`; `vmulsd`, `vaddsd`). The first source
  operand is the same at every site, so even the payload of a NaN when both operands are NaN is
  the same.
- ⚠ `1-io` and `n-io` overflow for `io` near the 32-bit limits in both languages (undefined
  behaviour in each); no DIA matrix of WW3's size reaches it.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect in the header. Recompiled `w3profsmd.F90` under
  `switch_lab_shrd` (identical to the CMake object) and the C++ unit with the openmp-release flags;
  compared GIMPLE and assembly: zeroing, `MAX0`/`MIN0` bounds, 64-bit leading dimension and
  `k+io` index, product and sum operand order per site, no FMA, no libm call. Fixed in this
  report only: the NaN-payload bullet, which described the Fortran's operand order wrongly and is
  now (v).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `amuxd`, so the WW3 regtests cannot exercise it.
