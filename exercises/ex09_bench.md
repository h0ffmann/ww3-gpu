# Exercise 09 — the compile-option matrix

**Lesson:** [09 — benchmark, profile, compile, run](../course/09-benchmark-profile-compile-run.md).
**Time:** ~45 min, most of it waiting for builds. **Solution:** [`solutions/ex09_matrix.sh`](solutions/ex09_matrix.sh).

## Goal

Measure, on your own machine, what two compiler flag sets and three OpenMP thread counts
do to one WW3 run, and whether any of them changes the answer. You produce one table:

```
| flags              | threads | wall s (ww3_shel) | nccmp-tol vs reference |
|--------------------|---------|-------------------|------------------------|
| -O2                | 1       | ...               | pass                   |
| -O2                | 2       | ...               | pass                   |
| ...                |         |                   |                        |
| -O3 -march=native  | 4       | ...               | ?                      |
```

The second column is the one people forget. A faster binary that no longer reproduces the
reference inside tolerance is a different model, not an optimisation.

## Steps

1. **The reference.** `just rt ww3_tp1.1` builds with the test's own switch and runs it;
   its `ww3*.nc` in `$WW3/regtests/ww3_tp1.1/work_lab/` is what everything else is
   compared to. Copy it somewhere safe: every later run overwrites `work_lab/`.

2. **A switch with threads in it.** `switches/switch_lab_shrd` has `SHRD` but no `OMPG`,
   so OpenMP directives are not even compiled. Make a copy with `OMPG` added. (Why do
   both keywords coexist? Read `switches/README.md`.)

3. **Two builds.** `scripts/02_build_ww3.sh` configures from scratch, so CMake picks up
   `$FFLAGS`. Build once with `FFLAGS=-O2` and once with `FFLAGS="-O3 -march=native"`.
   WW3's own CMake appends per-build-type flags *after* yours; check
   `$WW3/model/CMakeLists.txt` to see what build type keeps them out of the way.

4. **Time only `ww3_shel`.** Run the regtest pipeline once per build to set `work_lab/`
   up, then rerun `./ww3_shel` alone with `OMP_NUM_THREADS=1`, `2`, `4` and time it with
   `date +%s.%N` around the call. Not `time` on the whole pipeline: `ww3_grid` is serial
   setup and flattens every scaling curve (lesson 09, Amdahl).

5. **The verdict.** After each run, `./ww3_ounf`, then
   `kokkos/build/openmp-release/tools/nccmp-tol/nccmp-tol reference.nc ww3*.nc`. Its exit
   code is the verdict; its table says which field moved and by how much.

## What to hand in

The table, plus one paragraph: which cell would you run operationally, and what in the
table justifies it? If `-march=native` or threads fail the tolerance, say which field and
whether the difference is explainable (FMA contraction, reduction order) or a bug.

## Going further

- `ww3_tp1.1` is pure propagation and tiny; threads will barely register. Repeat on
  `ww3_ts1` (source terms) or on `just bench-case --size small`, where `OMPG` has loops
  to thread.
- Add a `DIST` (MPI) build to the matrix with `switches/switch_lab_mpi` and
  `mpirun -np 1/2/4`. Now the layout question of lesson 09 (ranks × threads on one
  node) is yours to answer with numbers.
