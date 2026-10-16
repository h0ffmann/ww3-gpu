# PDLIB_EXPLICIT_BLOCK_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The explicit scheme of WW3's unstructured grids for all spectral components at once (the block
form, `EXPTOTAL = T`): per frequency, wavenumbers by `WAVNU3`, element velocities and CFL
sub-stepping, then N-scheme updates with the boundary applied, under `PDLIB`.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_EXPLICIT_BLOCK`, `model/src/w3profsmd_pdlib.F90:6408-6685` at NOAA-EMC/WW3 `03adda91`, with and without `W3_REF1` |
| Kokkos code | [`../src/ww_kokkos/pdlib_explicit_block.hpp`](../src/ww_kokkos/pdlib_explicit_block.hpp), `pdlib_explicit_block<kRef1>`; calls [`ww::disp::wavnu3`](WAVNU3_KOKKOS.md) out of line, as the Fortran does (`:6513`) |
| C shim | none yet. `allreduce_min` (`:6599`, only with `LCALC`) and `exchange_u` (`:6626`, and after each sub-step, `:6670`) are host hooks. `ExplWork::cgsig` must live from call to call, per grid under `ww3_multi` |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## What WW3 does here, at the pin

Read by the translator and confirmed by a fresh high-effort review, 2026-10-16 (v):

- `CGSIG(NPA)` (`:6487`) is an automatic array, put on the heap by gfortran `-O3` at every call. Only
  `WAVNU3` writes it (`:6513`), inside `IF (LCALC)`; it is read at `:6623` and `:6679` outside it. So
  with `LCALC` false the Fortran converts with whatever bytes the allocator returns. `DTMAXGL`
  (`:6489`) is assigned only at `:6600` (inside `LCALC`) and read at `:6651` under `FLBPI`.
- `LCALC` is `UGDTUPDATE` (`w3wavemd.F90:1925`): reset at each `W3WAVE` call (`:767`), set on every
  step when currents are on (`:1188-1210`), otherwise on the first step of each call
  (`:973,1219-1220`) and on water-level updates (`:1436`). Without currents, steps 2..NT of every
  call run with `LCALC` false.
- `DTMAXOUT` is assigned only at `:6594`; a rank with no qualifying node sends an undefined value into
  the `MPI_MIN`. With `REF1`, `UOLD` (`:6493`) is read at `:6643` and never stored.
- `DTSI(NPA)` (`:122`) has no frequency index: with `LCALC` false every frequency takes the `DTSI` of
  the last frequency of the last `LCALC` call with its own `ITER(IK)`. Defined, and reproduced.
- The only regtest with `EXPTOTAL = T` is `ww3_ufs1.1/input_unstr` grid b (`matrix.base:2208`); its
  run has currents, so `LCALC` is true on every step, `REF0`, and no boundary input. The `LCALC`
  false, `FLBPI` and `REF1` paths are exercised by no regtest.

## Arithmetic

Reference: the `-O3` objects of `w3profsmd_pdlib.F90` for `switch_PDLIB` and with `REF1`
(gfortran 16.2, WW3's Release flags), byte-identical to the CMake builds', read by the translator
and the reviewer.

- (v) Hook order: one `MPI_ALLREDUCE`, two exchange sites, `WAVNU3` out of line, as in the object.
- (v) `KKSUM` and `ST` gathered per node in increasing `IE`, no atomics; the `DTMAX` minimum on one
  thread in node order, with the object's operand roles: `minps` with `DTMAX` as destination in the
  4- and 2-wide paths, `minss` with `DTMAXEXP` as destination in the scalar tail (odd `NTH`).
- (v) `KELEM`, `FLALL`, `NM`, `DTSI` in double and every mixed site at `:6544-6641`; `THR` `0x2b8cbccc`;
  the `+ 0` kept; the boundary pass parallel over directions, serial over boundary points.
- ⚠ Not bit-identical where the Fortran is undefined: with `LCALC` false the C++ uses the `CGSIG`
  and `DTMAXGL` it kept from the last `LCALC` call, `REF1`'s `UOLD` reads a zeroed array, and
  `DTMAXOUT` starts at 0 (so a rank with no qualifying node drives `DTMAXGL` to 0). No choice could
  reproduce bytes that depend on the Fortran's own heap and stack history.
- ⚠ CUDA compiled (`--fmad=false`; no `fma.rn` outside `WAVNU3`'s libdevice code), never run;
  `max.f64` on −0 at `:6641` not checked.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
