# W3FLDH_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs a shim
and the `ww3_shel` wiring first.

- (v) Fresh review 2026-10-16: own WW3 CMake Release build (`switch_lab_shrd`, gfortran 16.2) and
  `-O3` GIMPLE in `/tmp/claude-1000/fldh-review/`, the recompiled object's `W3FLDH` disassembly
  identical to WW3's (717 lines each). Checked every arithmetic statement against the C++ GIMPLE
  (DERA constant, `2.7e+2 - hd` then the product, one `__builtin_cexpif` per branch with `HA`
  first, `(float) nh != 0.0`, the `DTTST` compares, `tick21(..., 1.0)`), no FMA in either object
  nor in the CUDA SASS, the control flow, the `+7` shift, the `IX`-outer/`IY`-inner nesting
  (the source order, `:2323-2324`), every `w3fldsmd.F90` citation and the callee citations in
  `w3timemd.F90`. Defects found and fixed: the caller row named seven calls at `:2134-2234` (the
  source has 14 at `:2134-2425`); the header's `@note` said the Fortran always reaches `DSEC21`,
  which an illegal `J` does not. No defect in the arithmetic or the control flow.

`W3FLDH` updates one homogeneous input field `J` of the generic shell for the interval `T0` to
`TN`. Its header Purpose reads "Update homogeneous input fields for the WAVEWATCH III generic
shell." (`model/src/w3fldsmd.F90:2167-2170`). It takes the next value from the list of
homogeneous values `ww3_shel` read (`THO`, `HA`, `HD`, `HS`), fills the field at `TFN` with it,
shifts the previous one to `TF0`, and repeats until the field time passes `T0`. Water levels,
ice, air density and the ice and mud parameters are a scalar (`FSN = HA`); currents and
atmospheric momentum are a vector built from speed and nautical direction; winds are a vector
plus the air-sea temperature difference (`HS`). The arithmetic is one subtraction, three
products and a `sincosf` per new vector value; everything else is stores and copies.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FLDH`, `model/src/w3fldsmd.F90:2149-2504` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/flds_w3fldh.hpp`](../src/ww_kokkos/flds_w3fldh.hpp), a host-side function template over the execution space; the `DO IX / DO IY` field loops are `parallel_for` over `IX` with the Fortran's inner `IY` loop |
| Memory | `FX0`..`FSN (MX,MY)` are `Kokkos::View<Real**, LayoutLeft, ExecSpace::memory_space>`, index `(ix-1, iy-1)`; `THO(2,-7:10,NHM)` is a `HostSpace` View indexed `(i-1, j+7, ih-1)`, `HA`, `HD`, `HS (NHM,-7:10)` `HostSpace` Views indexed `(ih-1, j+7)`; times are `std::array<int,2>` |
| Callees | `DSEC21` and `TICK21` through `FldhHooks` (function pointer plus context, the style of `wave_w3wave_hooks.hpp`); `WRITE (NDSE,1001)` through an optional print hook |
| Errors | `IERR` returned: `kFldhOk` 0, `kFldhIllegalField` 1, `kFldhPastLastData` -1 (`:2211-2214`); it is an output, not a stop. A missing `dsec21` or `tick21` hook calls `Kokkos::abort` |
| C shim | none yet |
| Caller | `ww3_shel`, `model/src/ww3_shel.F90:2134-2425` (14 call sites in the source, all `MX = NX`, `MY = NY`; most pass the scratch array `XXX` for several of the six fields) |
| Profile | `w3fldsmd:w3fldh` 0.00 % and 0.00 s self time in [`docs/data/ww3_full_gprof.tsv`](../../docs/data/ww3_full_gprof.tsv) (`just profile-case full`, WW3 `6d1bcdfe`, 2026-10-14): the full case has no homogeneous field |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Order of operations

As the Fortran, `w3fldsmd.F90`:

1. `IERR = 0`, `DERA = ATAN(1.)/45.` (`:2290-2291`).
2. `J < -7` or `J > 10`: the message when `NDSE >= 0`, `IERR = 1`, return (`:2302-2306`).
3. `FLFRST = TFN(1) .EQ. -1` (`:2307`).
4. Loop (`:2315-2446`): `TF0 = TFN`; when `TFN(1) /= -1`, shift `FXN, FYN` (J 2, 5),
   `FXN, FYN, FSN` (J 3) or `FSN` (J 6) into the `0` fields (`:2317-2357`). Then, when
   `NH .NE. 0.`, `TFN = THO(:,J,1)`, fill the new field (`:2364-2405`), shift the lists down
   by one and `NH = NH - 1` (`:2407-2416`); otherwise `TFN = TN`, `TICK21(TFN, 1.)`,
   `IERR = -1` (`:2423-2426`). Exit when `DSEC21(T0, TFN) > 0`, or for `J <= 1`, 4, 6 when
   `FLFRST` and it is `0` (`:2436-2445`).
5. When `J /= 1` and `TFN(1) == -1`, `TF0 = T0` and all three `0` fields take the `N` fields
   (`:2450-2464`).

## Arithmetic and parity

The reference is the `-O3` object of `w3fldsmd.F90`, built 2026-10-16 by WW3's CMake Release
build (`cmake -DSWITCH=switches/switch_lab_shrd -DCMAKE_BUILD_TYPE=Release`, target `ww3_lib`)
in the pinned `ww3` shell (gfortran 16.2), and the same file recompiled with the flags and
defines of `ww3_lib.dir/flags.make` plus `-fdump-tree-optimized-lineno`.

- (v) The recompiled object is WW3's: `objdump -d --disassemble=__w3fldsmd_MOD_w3fldh` gives
  716 identical lines for both objects.
- (v) `DERA = ATAN(1.)/45.` (`:2291`) is folded at compile time to
  `1.745329238474369049072265625e-2` (GIMPLE `DEBUG dera`); the C++
  `std::atan(1.0f) / 45.0f` folds to the same constant in the C++ GIMPLE.
- (v) `DIR = ( 270. - HD(1,J) ) * DERA` (`:2377`, `:2392`): GIMPLE `2.7e+2 - hd`, then a
  product by that constant, in `real(kind=4)`; the C++ GIMPLE is the same two operations in the
  same order. The Fortran parentheses appear as a `PAREN_EXPR` with no effect on the order.
- (v) `COS(DIR)` and `SIN(DIR)` (`:2378-2379`, `:2393-2394`) are one `__builtin_cexpif`
  (a `sincosf` call) per branch in both; `X = HA*cos`, `Y = HA*sin` are single products with
  `HA` first in both. The C++ object has two `sincosf` relocations and no other trigonometric
  call.
- (v) `NH .NE. 0.` (`:2361`) is `(real(kind=4)) nh != 0.0` in the Fortran GIMPLE and
  `(float) nh != 0.0` in the C++.
- (v) `DTTST .GT. 0.` and `DTTST.EQ.0.` (`:2439`, `:2444`) are `float` comparisons with `0.0`
  in both; `DSEC21` returns `REAL` (`w3timemd.F90:330`) and the hook returns `float`.
- (v) `TICK21(TFN, 1.)` (`:2425`) passes the `REAL` literal `1.` by reference (`&C.5035`);
  `DTIME` is `REAL, INTENT(IN)` (`w3timemd.F90:141`), and the hook takes `1.0f` by value.
- (v) No FMA: none in the Fortran object's disassembly of `W3FLDH`, none in the C++ object
  (`objdump -d | grep -c 'vfmadd\|vfmsub\|vfnmadd'` gives 0 for each).
- (v) The field loops and the list shift (`:2409-2415`) are stores and copies. gfortran
  vectorises the shift of `HA`, `HD`, `HS` (vector loads and stores of 4 and 2 `REAL`s); a
  copy of 4-byte values carries the same bits however it is grouped.
- (v) `ww3_shel` passes one scratch array `XXX` for several of `FX0`..`FSN` (for instance
  `ww3_shel.F90:2134-2136`, five of the six). Every loop body touches only element `(IX,IY)`, and
  the Fortran GIMPLE of the first-field copy (`:2459-2461`) loads and stores each element in
  source order, scalar, as the C++ body does, so the last store into `XXX` is the same. ⚠ Not
  exercised: no shim passes aliased Views yet.
- ⚠ The order of the stores across `IX` differs from the Fortran's under `parallel_for`. No
  body reads what another `IX` writes, so the result is the same; this is argued from the loop
  bodies, not observed in a run.
- ⚠ The `sincosf` that runs is whichever glibc the host links; the Fortran and the C++ call
  the same symbol, so in one executable they agree. The fields are filled with values computed
  on the host, so a device build does not change them.
- ⚠ `MX`, `MY` are not checked against `NX`, `NY`, as the Fortran does not (`:2236`); a View
  smaller than `NX` by `NY` is out of bounds, caught only by a Kokkos bounds-check build.
- ⚠ `THO`, `HA`, `HD`, `HS` are `HostSpace` Views. A shim that keeps `ww3_shel`'s arrays must
  wrap them unmanaged with the Fortran lower bound `-7` of the second index subtracted.

## Not ported

- `W3_S` (`STRACE`, `:2256-2258`, `:2286-2288`) and `W3_T` (the `WRITE (NDST,90xx)` test
  prints and their FORMATs): not compiled under the lab switch.
- The text of `FORMAT 1001` (`:2478`): the print hook receives `NDSE` and `J` and prints it.
- `DSEC21` and `TICK21`: hooks, not translated here.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and the routine has no self time in the full
profile.

## How it was regtested

Not yet, and nothing of the port was run. What was done, 2026-10-16 (v), in
`/tmp/claude-1000/fldh-scratch/` in the pinned shell
`nix develop path:picasso/labs/pratico#ww3`:

1. `cmake -S WW3 -B build -DSWITCH=switches/switch_lab_shrd -DCMAKE_BUILD_TYPE=Release` and
   `cmake --build build --target ww3_lib -j 16`.
2. `gfortran` on `w3fldsmd.F90` with the defines, includes and flags of
   `build/model/src/CMakeFiles/ww3_lib.dir/flags.make` plus `-fdump-tree-optimized-lineno`;
   `objdump -d --no-show-raw-insn --disassemble=__w3fldsmd_MOD_w3fldh` on both objects, `diff`.
3. A throwaway translation unit with an explicit instantiation of
   `ww::flds::w3fldh<Kokkos::DefaultExecutionSpace>` (OpenMP), no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -ffp-contract=off -fopenmp \
    -fdump-tree-optimized-lineno -c tu_w3fldh.cpp
```

4. `objdump -d` and `objdump -dr` on `tu_w3fldh.o` (FMA count, `sincosf` relocations), and
   `grep` over both GIMPLE dumps for the statements above.

The unit was never linked or run.
