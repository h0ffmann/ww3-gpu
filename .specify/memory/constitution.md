# ww3-gpu Constitution

The principles Spec Kit's `/speckit-*` skills check a spec, plan and tasks against. They restate
`AGENTS.md`, "Repo invariants", which wins where the two differ.

## Core Principles

### I. Write for scientists, carry evidence
The first readers are researchers in wave modelling, numerical methods and HPC. Every claim
carries `(v)` with what was checked, or `⚠` when it was not; every number comes with the command
that reproduces it, the unit, the hardware. No glossing of the physics.

### II. WW3 is read-only
No WW3 source is vendored or edited. `WW3/` and `$WW3` are references; the only Fortran added is a
`bind(C)` shim behind a switch. A WW3 fact cites `model/src/<file>.F90:<line>` at the pinned
commit.

### III. Translate, don't improve
A ported routine reproduces the Fortran's arithmetic. A change of reduction order, limiter or
integration order is its own PR with L2 evidence, asked about first. A tolerance is never widened.

### IV. No timing without parity
A speed figure enters a table only after the L1 gate passed on the same build, with the machine,
the command and the median of three runs.

### V. Lab code is Fortran, C++ and shell
Python appears only in publishing and repo tooling (`scripts/`, `tests/`), never in `examples/`,
`exercises/`, `kokkos/` or `bench/`. Namelists use `.nml` and are `⚠` until executed.

### VI. Design before build, report by deliverable
A non-trivial change starts as a WFIP (`docs/WFIPs/`) naming the proposal deliverable it serves,
with a definition of done in its §7; implementation is a separate PR. An idea with no issue is
not work yet; filing is a person's act.

## Workflow and gates

Commits carry `Tested:` and `Cost:` trailers; the PR is opened with `just pr` from a branch named
after the work. Gates: `just kokkos-test`, `python3 scripts/figures.py check`,
`python3 -m unittest discover tests`, `python3 scripts/wfip.py check`, `just proposal-lint` and
the `revisor-proposta` review for `pubs/proposal/`. A Spec Kit spec lives in
`specs/<NNN-slug>/spec.md` and is named in its WFIP's *Spec-kit* row.

## Governance

`AGENTS.md` and `CONTRIBUTING.md` are the source; this file is amended in the same change when
they are. A spec or plan that conflicts with a principle is wrong, not the principle.

**Version**: 1.0.0 | **Ratified**: 2026-10-08 | **Last Amended**: 2026-10-08
