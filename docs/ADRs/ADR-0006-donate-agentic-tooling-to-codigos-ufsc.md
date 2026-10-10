# ADR-0006: The repository-agnostic agentic research tooling moves to codigos.ufsc.br/agentic-research, and ww3-gpu consumes it as a pinned submodule

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-10 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | The scripts, hooks, skills and agents in this repository that do not depend on WW3, the proposal or the book; where they are developed, and how ww3-gpu consumes them. Not the lab code, the proposal's reviewer, the book pipeline or the third-party skills vendored here |
| **Related** | [ADR-0003](ADR-0003-repository-is-a-book.md) (the book whose method this tooling encodes); [ADR-0005](ADR-0005-claude-project-settings-in-repo.md) (the project settings, which stay here); [`.gitmodules`](../../.gitmodules) (the submodules this one joins); [`.claude/skills/skills.lock`](../../.claude/skills/skills.lock) (the vendoring weighed against it); [`AGENTS.md`, "Skills and agents"](../../AGENTS.md#skills-and-agents) |

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
- One request named `codigos.ufrj/agentic-research`. This record assumes UFSC, where the
  project is co-advised and where the first request pointed. `codigos.ufrj.br` answers with a
  `robots.txt` that kept its pages from being read, so whether UFRJ runs a similar service is
  unknown ⚠; if it does and is the intended host, only the host name below changes.

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

**The units in the table above move to a Public project, `codigos.ufsc.br/agentic-research/<project>`,
under the MIT licence of this repository, and are developed there. ww3-gpu checks that project out
as a git submodule at `tools/agentic-research/`, pinned to a commit like `WW3/`, `WW4/` and
`nix-config/`, and keeps no copy of its own. Root-level symlinks and hook commands point Claude
Code at the files in the submodule; what differs for ww3-gpu is a config file here, not an edit
there.**

1. **Preconditions.** Nothing moves until (a) the group exists with a named UFSC maintainer, (b)
   the owner has a maintainer account on it, and (c) the project is Public. Public is not
   negotiable: a reader of the book and a CI runner on GitHub must fetch the pinned commit with no
   credential. If UFSC will only host it as Internal or Private, this ADR is Rejected.
2. **What moves.** The table above, each unit made repository-agnostic before it leaves: paths,
   prefixes, URLs and deliverable ids are read from `agentic-research.toml` in the consuming
   repository's root, with ww3-gpu's values as the example. The `(v)`/`⚠` evidence convention
   moves as a written template (a generic `AGENTS.md` section and the reading-card rule). Each
   skill states its audience rule as "the consuming repository's `AGENTS.md`", so ww3-gpu's
   audience paragraphs stay in ww3-gpu's `AGENTS.md`.
3. **What stays here.** Everything about WW3, the GPU port or this repository's own documents:
   `kokkos/`, `bench/`, `scripts/results.py`, the `noaa-notices` and `ww4-status` skills, the
   `revisor-proposta` agent and `scripts/proposal_lint.py` (they encode the Poli/UFRJ norm and the
   DEL structure, not UFSC's), the book pipeline (`book_prep.py`, `leanpub_manuscript.py`,
   `build_*.sh`, ADR-0004), the `release` skill and `codemeta.py` (they hold this repository's
   concept DOI), `.claude/project/` (ADR-0005), and this repository's `AGENTS.md` invariants.
4. **What is not ours to move.** `humanizar`, `humanizer`, the three `ponytail` skills and
   `sharingan` belong to their upstreams and stay vendored through `skills.lock`; `skills-vendor`,
   `uprd` and the deny-branches rule come from marola-devkit.
5. **History and authorship.** The extracted files keep their history (`git filter-repo --path`
   on a fresh clone), so each line keeps its commit and author. The copyright line stays
   Hoffmann's; contributions there come in under the same MIT terms.

### Why a submodule, and not vendoring

Both pin a commit; they differ in where the files live and where an edit is made.

| | Submodule (chosen) | Vendored copy through `skills.lock` |
|---|---|---|
| Edit the tools while working on ww3-gpu | in place, in `tools/agentic-research/`, pushed to GitLab | in the upstream clone, then wait for the vendoring PR |
| ww3-gpu-specific differences | a config file here | `.patch` files re-applied on every update |
| A clone or CI job fetches from codigos.ufsc.br | each time it inits the submodule | only when the weekly update runs |
| GitHub's source archive of a release, and the Zenodo deposit made from it | omits the submodule's files ⚠ (GitHub's documented behaviour, not checked on this repository's deposits) | contains them |
| Needs a change outside this repository | none | `skills-vendor` must accept a non-GitHub upstream ⚠ (its `upstream` field is `owner/repo` today `(v)` `skills.lock`) |

The owner asked to operate the tools there as a subrepository, and the first row decides it: with
a copy, every fix to a shared tool is a round trip through another clone and a vendoring PR.
The cost is the archive row. ww3-gpu already accepts it for `WW3/` and `WW4/`, and the book's PDF,
which is the edition of record (ADR-0004), is built with the submodule present, so the gates that
shaped it ran on the pinned tools.

### How skills, agents and hooks run from the submodule

Claude Code's rules for nested directories decide the layout `(v)` code.claude.com/docs,
`skills`, `sub-agents` and `settings` pages, read 2026-10-10:

- **Skills.** A `.claude/skills/` below the session's start directory loads only after Claude
  reads or edits a file under it, and is then named with its path when it clashes with a root
  skill. A skill entry in the project's own `.claude/skills/` "can be a symlink to a directory
  elsewhere on disk". So each moved skill is a symlink at the root,
  `.claude/skills/figure -> ../../tools/agentic-research/skills/figure`, and loads at startup
  under its plain name. `.agents/skills` already links to `.claude/skills`, so Antigravity
  follows the same links.
- **Agents.** Claude Code scans every `.claude/agents/` from the start directory up to the
  repository root, not down into subdirectories, so a session started at the root does not see
  `tools/agentic-research/.claude/agents/`. Each moved agent is a symlink in the root's
  `.claude/agents/`. A symlinked agent file is not documented either way ⚠; step 4 tests it, and
  if it fails the agent file is copied by `just agentic-sync` and checked for drift in CI.
- **Hooks.** Only the root `.claude/settings.json` and `settings.local.json` load; a
  `settings.json` inside the submodule does nothing. The hooks stay declared here and call the
  scripts in the submodule, as `bash tools/agentic-research/hooks/review-gate.sh`, with the
  reviewed paths and agent name read from `agentic-research.toml`.
- **Scripts and `just`.** Recipes keep their names (`just figures`, `just wfip`, `just adrs`) and
  call `tools/agentic-research/scripts/`. CI checks the submodule out over https in the jobs that
  run those gates, as `ci.yml` already does for `nix-config`.
- **Working inside the subrepository.** To change a shared tool: `cd tools/agentic-research`,
  `git switch -c <branch> origin/main`, edit, run its own tests, push to codigos.ufsc.br and
  open a merge request there. After it merges, a PR here moves the pin with
  `git -C tools/agentic-research checkout <sha> && git add tools/agentic-research`, and runs
  ww3-gpu's gates against it. A session that should work mainly on the tools starts inside the
  submodule, where its own `CLAUDE.md`, skills and agents load as a project of their own.
- **Distributing to other projects.** The same GitLab project can also carry a Claude Code plugin
  marketplace (`/plugin marketplace add https://codigos.ufsc.br/agentic-research/<project>.git`),
  which bundles skills, agents and hooks for a project that does not want a submodule `(v)`
  code.claude.com/docs `plugins/cli-reference`, 2026-10-10. ww3-gpu does not use it, because a
  plugin is not present in CI, where the scripts must run.

## Steps and effort

The estimate is from the line counts above and from what each step touches; nothing here has
been timed, so every figure is a projection ⚠.

| # | Step | Where | Estimate ⚠ |
|---|---|---|---|
| 0 | Ask the co-advisor for the group, a maintainer, Public visibility and an account for the owner | e-mail | outside this repository's control; it gates steps 2 to 5 |
| 1 | Make each unit read its paths, prefixes and URLs from `agentic-research.toml`; ww3-gpu's tests pass unchanged with its own config | here, one PR | 2 days |
| 2 | Extract the units with their history into the new project; add a README, the licence, the config example, and a GitLab CI job that runs the unit tests | codigos.ufsc.br | 1 day, if the instance has shared runners ⚠ (not checked) |
| 3 | Add the submodule at `tools/agentic-research/` (https URL, as CI needs), delete the local copies, point the `just` recipes and the hooks at it, add the symlinks | here, one PR | 1 day |
| 4 | Test in a fresh session that the symlinked skills and agents load and the hooks fire; init the submodule in the CI jobs that run the moved gates and in `scripts/agent_env.sh` | here, same PR as 3 | 0.5 day |
| 5 | Update `AGENTS.md`, `CONTRIBUTING.md`, `CLAUDE.md` (a fourth submodule, and the one worked in rather than only read), the glossary and the lessons that name a moved file | here, same PR as 3 | 0.5 day |

About five working days of agent work plus the owner's review, across two repositories, with
step 0 as the only unbounded wait. Step 1 can start before step 0 is answered: it removes
hardcoded paths this repository should not have anyway.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep everything here | Works, and is the fallback if a precondition fails. Other projects at UFSC then copy files by hand and the copies drift. |
| Vendor it back pinned through `skills.lock` | Weighed in the table above: better archives and fewer fetches, but every fix to a shared tool goes through another clone and a vendoring PR, and `skills-vendor` needs GitLab support first. It becomes the choice if the host proves unreliable for CI (see *Revisit when*). |
| Consume it only as a Claude Code plugin | Bundles skills, agents and hooks well, but the plugin is absent in CI, where `figures.py`, `wfip.py` and the review gate must run. Offered to other projects instead. |
| Move it to a new GitHub repository | Simpler for CI, but gives LabECO/UFSC nothing they host or co-maintain, and hosting there is the reason for the donation. Revisit if UFSC declines. |
| Move it into marola-devkit | marola-devkit is the marola organisation's harness; the evidence rule, the reading card and the review gate are research tooling and belong with a research group. |
| Mirror from GitHub to GitLab, keeping development here | The UFSC project would be a read-only shadow and other projects could not contribute back. |
| Donate the proposal reviewer too | It checks Resolução 05/2012 of the Escola Politécnica and the DEL structure `(v)` [`.claude/agents/revisor-proposta.md`](../../.claude/agents/revisor-proposta.md); a UFSC thesis follows other norms. The gate that runs it moves; the reviewer stays. |

## Consequences

- A change to a moved tool is a merge request there and a pin move here; the pin move is where
  ww3-gpu's gates check it.
- Every fresh clone and every CI job that runs a moved gate fetches from codigos.ufsc.br. When the
  host is down, those jobs fail; the book build and the kernel tests do not depend on it.
- GitHub's source archive, and so the Zenodo deposit of a release, will not contain the tools ⚠;
  the deposit's README must name the submodule's URL and pinned commit.
- The book's chapters that describe these tools point at the UFSC project as their source, and
  ww3-gpu becomes its first worked example.
- The owner becomes a co-maintainer of a project on UFSC's infrastructure, under UFSC's terms of
  use for that service ⚠ (not read).

## Revisit when

- Step 0 is answered: a refusal or a non-Public project makes this Rejected; the GitHub
  alternative above is then the next candidate.
- A second project consumes the tooling, the first evidence that the extraction serves more than ww3-gpu.
- CI fails twice in a month because codigos.ufsc.br is unreachable from GitHub's runners; the
  vendoring alternative then replaces the submodule.
- A symlinked agent or skill stops loading after a Claude Code update.
- `agentic-research.toml` grows options that only ww3-gpu uses, a sign the unit was not generic.
