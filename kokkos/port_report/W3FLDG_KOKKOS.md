# W3FLDG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim,
a port or a hook of `W3FLDO` that opens the unit, and the `ww3_shel` wiring first.

- (v) Fresh review 2026-10-16: WW3's CMake rebuilt from the pin under `switch_lab_shrd` in
  `/tmp/claude-1000/fldg-review/ww3build`, `w3fldsmd.F90` recompiled with its `flags.make` plus
  `-fdump-tree-optimized-lineno` (disassembly and `.text`, `.rodata*` identical to the CMake
  object); every READ/WRITE of the GIMPLE (kinds, descriptors, `flags = 32`, the `*ierr` reload
  after each WRITE), the `DSEC21` compares, `TICK21`'s `.rodata` 1.0 and the `MX`-wide `memcpy`
  of `:1351` read against the port; every `w3fldsmd.F90` and `ww3_shel.F90` citation checked. Two
  gfortran 16.2 probes built with WW3's flags (`/tmp/claude-1000/fldg-review/probe/fmt.f90`,
  `rd.f90`). One defect found and fixed: a READ call after the one that returned `IERR` -1 gets
  `IOSTAT` 5001 from libgfortran and returns 5, where the port read the end of file again and
  returned -1 (below). One citation fixed (Purpose is `:964-965`). Compiled warning-free on
  OpenMP, Serial and CUDA (below); nothing of the port was run.

Reads the next record of a WW3 input field file (`level.ww3`, `current.ww3`, `wind.ww3`,
`ice.ww3`, `momentum.ww3`, `density.ww3`, the ice and mud parameter files) until its time is past
`T0`, keeping the previous record of an interpolated field in the `0` arrays; or, for a
preprocessor, writes one record. It does no floating-point arithmetic: it moves bytes and copies
arrays, and the one comparison is of `DSEC21`'s result with zero.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLDG`, `model/src/w3fldsmd.F90:935-1474` at the `WW3/` pin `8e433f99`, byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3fldsmd.F90` is empty), as `switches/switch_lab_shrd` compiles it |
| Switch | `switch_lab_shrd`: `w3fldsmd.F90` is in every build (`model/src/cmake/src_list.cmake:11`), so the lab switch compiles W3FLDG and is the one used. It defines none of `W3_S`, `W3_T`, `W3_OASIS`, `W3_OASACM`, `W3_OASOCM`, `W3_OASICM`, the only switches inside the routine, so the PDLIB switch would compile the same code |
| Kokkos code | [`../src/ww_kokkos/flds_w3fldg.hpp`](../src/ww_kokkos/flds_w3fldg.hpp), `ww::flds::w3fldg<ExecSpace>`; the records go through [`../src/ww_kokkos/iogr_unformatted.hpp`](../src/ww_kokkos/iogr_unformatted.hpp) (included, not changed) |
| Memory | the six fields are `Kokkos::View<Real**, LayoutLeft, ExecSpace::memory_space>` of extents `(MX, MY)`, Fortran `(IX, IY)` at `(ix-1, iy-1)`; each field `READ`/`WRITE` stages the whole View through a host mirror (none when the space is host-accessible); the copies are kernels on `exec` |
| Calls not ported here | `DSEC21` and `TICK21` (`W3TIMEMD`) are hooks (`FldgHooks`), as in the `W3WAVE` and `W3FLDH` ports; the error messages go to the optional `write_ndse` hook with the text gfortran writes |
| Errors | the routine never stops; `IERR` is returned, the named constants `kFldg*` (-1, 0, 1, 2, 5, 6, 7; 3 and 4 exist but cannot be returned, see below) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Record layout

The unit is the one `W3FLDO` opens, `FORM=FORM` with `FORM = 'UNFORMATTED'`
(`w3fldsmd.F90:231`), default `ACCESS` (sequential), `CONVERT=file_endian`
(`:361-373`; `FILE_ENDIAN = ENDIANNESS`, `constants.F90:87`, `'big_endian'` from WW3's CMake,
`-DENDIANNESS="'big_endian'"` in `ww3_lib.dir/flags.make`). Each record is a big-endian 4-byte
length, the payload and the length again (the format `iogr_unformatted.hpp` documents). `W3FLDO`
writes or checks the header record first; `W3FLDG` then transfers, per call and per time:

| Rec | When | Fields, in order | Lines (WRITE / READ) |
|---|---|---|---|
| time | always | `TF0(1:2)` on WRITE, `TFN(1:2)` on READ: `i4` × 2, date `YYYYMMDD` and time `HHMMSS` | `:1209` / `:1287` |
| field 1 | `IDFLD` not `CUR`, `WND`, `WNS`, `ISI`, `TAU` (scalar) | `FA0` / `FAN(1:NX,1:NY)` `r4`, `IX` fastest | `:1217` / `:1315` |
| field 1 | `CUR`, `WND`, `WNS`, `ISI`, `TAU` (`FL2D`) | `FX0` / `FXN(1:NX,1:NY)` `r4` | `:1225` / `:1327` |
| field 2 | `FL2D` | `FY0` / `FYN(1:NX,1:NY)` `r4` | `:1232` / `:1338` |
| field 3 | `WNS` (`FLST`) | `FA0` / `FAN(1:NX,1:NY)` `r4` | `:1240` / `:1357` |

A READ call repeats time record and field records until `DSEC21(T0,TFN) > 0`, or until the time
equals `T0` on the first call of a field that is not interpolated, or until the end of the file.

- (v) Kinds, order and the conditions: every `_gfortran_transfer_array` (READ) and
  `_gfortran_transfer_array_write` (WRITE) call of the `-O3` GIMPLE of `w3fldsmd.F90`, read
  2026-10-16: kind 4 for all, each with `common.flags = 32` (`IOPARM_HAS_IOSTAT`), each field a
  rank-2 descriptor with `dim[0]` stride 1, extent `NX`, and `dim[1]` stride `MX`, extent `NY`.
  gfortran turned the implied `DO` into an array section, which libgfortran transfers as `NY`
  chunks of `NX` contiguous elements, swapping each element (`io/transfer.c`
  `transfer_array_inner`, `unformatted_read`, gcc 16.2.0 source from the nix store tarball). The
  port transfers element by element in the same order: the same bytes in the file.
- (v) The layout against files WW3 wrote, read 2026-10-16 with a scratch script
  (`/tmp/claude-1000/fldg-scratch/tools/walk.py`, which walks the record markers; nothing of the
  port ran): `wind.ww3`, `level.ww3` and `current.ww3` of
  `build/profile/pdlib/run-pdlib/work_c/` in the `ww3-gpu-42` worktree, written by `ww3_prnc`
  under the PDLIB switch on an unstructured grid (`NX` 3070, `NY` 1). Every marker is big-endian
  and matches its trailer; after a 40-byte header, `level.ww3` has 25 pairs of an 8-byte time
  record and a 12280-byte field (3070 × 4), `wind.ww3` and `current.ww3` 25 triples (time, X,
  Y). The first time decodes as `20151214 000000` and the last as `20151215 000000`, hourly.
- (v) A zero extent (`NX` or `NY` ≤ 0) transfers a zero-length record (`transfer_array_inner`
  passes `tsize` 0); the port's writer emits markers 0 and 0, and its reader reads nothing and
  skips the record.
- ⚠ The endianness conversion, `GFORTRAN_CONVERT_UNIT` and the host byte order carry the
  caveats of [`W3IOGR_KOKKOS.md`](W3IOGR_KOKKOS.md): the port ignores the environment variable
  and was compiled only on x86-64.

## Order of operations and parity

The reference is the `-O3` object of `w3fldsmd.F90`: WW3's CMake configured 2026-10-16 from the
`WW3/` pin in `/tmp/claude-1000/fldg-scratch/build` with `-DSWITCH=switches/switch_lab_shrd
-DCMAKE_BUILD_TYPE=Release` in the pinned `ww3` shell (gfortran 16.2.0), target `ww3_lib`; the
file recompiled with the same defines, includes and flags (`-O3 -fallow-argument-mismatch -g
-fno-second-underscore -ffree-line-length-none`) plus `-fdump-tree-optimized-lineno`.

- (v) The object is WW3's: `objdump -d` of the recompiled object equals that of
  `ww3_lib.dir/w3fldsmd.F90.o`, and `.text`, `.rodata`, `.rodata.str1.1`, `.rodata.cst4` and
  `.rodata.cst8` are byte-identical.
- (v) The checks of `INXOUT` (`:1118`) are `_gfortran_compare_string` (blank-padded) and of
  `IDFLD` (`:1123-1130`) `__builtin_memcmp_eq` of 3 bytes, `IDFLD` being `CHARACTER(LEN=3)`
  (`:1084`). The port pads the same way and compares the first three characters.
- (v) `FLAGSC` absent gives `FLAGSC_DEFAULT = .FALSE.` (`:1101`, `:1144-1148`); `FLAGSC` is
  never written.
- (v) `DTTST = DSEC21(T0, TFN)` (`:1375`) is followed by `ucomiss`/`setnp`/`cmovne` against 0
  for `DTTST .EQ. 0.` (`:1379`) and `comiss`/`ja` for `DTTST .GT. 0.` (`:1383`): a NaN is false
  in both. The C++ object has `vucomiss`/`setnp`/`cmovne` and `vcomiss`/`jbe` for `== 0.0f` and
  `> 0.0f`, the same tests.
- (v) `TICK21(TFN, 1.)` (`:1301`) passes the address of the REAL 1.0 (`0x3f800000`, offset 0 of
  `.rodata`); the port passes `1.0f`.
- (v) The shift (`:1182-1192`) and the fill after the loop (`:1401-1411`) are copies in the
  order `DO IX / DO IY`, `FX0`, then `FY0` when `FL2D`, then a second `DO IY` for `FA0` when
  `FLST .OR. .NOT.FL2D`. The port runs the `IX` loop as a `parallel_for` and keeps both inner
  loops: copies, so the order across `IX` does not change a bit.
- (v) `FAN(:,:) = FXN(:,:)` (`:1351`, `ISI` only) copies the whole `MX × MY` arrays, not `NX ×
  NY`: one `__builtin_memcpy` of `MX` REALs per column in the GIMPLE. The port copies `mx × my`.
- (v) The WRITE branch tests `IERR`, not `ISTAT`, after each WRITE (`:1210`, `:1218`, `:1226`,
  `:1233`, `:1241`). `IERR` is 0 there (`:1109`), so a failed WRITE is silently ignored and codes
  3 and 4 are never returned; the GIMPLE keeps the tests (it reloads `*ierr` after each library
  call). Reproduced, not corrected: the port tests its local `ierr`.
- (v) Every data transfer has `IOSTAT`, so no gfortran run-time error can stop W3FLDG; only the
  `WRITE (NDSE, ...)` of the messages has none, and those are the hook's.
- (v) The messages, FORMATs 1001-1007 (`:1431-1444`): `fldg_message` builds them as gfortran
  writes them. A standalone gfortran 16.2 program with the seven FORMATs copied verbatim
  (`/tmp/claude-1000/fldg-scratch/fmt/fmt_w3fldg.f90`) wrote, for each, an empty record, `
  *** WAVEWATCH III ERROR IN W3FLDG : ` with its trailing blank, the message line and an empty
  record; `I6` right-justifies (`  5016`, `    -1`) and gives `******` for 1234567. The port's
  text is those four records, each ended by `\n`. (v) Fresh review: a second probe
  (`/tmp/claude-1000/fldg-review/probe/fmt.f90`, the seven FORMATs diffed verbatim against
  `:1431-1444`) wrote 34 messages, `IOSTAT` 0, 5016, -1, 1234567, -99999, -100000 and `J` 3 and
  12; `tools/cmpfmt.py` rebuilt the same 34 from the string literals extracted from
  `fldg_message` and the `Iw` rule of `fortran_iw`, and the 2880 bytes match. The C++ function
  itself was not executed.
- (v) A READ after the end of file. When a READ of the time record hits the end of the file,
  `IERR` is -1 and libgfortran marks the unit `AFTER_ENDFILE` (`io/transfer.c` `hit_eof`); the
  next READ of the unit is refused before any byte is read, `IOSTAT` 5001
  (`LIBERROR_OPTION_CONFLICT`, `io/transfer.c:3357-3363`, gated by `compile_options.warn_std`).
  Measured: `rd.f90` case 1, built with WW3's flags, reads `0`, `-1`, `5001` on a file of one
  time record. So the W3FLDG call after the one that returned -1 returns 5. The port tests
  `std::feof` on the stream before the time record, which only the read that hit the end of the
  file at a record start leaves set outside the paths 5 to 7, and sets 5001. ⚠ That `ww3_shel`
  makes that second call for a non-interpolated field whose file ends before the run
  (`ww3_shel.F90:2095`, `:2119`, `TFN` left at the last record) is read from the code, not run.
- (v) Under the lab switch the routine calls nothing else: no `STRACE` (`W3_S`), no test
  writes (`W3_T`), no coupler receive (`W3_OASIS`, `:1255-1285`).
- (v) No FMA and no floating-point arithmetic in either object: `grep` of the Fortran
  disassembly of `__w3fldsmd_MOD_w3fldg` and of the C++ object for `vfmadd`/`vfmsub`/`fma`
  finds none.

### What the port does differently, and where it cannot matter

- ⚠ A READ that fails inside a record: reproduced for the `IOSTAT` and `IERR`, not for the
  array contents or the file position. libgfortran reads the bytes it gets into the array chunk
  (`NX` elements of one `IY` column) and then byte-swaps the whole chunk whatever happened
  (`read_block_direct`, then `bswap_array` in `unformatted_read`, gcc 16.2.0
  `io/transfer.c:648-800`, `:1180`); `transfer_array_inner` calls the transfer for every
  remaining chunk without testing the error, and each of those swaps its chunk too and, after a
  short record, seeks four more bytes (`next_record_r_unf`). (v) Measured with `rd.f90` (gfortran
  16.2, WW3's flags, an array 3 × 2 holding 1 to 6): a field record of 8 bytes gives `IOSTAT`
  5016, the two values read, the four others byte-swapped (`3.` reads back as `0x00004040`) and
  the file 4 bytes past the record's end; a record of 16 bytes, 5016, four values and two swapped,
  at the record's end; a file cut after 8 payload bytes, 5017, two values and four swapped; a time
  record of 4 bytes, 5016, `TFN(1)` read and `TFN(2)` swapped. The port's reader
  (`RecordReader::take`) stops at the first element that does not fit and leaves it and the rest
  unchanged, with the stream after the last byte read. Matching would need a chunk-wise reader
  beside `iogr_unformatted.hpp`'s; it was not written, because every caller stops on `IERR > 0`
  before reading the arrays or the unit again (`ww3_shel.F90:2507-2508`, `CALL FINALISE`;
  `wmupdtmd.F90:409-411`, `EXTCDE(2000)`; `ww3_prep`, `ww3_prnc`, `ww3_prtide` only WRITE).
- ⚠ A failed WRITE. The Fortran's `ISTAT` is never read (above); the port's is errno when
  `fwrite` fails. As for W3IOGR, an OS write error surfaces at the WRITE whose buffer flush
  fails, which depends on libgfortran's and stdio's buffer sizes.
- ⚠ Aliased arguments. `ww3_shel` passes one scratch array `XXX` for several of the six fields
  (for example `XXX, XXX, XXX, TI1, XXX, XXX, ICEP1`, `ww3_shel.F90:2142-2145`). The port
  completes each statement on `exec` and stages each READ through its own mirror before the next,
  so the Views may be the same View. Two different Views over overlapping memory are not
  supported. The calls at the pin alias only where the aliased arrays are never both written
  (scalar fields do not touch `FX0`/`FY0`; `CUR` and `TAU` do not copy `FA0`).
- ⚠ The unit. `NDS` is a Fortran unit number; the port takes the C stream of that file, which
  a port or hook of `W3FLDO` has to open ("rb" for READ, "wb" for WRITE) and leave positioned
  after the header record. Mixing Fortran and C I/O on the same file is not supported.

## Not ported

- `W3_S` (`STRACE`, `:1105-1107`), `W3_T` (the `WRITE (NDST, 90xx)` test prints and FORMATs
  `9000-9042`), `W3_OASIS`/`W3_OASACM`/`W3_OASOCM`/`W3_OASICM` (`:1255-1285`, `:1369-1371`, the
  `COUPL_COMM` argument): not compiled under the lab switch.
- `W3FLDO`, which opens the unit and its header record, and `DSEC21`, `TICK21`: hooks or the
  caller's.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and no number was measured.

## How it was regtested

Not yet, and nothing of the port was run. What was done, 2026-10-16 (v):

- the static parity above, from the CMake build and the recompiled object in
  `/tmp/claude-1000/fldg-scratch` (`build.sh`: `cmake` and `cmake --build --target ww3_lib`;
  `recomp.sh`: the recompilation, `objdump -d`, `objcopy` of the sections, `cmp`);
- helper programs run, none of them the port: `/tmp/claude-1000/fldg-scratch/fmt/fmt_w3fldg.f90`
  (gfortran 16.2, the seven FORMATs) and `python3 -I
  /tmp/claude-1000/fldg-scratch/tools/walk.py` on the three forcing files named above;
- a throwaway translation unit `/tmp/claude-1000/fldg-scratch/tu_w3fldg.cpp` with an explicit
  instantiation of `ww::flds::w3fldg<Kokkos::DefaultExecutionSpace>` (OpenMP, Kokkos of the
  pinned `ww3` shell), compiled with g++ 16.2.0, no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp -c tu_w3fldg.cpp
```

  The same unit also compiled with `-Wshadow -Wconversion`, and at `-O0 -g` with
  `-Wsign-conversion -fsanitize=address,undefined`, no warning. No `vfmadd`/`vfmsub` in the
  `-O3` object. It was never linked or run.
- the fresh review's own unit `/tmp/claude-1000/fldg-review/tu/tu_w3fldg.cpp`, explicit
  instantiations on `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, with the same
  command, with `-Wshadow -Wconversion`, and at `-O0 -g -Wsign-conversion
  -fsanitize=address,undefined`: no warning, no `vfmadd`/`vfmsub`; `vucomiss`/`setnp`/`cmovne`
  and `vcomiss`/`jbe` for the two `DTTST` tests in both instantiations. In the `#cuda` shell
  (Kokkos 5.2.2, CUDA 12.9), `nvcc_wrapper -O3 -DNDEBUG -std=c++20 -arch=sm_89 --fmad=false
  -Xcompiler -fopenmp -Xcompiler -Wall,-Wextra -Xcompiler -ffp-contract=off -c`, instantiated on
  `Kokkos::Cuda` and `Kokkos::Serial`: no warning, no `FFMA` in the SASS. ⚠ Never run on a
  device: the staging through `create_mirror_view` and the two `KOKKOS_LAMBDA` kernels are
  compiled, not exercised.
