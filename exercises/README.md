# Exercises for lessons 09–13

Five exercises, one per lesson of the second half of the course, 30–90 minutes each.
Each sheet says what to produce and how to check it; `solutions/` has a working version
of every one. Shell, Fortran and C++ only, the same languages as the lab code.

| | Sheet | Lesson | You produce |
|---|---|---|---|
| 09 | [`ex09_bench.md`](ex09_bench.md) | [09 — benchmark, profile, compile, run](../course/09-benchmark-profile-compile-run.md) | a compile-option matrix: flags × threads → wall-clock and an `nccmp-tol` verdict |
| 10 | [`ex10_profile.md`](ex10_profile.md) | [10 — modern Fortran refactoring](../course/10-modern-fortran-refactoring.md) | a gprof profile of `ww3_shel`, bucketed into source terms / propagation / communication / I/O |
| 11 | [`ex11_refactor.md`](ex11_refactor.md) | [10 — modern Fortran refactoring](../course/10-modern-fortran-refactoring.md) | a WW3-style routine refactored (explicit interface, `intent`, `pure`, no automatic array) and a parity test to 1e-6 |
| 12 | [`ex12_reduce.md`](ex12_reduce.md) | [11 — Kokkos and modern C++](../course/11-kokkos-and-modern-cpp.md) | `Hs` at every sea point from an action-density `View` with a team `parallel_reduce`, checked against a closed form |
| 13 | [`ex13_port.md`](ex13_port.md) | [12 — porting a kernel: W3SNL1](../course/12-porting-a-kernel-w3snl1.md), [13 — bulk porting](../course/13-bulk-porting-with-agents.md) | an L2 replay of `ww3_ts1` (Fortran vs Kokkos `W3SNL1`) and a written reading of its table |

Do them in order: 09 gives you the reference run and the measuring habit, 10 tells you
what to port, 11 is the Fortran half of a port, 12 the C++ half, and 13 is the validation
ladder end to end.

## Setup

Everything runs inside the pinned toolchain shell, from the repository root:

```bash
just ww3                          # gfortran, CMake, Kokkos, GoogleTest, NetCDF, gprof, cdo, ecCodes
just rt ww3_tp1.1                 # the reference run that 09 and 13 compare against  (~30 s)
just rt ww3_ts1 ST4               # ...and the source-term one (its input/ has switch_ST4, not switch_PR3_UQ)
just kokkos-build openmp-release  # nccmp-tol, ww_bench_case, ww_fetch_analyse
```

The shell solutions (09, 10, 13) need a WW3 checkout and take minutes; the compiled ones
(11, 12) build in seconds:

```bash
cmake -S exercises/solutions -B exercises/solutions/build -G Ninja
cmake --build exercises/solutions/build
ctest --test-dir exercises/solutions/build --output-on-failure
```

`just clean-runs` removes `exercises/solutions/build/` and `exercises/solutions/out/`.

## The two things that will trip you up

**1. A stale `mod_def.ww3`.** Every WW3 program reads it; `ww3_grid` writes it. If you
change a switch or a `ww3_grid.nml` and the numbers do not move, you are running against
the old one. `scripts/03_run_regtest.sh` rebuilds `work_lab/` from scratch each time,
which is why the solutions go through it rather than reusing a directory.

**2. Bit-for-bit is not the goal, agreement inside a stated tolerance is.** `-O3
-march=native`, OpenMP thread count, and a reduction written in a different order all
change the last bits. `nccmp-tol` and its tolerance table
(`kokkos/tools/nccmp-tol/tolerances.txt`) are how the lab decides whether a difference
matters; "the file changed" is not a finding, "`hs` differs by 3e-4 relative where the
table allows 1e-4" is.

## Debugging

When a solution script stops, read the log it points at (`exercises/solutions/out/exNN/`)
before re-running. `ww3_grid` and `ww3_shel` are unusually clear about which namelist
block or which file they did not like; a build failure is almost always a missing switch
keyword (`OMPG` for threads, say, not `NC4`, which is inert in 7.14: netCDF output only needs CMake to find netCDF).
