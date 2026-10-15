# The OpenMP entries of the regtest matrix: 25 of 25 byte-identical, with nesting allowed

2026-10-15 (UTC), the owner with an agent. WFIP-0003 §8 and §11, ADR-0010, PR #120, commit
`f32640c`; follows [2026-10-14-snl1-bitwise-af11.md](2026-10-14-snl1-bitwise-af11.md).

## Question

The serial `NL1` entries of WW3's `matrix.base` are byte-identical with the Kokkos DIA
([`../data/hoff_matrix_2026-10-15.tsv`](../data/hoff_matrix_2026-10-15.tsv)). The owner opened
the shared-memory OpenMP entries, CPU only. Does the Kokkos DIA still give the Fortran DIA's
answer when WW3 calls it from inside its own `!$OMP PARALLEL` region?

## What was done

Owner's workstation, Intel Core i9-14900, 32 hardware threads (`nproc --all`), CPU only, inside
`nix develop ./nix-config/labs/pratico#ww3` (gfortran 15.3.0, Kokkos 5.2.0 on OpenMP (v),
`docs/PORT_ASSESSMENT_202610.md`); ww3-gpu at `551684a` with the `--omp` selection of `f32640c`
uncommitted, WW3 at `6d1bcdfe` (`origin/hoff/kokkos-snl1`).

- The selection: every `matrix.base` entry whose switch has `NL1` and `OMPG` or `OMPH`, no `MPI`,
  and no `-p` or `-m` on its line. That is 25 entries, all `ww3_ts3` with `OMPG`, `ST1` to `ST6`
  crossed with `PR1`/`PR2`/`PR3` and `UQ`/`UNO` (v) (`just hoff-matrix --list --omp`). Their 25
  `mww3_test_05` twins run with `-m grdset_a`, three grids in one `ww3_multi` process, and the port
  refuses `IMOD > 1` (v) (`model/src/w3initmd.F90:754-759`), so they were left out.
- Each entry keeps `matrix.base`'s `-f -O -n $np`, with `$np` set to `OMP_NUM_THREADS=8`; four
  entries at a time, so 32 threads in use.
- First try, entries 1 to 4, without `OMP_MAX_ACTIVE_LEVELS`: `just hoff-matrix --omp --jobs 4`.
- Full run: `OMP_MAX_ACTIVE_LEVELS=2 just hoff-matrix --omp --jobs 4`, the variable exported for
  the Fortran run and the Kokkos run of each entry alike, 2026-10-15T04:04:06Z to 04:23:25Z.

These commands are those of `scripts/hoff_matrix.sh` at `f32640c`. The driver has since moved to
CTest under `regtests/` (WFIP-0003, update of 2026-10-15); its equivalent is
`just hoff-matrix 4 omp`, whose `omp` selection exports `OMP_MAX_ACTIVE_LEVELS=2` for both runs
itself (v) (`regtests/cmake/run_entry.cmake:39-41`, read 2026-10-15); not run with the new driver ⚠.

## Result

- (v) Without `OMP_MAX_ACTIVE_LEVELS`, every Kokkos run aborted at the first DIA call:
  `Kokkos::OpenMP: Nested parallelism requires the maximum active levels to be larger than 1`
  (entries 1 to 4, `ran=fortran-only`, recorded in the table's header). The shim is called at
  `model/src/w3srcemd.F90:1276`, inside `!$OMP CRITICAL (WW_KOKKOS_SNL1)` (`:1275-1278`), and
  `W3SRCE` runs inside `W3WAVE`'s `!$OMP PARALLEL` / `!$OMP DO` under `W3_OMPG`
  (`model/src/w3wavemd.F90:2240-2243`). Kokkos's OpenMP backend opens its own parallel region
  there, which is a nested one, and it refuses while the runtime allows only one active level.
  That libgomp's default was one level in this environment is inferred from the abort and from
  the variable curing it ⚠ (the runtime's documentation was not read).
- (v) With `OMP_MAX_ACTIVE_LEVELS=2`, all 25 entries built and ran with the Kokkos DIA logged:
  `test.comp` 0 of 20 files differing on every entry, `nccmp-tol` PASS on `hs` on every entry,
  tolerances unchanged ([`../data/hoff_matrix_omp_2026-10-15.tsv`](../data/hoff_matrix_omp_2026-10-15.tsv);
  `awk -F'\t' '!/^#/ && $1!="n" && $7=="0/20" && $8=="PASS"' docs/data/hoff_matrix_omp_2026-10-15.tsv | wc -l`
  prints 25).
- (v) Wall time 19 min 19 s for the matrix; per entry 79 s to 200 s, 4203 s summed over the 25
  (the `wall_s` column).
- ⚠ The `!$OMP CRITICAL` still serialises the DIA calls: this shows the answer does not change
  under OpenMP, not that the DIA runs in parallel. Whether a nested Kokkos team inside the critical
  section costs or saves time was not measured.

A correction to the serial table found on the way: the header of
[`../data/hoff_matrix_2026-10-15.tsv`](../data/hoff_matrix_2026-10-15.tsv) says "4 threads"
because the old script printed `nproc`, which honours `OMP_NUM_THREADS`; the machine has 32
hardware threads (v) (`nproc --all`). A comment line in that header now says so; no row changed.

## What it changes

- The `W3SNL1` port report and the `regtested` cell of `kokkos/ports.tsv` cite the OpenMP table.
- An open issue of the port, not of the driver: the shim works under `OMPG` only when the caller
  allows nested parallelism. Two fixes are possible, both a change to Kokkos code and both
  awaiting the owner: the shim sets `omp_set_max_active_levels(2)` when it initialises, or it runs
  the kernel on `Kokkos::Serial` when `omp_in_parallel()` is true. Until then the `omp` selection of
  the matrix driver exports `OMP_MAX_ACTIVE_LEVELS=2`, and a WW3 user outside it must do the same.
- The 161 `NL1` entries with `MPI` remain out of scope and were not run.
