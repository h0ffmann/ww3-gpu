# W3SLN1_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: called only by the Kokkos `W3SRCE`, and
regtested with it at the checkpoint of that unit (ADR-0011). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3SRCE`, behind `WW_KOKKOS_SRCE` (default off).

Cavaleri and Malanotte-Rizzoli linear wind input, the seeding term `S_ln` of the lab switch (`LN1`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SLN1`, `model/src/w3sln1md.F90:56-211` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/srce_ln1.hpp`](../src/ww_kokkos/srce_ln1.hpp), a per-point device function `w3sln1_point`, called by `w3srce_team` ([`W3SRCE_KOKKOS.md`](W3SRCE_KOKKOS.md)) |
| C shim | none of its own: [`../src/fortran_iface/srce_shim.cpp`](../src/fortran_iface/srce_shim.cpp) |
| Plan | [`W3SRCE_KOKKOS_PLAN_202610.md`](../../docs/W3SRCE_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2),
read as GIMPLE and as assembly, 2026-10-15.

- (v) `COS`/`SIN(USDIR)` are one `sincosf` (`__builtin_cexpif`) on the host; `MAX(0.,x)**4` is `m=MAX; (m*m)*(m*m)`; `USTAR**4` is `(u*u)*(u*u)`; `RFR**(-4)` is `1/((r*r)*(r*r))`, then a scalar `expf`; `FF1` is `(FSPM*GRAV)/(28.*USTAR)`.
- (v) `:185`, `FFILT = MIN(MAX(FF1,FF2),2.*SIG(NK))`, in the order of the object's `maxss`/`minss`: a NaN `FF1` (`RFPM = 0`, `w3gridmd.F90:1690`, with `USTAR = 0`) keeps `FF2`, as WW3 does. The review found the first translation returned NaN there; fixed.
- ⚠ On a device `Kokkos::cos`/`sin`/`exp` are not glibc's bits.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet. It runs only inside the Kokkos `W3SRCE` ([`W3SRCE_KOKKOS.md`](W3SRCE_KOKKOS.md)) and is
regtested with it, at the checkpoint of that unit (ADR-0011). What ran, 2026-10-15 (v): the
`openmp-release` and `serial-debug` builds (no warning, ctest 10/10 each, none of them a test of
the port, ADR-0010) and the `cuda-release` compile, never run.
