# TRIG_COMPUTE_DIFF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Gradient (`UGRAD`, `VGRAD`) of the linear basis function of vertex `I1` of triangle `IE` of an
unstructured PDLIB grid, used by `TRIG_WAVE_SETUP_COMPUTE_SYSTEM` (`w3wavset.F90:849`, `:855`) to
assemble the wave setup system, which `TRIG_WAVE_SETUP_COMPUTATION` calls (`:1712`).

## What was ported

| | |
|---|---|
| WW3 routine | `TRIG_COMPUTE_DIFF`, `model/src/w3wavset.F90:598-685` at NOAA-EMC/WW3 `03adda91`; compiled only with switch `SETUP` and `PDLIB`. No regtest or NOAA operational switch sets `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` block is `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/wavset_trig_compute_diff.hpp`](../src/ww_kokkos/wavset_trig_compute_diff.hpp), `ww::wavset::trig_compute_diff`; `IE`, `I1` keep their Fortran values, the module arrays `INE`, `x`, `y`, `PDLIB_TRIA` are passed as 0-based pointers |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` (WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`, MPI), recompiled and found identical to the
CMake object, read against a throwaway C++ unit with the openmp-release flags, 2026-10-16.

- (v) `h = 2.0*PDLIB_TRIA(IE)`: `addss h,h` in both objects, exact.
- (v) `UGRAD = -(y(IP3)-y(IP2))/h`: `subss`, `divss`, then a sign flip (`xorps`) of the quotient in
  both; `VGRAD = (x(IP3)-x(IP2))/h`: `subss`, `divss` in both. Same operands, same order.
- (v) `POS_TRICK` is the same six-integer table on the stack in both; `IP1 = INE(I1,IE)` (`:679`) is
  never used and neither object loads it.
- (v) No FMA, no libm or libmvec call, no reduction, no MIN/MAX.
- (v) Fresh review 2026-10-16: no arithmetic defect. The module recompiled from `ref3/build_setup`'s `flags.make` is identical to the CMake object; its `-O3` assembly read against the C++ unit's: the `POS_TRICK` table (16-byte constant plus `0x200000001`), `INE` column-major with leading dimension 3 (`allocate(INE(3, ne))`, `PDLIB/yowpdlibmain.F90:1261`), `x`, `y`, `PDLIB_TRIA` base 1 (`PDLIB/yowpdlibmain.F90:1329`, `:1333`, `:1369`), `addss h,h`, `y(IP3)-y(IP2)` then `divss` then `xorps`, and `x(IP3)-x(IP2)` then `divss`, in the same order in both. Fixed in this report: the caller at `:1712` is `TRIG_WAVE_SETUP_COMPUTATION`, not `TRIG_WAVE_SETUP_COMPUTE`.
- ⚠ `I1` outside 1..3 reads outside `POS_TRICK`; the callers pass 1..3 only.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
