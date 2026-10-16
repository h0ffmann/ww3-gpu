# W3QCK2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

UQ advection on a variable grid: the wavenumber pass of `W3KTP3`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3QCK2`, `model/src/w3uqckmd.F90:517-891` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_qck2.hpp`](../src/ww_kokkos/prop_qck2.hpp), a header-only device function `w3qck2`, serial, in the Fortran's structure and order |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) As `W3QCK1`, with the grid-spacing terms: `CFL = (DT*VEL)/DX2`, the curvature factor `((DX2*DX2)/DX1)*(1-CFL*CFL)` then `/6`, and the propagation `Q + (DT/DX1)*(FLA(-INC)-FLA)`, quotient first.
- (v) The boundary loops run in the lab build (`NB0 = NSPEC-NTH`, `NB1 = NSPEC`, `NB2 = NSPEC+NTH` from `W3KTP3`); the lab caller passes `CFLK` with `MY*MX` reals and `CLOSE` false, so column `MX+1` is never touched.
- (v) A fresh reviewer found it bit-identical to the object for every input, statically.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.
