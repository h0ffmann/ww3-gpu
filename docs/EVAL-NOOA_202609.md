# Evaluating NVIDIA-labs Object Oriented Agents (NOOA) for ww3-gpu

**Status:** evaluation, 2026-09-16. Nothing adopted yet; this document argues for a bounded pilot.
**Question asked:** could [NVIDIA-NeMo/labs-OO-Agents](https://github.com/NVIDIA-NeMo/labs-OO-Agents) help the
development of this repository, or the port of WW3 kernels to Kokkos on GPUs?
**Verification:** facts marked `(v)` were read from the NOOA repository on 2026-09-16 (README, `AGENTS.md`,
`docs/architecture.md`, `docs/tour.md`, `examples/README.md`, `skills/README.md`, `pyproject.toml`,
`packages/*/README.md`, `CHANGELOG.md`, releases) or from this repository. `⚠` marks inference.
**Refreshed 2026-10-01:** repository activity, releases, licences and model presets re-read from a clone of
NOOA `main` at `7919847` (2026-09-29) and from PyPI; §8 (costs) is new. Facts updated by the refresh carry
the date 2026-10-01. Most nvidia.com hosts were unreachable from the sandbox, so several price claims in §8
rest on search excerpts and are marked `⚠` (list in Sources).
**Refreshed 2026-10-08:** merged `main` into this branch (the README now lists this document in its
study-areas table instead of the retired `docs/` row). Re-checked: `nooa` on PyPI is still `v0.0.10`
(2026-09-04) and the repository's tags still end at `v0.0.10`; `main` has moved on (HEAD `564a340`)
`(v, PyPI JSON and `git ls-remote`, 2026-10-08)`. The question this document leaves open, whether
NVIDIA's agents help the `W3SDS4` port and what the lab can ask NVIDIA for without paying, is answered
in section 10 of `docs/W3SDS4_TRITON_PLANO_202610.pt.md`
([PR #57](https://github.com/h0ffmann/ww3-gpu/pull/57)): the agents help the workflow and not the kernel, the Developer Program account is free today,
and the Academic Grant Program is faculty-only and closed since 2026-06-30.

## TL;DR

- NOOA is a **Python agent framework**, not a GPU or HPC tool. It cannot make a kernel faster, port
  Fortran to Kokkos by itself, or replace CMake, Kokkos, ctest or the parity gates. Its relevance is to
  the *process* this repository already runs by hand: a coding agent translates one routine, writes the
  fixture and tests, runs them, records the timing row, and a human reviews the evidence.
- What NOOA adds to that process is exactly what `docs/AGENTS_KOKKOS_202609.md` §1.5–1.6 asks for and
  currently enforces only by prose: **typed contracts** for each step, **Python-enforced evidence gates**
  (no PR row without a passing ctest and a parity table), **traces** of every LLM call and executed cell,
  and **composition** (one orchestrator per routine, one subagent per phase). Their paper calls these the
  six harness capabilities; the repository's own definition of "done" for a ported routine maps onto them
  one to one (§3).
- The costs are real: NOOA is alpha (`v0.0.10`, 2026-09-04, still the latest release on 2026-10-01 with
  336 commits on `main` since) `(v)`, Python ≥ 3.12 with `litellm` and
  `pydantic` `(v)`, and its default CodeAct strategy **executes LLM-generated Python**, so NVIDIA says to
  run it only inside OS-level isolation `(v)`. This repository's rule is "no Python in lab code", and its
  sandbox story already exists in `nix-config` (`labs/agentic`, `ai-jail`).
- Money (§8, checked 2026-10-01): NOOA, the NeMo Agent Toolkit and OpenShell are Apache-2.0 and free
  `(v)`. NVIDIA's hosted model catalogue (build.nvidia.com) costs nothing for development but is rate-limited
  (40 requests/minute in NVIDIA's own example) `(v)`; a paid NVIDIA AI Enterprise licence applies only to
  production use of NIM, which nothing here is `⚠`. NVIDIA's own blueprint self-hosts NOOA's default
  NVIDIA model on three H100-class GPUs `(v)`, which this lab does not have.
- **Recommendation:** run a two-week pilot in a separate lab (`nix-config/labs/agentic` or a new
  `labs/nooa`), never inside `kokkos/`: one `PortAgent` orchestrator that re-does the `W3SNL1` port
  end to end against the existing fixtures (a known answer), then attempts the next routine on the ranked
  list (`W3SIN4`/`W3SDS4`, ST4) with the same gates. Adopt only if the second port reaches L1 parity with
  less human steering than the first did, measured by the trace (§6). Otherwise keep the current
  Claude Code + `SKILL.md` workflow and take only NOOA's *skill format* and evidence-gate pattern.

## 1. What NOOA is `(v)`

| Aspect | Fact |
|---|---|
| Repository | `NVIDIA-NeMo/labs-OO-Agents`, created 2026-07-20, 2 125 stars / 290 forks on 2026-09-16, last push the same day. On 2026-10-01: about 2.3k stars / 303 forks, 682 commits on `main`, last commit 2026-09-29; 126 non-merge commits since 2026-09-16, 118 of them by one author |
| Licence | Apache-2.0 (`LICENSE`, `pyproject.toml`); GitHub's API reports `NOASSERTION` because of the header layout. Unchanged on 2026-10-01, PyPI metadata also says Apache-2.0 |
| Package | `nooa` on PyPI, plus `nooa-cli` (trace viewer, eval runner), `nooa-acp` (Agent Client Protocol server for Zed), `nooa-memory`, `nooa-bench` (SWE-bench / Terminal-Bench runner); `requires-python >=3.12,<3.14`; core deps `pydantic`, `litellm` (`>=1.97.0` on 2026-10-01, pinned past CVE-2026-49468 and the yanked 1.82.7/1.82.8 releases per the comment in `pyproject.toml`), `httpx`, `msgpack`, `openinference-instrumentation-litellm` |
| Maturity | "Development Status :: 3 - Alpha"; releases `v0.0.7` (2026-07-30) … `v0.0.10` (2026-09-04); README: "research software … expect rough edges". No release between 2026-09-04 and 2026-10-01; `main` is 336 commits past the `v0.0.10` tag, and its unreleased changelog removes public names (`CodeActLiteStrategy`, `RespondResult`, `RespondReason`) |
| Core idea | An agent is a Python class. Fields are state, methods are capabilities, docstrings are prompts, type annotations are contracts. An `async` method whose body is `...` is implemented at run time by an LLM strategy; a method with a real body is ordinary Python |
| Strategies | `PredictStrategy` (one structured attempt, validated against the return type, no tools) and `CodeActStrategy` (default: the model acts by writing Python cells in a per-call REPL with `self`, imports and visible methods; return type validated; validation errors fed back) |
| Harness | Event history per instance with summarisation; context blocks; tracing of every LLM call, code cell and nested method call as spans (JSONL, OTLP, Langfuse, Phoenix exporters; `nooa start-dev` viewer); middleware `intercept()` and observers `on()`; visibility rules (`@hidden`, `Annotated[T, hidden]`) |
| Models | Anything LiteLLM routes: Anthropic, OpenAI, NVIDIA NIM, **Ollama** and **vLLM** local endpoints (`get_llm_client("ollama_chat/qwen3:1.7b", api_base=…)`). On 2026-10-01 the bundled quickstart picks `nvidia_nim/nvidia/nemotron-3-super-120b-a12b` on build.nvidia.com when `NVIDIA_API_KEY` is set, else OpenAI; `nooa connect` offers build.nvidia.com and, since 2026-09-29, an "NVIDIA Inference Hub" (`inference-api.nvidia.com`), which `src/nooa/util/quickstart.py` describes as NVIDIA's internal gateway for NVIDIA employees |
| Tools | Methods on the object; `ShellTools`, `TodoManager`; MCP servers as tools (`--extra mcp`); `TextSkill`/`SkillRegistry` (`SKILL.md` bundles, the same format Claude Code reads) |
| Safety | AST checks and module deny-lists are "defense-in-depth guardrails, not a containment boundary"; the boundary is OS isolation (container, VM, NVIDIA OpenShell) |
| Coding-agent skills | `skills/` ships twelve `SKILL.md` bundles *about* NOOA for Claude Code / Cursor / Codex (fourteen on 2026-10-01) |
| Paper | arXiv:2607.20709, "NVIDIA-labs OO Agents: Native Python Object-Oriented Agents"; six model-facing capabilities: typed input/output, pass-by-reference over live objects, code as action, programmable loop engineering, explicit object state, model-callable harness APIs for context and events; evaluated on SWE-bench Verified, Terminal-Bench 2.0 and ARC-AGI-3 (numbers not in the abstract page fetched) |

What it is **not**: there is no GPU, CUDA, Kokkos, Fortran or HPC content anywhere in the repository
`(v)` (examples are quickstarts, SWE-bench/Terminal-Bench, CyberGym, ARC-AGI-3). "NVIDIA" here means
NVIDIA's NeMo research group and an easy path to NIM-hosted models, not GPU tooling.

## 2. The process this repository runs today

The `W3SNL1` port (`kokkos/`) was produced by Claude Code subagents driven from a written plan, with
review loops, in one day. The steps, from `docs/AGENTS_KOKKOS_202609.md` §1.5 and `kokkos/PORT_STATUS.md`:

1. Freeze the Fortran source range (`w3snl1md.F90:115-473`, `483-786`) and copy it verbatim into a
   reference (`kokkos/tests/fixtures/snl1_ref.F90`).
2. Generate fixtures from the reference (`gen_snl1_fixture.F90` → `snl1_nk25_nth24.bin`).
3. Write the failing L1 tests (`L1_test_snl1_tables.cpp`, `L1_test_snl1_dia.cpp`), then the C++.
4. Reach bit-identical parity on Serial, OpenMP and CUDA (`-ffp-contract=off`).
5. Write the `bind(C)` shim, the Fortran interface and the caller patch (`PATCH.md`).
6. Record the timing row in `PORT_STATUS.md`; document scratch limits and deferred work.
7. Human review (two passes), fix rounds, merge.

Every gate in that list was enforced by a *person* reading a report ("did the test really fail first?",
"is the fixture byte-identical?", "is the number in the table the one ctest printed?"). Two of the review
findings on that branch were exactly gate failures: a claimed `(v)` number that nobody had run (the ST4
switch-file count), and a timing note whose consequence the code did not produce (the 48 KB scratch
claim). Those are the failures NOOA's design targets: **the model supplies judgment inside a method; Python
enforces the workflow** (`docs/tour.md` §4, `(v)`).

## 3. Mapping NOOA onto the port workflow

The ranked list in `AGENTS_KOKKOS` §2.2 has nine more routines after `W3SNL1`. A NOOA orchestrator for
"port one routine" would look like this (sketch, not code that exists):

```python
class PortAgent(Agent, llm=llm):
    """Port one WW3 routine to a Kokkos kernel, phase 1 (translate, do not improve)."""

    ww3_src: Path                      # WW3/model/src, pinned revision
    kokkos_tree: Path                  # kokkos/
    routine: RoutineSpec               # module, name, line range, regtest, interface contract

    # --- judgment: LLM-implemented, typed, one task each -----------------------
    async def extract_reference(self) -> FortranReference: ...      # verbatim copy + allowed edits list
    async def write_fixture_generator(self, ref: FortranReference) -> FortranProgram: ...
    async def write_failing_tests(self, spec: KernelSpec) -> list[TestFile]: ...
    async def translate(self, ref: FortranReference, spec: KernelSpec) -> CppKernel: ...
    async def write_shim(self, k: CppKernel) -> Shim: ...

    # --- rules: deterministic Python, cannot be skipped ------------------------
    def diff_reference(self, ref) -> DiffReport:  ...   # normalised diff vs WW3 source; only allowed edits
    def run_ctest(self, preset) -> CtestResult:  ...    # nix develop …#ww3 --command ctest …
    def parity(self, fixture, result) -> ParityTable: ...  # max rel error per output, per point
    def timing_row(self) -> PortStatusRow: ...          # from ww_bench_* output, never typed by hand

    @hidden
    async def run(self) -> PortReport:                  # the orchestrator: sequence + evidence gates
        ref = await self.extract_reference()
        assert self.diff_reference(ref).only_allowed_edits
        gen = await self.write_fixture_generator(ref); fixture = self.build_and_run(gen)
        tests = await self.write_failing_tests(...); assert self.run_ctest("serial-debug").failed
        k = await self.translate(ref, spec); assert self.run_ctest("serial-debug").passed
        assert self.parity(fixture, ...).max_rel <= 1e-5
        assert self.run_ctest("openmp-release").passed
        return PortReport(row=self.timing_row(), trace=self.trace_id)
```

| `AGENTS_KOKKOS` §1.5 "done" item | NOOA construct that enforces it |
|---|---|
| Kernel + heritage header | `translate()` returns a `CppKernel` Pydantic model whose validator requires the SPDX and heritage lines |
| `bind(C)` shim + Fortran interface with documented argument table | `Shim` model with an `args: list[ArgRow]` field; the orchestrator refuses an empty table |
| L1 test with stated, justified tolerances | `TestFile.tolerance` and `.justification` are required fields; `run_ctest()` must fail before `translate()` and pass after |
| L2 regtest replay comparing WW3 vs WW3+Kokkos | deterministic `run_l2_replay()` calling `kokkos/tests/L2_replay.sh`; the `nccmp-tol` verdict is parsed, not trusted |
| Timing line in `PORT_STATUS.md` | `timing_row()` reads `ww_bench_*` stdout; the model never writes numbers |
| Property test where physics allows | a `PredictStrategy` method proposes the property; a real method runs it |
| "Never report a speed-up without the measurement attached" (§5) | impossible by construction: `PortReport.row` is produced by code |
| Prompt patterns of §1.6 (exact routine, regtest, interface contract, phase) | the `RoutineSpec` argument is rendered to the model by the framework; no hand-written prompt |
| Anti-patterns (optimise while translating, Unified Memory, editing WW3 physics) | `diff_reference()` and a `intercept()` middleware that rejects cells touching `WW3/` |

Everything in the right column is ordinary Python, testable without a model. That is the part of NOOA
that matters for this repository; the LLM-side ergonomics (docstring prompts, CodeAct REPL) are a
convenience on top.

## 4. What NOOA would and would not change for the GPU port

**Would change (development process):**

- Reproducible evidence. Every port run produces a trace (LLM calls, executed cells, ctest output, parity
  tables) that can be exported as JSONL/OTLP `(v)` and attached to the PR — the proposal promises exactly
  this ("os *prompts* e a evidência de paridade são versionados com o código", `pubs/proposal/pt/07-methodology.md`).
- Repeatability across routines. The `W3SNL1` port cost roughly a day of agent time with two review
  rounds; the same orchestrator replayed on `W3SIN4`, `W3SDS4`, `W3XYP2` turns the plan's ranked list
  into a queue. Delegation (`nooa-bench`'s awaited workers, depth ≤ 4 `(v)`) allows one subagent per phase.
- Local models. LiteLLM routes to Ollama and vLLM `(v)`, so the pratico lab's `qwen2.5-coder` (or a
  larger model on the 4090) can drive the deterministic-heavy steps offline; hosted models only where
  judgment is hard (translation). This matters for a university lab's budget (§8) and for data that must stay local.
- Editor integration. `nooa-acp` runs the same coding agent inside Zed `(v)`; the `skills/` bundles
  install into `~/.claude/skills` and are the format this repository could use for its own "port a
  kernel" skill regardless of NOOA adoption.

**Would not change (the port itself):**

- Kokkos, CMake presets, `-ffp-contract=off`, the fixture format, the parity thresholds, the shim ABI and
  the WW3 caller patch are unchanged. NOOA has nothing to say about `TeamPolicy` scratch, `LayoutLeft`,
  H2D traffic or the residency ladder (§3.4 of the plan) — those are engineering decisions a person makes.
- GPU execution is still validated on the owner's RTX 4090 in the `#cuda` shell and, later, on an H100;
  NOOA does not run on the GPU and does not need one.
- The bulk-porting economics stay as Koldunov et al. (2026) describe them for FESOM2: weeks of expert-
  steered translation, not an unattended run. NOOA changes who holds the checklist, not the checklist.

## 5. Fit with this repository's constraints

| Constraint (from `README.md`, `docs/superpowers/specs/…`) | Tension | Resolution |
|---|---|---|
| Lab code is C++, Fortran and shell; Python only in the publishing pipeline | NOOA is Python-only (3.12+) | Keep it out of `kokkos/`, `examples/`, `exercises/`, `bench/`. It is *tooling that drives the lab*, like `translate_md.py`: a separate `uv` project, best placed in `nix-config` as a lab (`labs/agentic` already hosts agent sandboxing) and consumed as a submodule shell, so `git ls-files '*.py'` in ww3-gpu stays at the four ruled files |
| Everything runs from the pinned Nix toolchain | `nooa` is on PyPI with a `uv.lock`; nixpkgs has no package `⚠` | `labs/publisher` provides Python packages with `python3.withPackages (ps: [ ps.openai ])` from nixpkgs `(v)`; that pattern only works if every dependency is packaged, and `nooa` is not in nixpkgs `⚠`. The pilot therefore needs either a small nixpkgs overlay (`buildPythonPackage` from the `v0.0.10` sdist, `litellm`/`pydantic` from nixpkgs) or `uv` inside the devShell with `uv.lock` pinning NOOA while `flake.lock` pins the interpreter; prefer the overlay if the dependency tree resolves, because it keeps one lock file |
| Agents must not escape the workspace | CodeAct executes model-written Python with `open()`, `importlib`, subprocess `(v)` | `ai-jail` (bubblewrap/Landlock/seccomp) from `labs/pratico`/`labs/agentic` is the OS boundary NVIDIA asks for; NVIDIA's own answer is OpenShell. Never run a `PortAgent` outside it, and never with push credentials |
| Phase-1 rule "translate, do not improve"; never edit WW3 Fortran physics | A REPL agent can edit anything visible | Middleware `intercept()` rejecting cells that write under `WW3/`; `diff_reference()` gate; the model sees `WW3/` read-only through a helper, not through `ShellTools` |
| Reviews by a second agent, human merge | NOOA has no review notion; it has evidence gates | Keep the Claude Code review loop for PRs; NOOA produces the PR's evidence, not its approval |
| Trademarks / IP | Apache-2.0 tooling generating LGPL-derived kernels | Same as today: generated kernels carry the WW3 heritage header and `LGPL-3.0-or-later`; the orchestrator's validator makes the header mandatory |

## 6. Adoption options

| Option | What | Effort `⚠` | Value | Risk |
|---|---|---|---|---|
| **A. Skill format only** | Write `skills/port-a-kernel/SKILL.md` for Claude Code using NOOA's skill layout; no NOOA runtime | 1 day | Captures §1.5–1.6 as a reusable checklist for the tool already in use | None beyond docs drift |
| **B. Bounded pilot (recommended)** | `nix-config/labs/nooa` (or extend `labs/agentic`): devShell with `python312` + `uv`, `nooa[cli]`, `ai-jail` wrapper, Ollama/NIM model config; one `PortAgent` with the gates of §3; run it on `W3SNL1` (known answer) then on `W3SIN4`+`W3SDS4` | 1–2 weeks | Answers the adoption question with a trace instead of an opinion; produces the second kernel either way | Alpha API churn (v0.0.x, breaking renames in `CHANGELOG` `(v)`); model quality on Fortran→C++ is the real bottleneck, not the harness |
| **C. Full adoption** | `PortAgent` becomes the way every routine on the ranked list is ported; traces attached to every port PR; `nooa-acp` in the editor | months | Turns lesson 13 ("bulk porting with agents") into a runnable pipeline | Lock-in to an alpha framework; Python toolchain to maintain next to Nix; sandbox operations |

Pilot acceptance criteria (decide B → C or B → A):

1. `PortAgent.run()` on `W3SNL1` reproduces L1 parity (`max_rel == 0.0` on serial-debug) from the
   committed fixtures **without** the human editing generated C++; the trace shows every gate firing.
2. On `W3SIN4`/`W3SDS4`: L1 parity ≤ 1e-5 relative reached within the pilot; number of human
   interventions (messages to the agent, manual edits) recorded from the trace and compared with the
   `W3SNL1` history (two implementer rounds, one fix round, ~30 review findings across tasks).
3. All runs inside `ai-jail`; no write outside the worktree; no network except the model endpoint.
4. Cost: tokens and wall-clock per routine, hosted vs local model, in the report, priced with the
   list prices of §8 as they stand on the day of the run.

## 7. Risks and open questions

- **Alpha software.** Version numbers reached 0.0.10 in seven weeks (corrected 2026-10-01: the
  releases page and PyPI list four releases, `v0.0.7`–`v0.0.10`, and the repository has five version tags,
  `v0.0.6`–`v0.0.10`); the changelog records renamed APIs and moved subpackages `(v)`. Pin a tag
  (`uv add "nooa @ git+…@v0.0.10"`) and expect to re-pin. Update 2026-10-01: releases stopped at
  `v0.0.10` while `main` moved 336 commits, including removals of public names (§1) `(v)`. Pinning
  `v0.0.10` now means pinning a month-old snapshot; pinning a `main` commit means pinning unreleased API.
  Either way, read the changelog before re-pinning.
- **Security posture.** NVIDIA's README is explicit that the framework's own checks are not a boundary
  `(v)`. The pilot must run in `ai-jail`; a leaked `NVIDIA_API_KEY`/`ANTHROPIC_API_KEY` inside a REPL the
  model controls is the obvious failure (NOOA hides `Annotated[str, hidden]` fields from the prompt, not
  from the process `⚠`).
- **Model, not harness, limits translation quality.** FESOM2's port (Koldunov et al. 2026) succeeded with
  a frontier model and domain experts steering; a 7B local model will not translate `W3SRCE`. The pilot
  should separate "gates caught a wrong translation" (harness value) from "the model could not translate"
  (model value).
- **Python in the toolchain.** Even in a separate lab, NOOA brings a Python dependency tree that nixpkgs
  does not package; either an overlay to maintain or a second package manager (`uv`) next to Nix. The
  publisher lab avoided this by using only nixpkgs packages `(v)`.
- **Does the pilot pay for itself?** With one kernel ported by hand in a day, the framework only wins if
  the ranked list (nine routines, several with `#ifdef` mazes) is actually going to be ported. If the
  proposal stops at `W3SNL1` plus `PATCH.md`, option A is enough.

## 8. Costs (checked 2026-10-01)

Question: does using NVIDIA's agent tooling for this repository's workflow cost money? Short answer:
the frameworks are free, NVIDIA's hosted models are free at development scale with a rate limit, and the
only priced items are a production licence this repository would not need and datacentre GPUs it does not
have. Every figure below was checked on 2026-10-01 against the source in its row; `⚠` means it comes from
a search-engine excerpt of a page the sandbox could not open.

### 8.1 Software

| Item | Cost | Source, checked 2026-10-01 |
|---|---|---|
| NOOA (`nooa`, `nooa-cli`, `nooa-acp`, `nooa-memory`, `nooa-bench`) | Free. Apache-2.0; no paid tier and no hosted service from the project; you bring the model `(v)` | [`LICENSE`](https://github.com/NVIDIA-NeMo/labs-OO-Agents/blob/main/LICENSE), `pyproject.toml`, `README.md` § License, [PyPI](https://pypi.org/project/nooa/) |
| NVIDIA NeMo Agent Toolkit (`nvidia-nat`) | Free. Apache-2.0; `1.9.0` on PyPI 2026-09-10, `requires-python >=3.11,<3.14` `(v)`. It adds observability, profiling and optimisation to agents built in other frameworks (LangChain, LlamaIndex, CrewAI and others) and ships its own ReAct agent `(v)`. Its hello-world needs an `NVIDIA_API_KEY` from build.nvidia.com for the default model `(v)` | [`LICENSE.md`](https://github.com/NVIDIA/NeMo-Agent-Toolkit/blob/main/LICENSE.md), [`README.md`](https://github.com/NVIDIA/NeMo-Agent-Toolkit/blob/main/README.md), [PyPI](https://pypi.org/project/nvidia-nat/) |
| NVIDIA OpenShell (the sandbox NOOA's README recommends) | Free. Apache-2.0 `(v)` | [`LICENSE`](https://github.com/NVIDIA/OpenShell/blob/main/LICENSE) |

### 8.2 Models these agents call

| Route | Cost and limits | Source |
|---|---|---|
| build.nvidia.com hosted endpoints (`integrate.api.nvidia.com`, `NVIDIA_API_KEY`; NOOA's and the toolkit's default) | No charge for development. NVIDIA's own example notebook says "Don't worry about any credits to use this model, although there is a rate limit of 40 requests per minute" `(v)`, text added 2025-06-05. Older NVIDIA READMEs still describe "up to 5,000 free credits" per account `(v)`; search excerpts of NVIDIA's forum say credits were replaced by rate limits that vary by model and are not published, and users ask for 40 → 200 RPM increases `⚠`. Access needs an NVIDIA Developer Program account, free to join `⚠` | [GenerativeAIExamples notebook](https://github.com/NVIDIA/GenerativeAIExamples/blob/main/nemotron/VLM/llama_3.1_nemotron_nano_VL_8B/Llama_Nemotron_VL_nano_8B.ipynb) (v); [metropolis-nim-workflows README](https://github.com/NVIDIA/metropolis-nim-workflows/blob/main/README.md) (v); [forum: API credits](https://forums.developer.nvidia.com/t/api-credits-for-build-nvidia-com/306633) ⚠ |
| Terms of the hosted tier | Developer Program access is "for prototyping, research, development and testing purposes only"; production means serving real end users or business transactions `⚠` | [NVIDIA NIM FAQ (forum)](https://forums.developer.nvidia.com/t/nvidia-nim-faq/300317) ⚠ |
| NVIDIA Inference Hub (`inference-api.nvidia.com`) | Not open to us: NOOA's own source calls it the "NVIDIA internal inference gateway … NVIDIA employees" `(v)` | NOOA `src/nooa/util/quickstart.py` at `7919847` |
| Self-hosted NIM, development | Free for Developer Program members on up to two nodes or 16 GPUs, for research, development and testing `⚠`. NVIDIA's blueprint READMEs state "NVIDIA AI Enterprise developer licence required to local host NVIDIA NIM Microservices" `(v)`; the RAG blueprint, which runs NIM containers, is governed by the NVIDIA Software License Agreement and the Product-Specific Terms for AI Products `(v)` | [NVIDIA blog, NIM free to Developer Program members](https://developer.nvidia.com/blog/access-to-nvidia-nim-now-available-free-to-developer-program-members) ⚠; [ambient-patient README](https://github.com/NVIDIA-AI-Blueprints/ambient-patient/blob/main/README.md) (v); [RAG blueprint README § License](https://github.com/NVIDIA-AI-Blueprints/rag/blob/main/README.md) (v) |
| Self-hosted NIM, production | NVIDIA AI Enterprise list price: $4,500 per GPU for a one-year self-managed subscription ($9,000 two years, $13,500 three, $18,000 four or five; $22,500 perpetual); education and research institutions $1,125 per GPU for one year; about $1 per GPU-hour on AWS/Azure/Google Cloud marketplaces plus the instance; a free 90-day evaluation licence `⚠` | [AI Enterprise pricing](https://docs.nvidia.com/ai-enterprise/planning-resource/licensing-guide/latest/pricing.html) ⚠; [Get started with AI Enterprise](https://www.nvidia.com/en-gb/data-center/products/ai-enterprise/get-started/) ⚠ |
| Hardware for self-hosting NOOA's default NVIDIA model | NVIDIA's RAG blueprint, which uses `nemotron-3-super-120b-a12b` as its LLM, lists 3 × H100, 3 × B200 or 3 × RTX PRO 6000 for a self-hosted local NIM deployment (9 of each for its Helm deployment) `(v)`. No smaller figure for the model alone was found on a page the sandbox could open. The lab's RTX 4090 is not on that list. None of NOOA's own dependencies (`pydantic`, `litellm`, `httpx`, `msgpack`, `openinference-instrumentation-litellm`) is a GPU library, so the framework needs no GPU `(v)` | [RAG blueprint, Nemotron 3 Super deployment](https://github.com/NVIDIA-AI-Blueprints/rag/blob/main/docs/nemotron3-super-deployment.md) (v) |

### 8.3 Compared with the Claude-based workflow used today

| Item | Cost | Source |
|---|---|---|
| Claude Code subscription | Included in all paid Claude plans. Pro $20 per month billed monthly, or $17 per month billed annually ($200 up front); Max from $100 per month for 5× or 20× Pro's usage `(v)` | [claude.com/pricing](https://claude.com/pricing), 2026-10-01 |
| Claude API (what NOOA would call with `ANTHROPIC_API_KEY`) | Per token. Across the four models on the price list: input $1–$10, output $5–$50, cache reads $0.10–$0.25 per million tokens; batch processing halves the price `(v)`. An API key is billed per token, apart from the usage a Pro/Max plan includes `⚠` | [claude.com/pricing](https://claude.com/pricing), 2026-10-01 |
| What this repository has measured | One figure: ~$1.40 for a 2.4M-token documentation pass, 97 % cache reads (commit `4169b6c`, 2026-09-24) `(v)`. The 2026-09-22 commits write "$n/a (no … list price available)"; a list price is published on 2026-10-01, so those can now be priced. The `W3SNL1` port itself has no recorded dollar cost | `git log` of this repository |

### 8.4 What this means for the pilot

- Option B (§6) costs nothing in licences: NOOA, the toolkit and OpenShell are Apache-2.0 and a
  build.nvidia.com key is free. Porting kernels in a university lab is development and research, the use
  the free terms describe `⚠` (our reading of the terms; nothing in the pilot serves end users).
- The binding limit on the free hosted tier is rate, not money. One sequential `PortAgent` should stay
  under 40 requests per minute; parallel delegated subagents or an eval sweep over several routines will
  not `⚠` (inference, no call counts measured). The pilot report should record calls per routine.
- The free catalogue serves open-weight models `⚠`. If the translation step needs the frontier models used
  for `W3SNL1`, NOOA reaches them through their own APIs at the per-token prices above, and NOOA adds no
  charge of its own. The cost question for adoption therefore stays where §7 put it: model quality.
- Self-hosting NVIDIA's default agent model is out of reach (three H100-class GPUs in NVIDIA's own
  blueprint). The local route that fits the 4090 is Ollama or vLLM with a smaller model (§4), at no
  licence cost, with the quality caveat in §7.
- An NVIDIA AI Enterprise licence becomes relevant only if a NIM container serves something in production.
  Nothing in options A–C does.

## Sources

- NOOA repository, read 2026-09-16: `README.md`, `AGENTS.md`, `docs/architecture.md`, `docs/tour.md`,
  `examples/README.md`, `skills/README.md`, `pyproject.toml`, `packages/nooa-acp/README.md`,
  `packages/nooa-bench/README.md`, `CHANGELOG.md`, releases page.
- arXiv:2607.20709, *NVIDIA-labs OO Agents: Native Python Object-Oriented Agents* (abstract page).
- This repository: `docs/AGENTS_KOKKOS_202609.md`, `kokkos/PORT_STATUS.md`, `kokkos/src/fortran_iface/PATCH.md`,
  `course/13-bulk-porting-with-agents.md`, `pubs/proposal/pt/07-methodology.md`,
  `docs/superpowers/specs/2026-09-15-cpp-kokkos-course-design.md`.
- Koldunov et al. (2026), arXiv:2606.11356 (FESOM2 Fortran → C → C++/Kokkos with an LLM assistant).

Added for the 2026-10-01 refresh:

- NOOA, blobless clone of `main` at `7919847` (2026-09-29): `git log` since 2026-09-16 and since `v0.0.10`,
  `git ls-remote --tags`, `CHANGELOG.md`, `pyproject.toml`, `LICENSE`, `README.md`, `skills/`,
  `src/nooa/util/quickstart.py`, `src/nooa/unifiedllm/connect/__init__.py`; PyPI JSON for `nooa`;
  the github.com repository and releases pages (star and fork counts).
- NeMo Agent Toolkit: `git ls-remote --tags` (latest `v1.9.0`), `LICENSE.md` and `README.md` from
  raw.githubusercontent.com, PyPI JSON for `nvidia-nat`; github.com page for star counts (2.7k stars,
  773 forks).
- NVIDIA OpenShell `LICENSE`; NVIDIA GenerativeAIExamples (notebook text and its `git log -S` date);
  NVIDIA metropolis-nim-workflows `README.md`; NVIDIA-AI-Blueprints `ambient-patient` and `rag` READMEs and
  `rag/docs/nemotron3-super-deployment.md`.
- claude.com/pricing (plan and API prices); this repository's `git log` `Cost:` trailers.
- Not reachable from the sandbox (egress proxy refused): build.nvidia.com, docs.api.nvidia.com,
  developer.nvidia.com, docs.nvidia.com, www.nvidia.com, forums.developer.nvidia.com, huggingface.co,
  arxiv.org, web.archive.org, pi3g.com, and the GitHub REST API. Every claim in §8 that depends on one of
  those is marked `⚠` and rests on a search-engine excerpt of the URL cited.
