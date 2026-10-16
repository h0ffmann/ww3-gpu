# GET_ARRAY_SIZE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim that passes the
flags and the `GET_ARRAY_SIZE` hook of `DO_OUTPUT_EXCHANGES`.

- (v) Fresh review 2026-10-16: rebuilt `pdlib_field_vec.F90` from WW3's CMake Release build
  under `switch_PDLIB` (own build, disassembly identical to the CMake object) and read its `-O3`
  GIMPLE. All 80 flag tests (78 `IF` blocks of `:118-386`, two of `:387-391`) and their
  increments match the source, the C++ and the object, in order (script plus the multipliers of
  the summed flags read in the GIMPLE). Index maps of `FLOGRD`/`FLOGR2` (`(j-1)+(k-1)*10`),
  `E3DF` (`(i-1)+(k-1)*3`) and `P2MSF(2:3)` (`[1]`, `[2]`) checked. Defects fixed: the groups 3
  and 4 (`:172-241`) had been collapsed into two loops over `k`, a refactor with no precedent in
  the port reports, now restored as the Fortran's 20 straight `IF`s; the raw-word count of the
  `LOGICAL` ⚠ was 55, it is 57 of 80. Line citations checked; the C++ recompiled with zero
  warnings (command 3 below).

Under `PDLIB`, `DO_OUTPUT_EXCHANGES` packs the requested gridded output fields of each process
into one `REAL` array `ARRexch(TheSize, NSEAL)` (`model/src/pdlib_field_vec.F90:877-879`).
`GET_ARRAY_SIZE` computes `TheSize`, the number of columns: one per field whose output flag
`FLOGRD` or coupling flag `FLOGR2` is on, more for the fields that carry several components,
frequencies (`E3DF`, `P2MSF`, `NK`) or swell partitions (`NOSWLL`). It counts integers; there is
no `REAL` arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `GET_ARRAY_SIZE`, `model/src/pdlib_field_vec.F90:53-393` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), identical at NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 8e433f99 -- model/src/pdlib_field_vec.F90` is empty), as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it. Its profile row is `docs/data/ww3_pdlib_gprof.tsv:79`, 0.00 s: no measured cost |
| Caller | `DO_OUTPUT_EXCHANGES`, `pdlib_field_vec.F90:877`, when `FLOUT(1) .OR. FLOUT(7)`; the only call in `model/src` |
| Kokkos code | [`../src/ww_kokkos/pdlib_get_array_size.hpp`](../src/ww_kokkos/pdlib_get_array_size.hpp), `ww::pdlib::get_array_size`, an `inline` host function; `TheSize` (`INTENT(OUT)`) is the return value |
| Not ported, by switch | none: the body has no preprocessor branch; `W3_S` (not in the switch) only adds `USE W3SERVMD, ONLY: STRACE` (`:98-100`), never called |
| Structural change | None in the body: the 78 `IF` blocks of `:118-386`, the `DO` loops inside them and the `DO I=1,NOEXTR` of `:387-391` are written out as in the Fortran, in its order. `NOGRP = 10`, `NGRPP = 20`, `NOEXTR = 2` are the `PARAMETER`s of `w3odatmd.F90:330-335`. `FLGRDALL` stays a local array |
| Inputs | `FLOGRD`, `FLOGR2` (`w3odatmd.F90:491`, pointers to `OUTPTS(IMOD)%OUT1`, `:1696-1697`) as 200 contiguous `LOGICAL(4)` words, column-major; `E3DF(3,5)`, `P2MSF(3)` (`w3gdatmd.F90:1101`, pointers to the fixed-size `GRIDS(IMOD)%E3DF(3,5)`, `%P2MSF(3)`, `:715`, `:2309-2310`) as `INTEGER(4)`, column-major; `NK`, `NOSWLL` by value |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic and order

Reference: the `-O3` object of `pdlib_field_vec.F90` compiled with the flags of WW3's CMake
Release build under `switch_PDLIB` (gfortran 16.2.0, pinned `ww3` shell), whose disassembly is
identical to the CMake object's, read as GIMPLE (`-fdump-tree-optimized`). The C++ is a
throwaway unit calling `get_array_size`, compiled with the `openmp-release` flags (g++ 16.2.0,
same shell), zero warnings, read as GIMPLE. 2026-10-16. Commands under
[Reproduce](#reproduce).

- (v) Integer only: `IH`, the loop bounds and `TheSize` are `INTEGER(4)` (`:106-108`), `int32_t`
  in the C++; no floating-point operation in either GIMPLE.
- (v) The flag tests are the same 78 (`FLGRDALL(j,k)`), in the same order, with the same
  increments: a script listed the `(j,k)` of every `IF ... THEN` of `:118-386` with its body and
  matched it against the C++, all 78 written out.
  `(2,18)` is not counted, in both (`:166-171`); group 10 is `(10,1)`, `(10,2)` (`:387-391`,
  unrolled in the object).
- (v) `FLGRDALL(J,K) = FLOGRD(J,K) .OR. FLOGR2(J,K)` (`:112-116`) is short-circuit in the
  object: `FLOGRD` word nonzero gives 1, otherwise the `FLOGR2` word is copied. The C++ stores
  `(flogrd != 0) || (flogr2 != 0)`. Both tests then read the word as nonzero.
- (v) `IH = IH + NOSWLL + 1` (`:198`) is `(IH + NOSWLL) + 1` in the C++; the object computes
  `NOSWLL + 1` first and adds it. `IF` blocks with two or three `IH = IH + 1` become
  `IH + 2*flag`, `IH + 3*flag` in both objects, and both reassociate the sum of the single-flag
  groups. Addition modulo 2^32 is associative, so the order of these sums cannot change
  `TheSize`.
- (v) `DO IK=E3DF(2,k),E3DF(3,k)` (`:173`), `DO K=P2MSF(2),P2MSF(3)` (`:315`): both objects skip
  when the lower bound exceeds the upper, otherwise add the trip count `hi - lo + 1` (a
  vectorised induction, with a scalar loop below 26 iterations in the Fortran, 22 in the C++,
  and when `hi = INT32_MAX`). `DO IK=1,2*NK` (`:310`, `:327`): `2*NK` in `INTEGER(4)`, added when
  `2*NK > 0`, in both.
- (v) `ww3_grid` clamps `E3DF(2:3,k)` and `P2MSF(2:3)` to `1..NK` (`w3gridmd.F90:2992-3005`,
  `:3010-3011`), and `W3IOGR` reads them back from `mod_def.ww3` (`w3iogrmd.F90:1196`).
- ⚠ Where the Fortran is undefined the two may differ, and neither is defined: `IH` overflowing
  `INTEGER(4)`; `2*NK` overflowing (`NK > 2^30`); an upper bound of `INT32_MAX`, where the `-O3`
  Fortran's scalar loop never ends; and a `LOGICAL` word other than 0 or 1, for which the object
  adds the raw `FLOGR2` word to `IH` for the 57 of the 80 flags it sums without a branch (the 55 of
  `:118-386` and `(10,1)`, `(10,2)`; every flag except the 23 that guard `NOSWLL` or a loop:
  `(3,1..5)`, `(4,1..15)`, `(6,8)`, `(6,9)`, `(6,12)`; listed from the GIMPLE by a script), times
  2 or 3 for the blocks with two or three increments, and the C++ adds 1, 2 or 3. The C++ does not
  mirror this: it is gfortran's code for a `LOGICAL` it assumes is 0 or 1, not the Fortran's
  statement. WW3 writes `.TRUE.`/`.FALSE.` into the flags (`W3FLGRDFLAG`,
  `w3iogomd.F90:589-749`, sets them to 1).
- ⚠ The shim does not exist: `FLOGRD`, `FLOGR2`, `E3DF` and `P2MSF` are module pointers, read in
  the object through their descriptors' strides; the C++ assumes contiguous column-major storage,
  as `OUTPTS(IMOD)%OUT1%FLOGRD(NOGRP,NGRPP)` (`w3odatmd.F90:358`) and `GRIDS(IMOD)%E3DF(3,5)`
  (`w3gdatmd.F90:715`) are. The shim must pass that storage, not a strided section.
- CUDA not compiled (host-only function).

## Reproduce

All in `nix develop "path:<repo>/picasso/labs/pratico#ww3"`, scratch directory
`/tmp/claude-1000/gas-scratch/`:

1. `cmake -S <repo>/WW3 -B ww3 -DCMAKE_BUILD_TYPE=Release -DSWITCH=<copy of switch_PDLIB>` and
   `cmake --build ww3 --target ww3_lib -j 8` (WW3's own build; nothing run).
2. `gfortran` with `Fortran_DEFINES`, `Fortran_INCLUDES` and `Fortran_FLAGS` of
   `ww3/model/src/CMakeFiles/ww3_lib.dir/flags.make` (`-O3 -fallow-argument-mismatch -g
   -fno-second-underscore -ffree-line-length-none -DSCOTCH_707 -O3`, the `W3_*` defines of the
   switch) plus `-fdump-tree-optimized -c pdlib_field_vec.F90`; `objdump -d` of that object and of
   the CMake one differ only in the file name.
3. `g++ -O3 -march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -ffp-contract=off -fopenmp
   -I<repo>/kokkos/src/ww_kokkos -fdump-tree-optimized -c tu.cpp`, where `tu.cpp` wraps
   `get_array_size` in one non-inline function. Zero warnings.

Nothing was linked or run except two throwaway Python scripts: one matched the flag order of the
two source files, one classified the `FLGRDALL` loads of the Fortran GIMPLE as branched or summed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest reaches it until `DO_OUTPUT_EXCHANGES` is wired; the
PDLIB entries of `matrix.base` that write gridded output would be its denominator ⚠.
