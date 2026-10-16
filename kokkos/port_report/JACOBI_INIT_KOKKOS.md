# JACOBI_INIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Allocates the arrays of the PDLIB implicit solver when it runs as Jacobi: the sparse matrix
`ASPAR_JAC`, the right-hand side `B_JAC`, the refraction velocities `CAD_THE`, the frequency-shift
velocities `CAS_SIG` or `CWNB_SIG_M2`, and the solution `U_JAC` when the solver is not the block
Gauss-Seidel. `BLOCK_SOLVER_INIT` calls it once when `FSTOTALIMP` and `B_JGS_USE_JACOBI` are set
(`model/src/w3profsmd_pdlib.F90:6873-6878`). No floating-point arithmetic and no fill.

## What was ported

| | |
|---|---|
| WW3 routine | `JACOBI_INIT(IMOD)`, `model/src/w3profsmd_pdlib.F90:7515-7613` at h0ffmann/WW3 `8e433f99`, the same lines and text at NOAA-EMC/WW3 `03adda91` |
| Switch | `WW3/regtests/ww3_tp2.17/input/switch_PDLIB`, the switch of the profile `docs/data/ww3_pdlib_gprof.tsv` (self time 0.00 s there) |
| Kokkos code | [`../src/ww_kokkos/pdlib_jacobi_init.hpp`](../src/ww_kokkos/pdlib_jacobi_init.hpp), `int jacobi_init<ExecSpace>(const JacobiInitConfig&, JacobiArrays<MemSpace>&)`, returning `kJacobiInitOk` (0) or the `PDLIB_ABORT` istat (9 to 12); host only, no kernel |
| Not ported | `W3_S` (the `USE` of `STRACE` and `IENT`, `:7560-7562`, `:7579-7581`) and `W3_DEBUGINIT` (the `USE` of `IAPROC`, `:7568-7570`): neither is in `switch_PDLIB`, and neither has an executable statement. `GRIDS` is `USE`d (`:7563`) and never referenced |
| Not translated | `PDLIB_ABORT(istat)` (`PDLIB/yowfunction.F90:47-53`: a `PRINT` and the `STOP` of `PDLIB/yowerr.F90:159`): the istat is returned and the shim calls `PDLIB_ABORT` with it, the convention of `BLOCK_SOLVER_INIT`, `W3DIMS` and `W3IOGR`. The return type is what `BlockSolverInitHooks::jacobi_init` (`int (*)(void*, int imod)`) returns from `BLOCK_SOLVER_INIT`'s tail call |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## What the Fortran does

Reference: the `-O3` GIMPLE (`-fdump-tree-optimized`) and the disassembly of `w3profsmd_pdlib.F90`
compiled with the flags and defines of WW3's CMake Release build for `switch_PDLIB` (gfortran
16.2.0, pinned `ww3` shell), 2026-10-16. That object and the CMake build's own object disassemble
to the same instructions (`diff` of the two `objdump -d` listings, empty).

- (v) `IMEM` is the `INTEGER, PARAMETER` 1 (`model/src/w3parall.F90:90`): the GIMPLE has the
  `ASPAR_JAC` branch (`:7588-7590`) and no trace of `ASPAR_DIAG_ALL` (`:7591-7593`).
- (v) Each `ALLOCATE(..., stat=istat)` has one item. The object first tests the descriptor's data
  field: not `NULL` (already allocated) sets `istat` and skips the allocation; else
  `malloc(MAX(bytes, 1))` with no fill, and `istat /= 0` when it returns `NULL`. Bytes are
  `4 * MAX(NSPEC,0) * MAX(n2,0)` in `INTEGER(8)`, 0 when either extent is `<= 0`.
- (v) The codes passed to `PDLIB_ABORT` are 9, 10, 11 (for `CAD_THE`, `CAS_SIG` and `CWNB_SIG_M2`)
  and 12, read from `.rodata` of the object (`09 0a 0b 0c` at offset 0) through the relocations of
  the five call sites; the `U_JAC` call is a tail call.
- (v) `FreqShiftMethod` (`:125`, a module variable initialised to 2 and assigned nowhere at the
  pin, `grep` of `model/src`) is loaded once and compared with 1, then with 2 (`:7599`, `:7602`);
  any other value allocates neither array. Both branches are in the object.
- (v) `CWNB_SIG_M2(1-NTH:NSPEC, NSEAL)` (`:7603`): the first extent is `NSPEC + 1 - (1 - NTH)` in
  `INTEGER(8)`, with `1 - NTH` in `INTEGER(4)`, clamped at 0. Only this `ALLOCATE` carries
  gfortran's overflow test (element count `> 2^62 - 1`), which sets `istat` and so reaches
  `PDLIB_ABORT(11)`, allocated or not.
- (v) `B_JGS_BLOCK_GAUSS_SEIDEL` is tested as the word `/= 0` (`:7606`); `IMOD` is never read.

## Translation

- (v) `allocate_item` per `ALLOCATE`, in the Fortran's order and under the same conditions: an
  array already allocated, or a `view_alloc` that throws, returns false, and `jacobi_init` returns
  the Fortran's code at once. The allocations after it are not made, since the Fortran never
  continues past `PDLIB_ABORT`; the port neither prints nor stops.
- (v) `Kokkos::WithoutInitializing`: the Fortran leaves the arrays undefined, so nothing is
  written. The first writers (`CALCARRAY_JACOBI_VEC`, `CALCARRAY_JACOBI_SPECTRAL_1`, the block
  solver) are translated separately and fill what they read.
- (v) The `*_allocated` flags stand for `ALLOCATED()`: gfortran allocates one byte for an empty
  array, which is then allocated, and a Kokkos View of extent 0 does not say so by itself.
- (v) `CWNB_SIG_M2` is stored from index 0: `cwnb_sig_m2(isp + nth - 1, ip - 1)` is
  `CWNB_SIG_M2(ISP, IP)`, the convention of `pdlib_jacobi_spectral.hpp`. The extent and the
  overflow test are computed as the object does; the overflow test runs before the
  already-allocated test, which changes nothing, since both lead to `PDLIB_ABORT(11)`.
- (v) The `IMEM == 2` branch is kept behind `if constexpr (kImem == 2)` with `kImem = 1`, as dead
  as in the Fortran.
- ⚠ Kokkos allocates through its own memory space (aligned, labelled, reference-counted), not
  `malloc`; the arrays' contents are undefined in both. Whether these Views alias the Fortran's
  module arrays or replace them is the shim's decision. Not checked against a caller, since none
  exists.
- ⚠ Kokkos reports a failed allocation as an exception derived from `std::runtime_error`; the
  catch is `std::exception`. A failed allocation was not provoked.
- ⚠ Which regtests reach `JACOBI_INIT` (it needs `FSTOTALIMP` and `B_JGS_USE_JACOBI`,
  `:6873`), and which take the `U_JAC` branch, was not checked. A `grep -ril
  'JGS_BLOCK_GAUSS_SEIDEL *= *F'` of `WW3/regtests/*/input*` finds grid inputs of `ww3_tp2.17` and
  `ww3_ufs1.1` that set it false; whether those runs are implicit Jacobi was not read.
- ⚠ Static parity only: nothing of the port was run, and no regtest reaches it yet.

## Compile check

In the pinned `ww3` shell (`nix develop "path:<worktree>/picasso/labs/pratico#ww3"`, g++ 16.2.0,
Kokkos 5.2.2 with OpenMP and Serial): `g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra
-Wpedantic -ffp-contract=off -fopenmp -c` on a TU with the explicit instantiations
`template int jacobi_init<Kokkos::DefaultExecutionSpace>` and `<Kokkos::Serial>`, a function of
the type `BlockSolverInitHooks::jacobi_init` that calls it, and a `static_assert` that the two
types agree: 0 warnings, 0 more in this header with `-Wconversion -Wsign-conversion`, 0 FMA
instructions in `objdump -d`. In the `#cuda` shell (Kokkos 5.2.2 CUDA), `nvcc_wrapper -O3
-std=c++20 -arch=sm_89 --fmad=false -Xcompiler -fopenmp,-Wall,-Wextra,-ffp-contract=off
--expt-extended-lambda -c` on the same TU: 0 warnings, both instantiations emitted. Nothing was
linked or run. The Fortran reference: WW3's CMake `ww3_lib` target (Release, `switch_PDLIB`),
then `w3profsmd_pdlib.F90` recompiled with the same flags plus `-fdump-tree-optimized`; its
`objdump -d` equals the CMake object's.

- (v) Fresh review 2026-10-16: an independent Fortran object and `-O3` GIMPLE in
  `/tmp/claude-1000/ji-review/` (gfortran 16.2.0). Checked against them: the six `ALLOCATE`s,
  their extents (`CWNB_SIG_M2`'s `NSPEC + 1 - (1 - NTH)` in `INTEGER(8)`, clamped at 0) and order;
  the `data /= NULL` test before `malloc(MAX(bytes,1))`; the overflow test `> 4611686018427387903`
  on `CWNB_SIG_M2` only, reaching `PDLIB_ABORT(11)` allocated or not; `FreqShiftMethod` loaded once,
  compared with 1 then 2; `B_JGS_BLOCK_GAUSS_SEIDEL` dereferenced and tested `/= 0`; no
  `ASPAR_DIAG_ALL` and no read of `IMOD` in the GIMPLE, so the `if constexpr` branch is as dead as
  the Fortran's; the codes 9, 10, 11, 11, 12 at the five call sites, from the `.rodata` words
  `09 0a 0b 0c` and the `R_X86_64_PC32` addends; every line citation at `8e433f99`, and the routine's
  text identical at `03adda91` (`diff`, empty). The `CWNB_SIG_M2` index convention matches
  `pdlib_jacobi_spectral.hpp:68-69`. One defect, fixed: `jacobi_init` returned `void` and ended a
  failure in a `pdlib_abort` hook and `Kokkos::abort`, while its caller's hook
  `BlockSolverInitHooks::jacobi_init` returns the istat as `int`, so no shim could pass it on. It
  now returns `int` (`kJacobiInitOk` or 9 to 12), and `JacobiInitHooks` is gone.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing of the port was run.
