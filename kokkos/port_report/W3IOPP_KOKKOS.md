# W3IOPP_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only, with no test run (2026-10-16). Its checkpoint (ADR-0011) needs a shim that
fills the hooks below and the `w3iopp` hook of the Kokkos `W3INIT` (`init_w3init.hpp`), and the
`W3INIT` hunk.

- (v) Fresh review 2026-10-16: `w3iopomd.F90` recompiled from the pin in the pinned `ww3` shell
  with the `flags.make` of WW3's CMake build under `switch_lab_shrd` (`.text`, `.rodata*`
  identical to the CMake object, the optimized GIMPLE byte-identical to the translator's;
  `gfortran -E` of `W3IOPP` identical, with no `W3_RTD`, `W3_O7a`, `W3_MPI`, `W3_T` or `W3_S`
  line, and none of their names in the C++). Read against the port: the 33 `nf90_err_check`
  operands resolved from the `C.*` statics (556 to 679, the source lines of the `nf90_err` macro,
  `:8`, in source order, as the C++ passes them), the specific NetCDF procedures with their
  `START`/`COUNT` arrays and their constant operands, the four chained `MAPSTA` loads, `GTYPE`
  reloaded per point, the two `cvtss2sd` before `IS_IN_UNGRID` (and the two `vcvtss2sd`, no
  other floating-point instruction and no FMA in the C++ `-O3` object of `w3iopp`), and
  `iopo_shape_w3dmo2` against `W3DMO2` (`w3odatmd.F90:1060-1144`). An independent sample of
  21 582 751 `REAL(4)` bit patterns (every 199th from 7) gives `F10.3` and `E10.3`
  byte-identical to gfortran 16.2.0, and FORMATs 1000-1003 for 4 001 points with a name holding
  inner blanks are byte-identical. The driver was run in two throwaway harnesses with mock hooks
  (ASan/UBSan): a failing `nf90_inq_varid` of `PTNME` returns `exit_code` 0 with `nc_line` 591,
  `NOPTS` already 5 and the four `NF90_ERR_CHECK` records byte-identical to gfortran's; a
  structured grid with one point out of the grid, one on land and two kept stores `NOPTS` 2 with
  the expected corners and weights. Every `w3iopomd.F90`, `w3odatmd.F90`, `w3triamd.F90`,
  `w3gsrumd.F90` and `w3initmd.F90` citation checked. No defect found; the headers are unchanged.

`W3IOPP` prepares the point output. For each output point the caller of `W3INIT` requests it asks
the grid search `W3GRMP` (or, on an unstructured grid, the triangle search `IS_IN_UNGRID`) for the
four corner points of the enclosing cell and their bilinear weights, skips a point outside the grid
or with four land corners (`MAPSTA = 0`) with a warning on `NDSE`, and stores the location, the
corner indices `IPTINT`, the weights `PTIFAC` and the name of every accepted point in
`OUTPTS(IMOD)%OUT2`. Only the bilinear weights are stored: with ice possible, the actual
interpolation factors are known only at output time (`w3iopomd.F90:314-318`). On an unstructured grid it reads
these tables from `pnt_wght.<ext>.nc` when that file exists, and otherwise writes them to
`out.pnt_wght.<ext>.nc`. `W3INIT` calls it once per grid when point output is requested
(`w3initmd.F90:1262`, the non-MPI call).

## What was ported

| | |
|---|---|
| WW3 routine | `W3IOPP`, `model/src/w3iopomd.F90:242-859` at NOAA-EMC/WW3 `03adda91`, and its NetCDF error handler `NF90_ERR_CHECK` (`:1301-1315`); identical in the `h0ffmann/WW3` fork at the `WW3/` pin (`git -C WW3 diff 03adda91 HEAD -- model/src/w3iopomd.F90` is empty, v 2026-10-16), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/iopo_w3iopp.hpp`](../src/ww_kokkos/iopo_w3iopp.hpp): `ww::iopo::w3iopp` (host driver, no kernel), `IoppHooks`, `IoppNetcdf`, `IoppUnits`, `IoppStatus` |
| Shared state | [`../src/ww_kokkos/iopo_state.hpp`](../src/ww_kokkos/iopo_state.hpp), for `W3IOPP`, `W3IOPE` and `W3IOPO`: `IopoState` (`OUTPTS(IMOD)%OUT2`, `w3odatmd.F90:364-388`), `iopo_shape_w3dmo2` (the arrays and fills of `W3DMO2`, `:1086-1135`), the module's NetCDF names (`w3iopomd.F90:122-223`), and the gfortran `Fw.d`, `Ew.d` and list-directed edits the messages use |
| Reused | `FChar`, `FLogical`, `IogrState` (the grid fields `GTYPE`, `FLAGLL`, `FILEXT`, `NX`, `NY`, `MAPSTA`), `kUngType`, `kExitMissingHook`, `kExitShape` of the `W3IOGR` port ([`W3IOGR_KOKKOS.md`](W3IOGR_KOKKOS.md)) |
| Calls not ported here | Hooks the caller fills (`IoppHooks`): `W3DMO2` (`:438`; `iopo_shape_w3dmo2` reproduces its arrays, not its checks or `W3SETG`/`W3SETO`), `W3GRMP_R4` (`:490`, `w3gsrumd.F90:2521-2566`; the closure holds the grid search object `GSU`), `IS_IN_UNGRID` (`:492`, `w3triamd.F90:1582-1762`), every NetCDF-Fortran call (`IoppNetcdf`, with the Fortran arguments), `nf90_strerror`, and the formatted unit `NDSE`. `INQUIRE(EXIST=)` (`:468`) is ported as libgfortran does it, `access(F_OK)` on the trimmed name, and can be replaced by a hook. An empty hook on the path ends with `kExitMissingHook` (-101), never a skipped call |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

What `switch_lab_shrd` compiles in `W3IOPP` (v, `gfortran -E` with the defines of WW3's
`flags.make`, 2026-10-16; no `MPI_BCAST`, `W3LLTOEQ`, `STRACE` or `SCREEN,940` survives it):
the `FLAGLL` test, `W3DMO2`, `GRDID = FILEXT`, the weight-file branch for `UNGTYPE`, the loop over
points with `W3GRMP` or `IS_IN_UNGRID`, the warnings 1000-1003, the store, and the NetCDF read and
write of the weights. Not compiled and not ported: `W3_RTD` (`:443-457`, `:481-485`, `:535-540`,
`:684-686`), `W3_O7a` (the diagnostic map, `:690-812`, its formats `:818-830`), `W3_MPI`
(`:614-631`, the broadcast of the weights read from file), `W3_T` (`:477-479`, `:507-511`,
`:846-855`) and `W3_S`. `MPI_COMM_IOPP` is read only under `W3_MPI`.

## Arithmetic and control flow

The reference is the `-O3` object of `w3iopomd.F90` compiled with the flags of WW3's CMake Release
build under `switch_lab_shrd` (gfortran 16.2.0, pinned `ww3` shell), whose `.text`, `.rodata`,
`.rodata.cst4`, `.rodata.str1.1` and `.rodata.str1.8` are byte-identical to the CMake object
(v, `objcopy` and `cmp`, 2026-10-16), read as GIMPLE (`-fdump-tree-optimized`) and assembly.

- (v) No floating-point arithmetic on the compiled path. The assembly of
  `__w3iopomd_MOD_w3iopp` has two `cvtss2sd` (the `DBLE(XPT)`, `DBLE(YPT)` for `IS_IN_UNGRID`) and
  no other SSE arithmetic, compare or conversion; the C++ object of `w3iopp` has the same two
  `vcvtss2sd` and no other. `FACTOR` (`:432-436`) is read only by `W3_O7a`; the GIMPLE has no
  trace of it.
- (v) `GRDID = FILEXT` (`:439`) runs after `W3DMO2` and fills every element `W3DMO2` allocated,
  through a temporary, with the `FILEXT` of the grid `W3SETG` last selected: `W3DMO2` switches to
  `IMOD` and back (`w3odatmd.F90:1081`, `:1144`). The same holds for `GTYPE`, `FLAGLL`, `MAPSTA` and
  `GSU`; the caller passes that grid's `IogrState`.
- (v) `FILENAME` is `'pnt_wght.'//FILEXT(:LEN_TRIM(FILEXT))//'.nc'` blank padded to 124
  (`_gfortran_string_len_trim`, two `_gfortran_concat_string`, `memset` of blanks); at most 25
  characters, never truncated. The `INQUIRE` passes the 124-character name; libgfortran's
  `file_exists` trims trailing blanks and calls `access(path, F_OK)` (`io/unix.c:1912-1918`,
  `runtime/string.c:35-42`, `:128-135`, gcc 16.2.0 source, read 2026-10-16).
- (v) `GTYPE` is reloaded on every iteration of the point loop (GIMPLE `pretmp_1132 = *gtype`);
  the C++ tests `g.gtype` each time. `INGRID` from `W3GRMP_R4` is tested `!= 0`, from
  `IS_IN_UNGRID` as `ITOUT > 0`, `FLAGLL` as `!= 0`.
- (v) The four `MAPSTA` tests (`:515-518`) are evaluated in source order, each only when the
  previous one held (four chained branches in the GIMPLE), as the C++ loop does.
- (v) `IX`, `IY`, `RD` are locals kept from one point to the next; the C++ keeps them outside the
  loop, zero before the first call where the Fortran's are undefined.
- (v) The store (`:531-548`) writes `PTLOC(1:2,NOPTS)`, `IPTINT(1:2,K,NOPTS)`, `PTIFAC(K,NOPTS)` and
  `PTNME(NOPTS)` (a 40-byte `memcpy`) in the order of the C++.
- (v) The weight-file branch reads and writes only on `IAPROC = 1` (not `NAPERR`). Every NetCDF
  call is followed by `NF90_ERR_CHECK(ncerr, __LINE__)` and `RETURN` on an error; the 33 line
  numbers the object passes (static `C.*` operands of the GIMPLE: 556, 560, ..., 679) are the source
  lines of the checks and are the ones the C++ passes. `NOPTS = d_nopts_len` (`:563`) is assigned
  before the remaining reads, so an early return leaves it set, as in the Fortran.
- (v) The NetCDF arguments are those of the specific procedures the GIMPLE calls:
  `nf90_get_var_2d_fourbytereal` (`PTLOC`, `PTIFAC`), `nf90_get_var_1d_text` (`PTNME`, length 40),
  `nf90_get_var_3d_fourbyteint` (`IPTINT`, with a two-element `START` and a three-element `COUNT`),
  `nf90_def_var_manydims`, and the `put` counterparts on the sections `PTLOC(:,1:NOPTS)`,
  `PTNME(1:NOPTS)`, `IPTINT(:,:,1:NOPTS)`, `PTIFAC(:,1:NOPTS)`; `NF90_NOWRITE = 0`,
  `NF90_NETCDF4 = 4096`, `NF90_CHAR = 2`, `NF90_FLOAT = 5`. `IPTINT`, an `INTEGER` array, is
  defined as `NF90_FLOAT` (`:657`), a quirk kept: the conversion is the library's.

### Output bytes

- (v) FORMATs 1000-1003 (`:832-844`): an empty record, `' *** WAVEWATCH-III WARNING :'`, the
  point line (`2F10.3` when `FLAGLL`, `2E10.3` otherwise, then `2X` and the 40-character name), the
  literal `'     POINT SKIPPPED '` with its trailing blank and its misspelling, and an empty record.
  The bytes of all four, written by the C++ for one point, are identical to a gfortran 16.2.0
  program with the same FORMATs (`cmp`, 2026-10-16).
- (v) `fmt_f` and `fmt_e` (`iopo_state.hpp`) follow libgfortran's `FDTOA`/`DTOA` (`snprintf`
  `"%+-#.*f"`, `"%+-#.*e"`), `build_float_string` and `build_infnan_string`
  (`io/write_float.def`, gcc 16.2.0 source). `F10.3` and `E10.3` of 20 613 008 `REAL(4)` values
  (every 1021st bit pattern of the 2^32, 4 000 000 random patterns, the neighbours of every
  multiple of 0.0005 in [-1000, 1000], and of 4-digit decimal ties, every 7th mantissa, at each
  power of ten from 1e-41 to 1e38, both signs) are byte-identical to gfortran's, and so are ±0, ±Inf, quiet and signalling NaNs,
  ±HUGE, the smallest normal and subnormal (`cmp`, 2026-10-16).
- (v) `NF90_ERR_CHECK`'s four list-directed records: a leading blank, the character items with
  no separator, and the default `INTEGER` right-justified in 12 characters; byte-identical to
  gfortran for line 556, for `-2147483648`, an empty and a 120-character message (`cmp`).

### What the C++ refuses where the Fortran is undefined

- ⚠ A corner index outside `MAPSTA(NY,NX)` returned by a hook, or a weight file whose dimensions
  ask for more elements than `W3DMO2` allocated (`d_nopts_len > NPT`, or `VSIZE`, `NAMELEN`,
  `WGHTLEN` larger than 2, 40, 4), makes the Fortran read or write out of bounds (no bounds
  checks in the Release flags). The C++ returns `kExitIndexBounds` (-120) instead. Within bounds
  the reads fill the arrays contiguously from their first element, as the library does with the
  actual argument, so a file with `VSIZE` other than 2 is laid out the same wrong way.
- ⚠ Arrays `W3DMO2` leaves undefined (`IL`, `IW`, `II`, `DPO` .. `SPCO`, `GRDID` before `:439`) are
  zero or blank in `iopo_shape_w3dmo2`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet: no WW3 regtest, and the port's driver was never run inside WW3; only the fresh
review's throwaway harnesses with mock hooks ran it (above). Throwaway programs also compared the two float edits, the four warning FORMATs and the list-directed
records with gfortran 16.2.0 (above). Compiled, 2026-10-16 (v):

- a throwaway translation unit that takes the address of `w3iopp`, `iopo_shape_w3dmo2`, `fmt_f`
  and `fmt_e`, with `g++ 16.2.0 -std=c++20 -O3 -march=x86-64-v3 -fopenmp -Wall -Wextra -Wpedantic
  -ffp-contract=off` against the pinned `kokkos-openmp-5.2.2`: no warning, none from the headers
  with `-Wconversion -Wshadow`, no `vfmadd`/`vfmsub` in the object;
- the same unit at `-O0 -g -fsanitize=address,undefined`: no warning;
- the same unit in the `#cuda` shell with `nvcc_wrapper -std=c++20 -O3 -arch=sm_89 --fmad=false
  --expt-extended-lambda -Xcompiler -ffp-contract=off -Xcompiler -fopenmp` against
  `kokkos-cuda-5.2.2` (CUDA 12.9): no warning, compile only;
- a copy of `kokkos/src/ww_kokkos/wave_support.cpp` with the include and `(void)&ww::iopo::w3iopp;`
  added, at the `openmp-release` and `serial-debug` flags: no warning.

The `serial-debug`, `openmp-release` and `cuda-release` presets of `kokkos/` were not built.
