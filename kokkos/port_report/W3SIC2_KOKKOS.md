# W3SIC2_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Sea-ice dissipation source term S = D*A of IC2: Liu / Stopa attenuation, or the turbulent boundary layer mix of PTURB and PVISC.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIC2`, `model/src/w3sic2md.F90:120-513` at h0ffmann/WW3 `96892c95`, as `switches/switch_lab_shrd` compiles it with IC2 |
| Kokkos code | [`../src/ww_kokkos/ic2_w3sic2.hpp`](../src/ww_kokkos/ic2_w3sic2.hpp): header-only `KOKKOS_INLINE_FUNCTION`, raw 0-based pointers; error returns are EXTCDE codes and the caller does the WRITE |
| C shim | none yet |

## Arithmetic

- (v) Compared operation by operation with the `-O3` GIMPLE and disassembly of the module, `-ffp-contract=off`; MIN/MAX are written as the object's minss/maxss/minpd (destination first; a tie or NaN gives the source).
- (v) `tanhf` and `log10f` are the device copies of glibc's (`libm_device.hpp`). The IS2 smooth factor is the constant 1.
- (v) Differential of `__w3sic2md_MOD_w3sic2` in the `-O3` `libww3.a` built with `IC2` (an IC2 variant of `switches/switch_lab_shrd`, SHRD, ST4, NL1) against the header, 3000 random points per seed, seeds 1, 2, 3 (2026-11-12, reviewer, throwaway harness not kept, ADR-0010): 0 bit mismatches of `S` and `D`. Branches reached per seed: no ice about 470, IC2PARS(1) > 0.5 about 625, turbulent mix (IC2PARS(2) > 0) about 1600 with the southern-ocean `FTURB` about 265, viscous only about 300. Not reached: the two EXTCDE exits (the harness never clears INFLAGS2(-7) or (-6)), NaN inputs, a tie in a MIN/MAX. Host CPU only; the device `tanhf`/`log10f` copies are not compared here ⚠.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested, postponed to the validation phase (ADR-0016). Not wired. Compiled with no warning with -Wall -Wextra -Wpedantic.

## WW3 defects found

None.
