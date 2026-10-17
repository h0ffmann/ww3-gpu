# W3IORS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs a shim, the
`W3DIMW` hook filled, and the `W3INIT` and `W3WAVE` hunks (`w3initmd.F90:978`,
`w3wavemd.F90:2708`, `:2795`).

- (v) Fresh review 2026-10-16: `w3iorsmd.F90` recompiled at `-O3` with WW3's Release flags and
  the 37 lab defines in the pinned `ww3` shell (`.text` and disassembly identical to the CMake
  object of the scratch build); the GIMPLE's `MAX_EXPR <VA, 0.0>` and the assembly's
  `maxss %xmm1,%xmm0` read against the C++ `openmp-release` object's `vmaxss %xmm1,%xmm0,%xmm0`
  (VA first, `0.0` second in both, no FMA); every `FLOGOA` index of the GIMPLE (`flogoa[10]`,
  `[110]`, `[1]`, ..., `[46]`, and `(J-1)*10 + I-1` in the warning loop) against the port's
  `(J-1)*10 + I-1`; the `IAPROC + (I-1)*NAPROC` scatter, the `VA = 0.` of `:860` against the
  zero-filled mirror (columns 0 and above NSEA are zero in both), the place of the clamp, the
  order of the writes, the file name and `IFILE4`, the cold-start values (`0.0185f` is
  `0x3c978d50`) and every cited line at `03adda91`. `regular_file2` and the `LIBERROR_*` values
  re-read in the gcc 16.2.0 tarball of the Nix store. The translator's gfortran program rerun
  (same results), plus a probe that writes an 878-byte header over 84-byte records followed by
  record 2 and record 3 and one write without `POS=`: the bytes are the header up to byte 84,
  then each later write over it, and the unpositioned write lands right after the last data, as
  the port does it with `pwrite`. Four more WW3-written files walked with a new script: the
  `COLD` `restart.ww3` of `ww3_tp1.1` (NSEA 360, NSPEC 12, LRECL 84, 30 408 bytes =
  `84*(NSEA+2)`, the header overlap case, but its FLOGRR are all false, so the overlap bytes are
  zero either way ⚠), a `COLD` file of `ww3_tp2.7` (NSEA 111, every spectrum tail zero), an
  `FTCH` `restart.lowres` of `mww3_test_08` (8932 bytes: record 1, record 2 a hole of zeros,
  FETCH 10000.0 at `1 + 2*LRECL`), and the `FULL` `restart001.ww3` of `ww3_ta1` again. One
  defect found and fixed: in the OARST WRITE block the port handed EXTIOF the POS of the last
  `MAPTMP` record, because the parts loops kept their `RPOS` local; the Fortran's `RPOS` there is
  that of the last FPIS part (`:1034`), which the port now passes. It changes only the POS that
  FORMAT 903 prints, not the file. Recompiled warning-free with the `openmp-release` and
  `serial-debug` flags after the fix; CUDA not recompiled (the change is host code). Nothing of
  the port was run.

Writes and reads the restart file `restart.<ext>`: the action spectrum `VA(NSPEC,0:NSEALM)` of
every sea point and, for a full ("hot") restart, the model time, the times of the last water-level,
ice, air-density and ice-parameter fields (`TLEV`, `TICE`, `TRHO`, `TIC1`, `TIC5`), the water level `WLV`, the ice concentration `ICE`, the packed status map
`MAPSTA + 8*MAPST2`, the friction velocity `UST` and its direction `USTDIR`, the stability
correction factor `ASF` and the input peak frequency `FPIS`, and with `OARST` the coupling
fields that `FLOGRR` selects. When the file to read does not open, it starts from calm (under
`LN1`): no spectrum is read and the level fields take their cold-start values.

## What was ported

| | |
|---|---|
| WW3 routine | `W3IORS`, `model/src/w3iorsmd.F90:113-1600` at NOAA-EMC/WW3 `03adda91`, identical in the `h0ffmann/WW3` fork at the `WW3/` pin (`git -C WW3 diff 03adda91 HEAD -- model/src/w3iorsmd.F90` is empty), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/iors_stream.hpp`](../src/ww_kokkos/iors_stream.hpp) (a gfortran `ACCESS='STREAM'` big-endian unit: `OPEN` with libgfortran's retries, `POS=` and sequential transfers, IOSTAT values), [`../src/ww_kokkos/iors_w3iors.hpp`](../src/ww_kokkos/iors_w3iors.hpp) (`ww::iors::w3iors<ExecSpace>`, host driver; `VA(:,0) = 0` and `VA = MAX(0.,VA)` as `KOKKOS_INLINE_FUNCTION` launchers on any execution space) |
| Callees | `W3DIMW` (`:451`, `:453`, when `.NOT.DINIT`) is a hook (`IorsHooks`); an empty hook on that path ends with `kExitMissingHook`. `W3SETO`, `W3SETG`, `W3SETW` (`:422-424`): the caller passes the state of grid IGRD. `INIT_GET_ISEA` is `ISEA = JSEA` under `SHRD` (`w3parall.F90:1484`). `EXTCDE` and `EXTIOF` are exit codes in `IorsStatus`. `MAPTMP` is packed and unpacked with W3IOGR's `encode_maptmp`/`decode_maptmp` (`iogr_w3iogr.hpp`), the same integer statements (`:980`, `:1204-1205` here; `w3iogrmd.F90:768`, `:1071-1072`) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

The switch is `F90 NOGRB NOPA LRB4 NC4 SHRD PR3 UQ FLX0 LN1 ST4 STAB0 NL1 BT1 DB1 MLIM TR0 BS0 IC0
IS0 REF0 XX0 WNT1 WNX1 RWND CRT1 CRX1 O0 ... O14`. Inside `W3IORS` that compiles the `SHRD`
spectrum loops (`:691-700`, `:860-868`), the `LN1` calm start (`:608-611`), and nothing of
`W3_MPI` (`:701-777`, `:799-857`, `:898-905`), `W3_PDLIB` (with `LPDLIB` set both spectrum
branches are empty), `W3_WRST` (`:941-978`, `:1171-1192`, `:1476-1479`), `W3_REF1`
(`SET_UG_IOBP`, commented out anyway at `:1211`), `W3_LN0`/`W3_SEED`, `W3_TIMINGS`, `W3_T`, `W3_S`.
The lines were read from the preprocessed source (`gfortran -E -cpp` with the defines of WW3's
`flags.make`, 1307 lines).

## File layout

`OPEN(..., FORM='UNFORMATTED', CONVERT=file_endian, ACCESS='STREAM')` (`:522-526`): a byte stream
with no record markers, big-endian. `W3IORS` builds fixed records of `LRECL = MAX(4*NSPEC, 84)`
bytes (`:459-461`) and writes record n at `POS = 1 + LRECL*(n-1)`; `NSIZE = LRECL/4` values per
part. Every record is first written as `LRECL` zero bytes (`WRITEBUFF`), then its data from the
record's first byte. Kinds are those of the GIMPLE's `_gfortran_transfer_*` calls: `i4`
INTEGER, `l4` LOGICAL, `r4` REAL, `cN` CHARACTER(LEN=N). Lines are WRITE / READ.

| Record | When | Fields | Lines |
|---|---|---|---|
| 1 | always | IDSTR c26, VERINI c10, GNAME c30, TYPE c4, NSEA, NSPEC i4, FLOGRR(10,20) l4: 878 bytes | `:540-544` / `:549-550` |
| 2 | FULL | TIME(2) i4 | `:627-629` / `:632` |
| 3 | FTCH | FETCH r4, no zero fill | `:655` / `:788` |
| 3 .. NSEA+2 | FULL, COLD | VA(1:NSPEC,ISEA) r4, zeros to LRECL | `:691-700` / `:860-868` |
| NSEA+3 | FULL | TLEV, TICE, TRHO, TIC1, TIC5 i4 | `:907-915` / `:1151-1154` |
| next NPART each | FULL | WLV, then ICE r4, NSIZE points per record | `:916-939` / `:1155-1170` |
| next NY*NPRTX2 | FULL | MAPTMP(IY,1:NX) i4, row by row, NSIZE columns per record | `:979-995` / `:1193-1206` |
| next NPART each | FULL | UST, USTDIR, ASF, FPIS r4 | `:996-1043` / `:1218-1249` |
| no `POS=` | FULL and OARST | the FLOGRR (WRITE) or FLOGOA (READ) fields, X(1:NSEA) each, from where the last FPIS part stopped | `:1044-1133` / `:1250-1461` |

`NPART = 1 + (NSEA-1)/NSIZE`, `NPRTX2 = 1 + (NX-1)/NSIZE` (`:888-889`). A full restart without
`OARST` is `LRECL*(NSEA + 3 + 6*NPART + NY*NPRTX2)` bytes.

- (v) Kinds, order and conditions: every `_gfortran_transfer_*` call of the `-O3` GIMPLE of
  `w3iorsmd.F90` (gfortran 16.2, WW3's Release flags and the lab defines), read 2026-10-16. The
  `.text`, `.rodata`, `.rodata.cst4/8/16` and `.rodata.str1.1` sections of that object are
  byte-identical to the object of a WW3 CMake Release build of the fork under
  `switch_lab_shrd` (a scratch configure of the `WW3/` pin, 8e433f99); `.rodata.str1.8` differs
  only in the source path, and the disassembly is identical.
- (v) `LRECL` is `MAX_EXPR <NSPEC*4, 84>` and `NSIZE` `LRECL >> 2` in the GIMPLE; the C++
  `kLreclMin` is the Fortran's sum of `INT` of REAL quotients, `static_assert`ed to 84.
- (v) The layout against files WW3 wrote, read 2026-10-16 with a scratch script that walks the
  records (nothing of the port ran): three `CALM` files of `ww3_strt` (`ww3-smoke` and `ts1`
  runs, NSEA 1, NSPEC 864: 3456 bytes, one record, header then zeros); a `FULL`
  `restart001.ww3` of `ww3_ta1` (3×3, NSEA 1: 13 records, 44 928 bytes, every record tail zero);
  and a `FULL` `restart001.ww3` of `ww3_tp2.17` (NX 3070, NY 1, NSEA 3070, NSPEC 1152, LRECL
  4608: NPART 3, NPRTX2 3, 3094 records, 14 257 152 bytes, the size the formula gives). No file
  with `OARST` fields was found ⚠: that block follows the source and the GIMPLE only.
- (v) gfortran's stream behaviour, from a standalone gfortran 16.2 program in the pinned shell
  and `libgfortran/io/unix.c` (`regular_file2`, gcc 16.2.0 source), 2026-10-16: `OPEN` without
  `STATUS` keeps an existing file and a `WRITE` does not truncate it; a `WRITE` past the end
  leaves zeros; a transfer without `POS=` continues where the last one stopped; `.TRUE.` is
  `00000001`; a `READ` that reaches the end, also part way, gets IOSTAT -1; a missing file at
  `OPEN` gets 2 (`ENOENT`); a directory opens with `ACTION='READ'` and its `READ` gets 21
  (`EISDIR`); a read-only file opened without `ACTION` opens, and its `WRITE` gets 5007, or exit
  status 2 without IOSTAT; a `WRITE` with `POS=` on a unit not connected creates an empty
  `fort.<unit>` and gets 5002; `CLOSE` of a unit not connected is no error; `FILE=` drops
  trailing blanks. `iors_stream.hpp` reproduces each of these.

## Quirks reproduced

Each read in the preprocessed source and, where it is code, in the GIMPLE (v):

- **The header outgrows its record.** The 84 bytes of `LRECL`'s floor count IDSTR, VERINI,
  GNAME, TYPE and two integers, not the 200 LOGICALs of FLOGRR: the header is 878 bytes. With
  `NSPEC < 220`, it runs into record 2 and on, which the time record (FULL) or the spectra
  (COLD, from record 3) or the 4 bytes of FETCH at record 3 (FTCH) overwrite with their zero
  fill and data, so a later READ gets those bytes in FLOGOA. The port writes the same bytes in
  the same order.
- **The spectrum is clamped after it is written.** `VA = MAX(0.,VA)` (`:876`) comes after the
  WRITE of the spectra, so the file holds the unclamped values and the model's VA is clamped
  after a WRITE of type FULL or COLD. A WRITE of type WIND, CALM or FTCH returns (`:673`) without
  the clamp.
- **A failed OPEN for READ is a calm start, an empty file is an exit.** `IERR /= 0` at the `OPEN`
  sets TYPE `'CALM'` and RSTYPE 4 (`:608-612`), and VA is not zeroed (W3DIMW zeroed it at
  allocation, `w3wdatmd.F90:491`); a file that opens but is shorter than the header exits 30 at
  `EXTIOF`.
- **`restartNNN`.** With `IFILE4 > 0` the name is `restartNNN.<ext>` and `NNN` is replaced by
  `I3.3` of IFILE4 only for a WRITE on process NAPRST (`:499-501`): a READ, or another process,
  opens the literal `restartNNN.<ext>`. A `restartNNN.ww3` of 14 155 776 bytes (3072 records,
  header all zero) lies beside the three numbered files of the `ww3_tp2.17` run above (an MPI
  build, not the lab's). I3.3 of a value over 999 is `***`, as is I8.8 of a negative or
  nine-digit date in the `FLRSTRT` name (`:490`), (v) the program.
- **`FETCH` without a connected unit.** The FTCH WRITE (`:653-657`) is not guarded by the test
  that opened the unit; with `IOSTYP > 0` on a process other than NAPRST it goes to gfortran's
  implicit `fort.<NDSR>` and exits 31 (IOSTAT 5002).
- **Unchecked OARST writes.** In the OARST WRITE block, a WRITE whose IOSTAT is not tested
  (`ICEF`, `HS`, ..., the second of each pair) is reported only if it is the last before the test
  at `:1131`, with the POS of the last FPIS part.
- **IMOD passed when absent.** `W3DIMW(IMOD, ...)` (`:451`, `:453`) gets W3IORS's optional IMOD;
  the GIMPLE passes `imod`'s pointer, null when it is absent (`ww3_strt.F90:932` calls without
  it). The hook gets the same null.
- **No STATUS on the WRITE OPEN.** A longer old file keeps its tail past the last record written.

## Arithmetic

The only floating-point operation is `VA = MAX(0.,VA)` (`:876`).

- (v) The object computes it as `maxss %xmm1,%xmm0` with VA in `%xmm0` and `0.0` in `%xmm1`:
  `VA > 0 ? VA : 0.0`, so a NaN or a `-0.0` becomes `+0.0`, in a scalar loop. `va_clamp()` is
  that expression; the `openmp-release` object computes it as `vmaxss %xmm1,%xmm0,%xmm0`, the
  same operand order, in the probe function and in both instances of the launcher.
- (v) Constants of the cold start, from `.rodata.cst4`: `1.0` (`0x3f800000`, ASF, FP0) and
  `0.0185` (`0x3c978d50`, CHARN, `1.85000002384185791015625e-2` in the GIMPLE); `0.0185f` has
  the same bits. FPIS takes `DUMFPI` (W3INIT passes `SIG(NK)`, `w3initmd.F90:978`).
- (v) `MAPTMP = MAPSTA + 8*MAPST2` and `MAPSTA = MOD(MAPTMP+2,8) - 2`, `MAPST2 =
  (MAPTMP-MAPSTA)/8` are integer; the object vectorises the decode with `psrad`/`psrld` for the
  signed `MOD`, the C++ uses `%` and `/`, which truncate toward zero as Fortran's do.
- (v) Every LOGICAL test in the GIMPLE (`OARST`, `DINIT`, `FLRSTRT`, `FLOGRR`, `FLOGOA`) is
  `!= 0`; the port keeps LOGICALs as `int32_t` and tests `!= 0`.
- (v) Spectra and fields are copied as bytes (`put_be`/`get_be`), never through a floating-point
  operation, so NaN payloads survive the file as in the Fortran.
- ⚠ GPU. On CUDA (`sm_89`, `--fmad=false`, compiled, never run) the clamp is `FMNMX R, RZ, R`
  (a max with `+0`): a NaN gives `+0` as on the host; the sign of the result for `-0.0` was
  not checked. The SASS of the two launchers has no `FFMA`.
- ⚠ `-ffp-contract=off` matters nowhere here: there is no product to fuse.

## Not ported

- Every `#ifdef` branch `switch_lab_shrd` does not compile (above), and the `W3_T` test prints
  (FORMATs 9000-9020).
- The text of the messages: `IorsStatus` carries the FORMAT label and its values (900-906 with
  their exit codes, 990, 1000 and 903 as warnings, the `'(A,I8)'` of `:514` as 15, and EXTIOF's
  IOSTAT and POS) for the caller to print, on NDSE when IAPROC equals NAPERR.
- `W3DIMW` (hook), `W3SETO`/`W3SETG`/`W3SETW` (the caller's).
- The Fortran unit table. gfortran's `OPEN` on NDSR first closes whatever the program had
  connected to that unit; the port's unit is its own file descriptor, so the caller's units are
  untouched ⚠. NDSR only takes part in the `NDST == NDSR` test (`:512-517`) and the
  `fort.<NDSR>` name.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing of the port was run. What was done, 2026-10-16 (v): a throwaway translation
unit that includes `iors_w3iors.hpp` and explicitly instantiates `w3iors` on
`Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, and both launchers, compiled with
the `openmp-release` flags of the ninja file (`g++ -O3 -march=x86-64-v3 -DNDEBUG -std=c++20 -Wall
-Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter -ffp-contract=off -fopenmp`,
Kokkos 5.2.2 OpenMP of the pinned `#ww3` shell): no warning, no `vfmadd`/`vfmsub`; the same unit
with the `serial-debug` flags (`-fsanitize=address,undefined -g`): no warning; a copy of
`wave_support.cpp` with the `#include` and three `(void)&` lines added, compiled with both sets of
flags: no warning; the unit for CUDA in the `#cuda` shell (`nvcc_wrapper -std=c++20 -O3
-arch=sm_89 --fmad=false --expt-extended-lambda -Xcompiler -ffp-contract=off -Xcompiler -fopenmp`,
Kokkos 5.2.2 CUDA, CUDA 12.9): no warning, compile only. The test code that was run is the
standalone gfortran program and the record-walking script above, neither of which calls the port.
