# GSU_CREATE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only, with no test run (2026-10-16). Its checkpoint (ADR-0011) needs a shim, the
`W3GNTX` wiring and a port or a Fortran call for the `W3NNSC` hook.

- (v) Fresh review 2026-10-16: `w3gsrumd.F90` recompiled from the `WW3/` pin with the
  `flags.make` of WW3's CMake build under `switch_lab_shrd` plus `-fdump-tree-optimized-lineno`, in
  the pinned `ww3` shell (disassembly identical to the CMake object); the GIMPLE and the relocated
  assembly of `gsu_create` read against the header: no FMA and no `fmod` call in the Fortran
  (8 inline `fprem`, `__builtin_iround` lowered to one `lround` call), `cvtss2sd` widening, the
  `(real(kind=4))` then `(real(kind=8))` divisor of `DXB`/`DYB` (`:7624`), the 32-bit `INT` then
  `+1` of `:7744`/`:7746`, the NaN-aware `MINVAL`/`MAXVAL` with `HUGE` when either dimension is
  empty (`:7597`), `JB1`/`JB2` as stack arrays stored only at `:7755`, `:7759`, `:7780`, `:7787`;
  the C++ unit rebuilt with the `openmp-release` flags (`-O3 -march=x86-64-v3 -ffp-contract=off`,
  `-Werror`, 0 FMA). Every `file:line` of the report, the header and the glossary draft checked at
  `03adda91`. The throwaway differential program was rerun against the fresh object (not kept,
  ADR-0010; nothing of it is in the repository, `git status`) with six more cases: `IJG = .FALSE.`
  with `ICLO_TRPL`, `NNP=3` and lower bounds -2 and 0; longitudes beyond ±360 through `MOD`; a
  zero `DXB` (NaN quotients, `INT_MIN`); an empty first and an empty second dimension (`HUGE`,
  `-HUGE`, a `-Infinity` `DXB`); a `-0.0` first longitude with a column of NaN latitudes. All 23
  cases byte-identical in stdout, stderr and exit status at `-O3` and under ASan/UBSan. The -130
  of the first undefined pole-branch cell is kept: the `-O3` object reads an uninitialised stack
  slot there, so no value is the Fortran's, and stopping is the only choice that does not invent
  one. No defect found; two unreachable divergences added below as ⚠.

`GSU_CREATE` builds the grid-search-utility object of `W3GSRUMD` for a logically rectangular grid
(rectilinear or curvilinear, Cartesian or spherical). It records the grid's index closure
(`ICLO`), whether the grid is periodic in longitude (`LCLO`) and in which range its longitudes
lie (`L360`), the bounding box of the search domain (forced to `[0,360]` in longitude when
periodic, to the pole in latitude when a cell contains one), and a bucket-search structure:
`NBY` x `NBX` buckets of extent `DXB` x `DYB`, each listing the cells whose vertex box overlaps it.
Point and field interpolation (`W3GRMP`, `W3GFCL`, `W3GFPT`) search it. Under
`switch_lab_shrd` the model reaches it through `W3IOGR` (`w3iogrmd.F90:1039`, every grid that is
not unstructured) calling `W3GNTX` (`w3gdatmd.F90:3017-3018`), which calls `W3GSUC` with
`IJG = .FALSE.` (`:2976`), the grid's `ICLOSE` and its `REAL` `XGRD`, `YGRD` pointers, so
through `W3GSUC_PTR_R4` (`w3gsrumd.F90:625-650`), with no `NCB`, `NNP` or `DEBUG`.

## What was ported

| | |
|---|---|
| WW3 routine | `FUNCTION GSU_CREATE`, `model/src/w3gsrumd.F90:7354-7849` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), byte-identical to NOAA-EMC/WW3 `03adda91` (v, `git -C WW3 diff 03adda91 HEAD -- model/src/w3gsrumd.F90` is empty, 2026-10-16); author T. J. Campbell, NRL (`:21`), "Origination" 30-Oct-2009 (`:26`) |
| Kokkos code | [`../src/ww_kokkos/gsru_gsu_create.hpp`](../src/ww_kokkos/gsru_gsu_create.hpp): `ww::gsru::gsu_create(GsuCreateArgs, GsuCreateHooks)`, host-side; it creates no View and runs no kernel |
| Data | `GsuClass` is `CLASS_GSU` (`:332-354`), `Bkt` is `T_BKT` (`:358-362`), `Nns` is `T_NNS` (`:366-373`). The grid coordinates are not copied: `PTR%XG4 => XG4` (`:7465-7470`) keeps the caller's array, and so does `GsuClass`. `B(NBY,NBX)` is a vector in column-major order, `bkt(jb,ib)` |
| Callees | `W3NNSC` (`:6819-6879`) is not translated: it is the hook `GsuCreateHooks::w3nnsc`, called at the same two points as the Fortran (`:7482`/`:7484` with `NNP` or `NNP_DEFAULT = 2`, unless `BBOX_ONLY`; `:7829` with `NINT(HALF*MAX(NBX,NBY))`). An empty hook returns -101 (`kExitMissingHook`) at the point of the call |
| Output | The debug `WRITE(*,...)` records, under the `DEBUG` argument, are appended to `GsuCreateResult::out`; the error `WRITE(0,...)` records to `err`. `EXTCDE` is returned as `exit_code` (it ends in `CALL EXIT`, `w3servmd.F90:1207`) |
| Not ported, by switch | `W3_S` (`STRACE`, `:7382-7385`), the routine's only `#ifdef`; `switch_lab_shrd` does not set it (v, `gfortran -E` with the defines of `flags.make`) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3gsrumd.F90` compiled with the flags of WW3's CMake Release
build under `switch_lab_shrd` (`-O3 -g -fallow-argument-mismatch -fno-second-underscore
-ffree-line-length-none`, no `-ffp-contract` and no `-march`, gfortran 16.2.0 in the pinned `ww3`
shell), whose disassembly is identical to the CMake object's (v, `cmp` of the two `objdump -d`,
2026-10-16), read as GIMPLE (`-fdump-tree-optimized-lineno`) and as relocated assembly. The C++
is a throwaway unit with the `openmp-release` flags (`-O3 -march=x86-64-v3 -ffp-contract=off`),
read the same way. No FMA in either object (v, `grep vfmadd`: 0).

- (v) The vertex coordinates are `REAL(8)` locals: an R4 grid is widened by `cvtss2sd` at each
  load (`:7537-7552`, `:7700-7715`); the C++ does `static_cast<double>` of the `float`.
- (v) The branch-cut test `ABS(XC(K)-XC(L)) .GT. D180`, `K = MOD(L,4)+1` (`:7564`, `:7734`),
  subtracts in the order `XC(2)-XC(1)`, `XC(3)-XC(2)`, `XC(4)-XC(3)`, `XC(1)-XC(4)` in the
  GIMPLE; the C++ writes `xc[(l+1)%4] - xc[l]`. `ABS` is `ABS_EXPR` (`vandpd` in the C++).
- (v) `MINVAL` and `MAXVAL` of a `REAL` array are gfortran's NaN-aware inline reductions, both for
  `YC` (`:7574`, `:7580`, `:7754`, `:7758`) and for the whole `XG4`/`YG4` or `XG8`/`YG8` array
  (`:7597-7601`): the limit starts at `+Inf` (`-Inf`), the first element `<=` (`>=`) it starts the
  reduction, the rest replace it only when strictly less (greater), `_28 ? _350 : limit` in the
  GIMPLE; no element gives `HUGE` (`-HUGE`) of the array's kind (the constant
  `3.4028234663852885981170418348451692544e+38` for R4, `1.797...e+308` for R8), all NaN gives
  NaN. `minval_nan()` and `maxval_nan()` are that algorithm; the C++ compiles the strict compare
  to `vminsd`/`vmaxsd` with the element as first operand, which keeps the limit on a NaN or an
  equal value, as the GIMPLE's select does. A signed-zero tie keeps the first zero (case 9 below
  starts `YMIN` at `-0.0` and prints `-0.0000000000000000E+00` in both).
- (v) `XC = MOD(XC,D360)` is `__builtin_fmod`, an inline x87 `fprem` loop in both objects (the
  C++ also keeps a `fmod` call for the `errno` path). `fmod` is exact, so both give the same value
  for finite input; for Inf or NaN both give a NaN, whose sign bit no later statement reads (it
  only reaches comparisons, `INT` and the `NaN` text of `Ew.d`).
- (v) `DXB = (XMAX-XMIN)/REAL(NBX)`: `REAL(NBX)` is a default `REAL`, `cvtsi2ss` then
  `cvtss2sd` in the object (`:7624-7625`, vectorised as one `divpd` for `DXB` and `DYB`), so the
  divisor is `(double)(float)NBX`; the C++ writes exactly that (`vcvtsi2ss`, `vcvtss2sd`).
- (v) `INT((XC(L)-XMIN)/DXB)` and the `YC` one (`:7744`, `:7746`) are 32-bit `cvttsd2si`, which
  gives `INT_MIN` for a NaN or out-of-range quotient; `fortran_int()` returns that without C++'s
  undefined conversion (the same rule as `fint` of `st4_params.hpp` for `REAL`, here for
  `REAL(8)`), and the `+1` follows. Case 9 below has a NaN vertex and prints `******` for that
  `IBC`, in both.
- (v) `NINT(HALF*MAX(NBX,NBY))` (`:7829`) is `(double)MAX * 0.5` then a call of `lround`, of
  which the low 32 bits are kept; the C++ calls `std::lround` on the same product.
- (v) Integer `MOD`, `/` and `MIN`/`MAX` of the index closure and the bucket bounds are C++'s `%`,
  `/`, `std::min`/`std::max` (truncating, as Fortran). `NX = 0` in a closed direction divides by
  zero in both, as in the Fortran.

## Where the Fortran is undefined

- ⚠ In the pole branch, `JB1(1)` and `JB2(1)` are set only when `MINVAL(YC) > 0` or
  `MAXVAL(YC) < 0` (`:7754-7761`). A cell in the pole branch (one branch-cut crossing, or exactly
  one vertex at ±90) that straddles the equator, has a vertex at 0 latitude or has only NaN
  latitudes sets neither. The locals then keep the previous cell's values,
  across cells and across `ISTEP`; the C++ keeps them too (case 10 below exercises it, byte-identical
  to the Fortran). For the first such cell of the routine the value is undefined in the Fortran;
  the C++ returns -130 (`kExitUndefinedBounds`) instead of guessing it.
- ⚠ A bucket index outside `1..NBX` x `1..NBY`, or a second-pass count above the first pass's
  (possible only through the stale bounds above), would write outside `B` or `B%I`/`B%J`; the C++
  returns -131 (`kExitBucketBounds`). No grid read for this report reaches it.
- ⚠ `N` is undefined until the first `LLG` cell (`:7561`); it is printed only by the allocation
  failure message of `:7660-7672`. The C++ starts it at 0.
- ⚠ A quotient of `:7744`/`:7746` in `[2147483647, 2147483648)` makes `INT(...)+1` overflow: the
  object wraps to `INT_MIN`, the C++ addition is undefined. It needs a `DXB` some 1e-9 of the
  domain; not exercised.
- ⚠ `ALLOCATE(PTR%B(NBY,NBX))` (`:7640`) returns `ISTAT = 5014`, not 5020, when `NBY*NBX*136`
  bytes overflow 64 bits (the GIMPLE's `> 135637824071393761` test); the C++ maps a failed
  `resize` to 5020 and does not catch the `std::length_error` it throws past `max_size()`.
  Unreachable: the grid itself would hold more than 1e17 points.
- After `BBOX_ONLY` returns (`:7611-7614`), `NBX` .. `NNB` are undefined in the Fortran; the C++
  leaves them 0, empty and null. No caller at the pin passes `BBOX_ONLY` (v, `grep -n BBOX_ONLY
  model/src/*.F90` finds only `GSU_CREATE`).

## Messages

- (v) The `FORMAT` text, compared with gfortran 16.2 output: `(/1A,1A,1I2/)` with two items stops
  at `I2` and writes one empty record and the message; with `ICLO` it also writes the trailing
  empty record; `(/1A,2A,3I6/)` overflows to `******`; `Ew.d` uses `0.d1..d16E+nn`, or
  `0.d1..d16+nnn` without the `E` when the exponent exceeds 99 (case 11), and right-justified
  `NaN` and `Infinity`; `Lw` and `Iw` are right-justified, `(3A6,4A14)` pads the column titles.
- (v) The allocation failures return `ISTAT = 5020` (`LIBERROR_ALLOCATION`, the constant the
  GIMPLE stores at `:7449`) after the same message; the C++ maps `std::bad_alloc` to it. Not
  exercised: a host that runs out of memory here.

## How it was checked

All on 2026-10-16, in the pinned `ww3` shell (and `#cuda` for nvcc). Nothing of this port ran
inside WW3; no regtest, smoke or ctest was run, as the owner asked.

- (v) Static parity (ADR-0011): the GIMPLE and assembly of both objects read as above.
- (v) A throwaway differential program, not kept in the repository (ADR-0010): the `-O3` Fortran
  object called through `W3GSUC` with `DEBUG=.TRUE.` and then `W3GSUP(GSU, 6, .TRUE.)`, which
  prints the object in full (flags, domain, `NBX`, `DXB`, every bucket's bounds, count and cell
  list, both `T_NNS`), against the C++ with a test copy of `W3NNSC` in the hook and a C++ copy of
  `W3GSUP`'s prints. 13 grids and 4 error paths; stdout, stderr and the exit status identical
  (`cmp`) for all 17, at `-O3 -march=x86-64-v3` and at `-O0` with ASan/UBSan (34 runs, 1.5 MB of
  text). The grids: a regional rectilinear grid (`ICLO_NONE`); global grids on `[0,350]` and
  `[-180,170]` (`ICLO_GRDI`, `LCLO`, `L360` false then `XMIN` forced to 0); a grid with pole rows
  and lower bounds 0 and -3; a curvilinear grid across the dateline without closure; a tripole
  grid (`ICLO_TRPL`, the `CYCLE` of `:7682-7684` and the fold of `:7533-7536`); `IJG = .FALSE.` in
  R8 with `ICLO_GRDJ` and in R4 with `ICLO_GRDI` through `W3GSUC_PTR_R4`, the shape `W3GNTX`
  passes; a Cartesian R8 grid with `ICLO_TRDL` and `NCB=3`; a NaN vertex with a `-0.0`; the stale
  pole-branch bounds; cells with one vertex at +90 and at -90 (`LNPL`, `LSPL`); coordinates of
  1e150 and 1e-120. Error paths: `ICLO` 5 and -7, odd `NX` with `ICLO_TRPL`, `NCB=0`. The
  no-coordinates message and the allocation message were compared with a two-line gfortran
  program; the paths -101, a hook error code, -130 and `BBOX_ONLY` ran under ASan/UBSan.
- (v) Builds: the header alone, `-O3 -march=x86-64-v3` and `-O0 -fsanitize=address,undefined`,
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror` (g++ 16.2); a copy of
  `wave_support.cpp` with the two lines of the instantiation, in the `openmp-release` and
  `serial-debug` presets (0 warnings); a unit of its own with `nvcc_wrapper -arch=sm_89
  --fmad=false` (CUDA 12.9, build only, nothing run on the GPU).

## Benchmarks

None. `GSU_CREATE` runs once per grid at set-up, from `W3IOGR`; no committed profile times it.
No number enters a table before the WW3 regtest replay (AGENTS.md, "No timing without parity").

## Regtested

No: translated only, as the owner asked on 2026-10-16. The checkpoint needs the shim and the
`W3GNTX` hunk; the regtests that exercise it are every `matrix.base` entry with a structured grid,
since `W3IOGR` calls `W3GNTX` for each.
