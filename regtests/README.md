# regtests

This directory is the CMake/CTest driver that runs WW3's own regtests twice on one executable:
the Fortran and the Kokkos port. `test.comp` and `nccmp-tol` compare the pair.
[WFIP-0003](../docs/WFIPs/WFIP-0003-kokkos-regtest-matrix.md) is the design, and the header of
[`CMakeLists.txt`](CMakeLists.txt) lists every option. The `just ww3-gpu-matrix*` recipes run it.

## Selected regtests

The selected regtests are a small set of WW3 regtests that guard the resident Kokkos preset
(`--kokkos resident`: `SRCE`, `XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV`) during development. They
use the existing driver and its acceptance criteria. They add no test and no verdict.

### What runs

`just regtest-selected [jobs] [selected|matrix|smoke]` runs
[`scripts/regtest_selected.sh`](../scripts/regtest_selected.sh), which runs two commands:

1. `WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix <jobs> serial -R ':(ww3_tp2\.15|ww3_ts2):'`
2. `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident`

`WW3GPU_STRICT` stays `ON`. An entry fails when `test.comp` finds one file different. The
script writes its log and a copy of the matrix `summary.tsv` to `build/regtest-selected/`.

`selected` (the default) runs both. `matrix` and `smoke` run one of them. The matrix entries run
`jobs` at a time, in parallel (`ctest -j`), and each entry gives one verdict, pass or fail, in
`summary.tsv`. The default `jobs` is 5, so all selected matrix entries run together, each with
`nproc/5` threads. The slowest entry (`-g 5km`) then sets the time of the matrix. The `smoke`
pair runs after the matrix, not at the same time: both build and install the Kokkos package in
`kokkos/build/openmp-release`, and two builds there at once can break each other. The pair takes
about one minute.

### What the selection covers

`selected` is not WW3's whole matrix. It is the part of `matrix.base` that runs the code of the
resident preset, at the pin `WW3/` `80644ba9`:

| Set of `matrix.base` entries | Entries | Share | How it was counted |
|---|---|---|---|
| All entries | 758, in 60 tests | 100 % | ⚠ lines that `matrix.base` writes with `$rtst`; a machine section can turn some off |
| With the DIA (`NL1`) | 310 | 41 % | (v) `just ww3-gpu-matrix-list all` |
| With ST4 physics (`ST4`) | 136 | 18 % | (v) `WW3GPU_ROUTINES=SDS4 just ww3-gpu-matrix-list all` |
| That the resident preset can select (`ST4 NL1 PR3 UQ SHRD`, no `PDLIB MGP MGG`) | 7 (7 serial, 0 OpenMP, 0 MPI) | 0.9 % | (v) `WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix-list serial`, `omp`, `mpi`, `all` |
| **Selected** | **5**, plus the `ww3_ts1` pair | **0.7 %** | the 7 minus the 2 unstructured-grid entries that the preset refuses by design |

All counts are from 2026-10-18. The other entries do not run the preset's code: a Kokkos run of
them is the Fortran again, or the preset stops by design (MPI, unstructured grids, moving grids).
A full run of the 310 `NL1` entries takes days. The selection runs in hours (see "Time").

### How much of the Kokkos port the selection runs

The resident preset with the lab switch's source terms (`ST4 NL1 LN1 BT1 DB1 MLIM`, `PR3 UQ`)
runs 16 rows of `kokkos/ports.tsv`. The shares below use the same inputs as
`scripts/port_score.py`: `kokkos/ports.tsv`, the gprof profile `docs/data/ww3_full_gprof.tsv`
(the `full` case, Fortran, 131.4 s of self time) and the measured WW3 size at the pin.

| Measure | Selection | Translated (all rows) | Ported (`wired` yes) |
|---|---|---|---|
| Routines | **16** | 310 (the selection: 5.2 %) | 60 (the selection: 27 %) |
| Run time of the `full` profile | **99.0 %** | 99.3 % | 99.3 % |
| WW3 lines of these routines | **8 742** of 263 375 (3.3 %) | 67 699 (25.7 %) | 17 022 (6.5 %) |
| Kokkos C++ of these rows | **9 143 lines** in 28 files (15.6 % of the 58 497 lines of `kokkos/src`) | 51 746 lines in 320 files | 15 062 lines in 69 files |

(v) Computed 2026-10-18 from those files. ⚠ The C++ count takes the files that each row names
in its `kokkos` column; shared headers that every kernel includes (`real.hpp`,
`libm_device.hpp`, the timers) and `resident_shim.cpp` are not in it. So the selection runs
almost all of the measured run time, in a small part of the code. The other 294 translated rows,
for example ST1, ST2, ST3, ST6, the other flux and bottom terms and the I/O routines, run in no
selected entry.

The 16 routines, by their share of the `full` profile:

| Routine | Part | Share of the profile | WW3 lines | `regtested` in `kokkos/ports.tsv` |
|---|---|---|---|---|
| `W3SNL1` (DIA) | `SRCE` | 48.4 % | 359 | yes |
| `W3SDS4` | `SRCE` | 23.2 % | 617 | no |
| `W3SIN4` | `SRCE` | 12.7 % | 581 | no |
| `W3SRCE` | `SRCE` | 5.6 % | 2 223 | no |
| `W3SPR4` | `SRCE` | 3.1 % | 243 | no |
| `W3XYP3` | `XYP3` | 3.0 % | 837 | no |
| `W3QCK3` (UQ, inside `W3XYP3`) | `XYP3` | 1.1 % | 412 | no |
| `W3QCK1` (inside `W3KTP3`) | `KTP3` | 0.9 % | 364 | no |
| `W3SDB1` | `SRCE` | 0.4 % | 287 | no |
| `W3KTP3` | `KTP3` | 0.3 % | 443 | no |
| `W3SBT1` | `SRCE` | 0.2 % | 198 | no |
| `W3SLN1` | `SRCE` | 0.1 % | 156 | no |
| `INSNL1` (the DIA's tables, at set-up) | `SRCE` | 0.0 % | 304 | yes |
| `CALC_USTAR` | `SRCE` | 0.0 % | 163 | no |
| `W3QCK2` (inside `W3KTP3`, only when `FLCK` is on) | `KTP3` | 0.0 % | 375 | no |
| `W3OUTG` | `OUTG` | 0.0 % | 1 180 | no |

⚠ `W3QCK2` runs only in an entry with `FLCK` on; which selected entries set it was not read.
`IOPE` and `UPDV` are not routines of the ledger: they keep the host copy of `VA` current for
`W3IOPE` and `W3UICE` (`kokkos/src/fortran_iface/resident_shim.cpp`). A pass of the selection is
the evidence that can move these `regtested` cells to "yes".

### The ww3-gpu code each entry runs

| Entry | Kokkos parts on (`K` in the log) | ww3-gpu code it runs |
|---|---|---|
| `ww3_tp2.15`, four variants | `SRCE`, `XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV` | `kokkos/src/fortran_iface/srce_shim.cpp` with the ST4 and DIA kernels (`kokkos/src/ww_kokkos/st4_*.hpp`, `snl1_team.hpp`, `srce_kernel.hpp`); `prop3_shim.cpp` (`prop_xyp3.hpp`, `prop_ktp3.hpp`); `outg_shim.cpp` (`iogo_w3outg.hpp`); `resident_shim.cpp` (`VA` on the Kokkos side, `IOPE`, `UPDV`) |
| `ww3_ts2` `ST4_PR3_UQ` | the same six | the same files |
| `ww3_ts1` (smoke) | the same six are on | one sea point: `W3SRCE` 2 161 spectra and 2 161 partial host reads of `VA` (`IOPE`); `W3XYP3`, `W3KTP3` and `W3OUTG` make 0 calls |

(v) The `K` flags and the `W3SRCE` spectra (6 900 for `ww3_tp2.15`, 4 632 for `ww3_ts2`) are in
the record of 2026-10-18; the `ww3_ts1` counts are from a run of the same day. ⚠ The record does
not count the calls of `XYP3`, `KTP3`, `OUTG`, `IOPE` and `UPDV` per entry, and three
`ww3_tp2.15` variants have not run yet.

### Threads and parallel entries

The routine is sized for one machine, the owner's workstation, which is also its only runner:
an Intel Core i9-14900 (24 cores: 8 performance cores with two threads each and 16 efficiency
cores, 32 hardware threads), 184 GiB of memory that Linux sees (192 GB installed) and 6 TB of
free disk (v, `lscpu`, `free -g`, `df -h`, 2026-10-18). On it:

| Setting | Value | Why |
|---|---|---|
| `jobs` | 5 | the 5 matrix entries run together; the longest one sets the time |
| threads per entry | 6 (32 / 5) | 30 of the 32 hardware threads; 2 stay for the shell, the runner and the I/O |
| memory | no limit set | ⚠ not measured per entry; the regional cases of the selection need a few GB each, far below 184 GiB |
| builds | 5 x `make -j 8` | 40 compile processes on 32 threads, for a few minutes at the start |

On another machine, pass `--jobs` (or the `jobs` input of the workflow) so that `jobs` is at most
the number of hardware threads.

Each entry gets `nproc/jobs` threads (`OMP_NUM_THREADS`, from the `ww3-gpu-matrix` recipe), so
`jobs` entries together use at most `nproc` threads. With 5 jobs on 32 hardware threads, each run
has 6 threads and 30 are in use. `OMP_PROC_BIND` is `false` in every run
(`regtests/cmake/run_entry.cmake`), so the entries do not pin their threads to the same cores.
`scripts/regtest_selected.sh` refuses a `jobs` larger than `nproc`: then each entry gets 1
thread, and the runs compete for cores.

Two cautions:

- WW3's `run_cmake_test` builds each entry with `make -j 8`, whatever `jobs` is. While 5 entries
  build at the same time, 40 compile processes share the machine. This makes the build slower.
  It does not change a verdict.
- The runner is the owner's workstation. Another job on it takes cores from the entries, and
  libgomp threads that wait spin by default. Do not dispatch the routine while a timing runs.

### Why these entries

With the resident preset, the driver selects only switches with the words `ST4 NL1 PR3 UQ SHRD`
and without `PDLIB MGP MGG`. At `WW3/` `80644ba9` this gives 7 serial entries, 0 OpenMP entries
and 0 MPI entries (v, `WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON` and a configure of this
directory with `ctest -N -L entry`, 2026-10-18). `WW3GPU_LAB_SWITCH=ON` is necessary: the Kokkos
`W3SRCE` refuses every `matrix.base` switch, because no switch has `BT1 DB1 LN1 MLIM`
([log](../docs/log/2026-10-18-resident-regtest-lab-switch.md)).

| Entry | What it guards | Last result |
|---|---|---|
| `ww3_tp2.15` `-w work_PR3_UQ` | wind-wave growth in the Adriatic Sea with COSMO-ME winds and space-time extremes output: `W3SRCE` and `PR3`+`UQ` propagation on a real coast | (v) pass, `test.comp` 0/23, 92 s |
| `ww3_tp2.15` `-g 5km` | the same case at full resolution on the whole Adriatic Sea, the largest grid of the set | ⚠ not run; the test's `info` recommends MPI for it, so it is the slowest entry |
| `ww3_tp2.15` `-i input_const` | the same case with the inputs of `input_const` | ⚠ not run |
| `ww3_tp2.15` `-i input_rho` | the wind-driven run that reads, interpolates and writes wind stress and air density (the test's `info`) | ⚠ not run |
| `ww3_ts2` `ST4_PR3_UQ` | fetch-limited growth, the classic source-term balance test | (v) pass, `test.comp` 0/22, 85 s |
| `ww3_ts1` (smoke) | the timed configuration with `OMPG`, which no `matrix.base` entry covers | (v) pass: every model file byte-identical to the Fortran of the same executable |

The "last result" column is from
[`docs/data/ww3gpu_matrix_resident_lab_2026-10-18.tsv`](../docs/data/ww3gpu_matrix_resident_lab_2026-10-18.tsv)
on an i9-14900 with 4 jobs and 8 threads per run. The time of a matrix entry includes its WW3
build.

The regex selects by test name, so a WW3 pin bump that renumbers `matrix.base` does not change
the selection. Two of the 7 entries are not selected: `ww3_tp2.6` `ST4` and `ST4_ASCII` run on an
unstructured grid, and `XYP3` refuses it by design. Their Kokkos runs stop, and the strict verdict
fails them.

### Time

⚠ An estimate, not a measurement. Two of the five matrix entries took about 90 s each with 4
jobs. The `5km` entry is larger and runs serial, so it can take much longer. Add the build of the
Kokkos port and the smoke pair (two WW3 builds and two short runs). On the i9-14900 the set is
expected to finish in one to three hours. The first run on a new machine also downloads the
pinned toolchain into the Nix store. The workflow stops after 300 minutes.

### On the self-hosted runner

[`.github/workflows/regtest-selected.yml`](../.github/workflows/regtest-selected.yml) runs the
same script on the owner's machine. It starts only by hand (`workflow_dispatch`), only for the
repository owner, and never on the GPU. It uploads `build/regtest-selected/` as an artifact.

To set up the runner, do these steps on the owner's machine, outside a Nix shell:

1. Log in to GitHub with `gh auth login`. The account must be an administrator of the
   repository.
2. Make sure that `nix`, `just`, `git` and `curl` are on `PATH`. The runner keeps the `PATH` of
   the shell that registers it.
3. Run `just runner-register --dry-run`, and read the plan.
4. Run `just runner-register` just before you dispatch the workflow. The script downloads the
   pinned `actions/runner` release, checks its SHA-256, registers it with the label `ww3-gpu`
   for one job (`--ephemeral`), and starts it as a systemd user service.
5. Dispatch `regtest-selected` from the Actions tab. The runner takes that job and then
   unregisters itself. Register again before the next dispatch.
6. Run `just runner-status` to see the service and the state that GitHub reports.

Warning: do not keep a runner registered between dispatches. The repository is public. A pull
request from a fork can add its own workflow that asks for the `ww3-gpu` label, and GitHub runs
the pull request's workflow files, not this repository's. The job then runs on this machine as
your user, with access to your SSH keys and your `gh` token. The ephemeral registration limits
that window to the time between `just runner-register` and the dispatch. Also set the
repository's Actions setting "Approval for running fork pull request workflows" to "Require
approval for all external contributors".

`just runner-remove` stops the service, removes the registration from GitHub and deletes the
runner directory. The tokens come from `gh api` at run time, and the script does not write them
to a file. `scripts/runner.sh check-workflows` fails if a workflow of this repository that uses
`self-hosted` has a trigger other than `workflow_dispatch`, or an expression in `runs-on`. It
cannot see a workflow that a fork's pull request adds; the ephemeral registration is the guard
against that. The pattern comes from
`marola-dev/marola-devkit` (`scripts/setup-runners.sh`, `scripts/gha-runner.sh`) and the
self-hosted rule of `marola-dev/marola`.
