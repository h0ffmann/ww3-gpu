---
title: Production-ready single-node Kokkos runtime, CPU, GPU-prepared - Plan
type: feat
date: 2026-10-18
artifact_contract: ce-unified-plan/v1
product_contract_source: owner brief, session of 2026-10-18
execution: code
---

# Production-ready single-node Kokkos runtime - Plan

## Goal Capsule

- **Objective:** the shared-memory (single-node) Kokkos runtime of the `h0ffmann/WW3` fork gives
  the Fortran's bits on every WW3 driver it accepts, says when it falls back, is configured
  without the lab script, builds for CUDA and installs as one optional dependency.
- **Means:** seven units (U1 to U7 below, the owner's numbering), fork changes as one commit
  series on `ww3-gpu/kokkos-pnt` and one pin bump, WW3's own regtests under
  `WW3GPU_LAB_SWITCH=ON` as the only tests (ADR-0010).
- **Authority:** AGENTS.md over this plan. ADR-0016 (this is a validation phase the owner
  called, 2026-10-18), ADR-0017 (MPI stays Fortran: no MPI port), ADR-0014 (a GPU run is the
  owner's, supervised; agents only `--build-only`).
- **Stop conditions:** a `test.comp` difference that a Fortran/Fortran run does not reproduce
  stops the unit and is reported; no tolerance is widened, no entry is dropped or marked
  expected-to-fail, no `--gpu` run.
- **Execution profile:** large loop; branch `goal/abbb0456-6043-production-single-node` from
  `origin/main` (`5b48e08`); independent units as sub-agents in worktrees, their commits
  cherry-picked onto the branch; one PR through `just pr`.

---

## Brainstorm outcome

Decisions taken before planning, each with the reason; the owner can reverse any of them in
review.

- **B1. The `ww3_multi` fix marks the host newer only where `ww3_multi` writes `VA`** (the
  owner, 2026-10-18: the fix must not cost speed). `W3WAVE` keeps its exit read
  (`w3wavemd.F90:3151-3153` at `80644ba9`); the host writes of `WMIOHG` (`wminiomd.F90:2264`)
  and `WMIOEG` (`:3261`, `:3278`, `:3326`, `:3330`) are each followed by
  `W3KOKKOS_VA_HOST_WRITE` behind `W3_KOKKOS`. `ww3_shel` pays no copy, `ww3_multi` pays one
  only on a step that writes. Rejected: marking the host newer at `W3WAVE`'s exit, one
  host-to-device copy of `VA` per grid per step, which on a GPU is a bus transfer the resident
  design exists to avoid. The cost is one more WW3 file in the fork diff (gap 12). A partial
  update of the columns written is a further option, taken only if small and clearly correct.
- **B2. Strict mode is one environment flag, `WW_KOKKOS_STRICT=1`**, read where the parts are
  resolved: `KOKKOS_PART` under `WW_KOKKOS_ALL=1` and the per-call `W3SRCE` gate stop with
  `EXTCDE` and a `NDSE` line naming the part and the reason, instead of falling back. Without
  it, behaviour is today's, plus one line at the end of the run with the per-part count of
  Fortran fallback calls. The matrix's Kokkos run sets `WW_KOKKOS_STRICT=1`; its Fortran run
  does not.
- **B3. Configuration, smallest useful form: `WW_KOKKOS_PRESET=<name>`**, read by `W3INIT`
  through the shim, with `resident` the only preset (its part list moved from
  `scripts/ww3-gpu.sh:190` into one table in `kokkos/src/fortran_iface/`, which the script then
  reads, so the two cannot drift). A per-part variable still overrides the preset. The effective
  set is the existing parts line, printed once on rank 0. No namelist: the brief says one only if
  needed, and a `&KOKKOS_NML` group adds a WW3 input-file format for one user; recorded as the
  next step of gap 7.
- **B4. Packaging: `WW_KOKKOS_BUILD_TESTS` (default ON here)**; OFF drops GoogleTest, NetCDF,
  the tools and the Fortran language from the configure. `ww_kokkosConfig.cmake` records
  `Kokkos_VERSION` and the enabled backends, and `find_dependency(Kokkos <version> EXACT)`.
  WW3's `find_package(ww_kokkos 0.9 REQUIRED)` is unchanged; the check lives in the package.
- **B5. The ADR on where `ww_kokkos` lives** is drafted Proposed with three options (external
  package WW3 finds; submodule of WW3; inside `model/src`), no recommendation beyond what the
  evidence of gap 12 supports, and the licence question marked ⚠.
- **B6. GPU readiness is a build and a checklist.** `--build-only` of the fork on the CUDA
  backend with the resident preset's parts, warnings read and listed; the checklist is a file the
  owner follows during a supervised run.

## Requirements

- R1. A SHRD `ww3_multi` entry under the lab switch with the resident preset and
  `WW_KOKKOS_VA_CHECK=1` passes `test.comp` against the Fortran, before and after the fix is
  shown (the reproduction fails first, or is shown not to fail with the reason).
- R2. With `WW_KOKKOS_STRICT=1` no run can complete on the Fortran for a part it asked on
  Kokkos; the matrix's Kokkos runs use it.
- R3. The 7 resident serial entries, the OMPG entries with the part subset, and one restart
  pair pass under the lab switch, each recorded in `docs/data/` and `docs/log/`.
- R4. `WW_KOKKOS_PRESET=resident` alone gives the same parts line as `--kokkos resident`.
- R5. The fork and `ww_kokkos` build on Kokkos' CUDA backend, `--build-only`; a checklist for
  the owner's supervised run exists.
- R6. `ww_kokkos` configures and installs with Kokkos as its one dependency; the ADR is Proposed.
- R7. The stale remark of gap 13 is corrected.

## Scope boundaries

- Out: MPI (ADR-0017), `PDLIB`, any GPU run, any timing in a table (the owner's, median of
  three on a quiet machine), the profile's speed proposals (each its own PR with L2 evidence).
- A change to `regtests/` or to how a comparison is invoked (U2) is reviewed by a human before
  merge; the PR body says so.

## Implementation units

Fork work: each unit that changes the fork works on its own fork branch from `80644ba9`
(`ww3-gpu/u1-multi-va`, `ww3-gpu/u2-strict-preset`, `ww3-gpu/u6-package`), and the integrator
merges them, in that order, into `ww3-gpu/kokkos-pnt`, then bumps the pin once. Every fork change
gets a section in `kokkos/src/fortran_iface/PATCH.md`.

### U1 + U7. `ww3_multi` and the resident `VA`; the stale remark (agent A)

- **Files:** fork `model/src/w3wavemd.F90`, `w3kokkosmd.F90:99-104`; `PATCH.md`; possibly
  `kokkos/src/fortran_iface/resident_shim.cpp`.
- **Approach:** reproduce first: the `mww3_test_0x` entries that are SHRD with
  `WW3GPU_LAB_SWITCH=ON`, the resident preset, `WW_KOKKOS_VA_CHECK=1`; and a part subset
  where `XYP3` refuses (`SRCE,KTP3,OUTG,IOPE,UPDV`). Then B1, then the same entries
  byte-identical. Say whether this explains `mww3_test_08` (MPI): read its path, do not port MPI.
- **Done:** a `docs/log/` entry with the commands, before and after; the fork commits pushed.

### U2 + U4. Strict mode and the preset (agent B)

- **Files:** fork `w3kokkosmd.F90` (`KOKKOS_PART`, `W3KOKKOS_SRCE_ON`), `w3initmd.F90`;
  `kokkos/src/fortran_iface/part_flags.cpp`, a preset table; `scripts/ww3-gpu.sh`;
  `regtests/cmake/run_entry.cmake` (Kokkos run sets `WW_KOKKOS_STRICT=1`) and
  `regtests/cmake/selftest.cmake`; `PATCH.md`; `docs/GLOSSARY.md`.
- **Done:** `just ww3-gpu-matrix-selftest`, `bash scripts/ww3-gpu.sh --self-test`, shellcheck,
  `just kokkos-test serial-debug` and `openmp-release`; `just smoke-ww3-gpu` with the preset
  from `WW_KOKKOS_PRESET` only, parts line identical to `--kokkos resident`.

### U3. Wider CPU regtest coverage (agent E, after integration)

- **Approach:** on the integrated pin, `WW3GPU_LAB_SWITCH=ON`: the resident serial selection;
  the OMPG selection with the part subset; the restart-reproduction pair of a SHRD case (gap 10).
  Failures go through U4a of `2026-10-18-001-cpu-resident-validation-plan.md`.
- **Done:** summaries in `docs/data/`, one `docs/log/` entry, `kokkos/ports.tsv` `regtested`
  only where the record supports it (`port_score.py --write`, `--check`).

### U5. GPU readiness, build-only (agent D; the final build again after integration)

- **Files:** `docs/GPU_SUPERVISED_RUN_CHECKLIST_202610.md`; the build log summary in `docs/log/`.
- **Done:** `scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --build-only` with the resident preset and
  `just kokkos-test cuda-release` built (no run on the device), warnings listed; the checklist
  names `scripts/libm_bits.sh --device`, `just smoke-ww3-gpu gpu`, `just full-ww3-gpu gpu`,
  `just ww3-gpu-matrix-cuda` on the entries U3 passed, and what to record for each (ADR-0014).

### U6. Packaging and the ADR (agent C)

- **Files:** `kokkos/CMakeLists.txt`, `kokkos/cmake/ww_kokkosConfig.cmake.in`, `kokkos/README.md`,
  `docs/ADRs/ADR-0018-*.md` (Proposed), the ADR index.
- **Done:** an install-only configure and install in a scratch prefix with only Kokkos found;
  the installed config records the Kokkos version and backends; WW3 builds against that prefix
  (`just smoke-ww3-gpu` on it); `just kokkos-test serial-debug` still passes.

## Integration and gates (the integrator)

Merge the fork branches, push `ww3-gpu/kokkos-pnt`, bump `WW3/`, cherry-pick the units' commits,
update `docs/PRODUCTION_READINESS_202610.md` (each gap's state), then the gates of AGENTS.md for
what changed, `security-reviewer` (scripts/ changes), a report-only code review, `just pr`.

## Owner steps (listed in the PR body)

- U5 timing: median of three on a quiet machine, then the `bench/results/` record.
- U6: whether the lab-switch evidence lets `kokkos/ports.tsv` rows move to `regtested` "yes".
- Any GPU run of U5.
- Whether to act on the profile's speed proposals.
