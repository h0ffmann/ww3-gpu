# W3SDB1_KOKKOS

**Status:** ported, phase 1 (translated). Not regtested: called only by the Kokkos `W3SRCE`, and
regtested with it at the checkpoint of that unit (ADR-0011). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3SRCE`, behind `WW_KOKKOS_SRCE` (default off).

Battjes and Janssen (1978) depth-induced breaking (`DB1`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SDB1`, `model/src/w3sdb1md.F90:96-382` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/srce_db1.hpp`](../src/ww_kokkos/srce_db1.hpp), a per-point device function `w3sdb1_point`, called by `w3srce_team` ([`W3SRCE_KOKKOS.md`](W3SRCE_KOKKOS.md)) |
| C shim | none of its own: [`../src/fortran_iface/srce_shim.cpp`](../src/fortran_iface/srce_shim.cpp) |
| Plan | [`W3SRCE_KOKKOS_PLAN_202610.md`](../../docs/W3SRCE_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

The reference is the `-O3` object of the Fortran file (WW3's CMake Release flags, gfortran 16.2),
read as GIMPLE and as assembly, 2026-10-15.

- (v) `REAL*8` exactly where the Fortran is: `EB` summed in double in `ITH` order, `TANH`, `EXP` and `SQRT` in double, `THR = TINY(1.0)` compared as a double, `D = -(float)CBJ` (so `CBJ = 0` gives `-0.0`).
- (v) The early return at `EMEAN < TINY(1.d0)` leaves `S`, `D` and `LBREAK` untouched: `W3SRCE` passes the buffers of the previous sub-step, as the Fortran does.
- (v) At `B == 1` exactly (`BB = 1` or `1+2**-52`) the `-O3` control flow goes to `CBJ = 0` without the source's `ABS(BB-QB)` test; the port follows the build (an extra `B /= 1` term). The owner has not yet confirmed this departure from the source text ⚠.
- `IWB = 2` is dead (`IWB = 1` is a literal) and not ported; `ETOT` and `FMEAN2` are dead in the build and computed here without effect.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet. It runs only inside the Kokkos `W3SRCE` ([`W3SRCE_KOKKOS.md`](W3SRCE_KOKKOS.md)) and is
regtested with it, at the checkpoint of that unit (ADR-0011). What ran, 2026-10-15 (v): the
`openmp-release` and `serial-debug` builds (no warning, ctest 10/10 each, none of them a test of
the port, ADR-0010) and the `cuda-release` compile, never run.
