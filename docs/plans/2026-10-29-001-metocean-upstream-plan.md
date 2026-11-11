---
title: Move the WW3 upstream from NOAA-EMC/WW3 to metocean/WW3 7.14.mos-92e2cf6 - Plan
type: chore
date: 2026-10-29
artifact_contract: ce-unified-plan/v1
product_contract_source: "owner, session of 2026-10-29 (goal ef7f3ca1-4821), 'change from NOAA/WW3 and move to metocean/WW3 with includes 7.14 version'; ref 7.14.mos-92e2cf6 and 'metadata now, fork later' chosen in that session"
execution: code
---

# Move the WW3 upstream from NOAA-EMC/WW3 to metocean/WW3 7.14.mos-92e2cf6 - Plan

## Goal Capsule

- **Objective:** `WW3/` (the h0ffmann/WW3 fork) carries the KOKKOS patch on `metocean/WW3`
  `92e2cf66` instead of NOAA-EMC/WW3 `03adda91`, the reference submodule that `--ww3` builds is
  that same commit, and every `file:line`, pin and parity claim of this repository names it.
- **Why:** LabECO's operational system builds MetOcean's WW3 (the owner, 2026-10-29); speed gained
  on NOAA's ST4 is not speed for that system, because the arithmetic differs
  ([log 2026-10-29](../log/2026-10-29-metocean-ww3-714-mos.md)).
- **Authority:** AGENTS.md ("WW3 source is read-only", "Translate, don't improve", "Bit for bit",
  "Third-party issues"); ADR-0016 (regtests in the validation phase); ADR-0002 (port order by wall
  time, which a new profile at the new pin may reorder).
- **Done already (this goal's first PR):** `docker/Dockerfile.ondaseventos`, `VERSIONS.md`,
  `docs/GLOSSARY.md`, the log entry. Nothing below is done.
- **Stop conditions:** a conflict in the fork whose resolution would edit WW3's own lines rather
  than the KOKKOS patch's; a ported kernel whose MOS arithmetic cannot be shown statically to match
  (then the routine's `ported` claim is withdrawn, not argued); any `--gpu` run (never without the
  owner).
- **Size:** large (AGENTS.md, "Size the loop to the change"); `/ce-brainstorm` is replaced by the
  log entry's survey of the refs.

## What changes, at the pins read on 2026-10-29

| | Today | After |
|---|---|---|
| Reference WW3 (`--ww3`) | submodule `WW3-noaa/`, NOAA-EMC/WW3 `03adda91` (2026-10-05) | a submodule of `metocean/WW3` at `92e2cf66` (2026-09-01) |
| Fork base | NOAA `03adda91`, 47 additive commits, 16 files | `92e2cf66`, the same 47 commits rebased |
| NOAA history under the fork | to 2026-10-05 | to 2023-06-08 (`3eb8161`), plus MOS's ten commits |
| ST4 arithmetic | NOAA's | MOS's (`w3src4md.F90:680`, `:780`, `:835`, `:1683`; `w3srcemd.F90:1048`, `:1071`, `:1178`, `:1891` at `92e2cf66`) |
| Manual LaTeX | `manual/` at `03adda91` | `manual/` at `92e2cf66`; 11 files differ, `eqs/ST4.tex` and `eqs/NL1.tex` among them (`diff -rq`) |

Moving to an older base means losing NOAA's changes of 2023-06 to 2026-10: 150 commits
(`gh api repos/NOAA-EMC/WW3/compare/3eb8161...03adda91 --jq .ahead_by`) (v). They reach the
routines already ported: `w3snl1md.F90` differs in 1 379 diff lines between the two bases and
`w3src4md.F90` in 768 (`diff` of the files at `03adda91` and `92e2cf66`, counting `<`/`>` lines)
(v); at `92e2cf66`, `W3SNL1MD` has neither the GQM routines its header dates 2023-02-28 nor the
2025-07-04 "Remove labelled statements". Which of the fork's shims depend on post-2023 code is not
checked ⚠.

## Units

### U1. Rebase the fork onto `92e2cf66`

- New branch `ww3-gpu/kokkos-mos` on h0ffmann/WW3 from `92e2cf66`; cherry-pick the 47 commits of
  `ww3-gpu/kokkos-pnt` in order. Never force-push the existing branch: the current gitlink must
  stay buildable.
- Expect conflicts where MOS edits the same files: `w3srcemd.F90` (MOS +9 −7, patch +456),
  `w3initmd.F90` (MOS +157 −10, patch +119), `model/src/CMakeLists.txt` (MOS +13, patch +69).
  A resolution keeps MOS's line and puts the patch's block beside it.
- Where the patch relies on code NOAA added after 2023-06-08, record it in `PATCH.md` and stop:
  the choice between porting it and dropping the feature is the owner's.
- Gate: the fork builds with `switches/switch_lab_shrd` and with the ondaseventos switch, KOKKOS
  off and on (`bash scripts/ww3-gpu.sh --build-only`).

### U2. Replace `WW3-noaa/`

- Name: `WW3-upstream/` pointing at `https://github.com/metocean/WW3` `92e2cf66`, or keep the path
  and change only the URL. **Owner's choice** ⚠: a rename touches 373 files (`git grep -l WW3-noaa`),
  a URL-only change leaves a misleading name.
- `scripts/ww3-gpu.sh`'s check that the fork differs from the reference only by the KOKKOS patch
  runs against the new reference; `.gitmodules`, `just get`, `docker/Dockerfile`,
  `docker/Dockerfile.cuda`, `.github/workflows/health.yml` and `docker-dist.yml` follow.
- Gate: `bash scripts/ww3-gpu.sh --self-test`, shellcheck, `just smoke` (CPU) in the validation
  phase.

### U3. Re-translate what MOS changed

- `W3SIN4` (`USTAR` floor, two `ISNAN` guards), `TABU_TAUHF2` (`MAX(TAUW,0.0001)`), and `W3SRCE`'s
  `USTAR` floors: the Kokkos code reproduces MOS's lines, nothing more (Translate, don't improve).
  `ISNAN` in C++ must not be removed by `-ffast-math`-style flags; the kernel flags already forbid
  them, to be re-read in `kokkos/CMakeLists.txt` ⚠.
- `W3SNL1`, the first port: its Kokkos kernel was translated from `03adda91`, whose `W3SNL1MD` is
  not the one at `92e2cf66` (above). The static parity argument (`-O3` GIMPLE of both compilers)
  is redone against the older Fortran before the port is called bit for bit at the new pin.
- Every other ported routine: diff its Fortran between `03adda91` and `92e2cf66`; an unchanged
  routine keeps its static parity argument, a changed one is re-translated and re-argued.
- `kokkos/ports.tsv`: rows whose Fortran changed lose `wired` until re-argued; `regtested` stays
  "no: postponed (ADR-0016)". `python3 scripts/port_score.py --write`, then `--check`.

### U4. Re-cite

- Every `model/src/<file>.F90:<line>` in docs, port reports, lessons and the glossary names the new
  pin; `just src-st` prints it. The line numbers move (for example `UST=USTAR` is `:695` at
  `03adda91`, `:680` at `92e2cf66`). A script that rewrites them from a symbol, not by offset, is
  part of this unit ⚠ (not designed).
- AGENTS.md and CONTRIBUTING.md: "pure NOAA-EMC/WW3" becomes the new reference, in the same change
  (AGENTS.md, "When something here turns out to be wrong"). The owner agrees to that edit in its
  session.
- The proposal (`pubs/proposal/`) says WW3 is developed by NCEP/NOAA; that stays true. If it is to
  name MetOcean's build, that change goes through `revisor-proposta`.

### U5. Re-measure

- New profile at the new pin (`docs/data/ww3_*_gprof.tsv`) before choosing the next port
  (ADR-0002); the old timings stay as records of the NOAA pin, never relabelled.

## Open questions for the owner

1. The name of the reference submodule (U2).
2. Whether NOAA changes after 2023-06-08 that the patch needs are ported onto MOS or the feature
   is dropped (U1).
3. Whether ondaseventos' real switch file (not yet read, ⚠) replaces the one in
   `docker/Dockerfile.ondaseventos`.
