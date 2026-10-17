# W3SETG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3INIT`/`W3IOGR`/`W3WAVE` wiring first. One fresh review, below.

- (v) Fresh review 2026-10-16: WW3's CMake `ww3_lib` rebuilt from `WW3-noaa/` (`03adda91`,
  `cmp`-identical to the `WW3/` pin's `w3gdatmd.F90`) under `switch_lab_shrd` with
  `CMAKE_BUILD_TYPE=Release` in the pinned `ww3` shell, `w3gdatmd.F90` recompiled with its
  `flags.make` plus `-fdump-tree-optimized-lineno` (disassembly identical to the CMake object),
  and `gfortran -E` with the same defines. The 252 aliases regenerated from the preprocessed
  source by an independent script and diffed against `kW3setgAliases` (pointer, target, line,
  group): no difference; every alias line found as a store in the GIMPLE, the groups equal
  basic blocks 7 to 11 (93, 22, 17, 14, 106). The check order, the `.rodata` codes 1 and 2
  (relocations of the two `lea` before `extcde`) and the absence of floating-point instructions
  re-read from the object; the C++ `-O3` GIMPLE of `w3setg` re-dumped (same order, PHI
  `<1, 0, 2, -101>`). The message text compared again with gfortran 16.2 output of the two
  `FORMAT`s cut from `:2881-2886` by `sed`, for 0, -999999999, -1000000000 (asterisks),
  2147483647, -2147483647 and 42 (1095 bytes, `cmp`); a throwaway ASan/UBSan program ran the
  three status paths, the keep-earlier-target rule and `selected()` with and without `NAUX`.
  The "only `W3SETG` writes `IGRID`, `ISGRD`, `IPARS`" premise re-grepped (assignments, `CALL`
  arguments, `READ`, pointer targets), the alias-to-member table's "none yet" list re-derived
  (27 names, identical), and every `file:line` of the header and this report read at the pin.
  No defect in the code. Two citations fixed: `W3NMOD`'s range is `:1528-1547` (the `ALLOCATE`
  ends at `:1547`), and the header now names both `ww3_gint.F90` assignments (`:1303`,
  `:1374`). The CUDA build and `wave_support.cpp` in the presets were not rebuilt by the
  reviewer; nothing of the port was run inside WW3.

Selects grid/model `IMOD` in W3GDATMD: two checks, `IGRID = ISGRD = IPARS = IMOD`, then 252
module pointer aliases are re-pointed at the components of `GRIDS(IMOD)`, `SGRDS(IMOD)` and
`MPARS(IMOD)`, 216 always and 36 under the `GINIT` or `SINIT` condition. It does no
floating-point arithmetic and stores no array element. It is called 109 times in `model/src` at
the pin (`grep -i 'CALL W3SETG'`), among them by `W3DIMX`, `W3DIMS` and `W3DIMUG` after they
allocate (`w3gdatmd.F90:1900`, `:2116`, `:3273`), by `W3GNTX` (`:3027`), by `W3IOGR`
(`w3iogrmd.F90:569`), `W3INIT` (`w3initmd.F90:534`) and `W3WAVE`, which skips it when
`IGRID` already equals `IMOD` (`w3wavemd.F90:695`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SETG`, `model/src/w3gdatmd.F90:2156-2893` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`: `cmp` of the two files, and `git -C WW3 diff 03adda91 HEAD -- model/src/w3gdatmd.F90` is empty), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/gdat_w3setg.hpp`](../src/ww_kokkos/gdat_w3setg.hpp), host-side inline functions on `GdatModule` of [`gdat_sgrd.hpp`](../src/ww_kokkos/gdat_sgrd.hpp) (W3DIMS's port) |
| Memory | no View is created, copied or read; only host integers are written |
| Errors | the `EXTCDE` exit codes returned as `W3setgStatus` (1, 2), plus -101 (`missing_input`), not a WW3 code; `EXTCDE` always ends in `CALL EXIT` (`w3servmd.F90:1207`); `w3setg_message()` returns the text of the `WRITE(NDSE,1001/1002)` |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## What the switch compiles

The `gfortran -E` of `w3gdatmd.F90` with the defines of WW3's CMake build under
`switch_lab_shrd` (`W3_BS0 W3_BT1 W3_CRT1 W3_CRX1 W3_DB1 W3_F90 W3_FLX0 W3_IC0 W3_IS0 W3_LN1
W3_LRB4 W3_MLIM W3_NC4 W3_NL1 W3_NOGRB W3_NOPA W3_O0..O7 W3_O14 W3_PR3 W3_REF0 W3_RWND W3_SHRD
W3_ST4 W3_STAB0 W3_TR0 W3_UQ W3_WNT1 W3_WNX1 W3_XX0`, from `flags.make`) (v). Of the `#ifdef`
blocks inside `W3SETG`:

| Compiled | Lines | Aliases |
|---|---|---|
| `W3_RWND` | `:2554-2556` | `RWINDC` |
| `W3_PR3` | `:2567-2570` | `WDCG`, `WDTH` |
| `W3_LN1` | `:2615-2619` | `SLNC1`, `FSPM`, `FSHF` |
| `W3_ST4` | `:2672-2714` | `ZZWND` .. `SSINBR` (41) |
| `W3_NL1` | `:2737-2753` | `SNLC1` .. `GQAMP` (15) |
| `W3_BT1` | `:2818-2820` | `SBTC1` |
| `W3_DB1` | `:2827-2832` | `SDBC1`, `SDBC2`, `FDONLY`, `SDBSC` |

Not compiled, not ported: `W3_S` (`:2239-2241`, `:2254-2257`), `TEST_W3GDATMD` (`:2271-2273`,
`:2887-2889`), `W3_SMC` (`:2296-2307`, `:2468-2489`, `:2571-2578`), `W3_REF1` (`:2315-2320`),
`W3_IG1`, `W3_IC2`, `W3_IC3`, `W3_IC4`, `W3_IC5`, `W3_IS2` (`:2321-2343`), `W3_SEC1`
(`:2354-2356`), `W3_RTD` (`:2361-2365`, `:2442-2444`), `W3_BT4` (`:2463-2466`, `:2821-2823`),
`W3_WCOR` (`:2557-2559`), `W3_PR2` (`:2563-2566`), `W3_FLD1`/`W3_FLD2` (`:2582-2595`),
`W3_FLX2`/`3`/`4` (`:2599-2611`), `W3_ST1`/`2`/`3`/`6` (`:2625-2670`, `:2716-2733`), `W3_NL2` to
`W3_NLS` (`:2754-2814`, with the only other conditional alias, `DPTHNL` under `NDPTHS .NE. 0`,
`:2758`), `W3_UOST` (`:2835-2840`), `W3_IS1` (`:2844-2847`). `W3SETG` has no `ASSOCIATED` test
(v, read `:2156-2893`).

## How a pointer alias is represented

As in [`W3SETA_KOKKOS.md`](W3SETA_KOKKOS.md): the C++ has no module pointers, a consumer holds
the struct of a model. What `W3SETG` leaves behind is, for each group of aliases it re-points
together under one condition, the model the group now names:

| Group | Condition, read from model `IMOD` | Aliases | C++ record |
|---|---|---|---|
| `GRIDS`, unconditional | none | `GTYPE` .. `ZB`, `:2285-2432` (93) | `GdatModule::igrid` (`IGRID`, `:2278`) |
| `GINIT` block | `GRIDS(IMOD)%GINIT` (`:2434`) | `MAPSTA` .. `GSU`, `:2436-2491` (22) | `W3setgTargets::ginit` |
| `SGRDS`, unconditional | none | `NK` .. `SINIT`, `:2498-2516` (17) | `GdatModule::isgrd` (`ISGRD`, `:2279`) |
| `SINIT` block | `SGRDS(IMOD)%SINIT` (`:2518`) | `MAPWN` .. `DDEN2`, `:2520-2534` (14) | `W3setgTargets::sinit` |
| `MPARS`, unconditional | none | `PINIT` .. `B_JGS_TRUNK_DIGITS`, `:2541-2876` (106) | `GdatModule::ipars` (`IPARS`, `:2280`) |

`kW3setgAliases` lists the 252 as `{pointer, structure%component, line, group}` in the Fortran's
order.

- (v) `IGRID`, `ISGRD`, `IPARS` can be the targets of the unconditional groups, with no second
  home as `W3setaTargets::base` had to be for `IADATA`: at the pin only `W3SETG` writes them
  (`grep -E '^\s*(IGRID|ISGRD|IPARS)\s*=' model/src/*.F90` finds `:2278-2280` and the
  declaration `:619-620`; the `IGRID` of `ww3_gint.F90:1303`, `:1374` is a local of `W3EXGI`,
  declared at `:1059`), always in the same call that re-points those aliases.
- (v) A group whose condition is false keeps an earlier `W3SETG`'s model. The aliases are
  declared without `=> NULL()` (`w3gdatmd.F90:1097-1431`), so `W3setgTargets` starts at
  `std::nullopt`: a model number cannot mean "none", because models run from `NLOW = -NAUXGR` to
  `NGRIDS` (`W3NMOD`, `:1528-1547`): `NAUXGR = -1` without `NAUX` (models `1..NGRIDS`), and with
  `NAUX` present `0` and negative numbers are auxiliary grids.
- (v) `selected(v, gm, target)` returns slot `target + NAUXGR` of a caller's vector over
  `NLOW..NGRIDS`, `nullptr` for `std::nullopt` or a slot outside the vector.
- (v) The conditions read model `IMOD`: the GIMPLE loads `GINIT` and `SINIT` straight from
  `GRIDS(IMOD)` and `SGRDS(IMOD)` at the offsets of the aliases `:2384` and `:2516`. The C++ has
  two homes for those flags until the shim merges them (`IogrState::ginit`, `sinit` in
  `iogr_state.hpp:84`, `:141`, and `Sgrd::sinit` in `gdat_sgrd.hpp`), so the caller passes the
  readers `W3setgInputs::ginit` and `sinit`; `w3setg` calls each once, with `IMOD`, after the
  checks, as the Fortran reads them after the checks. An empty reader returns -101 before any
  write: the path never skips a read.

## Every alias, with the C++ member it corresponds to

Lines are `w3gdatmd.F90` at the pin; each is `X => GRIDS(IMOD)%X`, `SGRDS(IMOD)%X` or
`MPARS(IMOD)%<sub>%X`. "IogrState" is a field of the same name, lower case, in
[`iogr_state.hpp`](../src/ww_kokkos/iogr_state.hpp); a name with no member is "none yet".

| Group | Aliases (lines) | C++ member |
|---|---|---|
| `GRIDS` | `GTYPE` .. `FLAGLL` `:2285-2294`, `E3DF` .. `FFACBERG` `:2309-2314`, `SX` .. `Y0` `:2344-2347`, `DTCFL` .. `DMIN` `:2349-2353`, `CTMAX` .. `CMPRTRCK` `:2357-2360`, `FICEN` .. `AAIRCMIN` `:2366-2382`, `GINIT` .. `IC_NUMERICS` `:2384-2394`, `GNAME` .. `ZB` `:2396-2432` | `IogrState::` same name, except none yet for `RSTYPE` :2286, `NSEAL` :2292, `FETCH` :2381, `IICESMOOTH` :2393, `I_DIAG` :2416, `JA_IE` :2417, `NBEDGE` :2418, `EDGES` :2419, `NEIGH` :2420, `CROSSDIFF` :2426 |
| `GINIT` | `MAPSTA` :2436, `MAPST2` :2437, `MAPFS` :2438, `MAPSF` :2439, `CLATS` :2445, `CLATIS` :2446, `CTHG0S` :2447, `TRNX` :2448, `TRNY` :2449 | `IogrState::` same name |
| `GINIT` | `FLAGST` :2440, `DXDP` .. `HQFAC` :2451-2461, `GSU` :2491 | none yet |
| `SGRDS` | `NK`, `NK2`, `NTH`, `NSPEC` :2498-2501, `DTH` .. `FACHFE` :2503-2514, `SINIT` :2516 | `Sgrd::` same name (and `IogrState::`) |
| `SINIT` | `MAPWN`, `MAPTH` :2520-2521, `TH` .. `DDEN2` :2523-2534 | `Sgrd::` same name (and `IogrState::`) |
| `MPARS` | `PINIT` :2541 | none yet |
| `MPARS`, `NPARS` | `FACP` .. `FHMAX` :2545-2553, `RWINDC` :2555 | `IogrState::` same name |
| `MPARS`, `PROPS` | `WDCG`, `WDTH` :2568-2569 | `IogrState::` same name |
| `MPARS`, `SLNPS` | `SLNC1`, `FSPM`, `FSHF` :2616-2618 | `IogrState::` same name |
| `MPARS`, `SRCPS` | `WWNMEANPTAIL`, `SSTXFTFTAIL` :2623-2624, `ZZWND` .. `SSDSISO` :2673-2706, `SSINBR` :2713 | `IogrState::` same name |
| `MPARS`, `SRCPS` | `IKTAB`, `DCKI`, `QBI`, `CUMULW`, `SATINDICES`, `SATWEIGHTS` :2707-2712 | `Srcps::` same name (and `IogrState::`) |
| `MPARS`, `SNLPS` | `SNLC1` .. `GQAMP` :2738-2752 | `IogrState::` same name |
| `MPARS`, `SBTPS`, `SDBPS` | `SBTC1` :2819; `SDBC1`, `SDBC2`, `FDONLY` :2828-2830 | `IogrState::` same name |
| `MPARS`, `SDBPS` | `SDBSC` :2831 | none yet |
| `MPARS`, `SCHMS` | `FSBCCFL` .. `B_JGS_SOURCE_NONLINEAR` :2850-2875 | `IogrState::` same name, except none yet for `B_JGS_LIMITER_FUNC` :2870 |
| `MPARS`, `SCHMS` | `B_JGS_TRUNK_DIGITS` :2876 | none yet |

The member names were matched by a script over the field declarations of `Sgrd`, `Srcps` and
`IogrState` (v); that a member of the same name holds the same Fortran kind and shape is the
business of those ports, not checked here ⚠.

## Arithmetic and parity

The reference is the `-O3` object of `w3gdatmd.F90`, compiled 2026-10-16 with gfortran 16.2 in
the pinned `ww3` shell (`nix develop picasso/labs/pratico#ww3`) with the Fortran flags and
`switch_lab_shrd` defines of WW3's CMake Release build (`ww3_lib.dir/flags.make`: `-O3
-fallow-argument-mismatch -g -fno-second-underscore -ffree-line-length-none`), plus
`-fdump-tree-optimized-lineno`; read as GIMPLE and assembly.

- (v) The object is WW3's: the whole disassembly of the recompiled `w3gdatmd.o` equals the one
  of the CMake build's `w3gdatmd.F90.o` in the same scratch directory, and so does the `.text`
  section (`objcopy -O binary --only-section=.text`, `cmp`).
- (v) Order: `NGRIDS == -1` is tested first (`:2262`), then `IMOD < -NAUXGR`, then
  `IMOD > NGRIDS` with `NGRIDS` reloaded (`:2267`); the `IMOD` loaded there is stored to
  `IGRID`, `ISGRD`, `IPARS` (`:2278-2280`). The port tests in the same order and returns 1 or 2;
  `EXTCDE` never returns, so nothing after a failed check runs, and the port writes nothing
  either. The C++ evaluates `imod < -nauxgr | imod > ngrids` without a branch between them
  (`-O3` GIMPLE); neither operand has a side effect, so the result is the Fortran's.
- (v) `EXTCDE` receives 1 and 2: `.rodata` of the object holds `1, 2` at offsets 0 and 4, the
  two `lea` before the `call __w3servmd_MOD_extcde` read them (the code-1 block is placed after
  the code-2 block, a layout choice).
- (v) The 252 aliases: a script that evaluates the `#ifdef`s with the defines above lists 252
  assignments; every one appears at its source line as a store in the GIMPLE and the GIMPLE
  stores no other module variable (the two other names it finds, `ngrids` and `nauxgr`, are
  loads); the 93 unconditional `GRIDS` stores are in basic block 7, the 22 `GINIT` ones in 8,
  the 17 `SGRDS` in 9, the 14 `SINIT` in 10, the 106 `MPARS` in 11, so the grouping of
  `kW3setgAliases` is the compiler's.
- (v) No floating-point arithmetic: the assembly of `__w3gdatmd_MOD_w3setg` holds `mov`,
  `movups`, `movdqu`, `lea` (descriptor copies and addresses), three `imul` and three `add`
  (structure strides), and calls only `_gfortran_st_write`, `_gfortran_transfer_integer_write`,
  `_gfortran_st_write_done`, `extcde` and `__stack_chk_fail`.
- (v) The C++ `-O3` GIMPLE of `w3setg` (a non-inline wrapper in a throwaway unit) loads `ngrids`,
  tests `== -1`, then `imod < -nauxgr | imod > ngrids`, then the two readers' managers (empty
  `std::function`), stores `igrid`, `isgrd`, `ipars`, calls the `ginit` reader and stores
  `t.ginit`, calls the `sinit` reader and stores `t.sinit`; the status PHI is
  `<1, 0, 2, -101>`. No floating-point instruction and no FMA in the object.
- (v) The message text: `w3setg_message()` and a throwaway gfortran 16.2 program with the two
  `FORMAT`s copied from `:2881-2886` wrote byte-identical output (759 bytes, `cmp`) for code 1
  and code 2 with `IMOD` 7, -3, -2147483647 (eleven characters: `I10` writes ten asterisks)
  and 1234567890. The label `NAUXGR` prints the value `-NAUXGR` (`:2268`), kept.

## ⚠

- ⚠ A model number stands for the components of `GRIDS(k)`, `SGRDS(k)`, `MPARS(k)` as they are
  when read; the Fortran copies each array component's descriptor at the `=>` (the GIMPLE copies
  e.g. `mapsta = GRIDS(IMOD).mapsta`). They differ when a component is re-allocated between a
  `W3SETG` and a read through its alias. `W3DIMX`, `W3DIMS` and `W3DIMUG` call `W3SETG`
  after they allocate; a grep of `model/src` for `ALLOCATE (GRIDS(`, `SGRDS(`,
  `MPARS(` outside `w3gdatmd.F90` finds components with a compiled alias re-allocated at
  `w3triamd.F90:1505-1506` (`JAA`, `IAA`), `w3wavset.F90:1853`, `:1893` (`NEIGH`, `EDGES`) and
  deallocated at `w3profsmd_pdlib.F90:619` (`IOBP`, `IOBPD`). Not checked whether a read through
  those aliases follows before the next `W3SETG`; all of them are unstructured-grid or setup
  paths that the lab case does not run.
- ⚠ The W3DIMS port sets `gm.igrid`, `isgrd`, `ipars` for its `W3SETG` call (`:2116`) but not
  `W3setgTargets::sinit`, which the Fortran re-points there (`SINIT` is `.TRUE.` from `:2108`).
  The caller must call `w3setg` after an ok return of `w3dims` for the `GINIT` and `SINIT`
  groups to match. Not run.
- ⚠ Before any `W3SETG`, `gm.igrid` is -1 (`:619`), which is also auxiliary grid -1 when `W3NMOD`
  made one; `selected()` then returns that grid's slot where the Fortran aliases are undefined.
  The Fortran has the same ambiguity in `IGRID` itself (`W3WAVE` compares it with `IMOD`,
  `w3wavemd.F90:695`).
- ⚠ `W3setgStatus::missing_input` (-101) is not a WW3 code; it guards a caller that did not fill
  `W3setgInputs`, and is tested after the two Fortran checks and before any write.
- ⚠ `-gm.nauxgr` is undefined in C++ for `INT_MIN`, where gfortran's `neg` wraps; `NAUXGR` is
  `-NLOW` with `NLOW` `1` or `-NAUX` (`:1528-1543`), so not reachable.
- ⚠ The port does not test that the caller's vectors have `NGRIDS + NAUXGR + 1` slots;
  `selected()` returns `nullptr` outside them.

## Not ported

- The `#ifdef` blocks listed under "What the switch compiles", not compiled under the lab switch.
- `EXTCDE`: returned as the code it would exit with. The `WRITE(NDSE)` itself: the shim prints
  the text of `w3setg_message()` to `NDSE`; `NDSE`, `NDST`.
- The aliases with no C++ member yet (listed above): their group target is
  `GdatModule::igrid`, `ipars` or `W3setgTargets::ginit`, and the port that adds the member
  inherits it.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and the routine only moves indices.

## How it was regtested

Not yet, and the port's code was never run inside WW3. What was done, 2026-10-16 (v), in the
pinned `ww3` shell (gfortran and g++ 16.2, Kokkos 5.2.2 OpenMP):

- `build.sh`: `cmake -DCMAKE_BUILD_TYPE=Release -DSWITCH=<switch_lab_shrd>` on `WW3/`,
  `cmake --build --target ww3_lib`, `gfortran` on `w3gdatmd.F90` with the flags of
  `flags.make` and `-fdump-tree-optimized-lineno`, `gfortran -E` for the preprocessed source,
  `objdump -d`, `cmp`.
- `cxx.sh`: a throwaway translation unit with non-inline wrappers of `w3setg` and
  `w3setg_message` and `selected<Sgrd<HostSpace>>` explicitly instantiated, compiled with no
  warning under `-Werror`:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter -Werror \
    -ffp-contract=off -fopenmp -fdump-tree-optimized -c tu_w3setg.cpp
```

  and again at `-O0 -g -Wsign-conversion -fsanitize=address,undefined`; `objdump -d` of the `-O3`
  object has no `vfmadd`/`vfmsub`/`vfnmadd` and no scalar or packed floating-point arithmetic.
  The same unit compiled with `nvcc_wrapper -arch=sm_89 --fmad=false` in the `#cuda` shell
  (build only, never run).
- A copy of `wave_support.cpp` with the header and `(void)&ww::gdat::w3setg;`,
  `selected<Sgrd<E::memory_space>>`, `w3setg_message` added, in a copy of `kokkos/`, built as
  the object of `ww_kokkos` with `cmake --preset openmp-release` and `serial-debug`: no
  warning. On `cuda-release` the copy fails with three errors that the unmodified
  `wave_support.cpp` gives too (`__builtin_assoc_barrier` undefined in `ic3_wn_cmplx_hf.hpp`,
  `profs_dlassq.hpp`, `tide_dpythag.hpp`), none in this header.
- The only program run was the throwaway comparison of the message text with gfortran's
  `FORMAT` output (not kept, ADR-0010).
