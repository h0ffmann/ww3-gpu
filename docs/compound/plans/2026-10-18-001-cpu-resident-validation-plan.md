---
title: CPU validation of the resident Kokkos preset - Plan
type: feat
date: 2026-10-18
artifact_contract: ce-unified-plan/v1
product_contract_source: ce-plan-bootstrap
execution: code
---

# CPU validation of the resident Kokkos preset - Plan

## Goal Capsule

- **Objective:** a reader of this repository can check, by commands in it, that WW3 built with the Kokkos port runs `ww3_shel` faster than the original NOAA WW3 on the owner's i9-14900 (OpenMP, CPU), and that the configuration timed passes WW3's own regtests bit for bit against the Fortran.
- **Means:** WW3's regtest matrix restricted to the entries that run the timed preset, with the preset's flags (KTD1, KTD2); the paired timing of `scripts/ww3-gpu.sh`, median of three (KTD4).
- **Authority:** AGENTS.md (hard rules) over this plan; ADR-0016 (regtests postponed to a validation phase the owner calls: this is that phase, called 2026-10-18), ADR-0010 (WW3's regtests are the port's only tests), ADR-0014 does not apply (no GPU run).
- **Stop conditions:** a `test.comp` difference that a Fortran/Fortran run does not reproduce stops the work and is reported; no tolerance is widened, no entry is skipped to get green.
- **Execution profile:** medium loop (AGENTS.md, "Engineering work"); one branch, `goal/e49c20a7-5821-cpu-validation`, one PR through `just pr`.

---

## Product Contract

### Summary

The resident preset (`--kokkos resident`: `SRCE`, `XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV` on Kokkos) is the configuration with a measured end-to-end gain: 2.40x against `--ww3` on `bench_small`, one run on a loaded machine ⚠ (`docs/log/2026-10-17-resident-timing-cpu-gpu.md`). Its parts are wired but `regtested` is "no" in `kokkos/ports.tsv`. This plan regtests exactly that configuration and times it properly, nothing else.

### Problem Frame

A speed figure enters a table only after a passed WW3 regtest replay on the same commit (AGENTS.md, ADR-0016; `scripts/results.py check` refuses one without it). The matrix driver (`regtests/`, WFIP-0003) can only turn on single source-term routines (`WW3GPU_ROUTINES`), so it cannot run the parts the timed preset uses, and its selections are by one physics word, so the smallest one that would cover the preset is hundreds of entries.

### Requirements

- R1. The regtest Kokkos run uses the same run-time flags as the timed run (`WW_KOKKOS_ALL=0`, the preset's parts at 1), and the Fortran run of the pair uses `WW_KOKKOS_ALL=0` with every part unset.
- R2. The regtest selection is every `matrix.base` entry at the pin whose switch the preset can run: `ST4` and `NL1` (W3SRCE's Kokkos path), `PR3` (W3XYP3, W3KTP3), no `PDLIB` (refused by W3SRCE's set-up).
- R3. An entry passes only when `test.comp` finds no file different Fortran/Kokkos other than files two Fortran runs also write differently, and the Kokkos run's parts line in its log shows every preset part on Kokkos (`<PART>=K`).
- R4. The timing is `ww3_shel` wall time, `--ww3` against `--ww3-gpu --cpu --kokkos resident`, median of three, on a quiet machine, on the commit the regtests passed on.
- R5. No GPU run, no profile or perf study of the original WW3: the original enters only as the baseline run the paired script makes (session-settled: user-directed, chosen over a separate WW3 performance study and a GPU comparison: the claim is CPU Kokkos against original WW3, nothing more).

### Scope Boundaries

- In: the shared-memory claim, the serial and `OMPG` entries: 10 serial (`ww3_tp2.6` twice, `ww3_tp2.15` four times, `ww3_ts2` and `ww3_ts3` with `PR3_UQ` and `PR3_UNO`) and 4 OpenMP (`ww3_ts3` twice, `mww3_test_05` twice, a `ww3_multi` case) (v, `WW3GPU_KOKKOS=resident just ww3-gpu-matrix-list serial|omp` at `769c44f0`, 2026-10-18, after the ST4 with FLX5 exclusion drops `ww3_tp2.15 -i input_rho`).
- Deferred: the 43 MPI entries with the same switch words (v, same command with `mpi`), among them `ww3_ufs1.1` and the OASIS-coupled `ww3_tp2.14`, which may not build here ⚠. They are optional evidence for a later run; the timed case is shared memory.
- Out (session-settled: user-directed, chosen over the full 758-entry matrix that was proposed first: only the timed configuration matters): the full matrix, `WW_KOKKOS_ALL=1`, the per-routine source-term checkpoints, GPU runs.

### Success Criteria

- `docs/data/hoff_matrix_resident_<date>.tsv` with every selected entry `test.comp` 0 files different (or `ww3_self` only) and `kokkos_log` yes.
- A `bench/results/` record of the median of three, `regtest: passed: <the matrix command>`, accepted by `just results check`.
- `kokkos/ports.tsv`: `regtested` "yes: <how>" for the rows the preset ran, `port_score.py --write` then `--check`.

---

## Planning Contract

### Key Technical Decisions

- KTD1. A `WW3GPU_KOKKOS` configure option for the matrix with the meaning of `ww3-gpu.sh --kokkos`: `resident` (the preset), `all` (`WW_KOKKOS_ALL=1`), or a part list; empty keeps today's `WW3GPU_ROUTINES` behaviour. Same vocabulary as the script so the regtest and the timing run cannot drift. Parts are validated against `kokkos/src/fortran_iface/parts.d/*.txt`, the registry the script already reads, and every `COUNT` there is set in the Kokkos run.
- KTD2. A `WW3GPU_SWITCH_WORDS` option: words every selected switch must have, `!WORD` for one it must not; under `WW3GPU_KOKKOS=resident` it defaults to `ST4;NL1;PR3;!PDLIB`. The existing one-word physics selection stays for `WW3GPU_ROUTINES`.
- KTD3. In every mode the Fortran run sets `WW_KOKKOS_ALL=0`. Today a `WW_KOKKOS_ALL=1` inherited from the shell would turn the "Fortran" run into Kokkos for the parts outside `WW3GPU_ALL_ROUTINES` and the pair would compare Kokkos with Kokkos.
- KTD4. Timing is not part of the regtest: the matrix judges bits only, and the speed figure comes from `scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident --repeat 3`, whose perf summary `scripts/results.py record` turns into the record.
- KTD5. Step 0 needed no pin change: the committed pin is `WW3@769c44f0` (v, `git ls-tree origin/main WW3`, 2026-10-18); the working tree had a stale checkout (`4a94a8f6`) and was moved to the pin.

### Assumptions

- The kokkos-run evidence for a part is the `Kokkos parts (K Kokkos, F Fortran): ... <PART>=K` line `W3INIT` writes (`WW3/model/src/w3kokkosmd.F90:1316` at `769c44f0`); `XYP3`, `KTP3`, `IOPE`, `OUTG`, `UPDV` register into it (`w3kokkos_prop3md.F90:136-137`, `w3kokkos_pntmd.F90:57`, `w3kokkos_outgmd.F90:69`, `w3kokkos_updvmd.F90:65`) (v, grep at the pin). Whether a part is printed on every switch the selection keeps is ⚠ until the first run.
- In the matrix, `run_cmake_test` runs every program of an entry (`ww3_grid`, `ww3_prnc`, `ww3_ounf`, ...) under the same environment, so the preset reaches them too, where `ww3-gpu.sh` runs `ww3_prnc` and `ww3_bounc` with `WW_KOKKOS_ALL=0`. Both runs of a pair are still compared on the same outputs ⚠.
- Thread budget: `run_entry.cmake` divides `WW3GPU_OMP_THREADS` among an MPI line's ranks, so jobs x `WW3GPU_OMP_THREADS` is the core count; 4 jobs x 8 threads on 32 hardware threads. (The stopped ST4 matrix of 2026-10-15, 8 jobs x 4 threads, was therefore not oversubscribed.)

### Sequencing

U1 (done) -> U2 -> U3 -> U4 (-> U4a and back to U4 while an entry fails) -> U5 -> U6. U5 waits for U4 to pass, on the same commit.

---

## Implementation Units

### U1. Pin (step 0)

- **Goal:** the tree under test is `main` with `WW3/` at its committed pin.
- **Done:** branch `goal/e49c20a7-5821-cpu-validation` from `origin/main` (`614fab4`), `WW3/` at `769c44f0`, `git status` clean of `WW3`.

### U2. Driver: the preset and the switch words (step 1)

- **Goal:** R1, R2, R3, KTD1 to KTD3.
- **Files:** `regtests/CMakeLists.txt`, `regtests/cmake/ww3gpu_common.cmake`, `regtests/cmake/run_entry.cmake`, `regtests/cmake/summary.cmake`, `regtests/cmake/selftest.cmake`, `justfile` (`ww3-gpu-matrix`, `ww3-gpu-matrix-list`), `docs/WFIPs/WFIP-0003-kokkos-regtest-matrix.md` (an update paragraph).
- **Approach:** parse `parts.d` at configure; resolve the preset to a flag list in one function shared by configure, the dry run and the run; select with the word list; in the Kokkos run check `<PART>=K` for each preset part in the parts line and record the per-routine counters; record the preset in the summary header; expected counts for the new selection at the pin in `selection:count`.
- **Test scenarios (selftest.cmake, fixtures):** `parts.d` parsing (PART, COUNT, comments, a bad line refused); preset resolution (`resident`, `all`, a list, an unknown part refused); word selection (`ST4;NL1;PR3;!PDLIB` keeps and drops the right fixture switches); the parts-line check (all K passes, one F fails, no line fails); the Fortran env sets `WW_KOKKOS_ALL=0` in both modes; the pin counts 10 serial, 4 omp, 43 mpi, 57 in all.
- **Verification:** `just ww3-gpu-matrix-selftest`; `just ww3-gpu-matrix-list serial` with `WW3GPU_KOKKOS=resident`; a dry run (`-N`), with `WW3/` unchanged.

### U3. Smoke of the timed case, regtest form

- **Goal:** the timed case itself is byte-identical before the matrix spends an hour.
- **Approach:** `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident` and `... full ...`, each pair to pair with `--ww3`.
- **Verification:** both scripts exit 0; every model file `cmp`-identical as the script reports.

### U4. The restricted matrix

- **Goal:** R3 on the 14 shared-memory entries.
- **Approach:** `WW3GPU_KOKKOS=resident just ww3-gpu-matrix 4 serial`, then `omp` with `WW3GPU_OMP_THREADS=8`. Estimated 30 to 60 min in all ⚠ (from 100 NL1 serial entries in 22.6 min at 8 jobs and 25 `OMPG` entries in 19 min at 4 jobs, `docs/log/2026-10-14-snl1-bitwise-af11.md`, `docs/log/2026-10-15-hoff-matrix-omp.md`).
- **On a failure:** U4a, before anything else is changed.
- **Verification:** summaries copied to `docs/data/` with their headers.

### U4a. Failure triage: from a failed row to a fix

A failed entry is a defect of the port until a Fortran/Fortran run shows WW3 does it too (AGENTS.md, "Bit for bit"). The loop below finds the part, the file and the branch, so the fix can be written while the failing run is still on disk. It changes no gate: no tolerance, no excluded entry, no edit of `WW3/`.

**1. Read the row, then the one log it points to.** `build/ww3-gpu-matrix-<sel>-kokkos-resident/summary.tsv`, the row of entry `<n>`; logs in `logs/<n>.*` of the same directory.

| The row says | Read first | What it usually is |
|---|---|---|
| `built` no | `logs/<n>.F.log` | WW3 input or toolchain, both runs alike (as `ww3_tp2.21`'s `ww3_grid` format error, `docs/log/2026-10-17-st4-matrix-partial.md`; OASIS for `ww3_tp2.14`) |
| `ran` fortran-only | tail of `logs/<n>.K.log` | the Kokkos run stopped: a part refusing a switch it does not cover (`KOKKOS_PART`, `W3KOKKOS_SRCE_SETUP`), the nested-parallelism abort, an exception from a shim |
| `kokkos_log` no | `logs/<n>.parts_missing.log` | a preset part printed `=F` or not printed: the switch is outside what the part covers |
| `test_comp` k/n with k > 0 | `logs/<n>.test_comp.port.log`, then `logs/<n>.nccmp.*.out` | a real difference: steps 2 to 5 |
| `nccmp_tol` FAIL with `test_comp` 0/n | `logs/<n>.nccmp.log` | a comparison-tool problem; the owner reads it before anything changes |

For the first two kinds, confirm the cause before calling it "not the port": build the same entry from `WW3-noaa/` (`--ww3`) or read the Fortran run's log; if the original WW3 fails the same way, record it in `docs/log/` as a WW3 or toolchain failure of that entry at this pin, and the row stays in the record as failed, never dropped. If a preset part refuses the switch by design, the selection words were wrong: add the missing word to `WW3GPU_SWITCH_WORDS` with the `model/src/<file>.F90:<line>` of the refusal, in the same PR as the record.

**2. Find the part.** Rerun only that entry with one part at a time, same commit, same threads: `WW3GPU_KOKKOS=SRCE just ww3-gpu-matrix 1 <sel> -R '^<nnn>:'`, then `XYP3,KTP3`, then `OUTG`, `IOPE`, `UPDV` (a new build directory per spec, `-kokkos-<spec>`). One entry takes about 1.5 to 3.5 min ⚠ (the `wall_s` of the 2026-10-15 rows). The first spec that reproduces the difference names the part; if only the whole preset does, the resident VA protocol between parts is the suspect (`WW_KOKKOS_VA_CHECK=1`, `WW_KOKKOS_VA_POISON=1`, PATCH.md section 21).

**3. Find the file and the size.** Which files `test.comp` names narrows the code path: `out_grd` only (field output: `OUTG`, or the state), `out_pnt` (point output: `IOPE`), `restart` (the state at a restart time, a `W3WAVE` sync). `nccmp-tol`'s max |d| and max rel size it: a few ULP points at arithmetic (step 4a), a large or structured difference at a branch (step 4b).

**4. Find the cause.** Compare the entry's switch with `switches/switch_lab_ompg` and `switch_lab_shrd`, the switches every byte-identical run so far used: each word the entry adds is a branch the port has not run.
- 4a. ULP-level: the traps of the `ww3-port` skill, in order: gfortran's `-O3` chain for an integer power (`x**11`, `docs/log/2026-10-14-snl1-bitwise-af11.md`); a libm or libmvec call taking another path than the Fortran's (`kokkos/src/ww_kokkos/st4_libmvec.hpp`, `docs/GPU_LIBM_BITS_202610.md` for the host's FMA); a reduction whose order differs per point; contraction left on in one build. Read the optimised code of the Fortran routine in WW3's build of that entry (`build_<n>`), not the source.
- 4b. Large: the branches known never to have run, first: in `W3XYP3` the `FLCUR` (currents), global-closure and boundary-input paths (`kokkos/port_report/W3XYP3_KOKKOS.md`); `ww3_multi`'s one context per grid (`mww3_test_05`); `UNO` against `UQ`; `ww3_tp2.15`'s and `ww3_tp2.6`'s grid types. Find the Fortran branch at `model/src/<file>.F90:<line>` and its C++ counterpart; the static parity argument of ADR-0016 for that branch is what was missing.

**5. Fix, then widen the rerun back.** The fix goes in `kokkos/` (kernel or shim) and keeps the Fortran's arithmetic; if it is in the fork's caller patch, it is a commit on the fork branch and a pin bump, and a pin bump is a checkpoint (ADR-0011). Then, on the new commit: the failing entry alone (`-R '^<nnn>:'`), then the whole selection of U4, then U3, then U5. A fix that changes a reduction order, a limiter or the integration order is asked about first and is its own PR (AGENTS.md, "Porting").

**6. Record.** A dated `docs/log/` entry for each failure found, with the row, the part, the cause and the commit of the fix; a new porting trap also goes into the `ww3-port` skill. A failure that is WW3's own stays in the record with its evidence.

Never, in this loop: widen a tolerance in `kokkos/tools/nccmp-tol/tolerances.txt`, set `WW3GPU_STRICT=OFF`, drop an entry from the selection to get green, edit `WW3/`, or time anything before the selection passes on the commit being timed.

### U5. The timing

- **Goal:** R4.
- **Approach:** on a quiet machine (load average below 2 before the start, recorded), `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident --repeat 3`, then `python3 scripts/results.py record <perf summary>` with the U4 command as its regtest.
- **Verification:** `just results check`; `just results table`.

### U6. Record

- **Goal:** the ledger says what passed.
- **Files:** `kokkos/ports.tsv` (`regtested` of `w3srce`, `w3xyp3` and the other rows the preset runs), regenerated `kokkos/PORT_STATUS.md`, `kokkos/PORT_RANKING.md`, `kokkos/PORT_TABLE.tsv`, `site/data/port_score.tsv`; a `docs/log/2026-10-18-...` entry with wall times; the port reports' bench tables.
- **Verification:** `python3 scripts/port_score.py --write` then `--check`; `just vale`.

---

## Verification Contract

| Changed | Gate |
|---|---|
| `regtests/` | `just ww3-gpu-matrix-selftest`, `just ww3-gpu-matrix-list` |
| `justfile` | `just --list` parses |
| `bench/results/` | `just results check` |
| `kokkos/ports.tsv` | `python3 scripts/port_score.py --write`, `--check` |
| Markdown | `just vale`, links (lychee in CI) |
| `scripts/`, `regtests/` | `security-reviewer` before the PR is ready |

The change to the matrix driver changes how a comparison tool is invoked: the owner reviews it before it merges (AGENTS.md, "Never cheat a regtest tool").

## Definition of Done

- U2 to U6 done, their gates passed, with the commands in the `Tested:` trailers.
- No timing in a table without the U4 summary on the same commit.
- No experimental code left in the diff.
