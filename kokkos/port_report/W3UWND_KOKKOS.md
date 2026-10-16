# W3UWND_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3WAVE` wiring first.

Wind at the model time by linear interpolation of speed, direction and air-sea temperature
difference between the two input fields, then the 10 m wind relative to the surface current.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UWND`, `model/src/w3updtmd.F90:488-822` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it (`WNT1` and `RWND` branches, `w3updtmd.F90:684,746-770`) |
| Kokkos code | [`../src/ww_kokkos/updt_uwnd.hpp`](../src/ww_kokkos/updt_uwnd.hpp), a header-only device function |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2,
byte-identical to the CMake build's inside the pinned `ww3` shell), read as GIMPLE and assembly,
2026-10-16.

- (v) `SQRT(x**2+y**2)` is `(x*x)+(y*y)` then `__builtin_sqrtf` (`sqrtss`), not `hypotf`.
- (v) `ATAN2` is a libm `atan2f` call; `COS`/`SIN` of `UD` are one `sincosf` call.
- (v) `MOD(a,TPI)` is `__builtin_fmodf`, expanded inline as an x87 `fprem` loop (exact
  remainder); g++ expands `std::fmod` to the same loop and, under `-fmath-errno`, calls `fmodf`
  on a NaN result only to set `errno`, so the returned bits match for every input.
- (v) `SIGN(1.,DD)` is `copysignf(1.,DD)`; `ABS` is a mask; the `MAX` of the operand order the
  assembly emits.
- Not compiled under the lab switch and not ported: `WNT0`, `WNT2`, `OASACM`, `MGW` (so `VGX`,
  `VGY` are unused), `SMC`, `WCOR`, `STAB2`, `OMPG`, `S`, `T`. `DSEC21` (`w3timemd.F90`) is
  integer and double time arithmetic on the host: the caller computes `DT0N` and `DT0T` and
  passes the `REAL`s the Fortran stores (`w3updtmd.F90:677-678`).
- ⚠ The fresh reviewer was stopped before it reported (the session hit its usage limit); the
  translator's comparison above stands, the independent check does not.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit with
the `openmp-release` flags compiled the header with no warning and no FMA. The header is not in
a CMake target and was not compiled for CUDA yet ⚠.
