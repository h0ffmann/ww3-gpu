# Handover: the `goal/42-gpu-fast` integration branch, stopped for another session

2026-10-17, an agent for M. Hoffmann; issue #42,
[WFIP-0005](../WFIPs/WFIP-0005-device-resident-step.md),
[ADR-0016](../ADRs/ADR-0016-speed-first-regtests-postponed-one-person-loop.md). The owner asked
to stop all lanes and leave the work on branches for the next session.

## Goal

Make `--ww3-gpu --gpu --kokkos resident` faster than pure WW3 (`--ww3`) on the same case, with
every output byte-identical to the Fortran on the CPU. The integration branch is meant to land as a
single pull request, opened with `just pr`. No pull request was open when the work stopped.

## Branches

| Branch | State | What it holds |
|---|---|---|
| `goal/42-gpu-fast` | rebased on `main` `ed5ee97d`, pushed, gates green | lanes P, R, S and a doc fix (below) |
| `goal/42-gpu-step-srceteam` | `12685ff4`, on `b38b5fc8`, **WIP, not on the integration branch** | lane Q: W3SRCE split over a team on a device |
| `goal/42-gpu-step-tabu`, `-gpudef`, `-gpudef2`, `-mutate` | folded into `goal/42-gpu-fast` | the lanes' own branches; can be deleted after the PR merges |

What `goal/42-gpu-fast` adds to `main`, oldest first:

- **ST4 table rebuild on a host `parallel_for`**: `TABU_STRESS`, `TABU_TAUHF` and `TABU_TAUHF2`.
  `st4.tables.call` takes 7.57 s before and 0.44 s after on smoke. Since `d6b321af` the rebuild
  is opt-in: ask for it with `--kokkos SRCE,ST4_TABLES`.
- **Device defaults** (PATCH.md section 19):
  - W3XYP3 runs one team of 128 threads per component on CUDA;
  - W3SRCE batches are 128 MiB chunks on a device, for the display watchdog;
  - a `cuda-h100-release` preset, which needs a HOPPER90 Kokkos that picasso does not build yet;
  - ptxas tables, the W3OUTG rows measured again after #180.
- **Mutation audit** (PATCH.md section 21,
  [2026-10-17-mutation-audit.md](2026-10-17-mutation-audit.md)):
  - `ww3-gpu.sh` now compares every model file byte for byte and checks the coast call counts;
  - `WW_KOKKOS_VA_CHECK`, `WW_KOKKOS_VA_POISON` and `WW_KOKKOS_OUTG_STAGED`;
  - a wider coast fixture;
  - `L1_test_timers`, a test of the timers tool, not of a ported routine.
  - **The script change makes a comparison gate stricter**, so a person reviews it before it
    merges (AGENTS.md, "Never cheat a regtest tool").
- The TABU port reports and the ST4_TABLES note now give the opt-in command.

## Checked on `goal/42-gpu-fast` (before the last rebase, at `95074373`)

The last rebase onto `ed5ee97d` brought only docs, diagrams and tooling. No file under `kokkos/`
or `scripts/ww3-gpu.sh` changed, so these were not rerun ⚠:

- **Gates**: `bash -n` and shellcheck of `ww3-gpu.sh`, `--self-test`, `just kokkos-test
  serial-debug` and `openmp-release` (11/11 each), and `port_score --check`. The
  `security-reviewer` found nothing at confidence 8 or above.
- **Parity**, under the stricter script, CPU, OpenMP, 32 threads, shared machine:
  - (v) Every model file is byte-identical to the Fortran of the same executable.
  - (v) Every model file but the NetCDF is byte-identical to `--ww3`. The NetCDF differs only in
    its `WAVEWATCH_III_switches` attribute.
  - The runs were smoke, full and coast with `--kokkos resident`, coast with `all`, coast with
    `SRCE,ST4_TABLES` ("0 values differ"), and coast with all diagnostics on. Also coast with
    `WW_KOKKOS_XYP3_TEAM=16` and `WW_KOKKOS_SRCE_BATCH_MIB=128`, and coast and full on
    `switch_lab_shrd`.
- **Speed**: one run each on a loaded machine, so the figures are not for a table (regtest pending,
  ADR-0016) ⚠. Resident against `--ww3`: full 1.95x, coast 2.18x, smoke 0.88x.

## Not done

- **Nothing ran on a GPU.** The CUDA bits, launch times, the watchdog limit and the GPU's speed
  against `--ww3` are unknown. The owner's supervised commands:
  `WW_KOKKOS_SRCE_BATCH_MIB=32 bash scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos resident
  --timers`, then `just perf-ww3-gpu full 3 resident`.
- **Lane Q is unfinished**: `goal/42-gpu-step-srceteam` has `w3srce_split` with
  `srce_*_team.hpp` and `st4_*_team.hpp`. CPU parity passed with forced team sizes 4 and 16.
  - Three mutants survived even though the split kernel ran: skip the last update, an unrecorded
    one, and `if (is == 0) return;` in the update loop. They ran under the old script, which
    judged only the NetCDF ⚠.
  - Next steps:
    - rebase onto `goal/42-gpu-fast`;
    - rerun the mutants under the byte checks;
    - if they still survive, find why before trusting the kernel;
    - then squash, review and fold it in.
- **Decisions for the owner**, each its own PR:
  - a team version of `ww_ktp3_batch` (its `CFLTHMAX`/`CFLKMAX` maxima depend on order);
  - a team version of W3OUTG;
  - the XYP3 averaging scatter in parallel;
  - W3OUTG's point kernel at 161 registers after #180.
- **Still unprovable on the CPU** (mutation audit):
  - the device copies;
  - the prop3 input cache (no `WW_KOKKOS_PROP3_STAGED` yet);
  - the device libm route;
  - three paths no case takes.

## Traps met in this round

- **Never `pkill -f` by pattern**: a lane's `pkill -f "ww3-gpu.sh --ww3-gpu coast"` killed another
  lane's run. Kill only the PIDs you started.
- **Never run `git remote set-url` in the superproject**: it pointed `origin` at upstream for
  18 minutes. `origin` must stay `git@github.com:h0ffmann/ww3-gpu.git`.
- **A fresh worktree has empty submodules**: clone `WW3`, `WW3-noaa` and `picasso` from another
  worktree, then check out the gitlink commit.
- **The script unsets every `WW_KOKKOS_<PART>`**: ask for parts with `--kokkos <list>`. Only
  diagnostics and tuning variables pass through.
- **`WW3GPU_SWITCH`** takes the bare name (`switch_lab_shrd`), not a path.
- **Generated files**: after a rebase, `PORT_STATUS.md` and `site/data/port_score.tsv` conflict;
  take either side, then run `python3 scripts/port_score.py --write`.
