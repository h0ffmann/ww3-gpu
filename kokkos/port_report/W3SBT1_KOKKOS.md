# W3SBT1_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: called only by the Kokkos `W3SRCE`, and
regtested with it at the checkpoint of that unit (ADR-0011). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3SRCE`, behind `WW_KOKKOS_SRCE` (default off).

JONSWAP bottom friction (`BT1`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SBT1`, `model/src/w3sbt1md.F90:88-285` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/srce_bt1.hpp`](../src/ww_kokkos/srce_bt1.hpp), a per-point device function `w3sbt1_point`, called by `w3srce_team` ([`W3SRCE_KOKKOS.md`](W3SRCE_KOKKOS.md)) |
| C shim | none of its own: [`../src/fortran_iface/srce_shim.cpp`](../src/fortran_iface/srce_shim.cpp) |
| Plan | [`W3SRCE_KOKKOS_PLAN_202610.md`](../../docs/W3SRCE_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2),
read as GIMPLE and as assembly, 2026-10-15.

- (v) No libm call, no power. `CBETA = FACTOR*MAX(CG*WN/SIG-0.5, 0.)` in the object's `maxss` order; `S = D*A` element by element.
- (v) `NSCUT = (IK-1)*NTH` after the `EXIT` at `WN*DEPTH > 6.`, with `IK = NK+1` when the loop completes; `MAPWN` values are 1-based.
- (v) A fresh reviewer found it bit for bit with the build for every input it could construct, statically.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet. It runs only inside the Kokkos `W3SRCE` ([`W3SRCE_KOKKOS.md`](W3SRCE_KOKKOS.md)) and is
regtested with it, at the checkpoint of that unit (ADR-0011). What ran, 2026-10-15 (v): the
`openmp-release` and `serial-debug` builds (no warning, ctest 10/10 each, none of them a test of
the port, ADR-0010) and the `cuda-release` compile, never run.
