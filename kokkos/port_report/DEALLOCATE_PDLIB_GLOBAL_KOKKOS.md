# DEALLOCATE_PDLIB_GLOBAL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Releases nineteen unstructured-grid arrays of `GRIDS(IMOD)` (`SI`, `TRIA`, `IEN`, the cell and
sparse-matrix index arrays, `IOBDP`, `IOBPA`, ...). `W3INIT` calls it once per grid under
`W3_PDLIB`, after `W3IOPP` (`model/src/w3initmd.F90:1364-1366`). No arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `DEALLOCATE_PDLIB_GLOBAL(IMOD)`, `model/src/w3profsmd_pdlib.F90:7345-7441` at h0ffmann/WW3 `8e433f99`, the same lines at NOAA-EMC/WW3 `03adda91` |
| Switch | `WW3/regtests/ww3_tp2.17/input/switch_PDLIB`, the switch of the profile `docs/data/ww3_pdlib_gprof.tsv` (self time 0.00 s there) |
| Kokkos code | [`../src/ww_kokkos/pdlib_deallocate_pdlib_global.hpp`](../src/ww_kokkos/pdlib_deallocate_pdlib_global.hpp), `deallocate_pdlib_global<MemSpace>`; host only, no kernel |
| Arrays | [`../src/ww_kokkos/pdlib_deallocate_pdlib_global_arrays.hpp`](../src/ww_kokkos/pdlib_deallocate_pdlib_global_arrays.hpp), `PdlibGlobalArrays<MemSpace>`: a minimal mirror of the nineteen `GRIDS(IMOD)` components. The same arrays sit in `IogrState` (`iogr_state.hpp`) and partly in `PdlibGeometry` (`pdlib_params.hpp`); the shim must unify the three into one per-grid context |
| Not ported | the `W3_S` branch (`USE W3SERVMD` and `IENT`, `:7390-7392`, `:7407-7409`) and the `W3_DEBUGINIT` `USE` of `IAPROC` (`:7396-7398`): neither is in `switch_PDLIB`, and neither has an executable statement. The `USE`s of `pdlib_abort` and `IMEM` (`:7394-7395`) are never referenced |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## What the Fortran does

Reference: the `-O3` GIMPLE (`-fdump-tree-optimized`) of `w3profsmd_pdlib.F90` with the flags and
defines of WW3's CMake Release build for `switch_PDLIB` (gfortran 16.2.0, pinned `ww3` shell),
2026-10-16; the routine's disassembly in that object and in the CMake build's own object are the
same instruction for instruction.

- (v) One `DEALLOCATE` of nineteen `POINTER` components (`model/src/w3gdatmd.F90:765-778`), no
  `STAT=`: for each item in list order the object loads `GRIDS(IMOD)%X%data`; if it is `NULL` it
  calls `_gfortran_runtime_error_at` with "Attempt to DEALLOCATE unallocated 'grids'", which
  terminates the program; else it frees the block and stores `NULL` in the descriptor's data
  field. 19 such tests, in the order `SI, TRIA, CROSSDIFF, IEN, LEN, ANGLE, ANGLE0, CCON, COUNTCON,
  INDEX_CELL, IE_CELL, POS_CELL, IAA, JAA, POSI, I_DIAG, JA_IE, IOBDP, IOBPA` (`:7417-7437`).
- (v) The reported line is the statement's (`7415`) for `SI`, then each item's own line
  (`7418`-`7433`, `7436`, `7437`). The C++ returns the same line.
- (v) Kept, because they are commented out of the list: `TRIGP` (`:7416`), `IOBP`, `IOBPD`
  (`:7434-7435`); also not in the list: `XGRD`, `YGRD`, `ZB` (allocated with them in `W3DIMUG`,
  `w3gdatmd.F90:3235-3260`).
- (v) Only the data field is reset. `GUGINIT` stays `.TRUE.` (`w3gdatmd.F90:3285`), and the
  `W3SETG` aliases (`SI => GRIDS(IMOD)%SI`, `w3gdatmd.F90:2401`, and the others) are not touched:
  they dangle.
- (v) An empty array is still associated: gfortran's `ALLOCATE` of a pointer of extent 0 calls
  `malloc(1)` (GIMPLE of a three-line test subroutine, `allocate(p(n))`, `-O3`).

## Translation

- (v) `release(view)` per item, in the Fortran's order: test, then `view = {}`; on a
  disassociated item the routine returns at once. The C++ object at `-O3` carries the nineteen
  line constants (`7415`, `7418`-`7433`, `7436`, `7437`; `objdump` of the instantiation TU).
- (v) "Associated" is `data() != nullptr || use_count() > 0`. Kokkos 5.2.2 (OpenMP build,
  `HostSpace`), in a probe that calls only Kokkos: a default view has `data=(nil) use_count=0`;
  `View("e", 0)` a non-null `data` and `use_count=1`, so an empty view the port allocates counts
  as associated, as gfortran's `malloc(1)` does; after `= {}`, `data=(nil) use_count=0`. An
  unmanaged view of a `malloc(1)` block has a non-null `data` and `use_count=0` (Kokkos keeps no
  record for it, `impl/Kokkos_SharedAlloc.hpp:603-608`), and one of a `NULL` pointer
  `data=(nil) use_count=0`: the test also holds for a view a shim binds to Fortran memory.
- (v) A disassociated item ends the routine with `{exit_code = 2, line}`, after the items before
  it were released, the state in which the Fortran stops. 2 is the exit status of a gfortran
  16.2.0 program after "Attempt to DEALLOCATE unallocated" (a probe that deallocates a
  disassociated pointer component; the run time printed "At line 13 of file p.f90", the second
  item's own line, and exited 2). As in W3DIMS and W3IOGR, the routine returns the code and the
  shim writes "At line N of file ...w3profsmd_pdlib.F90" (gfortran prints the path it was
  given) and "Fortran runtime error: Attempt to DEALLOCATE unallocated 'grids'", then stops.
- ⚠ `view = {}` frees when the last View holding the allocation lets go; the Fortran frees at once
  and leaves every alias dangling. They agree wherever the Fortran is defined (reading a dangling
  alias is undefined), but memory is returned later if another View copy is alive. Not checked
  against a caller, since none exists.
- ⚠ An unmanaged View bound by a shim to `GRIDS(IMOD)` memory frees nothing on `= {}`, and the
  Fortran descriptor keeps its data pointer: with such a binding the release has to stay the
  Fortran `DEALLOCATE`. Which binding the shim uses is not decided (no shim yet).
- ⚠ On `CudaSpace` the release goes through Kokkos' deallocation, whose fencing was not read.
  The header compiles for CUDA (below); nothing was run on a GPU.
- ⚠ Static parity only: nothing of the port was run, and no regtest reaches it yet.

## Compile check

Pinned `ww3` shell (`nix develop "path:<worktree>/picasso/labs/pratico#ww3"`, g++ 16.2.0, Kokkos
5.2.2 OpenMP), a TU that includes the header and explicitly instantiates `PdlibGlobalArrays<M>`
and `deallocate_pdlib_global<M>`:
`g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp -c`
with `M = Kokkos::HostSpace` and with `M = Kokkos::DefaultExecutionSpace::memory_space`: 0
warnings; also 0 with `-Wconversion -Wshadow -Werror` on `HostSpace` (2026-10-16). In the
`#cuda` shell (Kokkos 5.2.2 CUDA, nvcc 12.9.86): `nvcc_wrapper -O3 -std=c++20 -Wall -Wextra -Wpedantic
-ffp-contract=off -fopenmp -arch=sm_89 --fmad=false -extended-lambda -expt-relaxed-constexpr -x
cu -c` with both `M`, where the default memory space is `Kokkos::CudaSpace` (a `static_assert`):
0 warnings, compile only.

- (v) Fresh review 2026-10-16: an independent WW3 CMake Release build of `switch_PDLIB`
  (gfortran 16.2.0) and the `-O3` GIMPLE of `w3profsmd_pdlib.F90` with its flags and defines:
  the nineteen items, their order, the `NULL` test, `free` and `data = NULL` per item and every
  reported line match; `TRIGP`, `IOBP`, `IOBPD`, `GUGINIT` and the `W3SETG` aliases are not
  touched; `w3initmd.F90:1364-1366` is the only caller. Extents and kinds of the nineteen views
  match `w3gdatmd.F90:765-778`, `W3DIMUG` (`:3235-3260`) and `AREA_SI` (`w3triamd.F90:1505-1507`);
  every line citation checked. `malloc(1)` for an empty `ALLOCATE` and the predicate on owned
  and unmanaged views checked by probes (above). One defect fixed: the routine threw
  `std::runtime_error` where the repository's convention (W3DIMS, W3DIMA, W3IOGR) returns the
  code the program would exit with; it now returns `DeallocatePdlibGlobalStatus` (`2`, line).
  Documented the unmanaged-view case. CUDA compile added.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing of the port was run.
