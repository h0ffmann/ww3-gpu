# W3FLDO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs a shim,
the `ww3_shel` wiring and the `W3FLDG` port it opens the unit for.

- (v) Fresh review 2026-10-16: `w3fldsmd.F90` recompiled from `WW3-noaa/` (`03adda91`) in the
  pinned `ww3` shell with the defines and flags of the CMake `flags.make` under
  `switch_lab_shrd`, plus `-fdump-tree-optimized-lineno` (`.text`, `.rodata`, `.rodata.str1.1`,
  `.data`, `.bss` identical to the CMake object); the GIMPLE of `w3fldo` read block by block
  against the port (the entry store of `TIDEFLAG`, the 19 `memcmp_eq` in order, `TEMPXT`'s copy
  and `MAX(I,0)` concat, the static `FDHDR`, the four `open_parm` with their flags, `STATUS`,
  `convert`, the header transfers, the `:420-421` branches, the swap `memmove`, the `DT` compare
  `21572`, the `NX`/`GTYPE` stores and the `TIDEFLAGIN` store); every `file:line` checked;
  libgfortran 16.2.0's `st_open`, `already_open`, `edit_modes`, `new_unit`, `regular_file2`,
  `compare_file_filename`, `fc_strdup`, `data_transfer_init`, `hit_eof`, `st_write_done` and
  `unit_truncate` read against `FldoUnits` and the transfers. Three defects found and fixed,
  each on a unit kept connected to the same file or a name with a NUL: the header WRITE now
  truncates the file where it ends, as `st_write_done` does (the port truncated only at a new
  OPEN, so a reconnected unit kept the bytes after the header); a WRITE on a unit whose READ
  hit the end of the file now gets `IOSTAT` 5001 and returns 4 (the port wrote the header);
  the name loses its trailing blanks before it is cut at a NUL, in `fc_strdup`'s order. An
  `fseek(f, 0, SEEK_CUR)` now separates a READ and a WRITE on the same `FILE*`, which C
  requires. Then the port's `w3fldo` itself was run, in a throwaway driver, on the 25 cases of
  the probe plus 8 more calls (three pairs on a unit kept connected to its file, two with
  `FHDR` on existing files), against WW3's object on the same input files:
  every `IERR`, `IDFLD`, `NX`, `GTYPE`, `TIDEFLAGIN`, every file's size and MD5 and the 1975
  bytes of messages are the same, except the one ⚠ below (`FHDR = .FALSE.` on an existing
  file). Compiled warning-free again (OpenMP `-O3`, `-Wshadow -Wconversion`, `-O0`
  ASan/UBSan, CUDA `sm_89 --fmad=false`).

Opens a WW3 input field file on unit `NDS` and handles its header record. For a preprocessor
(`ww3_prep`, `ww3_prnc`, `ww3_prtide`) it creates `<FPRE>level.<FEXT>`, `wind.<FEXT>` and the
others and writes the header; for the model (`ww3_shel`, `ww3_multi`, `ww3_prtide`) it opens the
file and checks the header: the file id `WAVEWATCH III`, the field id (a `WND` file may hold
`WNS`, an `ICE` file `ISI`, and the id is changed to match), the grid dimensions and the tide
flag. It does no floating-point arithmetic. `W3FLDG` then reads or writes the records after the
header through the same unit ([`W3FLDG_KOKKOS.md`](W3FLDG_KOKKOS.md)).

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLDO`, `model/src/w3fldsmd.F90:89-514` at NOAA-EMC/WW3 `03adda91`, identical at the `WW3/` pin `8e433f99` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3fldsmd.F90` is empty), as `switches/switch_lab_shrd` compiles it |
| Switch | `w3fldsmd.F90` is in every build (`model/src/cmake/src_list.cmake:11`). The switches inside the routine are `W3_S` (`STRACE`, `:200-202`, `:226-228`, `:243-245`), `W3_T` (test prints `:247-250`, `:353-355`, `:436-438`, FORMATs `9000-9002`) and `W3_TIDE` (`TIDEOK = .TRUE.`, `:254-256`); `switch_lab_shrd` defines none of them (v, `gfortran -E` with the defines of WW3's `flags.make`, 2026-10-16). None is ported |
| Code | [`../src/ww_kokkos/flds_w3fldo.hpp`](../src/ww_kokkos/flds_w3fldo.hpp): `ww::flds::w3fldo` (host only), `FldoUnits` (the connected units, libgfortran's `OPEN`), `FldoSaved` (the SAVE local `FDHDR`), `fldo_message` (FORMATs 1001-1010) |
| Reused | the record reader and writer of [`iogr_unformatted.hpp`](../src/ww_kokkos/iogr_unformatted.hpp), `FChar` ([`iogr_state.hpp`](../src/ww_kokkos/iogr_state.hpp)), `FileCloser` ([`iogr_w3iogr.hpp`](../src/ww_kokkos/iogr_w3iogr.hpp)), and from [`flds_w3fldg.hpp`](../src/ww_kokkos/flds_w3fldg.hpp) `fstr_eq`, `fortran_iw`, `write_iostat`, `kIostatAfterEndfile`; none of them changed |
| Calls not ported here | none under the lab switch; the error messages go to the optional `write_ndse` hook (the `FldgHooks` signature) with the text gfortran writes |
| Errors | the routine never stops; `IERR` is returned as `kFldo*` (0 to 10). One code is the port's: `kFldoExitFextTooLong` (-120), where the Fortran's behaviour is undefined (below) |
| Memory | host only; no View. W3FLDG takes `FldoUnits::stream(nds)` |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Header record

Unformatted sequential, `FORM = 'UNFORMATTED'` (`:231`), `CONVERT=file_endian` (`:361-373`,
`'big_endian'` from WW3's CMake; the GIMPLE sets `open_parm.convert = "big_endian"` in all four
OPENs): one record of 40 bytes, a 4-byte big-endian length before and after it.

| Bytes | Item | WRITE (`:391-392`) | READ (`:405-406`) |
|---|---|---|---|
| 13 | `c13` | `IDSTR = 'WAVEWATCH III'` | `TSSTR` |
| 3 | `c3` | `IDFLD` | `TSFLD` |
| 12 | `i4` × 3 | `NX`, `NY`, `GTYPE` | `NXT`, `NYT`, `GTYPET` |
| 8 | `i4` × 2 | `FILLER(1:2)`, 0 | `FILLER(1:2)` |
| 4 | `i4` | `TIDEFLAG` | `TIDEFLAG` |

- (v) Kinds and order from the `-O3` GIMPLE: `_gfortran_transfer_character(_write)` of 13 and 3
  bytes, `transfer_integer(_write)` of kind 4 for the three dimensions, one `transfer_array` of
  kind 4 for `FILLER(1:2)` (a rank-1 descriptor of extent 2) and one `transfer_integer` for
  `TIDEFLAG`, `common.flags = 32` (`IOSTAT`).
- (v) Against a file the Fortran wrote: the probe below wrote `p_level.ww3` (`NX` 3, `NY` 2,
  `GTYPE` 1): 48 bytes, `00000028`, `WAVEWATCH III`, `LEV`, `00000003 00000002 00000001`, three
  zero words, `00000028`. The port's `RecordWriter` puts the same items in the same order.
- (v) `FORM` is a SAVE local (the GIMPLE has `static character(kind=1) form[1:11] =
  "UNFORMATTED"`) assigned nowhere in the routine but its initialiser; `FORM .EQ. 'UNFORMATTED'`
  (`:386`, `:404`) stays in the object as a `memcmp`, always true. The formatted branches
  (`FORMAT 900`, `:393-396`, `:407-410`) are compiled and never taken: not ported.

## Order of operations and parity

The reference is the `-O3` object of `w3fldsmd.F90`, recompiled 2026-10-16 from the `WW3/` pin in
the pinned `ww3` shell (gfortran 16.2.0) with the defines and flags of
`ww3_lib.dir/flags.make` of a CMake build configured with `-DSWITCH=switches/switch_lab_shrd
-DCMAKE_BUILD_TYPE=Release` (`-O3 -fallow-argument-mismatch -g -fno-second-underscore
-ffree-line-length-none`), plus `-fdump-tree-optimized-lineno`.

- (v) The object is WW3's: `objdump -d` equals that of the CMake build's
  `ww3_lib.dir/w3fldsmd.F90.o`, and `.text`, `.rodata`, `.rodata.str1.1`, `.rodata.cst4`,
  `.rodata.cst8`, `.data` and `.bss` are byte-identical; `.rodata.str1.8` differs only in the
  source path of the two worktrees.
- (v) `INXOUT` (`:264`) is tested with `_gfortran_compare_string` (blank-padded) against `READ`,
  then `WRITE`; the message 1001 writes `INXOUT` at its full length. `IDFLD` (`:269-278`) is
  tested with `__builtin_memcmp_eq` of 3 bytes, in the source's order. The port uses `fstr_eq`
  and a 3-byte compare.
- (v) `TEMPXT = FEXT` (`:285`) copies `MIN(LEN(FEXT),20)` bytes and pads with blanks; `I =
  LEN_TRIM(FEXT)` (`:286`); absent, `'ww3'` and 3. `FNAME = '<stem>.' // TEMPXT(:I)` (`:299-348`)
  is a `concat_string` of `len(stem) + I` bytes; `FILE=FNAME(:I)` (`:364`, `:372`) passes those
  bytes, and `FPRE//FNAME(:I)` (`:361`, `:369`) puts all of `FPRE` first, blanks included.
  libgfortran trims trailing blanks of the name, then cuts it at a NUL (`fc_strdup`: `fstrlen`,
  then `strndup`), as the port does. Measured: `FEXT='x   '`,
  `FPRE='a b'` created `a bwind.x`.
- ⚠ `LEN_TRIM(FEXT) > 20`: `TEMPXT(:I)` reaches past `TEMPXT`'s 20 bytes, and the object reads
  `I` bytes from the stack (`concat_string(_34, _448, 6, "level.", prephitmp_807, &tempxt)` with
  `prephitmp_807 = MAX(I,0)`). The bytes it reads are undefined, so the port returns
  `kFldoExitFextTooLong` there (after `FDHDR` is set, as in the Fortran). `ww3_prtide` can reach
  it: its `FILENAMEXT` is 256 characters read from input (`ww3_prtide.F90:185`, `:335`, `:340`).
- (v) `FDHDR` is a SAVE local (`static logical(kind=4) fdhdr = 1`), assigned only when `FHDR` is
  present (`:292-294`): a call with `FHDR = .FALSE.` turns the header off for every later call of
  the process that omits `FHDR`. Measured (probe case 12): `FHDR=.FALSE.`, then no `FHDR`, then
  `FHDR=.TRUE.` gave files of 0, 0 and 48 bytes. The port keeps it in `FldoSaved`.
- (v) `TIDEFLAG` is a SAVE local too, but it is stored at entry (`tideflag = cstore_1`, `:258-262`)
  before any read, so nothing survives a call; the port keeps it local. `TIDEOK` does not exist in
  the object: without `W3_TIDE` it is the constant `.FALSE.`, and `:421` is `TIDEFLAG .NE. 0`.
- (v) `:420-421` in the GIMPLE: when `FILLER(1) .OR. FILLER(2)` is nonzero, a `TIDEFLAG >= 0`
  becomes 0 and passes, a negative one fails with 10; otherwise a nonzero `TIDEFLAG` fails with
  10. Measured: header `0,0,1` gives 10; `7,0,5` gives 0 with `TIDEFLAGIN` 0; `7,0,-2` gives 10.
  This test comes before the file id test (`:428`), as in the port.
- (v) The field id swap (`:433-439`) is a 3-byte `memmove` of `TSFLD` into `IDFLD` before the
  test of `:440`; on error 9 `IDFLD` is already changed. Measured: `ICE` on an `ISI` header
  returns 0 and `ISI`.
- (v) `IDFLD(1:2) .NE. 'DT'` (`:445`) is one 2-byte compare (`21572`, `"DT"`): the `DT0`-`DT2`
  files skip the grid check and leave `NX`, `GTYPE` as passed (probe case 14: `NX` 99, `GTYPE`
  -5 kept). Otherwise `NX = NXT`, and `GTYPE = GTYPET` when `GTYPE .LE. 4` (`:453-454`).
- (v) No floating-point arithmetic in either object; no `vfmadd`/`vfmsub` in the C++ object.

### The OPEN

The four OPENs (`:361-373`) have `IOSTAT`, no `ACTION`, no `POSITION`; 'READ' adds
`STATUS='OLD'`. `FldoUnits::open` follows libgfortran 16.2.0 (v, `io/open.c` `st_open`,
`already_open`, `edit_modes`, `new_unit`; `io/unix.c` `regular_file2`, `compare_file_filename`;
`runtime/string.c` `fc_strdup`; the source of the gcc 16.2.0 tarball in the nix store, read
2026-10-16), and the probe measured each case below with WW3's own object:

| Case | libgfortran | Probe (IERR, IOSTAT) |
|---|---|---|
| file missing, 'READ' | `O_RDWR` gives `ENOENT`, not retried | 3, 2 |
| file read-only, 'WRITE' | `O_RDWR` `EACCES`, then `O_RDONLY`; the WRITE gets `LIBERROR_BAD_ACTION` | 4, 5007; the file kept |
| file write-only, 'READ' | `O_RDWR`, `O_RDONLY` refused, `O_WRONLY` opens; the READ gets `LIBERROR_BAD_ACTION` | 5, 5007 |
| negative unit | "Bad unit number", `LIBERROR_BAD_OPTION` | 3, 5002 |
| unit already connected to the same file | `edit_modes`: connection and position kept | a second 'READ' of `p_level.ww3` on the same unit gave 6 (end of file after the header) |
| existing 200-byte file, 'WRITE' | not truncated at the OPEN; `st_write_done` truncates after the first record | 48 bytes |
| a 48-byte header and 100 more bytes, 'READ', then 'WRITE' on the same unit | the WRITE starts at byte 48 and truncates after its record | 0, then 0; the file has 96 bytes |
| a new file, 'WRITE', then 'READ' on the same unit | the READ starts after the header, at the end of the file | 0, then 6 |
| an empty file, 'READ', then 'WRITE' on the same unit | the READ hits the end of the file (`AFTER_ENDFILE`); the WRITE is refused, `LIBERROR_OPTION_CONFLICT` | 6, then 4, 5001 |

- (v) Every other failure of the OPEN sets `IOSTAT` to `errno` (`LIBERROR_OS`), which the port
  returns from `open(2)`.
- (v) A file already connected to another unit is allowed: `new_unit`'s check is skipped when
  `allow_std` has `GFC_STD_F2018`, as under gfortran's default `-std=gnu` (WW3 sets no `-std`).
- (v) Truncation. The header WRITE truncates the file at the position after its record
  (`fflush`, `ftruncate`), as `st_write_done` does for a unit not at its end; a failure sets
  `IOSTAT` to `errno` and returns 4.
- ⚠ With `FDHDR` `.FALSE.` the first WRITE is `W3FLDG`'s, whose writer takes a `FILE*` and does
  not truncate, so the port truncates a newly connected unit at the OPEN of 'WRITE' instead. The
  file ends the same when a record follows; when nothing is ever written, the Fortran leaves an
  existing file as it was and the port empties it (measured: 200 bytes against 0). Every caller
  at the pin writes the field records with `W3FLDG` next.
- ⚠ The end-of-file state of a reconnected unit is the `FILE*`'s `feof`, as in `W3FLDG`: after a
  READ that failed with a record cut short (5017), `feof` is set where libgfortran is not
  `AFTER_ENDFILE`. Reached only by a caller that ignores `IERR` 5 and calls again on the same
  unit; none at the pin does.
- ⚠ Not reproduced: the preconnected units 0, 5 and 6 (an OPEN of one of them closes the process's
  stream in libgfortran); `GFORTRAN_CONVERT_UNIT`; when a failed WRITE surfaces (libgfortran and
  stdio buffer differently, as for W3IOGR).
- ⚠ For W3FLDG: a unit the fallback opened write-only for 'READ' never reaches it (W3FLDO
  returns 5 first), but `W3FLDG`'s reader takes the `FILE*` only and would see an end of file
  where libgfortran gives 5007 if a caller used such a unit; the callers at the pin stop on
  `IERR .NE. 0` (`ww3_shel.F90:1735-1736`, `wminitmd.F90:2564-2567`).

### Messages

FORMATs 1001-1010 (`:472-496`) go to `write_ndse` only when `NDSE .GE. 0`, as `fldo_message`
builds them. (v) The probe wrote 15 messages (1001, 1002, 1003 with `IOSTAT` 2 and 5002, 1004 and
1005 with 5007, 1005 with 5016, 1006, 1007, 1008, 1009, 1010 twice); a throwaway driver that calls
`fldo_message` alone with the same values, compiled with g++ 16.2.0, wrote the same 1541 bytes
(`cmp`). That driver ran the formatter only; the review's driver ran `w3fldo` itself and
wrote the same 1975 bytes as the extended probe.

## Not ported

- `W3_S`, `W3_T`, `W3_TIDE`: not compiled under the lab switch.
- The formatted header (`FORMAT 900`): compiled, never taken (above).

## Benchmarks

None. No timing without parity (`AGENTS.md`), and none was measured.

## How it was regtested

Not yet. The port's `w3fldo` ran only in the review's throwaway driver (first bullet; the
driver, `rev/probe/drv.cpp`, and the extended Fortran probe, `rev/probe/probe2.f90`, are in the
directory below, run by `rev/probe/runc.sh` and `rev/probe/runf.sh`). What was done, 2026-10-16, in
`/tmp/claude-1000/-home-hoffmann-code-ww3-gpu/900356d7-9a07-464f-88f0-492c00219d16/scratchpad/W3FLDO/`:

- (v) `recomp.sh`: the recompiled object, its GIMPLE `re/w3fldsmd.F90.273t.optimized` and the
  section comparison with the CMake object.
- (v) `probe/run.sh`: `probe/fldo_probe.f90` linked against that object and the CMake
  `libww3.a`, 25 calls of WW3's `W3FLDO` on input files written by `probe/mk.py` (`python3 -I`);
  results in `probe/run/out.txt` and `probe/run/msgs.txt`.
- (v) `probe/msg.sh`: the message comparison above.
- (v) `tu/cxx.sh`: a throwaway unit `tu/tu_w3fldo.cpp` with g++ 16.2.0 and the openmp-release
  flags, no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp -c tu_w3fldo.cpp
```

  also with `-Wshadow -Wconversion`, and at `-O0 -g -Wsign-conversion
  -fsanitize=address,undefined`; and a copy of `wave_support.cpp` with the include and
  `(void)&ww::flds::w3fldo;` (no warning from this header). `tu/cu.sh`: the same unit with
  `nvcc_wrapper -O3 -DNDEBUG -std=c++20 -arch=sm_89 --fmad=false` in the `#cuda` shell, no
  warning. The routine is host code; nothing of it runs on a device.
