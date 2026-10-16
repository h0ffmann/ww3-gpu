# PDLIB_IOBP_INIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim that passes
`GRIDS(IMOD)`'s boundary arrays, the `W3GDATMD` module pointers, `iplg` and the
`SET_IOBPA_PDLIB` hook, and that acts on the returned code.

- (v) Fresh review 2026-10-16: an independent CMake Release object of `w3profsmd_pdlib.F90` under
  `switch_PDLIB` (disassembly identical to a `-fdump-tree-optimized` recompile with its
  `flags.make`) and its `-O3` GIMPLE; every `file:line` cited here and in the header re-read at
  `8e433f99`; the C++ recompiled on OpenMP, Serial and CUDA. Checked: the four `ALLOCATE`s
  (kinds, extents, order, codes 7/8/9/9 from the `.rodata` words of the four call sites, mapped
  to the malloc tests that jump to them), the IP loop (IP outer, section inner, length from
  `IOBPD`'s descriptor, destination from `IOBPD_loc`'s lower bound), the per-IP temporary as a
  compiler artefact, the `DEALLOCATE` order and the tail call. Defects fixed: (1) a failed
  `ALLOCATE` called a `pdlib_abort` hook and a failed `DEALLOCATE` called `Kokkos::abort`, so the
  host routine stopped the run; it now returns the code (7, 8, 9, 9, or 2 for the `DEALLOCATE`,
  line 619) and the shim prints and stops, as `BLOCK_SOLVER_INIT` and `DEALLOCATE_PDLIB_GLOBAL`
  do; the `pdlib_abort` hook is gone. (2) The second-call ⚠ said the Fortran stops in
  `DEALLOCATE`; with `npa >= 1` the loop reads through the NULL `IOBP` first.

On an unstructured grid WW3 keeps, per node, a boundary type `IOBP` (`INTEGER*2`) and a
per-direction flag `IOBPD(NTH,·)` (`INTEGER*1`), first over the global nodes (`W3DIMUG`,
`model/src/w3gdatmd.F90:3256-3257`). Under `PDLIB` each rank works on its `npa` local nodes
(own plus ghost), and `PDLIB_IOBP_INIT` builds the local copies: it allocates `IOBP_LOC(NPA)`,
`IOBPD_LOC(NTH,NPA)`, `IOBDP_LOC(NPA)` and `IOBPA_LOC(NPA)` in `GRIDS(IMOD)`, copies `IOBP` and
`IOBPD` of the global node `iplg(IP)` into local node `IP`, sets the wet/dry flag `IOBDP_LOC` to
0, releases the global `IOBP` and `IOBPD`, and calls `SET_IOBPA_PDLIB`. Integer copies only; no
floating-point operation.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_IOBP_INIT`, `model/src/w3profsmd_pdlib.F90:514-624` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), identical at NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 8e433f99 -- model/src/w3profsmd_pdlib.F90` is empty), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it. Its profile row is `docs/data/ww3_pdlib_gprof.tsv:96`, 0.00 s: no measured cost |
| Caller | `W3INIT`, `model/src/w3initmd.F90:923`, when `IAPROC .LE. NAPROC` and `GTYPE .EQ. UNGTYPE` (`:921`), after `BLOCK_SOLVER_INIT`; `W3INIT` then calls `SET_IOBPA_PDLIB` a second time (`:924`). The only call in `model/src` |
| Kokkos code | [`../src/ww_kokkos/pdlib_pdlib_iobp_init.hpp`](../src/ww_kokkos/pdlib_pdlib_iobp_init.hpp), `ww::pdlib::pdlib_iobp_init<ExecSpace>`, host function with one `parallel_for` |
| Not ported, by switch | `W3_S` (`USE STRACE`, `:559-561`; `IENT`, `:577-579`; `CALL STRACE`, `:586-588`) and `W3_DEBUGINIT` (the `WRITE`, `:589-591`); neither is in `switch_PDLIB` |
| Returned, not called | `PDLIB_ABORT(istat)` (`:597-603`; `PDLIB/yowfunction.F90:47-53`, which prints and calls yowerr's `abort`, ending in `STOP` at `PDLIB/yowerr.F90:159`): the routine returns istat (`kIobpInitAlloc*`), and the shim calls `PDLIB_ABORT` with it. The failed `DEALLOCATE` of `:619`: it returns `kIobpInitDeallocate` (2) |
| Not translated, behind `IobpInitHooks` | `SET_IOBPA_PDLIB` (`:620`; `:6978-7061`), which sets `IOBPA_LOC(JSEA)` from `MAPSTA(1,iplg(JSEA)) .EQ. 2` for `JSEA = 1..NSEAL` |
| State | `IobpGridArrays`: `GRIDS(IMOD)%IOBP`, `IOBPD` (with the association flags `DEALLOCATE` tests) and the four `*_LOC` (`w3gdatmd.F90:759-762`, `:774-775`). `IobpModulePointers`: `W3GDATMD`'s module pointers `IOBP`, `IOBPD`, `IOBP_LOC`, `IOBPD_LOC`, `IOBDP_LOC`, `IOBPA_LOC` (`:1119-1122`, `:1132-1133`). `INTEGER*2` is `std::int16_t`, `INTEGER*1` is `std::int8_t`; LayoutLeft, index 0 is Fortran index 1 |
| Structural change | The `DO IP = 1, npa` loop (`:610-614`) is one `parallel_for`; the `IOBPD` section copy stays a sequential inner loop. `IOBDP_loc = 0` (`:616`) is a `deep_copy` of 0 |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic and order

Reference: the `-O3` object of `w3profsmd_pdlib.F90` compiled with the flags of WW3's CMake
Release build under `switch_PDLIB` (gfortran 16.2.0, pinned `ww3` shell), whose disassembly is
identical to the CMake object's, read as GIMPLE (`-fdump-tree-optimized`) and `objdump -dr`. The
C++ is a throwaway unit instantiating `pdlib_iobp_init` on `Kokkos::OpenMP` and `Kokkos::Serial`,
compiled with the `openmp-release` flags (g++ 16.2.0, Kokkos 5.2.2 OpenMP), read as GIMPLE.
2026-10-16. Commands under [Reproduce](#reproduce).

- (v) No floating point in either object: the Fortran routine has no `REAL` operand, and the
  C++ object has no FMA and no scalar or vector floating-point arithmetic (`grep` of the
  disassembly). Zero warnings, also with `-Wconversion -Wsign-conversion`.
- (v) `IF (IAPROC .gt. NAPROC) RETURN` (`:592-594`) is the first test in both, before any
  allocation.
- (v) The four `ALLOCATE(..., stat=istat)` (`:596-603`) are separate statements in the object,
  in source order. Each mallocs `MAX(size,1)` bytes: `2*npa` for `IOBP_LOC` (1 when `npa <= 0`),
  `MAX(NTH,0)*npa` for `IOBPD_LOC` (1 when `NTH <= 0` or `npa <= 0`), `MAX(npa,1)` for the other
  two. A NULL return calls `PDLIB_ABORT` with 7, 8, 9, 9 (the `.rodata` words the four `lea`
  arguments point at, read with `objdump -s`). No test of prior association: they are `POINTER`
  components. The C++ allocates each with `WithoutInitializing`, extents clamped at 0, and
  returns the same code when the allocation throws, leaving the later arrays unallocated.
- (v) The four module pointers are associated with the `GRIDS(IMOD)` components (`:605-608`)
  before the loop, as in the object.
- (v) Loop (`:610-614`): for `IP = 1..npa`, `IP_glob = iplg(IP)`, then the `INTEGER*2` load and
  store of `IOBP(IP_glob)` into `IOBP_loc(IP)`, then the `INTEGER*1` section. The section length
  is the first extent of `IOBPD`'s descriptor (`ubound - lbound + 1`), not `NTH`, and the
  destination starts at `IOBPD_loc`'s first lower bound; the C++ takes `iobpd.extent(0)` and
  writes from index 0. Both copy byte by byte. The C++ GIMPLE has the same `short int` and
  `signed char` load/store pairs.
- (v) gfortran copies the section through a temporary it mallocs and frees on every `IP`
  (`IOBPD` and `IOBPD_loc` are both pointers and may alias). `IOBPD_loc` was allocated in this
  call, so it cannot overlap `IOBPD`; the C++ copies directly. Same values, one malloc less per
  node.
- (v) `IOBDP_loc = 0` (`:616`) stores byte 0 over `IOBDP_loc`'s bounds, `1..npa`; the C++ fills
  the view.
- (v) `IOBP => NULL()`, `IOBPD => NULL()` (`:617-618`) clear the module descriptors' data, then
  `DEALLOCATE(GRIDS(IMOD)%IOBP, GRIDS(IMOD)%IOBPD)` (`:619`) tests `IOBP`, frees it and clears
  it, then does the same for `IOBPD`; a NULL one calls `_gfortran_runtime_error_at` ("At line
  619 ...: Attempt to DEALLOCATE unallocated 'grids'", the same line for both items, which share
  it). The C++ tests the association flags in that order, resets the views, and returns 2 on a
  cleared flag. 2 is gfortran 16.2.0's exit status after that error: a probe deallocating a
  disassociated `INTEGER*2` pointer component exited 2 (2026-10-16, Reproduce step 4).
- (v) `SET_IOBPA_PDLIB` is the last call (a tail call in the object); the C++ calls the hook
  last and returns 0.
- (v) Each element of `IOBP_LOC` and `IOBPD_LOC` is written once, from a source the loop never
  writes, so the `parallel_for` gives the same bytes on any execution space and thread count. No
  reduction, no atomic.

## Where the two may differ (⚠)

- ⚠ `IOBPA_LOC` entries `NSEAL+1..npa` and every element when `npa` or `NTH` is 0: the Fortran
  leaves them as `malloc` returned them, the C++ as Kokkos' uninitialised allocation returned
  them. Neither is defined; `SET_IOBPA_PDLIB` writes only `1..NSEAL` (`:7050-7057`). Whether a
  reader touches `NSEAL+1..npa` was not checked.
- ⚠ The codes are only as good as the shim: it must print what `PDLIB_ABORT` or gfortran's run
  time prints and stop with the same status. The shim does not exist.
- ⚠ `PDLIB_ABORT` is taken not to return (it ends in `STOP`); the object reloads `grids` and
  `npa` after it as if it might, and the C++ returns at that point instead.
- ⚠ A second call: the Fortran leaks the old `*_LOC` storage; `IOBP` and `IOBPD` are NULL from
  the first call, so with `npa >= 1` the loop reads through a NULL pointer (a crash, not checked)
  before `DEALLOCATE` is reached, and the C++ reads an empty view, undefined as well. With
  `npa <= 0` both reach `DEALLOCATE` and stop with 2. Not reachable from `W3INIT`, which calls
  it once per grid ⚠.
- ⚠ Out-of-range input: an `iplg(IP)` outside `IOBP`'s extent, or `IOBPD`'s first extent above
  `NTH`, writes out of bounds in both, unchecked; a Kokkos debug build would stop on it.
- ⚠ The shim does not exist: the module pointers `IOBP`, `IOBPD` are read through descriptors
  with strides; the C++ takes contiguous LayoutLeft views, as `W3DIMUG` allocates them. The shim
  must also make the views it stores in `IobpModulePointers` and `IobpGridArrays` alias the
  storage the Fortran pointers will use afterwards, and give `SET_IOBPA_PDLIB` the `IOBPA_LOC`
  this routine allocated.
- ⚠ A failed allocation is detected only because Kokkos throws: its allocation-failure exception
  derives from `std::runtime_error` (`impl/Kokkos_Error.hpp` of kokkos-openmp 5.2.2, read
  2026-10-16), which the `catch (const std::exception&)` takes. Not exercised.

## Reproduce

All in `nix develop "path:<repo>/picasso/labs/pratico#ww3"` (step 5 in `#cuda`), scratch
directory `/tmp/claude-1000/pii-review/` (the fresh review's; the translation used its own):

1. WW3's CMake Release build under a copy of `switch_PDLIB` (`cmake -S <repo>/WW3 -B ww3
   -DCMAKE_BUILD_TYPE=Release -DSWITCH=<copy>`, `cmake --build ww3 --target ww3_lib`), configured
   and built fresh for the review; its object provided the module files and the reference
   disassembly.
2. `gfortran` with `Fortran_DEFINES`, `Fortran_INCLUDES` and `Fortran_FLAGS` of
   `ww3/model/src/CMakeFiles/ww3_lib.dir/flags.make` (`-O3 -fallow-argument-mismatch -g
   -fno-second-underscore -ffree-line-length-none -DSCOTCH_707 -O3`, the `W3_*` defines of the
   switch) plus `-fdump-tree-optimized -c w3profsmd_pdlib.F90`; `objdump -d` of that object and
   of the CMake one are identical past the file name. `objdump -dr` and `objdump -s -j .rodata`
   for the `PDLIB_ABORT` codes.
3. `g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp
   -I<repo>/kokkos/src/ww_kokkos -I<kokkos-openmp-5.2.2>/include -fdump-tree-optimized -c tu.cpp`,
   where `tu.cpp` instantiates `pdlib_iobp_init` explicitly on `Kokkos::DefaultExecutionSpace`
   (OpenMP) and `Kokkos::Serial`; the same with `-Wconversion -Wsign-conversion -fsyntax-only`. Zero warnings;
   `grep` of `objdump -dr` for `vfmadd`/`vfmsub`/`vfnmadd`/`vfnmsub` and scalar floating-point
   arithmetic: none.
4. The `DEALLOCATE` exit status: a 9-line `p.f90` with a derived type holding an `INTEGER*2`
   and an `INTEGER*1` pointer component, the first left NULL, `DEALLOCATE` of both;
   `gfortran -O3 p.f90 -o p && ./p; echo $?` prints "At line 8 of file p.f90 / Fortran runtime
   error: Attempt to DEALLOCATE unallocated 'g'" and 2.
5. CUDA, compile only: `nvcc_wrapper -O3 -std=c++20 -arch=sm_89 --expt-extended-lambda
   --fmad=false -Xcompiler -fopenmp,-Wall,-Wextra,-ffp-contract=off` on the same `tu.cpp`
   (kokkos-cuda 5.2.2, `KOKKOS_ARCH_ADA89`, `DefaultExecutionSpace` = `Cuda`): exit 0, no
   warning, one `sm_89` cubin.

Nothing of the port was linked or run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest reaches it until the shim and the `W3INIT` hunk exist;
the `PDLIB` entries of `matrix.base` would be its denominator ⚠.
