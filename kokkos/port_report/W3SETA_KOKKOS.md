# W3SETA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3INIT`/`W3IOGO` wiring first.

- (v) Fresh review 2026-10-16, in `/tmp/claude-1000/seta-review/` (own CMake Release build of
  `ww3_lib` under `switch_lab_shrd`; recompiled `w3adatmd.o` disassembles identically to the CMake
  object; its `-O3` GIMPLE of `W3SETA` read): the checks, their order, the `EXTCDE` codes and
  `IADATA = IMOD` match; the five conditions load `WADATS(IMOD)` through the aliases of `:2758`,
  `:2760`, `:2809`, and `INPUTS(IMOD)`; the 218 aliases compiled under the switch are each in the
  GIMPLE and each in the table below, no more. Defects found and fixed: (1) `Wadats::iadata` was
  the target of the unconditional aliases, but `W3XETA` writes `IADATA` (`:3174`) without
  re-pointing them, so they now have their own target `W3setaTargets::base`; (2) the 93 aliases
  `HS` .. `USERO` (`:2839-2939`) are also re-pointed by `W3XETA` at the `X*` arrays
  (`:3181-3280`, the same 93 names), so they get `W3setaTargets::x`, cleared by the `AINIT`
  branch; (3) the accessors took `reg.iadata` and now take a target; (4) the `NLINIT`
  duplication note was stale (one home, `WadatNl1::nlinit`). Recompiled with no warning under
  `-Werror`, instantiated on `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, at
  `-O3` and at `-O0 -fsanitize=address,undefined`; never linked or run.

Selects model `IMOD` in W3ADATMD: two checks, `IADATA = IMOD`, then the module pointer aliases
are re-pointed at the components of `WADATS(IMOD)`, some always and some only under a
condition. It does no floating-point arithmetic and stores no array element.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SETA`, `model/src/w3adatmd.F90:2638-3100` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`), as `switches/switch_lab_shrd` compiles it (`W3_PR3`, `W3_NL1` in) |
| Kokkos code | [`../src/ww_kokkos/adat_w3seta.hpp`](../src/ww_kokkos/adat_w3seta.hpp), host-side function templates over the execution space, on the structs of [`adat_wadat.hpp`](../src/ww_kokkos/adat_wadat.hpp) (W3DIMA), [`adat_w3naux.hpp`](../src/ww_kokkos/adat_w3naux.hpp) and [`adat_w3dmnl.hpp`](../src/ww_kokkos/adat_w3dmnl.hpp) |
| Memory | no View is created, copied or read; only host integers are written |
| Errors | the `EXTCDE` exit codes returned as `W3setaStatus` (1, 2); `EXTCDE` always ends in `CALL EXIT` (`w3servmd.F90:1207`) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## How a pointer alias is represented

The C++ has no module pointers: a consumer indexes the model vector. What `W3SETA` leaves behind
is, for each group of aliases it re-points under one condition, the model the group now points
at. A group whose condition is false keeps the target of an earlier `W3SETA`, so one index
(`IADATA`) is not enough:

| Group | Condition, read from model `IMOD` | C++ record |
|---|---|---|
| unconditional | none | `W3setaTargets::base` |
| `AINIT` block | `WADATS(IMOD)%AINIT` (`:2826`) | `W3setaTargets::ainit` |
| `HS` .. `USERO` (`:2839-2939`), in the `AINIT` block | as `AINIT`; `W3XETA` re-points them at `WADATS(k)%X*` | `W3setaTargets::x`: 0 = plain arrays of `ainit`, k = `X*` of model k |
| `FL_ALL` block | and `WADATS(IMOD)%FL_ALL` (`:2947`) | `W3setaTargets::fl_all` |
| currents | and `INPUTS(IMOD)%INFLAGS1(2)` (`:2974`) | `W3setaTargets::cur` |
| wind | and `INPUTS(IMOD)%INFLAGS1(3)` (`:2981`) | `W3setaTargets::wind` |
| momentum | and `INPUTS(IMOD)%INFLAGS1(5)` (`:2990`) | `W3setaTargets::taua` |
| air density | and `INPUTS(IMOD)%INFLAGS1(6)` (`:2997`) | `W3setaTargets::rhoa` |
| `ITER` | and `GTYPE == UNGTYPE` (`:3028`) | `W3setaTargets::iter` |
| NL1 tables | `WADATS(IMOD)%NLINIT` (`:3038`) | `W3setaTargets::nlinit` |

A target of 0 means no `W3SETA` has associated the group (except `x`); the Fortran declares
these pointers without `=> NULL()` (`w3adatmd.F90:578-690`), so their status is undefined until
then. `IADATA` (`:2746`, `Wadats::iadata`) is written by `W3SETA` and by `W3XETA` (`:3174`), and
`W3XETA` re-points only `HS` .. `USERO`, so `IADATA` is not the target of any group.
`selected(reg, target)`, `selected_scalars(scalars, target)` and `selected_nl1(nl1, target)`
return the model's struct, `nullptr` for a target outside `1..NADATA`.

- (v) A model number stands for the components of `WADATS(k)` as they are when read. For an
  array component (a `POINTER` component, `w3adatmd.F90:384-570`) the Fortran copies its
  association at the `=>` (the GIMPLE copies the descriptor, e.g. `usero = WADATS(IMOD).usero`).
  The two agree while no component is re-associated after `W3SETA`: `W3DIMA` and `W3DMNL`
  refuse a second allocation (`:976`, `:2525`), and a grep of `model/src` at the pin for
  `ALLOCATE`, `DEALLOCATE`, `NULLIFY` and `=>` on the 218 names finds, outside `W3ADATMD`, only
  local arrays and `USE` renames.

## Every alias, with the C++ member it corresponds to

Lines are `w3adatmd.F90` at the pin; each is `X => WADATS(IMOD)%X`.

| Group | Aliases (line) | C++ member |
|---|---|---|
| unconditional | `NSEALM` :2754, `AINIT` :2758, `FL_ALL` :2760 | `Wadat::nsealm`, `ainit`, `fl_all` |
| unconditional | `ITIME` :2751, `IPASS` :2752, `IDLAST` :2753, `ITSTEP` :2755, `FLCOLD` :2756, `FLIWND` :2757, `AINIT2` :2759 | `WadatScalars::itime`, `ipass`, `idlast`, `itstep`, `flcold`, `fliwnd`, `ainit2` |
| unconditional, `W3_PR3` | `NMX0` :2774, `NMX1` :2775, `NMX2` :2776, `NMY0` :2777, `NMY1` :2778, `NMY2` :2779, `NACT` :2780, `NCENT` :2781 | none yet (written by the PR3 map routines, not ported) |
| unconditional, `W3_NL1` | `NSPECX` :2788 | `WadatNl1::nspecx` |
| unconditional, `W3_NL1` | `NLINIT` :2809 | `WadatNl1::nlinit` |
| unconditional, `W3_NL1` | `NFR` :2785, `NFRHGH` :2786, `NFRCHG` :2787, `NSPECY` :2789, `DAL1` :2790, `DAL2` :2791, `DAL3` :2792, `AWG1`..`AWG8` :2793-2800, `SWG1`..`SWG8` :2801-2808 | none yet (INSNL1's) |
| `AINIT` | `DW` :2828, `UA` :2829, `UD` :2830, `U10` :2831, `U10D` :2832, `AS` :2833, `CX` :2834, `CY` :2835, `TAUA` :2836, `TAUADIR` :2837 | `Wadat::` same name, lower case |
| `AINIT` | `HS` :2839, `WLM` :2840, `T02` :2841, `T0M1` :2842, `T01` :2843, `FP0` :2844, `THM` :2845, `THS` :2846, `THP0` :2847, `HSIG` :2848, `STMAXE` :2849, `STMAXD` :2850, `HMAXE` :2851, `HMAXD` :2852, `HCMAXE` :2853, `HCMAXD` :2854, `QP` :2855, `WBT` :2856, `WNMEAN` :2857 | `Wadat::` same name |
| `AINIT` | `EF` :2859, `TH1M` :2860, `STH1M` :2861, `TH2M` :2862, `STH2M` :2863 | `Wadat::` same name (lower bounds in `*_lb`) |
| `AINIT` | `PHS` :2865, `PTP` :2866, `PLP` :2867, `PDIR` :2868, `PSI` :2869, `PWS` :2870, `PWST` :2871, `PNR` :2872, `PTHP0` :2873, `PQP` :2874, `PPE` :2875, `PGW` :2876, `PSW` :2877, `PTM1` :2878, `PT1` :2879, `PT2` :2880, `PEP` :2881 | `Wadat::` same name |
| `AINIT` | `CHARN` :2883, `TWS` :2884, `CGE` :2885, `PHIAW` :2886, `TAUWIX` :2887, `TAUWIY` :2888, `TAUWNX` :2889, `TAUWNY` :2890, `WHITECAP` :2891 | `Wadat::` same name |
| `AINIT` | `SXX` :2893, `SYY` :2894, `SXY` :2895, `TAUOX` :2896, `TAUOY` :2897, `BHD` :2898, `PHIOC` :2899, `TUSX` :2900, `TUSY` :2901, `USSX` :2902, `USSY` :2903, `PRMS` :2904, `TPMS` :2905, `P2SMS` :2906, `US3D` :2907, `PHICE` :2908, `TAUICE` :2909, `USSP` :2910, `TAUOCX` :2911, `TAUOCY` :2912 | `Wadat::` same name (`p2sms_lb`) |
| `AINIT` | `ABA` :2914, `ABD` :2915, `UBA` :2916, `UBD` :2917, `BEDFORMS` :2918, `PHIBBL` :2919, `TAUBBL` :2920 | `Wadat::` same name |
| `AINIT` | `MSSX` :2922, `MSSY` :2923, `MSSD` :2924, `MSCX` :2925, `MSCY` :2926, `MSCD` :2927, `QKK` :2928, `SKEW` :2929, `EMBIA1` :2930, `EMBIA2` :2931 | `Wadat::` same name |
| `AINIT` | `DTDYN` :2933, `FCUT` :2934, `CFLXYMAX` :2935, `CFLTHMAX` :2936, `CFLKMAX` :2937, `USERO` :2939, `WN` :2941 | `Wadat::` same name |
| `FL_ALL` | `CG` :2949, `ATRNX` :2954, `ATRNY` :2955, `DDDX` :2957, `DDDY` :2958, `DCDX` :2959, `DCDY` :2960, `DCXDX` :2961, `DCYDX` :2962, `DCXDY` :2963, `DCYDY` :2964, `ALPHA` :2972 | `Wadat::` same name |
| currents | `CA0` :2975, `CAI` :2976, `CD0` :2977, `CDI` :2978 | `Wadat::` same name |
| wind | `UA0` :2982, `UAI` :2983, `UD0` :2984, `UDI` :2985, `AS0` :2986, `ASI` :2987 | `Wadat::` same name |
| momentum | `MA0` :2991, `MAI` :2992, `MD0` :2993, `MDI` :2994 | `Wadat::` same name |
| air density | `RA0` :2998, `RAI` :2999 | `Wadat::` same name |
| `FL_ALL`, `W3_PR3` | `MAPX2` :3019, `MAPY2` :3020, `MAPAXY` :3021, `MAPCXY` :3022, `MAPTH2` :3023, `MAPWN2` :3024, `MAPTRN` :3025 | `Wadat::` same name |
| `ITER` | `ITER` :3028 | `Wadat::iter` |
| `FL_ALL` | `IAPPRO` :3030, `SPPNT` :3031 | `Wadat::iappro`, `sppnt` |
| NL1 tables | `IP11`..`IP14` :3039-3042, `IM11`..`IM14` :3043-3046, `IP21`..`IP24` :3047-3050, `IM21`..`IM24` :3051-3054, `IC11`..`IC82` :3055-3070, `AF11` :3071 | `WadatNl1::` same name |
| not compiled | `W3_PR2` :2763-2770, :3010-3015; `W3_MPI` :2813-2823, :3076-3081; `W3_IC3` :2943-2944, :2951; `W3_SMC` :2967-2969; `W3_PR1` :3003-3006 | none |

## Arithmetic and parity

The reference is the `-O3` object of `w3adatmd.F90`, compiled 2026-10-16 with gfortran 16.2 in
the pinned `ww3` shell, the Fortran flags and `switch_lab_shrd` defines of WW3's CMake Release
build (`ww3_lib.dir/flags.make`), plus `-fdump-tree-optimized-lineno`; read as GIMPLE and
assembly.

- (v) The object is WW3's: the whole disassembly of the recompiled `w3adatmd.o` equals the one
  of the CMake build's `w3adatmd.F90.o` in the same scratch directory except for the file name
  line, and `__w3adatmd_MOD_w3seta` (2107 lines) is identical.
- (v) Order: `NADATA == -1` is tested first (`:2729`), then `IMOD <= 0`, then `IMOD > NADATA`
  (`:2734`), then `IADATA = IMOD` (`:2746`) stores the `IMOD` loaded once at `:2734`. The port
  tests in the same order and returns 1 or 2; `EXTCDE` never returns (`CALL EXIT`,
  `w3servmd.F90:1207`), so nothing after a failed check runs, and the port writes nothing either.
- (v) The five conditions read model `IMOD`: GIMPLE loads `AINIT` (`:2826`), `FL_ALL`
  (`:2947`) and `NLINIT` (`:3038`) from `WADATS(IMOD)` at the offsets of `:2758`, `:2760` and
  `:2809`, and `INFLAGS1(2,3,5,6)` from `INPUTS(IMOD)`. `GTYPE` is loaded through the W3GDATMD
  pointer and compared with the constant 3 (`UNGTYPE`, `w3gdatmd.F90:627`): it is the grid of
  the last `W3SETG`, which the caller passes in `W3setaInputs::gtype`.
- (v) The nesting is the Fortran's: the four forcing groups and `ITER` sit inside `FL_ALL`,
  inside `AINIT`; `NLINIT` is tested after the `AINIT` block, independently of it.
- (v) The C++ `-O3` GIMPLE of `w3seta<Kokkos::OpenMP>` loads `nadata`, tests `== -1`, then
  `imod <= 0 | nadata < imod`, then the `nl1` size, stores `iadata` and `base`, reads `ainit`
  (then stores `ainit`, `x = 0`), and nests `fl_all`, the four forcing flags and `gtype` as the
  Fortran does, `nlinit` after; no floating-point instruction, no FMA in the object.

## ⚠

- ⚠ The W3DIMA and W3DMNL ports do the unconditional part of their `W3SETA` calls (`:994`,
  `:1467`; `:2584`): `Wadats::iadata` and `W3setaTargets::base`. They cannot call `w3seta`
  (this header includes theirs, and they lack `nl1` or `W3setaInputs`), so the caller must call
  `w3seta(reg, nl1, t, imod, in)` after an `ok` return of `w3dima` and of `w3dmnl` for the
  `AINIT`, `x`, `FL_ALL`, forcing, `ITER` and NL1 groups to match the Fortran; their reports
  argue why one call is enough. Not run.
- (v) W3DIMA's port stores `FL_ALL` (`:960-964`) and reads `NSEALM` (`:1038-1039`) and `FL_ALL`
  (`:1323`) through `W3setaTargets::base`, as the Fortran does through the aliases (`:2754`,
  `:2760`); `adat_wadat.hpp` says `iadata` is the Fortran `IADATA`, not an alias target.
  `W3setaTargets` itself lives in `adat_wadat.hpp` so W3DIMA and W3DMNL can name it; this header
  defines the groups. ⚠ Not checked whether any caller at the pin runs `W3DIMA` after a `W3XETA`
  of another model, where `base` and `IADATA` differ.
- ⚠ `W3setaStatus::nl1_not_sized` (-1) is not a WW3 code; in the Fortran `NLINIT` lives inside
  `WADATS(IMOD)`, so this cannot happen there, nor here after `w3naux`, which sizes
  `nl1.models` with `reg.wadats`. It guards a caller that built the two vectors apart, and is
  tested after the two Fortran checks and before any write.
- ⚠ The port assumes `reg.wadats.size() == reg.nadata`, as W3NAUX's port leaves it; it does not
  test it.
- ⚠ A target of 0 stands for an undefined Fortran association status; the Fortran would read
  garbage through such a pointer, the accessors return `nullptr`.

## Not ported

- `W3_PR2`, `W3_MPI`, `W3_IC3`, `W3_SMC`, `W3_PR1`, the `W3_S` STRACE and the `W3_T` print: not
  compiled under the lab switch.
- The `WRITE(NDSE)` messages and `EXTCDE`: returned as the code it would exit with; `NDSE`, `NDST`.
- The aliases with no C++ member yet (the `W3_PR3` scalars `NMX0`..`NCENT` and the INSNL1 scalars
  `NFR`..`SWG8`): their group target is `W3setaTargets::base`, and the port that adds the member
  inherits it.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and the routine only moves indices.

## How it was regtested

Not yet, and the port's code was never run. What was done, 2026-10-16 (v), in
`/tmp/claude-1000/seta-scratch/`:

- `build.sh` in the `ww3` shell: `cmake -DCMAKE_BUILD_TYPE=Release -DSWITCH=<switch_lab_shrd>`,
  `cmake --build --target ww3_lib`, `gfortran` on `w3adatmd.F90` with the flags of
  `flags.make` and `-fdump-tree-optimized-lineno`, `objdump -d`, `diff`.
- `cxx.sh`: a throwaway translation unit `tu_w3seta.cpp` that explicitly instantiates `w3seta`
  and the four accessors on `Kokkos::DefaultExecutionSpace` (OpenMP in the shell), compiled with
  g++ 16.2 with no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
    -ffp-contract=off -fopenmp -fdump-tree-optimized -c tu_w3seta.cpp
```

The same unit also compiled at `-O0 -g` with `-Wsign-conversion` and
`-fsanitize=address,undefined`, again with no warning; `objdump -d` of the `-O3` object has no
`vfmadd`/`vfmsub`/`vfnmadd`. It was never linked or run. The only programs run were `cmake`,
the two compilers, `objdump`, `diff`, `grep`, `awk` and a `python3` script that listed the
aliases from the source for the table above.
