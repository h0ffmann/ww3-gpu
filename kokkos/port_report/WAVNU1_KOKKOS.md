# WAVNU1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

The linear dispersion relation by table interpolation: wavenumber and group velocity from frequency and depth. Called at initialisation and at each water-level update, not per time step.

## What was ported

| | |
|---|---|
| WW3 routine | `WAVNU1`, `model/src/w3dispmd.F90:85-202`, and the `DISTAB` table it reads at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp), a header-only device function `wavnu1`, serial, in the Fortran's structure and order |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) One quotient feeds `INT` and the weight; `INT` with `cvttss2si` semantics (INT_MIN on NaN, so the deep branch); the deep branch `(SI*SI)/GRAV` and `(0.5*GRAV)/SI` as true divisions with the folded constants; inline correctly rounded `sqrtf`.
- ⚠ The table (`EWN1`, `ECG1` of 0:121, `DSIE`, `N1MAX`) must be copied from the Fortran module after `W3IOGR`; nothing fills a `DispTable` yet.
- (v) A fresh reviewer found it bit-identical for every input. g++ keeps a discarded `sqrtf` call for a negative depth (math-errno), which sets `errno` and changes no output.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.
