# ADR-0016: The goal is WW3's run time on Kokkos and CUDA; WW3's regtests move to a later validation phase; one person works in a short compound loop

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-17 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | What the repository optimises for, when WW3's regtests run, how a speed figure is reported before they run, and how engineering work is planned, done, reviewed and remembered (`AGENTS.md`, `CONTRIBUTING.md`, `.compound-engineering/config.yaml`, `.claude/settings.json`). Not WW3 read-only, not "translate, don't improve", not bit for bit as the target, not the GPU supervision rule, not the proposal's reviewer, not the CI gates |
| **Related** | Supersedes [ADR-0011](ADR-0011-port-in-units-regtest-at-checkpoints.md) (regtests at checkpoints); keeps [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md) (WW3's regtests are the port's only tests) and [ADR-0002](ADR-0002-port-order-wall-time.md) (port order by wall time); agrees with [ADR-0012](ADR-0012-translate-and-wire-now-regtest-matrix-on-the-h100.md) (Proposed: the matrix on the H100); [ADR-0013](ADR-0013-operational-parts-default-to-kokkos.md) and [ADR-0014](ADR-0014-gpu-runs-judged-by-tolerance.md) unchanged |

## Context

The owner, 2026-10-17, verbatim: "the main goal of the repository is to speed up WW3 runtime with
Kokkos e CUDA. All the enginering framework should be adopted with this in mind. Regtests
postponed, will validate if the translation from Fortran to C++ was accurate. It's a 1 person
project so it needs eng. pattern with agility." The same day: "adopt compound-engineering-plugin,
and make sure AGENTS will follow its pattern when guided by me to do eng. work".

ADR-0011 ran WW3's regtests at every checkpoint: a finished unit, a pin or toolchain move, before
L2, before a timing, before a release. AGENTS.md also refused any speed figure without a passed
regtest replay on the same commit. With one person, a busy shared workstation and the CUDA
matrix gated on the owner's presence, those gates set the pace of the whole project: the same
`--ww3` run took 22.6 s to 361.7 s on this machine during the W3OUTG work ⚠ (a timing survey in
that session, no record in `bench/results/`).

Compound Engineering (EveryInc, MIT) is a Claude Code plugin of 36 skills built around one loop:
brainstorm, plan, work, simplify, review, compound (v, its `README.md` at
`compound-engineering-v3.30.3`, read 2026-10-17). Its last step writes what a solved problem
taught into a folder that the next plan reads.

## Decision

1. **The goal.** The repository exists to make WW3 run faster with Kokkos and CUDA, without
   changing the Fortran's arithmetic. A change is judged first by the wall time it removes from a
   WW3 run, and the port order stays the measured profile's (ADR-0002).
2. **Regtests postponed.** WW3's regtests (`just smoke-ww3-gpu`, `just full-ww3-gpu`,
   `just ww3-gpu-matrix`) no longer run at each checkpoint. They run in a validation phase the
   owner calls, and that phase answers one question: was the translation from Fortran to C++
   accurate, bit for bit on the CPU (ADR-0010), within ADR-0014's tolerances on a GPU. Until then a
   row of `kokkos/ports.tsv` keeps `regtested` as "no: postponed (ADR-0016)", and nothing becomes
   default-on (ADR-0013 still asks for its checkpoint).
3. **Between now and that phase,** translation accuracy is argued statically, as ADR-0011 already
   allowed between checkpoints: the `-O3` GIMPLE or disassembly of the Fortran and the C++, and,
   for device math, `libm_bits` against glibc. CI's gates and `health.yml` keep running.
4. **Speed figures before the regtests** may be measured and reported in a pull request, a port
   report, a log entry or a plan, each marked "regtest pending ⚠" with machine, command and the
   median of three runs. The tables generated from `bench/results/` keep their gate
   (`scripts/results.py check` refuses a timing without `regtest: passed`); changing that gate is a
   person's review, as every gate is.
5. **The loop.** Engineering work the owner asks for follows Compound Engineering's loop, sized to
   the change: small changes skip the plan, medium ones plan and review, large ones brainstorm
   first. The plugin is enabled in `.claude/settings.json` pinned to `compound-engineering-v3.30.3`;
   its artifacts live under `docs/compound/` (`docs_root` in `.compound-engineering/config.yaml`).
   Where a step clashes with a hard rule, the rule wins and `AGENTS.md`, "Engineering work", says
   how the step is redirected.
6. **Tools that serve the goal**, adopted in the same change: Context7 (Upstash, MIT) as a plugin,
   for current Kokkos, CUDA and CMake documentation; Archify (tt-a1i, MIT) as a pinned submodule,
   not a copy, for interactive twins of the repository's Mermaid figures.

## Alternatives considered

- **Keep ADR-0011's checkpoints.** Safest for the bit-for-bit claim; rejected by the owner because
  the regtest schedule, not the translation, set the pace.
- **Drop the regtests.** Rejected: they are the only evidence the translation is right (ADR-0010),
  and the speed goal is worthless if the answer changes.
- **A heavier process (a WFIP for every change, an issue before any work).** Built for several
  contributors; for one person it costs more than the review it buys. WFIPs stay for changes that
  move a proposal deliverable.
- **No framework, ad hoc sessions.** What each session learned stayed in that session; the
  compound step is the part that fixes this.

## Consequences

- A speed figure can appear before its parity is known; the "regtest pending ⚠" mark is what keeps
  it honest, and a later regtest failure invalidates every figure measured on that code.
- The validation phase carries more risk at once: a translation error found there may sit under
  several units of work. Static parity on each routine is what bounds it.
- Agents follow one named loop, so the owner can say "plan it", "work it", "review it", "compound
  it" and get the same steps every session.
- Two plugins and one vendored skill more to keep pinned; Context7 sends queries to Upstash's
  server, so a query never carries a secret or unpublished text.

## Revisit when

- The owner calls the validation phase and it reports: then decide whether regtests return to
  checkpoints.
- A regtest finds a translation error that static parity missed.
- A second regular contributor joins.
