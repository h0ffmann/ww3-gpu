# W3ADDIG_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Second-order (difference-interaction) spectrum for the infragravity band: for each of `NKIG`
infragravity frequencies and each pair of directions, the bound-wave coupling coefficient
`Df1f2theta` (Okihiro et al. 1992, ⚠ reference not fetched while writing) times the primary spectra at `f` and
`f+df` is accumulated, by the rectangle rule, into the direction bin of the difference wave vector.

## What was ported

| | |
|---|---|
| WW3 routine | `W3ADDIG`, `model/src/w3gig1md.F90:146-375` at h0ffmann/WW3 `96892c95`, byte-identical to NOAA-EMC/WW3 `03adda91` in this file (v) |
| Reference switch | `ww3_tig1.1/input/switch` |
| Kokkos code | [`../src/ww_kokkos/gig1_w3addig.hpp`](../src/ww_kokkos/gig1_w3addig.hpp), `ww::gig1::w3addig`, one spectrum; the ALLOCATABLE arrays are the caller's (`AddigWork`) |
| Callees | `WAVNU1` (`prop_wavnu1.hpp`), `Df1f2theta` (`gig1_df1f2theta.hpp`) |

## Arithmetic

- (v) `WFR1 = DBLE(DSIP)*DTH` is rounded to `REAL`: the `REAL` product. `ESPEC` from `A(k,theta)` is
  `((E*SIG)*TPI)/CG`; `Eadd = ((DD*WFR1)*ESPEC1)*ESPEC2` (GIMPLE).
- (v) `DKX`, `DKY` use `DOUBLE PRECISION` `cos` and `sin` of `DBLE(DTH*ITH)`, which the object
  compiles to one `__builtin_cexpi`; the difference is rounded to `REAL`. `ATAN2` is a scalar
  `atan2f`.
- (v) `MINLOC(...,1)` is gfortran's inline form: the first non-NaN element starts the scan, later
  ones win by strict `<`, all NaN gives 1 (`minloc1`).
- (v) No `vfmadd`; compiles without warning under the repo flags and under ASan/UBSan.
- (v) A throwaway differential program (not kept, ADR-0010) called `__w3gig1md_MOD_w3addig` and the
  C++ on the same inputs, `IACTION` 0 and 1, 12 runs of `NK`/`NTH`/`NKIG` (25/24/4 and 8, 3/8/2,
  4/6/3, 5/7/4, 7/9/5, 12/16/6, 31/25/10, 6/5/6, 33/36/12, 10/12/1 and 10), depth from 0.5 to
  2000 m, spectra with zeros and `WN` perturbed: 0 mismatches over about 159 000 spectral values.
  ⚠ No command in the repository reproduces these counts.
- ⚠ On a device `cos`, `sin`, `tanhf` and `atan2f` are CUDA's, not glibc's; `exp`/`tanh` inside
  `WAVNU1`/`Df1f2theta` as in their reports.

## Benchmarks

None.

## How it was regtested

Not regtested: postponed (ADR-0016). Not wired.

## WW3 defects found

- ⚠ `DD` is `NKIG*NK*NTH*NTH` reals per call, allocated and freed on each call.
- ⚠ The first loop runs `IFR=1..NK`, the accumulation loop `IFR=1..NK-1`; the last `DD` is unused.
