# F_ZHAO_V1_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its `JUDGE /= 0` branch calls `FUNC1_ZHAO`, which is not
ported: the header takes it as a callable argument.

A helper of the IC3 sea-ice source term, "V1" path: the function whose complex root
`CMPLX_ROOT_MULLER_V1` finds. With `JUDGE == 0` it is `FUNC0_ZHAO`, the open-water dispersion
residual; otherwise `FUNC1_ZHAO`, the viscoelastic-ice dispersion relation of Zhao and Shen
(`w3sic3md.F90:1363-1367`). Its only caller is `CMPLX_ROOT_MULLER_V1` (`:1212-1214`, `:1248`),
called by `WN_CMPLX_V1` with `JUDGE = 0` (`:953`, `:972`) and `JUDGE = 1` (`:958`, `:965`,
`:977`, `:1090`, `:1097`); the chain is reached from `W3IC3WNCG_V1`, called by `W3SIC3` (`:549`)
and `W3WAVE` (`w3wavemd.F90:1063`) when `IC3CHENG` is false.

## What was ported

| | |
|---|---|
| WW3 routine | `F_ZHAO_V1`, `model/src/w3sic3md.F90:1279-1369` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised only by regtest namelists with `IC3CHENG = F`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_f_zhao_v1.hpp`](../src/ww_kokkos/ic3_f_zhao_v1.hpp), `template <class Func1> ww::ic3::f_zhao_v1(const Kokkos::complex<double>& x, int judge, double sigma, double es, double nu, double dice, double hice, double depth, const Func1& func1) -> Kokkos::complex<double>`; calls `ww::ic3::func0_zhao` ([`FUNC0_ZHAO_KOKKOS.md`](FUNC0_ZHAO_KOKKOS.md)) and `func1(x, sigma, es, nu, dice, hice, depth)` in place of `FUNC1_ZHAO` |
| Not ported | `FUNC1_ZHAO` (`:1465-1608`) and its `BSDET` (`:1611-`): outside this batch. The callable argument is the only structural change, and goes when `FUNC1_ZHAO` has a header |
| C shim | none yet; its caller `CMPLX_ROOT_MULLER_V1` is not ported |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) read against that of a
throwaway C++ unit (`f_zhao_v1` with an external function as `func1`) compiled with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16. The C++ unit compiles warning-free.

- (v) The object has a standalone `__w3sic3md_MOD_f_zhao_v1`: a test `JUDGE == 0`, then
  `FUNC0_ZHAO` inlined, else a tail call of `func1_zhao` with the seven arguments unchanged. The
  C++ GIMPLE has the same test, the same inlined body and a tail call of the callable with the
  same arguments.
- (v) The inlined `FUNC0_ZHAO` body is operation for operation the one compared in
  [`FUNC0_ZHAO_KOKKOS.md`](FUNC0_ZHAO_KOKKOS.md): the same two `cexp` calls in the same order on
  the same arguments, Smith's quotient with the same two branches, the products with `0.0`
  kept, `SIGMA**2.` as `SIGMA*SIGMA`, `DBLE(GRAV)` as `9.805999755859375`. g++ SLP-vectorises
  some pairs and commutes some additions as described there; each is exact in IEEE arithmetic,
  and only which NaN payload survives when both operands are NaN could differ ⚠ (not checked).
- (v) No arithmetic of its own: no reduction, no MIN/MAX, no `INT`/`NINT`, no libmvec call.
- ⚠ The `JUDGE /= 0` result is whatever `func1` returns; its parity is that of a future
  `FUNC1_ZHAO` port. On the device, `func0_zhao_cexp` is not glibc's `cexp` (see
  `FUNC0_ZHAO_KOKKOS.md`). CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. `w3sic3md.F90` recompiled from `build_mixa`'s
  `flags.make` and found identical to the CMake object; the assembly of
  `__w3sic3md_MOD_f_zhao_v1` read against a throwaway C++ unit instruction by instruction: the
  `JUDGE == 0` test, the `REAL(WN*DEPTH) <= 4` branch (NaN to the `ELSE` path in both), the two
  `cexp` calls in the same order, both Smith branches, the `TH*WN*GRAV` and `SIGMA**2 - ...`
  operands, the constants `4.0` and `DBLE(GRAV)` in `.rodata`, and the tail call of
  `FUNC1_ZHAO` with the arguments unchanged. g++ packs the `GRAV` products into `vaddsubpd`
  lanes; each lane is the same IEEE operation. No #ifdef in `:1279-1369`; the switch count and
  every line cited were checked at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
