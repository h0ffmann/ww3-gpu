# COMPUTE_ACTIVE_NODE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Marks which nodes of an unstructured (PDLIB) grid take part in the wave setup solve: `ACTIVE(IP) = 1`
where the depth `DWNX(IP)` is at least `CRIT_DEP_STP`, else 0. Called only by
`TRIG_WAVE_SETUP_COMPUTATION` (`w3wavset.F90:1707`), itself called by `WAVE_SETUP_COMPUTATION`
(`:3157`) from `W3WAVE` (`w3wavemd.F90:1124`) under switch `SETUP`.

## What was ported

| | |
|---|---|
| WW3 routine | `COMPUTE_ACTIVE_NODE`, `model/src/w3wavset.F90:1493-1588` at NOAA-EMC/WW3 `03adda91`. Module `W3WAVSET` is compiled only with switch `SETUP`; no regtest switch and no NOAA operational switch names `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`) |
| Kokkos code | [`../src/ww_kokkos/wavset_compute_active_node.hpp`](../src/ww_kokkos/wavset_compute_active_node.hpp), `ww::wavset::compute_active_node(dwnx, active, npa, crit_dep_stp)`; arrays 0-based (Fortran 1-based, size `NPA`); the module values `NPA` (yowNodepool) and `CRIT_DEP_STP` (W3GDATMD, `REAL(8)`) are arguments |
| Variants | `W3_DEBUGSTP` (`:1562-1570`, `:1577-1579`, `:1582-1587`) only counts active nodes and writes them to unit `740+IAPROC`: not translated. `W3_S` is tracing only. No MPI call in the routine |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` recompiled with the flags of a WW3 CMake Release build
(gfortran 16.2, `regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP`), checked identical to that
build's `w3wavset.F90.o`, 2026-10-16; C++ unit with the openmp-release flags, warning-free.

- (v) `DWNX` is `REAL(rkind)` with `rkind = 4` (`PDLIB/yowdatapool.F90:48`), `CRIT_DEP_STP` is
  `REAL(8)`: both objects widen `DWNX(IP)` to double (`cvtps2pd`/`cvtss2sd`) and test
  `CRIT_DEP_STP <= DWNX(IP)` ordered (`cmplepd` then `and 1` in the vector body, `comisd` + `setae`
  in the tail). A NaN depth gives 0 in both.
- (v) Both objects vectorise the loop (Fortran with 16-byte vectors, C++ with 32-byte and a 16-byte
  epilogue); the output is an integer per node with no reduction, so the width does not change it.
- (v) No libm or libmvec call, no FMA, no MIN/MAX, no reduction.
- (v) Fresh review 2026-10-16: no defect. The module recompiled from `ref3/build_setup`'s
  `flags.make` is identical to the CMake object; its assembly read against the C++ unit's: the
  ordered `CRIT_DEP_STP <= DWNX(IP)` in double (`cmplepd`, and `comisd` + `setae` in both tails),
  1 or 0 stored per node, nothing written when `NPA <= 0`. `W3_DEBUGSTP` is set by no switch file
  (`git grep -lw DEBUGSTP -- regtests model/bin` finds none).
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `SETUP`, so no regtest of the WW3 matrix
can exercise it as the switches stand.
