# RUNRC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Reverse-communication driver of a SPARSKIT iterative solver: it copies the initial guess into the
solution, then calls the solver until it stops, and each time the solver returns a request in
`IPAR(1)` it applies `A x` (`AMUX`), `A^T x` (`ATMUX`) or the ILU preconditioner (`LUSOL`,
`LUTSOL`) to the work array slices `WK(IPAR(8))` and `WK(IPAR(9))`. In WW3 it solves the linear
system of the implicit N scheme on unstructured grids with `BCGSTAB`.

## What was ported

| | |
|---|---|
| WW3 routine | `runrc`, `model/src/w3profsmd.F90:3801-3886` at NOAA-EMC/WW3 `03adda91`, 86 lines; no `#ifdef` inside it |
| Callers | only `W3XYPFSNIMP` (`w3profsmd.F90:1239`), with `BCGSTAB` as the solver and `IPAR(2) = 1`, right preconditioning (`:1205`); `W3XYPFSNIMP` is selected by `FSNIMP` in `W3XYPUG` (`:257`), and `FSNIMP = IMPFSN` (`w3gridmd.F90:3725`) |
| Switches and regtests | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`). `ww3_ts4` with `input_ug` sets `IMPFSN = T` (`regtests/ww3_ts4/input_ug/namelists_Mediterranean.nml:1`, read through `ww3_grid.nml:40`; switch `PR3`, no `PDLIB`), and `matrix.base` runs it serial and under MPI (`regtests/bin/matrix.base:2091`, `:2095`). So this path is reached by a regtest ⚠ (read from the inputs, not run) |
| Kokkos code | [`../src/ww_kokkos/profs_runrc.hpp`](../src/ww_kokkos/profs_runrc.hpp), `ww::profs::runrc`, an `inline` host function template; `double` as `REAL*8`, arrays 0-based holding the Fortran's 1-based indices |
| Calls | `amux`, `atmux`, `lusol`, `lutsol` from [`profs_amux.hpp`](../src/ww_kokkos/profs_amux.hpp), [`profs_atmux.hpp`](../src/ww_kokkos/profs_atmux.hpp), [`profs_lusol.hpp`](../src/ww_kokkos/profs_lusol.hpp), [`profs_lutsol.hpp`](../src/ww_kokkos/profs_lutsol.hpp) |
| The `EXTERNAL` solver | a template parameter, any callable `solver(int n, const double* rhs, double* sol, int* ipar, double* fpar, double* w)`; `BCGSTAB` (`:2049-2370`) is not translated, so the caller supplies it |
| I/O | the five `WRITE(*,*)` messages (`:3837`, `:3873`, `:3875-3877`, `:3879`, `:3881`) stay on the host: `runrc` is a host function and writes them with `std::printf` in gfortran's list-directed layout (leading blank, `INTEGER(4)` in 12 columns, one blank between items), checked against the pinned gfortran's output for 5, 123456 and -2147483647 |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object (`objdump -d`), against a throwaway C++ unit (an `extern` solver, so the call stays opaque)
with the openmp-release flags and `-ffp-contract=off`, compiled warning-free, 2026-10-16.

- (v) `runrc` itself does no floating-point arithmetic: the copy `sol = guess` (vectorised in both
  objects, moves only), `ipar(1) = 0`, and the dispatch on `ipar(1)`. The GIMPLE dispatch is a
  switch on 1..6 with a default that tests `<= 0`; `ipar(1) > 6` (for instance `BCGSTAB`'s 10)
  calls the solver again in both.
- (v) The floating-point work is in the callees. The Fortran object inlines `AMUX` and `ATMUX`
  (`mulsd addsd` each) and calls `LUSOL` and `LUTSOL` out of line; the C++ unit inlines all four.
  Inlining moves no operation: the double-precision sequence of each inlined body is the one its
  own report compared (`mulsd subsd` for the sweeps, `mulsd` for the diagonal), no FMA, no libm
  call. The only loops the C++ vectorised (the copy, `ATMUX`'s zeroing, `LUTSOL`'s copy) move or
  store values and do no arithmetic.
- ⚠ Inherited from `ATMUX`: inside `runrc` the Fortran object's `ATMUX` multiply has `a(k)` as
  the first `mulsd` source and the C++ one `x(i)` (as in [`ATMUX_KOKKOS.md`](ATMUX_KOKKOS.md)), so
  when both are NaN with different payloads the propagated payload differs. `AMUX`, `LUSOL` and
  `LUTSOL` keep the Fortran's first source at every `mulsd` and `addsd`/`subsd` once inlined.
- (v) `W_2D(1:n,1:8) => WK(1:n*8)` is passed through `_gfortran_internal_pack`
  (`:3852`); the descriptor is contiguous, so the pack returns `wk` itself and no copy or unpack
  happens. The C++ passes `wk`.
- (v) `ITS` (`SAVE`, `:3828`, `:3832`) is zeroed, compared with `ipar(7)` and assigned, never read
  for any result or output; it is not kept.
- ⚠ `n` is passed by value. The Fortran passes it by reference and the object rereads `*n` after
  each solver call; only a solver that changed `n` would see a difference, and `BCGSTAB` does not.
- ⚠ Output order: `std::printf` writes through C stdio, the Fortran through libgfortran's unit 6.
  When both write to the same stream, lines may interleave differently from the all-Fortran run.
  The text of each line is the same.
- ⚠ CUDA not compiled; `runrc` is host-only by design (it calls a host solver and prints).
- (v) Fresh review 2026-10-16: no defect for non-NaN input. The `w3profsmd.F90` object was rebuilt
  from `build_lab`'s flags (identical to the CMake object) and its `runrc` GIMPLE and assembly read
  against a recompiled C++ unit: the `ipar(2) > 2` exit, the `sol = guess` copy, the dispatch on
  1..6 and `<= 0`, the inlined `AMUX`/`ATMUX` and the out-of-line `LUSOL`/`LUTSOL` operation by
  operation (operands of each `mulsd`, `addsd`, `subsd`), and the five messages against the pinned
  gfortran's list-directed output. Only the `ATMUX` NaN-payload ⚠ above differs; nothing was
  changed in the header.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. `ww3_ts4` with `input_ug` (serial and MPI entries of `matrix.base`)
sets `IMPFSN = T` and should reach this routine through `W3XYPFSNIMP` ⚠ (read from the inputs,
not run). That entry has `NL0`, not `NL1`, so it is outside the current `NL1` matrix.
