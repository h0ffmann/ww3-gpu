# W3GATH_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Gathers one spectral component `ISPEC` of the sea-point spectra `VA` into a 2-D field over the
grid, before the spatial propagation in `W3WAVE`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3GATH`, `model/src/w3wavemd.F90:3024-3329` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`SHRD` branch only, `w3wavemd.F90:3158-3166`) |
| Kokkos code | [`../src/ww_kokkos/wave_gath_scat.hpp`](../src/ww_kokkos/wave_gath_scat.hpp), a header-only device function, one `parallel_for` over `ISEA` |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3wavemd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), read as GIMPLE and assembly,
2026-10-16.

- (v) `FIELD = 0.` is one `__builtin_memset` of the whole extent; both `ISEA` loops are scalar
  and do no floating-point arithmetic, only an integer load of `MAPSF(ISEA,3)` and a `movss`
  load/store of the `REAL`, which moves NaN payloads and `-0.0` unchanged.
- (v) `parallel_for` over `ISEA` gives the same bits: each iteration is a pure copy and the
  targets do not collide; `MAPSF(:,3)` is injective in `ISEA` on a rectilinear grid
  (`w3iogrmd.F90:1073`, `w3gridmd.F90:5020-5028`). Grids built elsewhere (SMC, unstructured)
  are not checked ⚠.
- (v) A fresh reviewer found no defect: index bases, the `VA` column (`VA(NSPEC,0:NSEALM)`,
  `w3wdatmd.F90:491`), the zero fill and the injectivity argument all hold.
- The `W3_MPI` buffer management, the `W3_MPIT` test print and `W3_S` STRACE are not compiled
  under the lab switch and are not ported.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning and no FMA. Since then
`wave_support.cpp` instantiates its launcher on the default execution space in the
`serial-debug`, `openmp-release` and `cuda-release` builds (the CUDA one built, never run).
