# DO_OUTPUT_EXCHANGES_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim that fills the hook
table and the W3WAVE hunk at `model/src/w3wavemd.F90:2560`.

- (v) Fresh review 2026-10-16: own CMake Release build of `pdlib_field_vec.F90` under `switch_PDLIB` (its disassembly identical to a `-fdump-tree-optimized` recompile, 7266 instructions, no floating-point arithmetic) and its `-O3` GIMPLE and tree-original; a mechanical diff of the 98 copies of the packing, the unpacking and `walk_columns` (identical); the MPI calls, tags, counts, handles and ranks; the bounds of every 2-D field; `W3XDMA`'s `XUSERO`. Defects found and fixed: the packing (one launch per column) and the scatter (one thread per row walking `I`) had interchanged the Fortran's loops, which phase 1 allows only where a kernel may not allocate (`docs/AGENTS_KOKKOS_202609.md` §1.5); both now keep the Fortran's nesting. Citations corrected: `GET_ARRAY_SIZE` is called at `:877`, `INIT_GET_ISEA` at `:881`, `STH2M` is allocated at `w3adatmd.F90:1104-1105`. The MPI datatypes are a symbolic enum the shim maps; the `MPI_INTEGER` data receive is kept. `int` for `TheSize` agrees with the `std::int32_t` of `pdlib_get_array_size.hpp` (4-byte `INTEGER`).

Under `PDLIB` each computing process holds the gridded output fields (Hs, mean periods, the
partition fields, stresses, the extra fields, ...) of its own sea points only. Before a gridded
output `W3WAVE` calls `DO_OUTPUT_EXCHANGES` (`model/src/w3wavemd.F90:2560`), which packs the
requested fields into one `REAL` array per process, sends it with the global sea index of every
column to the field output process `NAPFLD`, and there stores the columns, by global sea point,
into the full-grid output arrays that `W3XETA` points the module at. The routine copies `REAL`
words; it does no arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `DO_OUTPUT_EXCHANGES`, `model/src/pdlib_field_vec.F90:784-1733` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it (`DIST MPI PDLIB SCOTCH`, no `S`). Its profile row is `docs/data/ww3_pdlib_gprof.tsv:78`, 0.00 s: no measured cost |
| Kokkos code | [`../src/ww_kokkos/pdlib_do_output_exchanges.hpp`](../src/ww_kokkos/pdlib_do_output_exchanges.hpp), `ww::pdlib::do_output_exchanges<ExecSpace>`: one packing launch over `JSEA` (`ARRpos(JSEA)`, then the columns in order), one scatter launch over `I` per process in the Fortran's process order, one unpacking launch per column |
| Callees | `INIT_GET_ISEA` (`:881`) through `ww::parall::init_get_isea` of [`parall_init_get_isea.hpp`](../src/ww_kokkos/parall_init_get_isea.hpp), with `DIST` and `PDLIB` set. Not translated, behind `OutputExchangeHooks`: `GET_ARRAY_SIZE` (`:877`, its own row in `kokkos/port_claims.tsv`), the six MPI calls (`:1303-1305`, `:1311-1315`), the read of `WADATS(IMOD)%AINIT2`, `W3XDMA`, `W3XETA` (`:1324-1325`), `W3SETA` (`:1729`), and the views of the X arrays after `W3XETA` (`output_fields`). A hook needed on a path and left null aborts |
| Not ported, by switch | none: the body has no preprocessor branch; `W3_S` only declares `IENT` (`:854-856`), which the body never uses |
| Not computed | `IT0 = NSPEC` and `IROOT = NAPFLD - 1` (`:874-875`): nothing reads them |
| Structural change | None: the loops keep the Fortran's nesting (`DO JSEA` over the columns, `DO I` over the rows, column by column in the unpacking). The two `IF` chains (packing `:883-1286`, unpacking `:1326-1728`) are carried by one walker, `walk_columns`, which runs inside the packing kernel and on the host for the unpacking |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic and order

Reference: the `-O3` object of `pdlib_field_vec.F90` compiled with the flags of WW3's CMake
Release build under `switch_PDLIB` (gfortran 16.2.0, pinned `ww3` shell), whose disassembly is
identical to the CMake object's, read as GIMPLE (`-fdump-tree-optimized`) and assembly. The C++
is a throwaway unit with the `openmp-release` flags, read as assembly. 2026-10-16.

- (v) No arithmetic in either object: the Fortran `DO_OUTPUT_EXCHANGES` and the C++ unit contain
  no `add`, `sub`, `mul`, `div`, `min`, `max`, `sqrt`, conversion or FMA instruction. Each element
  is one 4-byte load and store, so the interchange of the packing loops cannot change a bit:
  every element of `ARRexch` is written once from one source word. The kernels keep the Fortran's
  loop nesting all the same (phase 1 rule).
- (v) The column order: a mechanical diff of the field and loop sequence of the packing
  (`:883-1286`) against the unpacking (`:1326-1728`) finds them identical, and `GET_ARRAY_SIZE`
  (`:53-393`) counts the same columns in the same groups (read, not run). `FLGRDALL(2,18)` is not
  exchanged (`:948-955`). Group 3 loops `IK = E3DF(2,k)..E3DF(3,k)`, group 4 `0..NOSWLL`,
  `US3D`/`USSP` `1..2*NK`, `P2SMS` `P2MSF(2)..P2MSF(3)`, `USERO` `I = 1..NOEXTR` (`NOEXTR = 2`,
  `w3odatmd.F90:335`).
- (v) Index maps. `ARRpos(JSEA) = ISEA` from `INIT_GET_ISEA` (`:880-882`), under `PDLIB` with an
  unstructured grid `ISEA = IPLG(JSEA)` (`w3parall.F90:1493`). Fields are packed by `JSEA`, except
  `UST`, `USTDIR`, `ASF`, packed by `ISEA` (`:1084-1090`) from `W3WDATMD`'s `UST(0:NSEATM)`,
  `USTDIR(0:NSEATM)`, `ASF(NSEATM)` (`w3wdatmd.F90:570-572`); the C++ keeps those bases. The 2-D
  fields keep their lower bounds as a 0-based column (`w3adatmd.F90:1084-1103`, `:1119`, `:1211`,
  and the X arrays at `:1752-1790`, `:1798`, `:2115`).
- (v) Scatter order. `ARRtotal(:,ARRpos(I)) = ARRexch(:,I)` is one `memcpy` per `I` in the
  object, own columns first (`:1295-1299`), then `IPROC = 1..NAPROC` skipping `IAPROC`
  (`:1309-1321`). The C++ launches one kernel over `I` per process, in that order, on one
  execution space instance. Within one process the positions are distinct: `IPLG` lists this
  rank's nodes, then ghosts that belong to other domains, with no repeat (WW3 stops on one)
  (`PDLIB/yowpdlibmain.F90:947-1012`), and `NSEAL` counts the wet nodes of all `NPA`, ghosts
  included (`w3profsmd_pdlib.F90:276-280`, `w3parall.F90:1122`). So a ghost repeats another
  process's point, and the column of the last process in `IPROC` order is kept, in both. Read,
  not run.
- (v) MPI, in order. Non-root computing process: `MPI_SEND` of `NSEAL` (1 `MPI_INTEGER`, tag 23),
  `ARRpos` (`NSEAL` `MPI_INTEGER`, tag 29), `ARRexch` (`NSEAL*TheSize` `MPI_REAL`, tag 37), all
  to rank `NAPFLD-1` (`:1303-1305`). Root: for each other `IPROC`, `MPI_RECV` of the count, the
  positions and the data from rank `IPROC-1` with tags 23, 29, 37 (`:1311-1315`). In the object
  the datatype handles are 7, 7, 13 on the sends and 7, 7, 7 on the receives (Open MPI 5.0.11
  `MPI_INTEGER = 7`, `MPI_REAL = 13`, `mpif-handles.h:188,220`); the tags in the tree-original
  dump are 23, 29, 37 and the count 1. The C++ names the datatype by the symbolic
  `OutputMpiDatatype`, which the shim maps to its own `MPI_INTEGER`/`MPI_REAL`; no handle value
  is in the code. The data receive at `:1315` uses `MPI_INTEGER` for `REAL`
  words; the C++ passes `kOutputMpiInteger` there so the shim keeps it.
- (v) Root sequence (`:1323-1729`): `W3XDMA(IMOD, NDSE, NDST, FLGRDALL)` only when `AINIT2` is
  false, then `W3XETA`, the unpacking `X(1:NSEA) = ARRtotal(IH,:)` field by field, then `W3SETA`.
  `FLGRDALL` reaches `W3XDMA` as the same column-major `LOGICAL(4)` array.
- (v) Module state: `NRQGO = NRQGO2 = 0` on every call (`:872-873`); the `SAVE` counter
  `indexOutput` is incremented on every path (`:1732`; the object keeps the store on each of its exits),
  and nothing reads it.
- ⚠ `ARRtotal` is allocated without a value (`:1294`, `malloc` in the object); the C++ allocates it
  without initialising. A column no process sends is undefined in both. Whether every sea point is
  covered was not checked, nor whether a ghost's output value equals its resident's (the kept
  column is the last process's, which may be a ghost).
- ⚠ The data receive with `MPI_INTEGER` for a buffer sent as `MPI_REAL` (`:1305`, `:1315`) is a
  type-signature mismatch for the MPI standard; on one host Open MPI copies the bytes. Read, not run.
- (v) Seen in a callee, not in this routine (WW3 issue, read 2026-10-16): `W3XDMA` allocates `XUSERO` inside `DO I=1,NOEXTR`
  (`w3adatmd.F90:2363-2371`), so its final shape is `(NXXX,2)` or `(1,2)` by `FLGRDALL(10,2)`
  alone; with `(10,1)` set and `(10,2)` unset, `USERO(1:NSEA,1) = ...` (`:1723-1728`) writes out of
  bounds. Confirmed by reading: `XUSERO` is a `POINTER` (`w3adatmd.F90:497`), so the second
  `ALLOCATE` succeeds and replaces the first. Kept, not fixed. ⚠ Not run.
- (v) CUDA compiles (sm_89, `--fmad=false`, compile only); its kernels for this routine have no
  floating-point instruction in the SASS. ⚠ Not run: the X arrays the shim wraps must be device
  memory for a CUDA run; with no shim yet, that is not shown.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. The only program built was the throwaway unit
`/tmp/claude-1000/dooutx-scratch/tu/tu.cpp` (compiled, not linked or run): `g++ -O3
-march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp` against Kokkos
5.2.2 (OpenMP) in the pinned `ww3` shell, 0 warnings, 0 with `-Wconversion` from the header, no FMA.
The fresh review rebuilt its own unit after its fixes, with explicit instantiations on
`Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`: the same `g++` flags, 0 warnings,
0 `-Wconversion` warnings from the header, no FMA or scalar float arithmetic in the object; and
in the pinned `cuda` shell `nvcc_wrapper -O3 -std=c++20 -arch=sm_89 --fmad=false
-extended-lambda -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp` against Kokkos 5.2.2
(CUDA, ADA89), 0 warnings (`/tmp/claude-1000/dooutx-review/tu/`, compiled, not linked or run).
