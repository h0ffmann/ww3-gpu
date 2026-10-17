# W3IOBC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs a shim
that fills the `W3IOBC` hooks of the Kokkos `W3WAVE` (`w3iobc_read`, `w3iobc_write` in
`wave_w3wave_hooks.hpp`), the four hooks below and the `W3WAVE` hunk.

- (v) Fresh review 2026-10-16: WW3's CMake Release build of `WW3-noaa/` (`03adda91`) rebuilt
  under `switch_lab_shrd` in the pinned `#ww3` shell (gfortran 16.2.0) in a scratch directory,
  `w3iobcmd.F90` recompiled with its `flags.make` plus `-fdump-tree-optimized` (`.text`,
  `.rodata`, `.rodata.cst4` identical to the CMake object) and preprocessed with `gfortran -E`
  and the same defines; every `_gfortran_st_*`/`_gfortran_transfer_*` call, the OPEN parameters
  (`status "OLD"` on `:334` only, `convert "big_endian"` on all three, BACKSPACE `flags = 0`),
  the gather (`_811 = _800 * _810; _843 = _811 / _830`, row `(IS-1)/NTH + 1` signed) and `SPCONV`
  read against the port and against the C++ GIMPLE of a unit instantiating `w3iobc` and
  `iobc_gather_shrd` on OpenMP with the `openmp-release` flags (same operations, constants and
  branch order; no `vfmadd` in either object); every `w3iobcmd.F90`, `w3odatmd.F90`,
  `w3servmd.F90`, `w3gsrumd.F90`, `w3triamd.F90`, `w3cspcmd.F90` and `w3wavemd.F90` citation
  checked; libgfortran's `io/unix.c`, `io/transfer.c`, `io/file_pos.c` and `runtime/error.c`
  read from the gcc 16.2.0 source tarball of the Nix store. Three defects found and fixed, each
  shown on crafted nest files by a gfortran 16.2 program built with WW3's flags that executes the
  READ statements of the 'READ' path in their order, against the port on the same files: (1) a
  READ after one that hit the end of the file gets IOSTAT 5001 (`AFTER_ENDFILE`), so a
  `nest.<ext>` that ends inside the spectra of a time record with two or more spectra still to
  read exits 42 at the second; the port read the end of file again, ignored -1 and returned
  `IOTST = 0` (now `IobcUnits::Unit::after_endfile`); (2) a time record cut inside `TIME2`: the
  Fortran reads the bytes it has into `TIME2` and byte-swaps the whole item, so `TIME2(2)` became
  the byte swap of its old value (1800 gave 134676480 in both after the fix), where the port left
  it at 1800 (now `time_record`); (3) `OPEN(STATUS='OLD')` of a directory fails in libgfortran
  (`O_RDWR` gives EISDIR) and `W3IOBC` returns `IOTST = 1`, where `fopen("rb")` opened it and the
  port went on to exit 10 (now `IobcUnits::open` makes the `open(2)` calls of `regular_file2`).
  Compiled warning-free (below); nothing of WW3 was run.

`W3IOBC` is the boundary-data I/O of a nested run. On 'WRITE' it opens `nest1.<ext>` ..
`nest<NFBPO>.<ext>` on the first call, writes their headers and point tables, and on every call
writes one time record and the spectra of the output boundary points, converted from action to
frequency spectra, `VA*SIG2/CG`. On 'READ' it opens `nest.<ext>` on the first call, checks its
header, locates every input point in the grid, and on every call reads the next time record and
its spectra into `ABPIN`, keeping the previous ones in `ABPI0`. 'DUMP' writes the input spectra
back out for `ww3_multi`. It interpolates nothing: the time interpolation between `ABPI0` and
`ABPIN` is `W3UBPT`, the spatial weights `IPBPI`/`RDBPI` come from `ww3_grid` through the file,
and a change of spectral grid is `W3CSPC`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3IOBC`, `model/src/w3iobcmd.F90:100-852` at NOAA-EMC/WW3 `03adda91`, identical in the `h0ffmann/WW3` fork at the `WW3/` pin (`git -C WW3 diff 03adda91 HEAD -- model/src/w3iobcmd.F90` is empty, and `cmp` of the two files is silent), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/iobc_w3iobc.hpp`](../src/ww_kokkos/iobc_w3iobc.hpp): `ww::iobc::w3iobc<ExecSpace>` (host driver), `abpos_shrd` (the gather statement, `KOKKOS_INLINE_FUNCTION`) and its launcher `iobc_gather_shrd<ExecSpace>`, `IobcState` (the module variables), `IobcUnits` (the connected units), `backspace` (libgfortran's unformatted `BACKSPACE`) |
| Reused | the record reader and writer of [`iogr_unformatted.hpp`](../src/ww_kokkos/iogr_unformatted.hpp), `FChar`, `FLogical` and the exit codes of the `W3IOGR` port ([`W3IOGR_KOKKOS.md`](W3IOGR_KOKKOS.md)) |
| Calls not ported here | `W3DMO5` (blocks 1 and 3), `W3NESTUG`, `W3GFPT` and `W3CSPC` are hooks (`IobcHooks`) the caller fills; `iobc_shape_hooks()` fills `W3DMO5` with the shapes it allocates. An empty hook on the path ends with `kExitMissingHook`, never a skipped call. `W3SETO`, `W3SETG`, `W3SETW`, `W3SETA` (`:309-312`): the caller passes the state of grid `IGRD` |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

What `switch_lab_shrd` compiles in `W3IOBC` (v, `gfortran -E` with the defines of WW3's
`flags.make`, 2026-10-16): the `W3_SHRD` gather (`:666-671`) and nothing else under a switch.
Not compiled and not ported: `W3_DIST` (`:676-681`), `W3_RTD` (`:471-508`, the rotated-pole remap
of the input points, and `:683-693`, `W3ACTURN` of the output spectra), `W3_SMC` (`W3SMCGMP`,
`:512-518`; an SMC grid takes the `W3GFPT` branch, as in the Fortran without the switch), the
`W3_T`, `W3_T0`, `W3_T1` test output and `W3_S` STRACE.

## Record layout

Unformatted sequential, `CONVERT='big_endian'` (`FILE_ENDIAN = ENDIANNESS`, `constants.F90:87`;
the GIMPLE sets `open_parm.convert = "big_endian"` for all three OPENs), each record a big-endian
4-byte length, the payload and the length again (`iogr_unformatted.hpp`). Kinds from the GIMPLE's
`_gfortran_transfer_*` calls: `i4` INTEGER, `r4` REAL, `cN` CHARACTER(LEN=N). Lines are WRITE /
READ in `model/src/w3iobcmd.F90`.

| Rec | File, when | Fields, in order | Lines |
|---|---|---|---|
| 1 | `nestN`, first 'WRITE', `IAPROC = NAPBPT` | IDSTRBC c32, VERBPTBC c10, NK, NTH i4, XFR, FR1, TH(1) r4, NBO(N)-NBO(N-1) i4 | `:375-377` |
| 2 | same | XBPO(i), YBPO(i) r4, IPBPO(i,1:4) i4, RDBPO(i,1:4) r4, i = NBO(N-1)+1..NBO(N), each array whole before the next, J outer | `:389-393` |
| 1 | `nest`, first 'DUMP', `IAPROC = NAPBPT` | IDSTRBC c32, VERBPTBC c10, NK, NTH i4, XFR, FR1, TH(1) r4, NBI i4 | `:412` |
| 2 | same | XBPI(1:NBI), YBPI r4, IPBPI(NBI,4) i4, RDBPI(NBI,4) r4 | `:418-420` |
| 1 | `nest`, first 'READ' | IDTST c32, VERTST c10, NKI, NTHI i4, XFRI, FR1I, TH1I r4, NBI i4 | `:437-438` |
| 2 | same | XBPI, YBPI r4, IPBPI i4, RDBPI r4, all with NBI | `:465-468` |
| t | every call | TIME(2) i4, NPTS i4: NBO2(N)-NBO2(N-1) on 'WRITE', NBI2 on 'DUMP' and 'READ' | `:609`, `:617` / `:564`, `:624` |
| s | after each t, one per point | the spectrum, NSPEC r4 (NKI·NTHI on a READ with SPCONV) | `:695`, `:716` / `:724`, `:735` |

- (v) Kinds, order and conditions: every `_gfortran_transfer_*` and `_gfortran_st_*` call of the
  `-O3` GIMPLE of `w3iobcmd.F90` (gfortran 16.2.0, WW3's Release flags and the lab defines), read
  2026-10-16: the 8 unformatted WRITE and 6 READ statements, the 3 OPENs and the BACKSPACE (the
  other 12 `_gfortran_st_write` calls are the internal writes of `FILEN` and the FORMAT messages). The `.text`, `.rodata`,
  `.rodata.cst4` and `.rodata.str1.*` sections of that object are byte-identical to the object of
  a WW3 CMake Release build of the `WW3/` pin under `switch_lab_shrd`, configured and built in a
  scratch directory inside the pinned `#ww3` shell.
- (v) The layout against files WW3 wrote, read 2026-10-16 with a scratch script that walks the
  record markers (nothing of the port ran): `nest1.ww3`..`nest3.ww3` of the `ww3_tr1` entry
  `work_F_80` (`build/hoff-matrix-serial` of the main checkout, written 2026-10-15 by `ww3_shel`)
  have 65 records each, a 66-byte header (32+10+6·4), a 320-byte point record for 8 points (40
  bytes a point), then 7 time records of 12 bytes, each followed by 8 spectra of 1248 bytes (NK 13,
  NTH 24); the `nest.ww3` of `work_F_82` (written by `ww3_bound`) has the same header and point
  records and 3 spectra per time, the case where NBI (8 points) and NBI2 (3 spectra) differ. ⚠
  `ww3_tr1` is built with `W3_RTD`, which changes values, not the layout; no lab-switch run has
  written a nest file.
- (v) The header of a 'WRITE' file counts points with NBO and its time records count spectra with
  NBO2 (`:377`, `:608`); both reproduced.
- (v) `BACKSPACE (NDSB)` after the first time record (`:586`) is libgfortran's `st_backspace` and
  `unformatted_backspace` (`io/file_pos.c`, gcc 16.2.0 source, read 2026-10-16): nothing at the
  start of the file, otherwise read the 4-byte marker before the position and seek back over the
  subrecord and both markers, repeating while the marker is negative. `backspace()` does that; an
  OS error there is LIBERROR_OS with no IOSTAT, a run-time error, exit status 2. The first 'READ'
  therefore reads the first time record twice (`:564`, then `:624`).

## Arithmetic

The reference is the `-O3` object above, read as GIMPLE (`-fdump-tree-optimized`) and assembly
(`-S -fverbose-asm`), 2026-10-16. `W3IOBC` has three floating-point statements.

- (v) `ABPOS(IS,ISOUT) = VA(IS,ISEA) * SIG2(IS) / CG(1+(IS-1)/NTH,ISEA)` (`:668-669`) is
  `_811 = _800 * _810; _843 = _811 / _830` in the GIMPLE and one `mulss` and one `divss` per
  element in the assembly, not vectorised. `abpos_shrd` compiles to `vmulss`, `vdivss`
  (openmp-release flags), and in the OpenMP kernel of `iobc_gather_shrd` to `vmulss`, `idiv` (the
  index), `vdivss`. `(IS-1)/NTH` is a signed `int` division in both. Each element depends on its own
  inputs only, with no reduction and no atomic, so the bits do not depend on the execution space or
  the order of the points.
- (v) `SPCONV` (`:458-461`): the GIMPLE branches in the source order, `NKI/=NK`, `NTHI/=NTH`,
  `ABS(XFRI/XFR-1.)>0.01`, `ABS(FR1I/FR1-1.)>0.01`, `ABS(TH1I-TH(1))>DTH*0.01`, with
  `9.99999977648258209228515625e-3` and `1.0e+0`; the C++ GIMPLE of `w3iobc` has the same
  divisions, subtractions, `ABS_EXPR`, product and constants in the same order (`_62 < _970` for
  `_371 > _374` in the reviewed build). Only the LOGICAL is stored, so a short-circuit cannot change a bit.
- (v) `DCIN=0.1` (`:524`) is `static real(kind=4) C.5183 = 1.00000001490116119384765625e-1`;
  the hook receives `0.1f` and widens it as `W3GFPT_R4` does (`w3gsrumd.F90:2038-2074`).
- (v) No FMA in either object: no `vfmadd`/`vfmsub` in the C++ object, none in the Fortran
  assembly.
- ⚠ CUDA, `sm_89`, `--fmad=false`, compiled with `nvcc_wrapper` in the pinned `#cuda` shell and
  never run: the gather kernel has 7 `FMUL` (the product), 7 `FCHK` and 47 `FFMA`; the `FFMA` are
  the reciprocal refinement of CUDA's IEEE single-precision division and its slow-path subroutine,
  not a fused product. That this division returns the bits of `divss` for every input, subnormals
  included, was not checked.
- (v) `-ffp-contract=off` is private to the `ww_kokkos` target, but this header has nothing to
  fuse: a product followed by a division has no FMA form, and the throwaway unit compiled without
  `-ffp-contract=off` has no `vfmadd`/`vfmsub` either.

## Behaviour reproduced, quirks included

- (v) Exit codes, in the Fortran's order: 1 (INXOUT, FORMAT 900), 40 (`EXTOPN` of a 'WRITE' or
  'DUMP' OPEN, `:354`, `:365`), 42 (`EXTIOF(...,41)` under `IERR.GT.0`, which exits `ERRCODE+1`,
  `w3servmd.F90:910-923`), 10 and 11 (header ID and version), 12 (a point not in the grid, FORMAT
  910), 20 (a point on a cell with `|MAPSTA| /= 2`, after FORMAT 909 for each such point), 43 (no
  time record after the header, FORMAT 1010). A failing WRITE has no IOSTAT, so gfortran stops with
  exit status 2 (`kExitGfortranRuntime`). Messages are not printed: `IobcStatus` carries the FORMAT
  label and values when `IAPROC = NAPERR`.
- (v) The three READs of the header, the points and the spectra test only `IERR.GT.0`
  (`:439`, `:469`, `:725`, `:736`): a READ that hits the end of the file (IOSTAT -1) goes on with
  what it did not read, and leaves the unit `AFTER_ENDFILE` (`hit_eof`, `io/transfer.c:4956-4988`):
  the next READ fails with IOSTAT 5001 before reading a byte (`data_transfer_init`,
  `io/transfer.c:3357-3364`; `warn_std` is set without `-std`, and the probe built with WW3's
  flags got 5001). The next READ after an end of file inside the points or spectra therefore
  exits 42 (`IERR.GT.0`), or 43 at the first time record; reproduced. A READ that hits the end of
  the file assigns nothing.
- (v) A missing `nest.<ext>` on the first 'READ' sets `IOTST = 1`, `FLBPI = .FALSE.` and returns
  without resetting `FILER` (`:336-341`), so the next call tries the OPEN again; reproduced.
- (v) FORMAT 1001 writes `IMOD` (`:337`) and the GIMPLE transfers `*imod` with no `PRESENT` test:
  with `IMOD` absent and `IAPROC = NAPERR`, the Fortran dereferences a null pointer. The port
  returns `kExitNullImod` there instead of crashing. W3WAVE always passes `IMOD`
  (`w3wavemd.F90:1306`, `:2715`).
- (v) `FILEN` is `CHARACTER(LEN=18)`; the 'WRITE' name `nest<N>.` plus `FILEXT(:I)` needs `6+I`
  characters, so a 13-character `FILEXT` overflows the internal record, a run-time error with exit
  status 2 (a standalone gfortran 16.2 program in the pinned shell, 2026-10-16, printed
  `formatted_transfer_scalar_write` in its backtrace and exited 2). Reproduced.
- (v) `NDSL(IFILE) = NDSB + IFILE - 1` (`:346`) is unsigned wrapping arithmetic in the GIMPLE; the
  C++ computes it the same way.
- (v) `ISBPI(I) = MAPFS(IY,IX)` (`:535`) runs for a point on an inactive cell too; `FLOK` stops the
  run only after the loop (`:547`). The warning loop of `:549-560` changes only `FLOK`, which is not
  read after it, so the port runs it only when the message is written.
- (v) On end of file in the time READ of a later call (`:625-644`), `TIME1 = TIME2` takes whatever
  the failed READ left in `TIME2`, and `ABPI0(:,0:NBI2) = ABPIN(:,0:NBI2)` uses the `NBI2` of the
  previous record. What a READ cut part way leaves (`unformatted_read`, `io/transfer.c:1096-1231`):
  items before the failing one are assigned; the failing item receives the bytes that were read
  over its first bytes and is then byte-swapped whole (`CONVERT='big_endian'`), error or not; the
  items after it are untouched. `TIME2` is one `_gfortran_transfer_array` of 8 bytes and `NBI2`
  one `_gfortran_transfer_integer`, so a time record of 4 bytes leaves `TIME2(2)` the byte swap
  of its old value, and one of 8 bytes leaves `NBI2` the byte swap of the previous `NBI2`, which
  the copy then uses as its bound (an out-of-bounds copy in the Fortran, `kExitShape` here).
  Reproduced for the time records; every other READ that stops part way exits (42, 43), so what
  it leaves is not read.
- ⚠ Undefined values the port cannot reproduce: the locals `IDTST`, `VERTST` and the allocations of
  `W3DMO5` and `TMPSPC` start undefined in the Fortran and blank or zero here, so a header READ cut
  short gives exit 10 in both but a different FORMAT 901 text, and a points or spectra READ cut
  short leaves zeros where the Fortran leaves garbage.
- ⚠ `TIME2` is `INTENT(OUT)`: the port keeps its previous value where a failed READ does not
  reach it, as the memory does; gfortran may clobber an `INTENT(OUT)` dummy at the caller, not
  checked in `W3WAVE`'s object.
- (v) The OPENs have no ACTION, so libgfortran's `regular_file2` (`io/unix.c:1349-1490`) tries
  `O_RDWR` (with `O_CREAT` unless `STATUS='OLD'`), then on EACCES, EPERM or EROFS `O_RDONLY`
  without `O_CREAT`, then on EACCES, EPERM or ENOENT `O_WRONLY`; `IobcUnits::open` makes the same
  calls. A file readable only or writable only opens and fails at its first transfer (the probe:
  a write-only `STATUS='OLD'` file opens with IOSTAT 0 and its READ gets 5007, `kIostatBadAction`).
- ⚠ An OPEN without STATUS keeps an existing file until its first WRITE; the port truncates it at
  the OPEN (`ftruncate`). The files end the same, unless the run stops between the OPEN of one
  `nestN` and its first WRITE (an `EXTOPN` of a later file).
- ⚠ The IOSTAT of a failed OPEN is `errno` as `generate_error` reads it, after `gf_strerror`
  built the message (`io/open.c:566-579`, `runtime/error.c:687`): the probe got 2 for an EISDIR
  failure, the ENOENT left by the message-catalogue lookup, where the port returns 21. Only the
  value `EXTOPN` prints (exit 40) can differ; `:336` tests `IERR.NE.0`.
- ⚠ After a READ that stops at the end of its record (`LIBERROR_SHORT_RECORD`), libgfortran moves
  to the next record (`next_record_r_unf`, `io/transfer.c:776-784`); `RecordReader` stays inside
  it. Every such READ in `W3IOBC` exits or, for the time READ of a later call, returns with
  `FLBPI = .FALSE.`, after which `W3WAVE` does not call it again (`w3wavemd.F90:1305-1313`), so
  no READ follows; not reproduced.
- ⚠ An OPEN of a unit connected to the same file only changes its specifiers in Fortran; the port
  closes and reopens it. `W3IOBC` opens each unit once per grid (`FILER`, `FILEW`, `FILED`), so no
  call path reaches it. A transfer on a unit never opened makes gfortran open `fort.<unit>`; the
  port returns `kExitUnitNotConnected`.
- ⚠ `GFORTRAN_CONVERT_UNIT` and a big-endian host: as in the W3IOGR report.
- Bounds the Fortran does not check end with `kExitShape` (an `NBI2` larger than the `W3DMO5`
  allocation, a grid index from `W3GFPT` outside `MAPSTA`, View extents) or `kExitNfbpoBounds`
  (`NFBPO > 9`, beyond `NBO(0:9)` and `NDSL(9)`). The `ISEA` read from `ISBPO` on the device is not
  checked, as in the Fortran.

## Not ported

- The `#ifdef` branches above.
- `W3DMO5`, `W3NESTUG`, `W3GFPT`, `W3CSPC`: hooks. `iobc_shape_w3dmo5` gives the extents of blocks 1
  and 3 and nothing else of `W3DMO5` (not `BBPI0`/`BBPIN`, not its checks).
- The text of the FORMATs: the caller formats them from `IobcStatus`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet: no WW3 regtest ran, and the port ran only in the review's scratch driver (below). What was
done, 2026-10-16 (v):

- the Fortran object built from the pin with WW3's Release flags in the pinned `#ww3` shell, its
  sections compared with the CMake object, its GIMPLE and assembly read (above);
- a throwaway translation unit that includes `iobc_w3iobc.hpp` and explicitly instantiates
  `w3iobc` on `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial` and
  `iobc_gather_shrd` on OpenMP, compiled with `g++ -std=c++20 -O3 -march=x86-64-v3 -fopenmp -Wall
  -Wextra -Wpedantic -ffp-contract=off` against the pinned `kokkos-openmp-5.2.2`: no warning, also
  with `-Wconversion -Wshadow`; no `vfmadd`/`vfmsub` in the object; the same unit at `-O0 -g
  -fsanitize=address,undefined`: no warning;
- a copy of `wave_support.cpp` with the include and the two instantiation lines added
  (`ww::iobc::iobc_gather_shrd<E>`, `ww::iobc::w3iobc<E>`), compiled with the `openmp-release` and
  `serial-debug` flags plus the target's warnings (`-Wshadow -Wconversion -Wno-unused-parameter`):
  no warning;
- the throwaway unit in the `#cuda` shell with `nvcc_wrapper -std=c++20 -O3 -arch=sm_89
  --fmad=false --expt-extended-lambda -Xcompiler -ffp-contract=off` against `kokkos-cuda-5.2.2`
  (CUDA 12.9): no warning, compile only. The copy of `wave_support.cpp` does not compile for CUDA,
  with three errors in other ports' headers (`__builtin_assoc_barrier` undefined in
  `ic3_wn_cmplx_hf.hpp`, `profs_dlassq.hpp`, `tide_dpythag.hpp`) and none in this one.

Fresh review, 2026-10-16 (v), after the fixes: a unit instantiating `w3iobc` and
`iobc_gather_shrd` on OpenMP, compiled with the `openmp-release` flags of
`kokkos/build/openmp-release/build.ninja` (`-O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra
-Wpedantic -Wshadow -Wconversion -Wno-unused-parameter -ffp-contract=off -fopenmp`) and at `-O0 -g
-fsanitize=address,undefined`: no warning; the gather kernel's GIMPLE unchanged by the fixes, no
`vfmadd` in the object. A scratch driver ran `w3iobc('READ')` on three crafted `nest.<ext>`
files (a valid one to its end, one cut inside the spectra, one with a 4-byte time record) and a
directory, against the gfortran probe: the same `IOTST`, exit codes, IOSTAT, `TIME1`, `TIME2` and
`NBI2` on each. That driver is review evidence, not a test of the port (ADR-0010), and is not
committed. CUDA was not rebuilt after the fixes (host code only).

The `serial-debug`, `openmp-release` and `cuda-release` presets of `kokkos/` were not built.
