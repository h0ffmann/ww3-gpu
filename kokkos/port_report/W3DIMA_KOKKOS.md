# W3DIMA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3INIT`/`W3IOGO` wiring first.

- (v) Fresh review 2026-10-16: the 32 `CHECK_ALLOC_STATUS` of the lab switch (33 `ALLOCATE`
  statements, two of them the `LPDLIB` branches) read against the source at the pin and a
  regenerated `-O3` GIMPLE and disassembly (gfortran 16.2, the `ww3` shell, the Release flags and
  `switch_lab_shrd` defines of `ww3_lib.dir/flags.make`): extents, lower bounds, order, conditions,
  fills and unfilled arrays match; the `FL_ALL` store at `:961` before `W3SETA` and the reload at
  `:1323` confirmed; the disassembly differs from WW3's CMake object only in the 32 `__FILE__`
  length immediates; every line citation checked. No defect in the allocation or the fills. Fixed:
  `NOSWLL` documented as `OUTPTS(IMOD)`'s (it is the last `W3SETO`'s), `Wadats::iadata` not
  said to be distinct from the Fortran `IADATA` that `W3XETA` (`:3174`) also writes, the fence
  claimed on every return (only on `ok`). `fl_all_unassociated` kept: it replaces a store through
  an unassociated pointer, a path no caller at the pin takes. The header compiled again with an
  explicit instantiation on `Kokkos::DefaultExecutionSpace` (OpenMP), no warning.
- (v) Consistency fix 2026-10-16: the `FL_ALL` store of `:961` and the reads of `NSEALM`
  (`:1038-1039`) and `FL_ALL` (`:1323`) go through `W3setaTargets::base`, the target of the
  unconditional aliases (`:2754`, `:2760`), not `Wadats::iadata`, which is the one Fortran
  `IADATA` that `W3XETA` also writes (`:3174`). `w3dima` takes `W3setaTargets& t`; at `:994` and
  `:1467` it does `W3SETA`'s unconditional part (`IADATA = IMOD`, `:2746`; `t.base = imod`,
  `:2751-2809`) and leaves the conditional groups to one `w3seta` call by the caller after an
  `ok` return (see "Not ported" for why that one call is equivalent). `W3setaTargets` moved to
  `adat_wadat.hpp`, unchanged, so this header can name it without an include cycle. No allocation,
  extent, order or fill changed. Compiled with `adat_w3dmnl.hpp`, `adat_w3naux.hpp`,
  `adat_w3seta.hpp`, `adat_w3xeta.hpp` and `adat_w3xdma.hpp` in one unit, explicit instantiations
  on `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, g++ 16.2 `-O3
  -march=x86-64-v3` and `-O0 -g -fsanitize=address,undefined`, both `-std=c++20 -Wall -Wextra
  -Wpedantic -Wshadow -Wconversion -ffp-contract=off -fopenmp`: no warning. Nothing was run.
- (v) Fresh review of the consistency fix 2026-10-16: each changed statement checked against
  `w3adatmd.F90` at `8e433f99` (`:961`, `:976`, `:994`, `:1038`, `:1323`, `:1456`, `:1467`,
  `:2746`, `:2754`, `:2760`, `:2826`, `:3038`); the `:961` store still comes before the checks of
  `:966-979`. The identifiers of `:995-1466`, comments and `%` components removed, intersected
  with the 241 names `W3SETA` points (`:2746-3075`, every `#ifdef` branch counted): `FL_ALL` and
  `NSEALM` only, so the equivalence argument below holds as stated; `W3DIMA` never names
  `NLINIT`. The moved `W3setaTargets` is identical to the one it replaced (`diff` of the two
  struct bodies), and `adat_w3seta.hpp`, `adat_w3xeta.hpp`, `adat_w3xdma.hpp` use it as before.
  No defect in the code; two comments rewrapped (the 107-character `fl_all` line of
  `adat_wadat.hpp`, a broken line in the `fl_all_unassociated` comment). The same one-unit
  compile as above, with Kokkos 5.2.2 of the pinned `ww3` shell: no warning at `-O3` nor at
  `-O0 -g -fsanitize=address,undefined`. Nothing was run.

Sizes and allocates the W3ADATMD arrays of model `IMOD` (with `D_ONLY` only the data arrays,
not the propagation work arrays of the `FL_ALL` block), and gives the ones the Fortran
initialises their first value. It does no floating-point arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `W3DIMA`, `model/src/w3adatmd.F90:847-1509` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/adat_w3dima.hpp`](../src/ww_kokkos/adat_w3dima.hpp), a host-side function template over the execution space; the arrays of one model, `TYPE WADAT` (`w3adatmd.F90:384-570`), in [`../src/ww_kokkos/adat_wadat.hpp`](../src/ww_kokkos/adat_wadat.hpp) |
| Memory | every array a `Kokkos::View<..., Kokkos::LayoutLeft, ExecSpace::memory_space>`, created outside any kernel; a Fortran index maps 1:1 once its lower bound is subtracted (the map is the header comment of `adat_wadat.hpp`) |
| Errors | the `EXTCDE` exit codes returned as `W3dimaStatus` (1, 2, 3, 99) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Order of operations

As the Fortran, `w3adatmd.F90`:

1. `FL_ALL = .NOT. D_ONLY` (`.TRUE.` when absent), `:960-964`, stored through the module pointer
   `FL_ALL` *before* any check, so into the model the previous `W3SETA` selected,
   `W3setaTargets::base` (see below).
2. `NGRIDS == -1` exits 1 (`:966`); `IMOD < 1` or `IMOD > NADATA` exits 2 (`:971`);
   `WADATS(IMOD)%AINIT` exits 3 (`:976`).
3. `W3SETA(IMOD)` (`:994`): `IADATA = IMOD` (`:2746`), and `FL_ALL`, `NSEALM` now alias model
   `IMOD` (`t.base = imod`).
4. Sections 1 to 10 and the `FL_ALL` block, each `ALLOCATE` statement followed by its
   `CHECK_ALLOC_STATUS` (exit 99, `w3macros.h:49-51`) and then its fills, in the order of the
   table below; `NSEALM = NSEA` when `NSEALM` is 0 (`:1038-1040`), before section 2.
5. `AINIT = .TRUE.` (`:1456`), whatever `FL_ALL` was.
6. `W3SETA(IMOD)` again (`:1467`): the port sets `IADATA` and `t.base` again; the conditional
   groups are the caller's `w3seta` (see "Not ported").

## Arrays

`NL` is `WADATS(IMOD)%NSEALM` after `:1038-1040`. "undefined" means the Fortran allocates without
assigning; the View is created `WithoutInitializing` for the same reason. Fills are `REAL`
stores of one value (`UNDEF` is `-999.9`, bits `0xc479f99a`).

| Arrays | Fortran extents | Condition | Initial value | `w3adatmd.F90` |
|---|---|---|---|---|
| `DW` | `(0:NSEA)` | always | `0.` | `:1004`, `:1006` |
| `CX`, `CY` | `(0:NSEA)` | always | `0.` | `:1008`, `:1011-1012` |
| `UA`, `UD`, `AS` | `(0:NSEA)` | always | undefined | `:1014-1016` |
| `U10`, `U10D` | `(NSEA)` | always | undefined | `:1015` |
| `TAUA`, `TAUADIR` | `(0:NSEA)` | always | `0.` | `:1019-1023` |
| `HS`, `WLM`, `T02`, `T0M1`, `T01`, `FP0`, `THM`, `THS`, `THP0`, `HSIG`, `STMAXE`, `STMAXD`, `HMAXE`, `HMAXD`, `HCMAXE`, `HCMAXD`, `QP`, `WBT`, `WNMEAN` | `(NL)` | always | `UNDEF` | `:1041-1053`, `:1056-1074` |
| `EF`, `TH1M`, `STH1M`, `TH2M`, `STH2M` (i = 1..5) | `(NL, E3DF(2,i):E3DF(3,i))` | `E3DF(1,i) > 0` | `UNDEF` | `:1083-1107`, `:1109-1113` |
| `PHS`, `PTP`, `PLP`, `PDIR`, `PSI`, `PWS`, `PTHP0`, `PQP`, `PPE`, `PGW`, `PSW`, `PTM1`, `PT1`, `PT2`, `PEP` | `(NL, 0:NOSWLL)` | always | `UNDEF` | `:1119-1137`, `:1139-1155` |
| `PWST`, `PNR` | `(NL)` | always | `UNDEF` | `:1125-1126`, `:1145-1146` |
| `CHARN`, `TWS`, `CGE`, `PHIAW`, `TAUWIX`, `TAUWIY`, `TAUWNX`, `TAUWNY` | `(NL)` | always | `UNDEF` | `:1161-1171`, `:1173-1180` |
| `WHITECAP` | `(NL, 4)` | always | `UNDEF` | `:1169`, `:1181` |
| `SXX`, `SYY`, `SXY`, `TAUOX`, `TAUOY`, `BHD`, `PHIOC`, `TUSX`, `TUSY`, `USSX`, `USSY`, `TAUOCX`, `TAUOCY`, `PRMS`, `TPMS`, `PHICE` | `(NL)` | always | `UNDEF` | `:1187-1205`, `:1223-1238` |
| `TAUICE` | `(NL, 2)` | always | `UNDEF` | `:1203`, `:1239` |
| `P2SMS` | `(NL, P2MSF(2):P2MSF(3))` | `P2MSF(1) > 0` | `UNDEF` | `:1210-1213`, `:1240` |
| `US3D` | `(NL, NK*2)` | `US3DF(1) > 0` | `UNDEF` | `:1214-1217`, `:1241` |
| `USSP` | `(NL, NK*2)` | `USSPF(1) > 0` | `UNDEF` | `:1218-1221`, `:1242` |
| `ABA`, `ABD`, `UBA`, `UBD`, `PHIBBL` | `(NL)` | always | `UNDEF` | `:1248-1252`, `:1255-1260` |
| `BEDFORMS` | `(NL, 3)` | always | `UNDEF` | `:1250`, `:1259` |
| `TAUBBL` | `(NL, 2)` | always | `UNDEF` | `:1252`, `:1261` |
| `MSSX`, `MSSY`, `MSCX`, `MSCY`, `MSSD`, `MSCD`, `QKK`, `SKEW`, `EMBIA1`, `EMBIA2` | `(NL)` | always | `UNDEF` | `:1267-1272`, `:1275-1284` |
| `DTDYN`, `FCUT`, `CFLXYMAX`, `CFLTHMAX`, `CFLKMAX` | `(NL)` | always | `UNDEF` | `:1290-1294`, `:1297-1301` |
| `USERO` | `(NL, NOEXTR)`, `NOEXTR = 2` | always | `UNDEF` | `:1307`, `:1310` |
| `WN` | `(0:NK+1, 0:NSEA)` | always | undefined | `:1312` |
| `CG` | `(0:NK+1, 0:NSEA)` | `FL_ALL` | undefined | `:1325` |
| `CA0`, `CAI`, `CD0`, `CDI` | `(NSEA)` | `FL_ALL` and `FLCUR` | undefined | `:1334-1341` |
| `UA0`, `UAI`, `UD0`, `UDI`, `AS0`, `ASI` | `(NSEA)` | `FL_ALL` and `FLWIND` | undefined | `:1343-1352` |
| `MA0`, `MAI`, `MD0`, `MDI` | `(NSEA)` | `FL_ALL` and `FLTAUA` | undefined | `:1354-1361` |
| `RA0`, `RAI` | `(NSEA)` | `FL_ALL` and `FLRHOA` | undefined | `:1363-1368` |
| `ATRNX`, `ATRNY` | `(NY*NX, -1:1)` | `FL_ALL` | undefined | `:1370-1371` |
| `DDDX`, `DDDY`, `DCXDX`, `DCYDX`, `DCXDY`, `DCYDY` | `(NY, NX)`; `(1, NSEAL)` under `LPDLIB` | `FL_ALL` | `0.` | `:1374-1393`, `:1395-1402` |
| `DCDX`, `DCDY` | `(0:NK+1, NY, NX)`; `(0:NK+1, 1, NSEAL)` under `LPDLIB` | `FL_ALL` | `0.` | `:1374-1393`, `:1397-1398` |
| `ALPHA` | `(NK, NSEAL)` | `FL_ALL` | undefined | `:1411` |
| `ITER` (INTEGER) | `(NK, NTH)` | `FL_ALL` and `GTYPE == UNGTYPE` | undefined | `:1433-1436` |
| `MAPX2`, `MAPY2`, `MAPAXY` (INTEGER), `MAPTRN` (LOGICAL, here `int` 0/1) | `(NY*NX)` | `FL_ALL`, `W3_PR3` | undefined | `:1439-1445` |
| `MAPCXY` (INTEGER) | `(NSEA)` | `FL_ALL`, `W3_PR3` | undefined | `:1442` |
| `MAPTH2` (INTEGER) | `((NK+2)*NTH)` | `FL_ALL`, `W3_PR3` | `0` | `:1443`, `:1447` |
| `MAPWN2` (INTEGER) | `(NSPEC+NTH)` | `FL_ALL`, `W3_PR3` | undefined | `:1444` |
| `IAPPRO` (INTEGER) | `(NSPEC)` | `FL_ALL` | undefined | `:1450` |
| `SPPNT` | `(NTH, NK, 4)` | `FL_ALL` | undefined | `:1451` |

A bound pair with `UB < LB` gives a zero-size array, as `ALLOCATE` does (`NOSWLL = -1` from
`W3NOUT`, `w3odatmd.F90:699`, gives `0:-1`).

## Arithmetic and parity

The reference is the `-O3` object of `w3adatmd.F90`, compiled 2026-10-16 with gfortran 16.2 in
the pinned `ww3` shell, the Fortran flags of WW3's CMake Release build
(`ww3_lib.dir/flags.make`) and the `switch_lab_shrd` defines, plus
`-fdump-tree-optimized-lineno`; read as GIMPLE and assembly.

- (v) The object is WW3's: the disassembly of `__w3adatmd_MOD_w3dima` is identical to the one in
  the CMake build of `03adda91` (`build/ww3/.../w3adatmd.F90.o` in the `ww3-gpu-noaa` worktree)
  except for 32 `push` immediates, the length of the `__FILE__` string passed to `EXTCDE` by the
  32 `CHECK_ALLOC_STATUS` (an absolute path there, `w3adatmd.F90` here). 32 is also the number
  of `ALLOCATE` statements the port has under the lab switch.
- (v) `FL_ALL` is written through the module pointer before `W3SETA(IMOD)` (GIMPLE: the pointer
  `fl_all` is loaded at `:961` and stored through before the `NGRIDS` test) and reloaded after it
  at `:1323`. The value lands in the model of the previous `W3SETA` and the test reads
  `WADATS(IMOD)%FL_ALL`, which W3NAUX set to `.FALSE.` (`:807`). The two agree only when the
  caller selected `IMOD` first, as `W3INIT` (`w3initmd.F90:550`), `W3IOGO`
  (`w3iogomd.F90:2605`) and `ww3_gint` (`ww3_gint.F90:805`) do. The port keeps both through
  `W3setaTargets::base`, the target of the unconditional aliases (`:2760`), which only `W3SETA`
  sets; `Wadats::iadata` is the Fortran `IADATA`, which `W3XETA` (`:3174`) also writes without
  retargeting `FL_ALL`.
- (v) `NSEALM` at `:1038-1039` is read and written through the pointer `W3SETA` set
  (`:2754`), so it is `WADATS(IMOD)%NSEALM`; the port writes `wadats[t.base-1].nsealm`, with
  `t.base == imod` there.
- (v) Between `:994` and `:1467` the only W3ADATMD aliases `W3DIMA` names are `NSEALM` and
  `FL_ALL`; every other array is named as `WADATS(IMOD)%...` (identifiers of `:995-1466` listed
  2026-10-16 with the `WADATS(IMOD)%` components removed; the rest are W3GDATMD, W3ODATMD,
  W3IDATMD, CONSTANTS and locals).
- (v) Every fill (`= 0.`, `= UNDEF`, `MAPTH2 = 0`) is a scalar loop of 4-byte stores of one
  value, with no arithmetic. `UNDEF` is the variable `__constants_MOD_undef`, `0xc479f99a` in
  `constants.F90.o`'s `.data`, the bits of `-999.9f`; it is never assigned in `model/src`, and
  the port takes it as an input (`W3dimaGrid::undef`).
- (v) `NY*NX`, `NK*2`, `(NK+2)*NTH` and `NSPEC+NTH` are computed in `integer(kind=4)` and widened
  to `integer(kind=8)` for the bound (GIMPLE `:1215`, `:1371`); the port does the same.
- (v) `NXXX = NSEALM*NAPROC` (`:998`) is dead (only a `DEBUG` bind in the GIMPLE);
  `print_memcheck` writes only under `W3_MEMCHECK` (`w3servmd.F90:2501-2516`), not compiled.
- ⚠ The C++ fills go through `Kokkos::deep_copy(exec, view, value)`. That they store the same
  bits is argued from what a fill is, not read in the C++ GIMPLE.
- ⚠ An `ALLOCATE` failure is `malloc` returning NULL in the Fortran. In the port it is the
  exception Kokkos throws when a View allocation fails, caught per statement and returned as 99.
  That Kokkos throws, not aborts, was not checked on the CUDA backend.
- ⚠ `W3dimaStatus::fl_all_unassociated` (-1) is not a WW3 code. It covers the one case where the
  Fortran would store through an unassociated pointer (no `W3SETA` yet); no caller at the pin
  does that.
- ⚠ A zero-extent Fortran array is `ALLOCATED`, a zero-extent View carries no such flag. Consumers
  test the conditions in the table, not `View::is_allocated()`.
- ⚠ `FLCUR`, `FLWIND`, `FLTAUA` and `FLRHOA` are whatever the `W3IDATMD` pointers point at when
  `W3DIMA` is called, and `NOSWLL` the `W3ODATMD` one of the last `W3SETO`; the shim has to pass
  those values, not `INPUTS(IMOD)`'s or `OUTPTS(IMOD)`'s.

## Not ported

- `W3_IC3` (`:1315-1320`, `:1328-1331`), `W3_SMC` (`:1404-1409`), `W3_PR1` (`:1414-1420`),
  `W3_PR2` (`:1422-1431`), `W3_S` STRACE and the `W3_T` prints: not compiled under the lab switch.
- The `WRITE(NDSE)` messages and `EXTCDE`: returned as the code it would exit with.
- `W3SETG(IMOD)` (`:986`) and `W3SETG(JGRID)` (`:1485`): the caller passes grid `IMOD`'s state in
  `W3dimaGrid`.
- The conditional part of `W3SETA(IMOD)` at `:994` and `:1467`. The port cannot call `w3seta`:
  `adat_w3seta.hpp` includes this header (through `adat_w3dmnl.hpp`), and `W3SETA`'s conditions
  read `WADATS(IMOD)%NLINIT` (`WadatsNl1`) and `INPUTS(IMOD)%INFLAGS1`, which `W3DIMA` does not
  take. Both calls do the unconditional part (`IADATA`, `t.base`). After an `ok` return the
  caller must call `w3seta(reg, nl1, t, imod, in)` with `in.gtype` the `GTYPE` of grid `IMOD`
  (`W3SETG(IMOD)` at `:986` is still in force at `:1467`; `W3SETG(JGRID)` is `:1485`) and the
  `INFLAGS1` of `INPUTS(IMOD)`. ⚠ That one call is argued, not run, to leave every group as the
  two Fortran calls do: at `:994` `AINIT` of `IMOD` is false (`:976`), so of the conditional
  groups only `NLINIT` (`:3038`) can move; `W3DIMA` does not change `NLINIT`, `:1467` tests it
  again, and between the two calls the routine reads no alias of a conditional group. On an
  error return the Fortran has stopped, so the `NLINIT` move of `:994` is not reproduced there.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and the routine runs once per model at set-up.

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit that
takes the address of `ww::adat::w3dima<Kokkos::DefaultExecutionSpace>` compiled in the pinned
`ww3` shell (g++ 16.2) with the `openmp-release` flags and `-Wshadow -Wconversion`, no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter \
    -ffp-contract=off -fopenmp -c tu_w3dima.cpp
```

The same unit also compiled at `-O0` with `-Wsign-conversion` and
`-fsanitize=address,undefined`, again with no warning. It was never linked or run.
