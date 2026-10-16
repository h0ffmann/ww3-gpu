# W3FLGRDFLAG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Output set-up run once per list: the blank-separated list of field names `FLDOUT` (for example
`'HS DIR WND'`) is split, each name upper-cased and matched by `W3FLDTOIJ` to its group and field
indices, and the flag `FLG2D(IFI,IFJ)` of each match is set; then `FLG1D(IFI)` is set to `ANY`
of its group, and the output processor lists the selected fields. No floating-point arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLGRDFLAG`, `model/src/w3iogomd.F90:589-749` at NOAA-EMC/WW3 `03adda91`, author F. Ardhuin (name matching factored out into `W3FLDTOIJ` by C. Bunney). `w3iogomd` is compiled by every switch; the only `#ifdef` is `W3_S` (`STRACE`, tracing only, not translated). The `#ifdef` cases of the field names (`W3_BT4`, `W3_IS2`, `W3_OASACM`, `W3_OASOCM`, `W3_COU`) are in `W3FLDTOIJ`, not here |
| Callers | `ww3_shel.F90:953` (field list), `:1063` (coupling list, `W3_COU`), `:1077` and `:1390` (extra restart fields; `:1077` is reached by every namelist run of `ww3_shel`, so every regtest that runs `ww3_shel` calls it at least once), `ww3_ounf.F90:357`, `wminitmd.F90:4788` (`ww3_multi`) |
| Kokkos code | [`../src/ww_kokkos/iogo_w3flgrdflag.hpp`](../src/ww_kokkos/iogo_w3flgrdflag.hpp), `ww::iogo::w3flgrdflag`, an `inline` host function template |
| Structural change | The three routines it calls are callables the shim passes, because none is inlined in the object (it calls `__w3servmd_MOD_strsplit`, `__w3servmd_MOD_str_to_upper`, `__w3iogomd_MOD_w3fldtoij`) and none is translated: `STRSPLIT` (`w3servmd.F90:1882`), `STR_TO_UPPER` (`w3servmd.F90:1942`), `W3FLDTOIJ` (`w3iogomd.F90:764-1188`, which keeps its own `WRITE`s of formats 1004 and 1008). The `WRITE(NDSO,1945/1946)` lines (`:723`, `:726`, `:734`) become the returned count of lines; the shim rebuilds them from `FLG2D` (IFI outer, IFJ inner, `IDOUT(IFI,IFJ)`, format 1945 for the first, 1946 for the rest, or 1945 `'no fields defined'`). `NDSO`, `NDSS`, `NDSEN` are not arguments (`NDSS` is unused in the Fortran too). `NOGRP = 10`, `NGRPP = 20` are the `PARAMETER`s of `w3odatmd.F90:330-331`. The local `OUT_NAMES(100)`, `TESTSTR` stay local arrays of the host function |
| C shim | none yet; it passes `FLG1D`, `FLG2D` as `LOGICAL(4)` storage, binds `IAPROC`, `NAPOUT`, `NDSEN` into the `W3FLDTOIJ` callable and does the `WRITE`s |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3iogomd.F90` recompiled with the flags of WW3's CMake Release
build under `switch_lab_shrd` (gfortran 16.2.0, pinned shell); the whole object's disassembly is
identical to the CMake object of `build_lab`. Compared with a throwaway C++ unit (the template
instantiated on three `extern "C"` functions) compiled warning-free with the `openmp-release`
flags (g++ 16.2.0), 2026-10-16.

- (v) No floating-point operation in either object.
- (v) Same effects in the same order: `IERR = 0`; `FLG2D` zeroed as 800 bytes (`memset`,
  `0x320`); `FLG1D` zeroed as 40 bytes; `OUT_NAMES` filled with blanks (`0x20202020`, 10000
  bytes); one call of `STRSPLIT`; then per name a blank test, `STR_TO_UPPER` in place, a copy of
  100 bytes into `TESTSTR`, `W3FLDTOIJ` on `TESTSTR`, and `FLG2D(IFI,IFJ) = 1` (word offset
  `(IFI-1) + 10(IFJ-1)`) unless `IFI = -1`.
- (v) `LEN_TRIM(OUT_NAMES(IOUT+1)) /= 0` is `_gfortran_string_len_trim`, which trims blanks only;
  the C++ tests for any byte other than a blank (the object vectorises the test, integer only).
- (v) `ANY(FLG2D(IFI,:))` is a loop of stride 40 bytes over 20 words that stops at the first
  non-zero word, in both. `LOGICAL(4)` is tested as the whole word against zero and written as
  `0` or `1`.
- ⚠ The Fortran has no bound on `IOUT` (`:701`): with 100 names in `FLDOUT` the loop reads
  `OUT_NAMES(101)`, past the array. In the `-O3` object that address is `TESTSTR` (still holding
  name 100, so the loop goes on and calls `W3FLDTOIJ` on it again), then the stack-protector
  slot. `STRSPLIT` itself writes past `TAB(100)` with more than 100 names
  (`w3servmd.F90:1929-1932`, `TAB(*)` assumed size). The C++ stops at 100 names, the only
  defined reading; with 99 names or fewer the two agree. 100 names of three letters fit in the
  1024 characters of `FLDOUT`, so the path is reachable from a namelist.
- (v) The C++ sets `ifi`, `ifj` to 0 before the `W3FLDTOIJ` callable; `W3FLDTOIJ` sets both to
  `-1` first (`:803-804`), so nothing reads the initial value.
- CUDA not compiled (host-only function).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
