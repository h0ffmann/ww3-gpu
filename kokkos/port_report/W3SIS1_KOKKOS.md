# W3SIS1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Spectral reflection of wave energy by sea ice (switch `IS1`), a source term `W3SRCE` calls per sea
point (`w3srcemd.F90:1388`, `:2172`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIS1`, `model/src/w3sis1md.F90:74-227` at NOAA-EMC/WW3 `03adda91`; compiled by the `ww3_ts1/input_icesct_IC0..IC4` regtest switches (`git grep -lw IS1 -- '*switch*'` in `WW3-noaa`), by no NOAA operational switch. Its only `#ifdef` blocks are `W3_S`/`W3_T` output |
| Kokkos code | [`../src/ww_kokkos/srce_is1.hpp`](../src/ww_kokkos/srce_is1.hpp), `w3sis1_point` |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch B, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3sis1md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `IS0` replaced by `IS1`), read by the translator and a fresh reviewer,
2026-10-16.

- (v) `ALPHA = MAX(0., IS1C1*ICE+IS1C2)` (`:185`) is `maxss %xmm0,%xmm4` with the sum in `xmm4` and 0
  in `xmm0`, so `(v > 0) ? v : 0` and a NaN gives 0, as the header writes it (read from the object
  2026-10-16).
- (v) `S(IS) - A*ALPHA` and `S(IS2) + (A*ALPHA)/REAL(NTH-1)` with one shared product, true
  divisions, `S/DTMIN` element-wise; no FMA, no reciprocal, no libm.
- (v) The `ALPHA > 0 .AND. ICE > 0` and `A(IS) >= 0` branches both ways.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`; the FFMA in the SASS are the correctly rounded division),
  never run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
