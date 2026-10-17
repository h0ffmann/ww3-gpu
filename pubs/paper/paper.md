---
title: "Bit for bit with the Fortran: an agent-written Kokkos port of the WAVEWATCH III discrete interaction approximation, judged by the model's own regression tests"
author:
  - Matheus Hoffmann
thanks: "Escola Politécnica, Universidade Federal do Rio de Janeiro, Brazil. ORCID 0009-0009-1056-7661."
date: "v0 draft, 2026-10-16"
abstract: |
  WAVEWATCH III (WW3) spends about half of a single-thread run in one routine, the discrete
  interaction approximation (DIA) of the nonlinear four-wave transfer (48.4 % of self time on a
  9 520-point test case). We translated that routine and its table set-up from Fortran to
  C++/Kokkos with a coding agent, called it from WW3 behind a run-time switch, and asked for the
  strictest acceptance criterion we could state: every output file of WW3's regression tests
  byte-identical between the Fortran routine and the Kokkos routine, inside the same WW3
  executable. On the CPU this holds for all 100 serial and all 25 shared-memory OpenMP
  entries of WW3's regression matrix that select the DIA. Reaching it required one finding that
  reading the source could not give: gfortran expands `x**11` as a different chain of
  multiplications at `-O0` and at `-O3`, and the two chains disagree on 643 427 of 1 000 000
  single-precision inputs. The reference for a bit-for-bit port is therefore the optimised code
  the model's build produces, not the Fortran text. Before that finding, three entries of our
  research log explained the residual differences as round-off amplified by WW3; the explanation
  was wrong, and we report it. We place the result against a recent LLM-written port of the
  FESOM2 ocean model to Kokkos, which held bit-identity against an intermediate C reference and
  argued that, for a whole model, a change of language, compiler and arithmetic ordering makes
  bit-identity against the Fortran neither attainable nor meaningful. For one routine whose
  operation order is preserved, built by gfortran at `-O3` and run on the CPU, a change of language
  alone did not prevent it. We report no speed-up: the port has not
  yet run on a GPU inside WW3, and the repository's rules forbid a timing without a passed
  regression replay on the same commit.
keywords: [WAVEWATCH III, discrete interaction approximation, Kokkos, bit-for-bit reproducibility, regression testing, LLM-assisted code translation]
bibliography: refs.bib
link-citations: true
header-includes: |
  \sloppy
numbersections: true
arxiv:
  primary: physics.ao-ph
  cross-list: [physics.comp-ph, cs.DC]
  license: http://creativecommons.org/licenses/by/4.0/
  comments: "v0 preprint, not peer reviewed. 2 tables. Every number is reproduced by a command in https://github.com/h0ffmann/ww3-gpu"
---

# How to read this paper {.unnumbered}

This is a report of one result and the method that produced it, not a survey and not a position
paper. Every number about this work comes from a file committed to the repository named under
*Availability* and is listed, with the command that reproduces it, in the last section; numbers
about other work are from the cited source. Claims about other work were checked against the
source itself; the reference log
(`pubs/paper/REFERENCES.md`) records what was read of each source, how and on which day. Where a
claim was not checked, the text says so.

# Introduction

Spectral wind-wave models integrate the wave action balance equation
$$
\frac{\partial N}{\partial t} + \nabla_{x}\cdot(\dot{\mathbf{x}}N)
+ \frac{\partial}{\partial k}(\dot{k}N) + \frac{\partial}{\partial \theta}(\dot{\theta}N)
= \frac{S}{\sigma},
$$
where $N(k,\theta;\mathbf{x},t)$ is the action density spectrum, $\sigma$ the intrinsic
frequency and $S$ the sum of the source terms: wind input, whitecapping dissipation, the
nonlinear four-wave transfer and, in shallow water, bottom friction and depth-induced breaking
[@tolman1991; @ww3manual]. The four-wave transfer is a six-dimensional Boltzmann integral, and
operational models replace it with the DIA of @hasselmann1985, which evaluates the transfer for
one representative quadruplet configuration and its mirror at every spectral bin. WW3 implements
it in `W3SNL1` (`model/src/w3snl1md.F90:115-473`), with its interpolation tables built by
`INSNL1`. Every WW3 line cited here is at commit `6d1bcdfe` of the fork that carries the shim, the
build all results were made with; `w3snl1md.F90` and `w3iogrmd.F90` are identical there and at
the NOAA-EMC commit `03adda91` it branched from.

The source terms are local in space, so they are the natural first target for a GPU. Two groups
have moved them before. @ikuyajolu2023 identified WW3's source-term module `W3SRCEMD` as its most
expensive and accelerated it on GPUs for the E3SM Earth system model, reporting average speed-ups
of 2 and 4 times on P100 and V100 GPUs and data transfer between CPU and GPU as the limit.
@yuan2024 ported all the expensive components of WAM Cycle 6 to GPUs and report a speed-up of up
to 37 times on eight A100 GPUs over a dual-socket CPU node. @ikuyajolu2023 verified the GPU model
against the CPU model by the significant wave height of a 5-hour run, averaged over its last two
hours, and judged the relative difference "negligible and acceptable", attributing it to the
precision of the GPU's arithmetic (their Section 3.3 and Figure 7). For @yuan2024 we have read only
the abstract, which does not state how the GPU results were compared with the CPU. This work asks a
narrower question, at the step before any GPU run: can an agent-written
C++/Kokkos [@trott2022] translation of the DIA reproduce the Fortran's bits inside the running
model, under the model's own regression tests, with no test written by the agent?

The question matters for two reasons. First, a wave model's regression suite compares output
files; a port that changes a last bit makes every downstream comparison a matter of judgement
about tolerances, and a tolerance chosen by the porter is evidence chosen by the porter. Second,
large language models are increasingly used for such translations
[@ranasinghe2025; @chen2024fortran2cpp; @koldunov2026], and an acceptance criterion the agent
cannot negotiate is the cheapest defence against a translation that compiles, runs and is subtly
wrong.

The contributions are:

1. A bit-for-bit criterion for a routine-level port, stated against the model's own executable
   and judged by the model's own regression tests and comparison tool, with a measured rule for
   the one excuse it allows (Section 3).
2. The result for the DIA on the CPU: 125 of 125 regression entries byte-identical (Section 4.1).
3. A compiler finding that decided the result: the reference is the optimised code, and
   `x**11` in gfortran is a different product at `-O0` and `-O3` (Section 4.2).
4. A record of a wrong explanation that the criterion exposed (Section 4.3).
5. A comparison with the closest prior work, the LLM-written FESOM2 port [@koldunov2026], on
   what each criterion can and cannot show (Section 5).

# Setting

## The routine and its cost

The cost profile that set the port order was measured with `gprof` on a test case of 120 by 80
cells with 9 520 sea points, integrated for 12 hours on one thread, with WW3 at commit
`6d1bcdfe` and the switch file of the repository's lab (`switches/switch_lab_shrd`, physics
package ST4, nonlinear package NL1). `W3SNL1` takes 48.4 % of self time, the ST4 dissipation
`W3SDS4` 23.2 %, the ST4 wind input `W3SIN4` 12.7 %, and the source-term integrator `W3SRCE`
5.6 %. The rule that the next routine to port is the most expensive one by this profile, not the
easiest to validate, is a recorded decision of the project (ADR-0002 in the repository).

## The port

The port is a translation, not a rewrite. The Kokkos kernel reproduces the Fortran's arithmetic
in the Fortran's order: no vectorisation, no reassociation, no change of reduction order, and
floating-point contraction disabled (`-ffp-contract=off` on the host, `--fmad=false` for CUDA),
because a fused multiply-add changes the result of `a*b+c`. A `bind(C)` shim
and a caller patch on a fork branch select the Kokkos routine when the environment variable
`WW_KOKKOS_SNL1=1` is set and the Fortran routine otherwise, so both live in one executable. In
this first phase the shim copies one sea point's spectrum in and out at every call, inside the
`!$OMP CRITICAL` section where WW3 already calls the DIA. The memory layout and the copies are
made for validation and are the wrong ones for speed. WW3's physics is not edited: the only change
to WW3 is that caller patch, which selects one routine or the other.

## Who wrote it

The translation, the shim, the comparison scripts and the investigations of Section 4 were
written by a coding agent (Claude Code) working in the repository under rules that the author
wrote and the agent reads at the start of every session (`AGENTS.md`). The rules that bear on
this result are: translate, do not improve; never widen a tolerance; never write a unit test of a
ported routine, so that the only judge is WW3's suite; and record every attempt, including the
failed ones, in a dated research log. Section 6 states what this does and does not show about
agents.

# The criterion

**Statement.** A ported routine passes when, for every entry of WW3's regression matrix
(`regtests/bin/matrix.base` in WW3) whose switch file selects the routine's package, WW3's own
comparison script `test.comp` finds no output file that differs byte for byte between two runs
of the same executable, one with `WW_KOKKOS_SNL1=0` and one with `WW_KOKKOS_SNL1=1`. Same
build, same switches, same compiler flags, same inputs, same thread count.

**The one excuse.** Some WW3 configurations are not deterministic in themselves. When
`test.comp` finds a difference, the matrix driver runs the Fortran a second time; a file that
two Fortran runs write differently is recorded as WW3's own and excused, and a file that differs
between Fortran and Kokkos but not between Fortran and Fortran fails the entry. The excuse is
measured per file, never granted per entry in advance.

**What a tolerance is for.** The field comparator `nccmp-tol` reports the largest relative
difference per variable against tolerances in a versioned file. It describes how large a
difference is; it never turns a difference into a pass. In this paper its verdicts are reported
for completeness and play no part in the result.

**Why not unit tests.** A unit test of a ported routine checks the routine against inputs the
porter captured. Its failure mode is the one this criterion is built to avoid: the agent that
wrote the routine also chooses what it is tested on. The project dropped its own fixture tests
when the regression matrix ran (ADR-0010 in the repository). Section 4.2 shows a concrete case
where the fixture tests passed bit for bit and the model did not.

# Results

All runs in this section were made on one workstation (Intel Core i9-14900, 32 hardware threads,
CPU only), with WW3 at commit `6d1bcdfe` of the fork branch that carries the shim, gfortran
15.3.0 from the repository's pinned toolchain, on 2026-10-14 and 2026-10-15 (UTC).

## The regression matrix

Table 1 gives the verdicts. Of the `matrix.base` entries whose switch file contains `NL1`, the
matrix selected the 100 that run serially (29 configurations of test `ww3_ts3`, 25 of `ww3_ts2`,
15 of `ww3_ts1`, 14 of `ww3_ta1` and 17 of other tests) and the 25 that use shared-memory OpenMP
(`OMPG`), all configurations of `ww3_ts3`. All 125 built, ran, logged the Kokkos DIA
and produced output with zero differing files under `test.comp`. The OpenMP entries needed
`OMP_MAX_ACTIVE_LEVELS=2`: the shim is called inside WW3's own `!$OMP PARALLEL` region, and
without nested parallelism allowed Kokkos aborts at the first DIA call.

| Selection | Entries | `test.comp`: 0 differing files | `nccmp-tol` PASS / n/a / FAIL |
|---|---|---|---|
| serial `NL1` | 100 | 100 | 71 / 29 / 0 |
| OpenMP `NL1` (`OMPG`) | 25 | 25 | 25 / 0 / 0 |
| serial `NL1`, before the fix of Section 4.2 | 100 | 9 | 66 / 29 / 5 |

: Verdicts of the WW3 regression matrix, Kokkos DIA against Fortran DIA in one executable. "n/a"
marks entries with no NetCDF field output for `nccmp-tol` to read; `test.comp` judged them all.
The records are listed in the last section.

Two whole-model runs outside the matrix agree: regression test `ww3_ts1` and the 9 520-point case
of Section 2 give significant wave height with a largest relative difference of exactly 0, over
119 652 values for the latter, both against the Fortran DIA in the same executable and against a
WW3 built from the unmodified source without the shim.

## The reference is the optimised code

The first full matrix run (third row of Table 1) was byte-identical on only 9 of 100 entries.
The differences were small: in `ww3_ts1`, a largest relative difference in significant wave
height of $9.879\times10^{-8}$, and $1.832\times10^{-7}$ on the 9 520-point case. Five entries
failed `nccmp-tol`. At the same time the project's fixture tests, which compared the Kokkos
routine with the Fortran routine compiled at `-g` on captured inputs, were bit-identical.

The entries that were already byte-identical gave the clue: two of them use the Gaussian
quadrature variant of the nonlinear transfer, whose set-up never calls `INSNL1`
(`w3iogrmd.F90:1801-1804`), so the suspect was the tables, not the kernel. The agent compiled
`w3snl1md.F90` with the flags of WW3's CMake build plus `-fdump-tree-optimized` and read the
optimised GIMPLE of `INSNL1` and `W3SNL1` line by line against the C++. One line differed in
kind. `AF11A = (SIG(IFR)*TPIINV)**11` (`w3snl1md.F90:741`, and `:750` for the other branch) is an
integer power of a single-precision real. At `-O0` gfortran calls libgcc's `__powisf2`, whose
binary chain ends with $x^3\cdot x^8$. At `-O3` GCC's power tree expands it inline as
$x^2=x\cdot x$, $x^3=x^2\cdot x$, $x^5=x^2\cdot x^3$, $x^6=x^3\cdot x^3$,
$x^{11}=x^5\cdot x^6$. Both are exact in real arithmetic and differ in rounding. On 1 000 000
single-precision inputs in $[0.02, 1)$, called from C++ and compared bit for bit, the $x^3x^8$
chain matches the `-O0` Fortran on all inputs and the $x^5x^6$ chain differs on 643 427; against
the `-O3` Fortran the counts reverse. The translation had followed the `-O0` chain, which is what
the fixture build used.

Changing that one function to the `-O3` chain made all 100 serial entries byte-identical (first
row of Table 1). No tolerance, no Fortran, no compiler flag and no WW3 file was changed. Nothing
else in the optimised code differed from the C++: runtime integer exponents such as `XFR**IFRP`
remain calls to `__powisf2` at `-O3` and are reproduced by calling the same chain.

The general point is not specific to `**11`. A bit-for-bit port of Fortran is a port of what a
given compiler emits for that Fortran at the model's optimisation level, and source that is
unambiguous in real arithmetic is ambiguous in floating point [@monniaux2008]. The repository now
requires the comparison of the `-O3` GIMPLE of the Fortran and the C++ for every ported routine
between regression checkpoints (ADR-0011 in the repository). The finding also has a limit: the
chain is GCC's, so a WW3 built with another compiler, or at another optimisation level, computes
`AF11A` differently again, and the port follows the build that WW3's CMake makes with gfortran.

## A wrong explanation the criterion exposed

Before the GIMPLE comparison, three dated entries of the research log examined the five failing
entries one at a time and explained each as WW3 amplifying a round-off-sized perturbation of the
DIA by about three orders of magnitude over the run, supported by the observation that enabling
fused multiply-add in the Fortran alone produced differences of similar size. The explanation was
plausible, consistent with the data then in hand, and wrong: after the `AF11A` fix there was
nothing to amplify, and the same entries are byte-identical. The last of the three entries had
begun to discuss a per-entry exception to the criterion. Under a tolerance-based criterion the
five entries would have been tuned until they passed and the cause would not have been found; the
byte-level criterion left no room for that. The log entries are kept as written, with a pointer
to the one that supersedes them.

## Scope of the rest of the port

The DIA is one routine of a larger effort, and the project reports three levels for the whole
port, from its ledger (`kokkos/ports.tsv`) and the profile of Section 2.1. *Translated*: a Kokkos
translation exists for 293 of WW3's 1 023 routines, covering 99.3 % of profiled run time.
*Ported*: WW3 at the fork's pin calls 10 of them behind a run-time flag, 87.4 % of run time,
among them the ST4 input, dissipation and peak routines. *Regtested*: 2 routines, `W3SNL1` and
`INSNL1`, 48.4 % of run time, are the ones this paper reports. The ST4 routines were
byte-identical on `ww3_ts1` and the 9 520-point case, but their regression matrix has not been
recorded, and this paper makes no claim for them. The gap between the first and third numbers is
itself a finding: with an agent, translation outran validation under a strict criterion by two
orders of magnitude in routines, and the third number, not the first, is the one a reader of a
port should ask for.

# Comparison with the FESOM2 port

The closest prior work is the port of the FESOM2 ocean and sea-ice model to C++/Kokkos by
@koldunov2026, also written by a coding agent (Claude Code) under a literal-translation rule.
Their abstract names three practices that mattered most: two translation stages, a literal
translation in which the assistant "was not permitted to 'improve' the source", and an acceptance
criterion suited to each stage. Our "translate, do not improve" rule is the same as the second;
we use one stage, not two, and the comparison is in the third.

| | FESOM2 [@koldunov2026] | This work |
|---|---|---|
| Unit of port | the whole model, about 74 000 lines of core Fortran | one routine and its tables, behind a run-time switch |
| Path | Fortran to C, then C to Kokkos | Fortran to Kokkos, the Fortran kept in the same executable |
| Reference for bit-identity | the intermediate C port, on CPU | the Fortran routine, in the model's own `-O3` build |
| Fortran to new language | statistical: five-year means of sea-surface temperature and salinity | byte-identical output files |
| Judge | a validation ladder built for the port, with multi-year statistics | the model's existing regression suite and its comparison script |
| GPU | run, statistically close; not bit-identical (fused multiply-add, device `exp`/`log`, summation order) | not yet run inside the model |
| Speed | reported, 1.6 to 3.7 times per node on A100 | none reported |
| Precision | double | single, as WW3's `REAL` |

: The two ports side by side. FESOM2 facts are from the paper's text, read 2026-10-16.

@koldunov2026 write that their first stage "cannot be bit-for-bit: a change of language, compiler
and arithmetic ordering makes byte-level equality neither attainable nor meaningful" (their
Section 5). Our result does not contradict that statement for a whole model; it narrows it. When
the arithmetic order is held fixed and the reference is the compiler's optimised output rather
than the source, a change of language alone did not prevent byte-level equality for this routine
on this compiler. Whether that survives a whole model, with global reductions, halo exchanges and
I/O, is open; their two-stage design is one answer to that difficulty, and a routine-level
switch inside the original executable is another, with a different cost. Their port is also far
larger and faster than ours: they report production throughput on four GPU architectures, and we
report none.

What the routine-level criterion buys is that the judge is not written by the porter. The FESOM2
validation ladder is careful, but it was built for the port; WW3's matrix and `test.comp` existed
before this project and are maintained by the model's developers. What it costs is reach: a
byte-level criterion is all or nothing, and it will not by itself say whether a GPU result that
cannot be bit-identical is good enough. Section 7 states how we intend to meet that.

# What this does and does not show

**About the routine.** The DIA's Kokkos translation reproduces the Fortran's bits inside WW3 on
the CPU, for the serial and shared-memory OpenMP configurations of WW3's regression matrix that
select it, with gfortran at `-O3`. It does not show:

- anything on a GPU: the CUDA build of the port links, but it has not run inside WW3;
- anything under MPI: the matrix's MPI entries were not run;
- anything with another compiler: the `AF11A` chain is GCC's;
- anything about speed: inside WW3 on one thread, the first-phase shim makes the run slower
  than the Fortran, and no timing has passed the repository's gate (a timing enters a table only
  after a passed regression replay on the same commit).

The ST4 routines are wired into WW3 but not covered by a recorded matrix, and nothing here claims
them.

**About the records.** The three matrix records were produced from working trees that the driver
marked as modified (the change under test, committed in the same commit). The WW3 fork has since
moved to a later pin, and the matrix driver has been rewritten as a CTest suite. A re-run at a
clean commit with the current driver is listed in Section 7 as the condition for the first
version of this paper on arXiv.

**About agents.** This is one routine, one author, one agent, one model family. It shows that an
agent can be held to a byte-level criterion and that the criterion catches a wrong explanation the
agent itself produced; it does not show that agents translate better or worse than people, and it
was not designed to.

# Next steps

1. Re-run both matrix selections at a clean commit at the current WW3 pin with the current
   driver (`just ww3-gpu-matrix 8 serial`, `just ww3-gpu-matrix 4 omp` with `WW3GPU_STRICT=ON`),
   and replace the records of Table 1. This is the condition for submitting v1.
2. Record the ST4 matrix (`W3SDS4`, `W3SIN4`, `W3SPR4`, the next 39 % of run time) and report
   it as a replacement version.
3. Run the DIA inside WW3 on a GPU under the owner's supervision. Whether a GPU can meet the
   criterion depends on whether the device's `expf` and `logf` return glibc's bits for the inputs
   the DIA sees; contraction off is necessary and not sufficient. If it cannot, the result will
   be reported as such, with the size of the difference, and not as a pass.
4. Then, and only then, a timing: kernel-only and end-to-end, median of three runs, with the
   spectrum kept resident on the device (phase 2), against the Fortran on the same node.
5. The MPI entries of the matrix, and a second compiler, to bound the `AF11A` result.

# Statements {.unnumbered}

**Authorship.** Matheus Hoffmann designed the study, wrote the rules the agent worked under,
ran or supervised every run reported here and is responsible for the content.

**Use of AI.** The Kokkos translation, the shim, the comparison scripts, the investigations of
Section 4 and drafts of this text were produced by a Claude coding agent (Anthropic) in the
repository, under the author's direction. Every reference was fetched and checked at the time of
writing, and the checks are recorded in the reference log. The agent's configuration (rules,
skills, hooks) is published in the repository's `AGENTS.md` and `.claude/` directory.

**Competing interests.** **[AUTHOR: state funding and competing interests; Anthropic makes the
model that wrote much of the code.]**

**Acknowledgements.** **[AUTHOR: confirm the roles of P. V. Guimarães (LabECO, Universidade
Federal de Santa Catarina) and Pedro Henrique Cruz Caminha (GTA, Universidade Federal do Rio de
Janeiro), and whether either is a co-author.]**

**Availability.** Code, records and the build of this paper: <https://github.com/h0ffmann/ww3-gpu>,
archived on Zenodo under the concept DOI <https://doi.org/10.5281/zenodo.23221351>. WW3's
physics is used unmodified from NOAA-EMC; the shim and the caller patch live on the fork
`h0ffmann/WW3`. Licences: MIT, and LGPL-3.0-or-later for code
translated from WW3; this text is CC BY 4.0.

# Reproducing the numbers {.unnumbered}

Every number about this work, the file of the repository that records it, and the command that
reproduces it.

- **The profile of Section 2.1** (48.4, 23.2, 12.7, 5.6 and 3.1 % of self time; the 39 % of ST4
  is the sum of `W3SDS4`, `W3SIN4` and `W3SPR4`). Record: `docs/data/ww3_full_gprof.tsv`.
  Command: `just profile-case full`.
- **Table 1, rows 1 and 3, and the tests per entry.** Records:
  `docs/data/hoff_matrix_2026-10-15.tsv` and `docs/data/hoff_matrix_2026-10-14.tsv`. Command:
  recorded as `just hoff-matrix --jobs 8`, now `just ww3-gpu-matrix 8 serial`.
- **Table 1, row 2.** Record: `docs/data/hoff_matrix_omp_2026-10-15.tsv`. Command: recorded as
  `just hoff-matrix --omp --jobs 4` with `OMP_MAX_ACTIVE_LEVELS=2`, now
  `just ww3-gpu-matrix 4 omp`.
- **643 427 of 1 000 000, and the reading of the optimised GIMPLE.** Record:
  `docs/log/2026-10-14-snl1-bitwise-af11.md`, which gives the compiler flags and the program. The
  program itself was kept outside the repository, under the rule that the port has no tests of its
  own; committing it as a compiler experiment is a task before v1.
- **0 over 119 652 values; $9.879\times10^{-8}$ and $1.832\times10^{-7}$ before the fix.**
  Records: the same log entry and `kokkos/port_report/W3SNL1_KOKKOS.md`. Commands:
  `just smoke-ww3-gpu`, `just full-ww3-gpu`.
- **About three orders of magnitude** (the wrong explanation of Section 4.3). Records:
    - `docs/log/2026-10-14-hoff-matrix-entry16.md`
    - `docs/log/2026-10-15-hoff-matrix-entries51-54.md`
    - `docs/log/2026-10-15-hoff-matrix-entries6-10.md`
- **293, 10 and 2 routines; 99.3, 87.4 and 48.4 % of run time; 1 023 routines in WW3.** Records:
  `kokkos/PORT_STATUS.md`, `site/data/port_score.tsv`, `kokkos/ports.tsv`. Command:
  `python3 scripts/port_score.py --write`.

The `hoff-matrix` recipe was later renamed and rewritten as a CTest driver; that the rewrite
selects the same entries has not been checked, and the re-run of Section 7 removes the question.

# References {.unnumbered}

::: {#refs}
:::
