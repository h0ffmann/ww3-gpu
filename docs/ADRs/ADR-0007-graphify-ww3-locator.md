# ADR-0007: Graphify is adopted as an offline locator of WW3 module dependencies, built on preprocessed sources at the pin; it is never cited and never answers who calls what

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-11 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | How agents and contributors find their way around the WW3 Fortran tree (`WW3/model/src`, or `$WW3`) when porting. Not this repository's own files, not WW4, not the Markdown, not the management items of [#68](https://github.com/h0ffmann/ww3-gpu/issues/68) §4, which stay in that issue |
| **Related** | [#68](https://github.com/h0ffmann/ww3-gpu/issues/68) (the question, its pros and cons and the evaluation plan this record starts); [log entry of 2026-10-11](../log/2026-10-11-graphify-on-ww3.md) (the spike behind the numbers below); [ADR-0002](ADR-0002-port-order-wall-time.md) (port order, which a dependency graph serves but does not change); [`AGENTS.md`, "WW3 source is read-only"](../../AGENTS.md#ww3-source-is-read-only-hard-rule) (the citation rule); [marola MIP-0076](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0076-agent-routing-tooling.md) (the sister project's adoption of the same tool, whose safety envelope this copies) |

## Context

Every porting session re-reads WW3 to answer the same questions: which modules a routine `USE`s,
which arrays it reads and writes, where its callers are. The tree is 146 entries and 9.8 MB in
`model/src` at the pin `(v)` `ls model/src | wc -l; du -sh model/src` at `WW3@03adda91`,
2026-10-11. Issue #68 asked whether Graphify, a tool that parses a codebase with tree-sitter into a
graph an agent can query, should take that work over, and proposed a one-day evaluation first.
The owner asked on 2026-10-11 for the adoption to be written down as a decision with every step it
needs.

The request named "Grafiphy". No tool by that name exists `(v)` web search, 2026-10-08 (#68); this
record reads it as **Graphify** (`Graphify-Labs/graphify`, PyPI `graphifyy`, Apache-2.0), the
reading #68 made and nobody corrected.

A spike on 2026-10-11 ran the code-only, keyless pass on WW3 at the pin, once on the raw tree and
once on the tree preprocessed with the lab switch `switches/switch_lab_shrd`. The commands are in
the [log entry](../log/2026-10-11-graphify-on-ww3.md); graphify 0.9.85, Python 3, an Intel Xeon at
2.80 GHz with 4 cores, one run each, so the times are an order of magnitude and not a benchmark.

| | Raw `.F90` | Preprocessed for `switch_lab_shrd` |
|---|---|---|
| Graph `(v)` | 2,427 nodes, 4,389 edges, 2.0 MB `graph.json` | 2,223 nodes, 4,057 edges |
| Build wall time ⚠ one run | 4.9 s | 2.9 s, plus the `gfortran -E` pass |
| Modules `W3SRCE`'s file `USE`s, per the graph `(v)` | `constants`, `w3dispmd`, `w3gdatmd`, `w3idatmd` | the four, plus `w3sbt1md`, `w3sdb1md`, `w3sln1md`, `w3snl1md`, `w3src4md` |
| `calls` edges, of which across files `(v)` | 752, 0 | 656, 0 |
| `graphify query "who calls W3SDS4"` `(v)` | the definition only, `w3src4md.F90` L1831; no caller | the same |
| `graphify path W3SRCE W3SDS4` `(v)` | no path | no path |
| `grep -in "call *w3sds4" *.F90` `(v)` | 7 call sites in 4 files, among them `w3srcemd.F90:1303` | |

Three things follow. The `USE` graph is right only after preprocessing: `w3srcemd.F90` has 239
`#ifdef` lines `(v)` `grep -c '^#ifdef' w3srcemd.F90`, and on the raw tree every `USE` behind a
switch is lost, so the graph silently drops `W3SRCE`'s dependence on `W3SRC4MD` and `W3SNL1MD`, the
two modules this repository ports. On the preprocessed tree the list is the one the lab build
compiles. Second, graphify's Fortran extractor resolves calls only inside one file; it does not
answer #68's questions (a) "every caller of `W3SDS4`" or (b) "the path from `W3WAVE` to `W3SNL1`",
and `grep` answers (a) in one command. Third, nothing in either pass left the machine: the AST pass
makes no model call `(v)` run under `env -i` with no API key and `OLLAMA_BASE_URL` pointed at a
closed port.

The sister project took the same tool on for symbol lookup across its repositories and measured
the same shape of result: 3 of 8 real questions answered, every miss about wiring the tool does not
see, and plain `git grep` cheaper whenever the keyword is known `(v)` marola MIP-0076 §2 and its
2026-10-10 revision, read 2026-10-11.

## Decision

**Graphify is adopted for one job: listing the module dependencies (`USE`) and the symbols a WW3
file defines, built offline from the sources preprocessed for a named switch at the pinned commit.
It is a way to find which files to open, never a source: a WW3 fact is still cited as
`model/src/<file>.F90:<line>` read from the file, and "who calls X" is still answered with `grep`.**
Its installers, hooks, strict mode, MCP server and model-backed passes are not used.

The adoption is done in these steps, in this order. Each is a PR with its own gates; the record
moves to Accepted when step 1 has a result the owner accepts, and the steps after it are not
started before that.

1. **Finish #68's evaluation on the preprocessed graph.** The three questions of #68 §3, each
   answered with the graph and without it, compared with `file:line` read by hand, tokens and wall
   time as a median of three runs with the harness named. Question (b) is expected to fail and is
   reported as such; (a) is asked of the graph and of `grep`. Results go in a new dated log entry and
   in a *Results* section of this record. If the graph does not beat reading files on (c), the
   module-variable question, this record is Rejected with the numbers and no later step runs.
2. **`scripts/graph.sh` and a `just graph` recipe**, shell only, `set -euo pipefail`, shellcheck
   clean, with `--self-test` and `--dry-run`:
   - `graph build [switch]` preprocesses `$WW3/model/src` (default the `WW3` submodule) with
     `gfortran -E -cpp -P` and `-DW3_<token>` for every token of the switch file (default
     `switches/switch_lab_shrd`), then runs `graphify extract . --code-only --no-label` on the result.
     Both the preprocessed tree and the graph go to
     `${XDG_CACHE_HOME:-$HOME/.cache}/ww3-gpu-graph/<ww3-sha>-<switch>/`, never into the checkout. It
     writes `build.json` there with the WW3 SHA, the switch file and its hash, and the graphify
     version. An uninitialised submodule or a dirty WW3 tree is an error.
   - Every graphify call runs under `env -i` with only `PATH`, `HOME` (the cache), `GRAPHIFY_OUT`,
     `GRAPHIFY_NO_AUTO_REFRESH=1` and `OLLAMA_BASE_URL=http://127.0.0.1:9`, and under `unshare -rn`
     where user namespaces allow it, as marola MIP-0076 §5.2 does. The script never passes
     `--backend` and refuses `extract` without `--code-only`.
   - `graph query|explain` pass the cached `graph.json`, default `--budget 400`, and print a
     staleness line when the WW3 SHA or the switch differs from `build.json`. `graph path` is not
     wrapped, since it cannot find a call path across files.
3. **Pin the tool.** graphify is run as `uvx --from graphifyy==<version>`, the mechanism `just
   specify` already uses, with the version in one variable in `scripts/graph.sh`. When
   `picasso/labs/pratico` carries the nixpkgs `graphify` package, the pin moves there in a
   `picasso` PR and the `uvx` line goes ⚠ until then: `uvx` pins the version, not the wheel's hash.
   An update is a PR that re-runs step 1's question (c) and records the graph size.
4. **Keep its output out of git and out of context.** `graphify-out/` goes in `.gitignore`, and
   `.claude/settings.json` denies `Read(**/graphify-out/**)` and `Read(**/GRAPH_REPORT.md)`, so an
   accidental in-tree run neither enters a commit nor fills a session. `graphify install`,
   `graphify hook install` and `--strict` are named in `CONTRIBUTING.md` as not to be run here.
5. **State the rule where agents read it.** One sentence in `AGENTS.md`, "WW3 source is read-only",
   and its longer form in `CONTRIBUTING.md`: `just graph` finds files, a claim cites the file at the
   pin, a graph result is never quoted as evidence and is `⚠` until the file is opened. The same
   sentence goes in `docs/AGENTS_KOKKOS_202609.md` §1, where porting agents look, and a row in the
   gates table of `AGENTS.md` (`bash -n`, `shellcheck`, `scripts/graph.sh --self-test`).
6. **Glossary.** Entries for Graphify and tree-sitter in `docs/GLOSSARY.md` with the first use, and
   ADR-0007 in "This repository's own names", with their remissive index lines.
7. **No skill until it earns one.** If sessions keep reaching for the graph, a skill that says when
   to use `just graph` and when `grep` is cheaper is written here or ported from marola-devkit's
   `routing` skill with `sharingan`, pinned in `skills.lock` with its audience `.patch`. Graphify's
   own `/graphify` skill is not installed, because its installer writes outside the lock.
8. **No CI job.** CI does not check out WW3 and the graph is never committed, so there is nothing to
   keep current; `scripts/graph.sh --self-test` runs where the other script self-tests run.

## Alternatives considered

| Option | Why not |
|---|---|
| Adopt it as #68 first sketched, on the raw tree, as a call graph for the port planner (#42) | Measured above: on the raw tree the `USE` edges behind a switch are missing, and on either tree there is no call across files. A planner fed from it would miss exactly the routines being ported. |
| Index this repository too | 337 tracked files, already linked by the WFIP index, the figure gallery and the glossary (#68 §2, Pros 1); `grep` and the indexes answer faster. |
| Run graphify's semantic pass on `docs/`, `pubs/` or WW3's manual | It sends the text to a model provider and costs tokens (#68 §2, Cons 5), and its inferred edges are a second, unreviewed reading of documents that already carry `(v)`. |
| Install the `/graphify` skill and the hooks, as upstream recommends | The installer writes assistant configuration outside `skills.lock`; strict mode blocks the first file read, which pushes an agent to trust the graph over the file, the opposite of `(v)` (#68 §2, Cons 3 and 4). |
| Reuse marola-devkit's `graph` tool directly | It indexes the checkout it runs in and knows nothing of switches or preprocessing; this repository is not a devkit flake consumer, only of its reusable workflows. The envelope is copied, the tool is not. |
| Commit `graph.json` | 2 MB per switch and pin, stale the moment the pin or the switch moves; the pin has already drifted from the text once (#68 §2, Cons 1). |
| Reject it now | The preprocessed `USE` graph is correct for the lab switch and builds in seconds, and question (c) of #68 is unanswered; one day of evaluation is cheaper than a guess either way. |

## Consequences

- A porting session can ask "what does `W3SRCE` depend on under `switch_lab_shrd`" and get the
  compiled answer in seconds, then open only those files. The answer for a different switch needs a
  different build, by construction.
- Callers, call paths and the arrays a routine writes are still found by reading and `grep`. The
  graph does not make the port planner (#42) automatic.
- The repository gains one shell script, one recipe and a third-party Python tool fetched at run
  time, pinned by version. Nothing in the lab code, the kernels or CI depends on it, and deleting
  the script undoes the adoption.
- A graph result never appears in a document as evidence; reviewers treat a citation that names
  the graph instead of a `file:line` as a defect.
- graphify releases about daily (marola MIP-0076 §4); the version pin and step 3's re-run keep an
  update from changing answers silently.

## Revisit when

- graphify's Fortran extractor resolves calls across modules (a `calls` edge whose ends are in
  different files on the preprocessed tree): then the call-graph use and the port planner join are
  reconsidered in a new record.
- Step 1's evaluation finishes: this record gains *Results* and moves to Accepted or Rejected.
- WW3 is built by CMake with a `compile_commands.json` that names the preprocessed sources, which
  would replace the `gfortran -E` pass in step 2.
- The WW3 pin moves to a branch whose switches change which modules `W3SRCE` uses.
- WW4 (C++/Kokkos) becomes a porting reference here; its tree is a different question.

## Revision, 2026-10-14: this repository's own code

The owner widened the scope on 2026-10-14 to this repository's code as well, for two Claude Code
commands: `/graph-check` (is the CLI installed, and how far behind) and `/graph-view` (prepared
queries over `kokkos/`, `scripts/`, `tests/` and the rest). The alternative "Index this repository
too" above is set aside for code only; the Markdown stays out, as does the semantic pass. The rest
of the envelope holds: `--code-only --no-label`, `env -i` with `GRAPHIFY_NO_AUTO_REFRESH=1` and a
closed `OLLAMA_BASE_URL`, output under `~/.cache/ww3-gpu-graph/self-<sha>/`, `graphify-out/` in
`.gitignore`, and a graph result is a lead to a file, never a citation. The measurement, including
what the graph misses, is the [log entry of 2026-10-14](../log/2026-10-14-graphify-on-ww3-gpu.md).

graphify's `/graphify` skill exists on the owner's workstation from a user-level `graphify install`
outside this repository (v) `~/.claude/skills/graphify/SKILL.md`, 0.9.91, 14 October 2026; step 7
still holds for the repository: nothing here installs it, and `graphify-out/` stays out of git.
