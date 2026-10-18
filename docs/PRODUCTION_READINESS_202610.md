# Production readiness of the Kokkos runtime: GFS-Wave operations and an upstream second runtime

2026-10-18. Status: **gap list, read against the code, nothing run.** Issue
[#42](https://github.com/h0ffmann/ww3-gpu/issues/42), the port planner. Every WW3 line below is
at the fork pin `WW3@80644ba9` (h0ffmann/WW3, `ww3-gpu/kokkos-pnt`; `WW3-noaa/` is pure NOAA WW3
at `03adda91`, the fork's base), every `kokkos/` line at this repository's `db2c27c`. No build,
regtest or timing was run for this page: where a gap needs a run to be closed, the run is its next
step.

## The question

Two targets, judged separately:

- **(a) Operations.** GFS-Wave's configuration (a `ww3_multi` mosaic of three structured grids,
  50 x 36 spectral components, ST4 with the DIA, MPI on CPU nodes; the `GFS-Wave` and `WCOSS2`
  rows of [`GLOSSARY.md`](GLOSSARY.md), which carry their sources) runs with the Kokkos runtime in
  production and gives the Fortran's bits.
- **(b) Upstream.** NOAA's WW3 carries the port as an optional second runtime: a build switch
  (`KOKKOS`) plus run-time flags, the Fortran always selectable and the default.

Today's state, for the record: the resident preset (`SRCE,XYP3,KTP3,OUTG,IOPE,UPDV`,
`scripts/ww3-gpu.sh:190`) gave every output file byte-identical to the Fortran of the same
executable on `smoke` and `full`, CPU, OpenMP backend, `switch_lab_ompg` and `switch_lab_shrd`
(v, the `regtested` cell of `w3srcemd:w3srce` in `kokkos/ports.tsv`, which still says "no: no
regtest matrix yet"). 10 of the 310 rows of `kokkos/ports.tsv` are regtested and 60 are wired
(v, counted from the `regtested` and `wired` columns).

## Severity scale

| Mark | For (a), operations | For (b), upstream |
|---|---|---|
| **Blocker** | GFS-Wave cannot run the Kokkos path, or its bits are not shown equal | A reviewer upstream would refuse the change as it stands |
| **Major** | Runs, but a forecaster or an operator would be misled or slowed | Accepted only with a stated limit |
| **Minor** | A defect of convenience | The same |

## Summary

| # | Gap | (a) | (b) | Smallest next step |
|---|---|---|---|---|
| 1 | No regtest switch has the source-term set the Kokkos `W3SRCE` translates; NOAA's UFS regtests miss it too | Blocker | Blocker | Read GFS-Wave's operational switch file at its tag; run the `WW3GPU_LAB_SWITCH` matrix on the MPI and `ww3_multi` entries |
| 2 | `ww3_multi` writes `VA` on the host between `W3WAVE` calls with no sync; fixed under `SHRD` on 2026-10-18 by column syncs at the exchanges (section 2) | Minor ⚠ | Minor ⚠ | Move the pin to the fork branch; `mww3_test_08` under MPI when the owner runs MPI |
| 3 | MPI: four of the six resident parts are `SHRD` only, the preset stops on `DIST MPI`, the communicator is ignored, the device id is not per rank | Blocker | Major | A resident preset without `XYP3`, `OUTG`, `IOPE`, `UPDV`; one run on `switch_lab_mpi` |
| 4 | Bit parity measured with gfortran and g++ only; the operational compiler and the GPU are not measured | Blocker | Major | Static argument for the Intel build's math calls; one smoke with it |
| 5 | Build and packaging: a separate static library, C++20 and CMake 3.25; the install-only configure and the exact Kokkos check are done (U6) | Major | Major | A shared-library build and a recipe for the module systems of an operational centre |
| 6 | `WW_KOKKOS_ALL=1` and `W3KOKKOS_SRCE_ON` fall back to the Fortran without a word | Major | Minor | Done on fork branch `ww3-gpu/u2-strict-preset`, not yet at the pin: `WW_KOKKOS_STRICT=1` stops, otherwise one fallback-count line at exit; the matrix's Kokkos runs set it |
| 7 | Configuration is 61 environment variables, no namelist; the preset lives in a lab script | Major | Major | Smallest form done on `ww3-gpu/u2-strict-preset`: `WW_KOKKOS_PRESET=resident` from one table, `presets.txt`; next, a namelist group read by `W3INIT` |
| 8 | Observability: log lines on every rank, counters at exit to `stderr`, no provenance in the output files | Major | Minor | Gate the lines by `NAPLOG`; one NetCDF global attribute |
| 9 | OpenMP inside MPI: the per-point parts are serialised by `!$OMP CRITICAL`; the thread budget per rank is not set by the shim | Major | Minor | Measure the hybrid entries of `ww3_ufs1.1` before claiming a speed for them |
| 10 | Restart with the resident spectrum: covered by the sync calls, never regtested | Major | Minor | The restart-reproduction pair of `ww3_ufs1.1` (`work_c` against `work_d`) under the lab switch |
| 11 | `PDLIB` and unstructured grids: translated, not wired, gated out | Minor ⚠ | Major | None for GFS-Wave v16; a wiring unit for `PDLIB` when the owner asks |
| 12 | The fork patch: 4 082 lines of new Fortran, 819 changed lines in three WW3 files, 58 218 lines of C++ in another repository, most of it LGPL-3.0-or-later, some MIT, 76 files with no licence line | Minor | Blocker | ADR-0018 (Proposed) names the three places `ww_kokkos` could live; the owner chooses, after a legal reading ⚠ |
| 13 | A stale contract remark: "one spectral grid per process"; edited on the fork branch `ww3-gpu/u1-multi-va` (section 13) | Minor | Minor | Move the pin |

The sections below give the evidence of each gap. The ranked top five closes the page.

## 1. The source-term set: no regtest and no NOAA application switch passes the gate

The Kokkos `W3SRCE` is enabled only when the switch file has `BT1 DB1 LN1 MLIM` with `ST4 NL1`
and none of 44 other switches, among them `FLD1 FLD2 SEED NL3 PDLIB IC1`-`IC5 REF1 TR1` and the
`T*` test switches (v, `model/src/w3kokkosmd.F90:1767-1784`). Outside the set,
`WW_KOKKOS_SRCE=1` stops in `WW_SRCE_INIT` and `WW_KOKKOS_ALL=1` leaves `W3SRCE` to the Fortran
(v, `:1785-1796`).

Run against every switch file of the fork's regtests (v,
`python3 -I gate.py /home/hoffmann/code/ww3-gpu/WW3`, a 21-line script that applies the
`#if` chain of `:1770-1776` to each file's words; not committed, its logic is that chain):

| Switch file | Gate | Why |
|---|---|---|
| all 721 files under `WW3/regtests/*/input*/` | **0 pass** | `db2c27c`'s message: 47 `ST4 NL1` files lack `BT1 DB1 MLIM`, `ww3_tp2.15`'s lacks `MLIM` |
| `model/bin/switch_NCEP_st4` | pass | |
| `model/bin/switch_NCEP_st4sbs` | pass | |
| `regtests/ww3_ufs1.1`, `ww3_ufs1.2`, `ww3_ufs1.3` (all four files) | fail | no `LN1`; `SEED` and `FLD2` present |
| `model/bin/switch_NCEP_glwu` | fail | `NL3`, no `NL1` |
| `model/bin/switch_NCEP_st2` | fail | `ST2`, `FLX3` |
| `model/bin/switch_NCEP_gwm` | fail | no `MLIM` |
| `model/bin/switch_NWS_rwps` | fail | `IC4 PDLIB REF1 TR1` |

`matrix.base` calls `ww3_ufs1.1` its "Test of UFS applications with `ww3_multi` and grib2
output" (v, `WW3-noaa/regtests/bin/matrix.base:2152`). So the regtests NOAA keeps for the UFS
applications do not run the Kokkos `W3SRCE`, and the claim of
[`NOAA_OPS_KOKKOS_PLAN_202610.md`](NOAA_OPS_KOKKOS_PLAN_202610.md), "Why these routines", that
`ww3_ufs1.x` runs the lab switch's set is wrong on three words (`LN1` missing, `SEED`, `FLD2`
extra) (v, the four switch files read today). Which switch file the operational GFS-Wave build
uses is not in either submodule and was not read ⚠.

`db2c27c` gives the matrix a way through: with `WW3GPU_LAB_SWITCH=ON` each entry's source-term
words are replaced by `switches/switch_lab_shrd`'s, grid, forcing, propagation and outputs kept
(v, the commit message; `regtests/cmake/stage.cmake`). That tests the port on WW3's cases, not on
GFS-Wave's switch.

- **(a) Blocker.** If GFS-Wave's switch has `SEED` or `FLD2`, the Kokkos `W3SRCE` does not run
  there at all, and only the per-point parts (`SNL1`, `SDS4`, `SIN4`, `SPR4`) do.
- **(b) Blocker.** Upstream's regtests are the acceptance test; a runtime no regtest switch
  selects is not tested upstream.
- **Next step.** Read the switch of the GFS-Wave tag and record it here; if it has `SEED` or
  `FLD2`, their `W3SRCE` branches are the next translation (`ADR-0002` order permitting). Run
  `just ww3-gpu-matrix` with `WW3GPU_LAB_SWITCH=ON` on the 43 MPI entries the resident preset
  selects (v, `regtests/cmake/selftest.cmake:5-6`).

## 2. `ww3_multi` and the resident spectrum

The resident `VA` is one device copy per grid, keyed by `IGRID`, with a state that says which
copy is newer (v, `kokkos/src/fortran_iface/resident_shim.cpp:54-56`, `:116-139`). A host read
copies back only when the device is newer and leaves both copies equal; a host write then marks
the host newer (v, `:221-240`). `W3WAVE` ends with a host read (v, `model/src/w3wavemd.F90:3151-3153`),
so it returns with the state "both equal".

Between two `W3WAVE` calls of one grid, `ww3_multi` writes that grid's `VA` on the host: `WMIOHG`
(v, `model/src/wminiomd.F90:2264`) and `WMIOEG` (v, `:3261`, `:3278`, `:3326`, `:3330`), called
from `WMWAVE` around its `W3WAVE` calls (v, `model/src/wmwavemd.F90:965-973`, `:1191`, `:1303`).
No `wm*.F90` file of the fork calls a sync routine (v, `diff -rq WW3-noaa/model WW3/model` lists
only `w3initmd`, `w3srcemd`, `w3wavemd`, the six `w3kokkos*md.F90`, `CMakeLists.txt` and
`switches.json`). The first device kernel of the next step, `W3KOKKOS_KTP3` with `KTP3` on
(v, `w3wavemd.F90:1882-1888`) or the batched `W3SRCE` (v, `:2396-2408`), then reads a device copy
that lacks the boundary and nesting update. A host write before that kernel, which the Fortran
paths make, hides the defect, so whether a run shows it depends on the preset ⚠ (not run).
`WW_KOKKOS_VA_CHECK=1` would stop it at the next sync, since it compares the host array with the
snapshot of the last sync (v, `resident_shim.cpp:161-186`); it is a diagnostic, off by default.

GFS-Wave is a `ww3_multi` mosaic (the `GFS-Wave` row of [`GLOSSARY.md`](GLOSSARY.md)), and
`PATCH.md` lists `ww3_multi` as not covered by the resident units (v,
`kokkos/src/fortran_iface/PATCH.md:1133-1134`, `:1250`).

The per-point parts are not affected: they copy in and out on every call. The other contexts are
per grid too (v, `snl1_shim.cpp:83-89`), so several grids in one process are supported.

- **(a) Blocker** for the resident preset; none for the per-point parts.
- **(b) Blocker.** `ww3_multi` is a supported WW3 driver.
- **Next step.** Replace the read at `w3wavemd.F90:3153` with `W3KOKKOS_VA_HOST_WRITE`, which
  costs one host-to-device copy of `VA` at the next call; run the `mww3_test_01` to `_03`
  entries with `WW_KOKKOS_VA_CHECK=1` under the lab switch.

**Update, 2026-10-18 (plan 002, U1).** Reproduced and fixed under `SHRD`
([log](log/2026-10-18-ww3-multi-resident-va.md), `PATCH.md` section 26). With the lab switch
and the resident preset, `WW_KOKKOS_VA_CHECK=1` stopped `mww3_test_02`, `_03` and `_04`
(`grdset_a`), and without it they differed from the Fortran in 5, 10 and 4 files, with
`ww3_self` 0 (v). The step above was not taken: on a GPU it adds a whole copy of `VA` per grid
per call, which the owner refused. Instead each `ww3_multi` routine that reads or writes `VA`
between `W3WAVE` calls (`WMIOBS`, `WMIOHS`, `WMIOES`, `WMIOHG`, `WMIOEG`, and `WMWAVE`'s
unified point output) syncs only the columns of its own loop, and `W3WAVE` no longer reads
`VA` back at its return, since `ww3_shel` reads none after it. This adds `wminiomd.F90` and
`wmwavemd.F90` to the fork's diff. After, the six `mww3_test_01` to `_05` entries gave
`test.comp` 0 files different, also with the host copy poisoned, and no entry's whole-array
copies went up: `_04` went from 864 to 0 device-to-host copies, and `ww3_shel` full from 1 to 0
(v). Left open: `mww3_test_08` (MPI), whose six differing files the same defect explains by
reading ⚠, not run (ADR-0017); the fork branch waits for the pin bump.

- **(a), (b) now Minor** under `SHRD` ⚠: shown on the four `PR3 UQ` `grdset_a`-type entries and
  one moving grid, with the lab switch's source terms, not on GFS-Wave's grids.

## 3. MPI

*Scope, 2026-10-18:* out of scope by decision. The port is single node, and an MPI build keeps
the Fortran path ([ADR-0017](ADRs/ADR-0017-kokkos-port-is-single-node-mpi-stays-fortran.md),
Accepted). The findings below stay as the starting point for the ADR that would supersede it.

**One Kokkos runtime per process, not per communicator.** `WW_KOKKOS_INIT` takes an MPI handle
and discards it: "the communicator is accepted and ignored on purpose" (v,
`kokkos/src/fortran_iface/snl1_shim.cpp:196-199`); `W3INIT` passes `-1` (v,
`model/src/w3initmd.F90:754`). Kokkos is started once (`Kokkos::is_initialized()`, v,
`snl1_shim.cpp:205`) and each shim keeps its contexts in a process-global map. That is right for
one wave model per rank; it does not know which ranks share a node.

**Device per rank.** On a device backend the shim sets the device id itself, from
`WW_KOKKOS_DEVICE_ID`, and 0 when it is unset (v, `snl1_shim.cpp:132-145`, `:208`). So every
rank of an MPI job uses device 0 unless the launcher sets the variable per rank, and Kokkos' own
mapping (`map_device_id_by`, an initialization setting of Kokkos 5.2.2, v,
`include/impl/Kokkos_InitializationSettings.hpp:33` in the Nix store's `kokkos-cuda-5.2.2`) is
overridden. What Kokkos does by default without the explicit id was not read ⚠.

**The resident units are `SHRD` only.** `XYP3` needs `W3_SHRD` (v,
`model/src/w3kokkos_prop3md.F90:127-129`); `OUTG` refuses `W3_MPI` (v,
`model/src/w3kokkos_outgmd.F90:62-65`); `IOPE` and `UPDV` are "`SHRD`, no `MPI`" (v,
`PATCH.md:749-750`). The preset sets each to 1 explicitly (v, `scripts/ww3-gpu.sh:247`), and an
explicit flag on a switch its translation does not cover stops the model (v,
`w3kokkosmd.F90:1188-1203`). So `--kokkos resident` stops on `DIST MPI`. What does run under MPI:
`SRCE` (its gate does not exclude `MPI`), `KTP3`, and the per-point parts. Under MPI the whole
`VA` comes back to the host at every output time, once (v, `w3wavemd.F90:2655-2660`).

**What was measured.** The per-point ST4 parts ran byte-identical on 36 MPI entries of the
regtest matrix, 4 ranks, at `WW3@c9a6a0c6` (v,
[`docs/log/2026-10-17-st4-matrix-partial.md`](log/2026-10-17-st4-matrix-partial.md), table
row 1). The batched `W3SRCE` and the resident `VA` were never run under MPI: the `ports.tsv` cell
names `switch_lab_ompg` and `switch_lab_shrd` only (v), and `switches/switch_lab_mpi` exists but
no script, recipe or log entry names it (v, `grep -rn lab_mpi scripts justfile docs`).

**Failure under MPI** is loud: `EXTCDE` with a non-zero code calls `MPI_ABORT` on
`MPI_COMM_WORLD` (v, `model/src/w3servmd.F90:1177-1200`). In a coupled run that stops every
component, which is the right outcome for a wrong spectrum.

- **(a) Blocker.** GFS-Wave is `DIST MPI`; the preset that gave the measured speed-up cannot run
  there, and what can run was not run.
- **(b) Major.** Upstream would accept a CPU-`SHRD`-first runtime only with the limit stated.
- **Next step.** A preset `SRCE,KTP3` for `DIST MPI`; one `--ww3-gpu --cpu` run on
  `switch_lab_mpi`, then its MPI matrix entries. On a device, map the device id from the
  node-local rank or leave it to Kokkos when the variable is unset.

## 4. Bit parity: the compiler, the math library and the GPU

**Host compiler.** Every byte-identical result in `ports.tsv` names gfortran and g++ 16.2.0 (v,
the `regtested` cells). WW3's CMake builds Intel Fortran with `-no-fma -fp-model precise
-real-size 32` (v, `model/src/CMakeLists.txt:113-115`); the Fortran routine then calls Intel's
math library, while the C++ host branches call glibc (`kokkos/src/ww_kokkos/libm_device.hpp`
routes only the CUDA build, v, `kokkos/src/ww_kokkos/CMakeLists.txt:305-309`). Whether
`expf`, `logf`, `powf` give the same bits there was not checked ⚠. The compiler NCEP builds
GFS-Wave with was not read here ⚠. Mixing an Intel Fortran OpenMP runtime with a g++-built
Kokkos OpenMP backend in one process is a second unknown ⚠.

**GPU.** No device run has compared the device copies of glibc's functions (`WW_KOKKOS_LIBM`,
WFIP-0006) with the host, and every flag's parity note says "GPU ⚠ never run" (v,
`w3kokkosmd.F90:129-130`; `PATCH.md:749-750`). Until then a GPU run is judged by tolerance
(ADR-0014). WCOSS2 has no GPUs (the `WCOSS2` row of [`GLOSSARY.md`](GLOSSARY.md)), so for (a)
today the GPU is not the path; GFS-Wave's spectral grid, 50 x 36, also needs the DIA's level-1
scratch on the RTX 4090 (v, `kokkos/PORT_STATUS.md:63`), a speed matter, not a parity one.
The CUDA build of the fork for the resident preset compiles, build only, with no warning in a
wired part (v, [`log/2026-10-18-cuda-build-only.md`](log/2026-10-18-cuda-build-only.md)); the
owner's supervised run follows
[`GPU_SUPERVISED_RUN_CHECKLIST_202610.md`](GPU_SUPERVISED_RUN_CHECKLIST_202610.md).

**Real kind.** The interface is `REAL(C_FLOAT)`; "a WW3 built with an 8-byte default REAL needs a
different ABI" (v, `w3kokkosmd.F90:30-31`).

- **(a) Blocker** on the operational compiler: "bit for bit with the Fortran" is a claim about one
  executable, and that executable was never built.
- **(b) Major.** Upstream builds with GNU and Intel in CI ⚠ (not read today); a runtime proven on
  one of them needs that limit in its documentation.
- **Next step.** The static argument of ADR-0016 for the Intel build's transcendental calls in
  `W3SDS4`, `W3SIN4`, `W3SPR4`, `W3SNL1`; then one `smoke` with the Intel compilers.

## 5. Build and packaging

What a WW3 build needs today:

| Item | Evidence | Gap |
|---|---|---|
| A separately built, installed `ww_kokkos` | `find_package(ww_kokkos 0.9 REQUIRED)` (v, `WW3 model/src/CMakeLists.txt:43`) | Upstream has no place to fetch it from |
| A static archive only | `add_library(ww_kokkos STATIC)` (v, `kokkos/CMakeLists.txt:52`) | No shared build for a coupled system that links WW3 as a library |
| GoogleTest, NetCDF C and Fortran, a Fortran compiler to configure the library | `find_package(GTest REQUIRED)`, `pkg_check_modules(NETCDF ...)`, `NETCDFF` under `WW_KOKKOS_BUILD_TESTS` (default ON), and C and Fortran enabled only then (v, `kokkos/CMakeLists.txt`) | Closed (U6): `-DWW_KOKKOS_BUILD_TESTS=OFF` configures with C++ only and searches Kokkos only (see below) |
| C++20, CMake 3.25 | v, `kokkos/CMakeLists.txt:11`, `:14` | WW3 asks for CMake 3.19 (v, `WW3/CMakeLists.txt:4`) |
| Kokkos, pinned by the package | `kokkos/cmake/ww_kokkosConfig.cmake.in`: `find_dependency(Kokkos <Kokkos_VERSION> EXACT)`, then `Kokkos_DEVICES` and `Kokkos_ARCH` compared with the values recorded at build time (v) | Closed (U6): another Kokkos is a configure error of WW3's `find_package(ww_kokkos 0.9 REQUIRED)`, unchanged in the fork |
| A version contract | `SameMajorVersion` at 0.9.0, entry points only added (v, `kokkos/CMakeLists.txt:86-90`) | Sound for a 0.x series, but no symbol check enforces "only added" ⚠ |
| Contraction off in the kernels | `-ffp-contract=off`, `--fmad=false`, `PRIVATE` to `ww_kokkos` (v, `kokkos/src/ww_kokkos/CMakeLists.txt:297-303`) | Correct where it is; WW3's side does not check it was built this way |
| `ww3_lib` links `ww_kokkos::ww_kokkos` `PUBLIC` | v, `WW3 model/src/CMakeLists.txt:44` | WW3's exported targets (v, `:285-299`) then name a package a consumer must also find ⚠ (no consumer built) |
| `FLX5` refused with `ST4` | v, `WW3 model/src/CMakeLists.txt:33-35` | A stated limit |
| The toolchain | Nix flake `picasso/labs/pratico` (`docs/TOOLCHAIN.md`); Kokkos 5.2.2 | No recipe for the module systems operational centres use ⚠ |

- **(a) Major.** NCEP can build it by hand; it cannot pin it as one more library in its stack
  without the items above.
- **(b) Blocker.** An upstream optional dependency must configure with its own dependencies only.
- **State (U6, 2026-10-18).** `WW_KOKKOS_BUILD_TESTS=OFF` is the install-only configure
  (`kokkos/README.md`, "Install-only build"). Traced with `cmake --trace-expand` in the `#ww3`
  shell, it ran `find_package(Kokkos REQUIRED)` and, inside Kokkos' config,
  `find_package(OpenMP REQUIRED COMPONENTS CXX)`, and no other `find_package` or
  `pkg_check_modules`; `Kokkos_DIR` is the only package directory in its cache (v). The
  installed config records Kokkos 5.2.2, `OPENMP;SERIAL`, no architecture (v). WW3 built
  against that prefix with `WW3GPU_KOKKOS_PREFIX=<prefix> scripts/ww3-gpu.sh --ww3-gpu smoke
  --cpu` (v, 2026-10-18): WW3's cache names that prefix as `ww_kokkos_DIR`; the Kokkos and the
  Fortran runs of the same executable wrote byte-identical files (`out_grd.ww3`, `out_pnt.ww3`,
  `restart.ww3`, the NetCDF), and against `--ww3` every binary model file was byte-identical and
  `nccmp-tol` found 0 difference in every NetCDF field. No fork change was needed.
- **Next step.** A shared build of `ww_kokkos` for a coupled system, and the CMake and C++
  minimums against what an operational centre's module system provides ⚠.

## 6. Failure behaviour

| Event | What happens | Evidence |
|---|---|---|
| A shim call returns non-zero | `*** WAVEWATCH III ERROR IN W3KOKKOSMD : KOKKOS <routine> FAILED, CODE n`, then `EXTCDE(1)` | v, `w3kokkosmd.F90:1801-1812` and each `*_CHECK`; `w3srcemd.F90:1538-1542` for `W3SNL1`; `w3kokkos_residentmd.F90:82-86` for a sync |
| A C++ exception or `bad_alloc` | caught at the boundary, turned into a code | v, `srce_shim.cpp:637-645` and the other `catch` blocks |
| `WW_KOKKOS_INIT` fails | `EXTCDE(1)` with no `NDSE` line; the reason is on `stderr` only | v, `w3initmd.F90:754-755`; `snl1_shim.cpp:107-110` |
| `VA` reallocated or reshaped | the sync refuses, the model stops | v, `resident_shim.cpp:130-139` |
| A sync inside an OpenMP region | stops | v, `w3kokkos_residentmd.F90:72-76` |
| `WW_KOKKOS_<PART>=1` on a switch it does not cover | stops | v, `w3kokkosmd.F90:1197-1201`; `w3initmd.F90:834-837` |
| `WW_KOKKOS_ALL=1` on a switch a part does not cover | **Fortran, silently**; only the parts line shows `F` | v, `w3kokkosmd.F90:1196`, `:1785-1792`; `w3initmd.F90:830-833` |
| `WW_KOKKOS_SRCE=1` and a step with `IQTPE <= 0`, Liu's ice dispersion, or `TAUWBUG = 0` with `IT > 0` | **Fortran for that call, silently** | v, `w3kokkosmd.F90:1822-1830`; `TAUWBUG` is 1 by default (v, `model/src/w3gridmd.F90:1748`) |
| A device kernel aborts (`Kokkos::abort`, an illegal address) | the process dies outside `EXTCDE` ⚠ (not provoked) | |

Every failure the shim can see stops the model, under MPI on every rank. The two silent paths are
by design (the Fortran is the reference, so falling back keeps the answer), but an operator who
asked for Kokkos and got the Fortran learns it only from one log line or a `*_COUNT` total.

- **(a) Major.** A run that is slower than planned for no logged reason is an operational fault.
- **(b) Minor.** The default is the Fortran anyway.
- **Next step.** `WW_KOKKOS_STRICT=1`: `KOKKOS_PART` and the `SRCE` gate stop instead of falling
  back, and `W3KOKKOS_SRCE_ON` counts its Fortran calls for one line at the end of the run.
- **Done (2026-10-18), not yet at the pin.** Fork branch `ww3-gpu/u2-strict-preset` (`f0cbe70d`):
  `KOKKOS_PART` under `WW_KOKKOS_ALL=1`, the two `W3SRCE` set-up fallbacks and a new per-step gate
  `W3KOKKOS_SRCE_STEP` call `KOKKOS_FALLBACK`, which stops with `EXTCDE` and a `NDSE` line
  naming the part and the reason under `WW_KOKKOS_STRICT=1`, and otherwise counts; the shim
  prints `ww_kokkos: calls left on the Fortran (WW_KOKKOS_STRICT=1 stops instead): SRCE=<n> ...`
  at exit on every process, as the `*_COUNT` lines do. The flag is read once; the per-point
  `W3KOKKOS_SRCE_ON` is unchanged. The regtest matrix's Kokkos run sets `WW_KOKKOS_STRICT=1`
  (`regtests/cmake/ww3gpu_common.cmake`, `ww3gpu_kokkos_env`). `PATCH.md` section 24 has the
  runs that show it.

## 7. Configuration surface

The runtime is configured by environment variables only: 61 distinct `"WW_KOKKOS_*"` literals in
`kokkos/src` (v, `grep -rhoE '"WW_KOKKOS_[A-Z0-9_]+"' kokkos/src | sort -u | wc -l`), plus the
names `KOKKOS_PART` builds as `'WW_KOKKOS_'//PART` (v, `w3kokkosmd.F90:1178-1182`). The rule is
one function, `ww_kokkos_flag`: `WW_KOKKOS_<PART>` wins, then `WW_KOKKOS_ALL=1`, except 11
opt-in parts the master never turns on (v, `part_flags.cpp:34-46`). Nothing reads a namelist
(v, no `NAMELIST` in the six `w3kokkos*md.F90`), and the resident preset is a line of the lab
script (v, `scripts/ww3-gpu.sh:190`), so a production job card would repeat it by hand.

- **(a) Major.** An operational suite configures WW3 through its input files; environment
  variables set in a job card are easy to lose between implementations.
- **(b) Major.** Upstream's run-time configuration is namelists (`ww3_shel.nml`, `ww3_multi.nml`)
  ⚠ (convention, not a rule read today).
- **Next step.** A `&KOKKOS_NML` group read in `W3INIT` that sets the same flags, the
  environment kept as override for tests, and the effective set written once to the log.
- **Done in its smallest form (2026-10-18), not yet at the pin.** `WW_KOKKOS_PRESET=resident`,
  read by `W3INIT` through the shim (fork `237ca0de` on `ww3-gpu/u2-strict-preset`): the part
  list is one line of `kokkos/src/fortran_iface/presets.txt`, which CMake compiles into
  `part_flags.cpp` and `scripts/ww3-gpu.sh --kokkos resident` and the matrix's
  `WW3GPU_KOKKOS=resident` read, so they cannot drift. A set `WW_KOKKOS_<PART>` still wins, an
  unknown name stops the model, and the parts line stays the effective set, with the preset's
  name on the line before it. A job card then carries one variable instead of six. The
  namelist group remains the next step. `PATCH.md` section 25.

## 8. Observability

- **The parts line.** `W3KOKKOS_REPORT` writes one line with every part as `K` or `F` (v,
  `w3kokkosmd.F90:1239-1317`). It and the "enabled" lines are written to `NDSO` on every rank
  (v, `w3initmd.F90:767-848`), where WW3's own header is gated by `IAPROC .EQ. NAPLOG` (v,
  `:725`, `:905`).
- **Counters and timers.** `WW_KOKKOS_TIMERS=1` prints region totals at exit (v,
  `timers.cpp:48-60`); the `*_COUNT` variables print spectra per part, `WW_KOKKOS_VA_COUNT`
  the copies of `VA` (v, `resident_shim.cpp:85-88`, `:105`). All go to `stderr`, per process,
  without a rank in the line ⚠ (the format strings read have none).
- **Provenance.** No output module of the fork changed (v, the `diff -rq` of section 2), so a
  `ww3.*.nc` or `out_grd.ww3` does not say which parts produced it.

- **(a) Major.** An archived forecast must be traceable to the code path that made it.
- **(b) Minor.**
- **Next step.** Gate the lines by `NAPLOG`; prefix `stderr` lines with the rank; write the parts
  line as a global attribute of the NetCDF output.

## 9. OpenMP inside MPI, and the thread budget

The per-point calls run inside WW3's `!$OMP PARALLEL` loop under `OMPG` and are serialised by a
named `!$OMP CRITICAL` per unit (v, `w3srcemd.F90:1534-1537`; `w3kokkosmd.F90:1594`, `:1631`,
`:1679`, `:1861`); inside a region the shim runs on `Kokkos::Serial` (v, `snl1_shim.cpp:146-158`).
With `OMPG` on, the per-point parts therefore run one sea point at a time across all threads.
The batched `W3SRCE` replaces the whole loop and runs outside the region (v,
`w3wavemd.F90:2404-2415`), so it does not have this cost. NOAA's UFS regtests use `MPI OMPG
OMPH` (v, the `ww3_ufs1.x` switch files). The shim sets no thread count: `Kokkos::initialize`
receives only the device id (v, `snl1_shim.cpp:206-209`), so each rank takes what the OpenMP
environment gives it ⚠ (Kokkos' default not read today). The regtest driver divides the threads
among the ranks itself (v, `regtests/cmake/run_entry.cmake:73-77`); a production launcher has to
do the same.

- **(a) Major** for a hybrid layout; none for pure MPI.
- **(b) Minor.**
- **Next step.** Time `ww3_ufs1.1`'s `MPI_OMPH` entries under the lab switch, per-point parts
  against the Fortran, before any speed claim for a hybrid layout.

## 10. Restart with the resident spectrum

A restart read happens in `W3INIT`, before the resident `VA` is bound, so the first sync starts
from the host (v, `w3kokkos_residentmd.F90:44-45`). Each restart write is preceded by a host read
under `SHRD` (v, `w3wavemd.F90:2938-2941`, `:3037-3040`) and, under `MPI`, by the one read at
the top of the output block (v, `:2655-2660`), whose `IF` closes at `:3088` after both
`W3IORS ('HOT', ...)` calls. The read copies the device `VA` to the host and leaves both copies
equal, the device copy unchanged (v, `resident_shim.cpp:208-240`). So the restart file holds the
device's bits, and the run continues from the same bits. A reallocated `VA` after a restart stops
the model (section 6). Nothing here was run: `matrix.base` has a restart-reproduction pair for
the UFS case (`work_c`, then `work_d` from its restart, compared by `test.comp`; v,
`WW3-noaa/regtests/bin/matrix.base:2173-2176`), and no Kokkos run of it exists.

- **(a) Major** until that pair passes; GFS-Wave cycles from restarts.
- **(b) Minor.**
- **Next step.** That pair under `WW3GPU_LAB_SWITCH=ON` with the per-point parts and `SRCE`.

## 11. `PDLIB` and unstructured grids

29 rows of `kokkos/ports.tsv` translate `PDLIB` routines (`pdlib_w3profsmd`, `w3parall`); none
is wired (v, the `wired` column). The fork has no call into them: `PDLIB` appears in its Kokkos
modules only to exclude it (v, `w3kokkosmd.F90:1775`; `w3kokkos_prop3md.F90:123-125`; `w3kokkos_updvmd.F90:61`), and the
batched `W3SRCE` is compiled only `!defined(W3_PDLIB)` (v, `w3wavemd.F90:452`, `:2396`).
`switch_NWS_rwps` fails the gate on `PDLIB` among others (section 1).

- **(a) Minor** for GFS-Wave v16, which runs structured grids; a later implementation on an
  unstructured grid would make it a Blocker ⚠ (its grid was not read).
- **(b) Major.** Unstructured grids are a large part of WW3's use ⚠ (not counted).
- **Next step.** None until the owner asks; the order rule is ADR-0002's.

## 12. What upstream would receive

`diff -r` of the fork against `WW3-noaa/` (v, today): six new modules, 4 082 lines
(`w3kokkos*md.F90`), and 114, 456 and 249 changed lines in `w3initmd.F90`, `w3srcemd.F90`,
`w3wavemd.F90`, plus a `kokkos` category in `switches.json` and the CMake block of section 5. The
Fortran is LGPL-3.0-or-later (v, `w3kokkosmd.F90:33`). The C++ it calls lives here, not in WW3:
58 218 lines in 347 files under `kokkos/src/`, of which 249 files carry `LGPL-3.0-or-later` (the
kernels translated from WW3, all in `ww_kokkos/`), 22 carry `MIT` (the shims, timers, fixture
reader) and 76 carry no line (v, `grep SPDX-License-Identifier`, 2026-10-18). An earlier version
of this section called the C++ MIT; the count corrects it. The `#ifdef
W3_KOKKOS` hunks inside `W3WAVE`'s time loop are what a reviewer reads; with the `KOKKOS` switch
off they compile to the original.

- **(a) Minor.** Operations can run the fork.
- **(b) Blocker.** Upstream needs to know where the C++ comes from, who maintains it, and how its
  version is pinned.
- **State (U6, 2026-10-18).** [ADR-0018](ADRs/ADR-0018-where-ww-kokkos-lives-for-upstream.md),
  Proposed, sets out the three options (an external package WW3 finds, a submodule of WW3,
  sources inside `model/src`) with the size and licence of each; only the first was built.
- **Next step.** The owner's choice among them, after a legal reading of the licence of the
  translated C++ and of the files with no licence line ⚠.

## 13. A stale remark in the contract

`w3kokkosmd.F90:99-104` says "One spectral grid per process. WW_SNL1_INIT replaces the previous
grid's tables". The shim keeps one context per grid index in a map (v, `snl1_shim.cpp:83-89`), and
`W3INIT` calls `WW_SNL1_INIT` with `IMOD` (v, `w3initmd.F90:764`). The remark describes phase 1
and misleads a reviewer of (b). Remark b of the same block, on concurrent calls, is still true.

**Update, 2026-10-18 (plan 002, U7).** Remark a now says one context per grid, keyed by the grid
index, with `W3INIT` passing `IMOD`; remark b is kept (v, fork commit `f330dc53` on
`ww3-gpu/u1-multi-va`, `w3kokkosmd.F90:99-104`). Closed once the pin moves.

## Ranked top five

Ranked by what blocks both targets first, then by how much of GFS-Wave's run it decides.

1. **No regtest exercises the Kokkos `W3SRCE`, and NOAA's UFS application switches fail its
   gate.** 0 of 721 regtest switch files pass `w3kokkosmd.F90:1767-1784`; `ww3_ufs1.x` lack
   `LN1` and carry `SEED` and `FLD2`. Next: read GFS-Wave's switch; run the
   `WW3GPU_LAB_SWITCH` matrix on its MPI and `ww3_multi` entries.
2. **`ww3_multi` writes `VA` behind the resident copy.** `wminiomd.F90:2264`, `:3261-3330`
   write it between `W3WAVE` calls; `W3WAVE` returns with "both equal" (`w3wavemd.F90:3153`,
   `resident_shim.cpp:221-240`). GFS-Wave is a `ww3_multi` mosaic. Next: a host write at
   `W3WAVE`'s return, then `mww3_test_0x` with `WW_KOKKOS_VA_CHECK=1`. Update, 2026-10-18:
   fixed under `SHRD` with column syncs at the exchanges and no read at the return, the
   `mww3_test_01` to `_05` entries byte-identical (section 2); `mww3_test_08` (MPI) not run.
3. **The resident preset cannot run under `DIST MPI`.** `XYP3`, `OUTG`, `IOPE`, `UPDV` are
   `SHRD` only and the preset sets them explicitly, which stops the model
   (`w3kokkos_prop3md.F90:127-129`, `w3kokkosmd.F90:1197-1201`); the batched `W3SRCE` never ran
   under MPI; the device id is 0 on every rank unless set (`snl1_shim.cpp:132-145`). Next: an
   MPI preset `SRCE,KTP3` and one run on `switch_lab_mpi`.
4. **Parity is shown for one compiler pair only.** gfortran and g++ 16.2.0; WW3's Intel build
   (`model/src/CMakeLists.txt:113-115`) calls another math library; no GPU run compared the
   device libm. Next: the static argument for the Intel build's transcendental calls, then one
   smoke.
5. **Where the library lives for upstream is open.** Since U6 it installs with Kokkos as its one
   dependency and pins that Kokkos exactly (`kokkos/README.md`, "Install-only build"); it still
   needs C++20 and CMake 3.25, and its C++ lives outside WW3, mostly under WW3's licence.
   Next: the owner's choice among the options of ADR-0018, after a legal reading.

## How the checks were made

- Switch gate: the `#if` chain of `w3kokkosmd.F90:1770-1776` applied to the words of each file
  under `WW3/regtests/*/input*/switch*` and `WW3/model/bin/switch_*` (a throwaway Python script,
  2026-10-18).
- Fork footprint: `diff -rq WW3-noaa/model WW3/model` and `diff WW3-noaa/model/src/<f>.F90
  WW3/model/src/<f>.F90 | grep -c '^[<>]'` for the three changed files.
- Ledger counts: the `wired` and `regtested` columns of `kokkos/ports.tsv` at `db2c27c`.
- Everything else: the files and lines cited, read on 2026-10-18.
