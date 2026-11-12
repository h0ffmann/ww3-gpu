# W3IC3WNCG_V1_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Ice wavenumber, attenuation and group velocity at every frequency from the V1 dispersion relation (Wang and Shen 2010).

## What was ported

| | |
|---|---|
| WW3 routine | `W3IC3WNCG_V1`, `model/src/w3sic3md.F90:657-836` at h0ffmann/WW3 `96892c95`, as `switches/switch_lab_shrd` compiles it with IC3 |
| Kokkos code | [`../src/ww_kokkos/ic3_w3ic3wncg_v1.hpp`](../src/ww_kokkos/ic3_w3ic3wncg_v1.hpp): header-only `KOKKOS_INLINE_FUNCTION`, raw 0-based pointers; error returns are EXTCDE codes and the caller does the WRITE |
| C shim | none yet |

## Arithmetic

- (v) Compared operation by operation with the `-O3` GIMPLE and disassembly of the module, `-ffp-contract=off`; MIN/MAX are written as the object's minss/maxss/minpd (destination first; a tie or NaN gives the source).
- (v) Differential test (2026-11-12, reviewer, throwaway harness, not kept, ADR-0010): the Fortran object of `libww3.a` (`-O3`, IC3 variant of `switches/switch_lab_shrd`) against the repository headers as they stand, seeds 1 and 7, 1500 trials each: 0 mismatches in `wn_precalc_cheng` (375 calls), `w3ic3wncg_v1`, `w3ic3wncg_cheng` (750) and `w3sic3`. The same harness on the batch-3 headers before the reviewer's three fixes gives 1339 and 1332 mismatches of `wn_precalc_cheng`, 95 and 101 of `w3sic3`, 151 and 158 of `w3ic3wncg_cheng` (the fixes are in `ic3_cmplx_root_muller_cheng.hpp` and `ic3_w3ic3wncg_cheng.hpp`). Not reached: EXTCDE exits, NaN inputs, the thick-ice branch with IC3PARS(2) <= 0 (Fortran undefined).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested, postponed to the validation phase (ADR-0016). Not wired. Differential test: the Fortran object of libww3.a against the header on random inputs (seeds 1, 2, 5), a throwaway harness that is not kept (ADR-0010).

## WW3 defects found

None.
