# W3QCK1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim and
the `W3WAVE` wiring first.

UQ (ULTIMATE QUICKEST) advection in one direction with closure: the direction pass of `W3KTP3`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3QCK1`, `model/src/w3uqckmd.F90:117-480` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`PR3`, `UQ`) |
| Kokkos code | [`../src/ww_kokkos/prop_qck1.hpp`](../src/ww_kokkos/prop_qck1.hpp), a header-only device function `w3qck1`, serial, in the Fortran's structure and order |
| C shim | none yet |
| Plan | [`PROPAGATION_KOKKOS_PLAN_202610.md`](../../docs/PROPAGATION_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2,
`-march=x86-64`, byte-identical to the CMake build's), read as GIMPLE and as assembly by a parser,
the translator and a fresh reviewer, 2026-10-16.

- (v) No libm or libmvec call, no FMA, no reduction. The central loop is vectorised four points at a time in the Fortran; every operation there is per element, so the scalar C++ loop gives the same bits.
- (v) Every `MIN`/`MAX` in the operand order of the object's `minss`/`maxss`, in the vector body and every scalar copy; the `MIN(0.,SIGN(1.1,CFL))` swap in the scalar copies cannot change bits, since `SIGN(1.1,.)` is +-1.1 for every input. `SIGN` is copysign (`CFL = -0.0` picks the other upwind cell); `INT` truncates; `2*Q` is `Q+Q` in both.
- (v) A fresh reviewer found it bit-identical to the object for every input, NaN and signed zeros included, statically.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): the `serial-debug`, `openmp-release`
and `cuda-release` presets compiled with the header included (`prop.cpp`), with no warning from it.
Nothing calls the function yet, so the compilers parse and type-check it without generating its
code; code generation, CUDA's included, comes with the shim ⚠.
