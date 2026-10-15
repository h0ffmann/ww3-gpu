# The Kokkos DIA byte-identical inside WW3: `AF11` used the `-O0` chain of `x**11`

2026-10-14 to 2026-10-15 (UTC), the owner with an agent. WFIP-0003, ADR-0010, PR #120; follows
[2026-10-14-hoff-matrix-entry16.md](2026-10-14-hoff-matrix-entry16.md),
[2026-10-15-hoff-matrix-entries51-54.md](2026-10-15-hoff-matrix-entries51-54.md) and
[2026-10-15-hoff-matrix-entries6-10.md](2026-10-15-hoff-matrix-entries6-10.md).

## Question

The Kokkos DIA matched a Fortran fixture built at `-g` bit for bit, but inside WW3 (built at
`-O3`) `hs` differed by about 1e-7 relative (`ww3_ts1`, 4 of 22 files differing in `test.comp`),
and five matrix entries failed `nccmp-tol` from that perturbation amplified by WW3. The entries
that were already byte-identical (`ww3_ts1 -g ST4_T707`, `-g ST4_T713`, `ww3_tc1`, `ww3_tnc1`,
`ww3_tp2.6`) gave the clue: the two GQM entries never call `INSNL1` (`w3iogrmd.F90:1801-1804` at
`6d1bcdfe` calls `INSNLGQM` when `IQTPE` is not positive), so the suspect was the tables, not the
kernel. Which line of `INSNL1` or `W3SNL1` does the `-O3` executable compute differently?

## What was done

Owner's workstation, Intel Core i9-14900, CPU only, inside
`nix develop ./nix-config/labs/pratico#ww3` (gfortran 15.3.0); WW3 at `6d1bcdfe`, never written.
All scratch work was outside the repository (no port test, ADR-0010).

1. `w3snl1md.F90` compiled with the flags of `build/hoff-openmp/ww3/model/src/CMakeFiles/ww3_lib.dir/flags.make`
   (`-O3 -fallow-argument-mismatch -g -fno-second-underscore -ffree-line-length-none -O3`) plus
   `-fdump-tree-optimized`, and `INSNL1` and `W3SNL1` read in the optimised GIMPLE line by line
   against `snl1_tables.cpp` and `snl1_dia.cpp`.
2. A scratch Fortran routine `y(i) = x(i)**11`, compiled at `-O0 -g` and at `-O3 -g`, called
   from C++ (`-O3 -march=x86-64-v3 -ffp-contract=off`) on 1 000 000 floats in [0.02, 1.0), and
   compared bit for bit with the two multiplication chains.
3. `pow11()` in `kokkos/src/ww_kokkos/snl1_tables.cpp` changed to the `-O3` chain; then
   `just kokkos-test serial-debug`, `just kokkos-test openmp-release`, `just smoke-hoff`,
   `just hoff-matrix --jobs 8` and `just full-hoff`.

## Result

- (v) `AF11A = (SIG(IFR)*TPIINV)**11` and `AF11A = FR**11` (`w3snl1md.F90:741`, `:750`) are
  expanded at `-O3` by GCC's power tree as `x2 = x*x`, `x3 = x2*x`, `x5 = x2*x3`, `x6 = x3*x3`,
  `x11 = x5*x6` (the `powmult_*` lines of the dump). At `-O0` the same source calls libgcc's
  `__powisf2`, whose binary chain ends `x3*x8`, which is what `pow11()` computed.
- (v) Step 2: against `-O0`, the `x3*x8` chain differs on 0 of 1 000 000 inputs and the `x5*x6`
  chain on 643 427; against `-O3`, the reverse (643 427 and 0). This is the whole `-g` versus
  `-O3` story of `docs/PORT_ASSESSMENT_202610.md` §4.
- (v) Nothing else in the dump differs from the C++: `XFR**IFRP` and the other runtime exponents
  stay calls of `__powisf2` at `-O3` (`nm` lists it), which `powi()` reproduces;
  `(1.+LAM)**4` is `x2*x2` at both levels; `CONS` (`:340-342`) is `expf` and the same operation
  order; `CONX = TPIINV / SIG(IFR) * CG(IFR)` (`:351`) uses `TPIINV` = 0.15915493667125701904296875,
  as `kTpiInv`; sections 3 and 4 are not reassociated.
- (v) After the fix: `just kokkos-test serial-debug` and `openmp-release`, 10/10 passed;
  `just smoke-hoff`: "the two outputs are byte-identical", `hs` max rel 0 against the Fortran
  DIA and 0 pair to pair against `--ww3` (was 9.879e-8).
- (v) `just hoff-matrix --jobs 8`, 2026-10-15T02:58:30Z to 03:21:07Z, 4 threads per entry:
  all 100 entries built, ran and logged the Kokkos DIA; `test.comp` 0 differing files on all
  100; `nccmp-tol` 71 PASS, 29 n/a, 0 FAIL
  ([`../data/hoff_matrix_2026-10-15.tsv`](../data/hoff_matrix_2026-10-15.tsv)). Entries 6, 10,
  16, 51 and 54, which failed on 2026-10-14, are byte-identical: the amplification described in
  the two earlier entries had nothing to amplify.
- (v) `just full-hoff` (`bench_small`, 9 520 points, 12 h), 2026-10-15: "the two outputs are
  byte-identical"; `hs` max rel 0 over 119 652 values against the Fortran DIA and pair to pair
  against `--ww3` (was 1.832e-7).
- (v) No tolerance, no Fortran, no compiler flag and no WW3 file was changed.
- ⚠ The `x5*x6` chain is GCC's (`powi_table`, unchanged for many releases). A WW3 built with
  another compiler, or at `-O0`, would compute `AF11` differently again; the port follows the
  build WW3's CMake makes with gfortran.

## What it changes

The Kokkos DIA reproduces the Fortran DIA bit for bit inside WW3 on the CPU, on every serial
`NL1` entry of `matrix.base`. The open question of `docs/PORT_ASSESSMENT_202610.md` §4 is closed,
and the per-entry exception discussed at the end of the entries 51-54 log is not needed.
`kokkos/port_report/W3SNL1_KOKKOS.md` and `kokkos/ports.tsv` say so. Not covered: the GPU
(the owner's supervised run), and MPI or OpenMP-parallel WW3 entries, which the matrix does not
select yet.
