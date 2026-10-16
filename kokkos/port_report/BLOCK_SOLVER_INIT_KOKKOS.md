# BLOCK_SOLVER_INIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim, the `W3INIT` wiring
and the `JACOBI_INIT` hook below.

- (v) Fresh review 2026-10-16: a CMake Release build of `ww3_lib` under `switch_PDLIB` of its own,
  `w3profsmd_pdlib.F90` recompiled with `-fdump-tree-optimized` (disassembly identical to the CMake
  object), its GIMPLE read against the C++ GIMPLE: `POS_TRICK` stores, the `ALLOCATE` order and
  skip-on-failure, the `.DIVMOD` loop and its store order, the `FSTOTALIMP`-first test and the
  `JACOBI_INIT` tail call agree. Two defects fixed. Error surfacing: the routine called a
  `pdlib_abort` hook and `Kokkos::abort`ed if it returned, unlike W3DIMS, W3DIMA and W3IOGR,
  which return the code the Fortran stops with; it now returns `PDLIB_ABORT`'s `istat` (8), and
  the `jacobi_init` hook returns 0 or its own (9 to 12, `:7590-7608`), for the shim to pass to
  `PDLIB_ABORT`. The `JACOBI_INIT` bullet named grid a, which `matrix.base` runs only with
  `NO_PDLIB` (`regtests/bin/matrix.base:2030-2031`); now grid c and `grdset_c`. Every other line
  citation checked at the pin.

`BLOCK_SOLVER_INIT` builds the index tables of the implicit block solver on WW3's unstructured
grids (`PDLIB`). `POS_TRICK(3,2)` gives, for corner `POS` of a triangle, the other two corners
counter-clockwise; the element loops of the Jacobi and Gauss-Seidel assemblies read it. The four
lists `ListISPprevDir`, `ListISPnextDir`, `ListISPprevFreq`, `ListISPnextFreq` give, for each
spectral component `ISP = ITH + (IK-1)*NTH`, the component one direction back or forward (periodic
in `ITH`) and one frequency down or up (`-1` at `IK = 1` and `IK = NK`); the refraction and
frequency-shift terms of `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` read the direction lists
(`model/src/w3profsmd_pdlib.F90:5908-5909`, `:6148-6149`). When the scheme is fully implicit and
uses Jacobi it then calls `JACOBI_INIT`. `W3INIT` calls it once per unstructured grid, on the
computing ranks (`model/src/w3initmd.F90:920-923`). The routine has no floating-point arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `BLOCK_SOLVER_INIT`, `model/src/w3profsmd_pdlib.F90:6752-6888` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it, the switch of the profile that times it (`docs/data/ww3_pdlib_gprof.tsv`). The text of the subroutine is identical at NOAA-EMC/WW3 `03adda91`, at the same lines |
| Kokkos code | [`../src/ww_kokkos/pdlib_block_solver_init.hpp`](../src/ww_kokkos/pdlib_block_solver_init.hpp): `ww::pdlib::block_solver_init<ExecSpace>`, the allocation on the host and one `parallel_for` over `ISP` |
| Callees | `JACOBI_INIT(IMOD)` (`:6878`; `:7515-7613`) is not translated here: a host hook of `BlockSolverInitHooks`, which returns 0 or the `istat` of the `PDLIB_ABORT` it ends in (9 to 12, `:7590-7608`). A hook needed on a path and left null aborts. `PDLIB_ABORT(8)` (`:6833`; `model/src/PDLIB/yowfunction.F90:47-53`, which prints and ends in the `STOP` of `model/src/PDLIB/yowerr.F90:159`) is not called: `block_solver_init` returns 8 and the shim calls `PDLIB_ABORT(8)`, as W3DIMS and W3IOGR return `EXTCDE`'s code, so that a host routine of the port never prints or stops |
| Not ported, by switch | `W3_S` (the `USE` of `STRACE`, `:6797-6799`; no call) and `W3_DEBUGINIT` (the `USE` of `IAPROC`, `:6810-6812`, and six `WRITE`/`FLUSH` pairs to unit `740+IAPROC`, `:6828-6887`). `switch_PDLIB` sets neither |
| Not used | `TPI`, `TPIINV`, `INIT_GET_ISEA`, `iplg` are named in `USE` (`:6800-6802`) and never read |
| State | `BlockSolverTables`: `POS_TRICK` of `PDLIB_W3PROFSMD` (`:129`) as six `int` in column-major order, and the four `W3PARALL` arrays (`model/src/w3parall.F90:86-87`) as `View<int*, LayoutLeft>` of the execution space's memory, element `isp-1`, values 1-based or `-1`. Four flags stand for Fortran's `ALLOCATED()` |
| Structural change | The `ISP` loop (`:6838-6868`) is one `parallel_for`. Each element of each list is written once, from its own `ISP`, with no reduction and no atomic, so the order of the iterations cannot change a bit on any execution space |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## What WW3 does here, at the pin

Read from the source and the `-O3` object, 2026-10-16:

- (v) `POS_TRICK` has no initialiser (`:129`); the six assignments (`:6821-6826`) become two
  vector stores of `{2,3,1,3}` and `{1,2}` at `&POS_TRICK`, that is `POS_TRICK(:,1) = (2,3,1)`,
  `POS_TRICK(:,2) = (3,1,2)`.
- (v) `ALLOCATE(ListISPnextDir(NSPEC), ListISPprevDir(NSPEC), ListISPnextFreq(NSPEC),
  ListISPprevFreq(NSPEC), stat=istat)` (`:6832`) allocates in the order of the list, each with
  `malloc(MAX(4*NSPEC,1))` and bounds `1:NSPEC` (zero-size when `NSPEC <= 0`). An array already
  allocated, or a `malloc` that returns null, sets `istat` and jumps past the remaining items,
  which stay unallocated; `istat /= 0` calls `PDLIB_ABORT(8)` (`:6833`). The contents are not
  initialised; the loop writes every element.
- (v) The four arrays are `ALLOCATABLE` module variables of `W3PARALL`, one copy per process, and
  no routine at the pin deallocates them (no `DEALLOCATE` of `ListISP*` in `model/src`). A second
  call of `BLOCK_SOLVER_INIT` in one process therefore ends in `PDLIB_ABORT(8)`. The `ww3_multi`
  inputs of `ww3_tp2.17` name one model grid each (`ww3_multi_grdset_*.nml:28`) and leave `DOMAIN%NRGRD` at
  its default 1 (`model/src/w3nmlmultimd.F90:617`), so its runs call it once.
- (v) The object compiles the loop as the source reads: one `idiv` of `ISP-1` by `NTH` gives
  `IK-1` and `MOD(ISP-1,NTH)`, then `ISPprevFreq = (IK-2)*NTH + ITH` or `-1`,
  `ISPnextFreq = NTH*IK + ITH` or `-1`, `ListISPprevDir = NTH*(IK-1) + JTH` with `JTH = NTH` at
  `ITH = 1`, else `MOD(ISP-1,NTH)`, and `ListISPnextDir = NTH*(IK-1) + JTH` with `JTH = 1` at
  `ITH = NTH`, else `MOD(ISP-1,NTH) + 2`, stored in the order prevFreq, nextFreq, prevDir,
  nextDir.
- (v) `FSTOTALIMP .and. B_JGS_USE_JACOBI` (`:6873`): the object loads `FSTOTALIMP` (pointer into
  `MPARS(IMOD)%SCHMS`, `model/src/w3gdatmd.F90:1409`, `:2855`), tests it `/= 0`, and only then loads
  `B_JGS_USE_JACOBI` (`:1418`, `:2867`); `JACOBI_INIT` is a tail call. Of the `PDLIB` entries of
  `ww3_tp2.17` in `matrix.base` (`regtests/bin/matrix.base:2032-2036`), grid c and `grdset_c` set
  both (`namelists_c.nml:25,32`, `namelists_inlc.nml:25,32`: `IMPTOTAL = T`, `JGS_USE_JACOBI = T`);
  grid b sets `IMPTOTAL = F` (`namelists_b.nml:25`), and the `-g pdlib` entry has no grid input in
  `ww3_tp2.17/input/`. Grid c is the profiled case, so the hook is
  on its path.

## Arithmetic

The reference is the `-O3` object of `w3profsmd_pdlib.F90` compiled with the flags of WW3's CMake
Release build under `switch_PDLIB` (gfortran 16.2.0 in the pinned `ww3` shell), whose disassembly
is identical to the CMake object of the same configure, read as GIMPLE (`-fdump-tree-optimized`)
and assembly (`objdump -dr`). The C++ is a throwaway unit with the `openmp-release` flags, read the
same way. 2026-10-16.

- (v) Integer operations only: in both objects one `.DIVMOD (ISP-1, NTH)`, then the same
  products and sums per list (C++ GIMPLE: `(quot-1)*nth + ith`, `nth*ik + ith`,
  `nth*quot + jth` with `jth` from `PHI <nth, rem>` and `PHI <1, rem+2>`). No floating point in
  either; no FMA in the C++ object (`objdump`, no `vfmadd`/`vfmsub`/`vfnmadd`/`vfnmsub`).
- (v) Fortran `MOD` and integer `/` truncate toward zero, as C++ `%` and `/` do, so the values
  agree for any sign of `NTH`. `ISP-1` is computed in unsigned arithmetic in both GIMPLE dumps.
- (v) Store order per element and the call order (allocate, `istat` test, loop, `JACOBI_INIT` hook)
  match the object. The C++ fences the execution space after the loop, before the hook. The
  object does not treat `PDLIB_ABORT` as `noreturn` and falls through to the loop; at run time it
  stops, so the C++ returns at that point.
- ⚠ `NTH = 0` traps in the Fortran (`idiv` by zero, `SIGFPE`); in the C++ it is undefined
  behaviour, which on x86 also traps in the `idiv`, but that was not run.
- (v) A failed host allocation is caught as a `std::exception`: in the pinned
  `kokkos-openmp-5.2.2`, `HostSpace::impl_allocate` calls `Impl::throw_bad_alloc`
  (`include/impl/Kokkos_Error.hpp:18`), which throws `Kokkos::Experimental::BadAlloc`, a
  `std::runtime_error` (`:34`) (`objdump -drC lib/libkokkoscore.a`, 2026-10-16).
- ⚠ The failed-allocation path was not run, and `CudaSpace`'s allocation failure was not read. A
  backend that aborts instead ends the program without `PDLIB_ABORT`'s message.
- ⚠ An integer overflow of `ITH + IK*NTH` (beyond 2^31 components) is undefined in both languages
  and not reproduced by construction.
- (v) CUDA compiles: an instantiation on `Kokkos::Cuda` with the pinned `kokkos-cuda-5.2.2`
  `nvcc_wrapper`, `-arch=sm_89 --fmad=false`, no warning, and no floating-point instruction in the
  SASS (`cuobjdump -sass`). Compiled only.
- ⚠ GPU: the views live in the execution space's memory, and the kernel was not run on any device.
  The routine has no floating point, so the transcendental-function caveat of `AGENTS.md`, "Bit
  for bit", does not apply.

## Benchmarks

None. No timing without parity (`AGENTS.md`). The PDLIB profile gives it 0.00 s self time
(`docs/data/ww3_pdlib_gprof.tsv`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v):

- `cmake -S WW3 -B /tmp/claude-1000/bsi-scratch/ww3 -DCMAKE_BUILD_TYPE=Release
  -DSWITCH=<switch_PDLIB>` and `cmake --build ... --target ww3_lib`, in
  `nix develop "path:picasso/labs/pratico#ww3"`; then `w3profsmd_pdlib.F90` recompiled alone with
  the flags of `flags.make` plus `-fdump-tree-optimized`; `objdump -d` of that object and of the
  CMake one identical.
- A throwaway unit that includes the header and instantiates
  `ww::pdlib::block_solver_init<Kokkos::DefaultExecutionSpace>` (OpenMP), compiled with
  `g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp -c`
  (g++ 16.2.0 of the same shell) against the pinned `kokkos-openmp-5.2.2`: no warning, none with
  `-Wconversion -Wsign-conversion` either, no FMA. It was compiled only, not linked and not run.
  The `serial-debug` and `cuda-release` presets were not built.
- Fresh review, 2026-10-16 (v): the same `cmake` and `--target ww3_lib` in its own directory, the
  same recompile with `-fdump-tree-optimized`; the header after the fix, instantiated on
  `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, compiled with the flags above, with
  `-Wconversion -Wsign-conversion`, and at `-O0 -g -fsanitize=address,undefined`: no warning, no
  floating-point or FMA instruction (`objdump -d`). In `nix develop "path:picasso/labs/pratico#cuda"`,
  `nvcc_wrapper -O3 -std=c++20 -arch=sm_89 --fmad=false -extended-lambda -Xcompiler -fopenmp
  -Wall -Wextra -Wpedantic -ffp-contract=off -c` on `Kokkos::Cuda`: no warning. Compiled only, not
  linked and not run.
