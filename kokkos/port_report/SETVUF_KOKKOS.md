# SETVUF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Fills column `ITIME` of the tables `F_ARG`, `U_ARG`, `V_ARG` at time `hr` (hours) and latitude
`XLAT`: for each main constituent the astronomical argument `V` (cycles, reduced modulo 2) from
the Doodson numbers and `ASTR`'s mean longitudes, and the nodal factor `f` and phase `u` from its
satellites, `f e^{2 pi i u} = 1 + sum_j r_j e^{2 pi i u_j}` with the latitude factor of `r_j`
for `IR = 1, 2`; then each shallow-water constituent as `f = prod f_L^|c|`, `V = sum c V_L`,
`u = sum c u_L` over its main constituents (Foreman, IOS tidal package).

## What was ported

| | |
|---|---|
| WW3 routine | `SETVUF`, `model/src/w3tidemd.F90:1893-2030` at NOAA-EMC/WW3 `03adda91` (138 lines). Called only by `ww3_prnc.F90:1672` (tidal analysis of a forcing field, `ITIME = I >= 1`); a call at `w3tidemd.F90:2213` is commented out. No `#ifdef` in the routine; `w3tidemd.F90` is in every build. Regtest switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`); also `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OAS*`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_setvuf.hpp`](../src/ww_kokkos/tide_setvuf.hpp), `ww::tide::setvuf`, `KOKKOS_INLINE_FUNCTION`; calls `ww::tide::astr` ([`ASTR_KOKKOS.md`](ASTR_KOKKOS.md)). The module state (counts, Doodson tables, satellite tables, `TIDE_INDEXJ/JK`, `COEF_CON`, the three output tables and their first extent, `TIDE_INDEX`) is passed in |
| C shim | none yet. On `ITIME = -1` (`:2014-2027`) the C++ writes `TIDE_INDEX` and returns `kTestOutput`; the shim does the `WRITE`s on unit 992 |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the disassembly of `setvuf` is the same in both, so the routine has
one variant. Compared operation by operation with a C++ unit built with the `openmp-release` flags
(no warning, no FMA in its object), 2026-10-16.

- (v) `D1 = hr/24 - 2415020` and `INTDYS = INT((hr + 0.00001)/24)`: `0.00001` is a default
  `REAL` literal widened to `REAL(8)` (`9.99999974737875e-6` in both GIMPLEs, `static_assert` in
  the header); `INT` of a `REAL(8)` is `cvttsd2si` in both, guarded in the C++ for out-of-range
  values; `TAU = HH/24 + H - S` in double.
- (v) `ASTR` is an out-of-line call in the Fortran object and inlined in the C++; its operations
  are those of `tide_astr.hpp`, checked in its own report.
- (v) `CXLAT = MAX(ABS(XLAT), 5.)` is `andps` then `maxss` with `ABS(XLAT)` as destination in
  both; `SLAT` is glibc `sinf` of `(PI*CXLAT)/180` in single precision in both.
- (v) `VDBL` and `UUDBL`: the integer Doodson numbers times the `REAL(8)` arguments summed left to
  right in double, `SEMI`/`PH` widened and added last, then rounded to `REAL` (`cvtsd2ss`); the
  integer parts by `cvttss2si` (`ww::st4::fint`); `IV = (IV/2)*2`; `VV = VDBL - REAL(IV)`.
  The Fortran writes `II*TAU` as `TAU*II` in its GIMPLE: a commutative product of a non-NaN
  integer, same bits.
- (v) Latitude factors: `((EE*0.36309)*(1 - (5*SLAT)*SLAT))/SLAT` and `(EE*2.59808)*SLAT`, the
  same sequence of `mulss`, `subss`, `divss` (the Fortran parentheses are a `PAREN_EXPR` that g++
  does not need to keep: nothing is reassociated without it).
- (v) `COS(UU*TWOPI)` and `SIN(UU*TWOPI)` become one glibc `sincosf` call in both objects;
  `SUMC = SUMC + RR*cos`, `SUMS = SUMS + RR*sin` in loop order, `addss` with the running sum as
  destination in both.
- (v) `F_ARG = sqrtss(SUMC*SUMC + SUMS*SUMS)` and `U_ARG = atan2f(SUMS, SUMC)/TWOPI` (glibc
  `atan2f`) in both; with no satellite both give `F = 1`, `U = 0`.
- (v) Shallow-water constituents: `F_ARG(K) * powf(F_ARG(L), ABS(COEF_CON(J)))` with glibc
  `powf`, `V_ARG(K) + COEF_CON(J)*V_ARG(L)`, same for `U`; `F_ARG(L)` is read from memory after
  each store, so `L = K` gives the same value in both.
- ⚠ Some commutative `mulss`/`addss` have their operands in the other order in the two objects
  (the `IR = 1` product, the shallow-water `powf` product). Results are identical except the
  payload of a NaN when both operands are NaN, which no finite input produces.
- (v) The search for `INDX` (`:1948-1952`) and the one for `IFLAG` (`:1998-2003`) set locals that
  are never read. gfortran leaves an empty counting loop for the first and the `memcmp_eq` loop
  for the second, neither of which changes a value; the C++ does not translate them.
- ⚠ With `ITIME = -1` the Fortran writes column -1 of `F_ARG`, `U_ARG`, `V_ARG`, outside both
  allocations (`ww3_prnc.F90:1522`, `ww3_shel.F90:1966`), before its test output; the C++ does the
  same writes. The only caller passes `ITIME >= 1`, so the path is never reached.
- ⚠ `INDX(170)` is indexed by `K` up to `NTIDAL_CON` and `TIDE_INDEX(70)` by `L` up to
  `TIDE_MF`, neither bounds checked; `INDX` is dead in the object.
- ⚠ On a device the C++ calls Kokkos' (CUDA's) `sin`, `cos`, `atan2`, `pow`, not shown to return
  glibc's bits. CUDA: not compiled (optional this batch).
- (v) Fresh review 2026-10-16: no defect. `w3tidemd.F90` recompiled from `build_mixa` and `build_lab`
  (identical to the CMake objects, `setvuf` identical in both); its GIMPLE and assembly read against a
  new C++ unit's statement by statement: the double sums of `VDBL`/`UUDBL`, the `0.00001` and `TWOPI`
  constants, `maxss` operand order, `cvttss2si`/`cvttsd2si` with the out-of-range value, `sincosf`,
  `atan2f`, `powf`, the column stride and the early `RETURN` before the `ITIME = -1` output. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
