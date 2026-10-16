# W3IOGR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim,
the hooks below filled by the ports of the routines W3IOGR calls, and the `W3IOGR` hunk first.

Writes and reads the model definition file `mod_def.<ext>`: the grid, the spectral grid and the
parameters of every compiled package, as `ww3_grid` writes them and `ww3_shel` reads them. On a
read it rebuilds the few tables it derives (the rectilinear `XGRD`/`YGRD`, `MAPSTA` and `MAPST2`
from the packed map, `MAPSF(:,3)`) and calls `DISTAB` once per process.

## What was ported

| | |
|---|---|
| WW3 routine | `W3IOGR`, `model/src/w3iogrmd.F90:116-2236` (the module ends at `:2240`) at NOAA-EMC/WW3 `03adda91`, identical in the `h0ffmann/WW3` fork at the `WW3/` pin (`git diff 03adda91 HEAD -- model/src/w3iogrmd.F90` is empty), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/iogr_state.hpp`](../src/ww_kokkos/iogr_state.hpp) (host state, the module fields W3IOGR transfers, their kinds and Fortran shapes; shape helpers for the allocating routines), [`../src/ww_kokkos/iogr_unformatted.hpp`](../src/ww_kokkos/iogr_unformatted.hpp) (gfortran sequential unformatted, big-endian, records), [`../src/ww_kokkos/iogr_w3iogr.hpp`](../src/ww_kokkos/iogr_w3iogr.hpp) (the routine, host; the derived tables as `KOKKOS_INLINE_FUNCTION`s with four launchers on any execution space) |
| C shim | none yet |
| Calls not ported here | `W3DIMX`, `W3DIMUG`, `W3GNTX`, `W3DIMS`, `W3DMO5`, `INSIN4`, `INSNL1`, `INSNLGQM` are hooks (`IogrHooks`) the caller fills; `DISTAB` calls its port ([`DISTAB_KOKKOS.md`](DISTAB_KOKKOS.md)) into the process-wide `IogrSaved`, since its `W3DISPMD` tables are module variables every grid shares and `FLDISP` runs it once per process. An empty hook on the path ends with `kExitMissingHook`, never a skipped call |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

The switch is `F90 NOGRB NOPA LRB4 NC4 SHRD PR3 UQ FLX0 LN1 ST4 STAB0 NL1 BT1 DB1 MLIM TR0 BS0 IC0
IS0 REF0 XX0 WNT1 WNX1 RWND CRT1 CRX1 O0 ... O14`. Inside `W3IOGR` that compiles the records of
LN1, ST4, NL1, BT1, DB1, RWND and the PR3 GSE record; the TNAME literals of LN1, ST4, NL1, BT1,
DB1, TR0, BS0, UQ, PR3 (FLX0 and IC0 leave theirs as dashes); and no MPI. The lines were read
from the preprocessed source (`gfortran -E` with the defines of WW3's `flags.make`).

## Record layout

Unformatted sequential, `CONVERT='big_endian'` (`:580-581`, `:589-590`; `FILE_ENDIAN =
ENDIANNESS`, `constants.F90:87`, `'big_endian'` from WW3's CMake). Each record is a big-endian
4-byte length, the payload and the length again. Kinds are those of the GIMPLE's
`_gfortran_transfer_*` calls: `i4` INTEGER, `i2` INTEGER\*2, `i1` INTEGER\*1, `r4` REAL, `r8`
REAL(8)/DOUBLE PRECISION, `l4` LOGICAL, `cN` CHARACTER(LEN=N). Arrays are whole, in Fortran
element order. Lines are WRITE / READ in `model/src/w3iogrmd.F90`.

| Rec | When | Fields, in order | Lines |
|---|---|---|---|
| 1 | always | IDSTR c35, VERGRD c10, NX, NY, NSEA, NTH, NK, NBI, NFBPO i4, GNAME, FNAME0..FNAME6, FNAMEP, FNAMEG, FNAMEF, FNAMEI c30 | `:599-603` / `:645-649` |
| 2 | always | NBO(0:NFBPO), NBO2(0:NFBPO) i4 | `:629-630` / `:751-752` |
| 3 | always | GTYPE i4, FLAGLL l4, ICLOSE i4 | `:769-770` / `:969` |
| 4 | GTYPE = RLGTYPE, SMCTYPE | SX, SY, X0, Y0 r4 | `:782-783` / `:984` |
| 4 | GTYPE = CLGTYPE | REAL(XGRD)(NY,NX), REAL(YGRD)(NY,NX) r4 | `:790-791` / `:994` |
| 4 | GTYPE = UNGTYPE | FSN..DO_CHANGE_WLV l4 (11), SOLVERTHR_STP, CRIT_DEP_STP r8, NTRI, COUNTOT, COUNTRI, NNZ i4, B_JGS_TERMINATE_MAXITER, _DIFFERENCE, _NORM, B_JGS_LIMITER, B_JGS_BLOCK_GAUSS_SEIDEL, B_JGS_USE_JACOBI l4, B_JGS_MAXITER i4, B_JGS_PMIN, B_JGS_DIFF_THR, B_JGS_NORM_THR r8, B_JGS_NLEVEL i4, B_JGS_SOURCE_NONLINEAR l4 | `:798-817` / `:1003-1022` |
| 5 | GTYPE = UNGTYPE | X0, Y0, SX, SY, DXYMAX r4, XGRD(1,NX), YGRD(1,NX) r8, TRIGP(3,NTRI) i4, TRIA(NTRI), LEN(NTRI,3), IEN(NTRI,6) r8, ANGLE0, ANGLE(NTRI,3) r4, SI(NX) r8, MAXX, MAXY, DXYMAX r4, INDEX_CELL(NX+1), CCON(NX), COUNTCON(NX), IE_CELL(COUNTOT), POS_CELL(COUNTOT) i4, IOBP(NX) i2, IOBPA(NX), IOBDP(NX), IOBPD(NTH,NX) i1, IAA(NX+1), JAA(NNZ), POSI(3,COUNTOT) i4 | `:860-864` / `:1029-1033` |
| next | always | ZB(NSEA) r4 (ZB(NX) unstructured), MAPTMP(NY,NX), MAPFS(NY,NX), MAPSF(NSEA,3), TRFLAG i4 | `:878-879` / `:1040-1041` |
| next | TRFLAG ≠ 0 | TRNX(NY,NX), TRNY(NY,NX) r4 | `:913` / `:1075` |
| next | always | DTCFL, DTCFLI, DTMAX, DTMIN, DMIN, CTMAX, FICE0, FICEN, FICEL, PFMOVE r4, FLDRY, FLCX, FLCY, FLCTH, FLCK, FLSOU, FLBPI, FLBPO l4, CLATS, CLATIS, CTHG0S(0:NSEA) r4, STEXU, STEYU, STEDU, IICEHMIN, IICEHINIT r4, IICEDISP l4, ICESCALES(1:4) r4, CALTYPE c8, CMPRTRCK l4, IICEHFAC r4, then IICEHDISP, IICEDDISP r4 on WRITE but IICEDDISP, IICEHDISP on READ, IICEFDISP, BTBETA r4, IC_NUMERICS l4, AAIRCMIN, AAIRGB r4 | `:917-924` / `:1085-1092` |
| next | always | GRIDSHIFT r8 | `:943` / `:1095` |
| next | always | MAPWN, MAPTH(NSPEC+NTH) i4, DTH r4, TH(NTH), ESIN, ECOS, ES2, ESC, EC2(NSPEC+NTH) r4, XFR, FR1 r4, SIG(0:NK+1), SIG2(NSPEC), DSIP(0:NK+1), DSII(NK), DDEN(NK), DDEN2(NSPEC) r4, FTE, FTF, FTWN, FTTR, FTWL, FACTI1, FACTI2, FACHFA, FACHFE r4 | `:1154-1157` / `:1169-1172` |
| next | always | E3DF(3,5), P2MSF(3), US3DF(3), USSPF(2) i4, USSP_WN(25) r4; INXOUT='GRID' closes the file here (`:1200-1203`) | `:1188-1189` / `:1196` |
| next | not GRID | XBPO, YBPO(n), RDBPO(n,4) r4, IPBPO(n,4), ISBPO(4n) i4, n = NBO(NFBPO) | `:1209-1210` / `:1218` |
| next | not GRID | IHMAX i4, HSPMIN, WSMULT, WSCUT r4, FLCOMB l4, NOSWLL, PTMETH i4, PTFCUT r4 | `:1238-1240` / `:1249-1251` |
| next | not GRID | FACP, XREL, XFLT, FXFM, FXPM, XFT, XFC, FACSD, FHMAX, FFACBERG, DELAB r4, FWTABLE(0:300) r4 | `:1263-1265` / `:1346-1348` |
| next | RWND | RWINDC r4 | `:1274-1275` / `:1351` |
| next | LN1 | SLNC1, FSPM, FSHF r4 | `:1442` / `:1448` |
| next | ST4 | ZZWND, AALPHA, ZZ0MAX, BBETA, SSINTHP, ZZALP, TTAUWSHELTER r4, SSWELLFPAR i4, SSWELLF(7), SSINBR, ZZ0RAT, SSDSC(21) r4, SSDSISO i4, SSDSBR, SSDSBT, SSDSBM(0:4), SSDSP, SSDSCOS, SSDSDTH, WWNMEANP, WWNMEANPTAIL, SSTXFTF, SSTXFTFTAIL, SSTXFTWN, SSTXFTF, SSTXFTWN, SSDSBRF1, SSDSBRF2 r4, SSDSBRFDF i4, SSDSBCK, SSDSABK, SSDSPBK, SSDSBINT, FFXPM, FFXFM, FFXFA, SSDSHCK r4, IKTAB(NK,2000) i4, DCKI, QBI(2000,1300) r4, SATINDICES(2·(NTH/2−1)+1,NTH) i4, SATWEIGHTS(same) r4, DIKCUMUL i4, CUMULW(NSPEC,NSPEC), SINTAILPAR(5), CAPCHNK(10) r4 | `:1541-1552` / `:1591-1602` |
| next | ST4, SINTAILPAR(1) > 0.5 | DELUST, DELTAIL, DELTAUW, DELU, DELALP r4, TAUT(0:200,0:200), TAUHFT(0:100,0:200) r4 | `:1579-1580` / `:1606-1608` |
| next | and TTAUWSHELTER > 0 | TAUHFT2(0:100,0:200,0:50) r4 | `:1581` / `:1611` |
| next | NL1 | SNLC1, LAM, KDCON, KDMN, SNLS1, SNLS2, SNLS3 r4, IQTPE i4, NLTAIL r4, GQNF1, GQNT1, GQNQ_OM2 i4, GQTHRSAT, GQTHRCOU r4, GQAMP(4) r4 | `:1648-1651` / `:1662-1665` |
| next | BT1 | SBTC1 r4 | `:1839` / `:1844` |
| next | DB1 | SDBC1, SDBC2 r4, FDONLY l4 | `:1871-1872` / `:1879` |
| last | PR3 | WDCG, WDTH r4 | `:1957` / `:1962` |

- (v) Kinds, order and the conditions: every `_gfortran_transfer_*` call of the `-O3` GIMPLE of
  `w3iogrmd.F90` (gfortran 16.2, WW3's Release flags and the lab defines), read 2026-10-16. The
  `.text` and `.rodata*` sections of that object are byte-identical to the object of a WW3 CMake
  Release build of the fork under `switch_lab_shrd` (configured in a scratch directory; only the
  debug sections differ, by path).
- (v) The layout against files WW3 wrote, read 2026-10-16 with a scratch script that walks the
  record markers (nothing of the port ran): `build/profile-bench_small/mod_def.ww3` of the
  `ww3-gpu-hoff` worktree (lab switch, the `bench_small` 120×80 grid, NSEA 9204, NK = NTH = 24)
  and `build/runs/ww3-smoke/mod_def.ww3` of `ww3-gpu-noaa` (3×3, one sea point, NK 36, NTH 24).
  Both have 21 records, markers big-endian, and every length is the one the table gives: 433
  bytes for record 1 (35+10+7·4+12·30), 8 for NBO with NFBPO = 0, 12, 16 (rectilinear), no
  TRNX record (TRFLAG 0), 4·(9·NSPEC+8·NTH+4·NK+16) for the spectral record, 192, 0 for the
  boundary record (NBO(0) = 0), 32, 1248, 4, 12, 24 078 732 for ST4 (6 019 683 words with
  NSPEC 864), 242 828 and 4 141 404 for the stress tables, 72, 4, 12, 8. Record 1 of both
  carries the lab's package names. In the smoke file FLAGLL reads `00000001`: `.TRUE.` is
  stored as 1 and a LOGICAL is byte-swapped like an INTEGER.
- (v) The WRITE lists `IICEHDISP, IICEDDISP` and the READ `IICEDDISP, IICEHDISP`
  (`:922-923` / `:1090-1091`; the GIMPLE transfers them in those orders), so WW3 swaps the two
  ice parameters on every read. Reproduced, not corrected (`rec_times`).
- (v) The WRITE branch zeroes `COUNTCON` and `IOBDP` of the state before writing the
  unstructured mesh (`:858-859`); reproduced.
- (v) Records over 2 147 483 639 bytes (`GFC_MAX_SUBRECORD_LENGTH`, `libgfortran/libgfortran.h`)
  are split into subrecords, the leading marker negative when another follows and the trailing
  one negative when one preceded (`io/transfer.c` `next_record_w_unf`, `us_write`; gcc 16.2.0
  source from the nix store, read 2026-10-16). The same layout was read in a file a standalone
  gfortran 16.2 program wrote with `-fmax-subrecord-length=16` (40 bytes: markers -16/16,
  -16/-16, 8/-8). ⚠ Not checked at the real length; no lab grid reaches it (CUMULW alone needs
  NSPEC > 23 000).
- (v) IOSTAT values are libgfortran's `LIBERROR_*` (`gcc/fortran/libgfortran.h:121-147`), and an
  OS error sets errno instead (`runtime/error.c` `generate_error_common`). The standalone
  program gave 5016 for a list past the record end, 5017 for a payload cut short, 5009 for a
  marker cut short, -1 at the end of the file, 2 (`ENOENT`) for a missing file at `OPEN`, 5007
  for a READ of a file opened write-only, and exit status 2 for a WRITE to a file opened
  read-only. A READ reads each item from the file and seeks over the rest of the record and its
  trailing marker, which is never read (`io/transfer.c` `read_block_direct`, `skip_record`):
  a file cut inside the last trailing marker reads with IOSTAT 0. The reader does the same.
- (v) An `OPEN` without `ACTION` refused read-write retries read-only, then write-only
  (`io/unix.c` `regular_file2`), so a read-only `mod_def` opened for WRITE fails at the first
  WRITE (exit 2) and a write-only one opened for READ fails at the first READ (exit 52), not at
  the `OPEN` (exit 50); the port reproduces both. `CLOSE` ignores a failed flush (`io/close.c`),
  as the port's `fclose` does. ⚠ An OS write error (a full disk) surfaces at the WRITE whose
  buffer flush fails, which depends on libgfortran's and stdio's buffer sizes, so the record
  that exits 2 can differ.
- (v) `GFORTRAN_CONVERT_UNIT` overrides the OPEN's `CONVERT` (`io/open.c:830-843`; the program
  wrote little-endian records with `GFORTRAN_CONVERT_UNIT='little_endian:10'`). ⚠ The port
  ignores it: a WW3 run with that variable set for the `mod_def` unit is not reproduced.
- ⚠ Endianness of the host: the conversion uses `std::endian`, so a big-endian host copies
  bytes as they are; only x86-64 was compiled.

## Arithmetic

The reference is the `-O3` object of `w3iogrmd.F90` described above, read as GIMPLE and
assembly, 2026-10-16. `W3IOGR` itself does almost no floating-point work: the frequency and
direction tables (`SIG`, `DSIP`, `TH`, `ESIN`, ...) are written by `ww3_grid` and transferred,
not computed here.

- (v) `XGRD(:,IX) = REAL(X0 + REAL(IX-1)*SX)` (`model/src/w3iogrmd.F90:987`) and the `YGRD`
  statement (`:990`): the Fortran object computes `cvtsi2ss` of `IX-1`, `mulss` by `SX`,
  `addss` of `X0`, `cvtss2sd`, once per column; `rect_coord()` compiles to `vcvtsi2ss`,
  `vmulss`, `vaddss`, `vcvtss2sd` (openmp-release flags), the same operations in float, no FMA.
  The C++ recomputes the value per element instead of per column: the same bits.
- (v) The WRITE of `REAL(XGRD)` (`:791`) is one `cvtsd2ss` per element in both; the READ of
  `XGRD4` (`:996-997`) widens exactly.
- (v) `HUGE(X0)` (`:1000-1001`) is stored as `3.4028234663852885981170418348451692544e+38`
  in the GIMPLE, `FLT_MAX`.
- (v) `MAPTMP = MAPSTA + 8*MAPST2` (`:768`), `MAPSTA = MOD(MAPTMP+2,8) - 2`, `MAPST2 =
  (MAPTMP-MAPSTA)/8` (`:1071-1072`) and `MAPSF(:,3) = MAPSF(:,2) + (MAPSF(:,1)-1)*NY`
  (`:1073`) are integer `%` and `/` truncating toward zero in the GIMPLE and in C++.
- (v) The tests `SINTAILPAR(1).GT.0.5` and `TTAUWSHELTER.GT.0` (`:1578`, `:1581`, `:1604`,
  `:1610`) are float compares with `5.0e-1` and `0.0` in the GIMPLE; the C++ compares with
  `0.5f` and `0.f`.
- (v) Each launcher (`iogr_rect_xy`, `iogr_decode_maptmp`, `iogr_encode_maptmp`,
  `iogr_mapsf3`) computes one element from its own inputs, with no reduction and no atomic,
  so the bits do not depend on the execution space or the order.
- (v) CUDA, `sm_89`, `--fmad=false`, compiled with `nvcc_wrapper` in the pinned `#cuda` shell
  and never run: the `iogr_rect_xy` kernels have `I2FP.F32.S32`, `FMUL`, `FADD`,
  `F2F.F64.F32` and no `FFMA`; the other three kernels are integer only.
- ⚠ `-ffp-contract=off` is private to the `ww_kokkos` target: code outside it that includes
  this header lets GCC fuse `rect_coord`'s product and sum (v: with the same flags minus
  `-ffp-contract=off` it compiles to one `vfmadd132ss`).
- The Fortran's checks of IDSTR, VERGRD, NFBPO and the package names follow its order and exit
  codes (10, 11, 13, 14-20, 22, 24; `FNAMEI` exits 17 and reports index 3, as `:711-716`
  does); `EXTOPN` exits 50 and `EXTIOF` exits 52 for IOSTAT > 0 and 51 for IOSTAT < 0
  (`w3servmd.F90:1042`, `:910-923`). The messages are not printed: `IogrStatus` carries the
  FORMAT label and values for the caller.
- (v) A failing WRITE has no IOSTAT in the Fortran, so gfortran stops with a run-time error and
  exit status 2 (`runtime/error.c` `generate_error`, `exit_error(2)`; the standalone program
  exited 2); the port returns `kExitGfortranRuntime` (2). `EXTOPN` gets errno as IERR.
- `FLDISP` is a `SAVE` local (`:353`) and `EWN1`, `ECG1`, `N1MAX`, `DSIE` are `W3DISPMD` module
  variables (`w3dispmd.F90:78-79`): `IogrSaved`, one per process, not per grid. `FNMPRE`, `DELAB`,
  `FWTABLE`, `CALTYPE` and the `W3SRC4MD` tables are module variables too, kept in `IogrState`;
  WW3 shares them between grids, the last READ winning, so a multi-grid caller shares them.

## Not ported

- Every `#ifdef` branch `switch_lab_shrd` does not compile: the records and TNAMEs of the
  other source-term, flux, ice, reflection and propagation packages (for example ST0-ST3, ST6,
  NL2-NL5, BT4, IS1, IS2, REF1, UOST, PR1, PR2, UNO), the `W3_MPI` items, `W3_T` test output,
  `W3_ASCII`, `W3_SMC`, `W3_TRNK` and `W3_S` STRACE.
- `print_memcheck` (`:382-2029`): a diagnostic write, not a model state.
- The routines W3IOGR calls (hooks above). The shape helpers in `iogr_state.hpp` give the
  extents of `W3DIMX`, `W3DIMUG`, `W3DIMS`, `W3DMO5` and of `INSIN4`'s allocations so a reader
  can be assembled; they are not ports of those routines.
- `W3SETO`/`W3SETG` (`:568-569`): the caller passes the state of grid IGRD.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing of the port was run. What was done, 2026-10-16 (v): a throwaway translation
unit that includes `iogr_w3iogr.hpp` and instantiates the four launchers on
`Kokkos::DefaultExecutionSpace` and `w3iogr` compiled with the `openmp-release` flags (`-O3
-march=x86-64-v3 -fopenmp -Wall -Wextra -Wpedantic -ffp-contract=off`, Kokkos 5.2.2 OpenMP of
the pinned `#ww3` shell) with no warning, also with `-Wconversion -Wshadow`, and no FMA in the
object; a copy of `wave_support.cpp` with the same lines added compiled with no warning from these
headers; the same unit compiled for CUDA (`#cuda` shell, Kokkos 5.2.2 CUDA, `sm_89`) with no
warning.

- (v) Fresh review 2026-10-16: every record of the READ and WRITE paths against the source and a
  regenerated `-O3` GIMPLE (WW3 CMake configure of the `WW3/` pin under `switch_lab_shrd` in a
  scratch directory; `.text` and `.rodata*` of the object identical to the CMake one), the
  arithmetic (`:768`, `:987`, `:990`, `:791`, `:996`, `:1000-1001`, `:1071-1073`), both quirks
  (`:922-923`/`:1090-1091`, `:858-859`), the hooks and every line cited here; the gfortran
  format settled from the libgfortran 16.2.0 source and a standalone gfortran program (above).
  Defects found and fixed: the reader read the whole record and its trailing marker at the
  start of a READ, so a cut file gave IOSTAT 5017 where gfortran gives 0 or -1; `EXTOPN` got
  5000 instead of errno; the `OPEN` fallbacks of `regular_file2` were missing (exit 50 instead
  of 2 or 52); the `DISTAB` tables sat in the per-grid `IogrState` while `FLDISP` is per
  process, so a second grid read never got them. The four headers recompiled after the fixes
  in the pinned `#ww3` shell (Kokkos 5.2.2 OpenMP), explicit instantiations of the four
  launchers on `Kokkos::DefaultExecutionSpace` and `w3iogr` with the shape hooks, `-std=c++20
  -O3 -march=x86-64-v3 -fopenmp -Wall -Wextra -Wpedantic -ffp-contract=off`, also with
  `-Wconversion -Wshadow`: no warning, no `vfmadd`/`vfmsub` in the object, `rect_coord` is
  `vcvtsi2ss`, `vmulss`, `vaddss`, `vcvtss2sd`. Nothing of the port was run.
