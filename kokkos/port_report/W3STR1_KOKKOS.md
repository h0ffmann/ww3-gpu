# W3STR1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Triad wave-wave interactions by the Lumped Triad Approximation (switch `TR1`), a source term
`W3SRCE` calls per sea point (`w3srcemd.F90:1283`). NOAA's `switch_NWS_rwps` selects it.

## What was ported

| | |
|---|---|
| WW3 routine | `W3STR1`, `model/src/w3str1md.F90:183-543` at NOAA-EMC/WW3 `03adda91`; its only `#ifdef` is `W3_S`. Compiled by `switch_NWS_rwps`, the `ww3_tp2.17` and `ww3_tp2.19` PDLIB switches and `ww3_tp1.9`'s two switches; under `PDLIB` its result also feeds `B_JAC`/`ASPAR_JAC` (`w3srcemd.F90:1643-1655`) |
| Kokkos code | [`../src/ww_kokkos/srce_tr1.hpp`](../src/ww_kokkos/srce_tr1.hpp), `w3str1_point`; `FTE`, `FTF` (module scalars of `W3GDATMD`) passed apart |
| C shim | none yet; it copies `S` and `D` in as well as out, since the early return and the `IK = NK` row keep the caller's values, and passes `sig` pointing at `SIG(1)` inside the whole `SIG(0:NK+1)` (with `NK = 2` or 3 the Fortran reads `SIG(0)`) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch B, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3str1md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `TR0` replaced by `TR1`), byte-identical to the CMake build's, read by the
translator and a fresh reviewer, 2026-10-16.

- (v) Constants with the object's values (`2*SQRT(2.)`, `(2./15.)*GRAV`, `LOG(2.)`, `0.5*PI`, `TPI`,
  0.4, 0.2, 0.01, `1E-12`, `TINY`); `LOG(0.5)/XISLN` as `-(LOG(2)/XISLN)`, the same bits; one `logf`,
  one `tanhf`, one `sinf`, one `sqrtss`; no FMA.
- (v) The four `__powisf2` calls reproduced by `powi()`, the same square-and-multiply loop as
  libgcc 16.2's, `1/y` for negative exponents included.
- (v) `EB`, `EMEAN`, `SIGM01` summed in order: gfortran vectorises the `:405` and `:416` loops but adds
  the lanes one at a time, left to right, so there is no partial-sum tree to reproduce.
- (v) The three `MAX` sites with the object's operand roles (a NaN product gives 0); `MAXVAL`'s scan;
  the interpolation indices and weights; both Fortran quirks kept (the early return leaves `S`, `D`
  unwritten; the `URSELL` path never writes the `IK = NK` row).
- (v) The `INT()` sites use `fint` (`st4_params.hpp`), gfortran's `cvttss2si` semantics: with a
  degenerate grid (`XISLN = 0`) a plain C++ cast would be undefined behaviour and CUDA would
  saturate, where the Fortran gives `INT_MIN`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`), never run; its `logf`, `tanhf`, `sinf` are not glibc's,
  and the 294 FFMA of the SASS are taken to be CUDA's math and division, not traced.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
