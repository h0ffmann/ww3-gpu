---
name: wfip
description: "Write, revise or report on a Wave Forecaster Improvement Proposal (WFIP), the numbered design doc under docs/WFIPs/ that ties a non-trivial change to a deliverable of the project proposal and carries its definition of done. Use when the user says WFIP, WF-001, improvement proposal, propose, design before building, definition of done, DoD, Spec Kit or specify, or asks what the project has delivered against the proposal."
---

**Audience.** A WFIP is read by scientists (`CONTRIBUTING.md`, "Who this repository is for") and,
through the index, by the supervisors and the DEL committee following the proposal's
deliverables. Every external fact carries `(v)` with URL and date or `⚠`; every number carries the
command that reproduces it; no model name appears in the repository.

# wfip

A WFIP (`docs/WFIPs/`) is marola's MIP (`marola-dev/marola`, `docs/MIPs/`) renamed and given two
things the proposal needs: a **Deliverable** row naming which of D1–D6
(`docs/WFIPs/README.md`, "Proposal deliverables") the work moves, and a §7 whose `- [ ]` boxes are
the definition of done. `scripts/wfip.py` turns the files into the index, the coverage table and
the dependency graph; nothing in `README.md` below the deliverables table is written by hand.

## When a WFIP is warranted

A new data path or forcing, a new program or example that runs the model, a port or a change to
how one is validated, a measurement campaign, a tool the lab will rely on. Not for a doc fix, a
refactor with no behaviour change, or a one-file tweak: do those directly.

## Write one

1. `just wfip new <slug> --title "<title>" --deliverable D5` creates the next number from
   `TEMPLATE.md`. The number is one more than the highest in the index *and* in open PRs.
2. Read first: `AGENTS.md`, the plan the idea belongs to (`docs/KOKKOS_H100_PLAN_202609.md`,
   `docs/W3SDS4_TRITON_PLANO_202610.pt.md`, issue #45), the proposal's objective the deliverable
   comes from (`pubs/proposal/pt/06-objective.md`), and the files the WFIP would touch.
3. Fill every section; "None" rather than a deleted heading; §10 skipped on purpose. Each §7 box
   is one verifiable outcome with its command. Each §11 question carries a **Default**.
4. Spec Kit, when the change has user-facing behaviour worth specifying: `just specify init`
   once per clone (it generates `.specify/` and the `/speckit-*` skills, which are gitignored;
   `.specify/memory/constitution.md` is tracked and restates `AGENTS.md`), then
   `/speckit-specify` writes `specs/<NNN-slug>/spec.md`; name it in the *Spec-kit* row. A spec
   written by hand in that shape is fine (`specs/001-weathernext3-wind/spec.md`).
5. `just wfip index` regenerates `README.md`; `just wfip check` is the CI gate.
6. Commit with `Tested:` and `Cost:`; open the PR as a draft with `just pr` from a branch named
   after the work; the PR gets the `wfip` label from `.github/labeler.yml`.
7. Do not build it in the same PR. Implementation PRs tick §7 boxes, update **Cost so far** from
   their `Cost:` trailers, and the last one flips Status to Implemented; `wfip check` refuses
   Implemented with an unticked box.
8. Filing the issues of the tasks file is a person's act (`AGENTS.md`); the *Issues* row says
   `not filed: Draft` until then.

## Report progress

- `docs/WFIPs/README.md` is the standing report: status, DoD share and deliverable coverage. A
  tagged release archives it with the source on Zenodo.
- `just wfip status --since <previous tag>` prints the created and revised WFIPs with their
  status and DoD, the index and the coverage table, as Markdown with absolute links; the
  release skill pastes it into the release notes so the version's notes on GitHub and Zenodo
  say what moved against the proposal (`.claude/skills/release/SKILL.md`).
- Weekly post or a note to a supervisor: the same output, read aloud; the "why" is in each
  WFIP's §2 and §9.
