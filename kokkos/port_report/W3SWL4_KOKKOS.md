# W3SWL4_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Swell dissipation after Ardhuin et al. (2010+) as coded in `W3SWLDMD`: an orbital Reynolds number
`RE = 4 UOSIG AORB / VA` chooses a turbulent damping `D = -(16/g)(rho_a/rho_w) FE sigma^2 UOSIG` or a
laminar one `D = -2 (rho_a/rho_w) CDSV k sqrt(2 VA sigma)`, and `S = D A`
(`w3swldmd.F90:206-228`).

⚠ Nothing in `model/src` calls `W3SWL4` (v) (`git -C WW3-noaa grep -n -i w3swl4 -- model/src`
finds only the module's own lines, `w3swldmd.F90:49`, `:73`, `:93-242`; `W3SRCE` imports only
`W3SWL6`, `w3srcemd.F90:575`, `:1332`; so do `gx_outp.F90:679`, `ww3_ounp.F90:1545`,
`ww3_outp.F90:1471`). It is translated for completeness. No regtest can exercise it, and its
checkpoint (ADR-0011) can never show a byte of it.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SWL4`, `model/src/w3swldmd.F90:93-242` at NOAA-EMC/WW3 `03adda91` (150 lines). The module is compiled with `ST6` (`model/src/cmake/switches.json:258`): 61 switch files (`git grep -lw ST6 -- '*switch*'`), among them `ww3_ts1/input/switch_ST6`, `ww3_tp2.15`, `mww3_test_05`, `model/bin/switch_NRL3`, `switch_UoM_*`, `switch_swin`; no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` is `W3_S` (tracing, no arithmetic) |
| Kokkos code | [`../src/ww_kokkos/srce_swl4.hpp`](../src/ww_kokkos/srce_swl4.hpp), `ww::srce::swl::w3swl4_point`, and `w3swl4_ikn`, the host build of the `SAVE` table `IKN = IRANGE(1,NSPEC,NTH)` (`:191-193`) through `irange` of [`srce_swl_util.hpp`](../src/ww_kokkos/srce_swl_util.hpp). The locals `EB(NK)` and `WN2(NSPEC)` are caller-provided work arrays. Returns 2 where the Fortran stops in `EXTCDE(2)` (`SIZE(IKN) /= NK`, `:194-197`), before `D = 0` as in the Fortran |
| C shim | none yet, and none planned while nothing calls the routine |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` object of `w3swldmd.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4`), recompiled with `-fdump-tree-optimized-lineno` and checked
byte-identical in its disassembly to the CMake object; compared with a throwaway C++ unit compiled
with the `openmp-release` flags (warning-free), operation by operation in the optimised GIMPLE,
2026-10-16.

- (v) Reductions, all in index order from `0.0`: `SUM(RESHAPE(A,(/NTH,NK/)),1)` (`:206`) and
  `SUM(RESHAPE(A*SIG2**2,...),1)` (`:211`) per `IK` over `ITH`, and `SUM(EB)` (`:207`, `:212`).
  gfortran vectorised the loads by 4 but folds the lanes one by one into the running sum
  (`BIT_FIELD_REF` 0, 32, 64, 96 added in turn), a strict left fold; the C++ does the same with 8
  lanes. No partial sums in either.
- (v) `EB = SUM*DDEN/CG` is `(sum*dden)/cg` in both (the object writes `dden*sum`, the same bits);
  `EMEAN = SUM(EB) + (EB(NK)/DDEN(NK))*FTE`; `A*SIG2**2` is `A*(SIG2*SIG2)` in both.
- (v) `AORB = 2*sqrt(EMEAN)`, `UOSIG = 2*sqrt(SUM(EB))`: `sqrtss`/`__builtin_sqrtf`, correctly
  rounded; in the C++ the errno path calls `sqrtf` only for a negative argument, which returns the
  same NaN. `RE = ((4*UOSIG)*AORB)/VA` with a true division by `VA = 1.4E-5` (`0x376ae18b`) in
  both; `RE > 1.0E5` false on NaN in both.
- (v) Folded constants, read in `.rodata.cst4` of both objects: `16.0/GRAV = 0x3fd0da0b`
  (`:223`), `2.0*VA = 0x37eae18b` (`:225`, exact), `CDSV = 1.2 = 0x3f99999a`, `DWAT = 1000.`
  (a true division `DAIR/1000.` in both), `4.0`, `1.0E5`. Each has a `static_assert` of how it is
  built.
- (v) `:223`: `-(((16/g)*(DAIR/DWAT))*FE*SIG2^2*UOSIG)`, the factor `((DAIR/1000)*16/g)*FE`
  hoisted out of the loop in both, then `(fac*SIG2^2)*UOSIG`, negated last. `:225`:
  `-(((((DAIR/1000)*2)*1.2)*WN2)*sqrt(SIG2*2.8E-5))` in both. `S = D*A` (`:228`).
- (v) `WN2(IKN+(ITH-1)) = WN` (`:202-204`): a scatter in `ITH` then `IK` order, as the object
  does; integer only.
- (v) No FMA (no `vfmadd` in the C++ object), no libm or libmvec call besides `sqrtf`, no `MIN`/`MAX`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`), never run. The SASS has 104 `FFMA`, with `MUFU.RCP` and
  `MUFU.RSQ`: from the IEEE division and square root sequences, not traced one by one.
- (v) Fresh review 2026-10-16: no arithmetic defect. The `W3SWL4` object (`ST6 FLX4`, recompiled
  disassembly-identical to the CMake object) and a fresh `openmp-release` C++ unit were compared in
  the optimised GIMPLE and the calls: same in-order lane folds for the four `SUM`s, same operand
  grouping and negation at `:223`, `:225`, `:228`, same folded `.rodata` bits, `sqrtf` the only
  call, no `vfmadd`. Fixed: the line cites of the two `D` branches and `S = D*A` were off by two
  (`:225`/`:227`/`:230` for `:223`/`:225`/`:228`) in the header and this report.

## Defects in WW3 itself, reproduced, not fixed

- ⚠ `IKN` is `ALLOCATABLE, SAVE` and built on the first call only (`:180`, `:191-193`): a second
  grid of `ww3_multi` with the same `NK` and another `NTH` would index `WN2(IKN+(ITH-1))` with the
  first grid's table, out of bounds when its `NTH` is smaller. Under `OMPG` the first allocation
  races as in `W3SWL6` (`AGENTS.md`, "Bit for bit"). The port takes the table from the caller,
  which decides per grid.
- ⚠ `FE = SWL6B1` (`:214`): the ST6 swell coefficient of `W3SWL6`'s namelist is reused here.
- ⚠ A negative `EMEAN` or `SUM(EB)` gives a NaN `RE`, which takes the laminar branch with a NaN-free
  `D` that does not depend on them; reproduced.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Nothing in WW3 calls `W3SWL4`, so no regtest can.
