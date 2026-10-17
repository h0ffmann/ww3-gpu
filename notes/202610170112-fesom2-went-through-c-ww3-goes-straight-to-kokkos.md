---
id: "202610170112"
title: "FESOM2 went through C; WW3 goes straight to Kokkos"
type: literature
status: partial
source: "Koldunov et al. (2026), arXiv:2606.11356v1, read in full from the arXiv PDF on 2026-10-16; FESOM/fesom2 at tag fesom2.7.3-cport-instr (9271ae92); this repository at goal/42-port-six-expensive (daecd98), WW3-noaa 03adda91, WW3 fork 357f575c"
tags: [porting, evidence, literature, fesom2]
created: 2026-10-17
---

# FESOM2 went through C; WW3 goes straight to Kokkos

Koldunov, Cheedela, Danilov, Sidorenko, Beyer and Jung (2026), "An Ocean Model Ported by a
Large Language Model: Experience and Lessons from FESOM2 (Fortran to C to C++/Kokkos)",
arXiv:2606.11356v1 [physics.ao-ph], submitted 9 June 2026, one version,
DOI [10.48550/arXiv.2606.11356](https://doi.org/10.48550/arXiv.2606.11356),
<https://arxiv.org/abs/2606.11356> `(v)` (abstract page and PDF, read 2026-10-16). The authors are
at the Alfred Wegener Institute (AWI), Bremerhaven; Jung also at the University of Bremen `(v)`
(PDF, p. 1). The paper is already the proposal's `koldunov2026` (`pubs/proposal/refs.bib:32`,
`REFERENCES.md:30`, verdict *verified*) `(v)`. Section and page numbers below are the PDF's.

This note sets the paper's method beside this repository's, which takes WW3 routine by routine
straight from Fortran to C++/Kokkos, with no C stage, inside the running WW3.

## The paper's pipeline, validation and numbers

**Pipeline** (§4.1, pp. 5, 8, Fig. 1). Fortran first goes to a "clean, single-threaded C
reference", with every `USE`-global made an explicit struct field, then C to C++/Kokkos. The
second stage starts with a "language flip" that compiles the C as C++ with no algorithmic change,
bit-identical on Serial, OpenMP and CUDA before any device kernel exists (App. B.1, p. 26). The C
code stays in the Kokkos tree as the reference, and each Kokkos kernel keeps its C original as a
"twin" `(v)`. The paper gives three reasons for the C stage: it separates mistranslated numerics from
mis-parallelised numerics, puts the agent in a language better represented in its training data,
and collapses the configurable Fortran onto the one configuration run (§4.1) `(v)`.

**Agent** (§4.3, p. 6). Claude Code driving Claude Opus 4.7 "at max reasoning effort and running
in auto-accept mode", on DKRZ's Levante; many short sessions carried by a plan file, a
lessons-learned file and a handoff file, built on the cc-thingz skills. The human set strategy,
reviewed plans and diffs, and caught physics and validation errors; the agent read, translated,
built harnesses, debugged, profiled and documented `(v)`. They credit a strictly literal
translation (§4.2) and the rule that "any statement about what the Fortran does with a constant must
quote the exact file:line and the literal value read there" (§4.2, p. 6) `(v)`.

**Validation** (§4.4, §5, Fig. 2). A ladder whose acceptance changes with the stage `(v)`:

| Tier | Compared | Acceptance |
|---|---|---|
| Stage 1, C against Fortran | five-year run, CORE2 mesh, production time step | statistics: five-year-mean RMS difference 0.006 °C in SST and 0.002 PSU in SSS, interior differences statistically zero below 700 m; sea-ice velocity correlation about 0.92 (§5.1, p. 9) |
| R1, each Kokkos kernel against its C twin | `FESOM_KK_VERIFY`, run live in the model on the same state | max abs difference 0 on Serial (§4.4, p. 7) |
| R2, whole model, Serial | one simulated year, 17 280 steps, 13 monthly snapshots | byte-identical to the C (§5.2, p. 10) |
| R3, OpenMP | as R2 | identical for map and gather kernels; ≲ 1e-12 per step where threads sum (§4.4) |
| R4, CUDA | 20-step run with active sea ice, before every commit | per-field ceilings above a recorded floor (§4.4) |
| R5, multi-year | CUDA against C and against Fortran | correlation and bias; SST correlation 1.0, bias +1.0e-4 °C against C (Table 3) |

They write that bit-for-bit agreement between Fortran and C "across compilers and arithmetic
orderings is neither attainable nor necessary" (§1, p. 2) `(v)`. Their Fortran reference was built
with Intel oneAPI 2022.0.1 (Code availability, p. 18) `(v)`; the compiler of the C build is not
stated ⚠. Bit identity holds only at a fixed MPI rank count (§4.4) `(v)`. On the GPU the
divergence comes from fused multiply-add and from the device `exp` and `log` in the equation of
state, and from reordered sums (§5.2, App. B.2) `(v)`. Debugging tools, each behind an
environment variable: per-substep dumps, an identical-input operator diff, range probes, a
stale-halo probe and subsystem disable-switches (§4.5) `(v)`.

**Does FESOM2 have regression tests?** Yes. At the paper's tag (`fesom2.7.3-cport-instr`,
`9271ae92`), `.github/workflows/fesom2_main.yml` builds the model, runs the `pi` mesh setup
`test_pi` and checks it with `fcheck`, against "truth values" of six global fields kept in
`setups/test_pi/setup.yml:58-64`; other workflows run cavity, channel, Icepack, OpenMP and other
setups, and `tests/` holds a CTest framework of integration runs `(v)` (read 2026-10-16).
`fcheck` and `mkrun` are not in the repository, so their tolerance is unknown ⚠. The paper does
not mention this suite: a search of its text for "regression", "test suite", "fcheck" and "CTest"
finds only the ladder's own use of "regression" `(v)`. Its quick kernel gates ran on the same `pi`
mesh (App. B.1, p. 26) `(v)`. Whether FESOM2's CI tests were run on the C or Kokkos port is not
stated ⚠.

**Size** (§3, Table 2, p. 7). The configuration exercises 109 top-level `src/*.F90` files,
73 991 lines; the full distribution is "211 files and 268,910 lines"; the C port is 20 010 lines
in 73 files, the Kokkos port 30 663 lines in 78 files `(v)`. The 73 991 reproduces exactly; the
full-distribution figure does not (counts below). The owner's "79k" is not in the paper `(v)`. The
core plus `src/cvmix_driver/` gives 79 579, one combination that matches it; the paper keeps CVMix
out of the ported configuration ⚠. They ported the whole exercised core, except I/O, which was
redesigned, not translated (§4.3) `(v)`; restart I/O, other vertical coordinates and the coupled
packages are not yet ported (§7.3) `(v)`.

**Hardware and speed** (§6). Levante: a GPU node is 4 A100-80GB and 2 EPYC-7763; a CPU node is 2
EPYC-7763, 128 cores `(v)`. Times per step for five builds on four meshes, 0.13 to 7.4 million
surface vertices (Fig. 5); the C port is within about 2 % of the Fortran, the Kokkos CPU build
13 % to 18 % slower than the C `(v)`. Node for node, a GPU node started 3.8 times slower than a CPU
node and ended 1.6 to 3.7 times faster on the large meshes; the gain came from making halo fields
device-resident (16.3 s to 2.7 s per step at four nodes) and then from coalesced kernels, down to
1.27 s (§6.2, Table 4) `(v)`. The same source ran on GH200 (JUPITER), H100 PCIe (MareNostrum 5)
and MI250X through HIP (LUMI), 1.3 to 3.5 SYPD at 32 nodes (§6.3) `(v)`. The number of repetitions behind
each time is not stated ⚠.

**Effort** (Table 2, Abstract). Days with commits: 10 for the C port, 7 for Kokkos; "a matter of
weeks" `(v)`. An earlier attempt by the first author failed on accumulated "improvements" (§4.2)
`(v)`.

## The same questions for this repository

**Size of WW3.** Every number is from the repository root, `WW3-noaa` at `03adda91`, the fork's
base (v, run 2026-10-16):

| Count | Lines | Command |
|---|---|---|
| `.F90` in `model/src`, recursive, the port score's denominator | 263 375 | `find WW3-noaa/model/src -name '*.F90' \| xargs wc -l \| tail -1`; equals `ww3_lines` in `kokkos/ports.tsv`, which `scripts/port_score.py --measure` writes by counting the newlines of every `.F90` under `model/src` at `ww3_rev` with `git show` |
| the same, top level only | 258 952 | `find WW3-noaa/model/src -maxdepth 1 -name '*.F90' \| xargs wc -l \| tail -1` |
| the same, non-blank and not a `!` comment | 143 726 | `find WW3-noaa/model/src -name '*.F90' -print0 \| xargs -0 cat \| grep -v -E '^\s*(!\|$)' \| wc -l` |
| every Fortran file in `model/src` (adds `mod_xnl4v5.f90`, 8 018 lines, and SCRIP's `.f` and `.F`) | 285 085 | `find WW3-noaa/model/src -type f \( -name '*.F90' -o -name '*.f90' -o -name '*.f' -o -name '*.F' \) \| xargs wc -l \| tail -1` |
| every Fortran file of the repository except `regtests/` | 288 143 | the same `find` on `WW3-noaa` with `-not -path 'WW3-noaa/regtests/*'` |
| the fork at `357f575c`, `.F90` in `model/src` | 267 027 | `find WW3/model/src -name '*.F90' \| xargs wc -l \| tail -1`; the 3 652 more lines are the KOKKOS shims and caller hunks |

The owner's "almost 300k" is the 285 085 of every Fortran file in `model/src`, or the 288 143 of
the repository without its regtests; the port score's 263 375 counts `.F90` only `(v)`. FESOM2 at
`9271ae92`, the same way: `cat src/*.F90 | wc -l` gives 73 991, recursive `src` `.F90` 256 348 in
192 files, every Fortran file of the repository 259 546 (v, run 2026-10-16, from
`git clone --depth 1 --branch fesom2.7.3-cport-instr https://github.com/FESOM/fesom2.git`).

So the two distributions are about the same size: 259 546 to 268 910 lines for FESOM2, 263 375
to 288 143 for WW3. The 74k against 300k compares FESOM2's exercised core with all of WW3. The
like-for-like number for WW3, the lines its lab switch compiles and runs, is not measured: the
H100 plan's 25 to 35 thousand is an estimate ⚠ (`docs/KOKKOS_H100_PLAN_202609.md:149`).

**Method.** Each routine goes from the Fortran at the pin straight to C++/Kokkos, with no C stage
`(v)` (`docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`, "Method"). One agent reads the routine as the
lab switch preprocesses it, with gfortran's `-O3` GIMPLE and assembly, and translates it; a fresh
agent then reviews the C++ object against the Fortran object (same file) `(v)`. Between
checkpoints bit parity is argued from the `-O3` GIMPLE of both compilers (ADR-0011, Decision, item
4) `(v)`. The only tests are WW3's regtests, run inside WW3 with the Fortran routine and with the
Kokkos routine, compared by WW3's `test.comp`; `nccmp-tol` measures, it does not pass
([ADR-0010](../docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md); `AGENTS.md`, "Bit
for bit") `(v)`. Each wired part has its own run-time flag, off by default, and `WW_KOKKOS_ALL`
turns on every wired part (`docs/GLOSSARY.md:647`) `(v)`. The regtests on the H100 wait for
access to LabECO's H100 at UFSC, which issue #143 asks for
(`pubs/relatorio-gta/relatorio.easy.pt-BR.md`, §7) `(v)`; that the next checkpoints were deferred to
that machine is the owner's statement of 2026-10-16, not found in a file ⚠.

**Porting throughput.** On `goal/42-port-six-expensive`, 2026-10-16, from
`git log --date=iso --format='%h %ad %s' origin/main..goal/42-port-six-expensive`: the first
claim at 20:42:14 -0300, twelve translations, each with one fresh review, the last at 21:50:03,
67.8 minutes, about 10.6 routines per hour `(v)`. Their source ranges in `kokkos/ports.tsv` sum to
12 425 Fortran lines `(v)`. Most are host-side I/O and set-up with no floating-point arithmetic,
and `W3MPIO`'s 3 340 lines preprocess to an empty routine under the lab switch (its commit
`073089e`) `(v)`; none is wired or regtested `(v)`. This is translation throughput, not port
throughput: no routine of that hour has met a regtest.

**Score at the same commit** (`kokkos/PORT_STATUS.md`, `scripts/port_score.py --write`) `(v)`:
translated, 99.3 % of the profiled run time and 65 429 of 263 375 lines; ported (wired), 87.6 %;
regtested, 48.6 % and 2 677 lines. Nothing has run on a GPU inside WW3.

## Side by side

| Aspect | FESOM2 (Koldunov et al., 2026) | ww3-gpu |
|---|---|---|
| Route | Fortran → C → C++/Kokkos (§4.1) | Fortran → C++/Kokkos per routine, inside WW3 |
| Size of the source | 73 991 lines exercised; distribution 268 910 (paper) or 259 546 (counted) | 263 375 `.F90` lines in `model/src`; 285 085 with every Fortran file; exercised subset not measured ⚠ |
| Reference | the C reference, itself checked against the Fortran by statistics | the Fortran routine, in the same WW3 executable, selected at run time |
| Acceptance, CPU | Serial byte-identical to C over one year; C within climate statistics of Fortran | byte-identical to the Fortran by `test.comp` on every regtest entry that exercises the routine |
| Acceptance, GPU | statistically close, recorded floor, 20-step gate per commit | bit for bit, not yet shown; GPU runs need the owner |
| Test cases | `pi` mesh gates; CORE2 one-year and five-year runs; four meshes for speed | WW3's `matrix.base` entries (311 selected for `NL1`, ADR-0011); `ww3_ts1`, `bench_small` |
| Model's own test suite | exists (`fcheck` CI); not mentioned in the paper | the only tests (ADR-0010) |
| Configuration collapse | by writing the C for one configuration | by translating the preprocessed source of one switch file |
| Per-kernel check | live twin comparison, `FESOM_KK_VERIFY` | static `-O3` GIMPLE comparison and a fresh review; per-part flags for bisection at a checkpoint |
| Agent | Claude Code, Opus 4.7, max effort, auto-accept | Claude Code with subagents, one translator and one fresh reviewer per routine |
| Hardware | A100, GH200, H100 PCIe, MI250X | i9-14900 CPU; RTX 4090 for one kernel benchmark; H100 not yet available |
| Speed | 1.6 to 3.7 times a CPU node; 1.3 to 3.5 SYPD | no timing inside WW3 on a GPU |
| Time | 10 + 7 days with commits | 30 calendar days for `W3SNL1` (ADR-0011); 12 routines translated in 68 minutes on 2026-10-16 |

## Why WW3 has no C stage

Lines are not the argument by themselves: the two distributions are about the same size. The
arguments are these.

1. **A C stage is a second complete translation with its own validation.** FESOM2 needed a
   five-year comparison to accept its C, and the paper counts its cost: "another code base to
   build, validate, document and maintain" (§7.3, p. 16) `(v)`. For this project that is a C WW3
   that runs on its own, its own I/O, then the Kokkos translation. ADR-0011 measured one routine
   at 30 calendar days and judged the per-routine regtest cadence unable to finish in six months
   `(v)`; a stage that doubles the translations does not fit either ⚠ (an inference, not a
   measurement).
2. **The reference the C stage gives already exists.** FESOM2 needed C because it wanted a
   deterministic reference built by the same compiler as the Kokkos code. Here the Fortran
   routine stays in the executable behind its flag, and WW3's own regtests compare the two
   paths. For `W3SNL1` that comparison is byte-identical on all 100 serial `NL1` entries and 25
   OpenMP entries `(v)` ([[202610141900-bit-for-bit-is-the-gate]]). The paper's claim that
   cross-language bit identity is not attainable holds for its toolchain, Intel Fortran against a
   C build; with gfortran and g++ it was attained for this routine. That the shared GCC optimiser
   is the reason is an inference ⚠.
3. **The configuration is pinned by the preprocessor.** WW3 is CPP-preprocessed since 7.14, so
   the translator reads the routine as one switch file compiles it
   (`docs/KOKKOS_H100_PLAN_202609.md:20`, `W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`, "Method") `(v)`.
   The C stage's benefit of following no inactive branch comes for free.
4. **The port grows inside a running model.** FESOM2 replaced the model; this repository swaps
   routines into WW3 one flag at a time, so the remaining Fortran is the harness. There is no
   standalone C model to run for five years, and none is needed for a routine to be judged.

## The paper's advantages over this repository

- **Long runs at the production configuration.** Its costliest bug, a constant of 1e-9 where the
  Fortran has 0.1, showed only after about 110 model days at the production time step (§7.1)
  `(v)`. WW3's regtests are short cases; a byte-identical regtest finds such a bug only where the
  case exercises it. The proposal's operational case is the place for a long run ⚠.
- **A running model on four GPUs from two vendors, with measured speed.** This repository has no
  GPU run inside WW3 `(v)`.
- **Evidence that GPU bit identity fails.** Their CUDA build stops being bit-identical at the first
  device kernel that calls `exp` and `log` (App. B.2) `(v)`. This repository's GPU rule asks for
  glibc's bits from CUDA's math library, which `AGENTS.md` already marks ⚠. The paper's floor
  and per-field ceilings are a worked answer if that rule cannot be met; choosing it is the owner's
  decision.
- **Checks that run during the model run.** The twin check runs every call of a long run, on
  live state, and restores its own output (§4.4) `(v)`. The static GIMPLE argument covers the
  host build and every input but runs nothing; a checkpoint runs the regtests only.

## What could be adopted

1. The `file:line` rule for constants is already this repository's ("WW3 source is read-only")
   `(v)`; the paper's history of it is a reason to keep it.
2. A twin comparison inside a WW3 run, both paths called on the same state behind a flag, would
   be a diagnostic inside a regtest run, not a test of the port's own. Whether ADR-0010 allows it
   is a question for the owner ⚠.
3. For phase 2, when the spectrum stays on the device: the host-copy NaN poisoning that found
   their stale-field bug (App. B.2) and a sync map that aborts on a stale read (App. B.1) `(v)`.
4. A recorded GPU divergence floor, if bit for bit on the GPU proves unattainable, as a fallback
   for the owner to accept or refuse ⚠.

## What in this repository disagrees with the paper or with practice

- `docs/KOKKOS_H100_PLAN_202609.md:14` says the port follows the two-stage recipe, and §6 and §9 plan a
  C reference (`ww3c/`); `pubs/proposal/pt/07-methodology.md:16` and its English mirror promise a
  literal C translation first. The port has no C stage `(v)`. The proposal goes to its reviewer.
- `course/13-bulk-porting-with-agents.md:18` says each rung is checked "on captured inputs"; the
  paper accepts the C by five-year statistics and uses identical inputs only as a debugging tool
  (§4.5, §5.1) `(v)`.
- The plan's Task 12 (§8) expects "C timing vs Fortran (expect within ±5%, as FESOM2 saw)"; the paper
  reports about 2 % (§6.1) `(v)`.

What the paper calls the literal-translation rule is this repository's
[[202610141904-ww3-is-read-only]] and "translate, don't improve"; the order in which routines are
drawn out is [[202610141901-port-order-is-measured-wall-time]], and what counts as done is
[[202610150226-a-partial-distillation-of-ww3-on-my-own]].
