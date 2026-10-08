# SPIKE-001 quick wins: research metadata, forms, log, results data, agent environment

2026-10-08, an agent session for Hoffmann, issue #65 section 1.

## Question

Which of the ten quick wins in #65 can be built and gated in one change, and what stops the rest?

## What was done

On `main@483e746`, in a cloud container (Ubuntu 24.04, Python 3.13, no GPU, no Docker daemon):

- `scripts/codemeta.py` writes `codemeta.json` from `CITATION.cff` and `.zenodo.json`;
  `citation.yml` fails when it is stale. `cffconvert 2.0.0 -f codemeta` was tried first `(v)`
  and rejected: it renders the two licences as one URL,
  `https://spdx.org/licenses/['MIT', 'LGPL-3.0-or-later']`.
- Three issue forms in `.github/ISSUE_TEMPLATE/`: `spike.yml`, `measurement.yml`, `wfip-idea.yml`.
- This log (`docs/log/`).
- `bench/results/snl1-i9-14900-rtx4090-2026-09-15.json` holds the 2026-09-15 W3SNL1 timings;
  `scripts/results.py` generates the timing table in `kokkos/PORT_STATUS.md` from it, and its
  `check` (CI, `lint` job) refuses a record whose `l1` is not `passed: ...`. The ledger's three
  timing cells link the commands that reproduce them.
- `scripts/agent_env.sh` (apt: just, shellcheck, gfortran, PyYAML; `nix-config` over https,
  sparse to `labs/pratico`), run by a `SessionStart` hook in cloud sessions only and by
  `.devcontainer/devcontainer.json` after create.
- Both release workflows open the notes with `wfip.py status --since <previous tag>`, in place of
  a hand-kept `CHANGELOG.md`: the GitHub release is what Zenodo archives, so the notes are where a
  citing reader looks.

## Result

- `(v)` `python3 -m unittest discover tests`, `just results check`, `just codemeta --check`,
  `python3 scripts/wfip.py check`, `python3 scripts/figures.py check`,
  `python3 .claude/hooks/check_agent_frontmatter.py`, `shellcheck` on both new scripts: all pass
  (the commit's `Tested:` trailer lists the output).
- `(v)` The hook ran in this container from a clean state: it installed just 1.21.0, shellcheck
  0.9.0 and gfortran 13.3.0 from apt and checked out `nix-config@2f48d55`; a second run did
  nothing. Outside a cloud session (`CLAUDE_CODE_REMOTE` unset) it exits at once.
- `⚠` The devcontainer was not built: `devcontainers/cli up` failed because this container has
  the Docker client but no daemon. Its cold-start time, which #65 asked for before committing to
  it, is still unmeasured.
- `⚠` The derived columns of the SNL1 table (points/s, speed-up) are kept as recorded on
  2026-09-15. Recomputed from the rounded medians they give 4.3x and 35x, not 4.4x and 34x; the
  unrounded medians were not kept, and the build commit was not written down (the record says
  so in `commit_note`).

## What it changes

- Next timing run writes its record in `bench/results/` with the build commit and the unrounded
  medians, so derived columns can be computed instead of copied.
- Two quick wins stay open, both outside this change: the Software Heritage archive (an external
  submission, for Hoffmann to make at archive.softwareheritage.org, then the SWHIDs go into the
  README's "How to cite") and the `docs/ADRs/` index (waits for ADR-0001, PR #43).
