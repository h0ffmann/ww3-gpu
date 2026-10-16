# COMPUTE_REDUCTION_PSI_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The wind-sea reduction of the unresolved-obstacles source term (switch `UOST`, Mentaschi et
al.): from the angle between the wind and the group velocity and the wave age
`WA = 2 CG / U10 / COS(THDELTA)`, a factor `PSI` that is 0 for `WA <= 0.5`, `(WA - 0.5)/1.0`
up to 1.5, and 1 beyond. Its callers are `UOST_SOURCETERM_COMPUTE_LD` (`w3uostmd.F90:882`) and
`UOST_SOURCETERM_COMPUTE_SE` (`w3uostmd.F90:1024`), once per frequency.

## What was ported

| | |
|---|---|
| WW3 routine | `COMPUTE_REDUCTION_PSI`, `model/src/w3uostmd.F90:688-770` at NOAA-EMC/WW3 `03adda91`, 83 lines; compiled by nine regtest switches (`ww3_tp2.21` `switch_NO_PDLIB`, `switch_PDLIB`, `switch_PDLIB_METIS`; `ww3_ts4` `input_rg_multi`, `input_rg_shel`, `input_ug`, each with and without MPI; `git grep -lw UOST -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). Its only `#ifdef` block is `W3_S` (trace) |
| Kokkos code | [`../src/ww_kokkos/uost_compute_reduction_psi.hpp`](../src/ww_kokkos/uost_compute_reduction_psi.hpp): `ww::uost::compute_reduction_psi(u10abs, u10dir, cgabs, cgdir, dt, psi)`, `KOKKOS_INLINE_FUNCTION`, scalars by value, `psi` by reference. It reads no `UOST_SOURCETERM` state |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3uostmd.F90` in WW3's CMake Release build (gfortran 16.2,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled with the build's flags and
checked byte-identical in disassembly to the CMake object, 2026-10-16. The C++ unit was compiled
with the openmp-release flags (g++ 16.2, `-O3 -march=x86-64-v3 -ffp-contract=off`), no warning.

- (v) The optimised GIMPLE of the Fortran and of the C++ have the same operations in the same
  order: `ABS(U10DIR - CGDIR)`, the loop subtracting `6.2831855` while `> 3.1415927`, `ABS`,
  `1.5707964 - THDELTA > 1e-6` (REAL), `CGABS * 2`, `/ U10ABS`, `/ cosf(.)`, and the three-way
  branch. The disassembly has the same instruction sequence (SSE in the Fortran, VEX in the C++
  because of `-march=x86-64-v3`, same rounding).
- (v) `2*PI` and `PI/2` fold to `0x1.921fb6p+2f` and `0x1.921fb6p+0f`, both exact products of
  the REAL `PI` (`constants.F90:72`); the header builds them the same way and asserts the bits.
- (v) Both compilers call `cosf` on the angle before the second `ABS` (`:749`): `COS(ABS(x))`
  is rewritten as `COS(x)`. glibc's `cosf` is even, so the value is the same either way.
- (v) `(WA - 0.5)/(1.5 - 0.5)` is folded to `WA - 0.5` in both (division by exactly 1). The
  `WA = 9999999` branch is folded to `PSI = 1` in both.
- (v) A NaN wave age (for example `U10ABS = 0` with `CGABS = 0`) gives `PSI = 1` in both: every
  comparison is false and the `ELSE` is taken. A NaN `THDELTA` skips the loop and also gives 1.
- ⚠ `U10ABS = 0` with `CGABS > 0` gives `WA = +Inf` and `PSI = 1`; WW3 does not guard it
  (`:752`). Reproduced, not fixed.
- ⚠ `DT` is a dummy argument the Fortran never reads; it is kept in the signature.
- (v) No reduction, no MIN/MAX, no integer conversion, no libmvec call.
- ⚠ CUDA not compiled. On a GPU `cosf` is CUDA's, not glibc's (`AGENTS.md`, "Bit for bit").
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3uostmd.F90` from `build_mixa`'s flags, disassembly
  identical to the CMake object; compared its optimised GIMPLE and assembly with the C++ unit's (openmp-release
  flags, no warning) operation by operation: same `.rodata` constants, scalar `cosf` in both, `CGABS*2` as an add,
  same branch order on NaN. The two copies inlined into `UOST_SOURCETERM_COMPUTE_LD` and `_SE` have the same
  sequence with scalar `cosf` (the only libmvec calls in the object are `SETGRID`'s, `:664-665`). An infinite
  `U10DIR - CGDIR` makes the `DO WHILE` at `:746` loop forever in WW3; the C++ keeps the loop (g++'s
  `-ffinite-loops` did not remove it), so it hangs the same way ⚠ (WW3 defect, reproduced). Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
