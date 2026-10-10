# ADR-0006: The repository-agnostic agentic research tooling moves to codigos.ufsc.br/agentic-research, and ww3-gpu vendors it back pinned

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-10 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | The scripts, hooks, skills and agents in this repository that do not depend on WW3, the proposal or the book; where they are developed, and how ww3-gpu consumes them. Not the lab code, the proposal's reviewer, the book pipeline or the third-party skills vendored here |
| **Related** | [ADR-0003](ADR-0003-repository-is-a-book.md) (the book whose method this tooling encodes); [ADR-0005](ADR-0005-claude-project-settings-in-repo.md) (the project settings, which stay here); [`.claude/skills/skills.lock`](../../.claude/skills/skills.lock) and [`skills.yml`](../../.github/workflows/skills.yml) (the vendoring this decision reuses); [`AGENTS.md`, "Skills and agents"](../../AGENTS.md#skills-and-agents) |

## Context

This repository grew a set of tools that make agents hold to the evidence rule: a Mermaid
harness that refuses a figure without a reading card, a design-proposal harness that ties each
proposal to a deliverable and a definition of done, an ADR index, a gate that refuses to end a
session whose reviewed text changed without a recorded review, a front-matter check for skills
and agents, a security reviewer, and a Vale style for AI-writing tells. Most of it says nothing
about waves. LabECO/UFSC co-advises the project, and the owner proposed giving the generic part
to a UFSC group where other research projects that use agents can share it, with ww3-gpu as one
consumer among them.

What codigos.ufsc.br is, checked on 2026-10-10:

- A GitLab Enterprise Edition instance that describes itself as "um serviço oferecido a
  comunidade universitária" `(v)` the `/explore` and `/users/sign_in` pages, fetched 2026-10-10.
  Its version was not read: the fetcher refused `/api/v4/version` under the host's `robots.txt` ⚠.
- Sign-in is by IdUFSC (the university's LDAP account, `nome.sobrenome`) or a standard GitLab
  account; the page has no registration link `(v)` `/users/sign_in`, 2026-10-10. A person outside
  UFSC needs an account an administrator creates ⚠ (inferred from the missing link; the
  instance's policy was not found in writing).
- Its public groups include SeTIC-HPC and Lab. Oceanografia Costeira, so research groups already
  host code there `(v)` `/explore`, first page, 2026-10-10.
- `codigos.ufsc.br/agentic-research` answers an anonymous request with the sign-in page `(v)`
  2026-10-10. GitLab does that for a private group and for a missing one alike, so whether the
  group exists, and who owns it, is unknown ⚠.
- This session's network proxy refused a direct `curl` to the host, so nothing above was read
  through the API; a CI job on GitHub's runners may see the same host differently ⚠.

What would move, measured on `main` at `deb3e9b` with
`wc -l scripts/{figures,wfip,adrs,proposal_review_gate}.py scripts/vale.sh .claude/hooks/* .claude/agents/security-reviewer.md .claude/skills/{figure,wfip,eli5}/SKILL.md` `(v)` 2026-10-10:

| Unit | Lines | What ties it to ww3-gpu today |
|---|---|---|
| `scripts/figures.py`, the `figure` skill, `tests/test_figures.py` | 434 + 99 | `REPO_URL` hardcoded (`scripts/figures.py:50`), the gallery path `pubs/figures/` |
| `scripts/wfip.py`, the `wfip` skill, `docs/WFIPs/TEMPLATE.md` | 285 + 57 | the `WFIP` prefix, the proposal's deliverable ids (D1–D6) in the template and the self-test, the GitHub URL at `scripts/wfip.py:225` |
| `scripts/adrs.py`, `/adrs`, `tests/test_adrs.py` | 40 | the path `docs/ADRs/` only |
| `scripts/proposal_review_gate.py`, `.claude/hooks/proposal-review*.sh` | 162 + 86 | the reviewed paths (`pubs/proposal/`) and the agent name `revisor-proposta` |
| `.claude/hooks/check_agent_frontmatter.py` | 67 | none |
| `.claude/agents/security-reviewer.md` | 64 | the list of sensitive paths |
| the `eli5` skill | 76 | its examples are wave physics |
| `.vale/styles/`, `scripts/vale.sh` | 41 + the styles | the excluded paths (`WW3/`, `pubs/`) |

About 1,400 lines of code and instructions, plus their tests. The gate pattern is the most
reusable piece: any project with a text that must pass a reviewer before merging needs it.

## Decision

**The units in the table above move to a project in `codigos.ufsc.br/agentic-research`, under
the MIT licence of this repository, and are developed there. ww3-gpu keeps no copy of its own: it
vendors them at a pinned commit through the same `skills.lock` and `skills-vendor` mechanism it
uses for its third-party skills, and its local differences live in `.patch` files.**

1. **Preconditions.** Nothing moves until (a) the group exists with a named UFSC maintainer, (b)
   the owner has a maintainer account on it, and (c) the project is Public. Public is not
   negotiable: a reader of the book and a CI runner on GitHub must fetch the pinned commit with no
   credential, as they fetch WW3 today. If UFSC will only host it as Internal or Private, this
   ADR is Rejected.
2. **What moves.** The table above, each unit made repository-agnostic before it leaves: paths,
   prefixes, URLs and deliverable ids become a config file read from the consuming repository
   (`agentic-research.toml` or the script's flags), with ww3-gpu's values as the example. The
   `(v)`/`⚠` evidence convention moves as a written template (a generic `AGENTS.md` section and
   the reading-card rule), since it is a rule, not code.
3. **What stays here.** Everything about WW3, the GPU port or this repository's own documents:
   `kokkos/`, `bench/`, `scripts/results.py`, the `noaa-notices` and `ww4-status` skills, the
   `revisor-proposta` agent and `scripts/proposal_lint.py` (they encode the Poli/UFRJ norm and the
   DEL structure, not UFSC's), the book pipeline (`book_prep.py`, `leanpub_manuscript.py`,
   `build_*.sh`, ADR-0004), the `release` skill and `codemeta.py` (they hold this repository's
   concept DOI), `.claude/project/` (ADR-0005), and this repository's `AGENTS.md` invariants.
4. **What is not ours to move.** `humanizar`, `humanizer`, the three `ponytail` skills and
   `sharingan` belong to their upstreams (`skills.lock`), and `skills-vendor`, `uprd` and the
   deny-branches rule come from marola-devkit. A UFSC project that wants them vendors them from
   their upstreams, not through ww3-gpu.
5. **History and authorship.** The extracted files keep their history (`git filter-repo --path`
   on a fresh clone), so each line keeps its commit and author. The copyright line stays
   Hoffmann's; contributions there come in under the same MIT terms.
6. **How ww3-gpu consumes it.** Each unit gets a `skills.lock` entry naming the codigos.ufsc.br
   project, the path and the commit; `skills.yml` re-vendors weekly, 14 days behind HEAD, and opens
   a PR for a person, as it does for the third-party skills now. The ww3-gpu config and the
   audience paragraphs are the `.patch`. A fix found here is made upstream first and arrives in the
   next vendoring PR; an urgent one goes in the `.patch` and is removed when upstream has it.

## Steps and effort

The estimate is from the line counts above and from what each step touches; nothing here has
been timed, so every figure is a projection ⚠.

| # | Step | Where | Estimate ⚠ |
|---|---|---|---|
| 0 | Ask the co-advisor for the group, a maintainer, Public visibility and an account for the owner | e-mail | outside this repository's control; it gates everything below |
| 1 | Make each unit read its paths, prefixes and URLs from a config file; ww3-gpu's tests pass unchanged with its own config | here, one PR | 2 days |
| 2 | Find whether `skills-vendor` accepts a non-GitHub upstream; its `upstream` field is `owner/repo` today `(v)` `.claude/skills/skills.lock`. If not, add a git-URL upstream to it | marola-devkit, one PR | 0.5 to 1 day ⚠ (its source was not read) |
| 3 | Extract the units with their history into the new project; add a README, the licence, the config example, and a GitLab CI job that runs the unit tests | codigos.ufsc.br | 1 day, if the instance has shared runners ⚠ (not checked) |
| 4 | Replace the local copies here with vendored ones: `skills.lock` entries, `.patch` files, `skills.yml` pointing at the new upstream; `just` recipes keep their names | here, one PR | 1 day |
| 5 | Update `AGENTS.md`, `CONTRIBUTING.md`, `CLAUDE.md`, the glossary and the lessons that name a moved file | here, same PR as 4 | 0.5 day |

About five working days of agent work plus the owner's review, spread over three repositories,
with step 0 as the only unbounded wait. Steps 1 and 2 can start before step 0 is answered:
step 1 removes hardcoded paths this repository should not have anyway, and step 2 is useful to
any marola repository that vendors from GitLab.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep everything here | Works, and is the fallback if a precondition fails. Other projects at UFSC then copy files by hand and the copies drift, which is the problem `skills.lock` already solved for the third-party skills. |
| Move it to a new GitHub repository instead | Simpler for CI and for `skills-vendor`, but it gives LabECO/UFSC nothing they host or co-maintain, and hosting there is the reason for the donation. Revisit if UFSC declines. |
| Move it into marola-devkit | marola-devkit is the marola organisation's harness; the evidence rule, the reading card and the review gate are research tooling and belong with a research group. |
| A git submodule pointing at the UFSC project | Every clone and CI job would then fetch from codigos.ufsc.br on each run, the files would not sit where Claude Code reads them (`.claude/skills/`), and ww3-gpu's local edits would need a fork there. Vendoring copies once per PR and keeps the patches here. |
| Mirror from GitHub to GitLab, keeping development here | Development would stay in ww3-gpu, so the UFSC project would be a read-only shadow and other projects could not contribute back. |
| Donate the proposal reviewer too | It checks Resolução 05/2012 of the Escola Politécnica and the DEL structure `(v)` [`.claude/agents/revisor-proposta.md`](../../.claude/agents/revisor-proposta.md); a UFSC thesis follows other norms. The gate that runs it moves; the reviewer stays. |

## Consequences

- A change to a moved tool takes two PRs, one there and the vendoring PR here, and waits up to the
  14-day lag unless forced. That is the price of sharing it.
- ww3-gpu's CI depends on codigos.ufsc.br being reachable when `skills.yml` runs; between runs the
  vendored copies are in git, so the gates and the book build never fetch from it.
- The book's chapters that describe these tools (the figure harness, the WFIPs, the review gate)
  point at the UFSC project as their source, and ww3-gpu becomes its first worked example.
- The owner becomes a co-maintainer of a project on UFSC's infrastructure, under UFSC's terms of
  use for that service ⚠ (not read).

## Revisit when

- Step 0 is answered: a refusal or a non-Public project makes this Rejected; the GitHub
  alternative above is then the next candidate.
- A second project consumes the tooling, the first evidence that the extraction serves more than ww3-gpu.
- `skills.yml` fails twice in a row because codigos.ufsc.br is unreachable from GitHub's runners.
- The tooling there diverges from what this repository needs often enough that the `.patch` files
  grow past the units they patch.
