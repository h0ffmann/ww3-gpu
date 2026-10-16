# The 92 easiest routines left in Fortran translated to Kokkos, one agent per routine

2026-10-16, the owner with agents. Issue #42, PR #159 (`goal/42-port-batch-easy`), plan
[`docs/EASY_BATCH_KOKKOS_PLAN_202610.md`](../EASY_BATCH_KOKKOS_PLAN_202610.md).

## Question

After the source-term packages (PR #157), the owner asked for the easiest Fortran left, smallest
first. Can the 92 untranslated routines of at most 100 lines of the compute modules of
`WW3-noaa/model/src` at `03adda91` (no MPI, not file I/O) be translated to the bit-for-bit
standard in one pull request, with one translator and one fresh reviewer per routine, and nothing
run?

## What was done

- Six WW3 CMake Release builds of `WW3-noaa` at `03adda91`, target `ww3_lib`, in the pinned shell
  (gfortran 16.2.0): `lab` (`switch_lab_shrd`), `mixa` (with `TIDE UOST FLD2 IG1 IC3 IS2 BT8`),
  `mixb` (with `NL4 IC5 BT9 NLS`), `nl5`, `smc` (`switch_SMCMlt`) and `fld1`
  (`ww3_ts3/input/switch_ST4_PR1_FLD1`). Every translator and reviewer recompiled its module from
  that build's `flags.make` and checked the object identical to the CMake one.
- 92 routines of 20 modules, one agent each, into 92 headers under `kokkos/src/ww_kokkos/` with a
  port report each: 34 of `w3profsmd` (the SPARSKIT and BLAS helpers of the implicit solvers), 7
  each of `w3tidemd`, `w3smcomd` and `w3sic3md`, 5 each of `w3uostmd` and `w3gkemd`, 4 of
  `w3psmcmd`, 3 each of `w3sis2md`, `w3sic5md` and `w3fld1md`, and 1 or 2 of ten others. The
  `W3RPWNICE` header also translates `FINDROOTS_NR` (`w3sis2md.F90:1204-1301`), the Newton solver
  between it and `FUNCD_FVAL`/`FUNCD_FDERIV`; it gets its own row, so 93 rows enter
  `kokkos/ports.tsv`.
- Then one fresh agent per routine compared the C++ object (openmp-release flags,
  `-ffp-contract=off`) with the Fortran object again, operation by operation, and fixed only what
  it was sure of. One pass, as the owner asked.
- Not run: no ctest, no regtest, no executable that calls a kernel, no GPU, no test written (the
  owner, 2026-10-16). `just kokkos-build serial-debug` and `openmp-release` compile all 92 headers
  together through `wave_support.cpp`. CUDA was not compiled.

## Result

- (v) Static parity on the host for all 92 (and `FINDROOTS_NR`), for every input whose result is
  not a NaN: the same operations in the same order, the folded constants' bits, `MIN`/`MAX`
  operand order per site including vector bodies and scalar tails, libgfortran's
  `_gfortran_pow_c8_i4` (`FUNCD_FVAL`, `FUNCD_FDERIV`) read from the pinned `libgfortran.so.5.0.0`
  and written out, glibc `cexp` where gfortran calls it (`W3SIC3`, `W3SIC5`), and libmvec
  `_ZGVbN4v_cosf`/`sinf` where gfortran vectorises (`UOST_SOURCETERM_SETGRID`).
- (v) What the fresh reviews fixed in the port, each read again in the object: `DPYTHAG` lost a
  `fabs` before the squared quotient (the sign of a NaN), fixed with `__builtin_assoc_barrier` for
  the Fortran's parentheses; `MAPSTA_SMC` cleared out of bounds for negative extents;
  `UOST_SOURCETERM_SETGRID` took the libmvec path at `NTH = HUGE(0)` where the object stays scalar;
  `UOST_SOURCETERM_COMPUTE` and `CMPLX_COSH` added in the other operand order (a NaN payload only),
  fixed with `GCC novector` and with the order of the `cexp` calls. The other reviews found no
  arithmetic defect; 23 corrected a line citation, a switch list or a claim of a report.
- ⚠ Not fixable from C++ source: in 13 routines the review named the site (the translators named more), g++ and gfortran put the two operands of
  one commutative product or sum in different registers, so when both are NaN with different
  payloads the result's payload can differ. Each report names the site.
- (v) What they found in WW3 at `03adda91` (read from the source and the objects), reproduced and
  marked ⚠ in each report, not fixed: `CALC_INTERP` writes `XDIST`/`YDIST` out of bounds
  (`w3smcomd.F90:646`, `:688-689`) and cannot run to completion (`NNIDX`, `:673`);
  `FINDROOTS_NR`'s "ROOT NOT CONVERGED" stop is unreachable (`w3sis2md.F90:1278`, `:1290-1293`);
  `F_ZHAO_CHENG` returns an unassigned value for `JUDGE < 0` (`w3sic3md.F90:2803-2807`); an
  infinite `U10DIR-CGDIR` makes `COMPUTE_REDUCTION_PSI` loop forever (`w3uostmd.F90:746`);
  `SMCGradn` is called with `CVQ(-9)` by sequence association (`W3S2XY_SMCNN_INT`); `IMPLU`
  reads an undefined `npm1`; `TIDE_PREDICT` clears `SDEV` on every call (`w3tidemd.F90:2531`).
- (v) Many have no caller in WW3 (most of the SPARSKIT helpers of `w3profsmd`, `TIDE_PREDICT`,
  `TIDE_PREDICT_ONLY`, `CALC_INTERP`) or none a regtest reaches (`W3RPWNICE` needs `IS2DISP = T`,
  `WND2SAT` of FLD1 needs `TAILTYPE = 1`): their translation can never be exercised, each report
  says so.
- ⚠ On a GPU none of these is shown bit for bit: no glibc `cexp`, `hypot` or libmvec there.

## What it changes

184 of the 1 023 routines of `model/src` have a C++ translation, 30 531 of its 263 375 lines (11.59 %; the counts are
`python3 scripts/port_score.py --write` on a clean tree, in `kokkos/PORT_STATUS.md`). None of the
92 is wired: no shim and no caller hunk was written. Their checkpoint (ADR-0011) needs both, then
the regtest matrix.
