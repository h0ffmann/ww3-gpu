# W3KTP3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

Propagation in spectral space for one sea point: refraction in direction and the wavenumber shift from depth and current gradients, through `W3QCK1` and `W3QCK2`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3KTP3`, `model/src/w3pro3md.F90:1519-1961` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_ktp3.hpp`](../src/ww_kokkos/prop_ktp3.hpp), a header-only device function `w3ktp3`, serial, in the Fortran's structure and order |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) Every floating-point statement in the object's order; no libm call, no FMA; the dead `FDDMAX` loop is not translated (-O3 removed it); the two CFL caps take their `MIN` operands in opposite orders, as the object does.
- (v) The `CFLKMAX` maximum is vectorised by -O3 (four lanes from the running value, a pairwise fold, a two-lane block, a scalar tail); the port follows the same grouping, so a NaN survives where the Fortran's does. The grouping was read from the object by the reviewer and corrected by a second one: after the four lanes, the two-lane block starts from the two partial folds, not from their maximum.
- ⚠ With both `WN(IK)` and `FKC(ITH)` NaN, the Fortran's multiply puts `FKC` first in the four-lane loop and `WN` first elsewhere, which decides which NaN payload propagates into `CFLK` and `CFLKMAX`; the C++ operand order there was not compared.
- (v) The k and theta passes in `MOD(ITSTEP,2)` order with the exact actual arguments of `W3QCK2` and `W3QCK1`; zero fills only where the Fortran zeroes.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.
