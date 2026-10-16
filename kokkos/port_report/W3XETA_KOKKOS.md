# W3XETA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3IOGO`/`W3MPIO` wiring first, and no lab-switch run calls it (every caller at the pin
is under `W3_MPI` or `PDLIB`, see below).

- (v) Fresh review 2026-10-16: own WW3 CMake Release build (`ww3_lib`, `switch_lab_shrd`) and
  `-O3 -fdump-tree-optimized-lineno` object of `w3adatmd.F90` in `/tmp/claude-1000/xeta-review/`,
  disassembly equal to the CMake object's; checks, exit codes 1 and 2, the `IADATA` store before the
  `AINIT2` pointer load, and the 93 `kXetaAliases` rows (pointer, `X` component, line) diffed
  against the GIMPLE and their `W3SETA` lines against the source, no difference; every line
  citation re-read at the pin. One defect found and fixed: the port kept the Fortran `IADATA` in a
  second home (`W3xetaState::iadata`, beside `Wadats::iadata`) and read `AINIT2` through
  `Wadats::iadata`. Now `W3XETA` writes the one `Wadats::iadata` (`:3174`), reads `AINIT2` from
  `W3setaTargets::base` (the unconditional group, `:2759`) and sets `W3setaTargets::x`;
  `W3xetaState` is removed.

W3XETA is "Reduced version of W3SETA to point t expended output arrays" (`w3adatmd.F90:3127`):
it checks `IMOD`, writes `IADATA` and, when `AINIT2` holds, points 93 output pointers of
W3ADATMD at the `X*` (full-grid, expanded) components of `WADATS(IMOD)` that `W3XDMA`
allocates. It does no floating-point arithmetic and touches no array element.

## What was ported

| | |
|---|---|
| WW3 routine | `W3XETA`, `model/src/w3adatmd.F90:3112-3300` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`), as `switches/switch_lab_shrd` compiles it |
| Switch | the lab switch: `W3XETA` is outside every `#ifdef`, so `switch_lab_shrd` compiles it (only `W3_S` and `W3_T` lines inside it are conditional, neither in the switch) |
| Kokkos code | [`../src/ww_kokkos/adat_w3xeta.hpp`](../src/ww_kokkos/adat_w3xeta.hpp), a host-side function template over the execution space; `Wadats` from [`adat_wadat.hpp`](../src/ww_kokkos/adat_wadat.hpp), `WadatScalars` (holds `AINIT2`) from [`adat_w3naux.hpp`](../src/ww_kokkos/adat_w3naux.hpp) |
| Memory | none: no View is created, read or written |
| Errors | the `EXTCDE` exit codes returned as `W3xetaStatus` (1, 2), plus -1 (`ainit2_unassociated`), not a WW3 code |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Order of operations

As the Fortran, `w3adatmd.F90`:

1. `NADATA == -1` exits 1 (`:3157-3160`).
2. `IMOD < 1` or `IMOD > NADATA` exits 2 (`:3162-3165`).
3. `IADATA = IMOD` (`:3174`), into `Wadats::iadata`, the same field `W3SETA` writes (`:2746`).
4. `IF (AINIT2)` (`:3179`), `AINIT2` read through the module pointer, which `W3XETA` does not
   re-point: `WADATS(J)%AINIT2` with `J` the model of the last `W3SETA` (`:2759`, unconditional),
   in the port `scalars[t.base-1].ainit2` (`W3setaTargets::base`, not `ainit`).
5. When it holds, the 93 pointer assignments `:3181-3280`, in the port `W3setaTargets::x = imod`.

## How it differs from W3SETA

| | `W3SETA` (`:2638-3100`, [`adat_w3seta.hpp`](../src/ww_kokkos/adat_w3seta.hpp)) | `W3XETA` (`:3112-3300`, this port) |
|---|---|---|
| `IADATA` | `= IMOD` (`:2746`), `Wadats::iadata` | `= IMOD` (`:3174`), the same `Wadats::iadata` |
| Scalar aliases `NSEALM`, `AINIT`, `AINIT2`, `FL_ALL`, ... | re-pointed at `WADATS(IMOD)` (`:2751-2760`) | not re-pointed |
| Condition | `AINIT` of `IMOD` (`:2826`) | `AINIT2` of the last `W3SETA`'s model (`:3179`) |
| `HS .. USERO` (the 93 of the table below) | `WADATS(IMOD)%HS` .. `%USERO` (`:2839-2939`), `W3setaTargets::x = 0` | `WADATS(IMOD)%XHS` .. `%XUSERO` (`:3181-3280`), `W3setaTargets::x = IMOD` |
| `DW .. TAUADIR`, `WN`, `CG`, `FL_ALL` group, NL1 tables | re-pointed | not re-pointed |

The trap kept from the Fortran: after `W3XETA(IMOD)` the Fortran `IADATA` is `IMOD`, while
`NSEALM`, `AINIT`, `AINIT2`, `FL_ALL` and the plain arrays still alias the model of the last
`W3SETA`. The Fortran has one `IADATA`, so the port has one, `Wadats::iadata`, written by both
routines; the alias targets are `W3setaTargets` (`adat_w3seta.hpp`): `base` for the unconditional
group, `ainit` for `DW .. TAUADIR`, `WN`, `x` for the 93 below. `W3WAVE` tests `IADATA` to skip its
`W3SETA` (`w3wavemd.F90:700`), so after `W3XETA(IMOD)` it skips it for that `IMOD`.

## The aliases W3XETA re-points

All 93 are `REAL, POINTER` module variables of W3ADATMD, each re-pointed by `W3XETA` at
`WADATS(IMOD)%X<name>` and by `W3SETA` at `WADATS(IMOD)%<name>`. The C++ exposes them as one
group: `W3setaTargets::x` is the model whose `X*` components they name (0 while they name the
plain components of `W3setaTargets::ainit`, or nothing), and `kXetaAliases` lists each one as
`{pointer, target, W3XETA line, W3SETA line}`; the X Views themselves belong to the `W3XDMA` port.

| Pointer → `WADATS(IMOD)%` | `W3XETA` line | `W3SETA` line |
|---|---|---|
| `HS`, `WLM`, `T02`, `T0M1`, `T01`, `FP0`, `THM`, `THS`, `THP0`, `HSIG`, `STMAXE`, `STMAXD`, `HMAXE`, `HMAXD`, `HCMAXE`, `HCMAXD`, `QP`, `WBT`, `WNMEAN` → `X<name>` | `:3181-3199` | `:2839-2857` |
| `EF`, `TH1M`, `STH1M`, `TH2M`, `STH2M` → `X<name>` | `:3201-3205` | `:2859-2863` |
| `PHS`, `PTP`, `PLP`, `PDIR`, `PSI`, `PWS`, `PWST`, `PNR`, `PTHP0`, `PQP`, `PPE`, `PGW`, `PSW`, `PTM1`, `PT1`, `PT2`, `PEP` → `X<name>` | `:3207-3223` | `:2865-2881` |
| `CHARN`, `TWS`, `CGE`, `PHIAW`, `TAUWIX`, `TAUWIY`, `TAUWNX`, `TAUWNY`, `WHITECAP` → `X<name>` | `:3225-3233` | `:2883-2891` |
| `SXX`, `SYY`, `SXY`, `TAUOX`, `TAUOY`, `BHD`, `PHIOC`, `TUSX`, `TUSY`, `USSX`, `USSY`, `PRMS`, `TPMS`, `P2SMS`, `US3D`, `PHICE`, `TAUICE`, `USSP`, `TAUOCX`, `TAUOCY` → `X<name>` | `:3235-3254` | `:2893-2912` |
| `ABA`, `ABD`, `UBA`, `UBD`, `BEDFORMS`, `PHIBBL`, `TAUBBL` → `X<name>` | `:3255-3261` | `:2914-2920` |
| `MSSX`, `MSSY`, `MSSD`, `MSCX`, `MSCY`, `MSCD`, `QKK`, `SKEW`, `EMBIA1`, `EMBIA2` → `X<name>` | `:3263-3272` | `:2922-2931` |
| `DTDYN`, `FCUT`, `CFLXYMAX`, `CFLTHMAX`, `CFLKMAX` → `X<name>` | `:3274-3278` | `:2933-2937` |
| `USERO` → `XUSERO` | `:3280` | `:2939` |

Each row's lines are consecutive, one assignment per line, in the order listed.

## Arithmetic and parity

The reference is the `-O3` object of `w3adatmd.F90`, compiled 2026-10-16 with gfortran 16.2 in
the pinned `ww3` shell, the Fortran flags of WW3's CMake Release build
(`ww3_lib.dir/flags.make`: `-O3 -fallow-argument-mismatch -g -fno-second-underscore
-ffree-line-length-none -O3`) and the `switch_lab_shrd` defines, plus
`-fdump-tree-optimized-lineno`; read as GIMPLE and assembly.

- (v) The object is WW3's: `objdump -d` of the recompiled `w3adatmd.F90.o` equals that of the
  CMake build's object (`ww3_lib` target, same source path) in every line but the file name of
  the header; `__w3adatmd_MOD_w3xeta` is 1060 identical lines.
- (v) Order: `nadata` loaded and tested against -1 (`:3157`); `*imod` loaded once, tested
  `<= 0`, then `nadata` reloaded and `imod > nadata` (`:3162`); `iadata = imod` (`:3174`) stored
  before the module pointer `ainit2` is loaded and dereferenced (`:3179`). The port tests in the
  same order and writes `reg.iadata` before the `AINIT2` read. The reload of `NADATA` only
  matters on the path after `EXTCDE(1)`, which never returns (`CALL EXIT`,
  `w3servmd.F90:1207`); the port returns there.
- (v) The 93 assignments are descriptor copies of `wadats.data[wadats.offset + imod].x<name>`,
  the index from the `imod` value loaded at `:3162`; 93 `MEM <struct wadat[0:]>` reads in the
  GIMPLE, no arithmetic on the data. The C++ records `imod` instead of copying descriptors.
- (v) `kXetaAliases` was generated from the source at the pin (an `awk` over
  `w3adatmd.F90:3179-3285` and `:2828-2941`), 93 entries, every `W3SETA` line found; the
  first, last and count are `static_assert`ed in the throwaway TU. The fresh review diffed all 93
  `(line, pointer, X component)` triples against the GIMPLE's `MEM <struct wadat[0:]>` reads and
  every `W3SETA` line against `:2826-2945`: equal; the `AINIT` group aliases left out are `DW ..
  TAUADIR`, `WN` and the `W3_IC3` pair, not compiled.
- (v) `UNGTYPE` is `USE`d (`:3133`) and never read; `NDSE` only by the two `WRITE`s and `NDST`
  only under `W3_T`.
- (v) The C++ GIMPLE (`-fdump-tree-optimized`, instantiations on `Kokkos::OpenMP`, the default
  execution space of the shell's Kokkos, and `Kokkos::Serial`) has the same three tests and the
  `reg.iadata` store before the `t.base` load; no floating-point operation, no FMA in the
  assembly. The C++ evaluates `imod <= 0` and `imod > nadata` together; neither has a side effect.
- ⚠ `ainit2_unassociated` (-1) is not a WW3 code. `AINIT2` is declared without `=> NULL()`
  (`:690`); before any `W3SETA` the Fortran dereferences it, undefined behaviour (the symbol
  `__w3adatmd_MOD_ainit2` is in `.bss`, `nm`, so a NULL load; not run). No caller at the pin does
  this; the port returns -1 after writing `reg.iadata`, as far as the Fortran had got.
- ⚠ The `AINIT2` read is of the last `W3SETA`'s model, not `IMOD`'s; the two coincide at
  `W3IOGO` (`W3SETA(IGRD)` at `w3iogomd.F90:2605` before `:2607`) and in `W3MPIO` only when
  `W3INIT`'s `W3SETA` (`w3initmd.F90:550`) selected the same `IMOD`. Kept as the Fortran does it.
- ⚠ The header compiles only against the `adat_w3seta.hpp` that holds `W3setaTargets::base` and
  `::x` (uncommitted, read 2026-10-16; its `W3SETA` sets `base` always and `x = 0` in the `AINIT`
  branch, `:2826`). It has to be committed with or before this one.
- ⚠ Committed headers still treat `Wadats::iadata` as the unconditional-alias target:
  `adat_wadat.hpp` says a `W3XETA` port must not write it, and `W3DIMA` reads `FL_ALL` through it
  (`adat_w3dima.hpp`, `:960-964`). After a `W3XETA(j)` with no `W3SETA(j)` they name `j`, the
  Fortran names `base`. `W3XDMA` already reads `NSEALM` through `W3setaTargets::base` (`:1596`).
- (v) `AINIT2` has one home, `WadatScalars::ainit2`: `W3NAUX` clears it (`:806`), `W3XDMA` sets it
  (`:2375`) and this routine reads it through `base`.
- ⚠ As in the Fortran, `x` is set whether or not `W3XDMA` allocated `WADATS(IMOD)`'s X arrays;
  `W3MPIO` and `DO_OUTPUT_EXCHANGES` call `W3XDMA(IMOD)` first (`w3initmd.F90:3440-3442`,
  `pdlib_field_vec.F90:1324`), `W3MPIO`'s restart branch only when `NAPRST /= NAPFLD`
  (`w3initmd.F90:4875`), `W3IOGO` (`w3iogomd.F90:2607`) and `W3IORS` (`w3iorsmd.F90:1046`) rely
  on an earlier call; not traced.

## Not ported

- `W3_S` STRACE (`:3136-3138`, `:3149-3152`) and the `W3_T` print (`:3167-3169`): not compiled
  under the lab switch.
- The `WRITE(NDSE,1001/1002)` messages and `EXTCDE`: returned as the code it would exit with.
- The pointer descriptors: the C++ has no module pointers; a consumer indexes the X Views of
  model `W3setaTargets::x` (the `W3XDMA` port's storage).

## Benchmarks

None. No timing without parity (`AGENTS.md`); the routine is a pointer swap with no arithmetic.

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v), in
`/tmp/claude-1000/xeta-scratch/` and the pinned shell
`nix develop "path:<worktree>/picasso/labs/pratico#ww3"`:

1. `cmake -S WW3 -B build -DSWITCH=switches/switch_lab_shrd -DCMAKE_BUILD_TYPE=Release` and
   `cmake --build build --target ww3_lib -j 8` (gfortran 16.2).
2. `gfortran` on `WW3/model/src/w3adatmd.F90` with the `flags.make` defines and flags plus
   `-fdump-tree-optimized-lineno`; `objdump -d --no-show-raw-insn` of that object and of the
   CMake one, compared with `cmp` and `diff`.
3. A throwaway TU that explicitly instantiates `ww::adat::w3xeta<Kokkos::DefaultExecutionSpace>`
   and `static_assert`s the alias table, compiled with no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -ffp-contract=off -fopenmp -fdump-tree-optimized -S tu_w3xeta.cpp
```

   and again with `-c` and the flag set without `-Wshadow -Wconversion -Wsign-conversion`. It was
   never linked or run. Helper programs: `cmake`, `gfortran`, `g++`, `objdump`, `cmp`, `diff`,
   `awk`, `grep`, `git diff --quiet 03adda91 HEAD -- model/src/w3adatmd.F90` (exit 0).
