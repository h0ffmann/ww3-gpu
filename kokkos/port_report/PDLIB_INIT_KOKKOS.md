# PDLIB_INIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim that fills the hook
table (PDLIB library, MPI, the list-directed writes) and the `W3INIT` hunk first.

- (v) Fresh review 2026-10-16: an independent reviewer rebuilt WW3's CMake Release object of
  `w3profsmd_pdlib.F90` under `switch_PDLIB` (gfortran 16.2.0, `-fdump-tree-optimized` and
  `-fdump-tree-original`) and checked against its GIMPLE the call sequence (communicators, count
  1, tags 23 and 24, ranks `IPROC-1` and 0, `secDim` by `FSTOTALEXP`), the `stat=` and plain
  `ALLOCATE`s, `MAXVAL`'s start value, `FLCY` read only under `FLCX`, the `COFRM4` divisor, power
  tree and folded constant (`static_assert`ed in the C++), and every line citation. Two defects
  fixed: the routine aborted or relied on non-returning `pdlib_abort` and `stop` hooks where the
  Fortran stops (now it returns a code, as `BLOCK_SOLVER_INIT` does), and the `IS0_pdlib` stride
  loop ran once for `NTH < 0` and `NSPEC > 1`, where the object's trip count runs none.

The set-up of the PDLIB domain decomposition of one unstructured grid: the call of the PDLIB
library's `initFromGridDim`, which partitions the mesh; the number of wet nodes of this rank
`PDLIB_NSEAL`; the maps `JX_TO_JSEA` (local node to local sea point) and `ISEA_TO_JSEA` (global
sea point to local sea point); `PDLIB_NSEALM`, the maximum of `PDLIB_NSEAL` over the computing
ranks, gathered on rank 1 and sent to every process; `FSGEOADVECT`; and two tables of the
implicit scheme, `COFRM4(IK) = COEF4*GRAV/FR(IK)**4` and the spectral offsets `IS0_pdlib(NSPEC)`.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_INIT`, `model/src/w3profsmd_pdlib.F90:143-399` at h0ffmann/WW3 `8e433f99`, text identical at NOAA-EMC/WW3 `03adda91` (`git diff 03adda91 8e433f99 -- model/src/w3profsmd_pdlib.F90` is empty), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it |
| Kokkos code | [`../src/ww_kokkos/pdlib_pdlib_init.hpp`](../src/ww_kokkos/pdlib_pdlib_init.hpp), `ww::pdlib::pdlib_init`, host code |
| C shim | none yet; every call that stays outside is a hook of `PdlibInitHooks` |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

Ported: every statement `switch_PDLIB` compiles (it has no `S`, `DEBUGSOLVER` or `DEBUGINIT`).
Not ported, compiled only under other switches: `W3_S` (`:188-190`, `:220-222`, `:238-240`),
`W3_DEBUGSOLVER` (`:201-203`, `:208-210`, and the `WRITE`/`FLUSH` blocks at `:241-246`,
`:254-259`, `:267-270`, `:281-288`, `:290-293`, `:308-311`, `:325-328`) and `W3_DEBUGINIT`
(`:363-367`).

Behind the hook table, in the object's order: `MPI_COMM_RANK` (`:252`), `initFromGridDim`
(`:262`, `:264`; `PDLIB/yowpdlibmain.F90:65-174`), the read of `YOWNODEPOOL`'s `npa` and `iplg`
it sets, the two `WRITE`s of `:321-322`, `MPI_RECV` and `MPI_SEND` with tags 23 and 24 (`:342`,
`:349`, `:356`, `:359`), `ComputeListNP_ListNPA_ListIPLG` (`:369`;
`PDLIB/yowfunction.F90:198-281`), and the two `WRITE`s of `:389-390`. The MPI communicators and
the datatype are enums the shim maps to its library's handles; no handle value is written in the
C++.

Where the Fortran stops, `pdlib_init` returns a code and the shim stops, as for `W3DIMS`,
`W3IOGR` and `BLOCK_SOLVER_INIT`: 3 and 20 are the `istat` of `PDLIB_ABORT` (`:294`, `:391`;
`PDLIB/yowfunction.F90:47-53`, ending in the `STOP` of `PDLIB/yowerr.F90:159`), which the shim
calls with it; `kPdlibInitStop` (-1) is the bare `STOP` of `:323`, which the object compiles to
`_gfortran_stop_string(0, 0, 0)` (exit status 0, hence a code other than 0); -2 to -6 are
gfortran's run time errors of the `ALLOCATE`s without `stat=` (`:339`, `:370`, `:376`), each
naming its variable and line. Only a missing hook aborts.

## Arithmetic and structure

The reference is the `-O3` object of `w3profsmd_pdlib.F90` from WW3's CMake Release build
(`-O3 -fallow-argument-mismatch -g -fno-second-underscore -ffree-line-length-none -DSCOTCH_707`,
gfortran 16.2.0, `switch_PDLIB`, the pinned `picasso/labs/pratico#ww3` shell), rebuilt by the
translator with `-fdump-tree-optimized` to a disassembly identical to the CMake build's, and read
as GIMPLE by the translator, 2026-10-16.

- (v) One floating-point expression, `:372-374`. The object computes `efr = sig / 6.2831854820251465f`
  (`TPI`, `constants.F90:72-73`), `efr2 = efr*efr`, `efr4 = efr2*efr2` (the power tree of
  `eFR**4`) and `4.90299999000853858888149261474609375e-6f / efr4` (`COEF4*GRAV` folded to one
  `REAL`, `:237`, `constants.F90:62`). The C++ GIMPLE has the same divisor, the same two squarings
  and the same folded constant; `divps`/`mulps` in both objects, no FMA in either.
- (v) Call sequence: the C++ GIMPLE calls the hooks in the order of the Fortran object's calls:
  `mpi_comm_rank`; `init_from_grid_dim` with `secDim = NTH` when `FSTOTALEXP`, `NSPEC` otherwise;
  return 3 only when an item of `:289` fails, before any zero-fill; `write_nb_error` then return
  -1; `mpi_recv(src IPROC-1, tag 23)` per rank on rank 1, `mpi_send(dest 0, tag 23)`
  elsewhere; `mpi_send(dest IPROC-1, tag 24)` for IPROC = 2..NTPROC on rank 1, `mpi_recv(src 0,
  tag 24)` on every other process (I/O processes included, `:353-361` is outside the
  `IAPROC <= NAPROC` test); `compute_list_np_npa_iplg`; `write_isea_error` then return 20.
- (v) `FLCY` is read only when `FLCX` is set (`:331`), as in the object.
- (v) `ALLOCATE(JX_TO_JSEA(npa), ISEA_TO_JSEA(NSEA), stat=istat)` (`:289`): the object tests each
  item's data pointer in list order and skips the rest on the first failure; extents are
  `MAX(n,0)`. `ALLOCATE(COFRM4(NK))` and `ALLOCATE(IS0_pdlib(NSPEC))` (`:370`, `:376`) have no
  `stat=`: the object stops with gfortran's "already allocated" run time error
  (`_gfortran_runtime_error_at`) or its allocation OS error (`_gfortran_os_error_at`); the C++
  returns -2/-3 and -5/-6. `NSEAL_arr(NAPROC)` (`:339`) is a local and is never tested for
  allocation in the object, only for a failed `malloc` (-4); it is a `std::vector` scoped to the
  branch (its `DEALLOCATE`, `:346`).
- (v) `MAXVAL` of `INTEGER` (`:345`) starts at `-2147483648` in the object; integer `MAX` gives
  the same value in any order, so the object's vectorised reduction and the C++ loop agree.
- (v) The JSEA loop (`:298-306`) carries its counter from node to node and stays serial; every
  loop of the C++ is serial in the Fortran's order. The `IS0_pdlib` stride loop (`:380-382`) uses
  the object's trip count in unsigned arithmetic, both branches: `(NSPEC-1)/NTH + 1` when
  `NTH >= 0` and `NSPEC >= 1`, `(1-NSPEC)/(-NTH) + 1` when `NTH < 0` and `NSPEC <= 1`.
- (v) Zero warnings with `-O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic
  -ffp-contract=off -fopenmp`, and none with `-Wconversion -Wsign-conversion` added (fresh
  review, 2026-10-16); `objdump -d` of that object has no `vfmadd`/`vfmsub`/`vfnmadd`.
- ⚠ `NTH = 0` in the stride loop divides by zero in the object before its `NSPEC` test and in
  the C++ after it (only when `NSPEC >= 1`); not a reachable grid, not compared further.
- ⚠ `IMOD` is unused in the body (`:234`); the shim must still select the grid whose `MAPFS`,
  `SIG`, `TRIGP` and flags it passes (`W3SETG`), which is not checked (no shim yet).
- ⚠ The partitioner sits inside `initFromGridDim`: under `SCOTCH` and `SCOTCH_707` its
  `runParmetis` (`PDLIB/yowpdlibmain.F90:422-755`) calls `SCOTCHFParMETIS_V3_PartGeomKway`
  (`:679`; the `#ifdef` read, the library not). Its result enters the C++ only as `npa` and
  `iplg`.
- ⚠ The return codes have no shim to print gfortran's messages and stop with them yet.

## Benchmarks

None. No timing without parity (`AGENTS.md`). The PDLIB profile gives the routine 0.00 s of self
time (`docs/data/ww3_pdlib_gprof.tsv`, `just profile-case pdlib`): it runs once per grid at
initialisation.

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16:

```bash
# in nix develop "path:picasso/labs/pratico#ww3"
cmake -S WW3 -B /tmp/claude-1000/pi-scratch/ww3 -DCMAKE_BUILD_TYPE=Release \
      -DSWITCH=/tmp/claude-1000/pi-scratch/switch      # a copy of ww3_tp2.17/input/switch_PDLIB
cmake --build /tmp/claude-1000/pi-scratch/ww3 --target ww3_lib -j 8
gfortran <flags.make> -fdump-tree-optimized -c WW3/model/src/w3profsmd_pdlib.F90   # objdump identical
g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp \
    -I kokkos/src/ww_kokkos -I <kokkos-openmp-5.2.2>/include -fdump-tree-optimized -c tu.cpp
```

`tu.cpp` includes the header and calls `pdlib_init` out of line; it was compiled, never linked
or run.
