---
title: The first supervised GPU run of the resident preset, and the five plans after it - Plan
type: feat
date: 2026-10-18
artifact_contract: ce-unified-plan/v1
product_contract_source: owner, session of 2026-10-18 (goal abbb0456-6043, PR #191)
execution: code
---

# The first supervised GPU run, and the five plans after it - Plan

## Goal Capsule

- **Objective:** answer, with one measured number, whether WW3 with its time step resident on
  the GPU beats the same WW3 resident on the CPU, and how far the GPU's output is from the
  Fortran's; then pick the next plan from that answer.
- **Means:** plan 1 below: agent preparation with no device run (phase A), one session the owner
  supervises at the terminal (phase B), records and a decision (phase C).
- **Authority:** AGENTS.md over this plan; "Running on the GPU (hard rule)" (no `--gpu` run
  without the owner), ADR-0014 (a GPU run is judged by `nccmp-tol`'s tolerances, never widened),
  ADR-0017 (single node), "Resident spectrum (hard rule)".
- **Stop conditions:** any `nccmp-tol` failure, any NaN in the GPU output where the Fortran has a
  value, a watchdog kill that a smaller batch does not cure, or a run that is not reproducible run
  to run: record it and stop that step; never change a tolerance or a gate during the session.
- **Execution profile:** large. Phase A in the next session, on a branch from `main` after PR
  #191 merges; phase B when the owner says so; phase C in the same PR as phase A's records or a
  follow-up.

---

## Context: NOAA's most common use of WW3

GFS-Wave, and GEFS-Wave built the same way, is a `ww3_multi` mosaic of three grids (global 10
arcmin, Southern Ocean 15 arcmin, Arctic 9 km), 50 x 36 spectral components, ST4 with the DIA,
to 384 h, run with MPI on WCOSS2, whose nodes are AMD EPYC 7742 with no GPU, moving to Google
Cloud by December 2027 (v, `docs/GLOSSARY.md` rows GFS-Wave and WCOSS2, sources read
2026-10-16).

Against that use, the Kokkos runtime has three gaps after PR #191 is written:

- `ww3_multi`: the resident path gave wrong numbers between `W3WAVE` calls (gap 2 of
  `docs/PRODUCTION_READINESS_202610.md`); U1 of PR #191 fixes it, and its regtests decide.
- MPI: out of scope (ADR-0017).
- GFS-Wave's own switch file was never read. If it has `SEED` or `FLD2`, the Kokkos `W3SRCE`
  does not run there (gap 1) ⚠.

## Could WW3 entirely on the GPU beat ww3-gpu on the CPU?

On `full` (`bench_small`, 9 520 sea points, 24 x 24 spectral components, 12 h), very likely, by
an order of magnitude ⚠. No WW3 run on a GPU exists yet (`kokkos/PORT_STATUS.md`); this is a
projection from these measurements:

| Fact | Number | Source |
|---|---|---|
| CPU resident, 32 threads, `full` | 14.36 s wall; `W3SRCE` kernel 12.45 s (87 %); outside Kokkos 0.22 s (1.5 %) | `docs/log/2026-10-18-resident-profile.md`, one run, load 3.5 ⚠ |
| One thread, port against Fortran | 174.78 s against 174.27 s | same log, one run each ⚠ |
| `W3SRCE` kernel, 1 to 32 threads | 12.6x faster (the i9-14900 has 24 cores) | same log |
| DIA kernel alone, 1000 points, OpenMP 32 threads against RTX 4090 | 5.02 ms against 0.047 ms, 107x | `bench/results/snl1-i9-14900-rtx4090-2026-09-15.json`, median of three (v) |
| The same call with the per-call copies of phase 1 | 5.95 ms against 0.75 ms, 7.9x | same record (v) |

The second row says the CPU gain is all in spreading work over threads: the per-point code costs
what the Fortran costs. The last two rows bracket the GPU: with the spectrum resident (the design
since WFIP-0005, and with no copies after PR #191), the end-to-end ratio moves from 7.9x toward
the kernel's 107x. If the whole step ran 20 to 50 times faster on the GPU, `full` would take 0.5
to 1 s against 14.4 s ⚠. Three assumptions carry that figure, and plan 1 tests each:

1. **The whole `W3SRCE` kernel scales like the DIA alone.** It does not, as written: the body on
   `main` is `w3srce_team`, `TeamPolicy(n, AUTO)`, 168 registers, most of the work in
   `Kokkos::single`, so by ptxas' estimate about three threads per SM do the non-DIA work
   (`origin/goal/42-gpu-step-srceteam`, `438ab8b`, `12685ff`, not merged). It also runs with
   `--fmad=false` and the device copies of glibc's functions (`WW_KOKKOS_LIBM`), both slower than
   CUDA's defaults.
2. **Launches are cheap.** Each step launches several kernels; on 9 520 points their latency may
   dominate. Smoke (one point, 2 161 launches) is slower on a GPU by construction.
3. **The host remainder stays small.** 1.5 % on the CPU becomes the floor once kernels shrink:
   0.22 s against a 0.5 s target.

**"Without any CPU" is not possible.** Set-up, namelists, I/O and the time-loop driver stay on the
host; a GPU is driven from a CPU. The target is the whole time step on the device, the host only
orchestrating and writing outputs: plan 4.

**More CPU threads do not change the picture.** At 12.6x on 32 threads of a 24-core part, the
i9 is near its ceiling. Beyond one node is MPI (ADR-0017). A 128-core EPYC node may scale further
⚠ (not measured). Inside a node, `W3XYP3`'s threads are idle in 57 % of its samples, which is
1.1 s at most.

**Correctness costs.** The GPU is not yet bit for bit: a GPU run is judged by tolerance
(ADR-0014) until `scripts/libm_bits.sh --device` shows the device copies of glibc's functions
give glibc's bits, and the host C++ of the CUDA build is compiled by g++ 14.4.0, not the 16.2.0 of
every parity argument so far (`docs/log/2026-10-18-cuda-build-only.md`) ⚠.

## The five plans, ranked

| # | Plan | Attacks | Why this rank | Owner decision needed |
|---|---|---|---|---|
| 1 | The first supervised GPU run (this file, below) | the unknown: GPU against CPU, and the GPU's distance from the Fortran | cheapest, most information; decides whether 2 and 4 aim at the CPU or the GPU | the run itself |
| 2 | Several sea points per vector lane in `W3SRCE` | DIA, `W3SDS4`, `W3SIN4`, the integration: 12.0 of 14.4 s on the CPU | the only CPU plan that attacks the per-point cost, now all of it; on a GPU the same layout gives coalesced loads | yes: it changes loop nesting (AGENTS.md, "Porting"); its own PR with the per-lane parity argument |
| 3 | GFS-Wave's own configuration | correctness on what NOAA runs | without it every speed-up is shown on a configuration NOAA does not run | which tag to read |
| 4 | The whole time step resident on the GPU | the host remainder, launch count, readers left after PR #191 | what makes plan 1's number hold on cases with more output than `full` | lane Q's merge |
| 5 | Thread efficiency in one node, and a many-core baseline | `W3XYP3` imbalance, pinning, scaling on a quiet machine and a WCOSS2-like or cloud EPYC node | the honest CPU baseline a GPU result must beat for NOAA's move to the cloud | access to such a node |

Not ranked: skipping `W3SDS4`'s zero-weight terms (12 of 23 products, profile log) changes the
result for an infinite or NaN input, so it is the owner's decision and its own PR.

**Work in progress elsewhere (2026-10-18).** PR #193 (branch
`goal/eb10363f-4821-noaa-progress-record`, another session) builds the `W3SDS4` saturation-integral loop
interchange (`kokkos/src/ww_kokkos/st4_sds4.hpp`), a DIA direction-halo layout that turns the
index-table gathers into contiguous loads (`snl1_team.hpp`, `snl1_tables.{hpp,cpp}`), and
`-fno-math-errno` for the host build (`kokkos/src/ww_kokkos/CMakeLists.txt`). The halo layout
attacks the same gathers as plan 2 by another route. That session reports the DIA kernel alone
(`ww_bench_snl1`, medians of seven) 1.93x faster at one thread and 1.19x at 32 with the halo, and
the `W3SDS4` interchange about 7 % faster unless `SATWEIGHTS` and `SATINDICES` are transposed;
end to end not measured ⚠ (PR #193's own record is the source). Plan 2 starts from that
result, not before it. Its timing and the GPU run of plan 1 need a resident regtest pass on the
tree that holds both PRs.

### Plan 2 in short

A team of eight points, one per AVX2 lane, each lane its own point's arithmetic in the
Fortran's order, spectra point-minor in team scratch (profile log, proposal 1). No sum crosses a
lane, so the parity argument is the per-lane GIMPLE. Covers ranks 1 to 4 of the profile if built
as a layer for the whole `W3SRCE` team body. Trap: the five libmvec sites of ST4
(`st4_libmvec.hpp`) keep the Fortran's groups of four directions per call, so those loops stay.
Gain ⚠: the profile does not separate gathers from arithmetic.

### Plan 3 in short

Read the switch of the GFS-Wave release tag and record it in gap 1; if it has `SEED` or `FLD2`,
translate their `W3SRCE` branches (ADR-0002 order permitting); build a three-grid `ww3_multi`
regtest case with that switch from WW3's inputs, and add it to the matrix under
`WW3GPU_LAB_SWITCH=OFF`.

### Plan 4 in short

After PR #191 (readers U8, U9, the `ww3_multi` exchange of U1): review and land lane Q
(`w3srce_split`, 96 threads per point; three surviving mutants in `12685ff`'s message to resolve
first); move the forcing updates left on the host; cut launches per step (merge the small
kernels, or CUDA graphs if Kokkos' graph API covers them ⚠); count with `WW_KOKKOS_VA_COUNT=1`
and the timers.

### Plan 5 in short

`W3XYP3` component scheduling (dynamic over components, or a team size from the component
count), `OMP_PROC_BIND`/`OMP_PLACES` on a quiet machine, scaling at 8, 16, 24, 32 threads, median
of three; one run on a many-core EPYC node if the owner has one.

---

## Plan 1 in detail: the first supervised GPU run

### What the run must answer

- Q1. Wall time of `ww3_shel`, `full`, `--kokkos resident`, on the RTX 4090 against the same
  preset on the CPU (32 threads) and against `--ww3`, median of three, same commit.
- Q2. Per-field distance of the GPU output from the Fortran of the same executable and from
  `--ww3` (`max|d|`, rms, largest relative difference), and whether it passes
  `kokkos/tools/nccmp-tol/tolerances.txt` unchanged.
- Q3. Where the GPU wall time goes (timers): `W3SRCE`, propagation, outputs, host remainder,
  copies.
- Q4. Whether the device copies of glibc's functions give glibc's bits (`libm_bits --device`).
- Q5. Whether the run is reproducible run to run on the GPU.

### Facts the plan is built on (each checked in a file, 2026-10-18)

- `scripts/ww3-gpu.sh --gpu` asks for a typed "yes" at a terminal (`:359-370` at `fe3cbc8`), exit
  7 without one, exit 4 without a driver; it then re-executes inside
  `nix develop picasso/labs/pratico#cuda`. `--build-only` skips the prompt and is the agents'.
- `--gpu` alone means the `gpu` preset, `SRCE` only (`:296-298`); `resident` is
  `SRCE,XYP3,KTP3,OUTG,IOPE,UPDV` (`:191-197`). `just smoke-ww3-gpu gpu`, `just full-ww3-gpu gpu`
  and `just perf-ww3-gpu full 3` run `gpu`, not `resident`; `perf-ww3-gpu` takes the parts as
  its third argument (`justfile:215`).
- `--timers` fences the device (a breakdown, not a timing) and cannot combine with `--repeat`.
  `--repeat N` alternates `--ww3` and Kokkos runs and requires every Kokkos NetCDF to equal run
  1's (`:709-721`).
- Comparison: `nccmp-tol` on the first `ww3.*.nc` only (`:673-677`), against the Fortran of the
  same executable then against `--ww3` (`:695`); `cmp` of every model file is reported, not fatal,
  on CUDA (`:682`, `:698`). `full` writes only `HS` (`bench_small/ww3_ounf.nml:10`); `coast`
  writes every `OUTG` field.
- `nccmp-tol` counts a NaN or fill in the test where the reference has a value as "dropped", says
  so on `stderr` and still passes the variable (`kokkos/tools/nccmp-tol/nccmp_tol.cpp:52-56`).
  `compare.cpp` has no 0/360 degree wrap for `dir` and `dp`.
- `counts_resident.txt` expects `W3SRCE batch inputs unchanged, not copied: 422` on `coast`. At
  the device's 128 MiB batch cap `coast` takes two launches per step, so no shadow is kept and
  the line differs: the script exits 1 before `nccmp-tol`. With `WW_KOKKOS_SRCE_BATCH_MIB` set the
  check is skipped (`:651-662`).
- The 4090 drives the display; the driver's watchdog ends a long kernel
  (`cudaErrorLaunchTimeout`); the batch cap's estimate assumes the GPU runs at least a sixth of the
  CPU's speed (`kokkos/src/fortran_iface/PATCH.md` section 19).
- `just ww3-gpu-matrix-cuda` forwards `WW3GPU_ROUTINES` only, not `WW3GPU_KOKKOS` or
  `WW3GPU_LAB_SWITCH`, and `WW3GPU_STRICT` is on, so every entry fails on any byte difference
  (`justfile:143-159`).
- `results.py record` needs `--repeat` of at least 3, writes the ADR-0014 wording for CUDA, and
  refuses a second study `ww3-vs-ww3gpu-<case>-cuda-<date>` on the same day (`:121-171`); it keeps
  no per-field statistics.
- `libm_bits.sh --device` asks its own "yes", builds `device_check.cu` (`--fmad=false`, sm_89),
  checks 15 functions on 2^32 inputs each with a single-threaded host reference
  (`device_check.cu:146-168`): hours for the full set ⚠ (not timed); `--device expf` checks one.
- `WW_KOKKOS_LIBM`'s tables are generated from the build host's libmvec, so build and run happen
  on the same workstation (`docs/TOOLCHAIN.md`).
- The CUDA builds in the main checkout are from 2026-10-16, before `WW_KOKKOS_LIBM`, and on the
  `SHRD` switch: they rebuild from scratch. The toolchain stamp includes `CMAKE_PREFIX_PATH`, so
  `build/ww3` also rebuilds inside `#cuda` and again on the next CPU run (`:480-487`).
- `run_dir`s are deleted at every start (`build/runs/ww3-gpu-<case>-cuda/`); `WW3GPU_RUNS`
  moves them.
- Docker's CUDA image is another toolchain (CUDA 13.4, no OpenMP, counts check skipped): not
  used for this run.

### Phase A: preparation by agents (next session, no device run)

Each unit ends with its gates and a commit; nothing in phase A types "yes" or launches a kernel.

**A1. Freeze the commit.** A branch `goal/<id>-gpu-first-run` from `main` after PR #191 merged.
The commit that phase B runs is its tip at the end of phase A, written into the run card (A7).
No other session works in that checkout during phase B: a separate worktree, `WW3GPU_RUNS` under
it.

**A2. Make the tools able to judge this run.** Each change makes a comparison stricter or lets
it run the configuration asked; each is a change to a comparison or its invocation, so a person
reviews it before merge (AGENTS.md, "Never cheat a regtest tool"), and it merges before phase B.

- A2a. `nccmp-tol`: a NaN or fill in the test where the reference has a finite value fails the
  variable (today: passes with a note). Stricter, never looser.
- A2b. `nccmp-tol`: judge every `ww3.*.nc` the case writes, not the first only.
- A2c. `counts_resident.txt` on a device: the expected `not copied` count follows the number of
  launches per step (or the line is checked on the host backend only, and the device run prints
  its count for the record). Decide with the owner which; the CPU check stays as it is.
- A2d. `ww3-gpu-matrix-cuda` forwards `WW3GPU_KOKKOS` and `WW3GPU_LAB_SWITCH`, and a GPU entry's
  verdict comes from `nccmp_tol`/`judged` with `test.comp` recorded (ADR-0014), selftest fixtures
  for both.
- A2e. Ask, do not do: a 0/360 wrap for `dir`/`dp` in `compare.cpp`. A wrap changes what passes
  near north, so it is the owner's decision; until then, `coast`'s direction fields are read by
  hand when they fail near 0 or 360.
- A2f. `smoke-ww3-gpu`, `full-ww3-gpu`: accept a parts argument as `coast-ww3-gpu` does, so the
  run card can use `just` and the preset is visible in the command. If not, the card uses
  `scripts/ww3-gpu.sh` directly.

**A3. Build everything the session will run, on the frozen commit, on the workstation.**
`JOBS=8 bash scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos resident --build-only`, the same for
`smoke` and `coast` if their builds differ, and `scripts/libm_bits.sh --nvcc` (build only, if the
script separates it; else record that its build happens in the session). Then build `--ww3` in
the `#cuda` shell so the toolchain stamp matches (`--ww3 full --build-only` if supported ⚠).
Record nvcc, Kokkos, g++ host, gfortran, driver, and the `CMakeCache.txt` lines for
`WW_KOKKOS_LIBM`, `CMAKE_CUDA_ARCHITECTURES`, contraction flags. Read the warnings against
`docs/log/2026-10-18-cuda-build-only.md`'s list; a new warning in a wired part stops phase B.

**A3b. The host compiler of the CUDA build.** gfortran is 16.2.0 in both builds, but
`nvcc_wrapper` compiles the host half of every `.cpp` with g++ 14.4.0, against 16.2.0 for the CPU
build and every static parity argument (`docs/log/2026-10-18-cuda-build-only.md:25-26`,
`:74-77`). Before phase B, one of two things: build the library on the OpenMP backend with g++
14.4.0 and run `smoke` and `full` resident pair to pair with the Fortran (byte-identical, or a
defect to fix before the GPU run); or compare the `-O3` GIMPLE of the host-side shim paths under
both compilers. Record why the `#cuda` shell pins g++ 14.4 (presumably the newest host GCC nvcc
12.9 accepts ⚠, from NVIDIA's support matrix). Without it, a GPU difference cannot be split into
the device's share and the host compile's.

**A4. The same commands on the CPU first.** On the frozen commit:
`bash scripts/ww3-gpu.sh --ww3-gpu {smoke,full,coast} --cpu --kokkos resident`, each byte-identical
to the Fortran and to `--ww3` (NetCDF attribute aside); `WW_KOKKOS_VA_COUNT=1` and
`WW_KOKKOS_VA_POISON=1` on `full` and `coast`. These are phase B's reference numbers and prove
the commit is sound before the GPU is involved.

**A5. Static readiness of the device kernels.** ptxas `--resource-usage` (registers, spills,
shared memory) of every kernel the preset launches, from A3's build; occupancy per kernel from
the 4090's limits; the expected launches per step on `full`. From them, the watchdog risk of
`W3SRCE` at 128 MiB and the batch size for the first run (32 MiB, as the handover proposed,
unless A5 says otherwise). Written into the run card; nothing tuned.

**A6. Lane Q stays out of the first run.** The first run measures `main`'s `w3srce_team`, so
its number is the baseline lane Q must beat (plan 4). A6 records lane Q's state and its three
surviving mutants, nothing more.

**A7. The run card.** Update `docs/GPU_SUPERVISED_RUN_CHECKLIST_202610.md` into one ordered card
with the exact commands of phase B (no `just` recipe whose default preset is not `resident`), the
frozen commit, the expected CPU numbers from A4, the per-field template for
`docs/data/ww3gpu_cuda_{smoke,full,coast}_<date>.md`, the stop rule of each step, and the
`script -a` capture of the whole session.

**A8. Phase A's PR.** Through `just pr`, the A2 changes flagged for human review in the body;
phase B starts after it merges.

### Phase B: the owner's session (in order; the owner types every "yes")

| Step | Command (from the run card) | Records | Stop rule |
|---|---|---|---|
| B0 | quiet machine: load below 1, no other session, `nvidia-smi -q` snapshot, `nvidia-smi --query-gpu=display_active --format=csv`, `script -a <log>` | machine block of the card | load above 1: wait |
| B1 | `scripts/libm_bits.sh --device expf` | TSV line, exit code | exit 2: CUDA error, stop and diagnose |
| B2 | `WW_KOKKOS_SRCE_BATCH_MIB=32 bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --kokkos resident` | parts line all `=K`, `nccmp-tol` both references, every field's statistics; speed not read | any failure: stop, record |
| B3 | `WW_KOKKOS_SRCE_BATCH_MIB=32 bash scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos resident --timers` | timers breakdown (Q3), `nccmp-tol` | `cudaErrorLaunchTimeout`: halve the batch once; again: stop |
| B4 | B3 without `--timers`, twice, with `WW3GPU_RUNS` apart; `cmp` the two Kokkos outputs | Q5 | not identical: record, skip B5's `--repeat` |
| B5 | `just perf-ww3-gpu full 3 resident` (with B3's batch variable if B3 needed it) | median of three (Q1); `results.py record` | `nccmp-tol` failure: no record |
| B6 | `WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_COUNT=1` on B4's command | NaN check, bytes moved (Resident spectrum rule) | a NaN in the output: a missed sync, stop |
| B7 | `coast` as B2, then without the batch variable once A2c is merged | every `OUTG` field, directions by hand near 0/360 | as B2 |
| B8 | `just ww3-gpu-matrix-cuda` on the entries U3 of PR #191 passed, if A2d merged | per-entry `nccmp_tol`, `test.comp`, `judged` | as the matrix reports |
| B9 | `scripts/libm_bits.sh --device` for all 15 functions, left running at the end | the full TSV (Q4) | none: a record |

### Phase C: records and the decision

- `docs/data/ww3gpu_cuda_{smoke,full,coast}_<date>.md` (per-field statistics),
  `bench/results/` from `results.py record` (`just results check`), a `docs/log/` entry with the
  session's log and every stop rule that fired, `docs/GPU_LIBM_BITS_202610.md` with B1 and B9.
- `kokkos/PORT_STATUS.md` "On a GPU" and `port_score.py --write`; WFIP-0005 unit 6's done
  criterion corrected to the resident preset (`just perf-ww3-gpu full 3 resident`); ADR-0014's
  revisit after the first run, as it asks.
- The decision on plans 2 and 4, from B5's median against A4's CPU median, both on the same
  commit:

| GPU / CPU resident, `full` | What it says | Next |
|---|---|---|
| at least 5x | the device step is the faster design here | plan 4 (lane Q, launches, host remainder), then plan 2 for the GPU layout |
| 2x to 5x | the GPU wins but something bounds it | read B3's timers: `W3SRCE` dominant means lane Q first; launches or host means plan 4's launch work |
| below 2x | the GPU does not pay on this case yet | plan 2 on the CPU, and a larger case for the GPU before any further GPU work ⚠ |

Whatever the ratio, the GPU number enters a table only with B5's `nccmp-tol` pass (ADR-0014) and
says "regtest pending" until WW3's regtests passed on its commit.

## Risks the plan cannot remove

- The 4090 is a desktop part with a display; an H100 has no watchdog and other limits, and
  `cuda-h100-release` needs a HOPPER90 Kokkos picasso does not build. The ratio does not transfer
  to an H100 ⚠.
- `full` is a 9 520-point regional case; GFS-Wave's global grid is larger, which favours the GPU,
  and writes more output per step, which does not ⚠.
- g++ 14.4 for the CUDA build's host code: a host branch of the CUDA build may round differently
  from the CPU build; A4 on the CPU build does not cover it ⚠.
