# Solutions

One working version of each exercise, in the language the sheet asks for.

| Exercise | File | Needs | Status |
|---|---|---|---|
| 09 | `ex09_matrix.sh <ww3-dir> [regtest]` | a WW3 checkout, `just rt`, `nccmp-tol` | ⚠ not executed here (no WW3 checkout in the environment it was written in); `bash -n` + shellcheck clean |
| 10 | `ex10_profile.sh <ww3-dir> [regtest]` | a WW3 checkout, `just rt`, `gprof` | ⚠ same |
| 11 | `ex11_refactor.F90` + `ex11_refactor_test.F90` | gfortran | (v) built with `-std=f2018 -Wall -Wextra -fimplicit-none`, test passes (legacy and refactored agree bit for bit) |
| 12 | `ex12_reduce.cpp` | Kokkos (the `#ww3` shell) | (v) built warning-free, self-check passes on the OpenMP backend (≤ 0.3 % from the closed form) |
| 13 | `ex13_compare.sh <ww3-dir> [regtest]` | a WW3 checkout, `just rt ww3_ts1 ST4`, `kokkos/tests/L2_replay.sh`, `nccmp-tol` | ⚠ same as 09 |

## Building the compiled ones

`CMakeLists.txt` here is a deliberately tiny standalone project (not part of the
`kokkos/` tree), so it doubles as the template for building your own code against the
lab's headers. From the repository root, inside `just ww3`:

```bash
cmake -S exercises/solutions -B exercises/solutions/build -G Ninja
cmake --build exercises/solutions/build
ctest --test-dir exercises/solutions/build --output-on-failure
```

`ex12_reduce` only includes `kokkos/src/ww_kokkos/{real,spectrum_fixtures}.hpp`, which
are header-only, so the project needs `find_package(Kokkos)` and an include path, nothing
else. `ex11_refactor_test` needs nothing at all.

## Running the shell ones

They write everything under `exercises/solutions/out/exNN/` (git-ignored) and print a
Markdown table at the end. Each rebuilds WW3 (`scripts/02_build_ww3.sh`), so budget a
few minutes per build; 09 builds twice.

```bash
just ww3
export WW3=$HOME/src/WW3
exercises/solutions/ex09_matrix.sh  $WW3 ww3_tp1.1
exercises/solutions/ex10_profile.sh $WW3 ww3_ts1
exercises/solutions/ex13_compare.sh $WW3 ww3_ts1
```

## If a shell solution does not run

1. Read the log it names: `build.log`, `regtest.log`, `replay.log` under `out/exNN/`.
2. The flag plumbing (`FFLAGS`, `LDFLAGS`, the `None` build type in 09) assumes WW3's
   CMake seeds `CMAKE_Fortran_FLAGS` from the environment and appends its own per
   build type. Check `grep -n Fortran_FLAGS $WW3/model/CMakeLists.txt $WW3/cmake/*` for
   your revision and adjust the one line that passes them.
3. 13 depends on the format of the table `L2_replay.sh` appends to
   `kokkos/PORT_STATUS.md`; if the pass/fail count comes out empty, the table is still
   in `out/ex13/l2_table.md` for reading by hand.
