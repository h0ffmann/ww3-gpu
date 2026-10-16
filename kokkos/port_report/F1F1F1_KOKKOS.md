# F1F1F1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The table `F1SF(1:NF1+1)` of ratios f1/f that bound the `GQNF1` intervals of the first
frequency of the Gaussian Quadrature Method (GQM) form of `NL1`, in one of seven layouts chosen by
`IQ_OM1`: four fixed tables (1, 2, 3, 7), a geometric table between 1/9 and 3 around 1/3 (4), a
geometric (5) or arithmetic (6) table from 1/3 to 3. `INSNLGQM` calls it once per grid
(`w3snl1md.F90:1850`), after setting `IQ_OM1` from `GQNF1` (`:1601-1607`: 14, 26, 11, 40, 20 give
1, 2, 3, 4, 7; layouts 5 and 6 are never selected there).

## What was ported

| | |
|---|---|
| WW3 routine | `F1F1F1`, `model/src/w3snl1md.F90:1349-1500` at NOAA-EMC/WW3 `03adda91`, adapted from TOMAWAC. Compiled wherever `NL1` is (392 switch files under `git grep -lw NL1 -- '*switch*'`; NOAA operational `switch_NCEP_gwm`, `switch_NCEP_st2`, `switch_NCEP_st4`, `switch_NCEP_st4sbs`, `switch_NWS_rwps`); reached only with `IQTYPE <= 0` in the `SNL1` namelist (`w3gridmd.F90:1923`, `w3iogrmd.F90:1801`), for example `regtests/ww3_ts1/input/namelists_ST4_T707.nml`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/snl1_f1f1f1.hpp`](../src/ww_kokkos/snl1_f1f1f1.hpp), `ww::snl1::f1f1f1`, an `inline` host function (a table built once) |
| C shim | none yet |
| Plan | issue #42, batch 3 |

The five `WRITE(*,*)` lines that warn of an `NF1` not matching the layout (`:1382`, `:1401`,
`:1432`, `:1448`, `:1475`) become a returned bitmask (`f1f1f1_detail::kNf1Case*`), as in
`snl1_couple.hpp`; the shim writes the message, with `NF1` as it was on entry, since layout 4
overwrites it.

## Arithmetic

Reference: the `-O3` object of `w3snl1md.F90` built with the flags of WW3's CMake Release build
under `switch_lab_shrd` (gfortran 16.2.0, pinned shell), identical by disassembly to the CMake
object; compared with a throwaway C++ unit under the `openmp-release` flags, 2026-10-16.

- (v) Every value stored in the fixed layouts 1, 2, 3 and 7 (15, 27, 12 and 21 entries) has the
  same double bits in both optimised GIMPLE dumps, compared entry by entry by a script; `1.D0/3.D0`
  folds to the same double in both.
- (v) Layout 4: gfortran knows `NF1 = 20` and folds `RAISON = 9.D0**(1.D0/20)` to
  1.1161231740339043749798975113662891089916229248046875 (`0x1.1dba3f92c888bp+0`) with no `pow`
  call; the header writes it as a literal with a `static_assert` against `__builtin_pow`, also
  folded at compile time. The upward loop (`:1455-1457`) multiplies at run time in both objects;
  the downward loop (`:1458-1460`) is folded to ten constants by gfortran and divided at run time
  by g++. A double division is correctly rounded, and the ten quotients recomputed from the same
  operands equal the ten folded constants bit for bit. `NF1` ends at 40 in both.
- (v) Layout 5: one call to glibc's scalar `pow` (double) in each object, `pow(9, 1/NF1)`; the
  Fortran's parentheses around `1.D0/DBLE(NF1)` are a `PAREN_EXPR` with nothing to reassociate.
  gfortran sinks the call under `NF1 >= 1`, g++ makes it before the test; it is pure, so the table
  is the same.
- (v) Layout 6: `(3.D0-1.D0/3.D0)` folds to 2.666666666666666518636930049979127943515777587890625 in
  both; then one division and a running sum, in the same order.
- (v) No reduction, no MIN/MAX, no `INT`; the only vectorisation in either object is of the
  constant stores of the fixed layouts.
- ⚠ `F1SF` is assumed size. Layouts 1, 2, 3, 7 write 15, 27, 12, 21 entries and layout 4 writes
  41, whatever `NF1` is, after only a warning: with an `NF1` smaller than the layout's, the Fortran
  writes past the `GQNF1+1` entries `INSNLGQM` allocates (`:1848`). `INSNLGQM` selects the layout
  from `GQNF1`, so it does not reach that path; the C++ reproduces it (the caller sizes `f1sf`).
- ⚠ `IQ_OM1` outside 1..7 writes nothing. `INSNLGQM` leaves `IQ_OM1` unset when `GQNF1` is none of
  14, 26, 11, 40, 20 (`:1601-1607`), so the Fortran then reads an undefined variable and `F1SF`
  stays uninitialised; the C++ takes `iq_om1` as given.
- CUDA: not compiled; the function is host only.
- (v) Fresh review 2026-10-16: no defect. `w3snl1md.F90` recompiled from `build_lab` (identical to the
  CMake object); the GIMPLE of `f1f1f1` read against a new C++ unit's: every stored constant of the
  seven layouts, the folded `RAISON` of layout 4, the ten quotients of its downward loop recomputed
  by correctly rounded double division (equal to gfortran's folded constants), the glibc `pow` of
  layout 5, the folded `3-1/3` of layout 6, and `NF1` written back as 40. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
