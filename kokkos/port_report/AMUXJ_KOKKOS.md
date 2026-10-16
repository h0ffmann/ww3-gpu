# AMUXJ_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes `y = A x` for a `REAL*8` matrix in jagged-diagonal (JAD) form: jagged diagonal `ii`
occupies `a(ia(ii):ia(ii+1)-1)`, with column numbers in `ja` at the same positions, and adds into
`y(1:ilen)`. The JAD row permutation is not applied (the Fortran leaves it to `permvec`). A
SPARSKIT routine (Y. Saad) kept in `w3profsmd.F90`.

## What was ported

| | |
|---|---|
| WW3 routine | `amuxj`, `model/src/w3profsmd.F90:3148-3201` at NOAA-EMC/WW3 `03adda91`, 54 lines; no `#ifdef` inside it |
| Callers | none: `git -C WW3-noaa grep -nw amuxj -- model/src` finds only the `PUBLIC` list (`:63`), the index comment (`:2839`) and the routine. Translated for completeness; no WW3 run can exercise it |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json`), so by nearly every switch file, `switch_lab_shrd` included |
| Kokkos code | [`../src/ww_kokkos/profs_amuxj.hpp`](../src/ww_kokkos/profs_amuxj.hpp), `ww::profs::amuxj`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based, `ja` and `ia` holding the Fortran 1-based positions |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Zeroing loop (`:3187-3189`): both objects store `+0.0`; the Fortran vectorises it by 2
  (16-byte stores), the C++ by 4 (32-byte stores). Same bits stored.
- (v) Product loop (`:3190-3196`): scalar in both objects. Each `y(j) = y(j) + a(k1+j)*x(ja(k1+j))`
  is one `mulsd` (`a*x`) and one `addsd` (`y + product`) per element in the GIMPLE of both, in the
  order `ii = 1..jdiag`, `j = 1..ilen`. No FMA, no libm call, no reduction reordered.
- (v) Index arithmetic: gfortran folds `ilen = ia(ii+1)-k1-1` to `ia(ii+1)-ia(ii)` and forms
  `k1+j` in 32-bit wrapping arithmetic before widening; g++ does the same fold and the same 32-bit
  sum. They agree for every index that is in bounds.
- (v) The Fortran object reuses `ia(ii+1)` as the next iteration's `ia(ii)`; the C++ reloads it.
  Same value, since `ia` is not written.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The `w3profsmd.F90` object was recompiled from
  `build_lab`'s flags and is identical to the CMake object; its `amuxj` disassembly was read against a
  new C++ unit's, instruction by instruction: same `+0.0` stores, the same `x*a` then `+y` per element
  in the same loop order, no FMA (no `-march` on the Fortran side, `-ffp-contract=off` on the C++),
  the same 32-bit index sums. The routine has no `#ifdef`; line cites and the `PR1`/`PR2`/`PR3`
  switch claim were checked at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `amuxj`, so the WW3 regtests cannot exercise it.
