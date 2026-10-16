# W3XYP3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

Spatial propagation of one spectral component on a regular grid with UQ: the sub-steps, the garden-sprinkler averaging, the two `W3QCK3` sweeps and the boundary input.

## What was ported

| | |
|---|---|
| WW3 routine | `W3XYP3`, `model/src/w3pro3md.F90:641-1477` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_xyp3.hpp`](../src/ww_kokkos/prop_xyp3.hpp), a header-only device function `w3xyp3`, serial, in the Fortran's structure and order |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) No libm call but `lroundf` (`NINT`, the C++ uses `roundf` with the same parity for every input), no FMA, no vectorised arithmetic; `MINVAL`/`MAXVAL` as gfortran inlines them (NaN-skip scan, in-order reduction, `HUGE` when empty); the `CGN` maximum as the tree -O3 emits; `FCG` divided by `NTLOC` twice, as the Fortran does.
- (v) The scatter stores in the Fortran's order (centre, +1..+4, -1..-4 for central points; centre, +-1..+-4 near the coast), the closure fold, the `W3QCK3` sweeps in `YFIRST` order, the store back skipping `MAPSTA <= 0`.
- ⚠ The Fortran writes one element below `VQ`'s lower bound for a sea point at row 1, column 1; the port keeps that write. The maps (`MAPX2`, `MAPAXY`, `MAPCXY`, `MAPTRN`, ...) and the transparencies are to be copied from the Fortran, not rebuilt: `W3MAP3` reads `MAPSTA` out of bounds for sea on rows 1 and `NY`.
- (v) A fresh reviewer found it bit-for-bit with the object, NaN and signed zeros included, statically.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.
