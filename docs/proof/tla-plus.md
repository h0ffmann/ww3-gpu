# TLA+

Checked 2026-10-01: the TLA+ tools (`tlaplus/tlaplus`) are MIT-licensed `(v, LICENSE)`. TLC,
the explicit-state model checker, works over integers, sets, sequences and records and has no
floating-point values ⚠ (from the TLA+ documentation, not reread). Plan:
[`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §6.

## What it is

A specification language for concurrent and distributed systems. A spec describes states and
allowed steps; TLC checks invariants and liveness over every interleaving of a finite instance.

## Applied to ww3-gpu

- **The card-deck distribution and the transposes.** WW3 assigns sea points to ranks
  (`INIT_GET_ISEA`, `INIT_GET_JSEA_ISPROC`, `w3parall.F90:1152-1504` `(v)`) and `W3GATH`/`W3SCAT`
  move spectra between the spectral and the spatial decomposition. Invariant: every `(ISEA,
  ISPEC)` has exactly one owner at every step, and gather then scatter is the identity.
- **A deterministic reduction schedule.** Model the partial sums of a `team_reduce` as *terms*
  (who was added to whom, in which order) instead of numbers. Then "Serial, OpenMP and CUDA
  perform the same additions in the same order" is an invariant TLC can check over thread
  interleavings and team sizes. If it holds, bit equality across backends follows from IEEE
  determinism, which is the argument `WW_DETERMINISTIC` relies on.
- **Phase 3 halo exchange** (`AGENTS_KOKKOS` §3.4): no deadlock, and no kernel reads a halo
  before the exchange that fills it completes.

## Wave model and physics fit

None for the arithmetic. It fits the parts of the port that are protocols: ownership, ordering,
synchronisation.

## Bit for bit

Indirectly: it can prove that an *order of operations* is fixed, which is the precondition for
bit equality across backends with reductions. It never sees a float.

## Cost of the proof

One to two weeks to learn the language and model one protocol; TLC runs in minutes on small
instances. Specs need updating when the protocol changes, not when the physics does. Free.

## Pros

- The right tool for the reduction-order and halo questions that testing covers badly.
- Small, readable specs; counterexamples are step-by-step traces.

## Cons

- Proves the model, not the MPI or Kokkos code.
- Nothing about values.

## Verdict

**Later**: for phase 3 halos, and when the first kernel with a reduction (`W3SRCE`, ST4
integrals) needs a deterministic schedule argued rather than asserted.
