# LUTSOL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Solves `(LU)^T x = y` for an incomplete LU factor stored in modified sparse row form, the
transposed preconditioner step of the SPARSKIT iterative solvers that `W3PROFSMD` carries for its
implicit unstructured-grid N scheme.

## What was ported

| | |
|---|---|
| WW3 routine | `LUTSOL`, `model/src/w3profsmd.F90:3701-3736` at NOAA-EMC/WW3 `03adda91`. No `#ifdef` inside it. `w3profsmd.F90` is compiled by any `PR1`, `PR2` or `PR3` switch (`model/src/cmake/switches.json:117-125`) |
| Caller | only `RUNRC` (`w3profsmd.F90:3867`), when the solver returns `IPAR(1) = 4` or `6`. `RUNRC` is called only from `W3XYPFSNIMP` (`:1239`) with `BCGSTAB`, and `BCGSTAB` (`:2049-2370`) sets `IPAR(1)` to 0, 1, 3, 5 or 10, never 4 or 6. `LUTSOL` is therefore never executed by WW3; it is translated for completeness. The implicit N scheme itself (`IMPFSN = T`) appears in `regtests/ww3_ts4/input_ug` |
| Kokkos code | [`../src/ww_kokkos/profs_lutsol.hpp`](../src/ww_kokkos/profs_lutsol.hpp), `ww::profs::lutsol`, one thread: both sweeps carry a dependence through `x` |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd`), rebuilt by the translator and identical to the CMake object
(`objdump -d`), read against the GIMPLE and object of a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) `REAL*8` throughout, kept as `double`. Each update is one `mulsd` (`ALU(K)*X(I)`) and one
  `subsd` (`X(JLU(K)) - product`), `X(I)*ALU(I)` one `mulsd`, in both objects; no FMA, no libm
  call, no reduction.
- (v) The object reloads `X(I)` on every `K` (`JLU(K)` may equal `I`); the C++ does the same, since
  `x` is written through a pointer that may alias it. The `DO` bounds `JU(I)`, `JLU(I+1)-1` are read
  once before each inner loop in both.
- (v) The copy `X = Y` is vectorised in both objects (2 doubles in the Fortran, 4 with a runtime
  overlap check in the C++); a copy, so no bits change.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3profsmd.F90` recompiled from `build_lab`'s flags, identical to the CMake object; its `lutsol` read against the C++ unit instruction by instruction: same `mulsd` operand order at every site (first `K` `X(I)*ALU(K)` from the register, later `K` `ALU(K)*X(I)` reloaded), same `X(JLU(K)) - product`, bounds read once, no FMA, no call, no `#ifdef` in `:3701-3736`; the caller chain (`:1239`, `:3867`, `IPAR(1)` values set in `:2049-2370`) checked. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: WW3 never calls it (see Caller).
