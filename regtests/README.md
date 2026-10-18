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

`just regtest-selected [jobs] [all|matrix|smoke]` runs
[`scripts/regtest_selected.sh`](../scripts/regtest_selected.sh), which runs two commands:

1. `WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix <jobs> serial -R ':(ww3_tp2\.15|ww3_ts2):'`
2. `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident`

`WW3GPU_STRICT` stays `ON`. An entry fails when `test.comp` finds one file different. The
script writes its log and a copy of the matrix `summary.tsv` to `build/regtest-selected/`.

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
