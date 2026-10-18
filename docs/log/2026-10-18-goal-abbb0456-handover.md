# Handover of goal `abbb0456`: the production single-node runtime, what worked, what is open

2026-10-18, an agent for M. Hoffmann (Claude Code session `e664b977-8f49-44ff-a914-0cf9abbb0456`).
The owner stopped the session's runs for another session to take over; this entry is what that
session needs. Every claim names its record; nothing here was re-run for this entry.

## What worked (merged)

PR #191, merged into `main` at `5cd0140` (2026-10-18 19:44 UTC), `WW3/` pinned at `50a10730`, which
the owner fast-forwarded `ww3-gpu/kokkos-pnt` of h0ffmann/WW3 to (`80644ba9..50a10730`, 18 commits).

| Result | Record |
|---|---|
| The `ww3_multi` defect (host writes of `VA` between `W3WAVE` calls) reproduced (4 to 10 files different on `mww3_test_02` to `_04`) and fixed with column syncs; the read at `W3WAVE`'s return removed | `docs/log/2026-10-18-ww3-multi-resident-va.md`, PATCH.md section 26 |
| No whole-spectrum copy in steady state: on `full`, `VA` host-to-device 1 (the bind), device-to-host 0 | the same log; `AGENTS.md`, "Resident spectrum (hard rule)" |
| Boundary and track output read only their columns; `W3CPRT` and `W3OUTG`'s partition fields on the resident `VA` | `docs/log/2026-10-18-resident-output-readers.md`, `docs/log/2026-10-18-cprt-resident.md`, PATCH.md 23 and 27 |
| `WW_KOKKOS_STRICT`, `WW_KOKKOS_PRESET` (`presets.txt`), the fallback count; `XYP3`/`KTP3` stop or count without `PR3`; the ESMF cap reads `VA` before its exports | PATCH.md 24, 25, 28, 29 |
| Install-only `ww_kokkos` (Kokkos its one dependency, version and backends pinned in the config); ADR-0018 Proposed | `kokkos/README.md`, "Install-only build" |
| CUDA `--build-only` of the preset (51 nvcc warnings, none in a wired part); the owner's GPU checklist | `docs/log/2026-10-18-cuda-build-only.md`, `docs/GPU_SUPERVISED_RUN_CHECKLIST_202610.md` |
| WW3's regtests on the rebased tree, lab switch, strict, POISON on: resident serial 5 PASS and `ww3_tp2.6` x2 FAIL by design (unstructured grid, kept FAIL by the owner); OMPG 10/10; `mww3_test_01` to `_05` 0 files different; `ww3_tnc1` 0/30 | `docs/log/2026-10-18-u3-regtests-v2.md`, `docs/data/ww3gpu_matrix_*_pin-50a10730_2026-10-18.tsv` |
| Plans 002 (this goal), 003 (the first supervised GPU run and the five plans after it), 004 (pin to `production/GFS.v17`, own PR) | `docs/compound/plans/` |

## Open work, by branch

| Branch | State | Next step |
|---|---|---|
| `goal/abbb0456-8241-strict-outg-poison` (ww3-gpu) and `ww3-gpu/strict-outg` (fork, `fac74069`) | the follow-up of #191's review findings 1 to 3, four commits, pushed, **no PR**; last commit is a `wip:` of the docs | run the resident serial matrix and fill `MATRIXRESULT` in `docs/log/2026-10-18-strict-outg-poison.md`; `port_score.py --write`/`--check`; security-reviewer (scripts/, regtests/); `just pr` with a hand-written body listing units 2 and 3 for human review |
| `goal/abbb0456-5370-gtest-infra-plan`, PR #198 | ADR-0019 (Proposed) and plan 006: GoogleTest for the runtime infrastructure only | the owner accepts or changes ADR-0019 and answers plan 006 T2's strict-parsing question; then plan 006 from T0, after the follow-up above merges |
| `goal/abbb0456-6043-fortran-ab` (`e9366f9`) | `scripts/ww3_fortran_ab.sh`, the A/B of a Fortran patch proposed to NOAA, and the AGENTS.md exception the owner approved; checked on smoke with a no-op, a positive control and a non-applying patch; **no PR** | the owner chooses where it goes (its own PR, or the NOAA study's) |

The follow-up's log already shows one consequence for the next run: `smoke` asks for `WBT` and
`SKW`, which `W3OUTG` leaves to the Fortran, so with strict mode a resident run of `smoke` stops
(37 output times fell back; `scripts/ww3-gpu.sh` sets no strict mode, so its `smoke` still passes),
and a matrix entry that asks for those fields fails under strict until they are translated.

## Waiting on the owner

- PR #191's review list: the five items under "Needs a person's review" in its body, and the items
  6 to 8 there.
- Run the restart pair: `~/code/ww3-gpu-3/test-191-restart.sh` (it runs `scripts/restart_pair.sh`,
  never run before). The other scripts there (`test-191-omp.sh`, `-multi.sh`, `-tnc1.sh`,
  `test-fortran-ab.sh`) reproduce what the records above already hold.
- The quiet-machine timing of `main` (one loaded run had Kokkos 46.4 s against the Fortran's
  31.8 s on `full`, against the owner's earlier 15.5 s against 23.4 s ⚠); then
  `docs/NOAA_PROGRESS_RECORD_202610.md`'s speed sentence and its facts #191 changed (seven preset
  parts, the pin `50a10730`, `ww3_multi` now shown).
- Branches: 52 of ww3-gpu's were deleted with the owner's approval (recovery log
  `~/branch-deletion-recovery-ww3-gpu-20261018.log`); 15 with unique commits and every fork branch
  were kept. On the fork, every branch but `develop`, `kokkos-pnt`, `strict-outg` and #196's
  `xypug-196` is now contained in `kokkos-pnt` and could go, once the follow-up's `.gitmodules`
  change has merged (it points `WW3` at `kokkos-pnt`).

## Traps met (for whoever continues)

- `git pull` does not move submodules: after a pull of `main`, run `git submodule update WW3`, or
  `WW3/` stays at an old pin. A resident matrix then fails every entry with `parts not on Kokkos in
  the Kokkos run: CPRT` and `test.comp` 0 (seen in `~/code/ww3-gpu-3`).
- Inside this repository's work tree, `git apply` skips paths outside the current directory without
  an error: `ww3_fortran_ab.sh` uses GNU `patch` and refuses a patch that leaves a named file
  unchanged.
- `W3SDS4` returns before its `END SUBROUTINE`: a positive control placed just before `END` is dead
  code.
- Two matrix runs started at once both configure `kokkos/build/openmp-release` and break
  `setup:kokkos`: run them one at a time.
- Piping `scripts/vale.sh` to `tail` in an `&&` chain hides its exit code: one commit of this goal
  (`c487900`) claimed a vale pass that had failed; `686dcd0` fixed it.
- PATCH.md section numbers are claimed across sessions: 30 is #196's (`W3XYPUG`), 31 the follow-up's.
