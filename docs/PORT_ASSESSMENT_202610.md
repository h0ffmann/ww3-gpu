# Assessing the Kokkos port: does WW3 run with it?

Written 14 October 2026, at the owner's request: assess the existing port, make WW3 run with the
forked code on the Kokkos backend, and give two ways of running it with a smoke test and a more
expensive test each. `(v)` marks a claim checked by a command run for this document, on the
author's workstation without a working GPU driver (`nvidia-smi` could not reach the driver), with
the pratico toolchain (gfortran 15.3.0, Kokkos 5.2.0 OpenMP); `⚠` marks one that was not.

## 1. Summary

Before today, the port was a kernel and a recipe. `W3SNL1` (the DIA) and its `INSNL1` tables
existed in `kokkos/`, matched a Fortran fixture one call at a time on three presets, and
`kokkos/src/fortran_iface/PATCH.md` described how to wire them into WW3. The recipe had never
been applied or compiled, and WW3 had never run with the kernel.

Today WW3 runs with it on the OpenMP backend, end to end `(v)`:

| | `--ww3` (WW3 as written) | `--hoff` (WW3 with the Kokkos DIA) |
|---|---|---|
| smoke: `ww3_ts1`, one point, 10 days | `just smoke-ww3`: ww3_shel 2.67 s | `just smoke-hoff openmp`: 2.70 s Fortran DIA, 2.85 s Kokkos DIA; `hs` within 9.9e-8 relative, PASS |
| full: `bench_small`, 9 520 points, 12 h | `just full-ww3` | `just full-hoff openmp`: 152.7 s Fortran DIA, 217.9 s Kokkos DIA; `hs` within 1.8e-7 relative over 119 652 values, PASS |

The Fortran comes only from WW3: the h0ffmann/WW3 branch `hoff/kokkos-snl1` (commit `6d1bcdfe`),
which the `WW3/` submodule pins. The CUDA build compiles and links `(v)`; the CUDA run on the owner's
RTX 4090 is the step that remains `⚠`.

## 2. How the two ways run

`scripts/hoff.sh` (`just smoke`, `smoke-ww3`, `smoke-hoff`, `full-ww3`, `full-hoff`):

- `--ww3` builds the `WW3/` tree with `switches/switch_lab_shrd` and runs the case once.
- `--hoff` builds and installs `kokkos/` as the `ww_kokkos` CMake package (preset
  `openmp-release` or `cuda-release`), builds the same tree with the switch plus `KOKKOS` and
  `-DWW_KOKKOS=ON`, runs the case twice with the same executable (`WW_KOKKOS_SNL1=0` and `1`),
  checks that the second log says "Kokkos DIA (W3SNL1) enabled.", and compares the gridded output
  with `nccmp-tol` (the L2 replay).
- The backend is CUDA when `nvidia-smi -L` lists a GPU, else OpenMP; `just smoke-hoff cuda`
  forces it. The CUDA build configures WW3 with `CMAKE_CXX_COMPILER=nvcc_wrapper`.

## 3. What the first build and runs found

Each was fixed in this change, and each is a claim the recipe or the tests had not caught.

1. **`SIG(1)` does not compile** `(v)`. The recipe passed `SIG(1)` so that the shim reads bins
   1..NK, not bin 0. `SIG` is a POINTER in `W3GDATMD`, and gfortran rejects an element of a
   pointer array as the actual argument of an assumed-size dummy. `SIG(1:NK)` passes the same
   bins.
2. **The Kokkos package needs C++ enabled in WW3** `(v)`. WW3's CMake project is C and Fortran;
   `find_package(ww_kokkos)` runs Kokkos' package config, which needs a C++ compiler. The branch
   enables C++ only under `WW_KOKKOS`.
3. **`ww_kokkos` had no package** `(v)`. `kokkos/CMakeLists.txt` now installs the library, its
   headers and a `ww_kokkosConfig.cmake` that finds Kokkos.
4. **The CPU backend could not run WW3's grids** `(v)`. At `ww3_ts1`'s NK=36, NTH=24 the kernel
   needs 39 248 B of team scratch, more than the OpenMP backend's level-0 limit; the L1 fixture
   (NK=25) never reached it. The kernel now falls back to level-1 scratch when level 0 is too
   small; the arithmetic is the same, and the 14 tests still pass `(v)`.
5. **The `bench_small` fixture wrote no field and a key WW3 does not read** `(v)`. Its field
   stride was 0 (it was made for timing), and its `ww3_ounf.nml` used `FIELD%TIMESTOP`, which
   WW3's `FIELD_NML` does not have (`FIELD%TIMECOUNT` it does). The generator
   (`kokkos/tools/bench_case`) and the fixture are fixed; `hoff.sh` turns hourly HS output on.

## 4. Agreement, not identity

Inside WW3 the two DIAs agree to about 1e-7 relative, well inside the 1e-4 tolerance of
`kokkos/tools/nccmp-tol/tolerances.txt`, but the outputs are not byte-identical `(v)`. The L1
column of `kokkos/PORT_STATUS.md` says "bit-identical": that holds against the committed fixture,
which gfortran produced at `-g`, while WW3 runs at `-O3`. `docs/BITWISE_PROOF_202610.md` §2 and §4
had already measured the same Fortran lines giving different bits at `-O3`. One cause was ruled
out: building the Fortran with `-ffp-contract=off` changed nothing in `ww3_ts1`'s differences
`(v)`. The next check is one call at this grid, compared bit for bit against the `-O3` Fortran,
which `gen_snl1_ww3lib` (`kokkos/tests/fixtures`) can produce from a WW3 build. Until then the
ledger says L2, not bit-identical.

## 5. Where the time goes, and the port score

The ranking in `kokkos/PORT_STATUS.md` ("Port score", `just port-score`) is gprof on `bench_small`
(`just profile-case full`). On that case `W3SNL1`, the routine already ported, is the most
expensive (48 % of self time), then `W3SDS4` (23 %), `W3SIN4` (13 %), `W3SRCE` (6 %) and `W3SPR4`
(3 %); propagation is about 5 % `(v)`. That differs from the `ww3_ts1` table in `docs/data/`
(transcribed, `⚠`), where `W3SDS4` led: the share depends on the case, and the operational case is
not in this repository. By [ADR-0002](ADRs/ADR-0002-port-order-wall-time.md)'s rule, the next
routine on this case is `W3SDS4`, which #45 already plans.

The port score counts what passes the WW3 replay: about half of `bench_small`'s self time, 2 of
1 023 routines and 663 of 263 375 Fortran lines in `model/src` (`just port-score`). The time share
is the one that matters for a run; the other two say how little of WW3 that takes.

## 6. Why the Kokkos run is slower on a CPU

Phase 1 calls the shim once per sea point, inside an `!$OMP CRITICAL` section, and copies the
spectrum in and out at each call (`PATCH.md`, "Threading"). On one CPU thread that overhead takes
more time than the kernel saves: 217.9 s against 152.7 s on `bench_small` `(v)`. This is the
correctness step. Phase 2 hands the shim all points of a step at once and keeps the spectrum on
the device; until then no speed claim is made for `--hoff`, on the CPU or the GPU.

## 7. What was not checked

- **The CUDA run** `⚠`. The CUDA build is checked: `scripts/hoff.sh --hoff smoke --backend cuda
  --build-only` builds `ww_kokkos` with the `cuda-release` preset, configures WW3 with
  `nvcc_wrapper` (Kokkos 5.2.0 CUDA, Ada 8.9), and links `ww3_shel` to `libcudart.so.12` `(v)`.
  Running it here stops at Kokkos' start, "no CUDA-capable device is detected" `(v)`, and does so
  in both runs: with the `KOKKOS` switch, `W3INIT` starts Kokkos even when `WW_KOKKOS_SNL1=0`.
  `just smoke-hoff` on the RTX 4090 is the owner's test; the level-1 scratch on the GPU is
  untested.
- **MPI and threaded switches** `⚠`. The lab switch is SHRD (no MPI); the `!$OMP CRITICAL` path
  was not run with threads.
- **The operational case** `⚠`, for the ranking.
- **Bit-for-bit inside WW3** at `-O3`: open, §4.

## 8. What remains Fortran in this repository

The WW3 Fortran now comes only from WW3: `w3kokkosmd.F90` moved to the fork branch, and the tests
compile it from `WW3/model/src` (CI fetches that one file at the pinned commit). Still here:

- `kokkos/tests/fixtures/snl1_ref.F90`: a copy of WW3's `W3SNL1`, the L1 reference. Replacing it
  with `gen_snl1_ww3lib` (which links the real WW3 library) removes the last copy of WW3 Fortran
  from this repository; it needs a WW3 build in the test job.
- Test drivers (`gen_snl1_fixture.F90`, `shim_driver.F90`, `snl1_sea_state.F90`), the lesson-11
  interop example, and the course's own Fortran (`gpu/`, `examples/`, `exercises/`, `bench/`).
  These are not WW3 source; whether the book keeps them is the owner's call.
