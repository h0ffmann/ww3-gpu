# W3INIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked for
translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs a shim that
fills the hooks and owns the per-grid state, and a W3INIT hunk on the fork, first.

- (v) Fresh review 2026-10-16: `w3initmd.F90` preprocessed again with the lab defines (identical
  to the translator's) and recompiled with WW3's Release flags from the scratch CMake build's
  `flags.make` (`.text`, `.rodata*`, `.data`, `.bss` identical to the CMake object, disassembly
  identical to the one the translation used); every floating-point instruction of the object
  (`:622`, `:862`, `:1070`, `:1081`, `:1151`-`:1240`, `:1317`-`:1347`, `:1396`-`:1420`, `:1521`,
  `:1523`) and the LOGICAL tests of `:1096`, `:1118`-`:1119`, `:1152`, `:1460`-`:1489` read
  against the C++ `-O3` object rebuilt with the `openmp-release` flags (0 warnings, 0 FMA): the
  `MAX` operand orders and the word handling are the Fortran's. The state plumbing was checked
  against `w3adatmd.F90:2751-3033` (`NSEALM`, `FLCOLD`, `FLIWND` unconditional; `DW`, `UA`..`AS`,
  `WN` under `AINIT`; `CG`, `IAPPRO` under `FL_ALL`), `adat_w3seta.hpp` and `adat_w3dima.hpp`
  (`NSEALM` of `:805` lands in `WADATS(IMOD)`, which `W3DIMA` reads at `:1038`), the
  `FSTOTALIMP` default (`w3gdatmd.F90:1043`, read before `W3IOGR` as in the Fortran), the depth
  loops and the `VA` zeroing (no dependency between them), the `WAVNU1` kernel indices. No
  arithmetic or ordering defect. Fixed: the `print_memcheck` hook ran without a fence after the
  `VA` fill of `:1044` (it now fences, so every hook sees settled state); the citations of the
  uncompiled `W3_MPI` and `W3_DIST` blocks (`:625-640`, `:1260`, `:1560-1562`, `:874-934`,
  `:956-965`), `EXTOPN`'s format 1000 (`w3servmd.F90:1044`), `W3SETI`'s aliases
  (`w3idatmd.F90:962-984`), `OTYPE2` and `WDATA`. Added: what gfortran's bare `STOP` writes
  (below). After the fix the unit compiles with the `openmp-release` and `serial-debug` flags
  with no warning and no FMA; the CUDA compile was not repeated.

W3INIT sets up one wave model grid `IMOD` before the first `W3WAVE` call: it selects the grid's
data structures, sets the process counts, opens the log file, reads `mod_def.<ext>` through
`W3IOGR`, allocates the `W3WDATMD`, `W3ADATMD` and `W3IDATMD` arrays, reads the restart file
through `W3IORS`, merges the restart `MAPSTA` with the grid's, sets the output times, computes the
depth `DW` and, per sea point, the wavenumber `WN` and group velocity `CG` of every frequency
through `WAVNU1`, and writes the log header. The port is a host-side sequencer that calls the
existing ports where they exist, runs the depth bookkeeping on the host in the Fortran's order and
the `WN`/`CG` loop as a Kokkos kernel, and reaches every other callee through a hook table.

## What was ported

| | |
|---|---|
| WW3 routine | `W3INIT`, `model/src/w3initmd.F90:163-1683` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`SHRD`, no MPI, no DIST, no PDLIB) |
| Fork | the `h0ffmann/WW3` fork at the `WW3/` pin adds 100 lines, all under `#ifdef W3_KOKKOS`, after the `W3IOGR` call (`git -C WW3 diff 03adda91 HEAD -- model/src/w3initmd.F90`): the Kokkos start-up and table set-up. `switch_lab_shrd` has no `KOKKOS`, so that block is not compiled and not ported |
| Kokkos code | [`../src/ww_kokkos/init_w3init.hpp`](../src/ww_kokkos/init_w3init.hpp): the routine `ww::init::w3init<ExecSpace>`, its state (`InitState`, `InitOdat`, `InitIdat`, `InitGdat`, `InitConstants`, `InitWdat`), arguments (`InitArgs`), result (`InitStatus`), hooks (`InitHooks`) and two launchers, `init_wavenumbers` (`:1394-1439`) and `init_zero_dry` (`:1347-1349`) |
| Translated callees it calls | `W3SETA` ([`adat_w3seta.hpp`](../src/ww_kokkos/adat_w3seta.hpp), at `:540` and again after `W3DIMA`, the call `adat_w3dima.hpp` leaves to its caller), `W3IOGR` ([`iogr_w3iogr.hpp`](../src/ww_kokkos/iogr_w3iogr.hpp)), `W3FLGRDUPDT` ([`iogo_w3flgrdupdt.hpp`](../src/ww_kokkos/iogo_w3flgrdupdt.hpp)), `SET_UP_NSEAL_NSEALM` and `INIT_GET_ISEA` (`parall_*.hpp`, the shared-memory branch), `W3DIMA` ([`adat_w3dima.hpp`](../src/ww_kokkos/adat_w3dima.hpp)), `WAVNU1` ([`prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp)) |
| Callees through hooks | `W3SETO`, `W3SETG`, `W3SETW`, `W3SETI`, `PRINT_MEMCHECK` (may be empty), `ITRACE`, `WWDATE`/`WWTIME` with format 900, the `OPEN` of the log file, the list-directed `WRITE`s and `FLUSH` of `:647-654`, `SPATIAL_GRID`, `NVECTRI`, `COORDMAX`, `AREA_SI`, the format 1008 lines of `W3FLGRDUPDT`, `W3DIMW`, `W3DIMI`, `W3IORS('READ')`, `DSEC21`, `TICK21`, `W3DMO5`, `W3IOPP`, `STME21`, every log `WRITE` (formats 920, 930, 970-990, 9970-9977); `W3IOGR`'s own hooks pass through (`InitHooks::iogr`) |
| Not hooks | `W3MPII`, `W3MPIO`, `W3MPIP` (`:1560-1562`) are under `W3_MPI`, not compiled under the lab switch |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

State. The grid arrays `W3IOGR` fills (`MAPSTA`, `MAPST2`, `MAPSF`, `ZB`, `SIG`) are the host
vectors of `iogr::IogrState`, and `W3INIT` changes `MAPSTA` and `MAPST2` there. The `W3ADATMD`
arrays are `adat::Wadat` Views, reached through the `W3SETA` alias targets as the Fortran's
pointers are: `DW`, `UA`..`AS` and `WN` through the AINIT group, `CG` and `IAPPRO` through FL_ALL,
`NSEALM` through the unconditional group, `FLCOLD` and `FLIWND` through `WadatScalars`. `VA` and
`WLV` are `InitWdat` Views. The `W3ODATMD`, `W3IDATMD`, `CONSTANTS` fields and the `W3GDATMD`
fields `IogrState` lacks (`NSEAL`, `RSTYPE`, `NGRIDS`) are small structs of this header. Every
hook gets the whole `InitState` and runs after a fence.

## Compiled code

The preprocessed source (`gfortran -E` with the defines of WW3's `flags.make` and the
`__WW3_SWITCHES__` define CMake adds for this file) has 549 non-comment lines in `:163-1683`.
What is ported, besides the calls above: `:562-623` (process counts, `FRACOS`), `:642-656` (the
PDLIB checks with `STOP`), `:668-704` (the log file and `NDS`), `:786-796`, `:803-805`, `:871`,
`:971`, `:987-1053`, the output times `:1066-1246`, the depth and dry-point loops `:1285-1370`,
the wavenumbers `:1394-1439`, the fills `:1445-1453`, the log `:1457-1546`, `:1548`.

Not compiled under the lab switch, and not ported: `W3_MPI` (`:568-574`, `:625-640`, `:1260`,
`:1560-1562`), `W3_DIST` (`:807-818`, `:874-934`, `:956-965`), `W3_PDLIB`, `W3_SETUP`, `W3_UOST`, `W3_T`,
`W3_T1`, `W3_S`, `W3_TIMINGS`, `W3_DEBUGCOH`, `W3_DEBUGSTP`, `W3_DEBUGINIT`.

Compiled but dead in the `-O3` object, and not ported (v, GIMPLE and objdump):

- `:690-696`, the test file: `TSTOUT` is a `.FALSE.` PARAMETER (`constants.F90:57`); the
  `INQUIRE` and `OPEN` of `MDS(3)` are folded away. `TFILE` and `IFT` (`:672`, `:682`) feed only
  that block.
- `:860-867`, `:872-873`, `:966`: `NTTOT`, `NTTARG`, `NTTMAX` and `NT` are read only under
  `W3_DIST` and `W3_T`. The object still computes the `NTLOC` loop (vectorised four wide) and
  mallocs and frees `NT`; no store reaches module state.

Kept although unreachable: the `IOSTYP > 1` branch `:586-619`. `IOSTYP = 1` at `:565` is folded
into the test of `:577` in the object; the C++ keeps the branch and its compiler folds it too.

## Arithmetic

The reference is the `-O3` object of `w3initmd.F90`: gfortran 16.2 in the pinned `ww3` shell,
WW3's CMake Release flags (`-O3 -fallow-argument-mismatch -g -fno-second-underscore
-ffree-line-length-none`) and the lab defines. Its `.text`, `.rodata`, `.rodata.cst4`,
`.rodata.str1.*`, `.data` and `.bss` are byte-identical to the object of a CMake Release build of
`WW3-noaa/` under `switch_lab_shrd` configured in a scratch directory (v, 2026-10-16). It was read
as GIMPLE (`-fdump-tree-optimized-lineno`) and `objdump -dlr`. The C++ side is the `-O3` object
of a throwaway unit with the `openmp-release` flags, read the same way.

- (v) `:622` `FRACOS = 100.*REAL(NTPROC-NAPROC)/REAL(NTPROC)`: `cvtsi2ss`, `mulss` by 100.,
  `divss`; the test at `:623` is `> 15.` (`CRITOS`, `:129`). The Fortran loads `NTPROC` and
  `NAPROC` through their pointers; the C++ compiler folds the expression to 0 from the stores of
  `:562-563`, which no call separates from it. Same value.
- (v) `:1070`, `:1081` `REAL(ODAT(..))`: one `cvtsi2ss` in both.
- (v) `:1151`, `:1205` `MAX(0., DTOUT(J))`: the Fortran is `pxor xmm0; maxss (DTOUT),xmm0`,
  `0 > D ? 0 : D`, so NaN gives `D`. The C++ `(0 > d) ? 0 : d` compiles to `vcmpnltss` and
  `vandps`: `d` when `!(d < 0)`, else +0. The same selection for every input, NaN and -0.0
  included.
- (v) `:1152`, `:1206` `FLOUT(J) = FLOUT(J) .AND. (DTOUT(J) .GT. 0.5)`: the object tests the
  LOGICAL word against 0, then `comiss` with 0.5, and stores 0 unless both hold; when both hold
  the word is left as it was. The C++ does the same.
- (v) `:1096`, `:1108` `FLOUT(1) = FLOUT(1) .OR. FLOGRD(J,K)`: the object stores 1 once
  `FLOUT(1)` is non-zero and otherwise copies the word of `FLOGRD(J,K)` unchanged
  (`detail::lor_raw`). This matters only for a LOGICAL word other than 0 or 1.
- (v) `:1165` `.LT. 0.` for `J /= 4` and `.LE. 0.` for `J = 4` (the object unswitches the loop on
  `J == 4`); `:1180` `.LT. 0.`, `:1189` `.GT. 0.`; the same for `J = 8` at `:1216-1240`.
- (v) `:1317`, `:1346` `DW = MAX(0., WLVeff-ZB)`: `subss`, then `maxss` with a zero register as
  source, `D > 0 ? D : 0`, NaN gives 0. The C++ compiles to `vsubss` and `vmaxss` with the same
  operand order. `:1321`, `:1347` recompute `WLVeff-ZB` (ZB reloaded after the DW store) and test
  `.LE. 0.` with `comiss`; the C++ recomputes it and has the same `vcomiss`.
- (v) `:1396` `MAX(DMIN, DW(IS))` is `maxss (DW),xmm0` with `xmm0 = DMIN`: `DMIN > DW ? DMIN :
  DW`. `:1410` `MAX(DMIN, -ZB(IS))` is `xorps` then `maxss (DMIN),xmm0`: `-ZB > DMIN ? -ZB : DMIN`.
  The kernel compiles to `vmaxss %xmm0(DW),%xmm2(DMIN)` and `vxorps; vmaxss %xmm2(DMIN),
  %xmm0(-ZB)`: the same operand orders.
- (v) `:1419-1420` `KDCHCK = WNbat*DEPTHbat` (one `mulss`), `.GE. KDMAX` with `KDMAX` the SAVE
  local 4. (`:518`; nothing in W3INIT writes it), `comiss`/`jb`, NaN false. The kernel has one
  `vmulss` and `vcmpgess` with 4.0, NaN false.
- (v) `:1013` `SIGN(MAPTST, MAPSTA)` on INTEGER is the xor, arithmetic shift, add, xor sequence of
  `|A|` with `B`'s sign; `detail::isign` gives the same integer for every `A` but `INT_MIN`
  (overflow in both). `MOD` and `/` on INTEGER (`:1285`, `:1286`, `:1368`) truncate toward zero,
  as C++ `%` and `/`.
- (v) `:1521`, `:1523` `FACTOR*PTLOC`: one `mulss` each, `FACTOR` 1. or `1.E-3` (`:786-790`); the
  C++ has one `vmulss` each.
- (v) `WAVNU1` is a call into `w3dispmd` in the Fortran (not inlined across files); the kernel
  calls `prop::wavnu1`, whose report covers its bits, including the `sqrtf` call g++ keeps for a
  negative depth.
- (v) No FMA in the C++ object (`objdump`, 0 `vfmadd`/`vfmsub`/`vfnmadd`/`vfnmsub`), and the
  only libm call is that `sqrtf`.
- (v) `init_wavenumbers` writes only `WN(:,IS)`, `CG(:,IS)` from inputs of its own `IS`, with no
  reduction and no atomic, so any execution space and order give the bits of the serial loop.
  `init_zero_dry` stores 0. The depth loops `:1300-1350` run on the host in the Fortran's order
  because two sea points could in principle map to one `(IX,IY)` through `MAPSF`.

## Not ported

- Every `#ifdef` branch `switch_lab_shrd` does not compile, and the dead code above.
- The messages that end the run come back in `InitStatus` instead of being written: `EXTOPN`'s
  format 1000 (`w3servmd.F90:1044-1046`) for the log file, written by the caller to `NDSE` as it
  was at `:687`, before `:702` resets it. `STOP` at `:650` and `:655` returns `stop = true`; the
  object calls `_gfortran_stop_string(0, 0, 0)`, which exits with status 0, writes nothing to
  stdout and, when an IEEE flag other than inexact is raised, writes `Note: The following
  floating-point exceptions are signalling: ...` to stderr (v, a gfortran 16.2 probe with WW3's
  flags, 2026-10-16). The caller reproduces both.
- An `ALLOCATE` without `STAT=` that fails stops gfortran with exit status 1: reproduced for
  `MAPTST` (`kExitAllocate`); for `NT` it is not, since `NT` is not ported ⚠.
- `IsMulti` and `MPI_COMM_IN` are read only under PDLIB and MPI and are not arguments here.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run: no ctest, no regtest, no smoke. What was done, 2026-10-16 (v):

- A throwaway unit that includes `init_w3init.hpp` and instantiates `w3init`,
  `init_wavenumbers` and `init_zero_dry` on `Kokkos::DefaultExecutionSpace` (OpenMP, Kokkos
  5.2.2 of the pinned `ww3` shell) compiled with g++ 16.2 and the `openmp-release` flags (`-O3
  -march=x86-64-v3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wno-unused-parameter -ffp-contract=off -fopenmp`) with no warning and no FMA; the same unit with
  `-g` has the same `.text` and was the one read.
- A copy of `wave_support.cpp` with the `#include` and the three instantiations added (not the
  real file) compiled with the `openmp-release` flags and with the `serial-debug` flags (`-g
  -fsanitize=address,undefined -fno-omit-frame-pointer -DWW_DETERMINISTIC=1`, same warnings),
  with no warning.
- The throwaway unit compiled for CUDA in the pinned `#cuda` shell (Kokkos 5.2.2 CUDA,
  `nvcc_wrapper -O3 -std=c++20 -arch=sm_89 --fmad=false -extended-lambda -fopenmp`), never run:
  no warning; the 94 `FFMA` of the SASS are all in `init_wavenumbers`, beside its 16 `MUFU`
  reciprocal and square-root instructions. With `--fmad=true` the count is 101 and 7 `FMUL` and 7
  `FADD` disappear, so with `--fmad=false` the header's products and sums stay separate and the
  94 come from CUDA's division and square-root sequences. Whether those return glibc's bits is
  not shown ⚠ (`AGENTS.md`, "Bit for bit").

Open ⚠: nothing was run or regtested; the hooks are
declared, not filled; the `W3IORS`, `W3DIMW`, `W3DIMI`, `W3IOPP` hooks must leave the Views and
`IogrState` in the extents the checks of `w3init` expect, which only a shim can show; the CUDA
division and square root against glibc.
