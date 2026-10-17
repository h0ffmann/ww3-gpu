# W3IOGO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only, with no test run (2026-10-16). Its checkpoint (ADR-0011) needs a shim, the
`W3WAVE` wiring, the hooks below filled and WW3's regtests.

- (v) Fresh review 2026-10-16: `w3iogomd.F90` preprocessed with the `flags.make` defines
  (`gfortran -E`, identical to the translator's) and recompiled with WW3's Release flags against
  the `.mod` files of the CMake build of `03adda91` in `/home/hoffmann/code/ww3-gpu-noaa/build`
  (`.text`, `.rodata`, `.rodata.cst4/8/16`, `.rodata.str1.1` identical to that build's object);
  the GIMPLE and assembly of the five `AUX` loops read against the C++ unit rebuilt with the
  `openmp-release` flags (no warning with `-Wshadow -Wconversion`, no FMA, ten `sincosf`, no
  other libm call; ASan/UBSan `-O0` build warning-free): same compares and products, and the
  operand orders of the NaN-payload ⚠ below confirmed in the inlined loops. The two ladders
  re-extracted from the preprocessed source by a separate script: 94 branches each (the
  (2,6)/(2,18) branch and the (10,*) branch count once), the same arrays and sections as the
  port's `write_field`/`read_field`, 23 intermediate `EXTIOF` tests at the same places; every
  `w3iogomd.F90` line cited here and in the header, and the `w3odatmd`, `w3adatmd`, `w3wdatmd`,
  `w3servmd`, `w3wavemd`, `ww3_outf`, `ww3_gint` ones, read at `03adda91`. Column bases checked
  against `W3DIMA`'s allocations: `US3D` and `USSP` are `(NSEALM,NK*2)` (`w3adatmd.F90`, the
  `US3DF(1)`/`USSPF(1)` blocks), so `US3DF(2)-1` and `0 .. USSPF(2)-1` with `NK` added for the
  second record; `P2SMS` and `EF` .. `STH2M` take their allocation lower bound; `USERO(NSEALM,
  NOEXTR)` column `IFJ-1`. Records re-walked with a separate script in three WW3-written files
  (`ww3_tp1.8`: 19 records of `NSEA` = 49 and `US3D` (6,8) as two of 1470; the PDLIB `work_c`
  run: partitions (4,1), (4,2), (4,4) at `NSEA·(NOSWLL+1)`; `ww3_tp2.6`: 15 records); the map
  record of `ww3_tp1.8` (`NX` = 52, `NY` = 3) read as `NY` rows of `NX` is the channel geometry
  (sea row between land rows), the IX-fastest order; in `ww3_tp2.6` (unstructured, `IX` = `ISEA`)
  the (5,1) components are `UNDEF` at exactly the 84 points with `MAPSTA` other than 1 of 1778.
  The gfortran probes rerun (IOSTAT 0, -1, 5001, 0 after REWIND, 2 for `STATUS='OLD'` on a
  missing file; `TIMETAG` asterisks) plus one more: a WRITE after the end of file gets 5001 and
  a short record 5016. One defect fixed: `MAPSF(ISEA,2)` was indexed with the current `NSEA` as
  the leading extent; the Fortran strides by the extent `MAPSF` was allocated with
  (`MAPSF(MSEA,3)`, `w3gdatmd.F90:1736`), so the port now uses `mapsf.size()/3` (the two differ
  only when `NSEA` changed after `W3DIMX`). The entry-grid contract of `AUX1`/`AUX2` is now stated
  on `W3IogoModules::grid`. The CUDA compile was not repeated.

`W3IOGO` writes and reads the gridded output file `out_grd.<ext>`: a header record, then for
each output time a record with `TIME` and the 10 × 20 `FLOGRD` flags, the packed land-sea map
`MAPSTA + 8*MAPST2`, and one or more records of `NSEA` values for every requested field. Before a
WRITE it sets the fields of land and ice points to `UNDEF`, and it writes the wind, the
atmospheric momentum flux, the friction velocity and the near-bottom excursion amplitude and
orbital velocity as Cartesian components. `W3WAVE` calls it (`w3wavemd.F90:2656`) after
`W3OUTG` at every gridded output time; `ww3_outf`, `ww3_grib`, `gx_outf` and `ww3_gint` read
with it (`ww3_outf.F90:245`, `ww3_gint.F90:823`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3IOGO`, `model/src/w3iogomd.F90:2395-4097` at NOAA-EMC/WW3 `03adda91`, identical in the `h0ffmann/WW3` fork at the `WW3/` pin `8e433f99` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3iogomd.F90` is empty, 2026-10-16), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/iogo_w3iogo.hpp`](../src/ww_kokkos/iogo_w3iogo.hpp): `ww::iogo::w3iogo<ExecSpace>` (host), the element functions `aux_polar` and `aux_ust` (`KOKKOS_INLINE_FUNCTION`), the persistent unit `W3IogoUnit`, the module state `W3IogoModules` and the host staging `W3IogoHostFields` |
| Reused | the record writer and reader of [`iogr_unformatted.hpp`](../src/ww_kokkos/iogr_unformatted.hpp); `IogrState` (W3GDATMD of the grid, with `NOSWLL` and `FNMPRE`), `FChar`, `encode_maptmp`/`decode_maptmp` and the exit codes of [`iogr_w3iogr.hpp`](../src/ww_kokkos/iogr_w3iogr.hpp); `Wadat`/`Wadats`/`W3setaTargets` of [`adat_wadat.hpp`](../src/ww_kokkos/adat_wadat.hpp) for the W3ADATMD fields |
| Callees | `W3SETA` and `W3DIMA` call their ports through `iogo_w3seta_port` and `iogo_w3dima_port` (the latter followed by the `W3SETA(IMOD)` that `W3DIMA` makes at `w3adatmd.F90:1467`, which [`adat_w3dima.hpp`](../src/ww_kokkos/adat_w3dima.hpp) leaves to its caller). `W3SETO`, `W3SETG`, `W3SETW` and `W3DIMW` are hooks (`W3IogoHooks`); a selection hook re-points the matching `W3IogoModules` pointer, as the Fortran re-points its module pointers. An empty hook on the path ends with `iogr::kExitMissingHook`, never a skipped call |
| Not ported, by switch | `W3_ASCII` (the `NDSOA` text copy, its OPENs `:2654-2657`, `:2743-2746`, and every `WRITE (NDSOA,*)`), `W3_MPI` (`W3XETA`, `:2606-2608`), `W3_SMC` (the Arctic rotation of `UD` and `TAUADIR`, `:3052-3058`, `:3097-3103`), `W3_BT4` (`SED_D50`, `:3125-3130`), `W3_IS2` (`ICEH`, `ICEF`, `:3132-3143`), `W3_SETUP` (`ZETA_SETUP`, `:3144-3150`) and their READs, `W3_T` (`NDST` prints) and `W3_S` (`STRACE`). `switch_lab_shrd` sets none of them; the compiled lines were read from `gfortran -E` with the defines of WW3's `flags.make` |
| Structural changes | The fields live in `ExecSpace` memory and are staged through host mirrors (`create_mirror_view` and `deep_copy`: the same View on a host backend, a copy on CUDA) after the READ path's `W3DIMA`, and copied back at every return after that point. The loops run on the host in the Fortran's order. The unit `NDSOG` is a `W3IogoUnit` the caller keeps between calls; the `NDSE` messages are returned in `W3IogoStatus`, not printed |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Record layout

Unformatted sequential, `CONVERT=file_endian` (`:2651`, `:2660`; `'big_endian'` from WW3's
CMake, `open_parm.convert = "big_endian"` in the GIMPLE), records as in
[W3IOGR_KOKKOS.md](W3IOGR_KOKKOS.md): a 4-byte big-endian length, the payload, the length
again. Kinds are those of the `_gfortran_transfer_*` calls of the `-O3` GIMPLE. `NSEA` is the
current `NSEA` except for the `AUX` records.

| Record | When | Items | Bytes | Lines (WRITE / READ) |
|---|---|---|---|---|
| header | the first call (`OFILES(1)` = 0) or every call (`OFILES(1)` = 1) | `IDSTR` c30 `'WAVEWATCH III GRID OUTPUT FILE'`, `VEROGR` c10 `'2019-10-04'`, `GNAME` c30, `NOGRP` (10), `NGRPP` (20), `NSEA`, `NX`, `NY` i4, `UNDEF` r4, `NOSWLL` i4 | 98 | `:2670-2672` / `:2681-2683` |
| time | every call | `TIME(2)` i4, `FLOGRD(10,20)` l4 column-major | 808 | `:2807` / `:2813` |
| map | every call | `((MAPTMP(IY,IX),IX=1,NX),IY=1,NY)` i4, `MAPTMP = MAPSTA + 8*MAPST2`: **IX fastest**, the transpose of the array's storage order | 4·NX·NY | `:2834-2835` / `:2841-2842` |
| fields | per `FLOGRD(IFI,IFJ)` set, `IFI` = 1..10 outer, `IFJ` = 1..20 inner | one record of `NSEA` r4 per array named in the WRITE; the sections below | 4·NSEA each | `:3023-4022` |

Fields with more than one record or a section: `CX`, `CY` (1,2); `AUX1`, `AUX2` (1,3), (1,8),
(5,1), (7,1), (7,2); `TAUWIX/Y` (5,5), `TAUWNX/Y` (5,6); `SXX`, `SYY`, `SXY` (6,1); the pairs of
(6,2), (6,5), (6,6), (6,7), (6,13), (8,1), (8,2); `TAUICE(:,1)`, `(:,2)` (6,10); `BEDFORMS(:,1..3)`
(7,3); `TAUBBL(:,1..2)` (7,5). Sections, `ISEA` fastest: `EF`..`STH2M(1:NSEA,E3DF(2,k):E3DF(3,k))`
(3,1-5); `WN(1:NK,1:NSEA)`, `IK` fastest (3,6); `PHS`..`PEP(1:NSEA,0:NOSWLL)` (4,1-15);
`US3D(1:NSEA,US3DF(2):US3DF(3))` then `US3D(1:NSEA,NK+US3DF(2):NK+US3DF(3))` (6,8);
`P2SMS(1:NSEA,P2MSF(2):P2MSF(3))` (6,9); `USSP(1:NSEA,1:USSPF(2))` then
`USSP(1:NSEA,NK+1:NK+USSPF(2))` (6,12); `USERO(1:NSEA,IFJ)` (10,*). (2,6) and (2,18) both write
`FP0`. A flag no branch matches ((1,10)-(1,13) under this switch, (2,20), ...) writes nothing.

- (v) Kinds and order: every `_gfortran_transfer_*` call of the `-O3` GIMPLE of `W3IOGO`
  (gfortran 16.2.0, WW3's Release flags `-O3 -fallow-argument-mismatch -g -fno-second-underscore
  -ffree-line-length-none`, the `switch_lab_shrd` defines from `flags.make` of a WW3 CMake build of
  `03adda91`), read 2026-10-16: `transfer_character_write` 30, 10, 30, `transfer_integer_write`
  ×5, `transfer_real_write` `undef`, `transfer_integer_write` `noswll` for the header; integer then
  logical kind 4 for the time record; every field item `transfer_real(_write)` or
  `transfer_array(_write)` of kind 4. The object's `.text`, `.rodata`, `.rodata.cst4/8/16` and
  `.rodata.str1.1` are byte-identical to the object of that CMake build; `.rodata.str1.8` differs
  only by the source path in the runtime-error strings (diffed).
- (v) The map order: the object makes one `_gfortran_transfer_array_write` per `IY` of the row
  `MAPTMP(IY,1:NX)` with stride `NY` (GIMPLE of `:2835`). Checked on files WW3 wrote, with a
  scratch script that compares the map record of `out_grd.ww3` with the `MAPTMP` record of the
  `mod_def.ww3` beside it (written by `W3IOGR` in storage order, `w3iogrmd.F90:878-879`),
  2026-10-16: in 253 run directories under `build/` of the worktrees the out_grd map is the
  transpose (IX fastest) and in none is it in storage order; 674 more have a symmetric map, and in
  54, all with `NY` = 1 (unstructured grids), the values differ from the model definition's.
- (v) Record lengths: a scratch script predicts each output time's records from its `FLOGRD` by
  this table and walks the record markers of every non-empty `out_grd.*` under `build/` of the
  worktrees, 1448 files written by WW3 (the `ww3_ts1` smoke and `bench_small` runs and the
  regtest matrix runs, of several switches), 2026-10-16: in all of them the header is 98 bytes,
  every time record 808, every map record 4·NX·NY, and every output time's field records match in
  number and length. The flags seen cover 49 (IFI,IFJ) pairs, among them the two-record fields
  (1,2), (1,3), (1,8), (5,1), (5,5), (7,1), (7,2), (8,1), the three-record (6,1), the partition
  fields (4,1), (4,2), (4,4) at 4·NSEA·(NOSWLL+1) bytes, and `US3D` (6,8) as two records of
  4·49·30 bytes in `ww3_tp1.8`. ⚠ No file has (3,*), (6,9), (6,12) or `USERO` (10,*); those
  layouts rest on the GIMPLE and the source only.
- (v) `TIMETAG` (`OFILES(1)` = 1) is `WRITE(TIMETAG,"(i8.8,'.'i6.6)") TIME(1), TIME(2)`
  (`:2734`): a standalone gfortran 16.2 program gave `20200101.010000` for (20200101, 10000) and
  asterisks for a negative value or one with too many digits in either field; `fortran_iwm`
  reproduces it.
- (v) The log line of `OFILES(1)` = 1 (`:4040-4041`) is list-directed: a leading blank, the
  string, a newline, no wrap at 210 characters (the same program, `od -c`).
- (v) libgfortran behaviour measured with that program built with WW3's flags: a READ of
  `TIME, FLOGRD` past the last record gives IOSTAT -1, the next READ 5001, and a REWIND clears it;
  `OPEN(..., STATUS='OLD')` of a missing file gives IOSTAT 2 (errno `ENOENT`); a failed
  `ALLOCATE` without `STAT` prints "Error allocating" and exits 1. `kIostatAfterEndfile`,
  `kExitAllocate` and the EXTOPN code follow.
- ⚠ The other gfortran behaviours are those W3IOGR's review settled for `mod_def.ww3`
  (subrecords, `regular_file2` fallbacks, short records, `GFORTRAN_CONVERT_UNIT` ignored, a full
  disk surfacing at a buffer flush), reused here unchanged and not measured again for this file.

## Arithmetic

The reference is the `-O3` object above, read as GIMPLE (`-fdump-tree-optimized-lineno`) and
assembly (`-fverbose-asm -S`); the C++ is a throwaway unit with the `openmp-release` flags read
with `objdump -dr`. 2026-10-16.

- (v) `AUX1 = UA*COS(UD)`, `AUX2 = UA*SIN(UD)` when `UA .NE. UNDEF` (`:3059-3064`), and the same
  for `TAUA`/`TAUADIR` (`:3104-3110`), `ABA`/`ABD` (`:3564-3570`), `UBA`/`UBD` (`:3584-3590`):
  in the Fortran object a `ucomiss` against `UNDEF` reloaded for every point (a NaN amplitude
  takes the first branch), a `cosf` call, one `mulss`, a `sinf` call, one `mulss`; no `sincosf`,
  no libmvec, no vectorisation of these loops. `aux_polar` compiles to the same compare and
  branch sense (`vucomiss`, `jp`/`jne` to the product), one `sincosf` and two `vmulss`; no FMA in
  the object (`objdump`: no `vfmadd`/`vfmsub`).
- (v) `AUX1 = UST*ASF*COS(USTDIR)` where `MAPSTA(IY,IX) .EQ. 1` (`:3375-3383`): `(UST*ASF)*cosf`
  in both, with `UST` first and the product first; the Fortran recomputes `UST*ASF` for the SIN
  line, the C++ keeps one product, the same value.
- (v) glibc's `sincosf` returns the bits of `sinf` and `cosf` for every float (the exhaustive run
  of [W3OUTG_KOKKOS.md](W3OUTG_KOKKOS.md), glibc 2.44, the glibc of the pinned shell), so g++
  merging the two calls does not change a bit. ⚠ For glibc 2.44 only.
- ⚠ NaN payloads. The Fortran object multiplies with the `cosf`/`sinf` result as the first
  operand (`mulss %xmm1, %xmm0`, destination the libm result) for the four `aux_polar` pairs; the
  C++ object has the amplitude first for the cos product and the libm result first for the sin
  product, and for `aux_ust` the libm result first for the sin product. g++ chose these orders
  whichever way the source wrote the product (tried both). Bits differ only when both operands are
  NaN with different payloads: a NaN amplitude with a NaN or infinite direction.
- (v) `MAPTMP = MAPSTA + 8*MAPST2` (`:2833`), `MAPSTA = MOD(MAPTMP+2,8) - 2`, then `MAPST2 =
  (MAPTMP-MAPSTA)/8` with the new `MAPSTA` (`:2844-2845`): `% 8` and `/ 8` truncating toward zero
  in the GIMPLE; `iogr::encode_maptmp`, `iogr::decode_maptmp`.
- Everything else is a copy or a store of `UNDEF`, loaded from the module variable.

## Behaviour reproduced as compiled

- (v) `AUX1(NSEA)` and `AUX2(NSEA)` are automatic arrays (`:2577`) whose bound the object
  evaluates at entry, before `W3SETO` (`:2603`) and `W3SETG(IGRD)` (`:2604`) re-point `NSEA`:
  `malloc` of `4*NSEA` first in the GIMPLE, and the `AUX1` record's `ubound` is that entry value
  while the loop runs to the current `NSEA`. A caller that switches grids (`ww3_gint` writes grid
  `NG` with `IMOD`, `ww3_gint.F90:3527`) gets records of the previous grid's length. The port sizes
  `aux1`/`aux2` from `m.grid` at entry; a loop past that size ends with `kExitShape`.
- (v) A READ puts the Cartesian components the WRITE stored into `UA`, `UD` (`:3737-3739`),
  `TAUA`, `TAUADIR`, `UST`, `USTDIR` (`:3865-3867`), `ABA`, `ABD`, `UBA`, `UBD`: the file holds
  components, the READ names them as amplitude and direction. Reproduced.
- (v) The WRITE's initialization sets `UST(ISEA)` and `USTDIR(ISEA)` to `UNDEF` at every point with
  `MAPSTA < 0`, whatever the flags (`:2866-2867`), and changes the model's fields, not copies.
  A scratch script compared the 93 flag/array pairs of the land block (`:2855-2985`) and the 15 of
  the ice block (`:2987-3013`) with the C++: equal.
- (v) A READ overwrites the grid's `NSEA`, `NX`, `NY` and the global `UNDEF` from the header
  (`:2681-2683`) before any check, and the model's `TIME` and `FLOGRD` from the time record.
- (v) The control flow tests `WRITE1`, not `INXOUT`: after a first READ call (`OFILES(1)` = 0) a
  `'WRITE'` request passes the mixed-mode test (`:2622`, `WRITE1` false) and reads. Reproduced.
- (v) With `OFILES(1)` = 1 a READ opens `out_grd.<ext>`, not the time-tagged name, and rewinds it
  on every call (`:2748-2753`), so it always reads the first output time. Reproduced.
- (v) The READ ladder (`:3730-4008`): 94 branches, each reading the arrays the WRITE ladder writes
  (scratch script over the preprocessed source and the header, 94 = 94 in both ladders, the
  (2,6)/(2,18) branch and the (10,*) branch counted once, the array names equal); 23 intermediate `IF (IERR.NE.0) CALL EXTIOF` in both; the final test of `:4009`
  sees, for a flag no branch matches, the `IERR` of the last statement.
- `WRITE1` is never initialised by `W3NOUT` (`w3odatmd.F90:704` sets only `IPASS1`), so its first
  value is undefined in the Fortran; `W3IogoOutput::write1` starts at 0 ⚠.
- Exit codes: 1 (FORMAT 900, `INXOUT`), 2 (901, mixed READ/WRITE), 20-22 and 24 (902-904, 906),
  41 (`EXTOPN`), 42 and 43 (`EXTIOF` with `IERR` < 0 and > 0, `w3servmd.F90:910-923`); FORMAT 905
  (`GNAME`) is a warning the Fortran prints and goes on. Codes the Fortran cannot return:
  `kExitGfortranRuntime` (2, a WRITE without `IOSTAT` failing), `kExitAllocate` (1),
  `kExitMissingHook`, `kExitShape` (an array whose extent the Fortran would overrun),
  `kExitUnassociated` (a module pointer never set), `kExitUnitNotConnected`.

## Not ported

- The `#ifdef` branches above, `print`s of `W3_T`, and the `NDSOA` text file.
- ⚠ gfortran connects an unconnected unit to `fort.<n>` on the first transfer; the port returns
  `kExitUnitNotConnected` instead (reachable only with `OFILES(1)` other than 0 or 1, or a unit
  closed by someone else between calls).
- ⚠ An `OPEN` of a file already connected to another unit fails in libgfortran; the port cannot
  see other units. An `OPEN` of the same file on the same unit keeps the connection in gfortran and
  is a close and reopen here; every such `OPEN` in `W3IOGO` is followed by a `REWIND`, so the
  position is the same.
- ⚠ When `NSEA` shrank since entry, the Fortran writes `AUX1(NSEA+1:)` as left by `malloc`
  (undefined); the port writes 0 there.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing of the port was run. Test code was run, not the port: a standalone gfortran
program for the format, list-directed, IOSTAT and allocation facts above, and scratch Python
scripts that read WW3-written `out_grd.ww3` files and the sources. Compiled, 2026-10-16 (v), in the
pinned `#ww3` shell (gfortran and g++ 16.2.0, Kokkos 5.2.2 OpenMP):

- a throwaway unit that includes the header and explicitly instantiates `w3iogo` on
  `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, `iogo_w3seta_port` and
  `iogo_w3dima_port`, with `g++ -std=c++20 -O3 -march=x86-64-v3 -fopenmp -Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -ffp-contract=off`: no warning, no `vfmadd`/`vfmsub`, ten `sincosf` calls
  and no other libm call;
- the same unit at `-O0 -g -fsanitize=address,undefined`: no warning;
- a copy of `wave_support.cpp` with `#include "iogo_w3iogo.hpp"` and `(void)&ww::iogo::w3iogo<E>;`
  added, with the openmp-release flags: no warning from this header;
- the throwaway unit in the `#cuda` shell with `nvcc_wrapper -std=c++20 -O3 -arch=sm_89
  --fmad=false --expt-extended-lambda -Xcompiler -ffp-contract=off -Xcompiler -fopenmp` against
  `kokkos-cuda-5.2.2` (CUDA 12.9), `w3iogo` on `Kokkos::Cuda` through the host mirrors: no warning,
  compile only. ⚠ The `wave_support.cpp` copy does not compile with `nvcc_wrapper`, nor does the
  file as it is in the tree: `ic3_wn_cmplx_hf.hpp`, `profs_dlassq.hpp` and `tide_dpythag.hpp` use
  `__builtin_assoc_barrier`, which nvcc's front end does not know.

The `serial-debug`, `openmp-release` and `cuda-release` presets of `kokkos/` were not built.
