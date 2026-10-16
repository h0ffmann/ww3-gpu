# W3FSD_DAVE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mean floe diameter of a floe size distribution P(D) = D^(-1-γ) truncated to
D_min <= D <= D_max', with γ = 2 + ln(FRAGILITY)/ln 2 and D_max' = max(1.01 D_min, D_max):
D_ave = γ/(γ-1) (D_max'^(1-γ) - D_min^(1-γ)) / (D_max'^(-γ) - D_min^(-γ)), and D_ave = D_max'
when D_min = 0.

## What was ported

| | |
|---|---|
| WW3 routine | `W3FSD_DAVE`, `model/src/w3sis2md.F90:1317-1405` at NOAA-EMC/WW3 `03adda91`, `PRIVATE` to `W3SIS2MD`. No `#ifdef` in the routine |
| Callers | `INSIS2` (`:582`), filling `ICEDAVETAB(1:NICED)` with `ICEDMAX = REAL(I)`; `W3SIS2` (`:850`), per wavenumber when `DMAX <= NICED`, `IS2PARS(9) <= 0` and `IS2PARS(14)*(TPI/WN_R(IK)) < DMAX` |
| Switches that compile it | `IS2`: `ww3_tic1.1` (`input_IS2/switch`, `input_IC2_ifr/switch_IS2`, `switch_IC2IS2`), `ww3_tic1.4`, `ww3_tic2.3`, `ww3_ts4` (rg_multi, rg_shel, ug, each with `_MPI`), `mww3_test_03`, `mww3_test_08`, `ww3_tp2.14` (`switch_OASICM`), and `model/bin/switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASICM` (`git grep -lw IS2 -- '*switch*'`). No `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/is2_w3fsd_dave.hpp`](../src/ww_kokkos/is2_w3fsd_dave.hpp), `ww::is2::w3fsd_dave<kIcedmaxFirst>(icedmin, icedmax, fragility)` returning `float` |
| C shim | none yet |
| Plan | issue #42, batch `goal/42-port-batch-3-shims` |

## Arithmetic

Reference: the `-O3` object of `w3sis2md.F90` (WW3's CMake Release flags, gfortran 16.2, the lab
switch with `IC3 IS2 BT8 TIDE UOST FLD2 IG1`, build `build_mixa`), recompiled and checked
identical to the CMake object, compared with a throwaway C++ unit under the `openmp-release`
flags (compiled warning-free), GIMPLE and disassembly, 2026-10-16.

- (v) The object keeps a standalone `__w3sis2md_MOD_w3fsd_dave` and inlines the routine into
  `INSIS2` (`:582`) and `W3SIS2` (`:850`). All three copies have the same operations; they
  differ only in the operand order of `MAX` at `:1390` (below).
- (v) `MAX(ICEDMIN*1.01, ICEDMAX)`: the standalone function and the `W3SIS2` copy emit
  `maxss` with `ICEDMIN*1.01` as first source (unordered or equal returns `ICEDMAX`); the `INSIS2`
  copy has `REAL(I)` as first source (unordered or equal returns `ICEDMIN*1.01`). The template
  flag `kIcedmaxFirst` (false: standalone and `W3SIS2`; true: `INSIS2`) reproduces each; the
  C++ object's `vmaxss` operands were read for both instances. The two orders differ when
  either operand is NaN or on a tie of +0 and -0; at the `INSIS2` site `ICEDMAX = REAL(I) >= 1`,
  so there only a NaN `ICEDMIN` tells them apart, and at the `W3SIS2` site the guard
  `DMAX > ICEDMIN` (`:849`) excludes both cases.
- (v) `LOG(2.)` is folded to the `REAL` nearest ln 2, `6.93147182464599609375e-1`; `1.01` is
  `1.0099999904632568359375`. Both written as hex literals with a `static_assert`.
- (v) `GAM = LOG(FRAGILITY)/LOG(2.) + 2` is a `logf` call, a division and an addition, computed
  only on the `ICEDMIN /= 0` branch in both objects. The test at `:1392` is on `ICEDMIN`, and
  `ICEDMAXL` is computed before it in both.
- (v) `-GAM+1` is emitted by gfortran as `1 - GAM`, `-GAM` as a sign flip; the four powers are
  `powf` calls in the same order with the same arguments, no libmvec variant. Then
  `num = powf(M,1-γ) - powf(Dmin,1-γ)`, `den = powf(M,-γ) - powf(Dmin,-γ)`, `num/den`,
  `GAM/(GAM-1)`, and the product with the quotient as first source: the same operations, the
  same operand order, no FMA, in both objects.
- ⚠ `ICEDMIN < 0` or `FRAGILITY <= 0` give NaN through `powf`/`logf` in both; `GAM = 1`
  (`FRAGILITY = 0.5`) divides by zero in `GAM/(GAM-1)`. WW3 does not guard either; the
  translation reproduces it.
- ⚠ Not compiled for CUDA. On a device `logf` and `powf` are CUDA's, not glibc's.
- (v) Fresh review 2026-10-16: no arithmetic defect. `w3sis2md.F90` recompiled from
  `build_mixa`'s `flags.make` in the pinned shell (gfortran 16.2.0), disassembly identical to the
  CMake object; the three copies (standalone, inlined in `INSIS2` at `:582` and in `W3SIS2` at
  `:850`) read against the C++ `w3fsd_dave<false>` and `<true>` instances operation by operation:
  `1.01*ICEDMIN`, `maxss` operand order per site, the `ICEDMIN == 0` test, `logf`, `/ln 2`
  (`0x3f317218`), `+2`, `1-GAM`, `-GAM` by sign flip, four `powf` calls, two subtractions, the
  quotient, `GAM/(GAM-1)` and the product with the quotient as first source; `.rodata` bits of
  `1.01`, `ln 2`, `2` and `1` equal. Fixed: the comment that the two `MAX` orders differ only for
  a NaN `ICEDMIN` (they also differ for a NaN `ICEDMAX` or a +0/-0 tie, which neither call site
  can produce). For the future shim, ⚠ `W3SIS2`'s object computes the argument `ICEDMIN` once as
  `(TPI/WN_R(IK))*IS2PARS(14)` and reuses it for the guard at `:849`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
