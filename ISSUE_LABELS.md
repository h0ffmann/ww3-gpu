# Issue labels

What each label on h0ffmann/ww3-gpu means, when to set it, and how a label is added or changed.
The list itself is [`.github/labels.yml`](.github/labels.yml): every label with its colour and
description, in one file a pull request can review. GitHub's label settings page is not the source
of truth; a label made there and not in the manifest is an orphan the next sync reports.

The discipline is marola-devkit's (MIP-0063 §5.2, MIP-0070 §5.7): the same manifest shape, the same
sync tool (`scripts/issues.sh labels sync` at a pinned devkit tag) and the same reusable workflow.
[Adapted from marola-devkit](#adapted-from-marola-devkit) lists what this repository changes.

## The groups

An issue carries at least one `area/*`. `size/*`, `priority/*` and `kind/*` are optional. A pull
request doesn't need a label: `.github/labeler.yml` sets `wfip` from the paths it touches.

### `area/*`: which part of the work

| Label | Set it when the issue is about |
|---|---|
| `area/kokkos` | the C++/Kokkos port in `kokkos/`: a kernel, its `bind(C)` shim, `kokkos/ports.tsv`, a port report |
| `area/regtest` | WW3's own regtests as this repository runs them: `just rt`, `just smoke-ww3-gpu`, `just full-ww3-gpu`, the regtest matrix in `regtests/` (WFIP-0003), their references |
| `area/parity` | a ported routine against the Fortran it replaces: a file `test.comp` finds different, a `ww3_self` excuse, the GIMPLE comparison of ADR-0011 |
| `area/gpu` | the CUDA backend, a GPU build, GPU hardware (the RTX 4090, the H100) |
| `area/bench` | a timing or a profile: `bench/`, a record in `bench/results/`, the timing column of `kokkos/PORT_STATUS.md` |
| `area/nix` | the pinned toolchain: the `picasso` shells, `flake.nix`, compilers, `just prereqs` |
| `area/docker` | a container image, `.devcontainer/`, `just dbuild`/`drun`, the Trivy scan |
| `area/ufsc` | LabECO/UFSC: the H100 machine, access to it, the co-advising |
| `area/proposal` | the UFRJ/DEL project proposal in `pubs/proposal/` |
| `area/book` | the book: the `course/` chapters, `examples/`, `exercises/`, the PDF and Leanpub builds |
| `area/noaa` | a NOAA announcement or an upstream WW3/WW4 change that dates or breaks a file here (the `noaa-notices` and `ww4-status` skills) |
| `area/dev-tooling` | agents, skills, hooks, CI and the scripts behind `just`, rather than the lab code |

Areas overlap on purpose. A regtest of the Kokkos DIA on the H100 is `area/regtest`, `area/gpu` and
`area/ufsc`; filter by one of them and it shows.

### `kind/*`, `size/*`, `priority/*`

| Label | Meaning |
|---|---|
| `kind/research` | a spike, survey or evaluation: the output is findings in comments, a `docs/log/` entry or new WFIPs, rather than code; the Spike form sets it |
| `size/S`, `size/M`, `size/L` | under ~100 changed lines, 100 to 400, over 400; `size/L` means split it and is only legal on a parent issue (MIP-0063 §5.2) |
| `priority/high` | do before other open work of the same area |

`bug`, `documentation`, `duplicate`, `enhancement`, `good first issue`, `help wanted`, `invalid`,
`question` and `wontfix` are GitHub's defaults, kept with GitHub's colours and descriptions.

### Labels that automation reads

These keep their names because something reads them. Rename one only in the same pull request
as everything that reads it.

| Label | Read by |
|---|---|
| `wfip` | `.github/labeler.yml` sets it on a PR that touches `docs/WFIPs/` or `specs/`; `.github/release.yml` gives it a heading in the release notes |
| `A2A`, `para:marola`, `para:wave-forecaster` | the messages between Claude projects ([AGENTS.md](AGENTS.md#messages-between-claude-projects), [lesson 19](course/19-two-agents-one-issue-tracker.md)); the routines list issues by `para:*`; their descriptions stay in Portuguese, as the protocol was written |
| `skills-update` | `.github/workflows/skills.yml`: marola-devkit's skills-update workflow opens its PR with it |
| `kimi` | Kimi Code sessions and improvements (#142) |

## Changing a label

1. Edit `.github/labels.yml`. The shape is fixed and parsed strictly: one `- name:` per label, then
   `color:` (lowercase hex, no `#`) and `description:` (100 characters at most, GitHub's limit),
   in that order. Anything else is an error, not a warning.
2. Check the plan: `just labels-sync --dry-run` parses the manifest, reads the repository's labels
   and prints the `gh` calls it would make, without making them.
3. Open the pull request with the manifest and this file. On merge,
   [`.github/workflows/labels.yml`](.github/workflows/labels.yml) applies it; `just labels-sync`
   does the same from a terminal.

A sync creates what is missing and edits what differs. It never deletes: a label on the repository
and not in the manifest is listed as an orphan. `just labels-sync --prune` deletes orphans, and
refuses to delete more than half the repository's labels without `--force`.

**A rename is done by hand first.** The sync matches labels by name, so a renamed entry in the
manifest reads as a new label plus an orphan: a sync would create the new one empty, and `--prune`
would delete the old one from every issue carrying it. Rename the label on GitHub before the sync,
which keeps every issue on it:

```bash
gh api -X PATCH "repos/h0ffmann/ww3-gpu/labels/$(jq -rn --arg n 'old name' '$n|@uri')" -f new_name='new/name'
```

then record the rename in the table below.

## Renames and removals

| Date | Before | After | Issues it moved |
|---|---|---|---|
| 2026-10-16 | `priority: high` | `priority/high` | #143 |
| 2026-10-16 | `inbound:noaa` | `area/noaa` | #85, #86 |
| 2026-10-16 | `R&D` | `kind/research` | #81 |
| 2026-10-16 | `accessibility` | deleted | none; it came from marola's manifest and has no meaning here |

## Adapted from marola-devkit

Followed as written: the manifest at `.github/labels.yml` and its shape, `issues.sh labels sync`
with `--dry-run`, `--prune` and `--force`, the reusable `labels-sync` workflow on a push to `main`
that changes the manifest and on dispatch (pinned to `v0.11.1`, the tag `skills.yml` uses), the
`group/value` spelling (`area/*`, `kind/*`, `size/*`, `priority/high`), and `size/*`'s rule and
descriptions.

Adapted:

- **No `agent-ready` gate.** ww3-gpu is not a marola-dev repository and AGENTS.md has no
  Definition of Ready; an idea with no issue is not work yet, and filing is a person's act.
- **No `layer/*` or `phase/*`.** marola's layers are its code directories and its phases are
  `docs/PHASES.md`; neither exists here. `area/*` carries both jobs.
- **A local recipe that fetches the devkit.** This repository has no devkit flake input, so
  `just labels-sync` (`scripts/labels_sync.sh`) clones marola-devkit at `DEVKIT_REF` into
  `~/.cache/ww3-gpu/` and runs its `issues.sh` from the repository root.
- **Labels that predate this file keep their names** where automation reads them (`wfip`, `A2A`,
  `para:*`) or an issue already carries them (`kimi`), outside the `group/value` spelling.
