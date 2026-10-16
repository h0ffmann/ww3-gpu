# Assessing the Kokkos port: does WW3 run with it?

Written 14 October 2026, at the owner's request: assess the existing port, make WW3 run with the
forked code on the Kokkos backend, and give two ways of running it with a smoke test and a more
expensive test each. `(v)` marks a claim checked by a command run for this document, on the
owner's workstation (RTX 4090) while its NVIDIA driver was not loaded (§7), with
the pratico toolchain (gfortran 15.3.0, Kokkos 5.2.0 OpenMP); `⚠` marks one that was not.

> **Later the same day, [ADR-0010](ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md).** The L1 fixture, `snl1_ref.F90`, `gen_snl1_ww3lib` and the other test drivers this assessment discusses were removed: the Kokkos port has no tests of its own, and WW3's regtests on the CPU (`just smoke-ww3-gpu`, `just full-ww3-gpu`, `just ww3-gpu-matrix`) are its only tests. The frozen fixture stays as the benchmark's input. Where §4 proposes a fixture from a WW3 build to settle why L2 is not byte-identical, that is now a question for the regtests. The assessment is otherwise kept as written.

## 1. Summary

Before today, the port was a kernel and a recipe. `W3SNL1` (the DIA) and its `INSNL1` tables
existed in `kokkos/`, matched a Fortran fixture one call at a time on three presets, and
`kokkos/src/fortran_iface/PATCH.md` described how to wire them into WW3. The recipe had never
been applied or compiled, and WW3 had never run with the kernel.

Today WW3 runs with it on the OpenMP backend, end to end `(v)`:

| | `--ww3` (WW3 as written) | `--ww3-gpu` (WW3 with the Kokkos DIA) |
|---|---|---|
| smoke: `ww3_ts1`, one point, 10 days | `just smoke-ww3`: ww3_shel 2.67 s | `just smoke-ww3-gpu` (CPU): 2.70 s Fortran DIA, 2.85 s Kokkos DIA; `hs` within 9.9e-8 relative, PASS |
| full: `bench_small`, 9 520 points, 12 h | `just full-ww3` | `just full-ww3-gpu` (CPU): 152.7 s Fortran DIA, 217.9 s Kokkos DIA; `hs` within 1.8e-7 relative over 119 652 values, PASS |

The Fortran comes only from WW3: the h0ffmann/WW3 branch `hoff/kokkos-snl1` (commit `6d1bcdfe`),
which the `WW3/` submodule pins. The CUDA build compiles and links `(v)`; the CUDA run on the owner's
RTX 4090 is the step that remains `⚠`.

## 2. How the two ways run

`scripts/ww3-gpu.sh` (`just smoke`, `smoke-ww3`, `smoke-ww3-gpu`, `full-ww3`, `full-ww3-gpu`):

- `--ww3` builds the `WW3/` tree with `switches/switch_lab_shrd` and runs the case once.
- `--ww3-gpu` builds and installs `kokkos/` as the `ww_kokkos` CMake package (preset
  `openmp-release` or `cuda-release`), builds the same tree with the switch plus `KOKKOS` and
  `-DWW_KOKKOS=ON`, runs the case twice with the same executable (`WW_KOKKOS_SNL1=0` and `1`),
  checks that the second log says "Kokkos DIA (W3SNL1) enabled.", and compares the gridded output
  with `nccmp-tol` (the L2 replay).
- `--cpu`, the default, runs Kokkos on OpenMP; `--gpu` runs it on CUDA and configures WW3 with
  `CMAKE_CXX_COMPILER=nvcc_wrapper`. `--ww3-gpu --gpu` runs only under the owner's supervision, a
  hard rule of AGENTS.md: it asks for a "yes" at a terminal and exits 7 without one. An NVIDIA GPU
  whose driver does not answer stops it with exit 4 and the cause.
- `--ww3-gpu --cpu` also builds and runs `--ww3` and compares the Kokkos run with it pair to pair:
  on `ww3_ts1` the result is the same as against the Fortran DIA of the KOKKOS build, `hs` within
  9.879e-8 relative, PASS `(v)`.

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
   (`kokkos/tools/bench_case`) and the fixture are fixed; `ww3-gpu.sh` turns hourly HS output on.

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

> **Update, 2026-10-15.** Closed: `pow11()` computed `AF11 = (SIG*TPIINV)**11` with the chain
> gfortran uses at `-O0` (`x^3*x^8`); at `-O3` GCC uses `x^5*x^6`. With the `-O3` chain the
> Kokkos DIA is byte-identical inside WW3 on all 100 matrix entries `(v)`
> ([`log/2026-10-14-snl1-bitwise-af11.md`](log/2026-10-14-snl1-bitwise-af11.md)).

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
the device; until then no speed claim is made for `--ww3-gpu`, on the CPU or the GPU.

## 7. What was not checked

- **The CUDA run** `⚠`. The CUDA build is checked: `scripts/ww3-gpu.sh --ww3-gpu smoke --gpu
  --build-only` builds `ww_kokkos` with the `cuda-release` preset, configures WW3 with
  `nvcc_wrapper` (Kokkos 5.2.0 CUDA, Ada 8.9), and links `ww3_shel` to `libcudart.so.12` `(v)`.
  Running it stopped at Kokkos' start, "no CUDA-capable device is detected" `(v)`, in both runs:
  with the `KOKKOS` switch, `W3INIT` starts Kokkos even when `WW_KOKKOS_SNL1=0`. The cause was
  the workstation, not the port: it had booted kernel 7.0.0-38, and the NVIDIA 595 module package
  was installed for 7.0.0-31 only, so no `nvidia` module could load `(v)`. `ww3-gpu.sh` now says so
  and stops, instead of falling back to the CPU when a GPU is present (`gpu_doctor`).
  `just smoke-ww3-gpu` on the RTX 4090 is the owner's test; the level-1 scratch on the GPU is
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

## 9. The third way: `--ww4`

The owner asked for a third way, WAVEWATCH IV from the WW4/ submodule (h0ffmann/WW4, `develop`,
`74ea792`), which returns an error code when a part the case needs is not ported yet.
`scripts/ww4_parts.py` reads the parts from the lab switch: the time loop, each source-term switch
that is on (LN1, ST4, NL1, BT1, DB1), and for `full` the UQ propagation solver. A part counts as
ported when its WW4 file has more than 60 lines of code, the threshold of `scripts/ww4_status.py`.
Today only the time loop is there (148 lines), so `just smoke-ww4` and `just full-ww4` stop with
exit 5 before any build `(v)`. Against the tree of WW4's open factory branch
(`FT-Runtime-Scheme-Selection-Design` of the `WW4_HLT` fork, at `afc40c2`), whose files are
placeholders, the same check reports each of them as a stub, 7 and 29 lines `(v)`.

`ww3-gpu.sh --ww4 smoke --build-only` builds WW4 from the fork in the pinned toolchain `(v)`. It needs
one flag: yaml-cpp 0.8.0, which WW4 pins, asks for a CMake older than 3.5, and CMake 4 refuses it
unless `CMAKE_POLICY_VERSION_MINIMUM=3.5` is given. No WW4 file is changed. The run itself, and its
input for each case (`kokkos/tests/fixtures/ww4/<case>/`, exit 6 while it is missing), wait for
the parts.
