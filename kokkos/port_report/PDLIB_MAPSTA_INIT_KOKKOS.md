# PDLIB_MAPSTA_INIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim that passes
`GRIDS(IMOD)`'s `MAPSTA_LOC`, `INDEX_MAP` and `NBND_MAP`, the `W3GDATMD` module pointers,
`MAPSTA` and `iplg`, and calls `PDLIB_ABORT` with the code returned.

- (v) Fresh review 2026-10-16: an independent WW3 CMake Release build under `switch_PDLIB`
  (gfortran 16.2.0) in `/tmp/claude-1000/pmi-review/`, its `-O3` GIMPLE and `objdump -dr` of
  `pdlib_mapsta_init` (the recompile's disassembly identical to the CMake object's), read against
  the C++ GIMPLE of explicit instantiations on `Kokkos::DefaultExecutionSpace` (OpenMP) and
  `Kokkos::Serial`. Confirmed: the allocation order, the untested `Status` malloc freed on both
  returns, the codes 5 and 6, the three loops and their tests, every line citation. No defect in
  the code; the single-work-item kernels and the one-iteration reduce are kept (the data live in
  `ExecSpace::memory_space`, so plain host loops would not compile for CUDA), with a comment on
  why. Defects fixed in this report: `NBND_MAP` does have a reader outside `MAPSTA_HACK` (`:731`,
  below), and the `.rodata` offsets named the relocation addends, not the words. CUDA now
  compiles (below).

On an unstructured grid under `PDLIB` each process works on its `npa` local nodes (own plus
ghost), numbered `IP = 1..npa`, with the global node `iplg(IP)`. `PDLIB_MAPSTA_INIT` gives each
process its own copy of the status map: it allocates `MAPSTA_LOC(npa)` in `GRIDS(IMOD)` and sets
`MAPSTA_LOC(IP) = MAPSTA(1, iplg(IP))`. It then counts in `NBND_MAP` the global nodes `IX` that
this process holds and whose `MAPSTA(1,IX)` is below 1, allocates `INDEX_MAP(NBND_MAP)`, and lists
there, in increasing `IX`, the local index of each such node. Integer work only; no
floating-point operation.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_MAPSTA_INIT`, `model/src/w3profsmd_pdlib.F90:401-512` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), identical at NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff --quiet 03adda91 8e433f99 -- model/src/w3profsmd_pdlib.F90` exits 0), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it, the switch of the profile that times it. Its profile row is `docs/data/ww3_pdlib_gprof.tsv:97`, 0.00 s: no measured cost |
| Caller | `W3INIT`, `model/src/w3initmd.F90:1127`, under `W3_PDLIB` when `GTYPE .eq. UNGTYPE` (`:1125-1129`). The only call in `model/src` |
| Kokkos code | [`../src/ww_kokkos/pdlib_pdlib_mapsta_init.hpp`](../src/ww_kokkos/pdlib_pdlib_mapsta_init.hpp), `ww::pdlib::pdlib_mapsta_init<ExecSpace>`, a host function that allocates and launches three single-work-item kernels |
| Not ported, by switch | `W3_S` (`USE STRACE`, `:446-448`; `IENT`, `:463-465`; `CALL STRACE`, `:472-474`) and `W3_DEBUGINIT` (the `WRITE`, `:475-477`); neither is in `switch_PDLIB` |
| Not used | `NSEAL` is named in `USE` (`:450`) and never read |
| Callees | Only `PDLIB_ABORT(5)` and `PDLIB_ABORT(6)` (`:483`, `:500`; `model/src/PDLIB/yowfunction.F90:47-53`, which prints `istat` and calls yowerr's `abort`, ending in `STOP` at `model/src/PDLIB/yowerr.F90:159`). They are not called: `pdlib_mapsta_init` returns `kMapstaInitAllocMapstaLocFailed` (5) or `kMapstaInitAllocIndexMapFailed` (6), and the shim calls `PDLIB_ABORT` with it, as the other PDLIB ports of this branch do. No hook table: there is no MPI call and no other callee |
| State | `MapstaInitGrid`: `GRIDS(IMOD)%MAPSTA_LOC`, `INDEX_MAP`, `NBND_MAP` (`model/src/w3gdatmd.F90:756-758`). `MapstaInitModulePointers`: `W3GDATMD`'s module pointers `MAPSTA` (read, `:1166`), `MAPSTA_LOC`, `INDEX_MAP` and `NBND_MAP` (`:1116-1118`), the last an `int*` to the host scalar. `View<int*>`/`View<const int**>`, LayoutLeft, index 0 is Fortran index 1; the values in `INDEX_MAP` and `iplg` stay 1-based |
| Structural change | Each of the three loops (`:487-491`, `:493-497`, `:503-508`) runs inside one Kokkos work item, with the Fortran's `DO` loop and its order unchanged: the count is a `parallel_reduce` over one iteration, which adds only the identity 0 and is how the count reaches the host to size `INDEX_MAP` (`:499`); the data live in `ExecSpace::memory_space`, so the loops cannot be plain host loops. `Status = 0` (`:486`) is a `deep_copy` of 0 |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## What WW3 does here, at the pin

Read from the source and the `-O3` object, 2026-10-16:

- (v) No output at the pin depends on what the routine builds, but `NBND_MAP` is read on every
  call of `PDLIB_W3XYPUG`. `MAPSTA_LOC` appears only in this routine and its declarations.
  `INDEX_MAP` is read only in `PDLIB_W3XYPUG` (`model/src/w3profsmd_pdlib.F90:817-822`,
  `:840-845`) under `IF (MAPSTA_HACK)`, and `MAPSTA_HACK` is `.FALSE.` at its declaration (`:113`)
  with no assignment in `model/src` (`grep -rn MAPSTA_HACK WW3/model/src`). `NBND_MAP` sizes the
  automatic array `REAL :: AC_MAP(NBND_MAP)` of `PDLIB_W3XYPUG` (`:731`), which the `-O3` object
  dereferences through the `NBND_MAP` module pointer and mallocs at entry, unconditionally;
  `PDLIB_W3XYPUG` is called per spectral bin from `model/src/w3wavemd.F90:1909` on the explicit
  path. A wrong `NBND_MAP` changes only that malloc's size; a disassociated `NBND_MAP` pointer
  would be a NULL dereference.
- (v) `MAPSTA` is the module pointer of `W3GDATMD`, which `W3SETG` associates with
  `GRIDS(IMOD)%MAPSTA` (`model/src/w3gdatmd.F90:2436`), allocated `(MY,MX)` (`:1733`). The
  routine does not call `W3SETG`; it reads whatever grid was selected last. `W3INIT` calls
  `W3SETG(IMOD, ...)` at `model/src/w3initmd.F90:544`, before the call at `:1127`.

## Arithmetic and order

Reference: the `-O3` object of `w3profsmd_pdlib.F90` from WW3's CMake Release build under
`switch_PDLIB` (gfortran 16.2.0, pinned `ww3` shell), built for this port, and the same file
recompiled with the build's flags plus `-fdump-tree-optimized`, whose disassembly is identical to
the CMake object's; read as GIMPLE and `objdump -dr`. The C++ is a throwaway unit instantiating
`pdlib_mapsta_init` on `Kokkos::OpenMP` and `Kokkos::Serial`, compiled with the `openmp-release`
flags (g++ 16.2.0, Kokkos 5.2.2 OpenMP), read as GIMPLE. 2026-10-16. Commands under
[Reproduce](#reproduce).

- (v) No floating point in either object: the Fortran routine has no `REAL` operand, and the
  C++ object has no FMA and no scalar or vector floating-point arithmetic (`grep` of the
  disassembly). Zero warnings, also with `-Wconversion -Wsign-conversion`.
- (v) The automatic array `Status(NX)` (`:471`) is `malloc(MAX(4*MAX(NX,0),1))` at entry, before
  the `IAPROC` test, and the result is not tested; it is freed on both returns (a tail call to
  `free`). The C++ allocates `Status` first too, with `WithoutInitializing`, extent clamped at 0.
- (v) `IF (IAPROC .gt. NAPROC) RETURN` (`:478-480`) is the next test, before any `ALLOCATE`, in
  both.
- (v) `ALLOCATE(GRIDS(IMOD)%MAPSTA_LOC(npa), stat=istat)` (`:482`) mallocs `4*npa` bytes, or 1
  when `npa <= 0`, with bounds `1:npa`; no test of prior association (a `POINTER` component). A
  NULL return calls `PDLIB_ABORT` with the `.rodata` word 5 (at `.rodata+0x38`; the `lea`'s
  `R_X86_64_PC32` addend is `+0x34`, read with `readelf -r` and `objdump -s -j .rodata`). The C++ returns 5 when the allocation throws, before associating
  anything.
- (v) `MAPSTA_LOC => GRIDS(IMOD)%MAPSTA_LOC` (`:484`), then `NBND_MAP => GRIDS(IMOD)%NBND_MAP`
  (`:485`), then `Status = 0` (`:486`, a `memset` of `4*NX` bytes when `NX > 0`), in that order in
  both.
- (v) Loop `:487-491`: for `IP = 1..npa`, load `iplg(IP)`, store `IP` into `Status(IP_glob)`,
  then load `MAPSTA(1,IP_glob)` through `MAPSTA`'s descriptor and store it into `MAPSTA_LOC(IP)`.
  The C++ GIMPLE has the same load, store, load, store sequence. One work item runs the whole
  loop, so a repeated `iplg` value keeps the last `IP`, as in the Fortran.
- (v) `NBND_MAP = 0` (`:492`) is a store into `GRIDS(IMOD)%NBND_MAP`, and the count loop
  (`:493-497`) loads and stores that field on each hit. The test is `MAPSTA(1,IX) .lt. 1`
  (`<= 0` in the object) first, then `Status(IX) > 0`, in both. The C++ counts in the reducer and stores the result in
  `g.nbnd_map` through `mp.nbnd_map`: the same final value, integer `+1` steps from 0 in the same
  order; no other thread reads the field during the loop.
- (v) `ALLOCATE(GRIDS(IMOD)%INDEX_MAP(NBND_MAP), stat=istat)` (`:499`) mallocs `4*NBND_MAP`, or 1
  when `NBND_MAP <= 0`, bounds `1:NBND_MAP`; a NULL return calls `PDLIB_ABORT` with 6
  (at `.rodata+0x3c`, addend `+0x38`). The C++ returns 6 when the allocation throws. `INDEX_MAP =>` (`:501`) follows.
- (v) Loop `:503-508`: `NX` is reloaded, `IBND_MAP` counts from 0 in a register, and on each hit
  the `Status(IX)` value already loaded for the test is stored into `INDEX_MAP(IBND_MAP)`. The C++
  GIMPLE stores the same loaded value; one work item, increasing `IX`.

## Where the two may differ (⚠)

- ⚠ A failed `malloc` of `Status`: the object does not test it and then writes through NULL
  (a `memset` when `NX > 0`, the loop's stores when `npa > 0`), undefined and in practice a
  `SIGSEGV`; on a process with `IAPROC > NAPROC` it only frees NULL and returns. The C++ returns
  0 on such a process and otherwise calls `Kokkos::abort` (`SIGABRT`). Neither status was checked.
- ⚠ `PDLIB_ABORT` is assumed not to return (it ends in `STOP`); the object reloads `grids`,
  `npa` and `NX` after it and continues, which the C++ does not reproduce because the shim stops
  on the code returned.
- ⚠ An empty array: the Fortran mallocs 1 byte for `npa <= 0` or `NBND_MAP <= 0`, so the pointer
  is associated; a View of extent 0 allocates nothing. Not reachable from a reader at the pin
  (see above).
- ⚠ A second call: the Fortran leaks the old `MAPSTA_LOC` and `INDEX_MAP` and allocates new
  ones; the C++ releases its references. `W3INIT` calls it once per grid ⚠.
- ⚠ Out-of-range input: an `iplg(IP)` outside `1..NX` or outside `MAPSTA`'s second extent, or a
  `MAPSTA` with first extent 0, reads or writes out of bounds in both, unchecked; a Kokkos debug
  build would stop on it.
- ⚠ The shim does not exist: `MAPSTA` is read through a descriptor with strides; the C++ takes a
  contiguous LayoutLeft view, as `W3DIMX` allocates it (`model/src/w3gdatmd.F90:1733`). The shim
  must also make the views in `MapstaInitGrid` and `MapstaInitModulePointers` alias the storage
  the Fortran pointers use afterwards, and `mp.nbnd_map` point at `GRIDS(IMOD)%NBND_MAP`, and
  associate the Fortran `NBND_MAP` pointer, which `PDLIB_W3XYPUG` dereferences (`:731`).
- ⚠ Running the loops in one work item is exact on any execution space but serial; on a GPU it
  is one thread. Spreading them is phase 2. (v) CUDA compiles (fresh review, 2026-10-16:
  `nvcc_wrapper -O3 -std=c++20 -arch=sm_89 --fmad=false --expt-extended-lambda -Xcompiler
  -fopenmp -Xcompiler -Wall,-Wextra,-Wpedantic,-ffp-contract=off` in the `#cuda` shell, Kokkos
  5.2.2 CUDA, compile only; the 118 warnings are all `-Wpedantic` on the line directives of
  nvcc's own `cudafe1.cpp`, none from the header); ⚠ not run.

## Reproduce

All in `nix develop "path:<repo>/picasso/labs/pratico#ww3"`, scratch directory
`/tmp/claude-1000/pmi-scratch/`:

1. `cmake -S <repo>/WW3 -B ww3 -DCMAKE_BUILD_TYPE=Release -DSWITCH=<copy of
   switch_PDLIB>`, `cmake --build ww3 --target ww3_lib -j 16` (`configure.sh`).
2. `gfortran` with `Fortran_DEFINES`, `Fortran_INCLUDES` and `Fortran_FLAGS` of
   `ww3/model/src/CMakeFiles/ww3_lib.dir/flags.make` (`-O3 -fallow-argument-mismatch -g
   -fno-second-underscore -ffree-line-length-none -DSCOTCH_707 -O3`, the `W3_*` defines of the
   switch) plus `-fdump-tree-optimized -c w3profsmd_pdlib.F90` (`ftn.sh`); `objdump -d` of that
   object and of the CMake one are identical past the file name. `objdump -dr` and
   `objdump -s -j .rodata` for the `PDLIB_ABORT` codes.
3. `g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp
   -I<repo>/kokkos/src/ww_kokkos -I<kokkos-openmp-5.2.2>/include -fdump-tree-optimized -c tu.cpp`,
   where `tu.cpp` wraps `pdlib_mapsta_init<Kokkos::OpenMP>` and `<Kokkos::Serial>` in two
   non-inline functions; the same with `-Wconversion -Wsign-conversion -fsyntax-only`
   (`tu/build.sh`). Zero warnings; `grep` of `objdump -dr` for `vfmadd`/`vfmsub`/`vfnmadd`/
   `vfnmsub` and scalar or packed floating-point arithmetic: none.

Nothing was linked or run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest reaches it until the shim and the `W3INIT` hunk exist;
the `PDLIB` entries of `matrix.base` would be its denominator ⚠. At the pin its outputs have no
reader that changes an output (`MAPSTA_HACK` is `.FALSE.`; `NBND_MAP` only sizes `AC_MAP`,
`:731`), so a regtest would show only that the port does not disturb the run.
