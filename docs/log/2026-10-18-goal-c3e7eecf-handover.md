# Handover of goal `c3e7eecf`: the NOAA record, the ten-run median, points in lanes up to `W3SIN4`

2026-10-18, an agent for M. Hoffmann (Claude Code session `ec4548a3-46a8-46e2-be0a-2d74c3e7eecf`).
What this session finished, what is open, and what it learned, for whoever continues. It picks up
where [the `abbb0456` handover](2026-10-18-goal-abbb0456-handover.md) stops; every claim names its
record, and nothing here was re-run for this entry.

## What worked (merged)

| Result | Record |
|---|---|
| The NOAA progress record after #191: pin `50a10730`, seven preset parts with `CPRT`, strict mode, the NaN fill, one copy of `VA` to the device on `full`, 22 regtest entries with 0 files different and the four FAIL rows kept, a status table up front; the READMEs link it; gaps 6, 7 and 13 of the readiness list marked at the pin | PR #197; `docs/NOAA_PROGRESS_RECORD_202610.md`, `docs/PRODUCTION_READINESS_202610.md` |
| The record's speed figure: median of ten alternated runs on `main`'s code (`5cd0140`), `full`, 32 threads, NOAA WW3 26.18 s, Kokkos resident 15.84 s, 1.65 times less wall time, every output byte-identical to run 1's; regtest pending ⚠ (22 entries, not the whole matrix) | PR #197; the record, section 3 |
| Plan 005, sea points in the vector lanes of the batched `W3SRCE`, with its per-lane parity argument and units S0 to L7 | `docs/plans/2026-10-18-005-srce-points-in-lanes-plan.md` |
| S0: one thread of the resident batch is 51.1 % scalar per-point code lanes can reach, 39.0 % DIA, 8.9 % libm and libmvec | `docs/log/2026-10-18-srce-lanes-s0-profile.md` |
| L1: `SrcePoint` sections with the kernel's bodies moved verbatim, `w3srce_lanes`, `WW_KOKKOS_SRCE_LANES` (1 to 8, host only; default 1 in #197, 8 since #199); `smoke` and `full` byte-identical with 1 and 8 | `kokkos/port_report/W3SRCE_KOKKOS.md`, "Points per team" |
| PR #198's conflict with `main` resolved by a merge (no history rewritten) | merge commit `41f4197` |

## Open work

| Item | State | Next step |
|---|---|---|
| PR #199, plan 005 L2: `W3SIN4`'s stress scan on 8-wide vectors | `smoke`, `full` byte-identical with 8 points per team; one-thread kernel 105.17 s to 99.77 s (5.1 %); CUDA `--build-only` clean; merged with `main` | the regtest matrix with `WW_KOKKOS_SRCE_LANES=8` (the matrix passes the variable through to its Kokkos runs, `regtests/cmake/ww3gpu_common.cmake`, `ww3gpu_kokkos_env`); the owner's review of the loop nesting and of the explicit vector types (the PR body) |
| Plan 005 L3: `W3SDS4` on lanes | not started; 24.8 % of the batch in S0, the largest unit left | split `w3sds4_point` as L2 split `W3SIN4`; the five libmvec sites (`st4_libmvec.hpp`) stay a loop over the lanes, each lane calling today's grouped code; time it pinned (`taskset -c 2`) as L2 was |
| Plan 005 L4 to L7 | not started | the integration and `w3srce_team` (9.9 %), `W3SPR4` and the small terms, the DIA layout, then a median of three at 32 threads on an idle machine |
| The opportunity and GFS-Wave analysis of 2026-10-18 | written by a sub-agent into the session's scratch directory, not committed; three of its claims checked (the `Kokkos::single` sites, the batched `W3SRCE` switch gate refusing `FLD2` and `SEED` and needing `LN1`, `7a23cd77` being plan 004's GFS.v17 commit) ⚠ the rest not checked | if wanted, redo it into `docs/plans/`; its order for GFS-Wave was: read GFS.v17's switch, grid and compiler from the UFS weather model; plan 004; `W3FLD2` and the `SEED` and no-`LN1` paths in the batched `W3SRCE`; a `SHRD` build of `ww3_ufs1.1`; profile, restart pair, the Intel compiler; an ADR before any MPI work (ADR-0017) |

## Waiting on the owner

- The decisions plan 005 names: whether the host may call the transcribed vector libm
  (`libm_device.hpp`) inside lane loops, and whether a sampled check is enough for `powf` and
  `atan2f` (`docs/GPU_LIBM_BITS_202610.md`); whether to skip `W3SDS4`'s zero weights (not
  bit-neutral: `0 x Inf` is NaN).
- The restart pair (`scripts/restart_pair.sh`), still never run.
- The `bench/results/` record of the ten-run median, once WW3's regtests have passed on `5cd0140`
  beyond the 22 entries.
- Whether the regtests of #191 move the `regtested` cells of `kokkos/ports.tsv` for the resident
  parts to "yes"; the ledger still says "no".
- From the `abbb0456` handover, still open: the strict-OUTG/POISON follow-up branch
  (`goal/abbb0456-8241-strict-outg-poison`, no PR), the Fortran A/B harness branch, #196.

## Left on disk, not in the repository

- `~/code/ww3-gpu-2/noaa-timing.sh` (untracked): the timing of the record, `bash noaa-timing.sh
  [N]`, N alternated runs each, the load printed at the start and the end. It moves `WW3/` to the
  checked-out commit's pin first.
- `~/code/ww3-gpu-2` is on `goal/c3e7eecf-6142-srce-lanes-sin4`, `WW3/` at `50a10730`.

## Traps met (for whoever continues)

- **A one-thread timing on the i9-14900 is no measure unless pinned.** The same code took 113 s to
  155 s in four unpinned runs as the thread moved between performance and efficiency cores; pinned
  to CPU 2 (`taskset -c 2`), three runs agreed within 0.1 s.
- **The load average counts the timed job's own threads.** A 32-thread run shows a load of 25 to 27
  with nothing else on the machine; read the load before the job starts.
- **g++ leaves a lane loop with selects scalar under its default `-ftrapping-math`**: it sinks the
  values only one side of a select uses into a branch and will not speculate them back
  ("unsupported control flow in loop"). Plan 005 uses GCC vector types instead of changing that
  flag for every kernel; `-fopt-info-vec-all` on the shim's translation unit, inside the pinned
  shell, tells which loops vectorised.
- **`serial-debug` has no `-march`**: a function that returns a 32-byte vector by value warns
  `-Wpsabi` there; pass vectors by reference.
- **`pkill -f <pattern>` from a tool shell kills that shell too** when the pattern is in its own
  command line; kill by PID.
- **Merged means finished**: work after a merged PR goes on a new `goal/` branch from `main`, as L2
  did after #197 merged.
