# 14 — WW4: what's coming, and how mature it is

**Short version: WW4 is real, it is a full ground-up rewrite in C++ (with Rust in
parallel) rather than a new WW3 version, and as of September 2026 it is a driver skeleton
with a test framework and no physics. Learn WW3. Watch WW4. Build your artefacts in the
shape WW4's tests already have.**

## It exists

- Repository: **[NOAA-EMC/WW4](https://github.com/NOAA-EMC/WW4)** `(v)` —
  "Home of the WAVEWATCH IV ™ (WW4 ™) third-generation wind wave modeling framework",
  created 2025-11-17 `(v)`.
- Planning documents: **NCEP Office Note 525**, *The WAVEWATCH III® Software Modernization
  Project: Phase I report*, Hendrik L. Tolman, November 2025,
  [doi:10.25923/h7j3-1h25](https://doi.org/10.25923/h7j3-1h25) `(v)`; and **NCEP Office
  Note 528**, *The WAVEWATCH IV Project: Phase II report*, Tolman, 2026,
  [doi:10.25923/0wyp-9f39](https://doi.org/10.25923/0wyp-9f39) `(v)`.

Office Note 525 is the single best thing to read if you want to understand where wave
modelling is going. It is unusually candid — it publishes the disagreements inside the
discussion group rather than papering over them. ON 528 closes Phase II.

## Maturity: a snapshot

Checked against the repository on **2026-09-15** `(v)`:

| Signal | Value |
|---|---|
| Default branch | `develop` |
| Commits | 37 |
| Releases / tags | **none**; `VERSION` reads `0.0.0` |
| Stars / forks / watchers | 3 / 7 / 4 |
| Open issues / open PRs | 31 / 0 |
| Last push | 2026-09-15 — the merge of PR #66, "CMake-only compile system", open since 2026-08-21 |
| Top-level | `src/ tests/ tools/ templates/ externals/`, plus `AGENTS.md`, `ARCHITECTURE.md`, `INTENT.md` |
| `src/` | `ww4_core/` (`w4core_init`, `w4core_wave`, `w4core_finalize`), `ww4_utils/`, `ww4_progs/ww4_standalone.cpp` |
| `tests/` | GoogleTest; four declared levels, L1 and L2 present, L3 and L4 absent |

Thirty-seven commits, ten months in, and no release. This is scaffolding and early core
work — an init/wave/finalize driver with a standalone program around it — not a model you
can run. Treat it as a project to follow, not a tool to adopt.

What has happened this year, in order `(v)`: PR #50 (2026-07-22) brought the unit (L1) and
integration (L2) tests to full coverage of the code that exists; issue #43 (2026-07-15,
still open, no comments) proposes the **CPU–GPU architecture**, naming Kokkos as the data
abstraction and MPI between nodes, and asks whether to keep one code base for CPU and GPU;
PR #60 (2026-08-19) removed the Python dependence from the build; PR #66 made the build
CMake-only. Around thirty open issues are design questions of that kind, not bugs.

⚠ Those numbers were true on one day — literally: the snapshot this lesson was drafted
from, earlier the same day, still showed 36 commits, 35 open issues and PR #66 open.
Check them yourself before quoting them; that is exactly the kind of thing that goes
stale fastest.

## Why a new model rather than WW3 v8

The Phase I report lays out the drivers plainly:

- **Parallel concepts are 24 years old.** WW3 uses a "shuffle" method for distributed
  computing (Tolman 2002a) that load-balances dynamic source-term timestepping beautifully
  but doesn't scale to exascale. WW4 moves to conventional domain decomposition.
- **Data structures are rooted in the Fortran 90 transition** of nearly two decades ago.
  WW4 wants spatial data structures local to each domain, for scalability and a smaller
  memory footprint — and for source-term integration over *areas* rather than individual
  grid points.
- **Optimisation now means memory, not FLOPs.** The report says this directly: the rise of
  GPUs and other advanced architectures requires focusing on memory use and access,
  complementing the traditional focus on floating-point operations. (Lessons 11–13 are
  the hands-on version of the same conclusion: where the spectrum *lives* decides the
  speed-up.)
- **Fractional stepping fights implicit schemes.** WW3's architecture leans on fractional
  steps (Yanenko 1971), which works beautifully for explicit propagation and badly for the
  implicit unstructured-grid solvers everyone now wants.
- **Closer coupling.** UFS wants component models callable at the level of functional
  units, not wrapped as monoliths. That needs an object-oriented interior.
- **The Fortran compiler pool is shrinking.** Stated as a continuity-of-operations risk:
  NWS wants to move off Fortran *in a controlled way, before it becomes urgent*.
- **Regression testing has become unsustainable.** Recompiling between individual tests is
  called out as untenable. WW4 moves to unit and integration testing.

The decision was a **complete bottom-up rewrite in a new repository**, explicitly modelled
on how NOAA handled MOM6 (developed separately from MOM4 rather than as an increment).
A new repo removes the obligation of backward compatibility and allows "house cleaning" of
options that no longer have an owner.

## Languages — the contentious part

The report says outright that language choice was the most contentious question, and that
**there is no community consensus**. NOAA/NWS made the call as primary funder:

| Language | Role in WW4 |
|---|---|
| **C++** (likely with Kokkos) | Initial **core** language. Conservative, proven, operational centres already have the workforce. Can be end-to-end. |
| **Rust** | Named "the modern language of choice for WW4" by NOAA/NWS, for memory safety, fearless concurrency, and lower porting/O&M cost. Being built in parallel with C++. |
| **Fortran** | **Solver language only**, plus a fast route to an initial operational capability. Also where unowned legacy options stay. |
| **Python** | Scripting, workflow, data management, product generation, grid generation, graphics. **Explicitly not** a core or solver language — the report cites C++/Kokkos outperforming Python/GT4Py on GPUs. |
| **Julia** | Considered and **declined** as a core language by NOAA/NWS: small user community, partial memory-safety benefit, workforce risk. Still permitted for non-operational solvers. |

The chosen path is the **"dual approach"**: build a C++ core to an operations-ready state
in roughly two years, while incrementally building Rust alternatives; a Rust-cored WW4 for
operations is a roughly five-year target. What the repository shows in September 2026 is
consistent with that: the core is C++, and the open architecture issue proposes Kokkos —
the "likely" in the table above has become a written proposal, not yet a decision `(v)`.

A design principle worth noting, because it's the thing that would let you contribute:
WW4 is deliberately keeping a **clean separation between core and solver code**, with
language bindings between them — so a contributor writing a new source-term
parameterisation can do it in whichever language they're fluent in without learning the
core language.

## Yes, it is being written partly by AI agents

The WW4 repository contains an `AGENTS.md` describing "an agentic AI approach used to
create, translate or refactor code using AI agents such as Copilot or Jules, the latter of
which has been used extensively in developing the WW4 code from WW3." `(v)` The agent is
also set up to enforce coding standards, generate doxygen documentation, and write unit
tests, all of which are mandated for WW4.

Worth knowing, for two reasons. It's a large, visible, government-operational test of
AI-assisted translation of scientific Fortran. And it means the provenance of any given
line in WW4 is a live question you should keep in mind when reading it. Lesson 13 is this
repo's own, much smaller, version of the same experiment, with the parity evidence
versioned next to the code.

## Timeline

From Office Note 525 `(v)`, with the report's own caveat that timelines for a project like
this are notoriously difficult, and a fourth column for what the repository actually shows:

| Phase | What | When (ON 525) | What happened `(v)` |
|---|---|---|---|
| I | Initial choices | complete (ON 525, Nov 2025) | Done. |
| II | Language test, governance setup, architecture design | began 1 Oct 2025, 3–6 months | **Done** — reported in NCEP Office Note 528 (2026). |
| III | Core code development (EMC) | open source, not yet open contribution | **In progress.** `src/ww4_core` init/wave/finalize, `ww4_standalone`, the utils library, the L1/L2 test framework, the CMake-only build. Issue #43 is the CPU–GPU architecture decision this phase has to make. |
| IV | **Initial Model Capability** — single domain, CPU *and GPU* efficiency focus | active community engagement expected summer/autumn 2026 | **Not visibly begun** as of 2026-09-15: no physics, no runnable single-domain model, no L3/L4 tests, no release. The date has slipped; nobody has said by how much. |
| V | Initial Operational Capability — multi-domain | | |
| VI | Complete first full code | first public release hoped for summer 2027 | **First public release: expected January 2027** per the advisor communication behind this repo's proposal ⚠ no cited NOAA source; ON 525 said summer 2027, and the state of the repository makes January look ambitious. |

Those dates are for the C++ path. Rust may take up to five years.

## What WW4's test levels mean for a WW3 port

WW4's `tests/README.md` declares a four-level strategy `(v)`: **L1** unit tests of single
functions in isolation (`L1_test_*`), **L2** integration tests of modules and their
interfaces (`L2_test_*`), **L3** functional tests of physics cases as a black box, and
**L4** regression tests of the full model. L1 and L2 exist, in GoogleTest, at full coverage
of the code that exists; L3 and L4 do not yet, because there is no physics to test.

This repo copies that structure on purpose, and the proposal says why: *do not compete
with WW4; build artefacts in the shape WW4 can use.* So the `W3SNL1` port in
[`12-porting-a-kernel-w3snl1.md`](12-porting-a-kernel-w3snl1.md) comes with `L1_test_*`
GoogleTest suites that compare the Kokkos kernel against captured Fortran fixtures, and an
`L2_replay.sh` that reruns a whole WW3 regtest with the kernel on and off and judges the
netCDF output with `nccmp-tol`. Same prefixes, same framework, same meaning of the levels.
When WW4 reaches the point of needing a DIA kernel and a way to prove it matches WW3, the
fixtures, the tolerances file and the parity report are the deliverables it can pick up —
and if it never does, they are still the evidence the lab needs to put the kernel into
operation. Either way the work is not wasted, which is the whole point of choosing the
shape before choosing the kernel.

## What this means for WW3 — and for you

**WW3 is not going away soon, but its end is now scheduled.** The report is explicit: the
cost of maintaining two models long-term will be mitigated by "formally sunsetting most
support for WW3 once WW4 is mature, with a clearly communicated transition period." Code
with no identified owner willing to port it stays in WW3 and is considered obsolete for
WW4.

Practically:

- **Learn WW3 now.** It is the mature, documented, validated, operational model, and it
  will be for years. Everything in this repo remains the right thing to learn.
- **The concepts transfer completely.** The action balance equation, source-term packages,
  spectral discretisation, CFL limits, grids, nesting, partitioning — none of that changes.
  WW4 is a software rewrite, not new physics.
- **The interfaces will not transfer.** Expect namelists to go (the report canvasses ASCII
  / YAML / namelist and notes the choice follows from the language), expect the binary
  `mod_def.ww3` / `out_grd.ww3` files to go (consensus to move to NetCDF, with interest in
  Zarr), and expect the compile-time switch file to be reconsidered — the pushback from
  researchers about recompiling between runs is recorded in the report.
- **The separate-executables workflow is under review.** `ww3_grid` / `ww3_prep` /
  `ww3_shel` / `ww3_ounf` as distinct programs is explicitly listed as a design decision
  to revisit.
- **If you were going to GPU-port WW3 with directives: don't.** WW4 Phase IV targets CPU
  *and* GPU efficiency in a code architected for it from the start, and any heroic
  OpenACC work on WW3 has a short shelf life. What *does* keep its value is a kernel
  written in the abstraction WW4 itself is converging on, validated in WW4's own test
  shape — which is what lessons 11–13 do, and why they use Kokkos rather than `!$acc`.

## One thing that must survive

The report singles out a property of WW3 that WW4 has to keep: **full numerical convergence
of its schemes**, achieved through the limiter formulation of Tolman (2002b). Without it
you cannot separate numerical error from physical error, which makes the model useless for
science. It's a good reminder that "modernisation" has constraints that aren't about
software at all.

→ [`15-swan.md`](15-swan.md) — the other model you should know.
