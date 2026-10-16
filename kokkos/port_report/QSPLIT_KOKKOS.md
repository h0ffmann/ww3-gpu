# QSPLIT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Quick-sort split of a `REAL*8` array by absolute value, permuting a companion integer array the
same way, so that `|A(I)| >= |A(NCUT)|` for `I < NCUT` and `|A(I)| <= |A(NCUT)|` for `I > NCUT`.
It is the SPARSKIT helper with which `ILUT` keeps the `LFIL` largest entries of a row of the
incomplete LU factor.

## What was ported

| | |
|---|---|
| WW3 routine | `QSPLIT`, `model/src/w3profsmd.F90:3738-3800` at NOAA-EMC/WW3 `03adda91`. No `#ifdef` inside it. `w3profsmd.F90` is compiled by any `PR1`, `PR2` or `PR3` switch (`model/src/cmake/switches.json:117-125`) |
| Caller | only `ILUT` (`w3profsmd.F90:4172`, `:4203`), and nothing in WW3 calls `ILUT` (`git grep -n -i -w ilut -- model/src`: its definition, comments and the `PRIVATE` list at `:64` only; the implicit N scheme uses `ILU0`, `:1220`). `QSPLIT` is therefore never executed by WW3; it is translated for completeness |
| Kokkos code | [`../src/ww_kokkos/profs_qsplit.hpp`](../src/ww_kokkos/profs_qsplit.hpp), `ww::profs::qsplit`, one thread: the partition is sequential |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd`), rebuilt by the translator and identical to the CMake object of `build_lab`
(`objdump -d`), read against the object of a throwaway C++ unit with the `openmp-release` flags
and `-ffp-contract=off`, 2026-10-16.

- (v) No floating-point arithmetic: the only operations on `A` are `ABS` (an `andpd` with the sign
  mask in both objects) and the compare `ABS(A(J)) > ABSKEY` (`comisd` with `|A(J)|` against
  `ABSKEY`, branch `jbe` past the swap in both objects), so a NaN in `A(J)` never moves, and a NaN
  `ABSKEY` moves nothing. The rest is loads and stores of the swaps, in the same order.
- (v) `ABSKEY` is `|A(FIRST)|` read once per outer pass, before the inner loop, in both objects; the
  early return when `NCUT < 1` or `NCUT > N` is the same two integer compares.
- (v) Nothing vectorised in either object; no libm call, no reduction, no MIN/MAX, no FMA.
- (v) The C++ unit compiles warning-free with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no arithmetic defect. Rebuilt `w3profsmd.F90` from `build_lab` flags (object identical to the CMake one) and a C++ unit with the `openmp-release` flags; compared the optimized GIMPLE and the assembly: same `ABS` (`andpd` sign mask), same ordered compare (`comisd` |A(J)| against `ABSKEY`, `jbe` past the swap), same swap loads and stores in the same order, same early-return and loop-exit compares, nothing vectorised, no call. Fixed two line cites in the header (`:3759`, `:3767`) and the report calling the list at `:64` `PUBLIC` (it is `PRIVATE`).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: WW3 never calls it (see Caller).
