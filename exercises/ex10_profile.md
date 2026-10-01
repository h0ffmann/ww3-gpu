# Exercise 10 — where does the time go?

**Lesson:** [10 — modern Fortran refactoring](../course/10-modern-fortran-refactoring.md)
(and the profiling half of 09). **Time:** ~40 min. **Solution:**
[`solutions/ex10_profile.sh`](solutions/ex10_profile.sh).

## Goal

Before refactoring or porting anything, know which routines cost what. You produce two
tables from a `gprof` flat profile of `ww3_shel` on `ww3_ts1`:

```
| routine  | self % | cumulative % | phase        |      | phase         | self % |
|----------|--------|--------------|--------------|      |---------------|--------|
| w3snl1   |  ..    |  ..          | source terms |      | source terms  |  ..    |
| w3srce   |  ..    |  ..          | source terms |      | propagation   |  ..    |
| w3pro3   |  ..    |  ..          | propagation  |      | communication |  ..    |
| ...      |        |              |              |      | I/O           |  ..    |
```

The phase buckets are lesson 09's: `w3srce*`/`w3sin*`/`w3sds*`/`w3snl*`/`w3sbt*` are
source terms, `w3pro*`/`w3uqck*` propagation, `w3gath`/`w3scat`/`mpi_*` communication,
`w3io*` I/O. Everything else is "other", and a big "other" is a finding in itself.

## Steps

1. **Build with `-pg`.** Both compile and link need it: `FFLAGS=-pg LDFLAGS=-pg` before
   `scripts/02_build_ww3.sh` (which configures from scratch, so CMake reads them). Keep
   the `Release` build type: a profile of `-O0` code tells you about `-O0`.

2. **Run the case, then run `ww3_shel` alone.** Every `-pg` program writes `gmon.out` in
   its working directory, so the pipeline's last program (`ww3_ounf`) overwrites
   `ww3_shel`'s. Rerun `./ww3_shel` by itself in `work_lab/` after the pipeline.

3. **`gprof -b -p ./ww3_shel gmon.out`.** The flat profile: `% time`, cumulative seconds,
   self seconds, calls, name. gfortran names module procedures `__w3snl1md_MOD_w3snl1`;
   strip the prefix to get the routine.

4. **Bucket and sum.** `awk` the top 25 into the first table and sum `self %` per phase
   into the second. `kokkos/tools/profile/gprof_table.sh` does the same with a maintained
   name map; write yours first, then diff the two tables.

## What to hand in

Both tables and three sentences: which phase dominates on `ww3_ts1`, which single routine
you would port first, and why the answer would differ on `ww3_tp2.2`.

## Going further

- Profile `just bench-case --size medium` instead of a regtest. Does the ranking change
  when the grid is big enough for propagation to matter?
- `gprof` attributes inlined code to the caller. Rebuild with `-fno-inline` and see which
  "big" routine dissolves into its callees; that tells you where the real loop is.
- `perf` is not in the pratico shell; on a host that has it,
  `kokkos/tools/profile/perf_table.sh` gives the same table without recompiling.
