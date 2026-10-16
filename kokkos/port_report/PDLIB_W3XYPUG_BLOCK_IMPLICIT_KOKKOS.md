# PDLIB_W3XYPUG_BLOCK_IMPLICIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim, the `W3WAVE` hunk
and the hooks of `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK`.

- (v) Fresh review 2026-10-16: an independent CMake Release build of `ww3_lib` under
  `switch_PDLIB` and the `-O3` GIMPLE and relocations of `w3profsmd_pdlib.F90` (disassembly
  identical to the CMake object) confirm the `/= 0` test, the tail call with the seven dummies
  unchanged and the `WRITE`/`STOP` path; `flags.make` defines `W3_ST4` and neither `W3_REF1` nor
  `W3_TRNK`; the six `JgsParams` stores and nothing else; code 2 passed through; `LCALC` unread.
  One defect fixed: the `STOP` (exit status 0) was returned as 3, a positive value like an abort
  code; it is now -1, the encoding `pdlib_pdlib_init.hpp` uses for its bare `STOP`. One citation
  corrected (the `LCALC` occurrences) and the caller's line at `03adda91` added. Compiled, not run, on
  OpenMP, Serial and CUDA (below).

`PDLIB_W3XYPUG_BLOCK_IMPLICIT` is the entry point of the block-implicit propagation step on
WW3's unstructured grids (`PDLIB`). `W3WAVE` calls it once per time step when the scheme is fully
implicit (`FSTOTALIMP .and. IT /= 0`, `model/src/w3wavemd.F90:1916-1922`). When the block solver
is set to Jacobi (`B_JGS_USE_JACOBI`) it calls `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK`, which solves
the implicit action balance for all spectral components of each node at once; otherwise it writes
an error line and stops. The routine has no floating-point arithmetic of its own.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_W3XYPUG_BLOCK_IMPLICIT`, `model/src/w3profsmd_pdlib.F90:2742-2813` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it, the switch of the profile that times it (`docs/data/ww3_pdlib_gprof.tsv`). The text of the subroutine is identical at NOAA-EMC/WW3 `03adda91`, at the same lines |
| Kokkos code | [`../src/ww_kokkos/pdlib_w3xypug_block_implicit.hpp`](../src/ww_kokkos/pdlib_w3xypug_block_implicit.hpp): `ww::pdlib::pdlib_w3xypug_block_implicit<kRef1, kTrnk, kSt4>`, on the host |
| Callees | `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` (`:2805`; `:5463-6406`), already translated: `pdlib_jacobi_gauss_seidel_block` of [`pdlib_jgs_block.hpp`](../src/ww_kokkos/pdlib_jgs_block.hpp), called, not re-translated. Its hooks (MPI exchanges, `CALCARRAY_JACOBI_VEC`, `APPLY_BOUNDARY_CONDITION`, the iteration report) are passed through unchanged |
| Not ported, by switch | `W3_S` (the `USE` of `STRACE`, `:2788-2790`; no call) and `W3_DEBUGSOLVER` (the `USE` of `IAPROC`, `:2792-2794`, and a `WRITE`/`FLUSH` pair to unit `740+IAPROC`, `:2800-2803`). `switch_PDLIB` sets neither |
| Errors | The `WRITE(*,*) 'Error: You need to use with JGS_USE_JACOBI'` and `STOP 'Correct your implicit solver options'` (`:2808-2809`) are not executed: the routine returns `kXypugBlockImplicitStopNoJacobi` (-1) and the shim writes the line and stops, as W3DIMS and W3IOGR return `EXTCDE`'s code, so that a host routine of the port never prints or stops. The code is negative because the `STOP` exits with status 0, as `kPdlibInitStop` (-1) for the bare `STOP` of `PDLIB_INIT` (`pdlib_pdlib_init.hpp`); a positive code would read as an exit or abort status. The value 2 of `pdlib_jacobi_gauss_seidel_block` is passed through (`kXypugBlockImplicitNormNoCasSig`) |
| Arguments | `IMOD, FACX, FACY, DTG, VGX, VGY` overwrite the same fields of a copy of the caller's `JgsParams` before the call, which is how the C++ solver receives what the Fortran passes by reference. `LCALC` is accepted and not read: the solver never reads it (in `:5463-6406` it appears only in the dummy list, `:5463`, and its declaration, `:5577`) and its C++ has no such argument |
| Switch flags | `kRef1`, `kTrnk`, `kSt4` (W3_REF1, W3_TRNK, W3_ST4) only reach the solver; their defaults are those of `switch_PDLIB` (REF0, no TRNK, ST4): the CMake `flags.make` of that switch defines `-DW3_ST4` and neither `W3_REF1` nor `W3_TRNK` (its `TRKNC` is the track-output NetCDF switch, `W3_TRKNC`, a different one) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## What WW3 does here, at the pin

Read from the source and the `-O3` object, 2026-10-16:

- (v) The object loads the pointer `B_JGS_USE_JACOBI` (`model/src/w3gdatmd.F90:1418`, pointed at
  `MPARS(IMOD)%SCHMS%B_JGS_USE_JACOBI` by `W3SETG`, `:2867`), loads the `LOGICAL(4)` word and
  tests it `/= 0` (`:2804`). The C++ takes it as `int` and tests `!= 0`.
- (v) When set, `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK(IMOD, FACX, FACY, DTG, VGX, VGY, LCALC)` is a tail
  call with the seven dummy addresses unchanged (`:2805-2806`); nothing else happens on that path.
- (v) When not set, `_gfortran_st_write` on unit 6 (line 2808), one
  `_gfortran_transfer_character_write` of the 42-character message, `_gfortran_st_write_done`,
  then `_gfortran_stop_string` of the 36-character string with `quiet = 0` (`:2809`).
- (v) A `STOP` with a character code ends the gfortran 16.2.0 program with exit status 0, the
  message `STOP Correct your implicit solver options` on standard error and the `WRITE` line on
  standard output (a four-line program with the same two statements, compiled and run once,
  below).
- (v) `W3WAVE` passes `FACX` twice, as `FACX` and as `FACY`
  (`CALL PDLIB_W3XYPUG_BLOCK_IMPLICIT(IMOD, FACX, FACX, DTG, VGX, VGY, UGDTUPDATE)`,
  `model/src/w3wavemd.F90:1922` at the pin, `:1919` at `03adda91`). That is the caller's text, which the shim must reproduce; this
  routine passes whatever it receives.
- (v) The profile gives it 0.00 s self time, against 138.51 s for
  `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` (`docs/data/ww3_pdlib_gprof.tsv`, `ww3_tp2.17` grid c under
  `-pg`).

## Arithmetic

The reference is the `-O3` object of `w3profsmd_pdlib.F90` compiled with the flags of WW3's CMake
Release build under `switch_PDLIB` (gfortran 16.2.0 in the pinned `ww3` shell), whose disassembly
is identical to the CMake object of the same configure, read as GIMPLE (`-fdump-tree-optimized`)
and assembly (`objdump -d`). The C++ is a throwaway unit with the `openmp-release` flags, read the
same way. 2026-10-16.

- (v) No floating-point operation in either object for this routine. The C++ GIMPLE of the
  instantiation tests `use_jacobi != 0`, copies the caller's `JgsParams`, stores `imod` and the
  five `float` dummies into it (plain moves, one 4-lane vector store of `dtg, facx, facy, vgx`
  and one scalar store of `vgy`), then the inlined solver. A move cannot change a bit.
- (v) No FMA in the C++ object (`objdump -d`, no `vfmadd`/`vfmsub`/`vfnmadd`/`vfnmsub`).
- (v) Call order: test, then the solver, then return; or test, then return the stop code. The
  Fortran has no work before the test under `switch_PDLIB`.
- ⚠ The solver's hooks take no arguments. `calcarray_jacobi_vec` reads `DTG, FACX, FACY, VGX,
  VGY` and `apply_boundary_condition` reads `IMOD` (`:5767`, `:5788`); in the C++ they must get
  them from their own context, and nothing in the type system checks that the shim gives them the
  values passed here. Not testable until the shim exists.
- ⚠ Under MPI, the Fortran `STOP` ends one rank with status 0 without `MPI_ABORT`; whether the
  other ranks are torn down depends on the MPI launcher and was not checked. Every rank reads the
  same `B_JGS_USE_JACOBI` from the namelist, so all of them stop at once in practice ⚠ (not run).
- ⚠ GPU: the driver runs on the host and adds nothing to the device code of the solver, whose
  own report carries its GPU caveats. Compiled for CUDA (sm_89, `--fmad=false`), never run.

## Benchmarks

None. No timing without parity (`AGENTS.md`). The PDLIB profile gives it 0.00 s self time
(`docs/data/ww3_pdlib_gprof.tsv`).

## How it was regtested

Not yet, and the port's code was not run. What was done, 2026-10-16 (v):

- `cmake -S WW3 -B /tmp/claude-1000/xbi-scratch/ww3 -DCMAKE_BUILD_TYPE=Release
  -DSWITCH=WW3/regtests/ww3_tp2.17/input/switch_PDLIB` and `cmake --build ... --target ww3_lib`,
  in `nix develop "path:picasso/labs/pratico#ww3"`; then `w3profsmd_pdlib.F90` recompiled alone
  with the defines, includes and flags of `flags.make` plus `-fdump-tree-optimized`; `objdump -d`
  of that object and of the CMake one identical.
- A throwaway unit that includes the header and calls
  `ww::pdlib::pdlib_w3xypug_block_implicit` on `Kokkos::DefaultExecutionSpace` (OpenMP) with a
  no-op hooks type and `JgsSpr4St4`, compiled with
  `g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp -c`
  (g++ 16.2.0 of the same shell) against the pinned `kokkos-openmp-5.2.2`: no warning, none from
  this header with `-Wconversion -Wsign-conversion` either, no FMA. It was compiled only, not
  linked and not run. The `serial-debug` and `cuda-release` presets were not built.
- The one program that was linked and run is not the port: four lines of Fortran with the
  routine's `WRITE(*,*)` and `STOP` statements, `gfortran -O3 stop.f90 -o stop && ./stop` in the
  same shell, to read the exit status (0) and the streams of the `STOP`.
- Fresh review: `cmake -S WW3 -B /tmp/claude-1000/xbi-review/ww3 -DCMAKE_BUILD_TYPE=Release
  -DSWITCH=$PWD/WW3/regtests/ww3_tp2.17/input/switch_PDLIB`, `cmake --build ... --target ww3_lib`,
  the object recompiled with `flags.make` plus `-fdump-tree-optimized`. A throwaway unit with a
  no-op hooks type and `JgsSpr4St4`, compiled only: `g++ -O3 -march=x86-64-v3 -std=c++20 -Wall
  -Wextra -Wpedantic -ffp-contract=off -fopenmp -c` on `Kokkos::DefaultExecutionSpace` (OpenMP)
  and on `Kokkos::Serial`, and at `-O0 -g -fsanitize=address,undefined` on `Kokkos::Serial`, in the
  `ww3` shell: no warning, no FMA in either `-O3` object; the GIMPLE returns -1, 2 or 0.
  In `nix develop "path:picasso/labs/pratico#cuda"` (CUDA 12.9, `kokkos-cuda-5.2.2`):
  `nvcc_wrapper -O3 -std=c++20 -arch=sm_89 --fmad=false -extended-lambda -Xcompiler -fopenmp
  -Xcompiler -Wall,-Wextra,-Wpedantic,-ffp-contract=off -c`: exit 0, its only warnings the
  host pass's `-Wpedantic` notes on nvcc's own line directives; none without `-Wpedantic`.
