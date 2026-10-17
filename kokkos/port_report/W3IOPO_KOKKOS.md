# W3IOPO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only, with no test run (2026-10-16). Its checkpoint (ADR-0011) needs a shim that
fills the hooks below, a `W3IOPO` hook in the Kokkos `W3WAVE` (the call at `w3wavemd.F90:2694`,
after `W3IOPE`), and the `W3WAVE` hunk.

- (v) Fresh review 2026-10-16: `w3iopomd.F90` recompiled from `WW3-noaa/` with the
  `flags.make` of WW3's CMake build under `switch_lab_shrd` in the pinned `ww3` shell
  (`.text`, `.rodata`, `.rodata.cst4`, `.rodata.str1.1` identical to the CMake object); in its
  GIMPLE the `IPASS2` increment through the entry pointer before `w3seto`, the static `WRITE`,
  the three `open_parm` (`big_endian`, `status = "OLD"` on `:2328` only), the `REWIND`s, the
  transfer kinds of every record, `IW`, `II`, `IL` stored as 0 before `st_write` of `:2533`, the
  trip count loaded once and the `CLOSE` only under `OFILES(2) == 1` were read again; no
  floating-point instruction in the Fortran or the C++ `-O3` object. `W3DMO2` runs
  `W3SETG(IMOD)` before it allocates (`w3odatmd.F90:1081`), so `SPCO`'s first extent is grid
  `IGRD`'s `NSPEC` whenever `W3DMO2` allocated it. Re-walked two `out_pnt.ww3` files the
  translator had not used (`ww3_ts1` runs of the `ww3-gpu-hoff` worktree, 7 652 218 bytes,
  2 161 times) with an independent record walker: header 53, point table 48, `TIME` 8, point
  3 517 = 61 + 4·864 bytes; round trip of one of them and of the `ww3_tp1.1` file
  byte-identical (`cmp`), `OFILES(2) = 1` giving 2 161 and 25 one-time files. Sixteen new cut or
  altered files (cut in the first marker, the header, the point table, the `TIME` payload and
  trailing marker, a point record, the next leading marker; a point record 4 bytes short; a
  `TIME` record 4 bytes long; a mismatched trailing marker; a leading marker of 10^8 and of -8;
  `IDSTR`, `VEROPT`, `NK` altered): the C++ `IOSTAT` (-1, 5009, 5016, 5017) at the same record
  and point as a gfortran 16.2.0 probe in every case, exits 10, 11, 12 on the altered header.
  FORMATs 900-904 and `TIMETAG` byte-identical to gfortran again. Found and fixed: the citation
  of `W3SETO`'s `IMOD` tests (`w3odatmd.F90:1631-1640`, not `:1552-1560`); the `line` field
  documented as the first line of the statement (gfortran's run-time message names the last);
  the contract that `entry` and the `select` hook's `out2` are one object when the entry grid
  is `IGRD`, written into the header. Open ⚠ unchanged otherwise.

`W3IOPO` writes the point output file, or reads it back. On `'WRITE'` its first pass opens
`out_pnt.<ext>` and writes the header and the point table; every pass then writes the time and,
per output point, the depth, the wind, the current, the ice and the grid ID with the spectrum
`SPCO(:,I)` that `W3IOPE` interpolated. With `OFILES(2) = 1` every pass writes a whole file of
its own, `<YYYYMMDD>.<HHMMSS>.out_pnt.<ext>`. On `'READ'` (`ww3_outp`, `gx_outp`) it reads the
same records back, one time per call, and returns `IOTST = -1` at the end of the file. `W3WAVE`
calls it on the point-output processor at every point-output time, after `W3IOPE` (`w3wavemd.F90:2686-2700`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3IOPO`, `model/src/w3iopomd.F90:2131-2626` at NOAA-EMC/WW3 `03adda91`; identical in the `h0ffmann/WW3` fork at the `WW3/` pin (`diff` of the two files is empty, v 2026-10-16), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/iopo_w3iopo.hpp`](../src/ww_kokkos/iopo_w3iopo.hpp): `ww::iopo::w3iopo` (host driver, no kernel), `IopoioGrid`, `IopoioSaved`, `IopoioHooks`, `IopoioStatus`, `iopoio_single_grid_hooks`, `rewind_unit` |
| Shared state | [`../src/ww_kokkos/iopo_state.hpp`](../src/ww_kokkos/iopo_state.hpp) of the `W3IOPP` port, unchanged: `IopoState` (`OUTPTS(IMOD)%OUT2`), `iopo_shape_w3dmo2`, `kIdstr`, `kVeropt`. What `W3IOPO` needs beyond `OUT2` (`OFILES`, `TIME`, `NK`, `NTH`, `NSPEC`, `FILEXT`, `FNMPRE`, `FNMPNT`, `NDSE`, `NDST`, the `SAVE` flag) is in this port's own header |
| Reused | `RecordWriter`, `RecordReader` and the `IOSTAT` codes of [`iogr_unformatted.hpp`](../src/ww_kokkos/iogr_unformatted.hpp); `FChar`, `FLogical`, `fstr_eq`, `kExitGfortranRuntime`, `kExitMissingHook`, `kExitShape` of the `W3IOGR` port; `IobcUnits` (the libgfortran `OPEN` model), `kIostatOptionConflict` and `kExitUnitNotConnected` of the `W3IOBC` port ([`W3IOBC_KOKKOS.md`](W3IOBC_KOKKOS.md)), whose header this one includes |
| Calls not ported here | Hooks the caller fills (`IopoioHooks`): `W3SETO`, `W3SETG`, `W3SETW` (`:2282-2284`, one `select` hook that fills `IopoioGrid` for `IGRD`) and `W3DMO2` (`:2364`, `:2464`; `iopo_shape_w3dmo2` reproduces its arrays). `EXTOPN`, `EXTIOF` and `EXTCDE` are reported in `IopoioStatus` with their error code and `IERR`, not called |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

What `switch_lab_shrd` compiles in `W3IOPO` (v, `gfortran -E` with the defines of WW3's
`flags.make`, 2026-10-16): the selection of `IGRD`, the `INXOUT` and mixed-mode checks, the
two open blocks (one file, or one file per time), the header and point-table records, the
`TIME` record and the loop over points. Not compiled and not ported: `W3_ASCII` (the `NDSOA`
argument and the `out_pnt.<ext>.txt` copies), `W3_FLX5` (`TAUAO`, `TAUDO`, `DAIRO` in the point
record), `W3_SETUP` (`ZET_SETO` in the point record), `W3_T` and `W3_S`. Under `W3_BIN2NC`
`W3WAVE` calls `W3IOPON` instead (`w3wavemd.F90:2691-2693`); the lab switch does not define it.

## Arithmetic and control flow

The reference is the `-O3` object of `w3iopomd.F90` compiled with the flags of WW3's CMake Release
build under `switch_lab_shrd` (gfortran 16.2.0, pinned `ww3` shell), whose `.text`, `.rodata`,
`.rodata.cst4` and `.rodata.str1.1` are byte-identical to the CMake object; `.rodata.str1.8`
differs only in the source path (`WW3-noaa/` against `WW3/`) and its padding (v, `objcopy` and
`cmp`, 2026-10-16). Read as GIMPLE (`-fdump-tree-optimized`) and assembly.

- (v) No floating-point arithmetic. The assembly of `__w3iopomd_MOD_w3iopo` (2 024 lines) has no
  SSE arithmetic, conversion or FMA instruction, and neither has the C++ `-O3` object of
  `w3iopo`; every `REAL` goes through `_gfortran_transfer_real(_write)` with kind 4, and the C++
  copies its four bytes, swapped to big-endian.
- (v) `IPASS => IPASS2` is incremented through the pointer `W3SETO` left at entry, before
  `W3SETO(IGRD)` (GIMPLE `*ipass2.156_1 = _3` before `w3seto (&igrd, ...)`); every later test
  reloads `ipass2` and reads `IGRD`'s counter. The C++ takes the entry grid's `IopoState` and the
  `select` hook's one separately; for one grid they are the same object.
- (v) `WRITE` is a `SAVE` `LOGICAL` in `.bss`, so zero before the first assignment, and one for
  all grids (`IopoioSaved`). It is set on the first pass with `OFILES(2) = 0` and on every pass
  with `OFILES(2) = 1`; elsewhere only `WRITE .AND. INXOUT == 'READ'` stops (FORMAT 901,
  `EXTCDE(2)`), so a `'WRITE'` after a reading first pass reads. Kept.
- (v) Branch order: `INXOUT` against `'READ'` then `'WRITE'` (`_gfortran_compare_string`, blank
  padded, as `fstr_eq`), `IPASS == 1` then `OFILES(2) == 0`, `IPASS > 0` then `OFILES(2) == 1`.
  None has a side effect, so the order of the operands cannot change an outcome.
- (v) The three `OPEN`s pass `form = "UNFORMATTED"`, `convert = "big_endian"`, `IOSTAT`, no
  `ACTION`; `:2328` also passes `status = "OLD"` (`open_parm.5`). `IobcUnits::open` makes the same
  `open(2)` calls as libgfortran's `regular_file2` for an `OPEN` without `ACTION`.
- (v) `REWIND` without `IOSTAT`: libgfortran's `st_rewind` (`io/file_pos.c:427-490`, gcc 16.2.0
  source, read 2026-10-16) does nothing on an unconnected unit, and otherwise seeks to 0 and
  clears `AFTER_ENDFILE`. `rewind_unit` does the same.
- (v) The point-table record is `(PTLOC(J,I),J=1,2),I=1,NOPTS` as `2*NOPTS` single
  `transfer_real` calls, then `NOPTS` `transfer_character` of 40 bytes. Each point record is
  `IW`, `II`, `IL` (`transfer_integer`, kind 4), nine `REAL`s, `GRDID` (13 bytes) and
  `SPCO(1:NSPEC,I)` (`NSPEC` single `transfer_real`), in that order. `IW`, `II`, `IL` are stored
  as zero in `OUT2` before the write (`:2530-2532`).
- (v) The loop's trip count `NOPTS` is loaded once, after the `TIME` record (GIMPLE
  `_1071 = *nopts.244_298`).
- (v) `TIMETAG` is `(i8.8,'.'i6.6)` into 15 characters: never overflows the internal record; a
  value that does not fit its field gives asterisks.
- (v) Error codes: `EXTOPN(..., 20)` exits 20; `EXTIOF(..., 21)` exits 22 when `IERR > 0` and 21
  when `IERR < 0` (`w3servmd.F90:822-972`). The `TIME` read alone tests the sign first and
  returns `IOTST = -1` at the end of the file (`:2506-2515`); every other read with `IERR /= 0`
  goes to `EXTIOF`, so a file that ends right after a `TIME` record exits 21.
- (v) `CLOSE(NDSOP)` only when `OFILES(2) = 1`, also after a read; not on the `IOTST = -1` return.

### The file

Checked against out_pnt files WW3 wrote on this machine, none with `SETUP` or `FLX5` in its switch (the `ww3_tp1.1` one under that regtest's switch, the smoke ones under `switch_lab_shrd`) (v, 2026-10-16): the C++ reads every time
of the file with `w3iopo('READ')` and writes each back with `w3iopo('WRITE')` into a new file,
which `cmp` finds byte-identical to the original:

| File | Size (bytes) | NK, NTH, NOPTS | Times |
|---|---|---|---|
| `~/src/WW3/regtests/ww3_tp1.1/work_lab/out_pnt.ww3` | 62 902 | 3, 4, 21 | 25 |
| `build/runs/ww3-smoke/out_pnt.ww3` of the main worktree (`just smoke`, `--ww3`) | 7 652 218 | 36, 24, 1 | 2 161 |
| `../ww3-gpu-noaa/build/runs/ww3-gpu-smoke-openmp/kokkos/out_pnt.ww3` | 7 652 218 | 36, 24, 1 | 2 161 |

With `OFILES(2) = 1` the C++ wrote 25 files from the first, each byte-identical to the header,
the point table and that time's records of the single file. Record lengths: header 53, point
table `48*NOPTS`, `TIME` 8, each point `61 + 4*NSPEC` bytes, read from the record markers. The
harness is a throwaway program (not committed, as ADR-0010 asks), built with ASan and UBSan.

### Read errors

A gfortran 16.2.0 probe built with WW3's Release flags executes the four `READ` statements of
the `'READ'` path in their order and prints the first nonzero `IOSTAT`. On ten files cut or
altered from the `ww3_tp1.1` file, the C++ gets the same `IOSTAT` at the same record and turns
it into the exit code `EXTIOF` would give (v, 2026-10-16):

| File | gfortran `IOSTAT` | C++ |
|---|---|---|
| ends after the `TIME` record of time 3 | -1, point 1 | exit 21 (`EXTIOF`, `IERR < 0`) |
| ends after point 5 of time 3 | -1, point 6 | exit 21 |
| cut inside point 6 of time 3 | 5017 | exit 22, `IOSTAT` 5017 |
| cut inside the `TIME` payload | 5017 | exit 22, 5017 |
| cut inside the `TIME` leading marker | 5009 | exit 22, 5009 |
| ends after time 2 | -1 at `TIME` | `IOTST = -1` on call 3 |
| cut inside the point table | 5017 | exit 22, 5017 |
| cut inside the header | 5017 | exit 22, 5017 |
| `IDSTR` altered | (not tested by the probe) | exit 10, FORMAT 902 |
| `VEROPT` altered | (not tested by the probe) | exit 11, FORMAT 903 |

A `READ` after the end of the file gets `IOSTAT` 5001 (the unit is `AFTER_ENDFILE`), as the
`W3IOBC` review measured, so a call after `IOTST = -1` exits 22.

### Output bytes

- (v) FORMATs 900-904 (`:2581-2593`), with `2I8` values that overflow, and `TIMETAG` for 0, 1,
  -1, 99999999, 100000000 and -2147483648, written by the C++ are byte-identical to a gfortran
  16.2.0 program with the same FORMATs (`cmp`, 2026-10-16).

### What the C++ refuses where the Fortran is undefined

- ⚠ `NOPTS` larger than what `W3DMO2` allocated (a file read with `O2INIT` already true and more
  points): the Fortran transfers out of bounds (no bounds checks in the Release flags). The C++
  returns `kExitShape` (-102).
- ⚠ A transfer on a unit nobody connected (a `'READ'` with `OFILES(2) = 1`, which never opens,
  on a unit the caller did not open): libgfortran would open `fort.<NDSOP>` implicitly with the
  default conversion. The C++ returns `kExitUnitNotConnected` (-105). Not checked against
  gfortran.
- ⚠ After a read error `RecordReader` leaves the failing item unchanged where libgfortran writes
  the bytes it read and swaps the item; the run then exits (21 or 22), so only state read after
  the exit differs.
- ⚠ `SPCO` is addressed with the grid's `NSPEC`; the Fortran uses the descriptor's column stride,
  the `NSPEC` `W3DMO2` allocated with (`w3odatmd.F90:1108`), which is grid `IGRD`'s because
  `W3DMO2` selects that grid first (`:1081`, v). They differ only if a hook allocates `SPCO`
  with another first extent.
- ⚠ An `OPEN` of a unit already connected to the same file: libgfortran keeps the connection and
  its position, `IobcUnits::open` closes and reopens it. `W3IOPO` reaches that only if another
  routine keeps `NDSOP` open on the same `out_pnt` file.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet: no WW3 regtest, and the port was never run inside WW3; only the throwaway harnesses
above ran it. Compiled, 2026-10-16 (v):

- a throwaway translation unit that takes the address of `w3iopo`, with `g++ 16.2.0 -std=c++20
  -O3 -march=x86-64-v3 -fopenmp -Wall -Wextra -Wpedantic -Wconversion -Wshadow
  -ffp-contract=off` against the pinned `kokkos-openmp-5.2.2` in the `ww3` shell: no warning, no
  floating-point instruction or FMA in the object;
- the same unit at `-O0 -g -fsanitize=address,undefined`: no warning;
- the same unit in the `#cuda` shell with `nvcc_wrapper -std=c++20 -O3 -arch=sm_89 --fmad=false
  --expt-extended-lambda -Xcompiler -ffp-contract=off -Xcompiler -fopenmp` against
  `kokkos-cuda-5.2.2` (CUDA 12.9): no warning, compile only (the header is host code);
- a copy of `kokkos/src/ww_kokkos/wave_support.cpp` with the include and
  `(void)&ww::iopo::w3iopo;` added, at the `openmp-release` and `serial-debug` flags: no warning.

The `serial-debug`, `openmp-release` and `cuda-release` presets of `kokkos/` were not built.
