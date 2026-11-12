# Batch 4, units U3 to U7: seventeen routines translated to Kokkos

2026-11-12, the owner with agents. Goal `hr8xzdg1-6142`, plan
[`docs/plans/2026-11-01-001-port-batch-4-plan.md`](../plans/2026-11-01-001-port-batch-4-plan.md),
issue #42 (the port planner). Follows
[U1 and U2](2026-11-12-batch4-u1-u2-kokkos-translation.md).

## Question

The rest of the parked batch 4 plan, units U3 to U7, is `W3UNO2`, `W3UNO2R`, `W3UNO2S`, `W3MAP1`,
`W3MAP2` (U3); `W3SIC2`, `W3SIC3`, `WN_PRECALC_CHENG`, `W3IC3WNCG_V1` (U4); `W3SIS2`, `INSIS2`,
`DIAGONALIZE`, `W3ADDIG` (U5); `W3SBS1`, `W3SBT8`, `W3SBT9` (U6); and `W3FLD1` (U7). None is MPI, so
ADR-0017 holds and the MPI plan's M2 gate is not touched. Can they be translated to the bit for bit
standard, with one translator and one fresh reviewer per unit (Sonnet), and nothing run in WW3?

## What was done

- The toolchain is the one of U1 and U2: gfortran 16.2.0 from the nixpkgs revision that
  `picasso/labs/pratico/flake.lock` pins, Kokkos 5.2.2 Serial+OpenMP built from the flake's
  `kokkosHost` recipe ⚠. The full `ww3` shell could not be entered in the cloud session: ParMETIS's
  source download returned HTTP 403. Each unit built `ww3_lib` of the fork at `WW3@96892c95` in
  Release with the switch that compiles its routines (`switch_PR2_UNO`, `switch_ST4_PR1_FLD1`, an
  `IC2`/`IC3` variant of `switch_lab_shrd`, the `IS2` and `IG1` regtest switches, `BS1` with `BT8`),
  recompiled each module from its `flags.make` with `-fdump-tree-optimized-lineno`, and checked the
  disassembly equal to the CMake object (v). `W3SBT9` is not in the library under the `BT8` switch,
  so its object was compiled by hand with the same flags.
- U3: `uno2_w3uno2.hpp` (`W3UNO2`, `W3UNO2R`, `W3UNO2S`, Kokkos launchers, one `parallel_for` per
  Fortran loop), `prop_map1.hpp`, `prop_map2.hpp` (host-order kernels like `prop_map3.hpp`).
- U4: `ic2_w3sic2.hpp`, `ic3_w3sic3.hpp`, `ic3_wn_precalc_cheng.hpp`, `ic3_w3ic3wncg_v1.hpp`. The
  Fortran `PRINT` and `EXTCDE` paths return flags and codes; the caller does the `WRITE`.
- U5: `is2_w3sis2.hpp`, `is2_insis2.hpp` (with `DIAGONALIZE`), `gig1_w3addig.hpp`.
  `W3SIS2` calls libmvec's `_ZGVbN4v_logf` in the object, so the header references it like
  `st4_libmvec.hpp` and needs `libmvec` at link.
- U6: `bs1_w3sbs1.hpp`, `bt8_w3sbt8.hpp`, `bt9_w3sbt9.hpp`. `INSBS1`, the `INFLAGS1` and mud loads and
  the `WRITE` and `EXTCDE` calls stay with the caller.
- U7: `fld1_w3fld1.hpp`, a per-point device function in the shape of `ww::fld2::w3fld2`. The `KB`
  search loop (`w3fld1md.F90:564-569`) has no effect on any output of the `-O3` object and is not
  translated.
- 17 rows in `kokkos/ports.tsv`, a port report each, glossary entries, the headers included from
  `wave_support.cpp`.
- Not run: any WW3 regtest (ADR-0016), any GPU run, wiring into WW3, a CUDA build, a test of the
  port (ADR-0010).

## Result

- (v) U3, fresh review: no defect; the citations were fixed. `MIN` is `minss` with the first operand
  first, `MAX(X,0)` is `maxss` with `X` first, `SIGN(1.1,X)` is `copysignf`, no FMA.
- (v) U4, fresh review: no defect in the four new headers. It found three defects in the batch 3
  headers they call, and fixed them (see below).
- (v) U5, fresh review: no arithmetic defect; about 30 citations were fixed.
- (v) U6, fresh review: no defect in `W3SBT8` and `W3SBT9`. `W3SBS1`'s no-current branch calls
  `MATMUL`, whose sum order is libgfortran's. The translator's two-accumulator model of that order
  matched for even `NTH` from 4 to 120 and failed at `NTH` 2 and at odd `NTH`. The reviewer replaced
  it: `matmul_tn` now calls `_gfortran_matmul_r8` with the Fortran's own array descriptors. The
  symbol is weak and the code aborts if it is absent; the branch is host only, needs libgfortran at
  link (any WW3 executable has it) and relies on the GCC 8 or later descriptor layout ⚠. Against
  `MATMUL(TRANSPOSE())` it gave 0 differences for `NTH` 1 to 130 (reviewer, throwaway harness).
- (v) U7, fresh review: no defect.
- (v) Fixed by the U4 review, in the batch 3 headers (reports
  [`CMPLX_ROOT_MULLER_CHENG_KOKKOS.md`](../../kokkos/port_report/CMPLX_ROOT_MULLER_CHENG_KOKKOS.md)
  and [`W3IC3WNCG_CHENG_KOKKOS.md`](../../kokkos/port_report/W3IC3WNCG_CHENG_KOKKOS.md)): the
  convergence test of `CMPLX_ROOT_MULLER_CHENG` was `||` where `w3sic3md.F90:2693` has `.AND.`
  (`ic3_cmplx_root_muller_cheng.hpp:126`); its `NU == 0` block kept the real part of `P3` in
  `double` where `CMPLX` without `KIND` rounds it to `float`; `W3IC3WNCG_CHENG` used `rr = 0.01`
  where the Fortran has the `float` literal widened, and `Kokkos::fmin` for the `HICE` and `WN_I` caps
  where the object has `minpd` and `minss` with their own operand order. Found while translating
  `W3SIC3` and `W3IC3WNCG_V1`. Differential before the fix: about 1300 mismatches in
  `WN_PRECALC_CHENG`, 95 in `W3SIC3` and 151 in `W3IC3WNCG_CHENG`; after: 0.
- ⚠ Differentials, all throwaway harnesses with no command in the repository (ADR-0010), 0 mismatches
  after the fixes: U3 `W3UNO2`, `W3UNO2R`, `W3UNO2S` 1 299 647 `REAL` outputs in 4 000 cases each,
  `W3MAP1` and `W3MAP2` 400 random grids each (the `MAPTH2(1) /= 0` early return of `W3MAP2` was not
  exercised); U4 seeds 1, 2 and 5 over `WN_PRECALC_CHENG`, `W3IC3WNCG_V1`, `W3SIC3`, with `W3SIC2`
  checked by the reviewer; U5 9 grids by 4 seeds for `W3SIS2`, 12 runs of `W3ADDIG`, `INSIS2` tables
  and eigenvectors; U6 300 000 cases of `W3SBT8`, 20 000 of `W3SBT9` (396 401 elements skipped as
  indeterminate in the Fortran), 54 000 of `W3SBS1`; U7 216 000 words at `-O3`, 54 000 at
  `-march=x86-64-v3`, clean under ASan and UBSan.
- ⚠ Known limits kept in the reports: the object commutes some `+` and `*` operands where g++
  does not (NaN payload only); the object's `WN_I` loop swaps the operands of `MIN` on its last element,
  so a NaN there cannot be matched exactly from C++; the device math (`exp`, `pow`, `tanh`, `logf`,
  `sinhf`, `csqrtf`) is not glibc's, CUDA was not compiled; `FACVX` and `FACVY` are `POINTER` arrays and
  the C++ assumes stride 1; `W3FLD1`'s `FIRST` and `INFLD` state is per thread in the Fortran and per
  grid in the port, the same caveat as `W3FLD2`.
- (v) WW3 defects found, read and marked ⚠ in the reports, not fixed: `W3SIC3`'s thick-ice branch
  never sets `PTURB` and `PVISC` when `IC3PARS(2) <= 0` (the port uses 0); `W3SBT9` reads `SMUDWD(MAPWN)`
  where it is not stored; `W3SIS2` reads `SIS2ALPHAS` out of bounds for a NaN or infinite `ICEH`;
  `SIS1ALPHATABLE2` holds 520 values and 500 are used; `W3FLD1`'s `CRIT2` (`:549`) multiplies by 0.5
  where `CRIT1` (`:547`) divides, which looks like a slip, and its `ITERATION .LT. 2` branch
  (`:709-713`) cannot run; `W3MAP2`'s brief (`w3pro2md.F90:128`) reads "ULTIMATE QUICKEST", copied from `W3MAP3`.

## What it changes

The translated level (`python3 scripts/port_score.py --check`, `kokkos/PORT_STATUS.md` as
regenerated): 344 of 1 023 routines, 77 268 of 263 375 lines (29.34 %), from 327 and 71 303
(27.07 %). The run-time shares do not move: none of the seventeen is wired, and the `full` profile
does not time them. The shim for `w3srcemd.F90:2740` (`FLD1`), the checkpoint (ADR-0011) and the
regtests are the next steps, and the owner's.
