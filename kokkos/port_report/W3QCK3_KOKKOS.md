# W3QCK3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

UQ advection in one spatial direction with obstruction (transparency): the two sweeps of `W3XYP3`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3QCK3`, `model/src/w3uqckmd.F90:925-1336` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_qck3.hpp`](../src/ww_kokkos/prop_qck3.hpp), a header-only device function `w3qck3`, serial, in the Fortran's structure and order |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) No libm or libmvec call, no FMA, no reduction; the vectorised central loop is per element.
- (v) Every `MIN`/`MAX` in the object's `minss`/`maxss` operand order in all five code paths; the final `MAX(0.,.)` turns -0.0 and NaN into +0.0; `TRANS(IXY,-1:1)` with column 0 for a NaN flux (`comiss`).
- (v) The closure copies (three ghost columns of `Q`, one of `CFLL`) in source order and persisting into the caller's arrays. The `TRANS` index is computed in 64-bit, as the Fortran does.
- (v) A fresh reviewer found it bit-identical for every non-NaN input, signed zeros included; a NaN can change only a payload that the final `MAX` removes before `Q` is stored.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.
