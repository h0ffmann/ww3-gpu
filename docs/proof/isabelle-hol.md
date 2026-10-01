# Isabelle/HOL

Checked 2026-10-01 by search: the Archive of Formal Proofs entry `IEEE_Floating_Point` (Lei Yu)
formalises IEEE-754 arithmetic, fused multiply-add included, with code generation, and its
definitions are ported from HOL Light `(v, AFP entry pages via search)`. Licences ⚠ (Isabelle and
AFP entries are BSD-style, not reread). Plan: [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §6.

## What it is

An interactive theorem prover with strong automation (Sledgehammer calls external provers) and a
large library. The AFP float entry gives an IEEE-754 model to prove theorems about.

## Applied to ww3-gpu

The same role as Rocq + Flocq, with a smaller numeric ecosystem: prove that a hand-written model
of `EP1` with FMA differs from the one without, or bound its error. Isabelle's C verification
stack (the C-parser and AutoCorres used for seL4) targets integer systems code and does not model
floating point ⚠.

## Wave model and physics fit

Hand-written model only.

## Bit for bit

On the model. Nothing about gfortran, g++ or nvcc output.

## Cost of the proof

Months to learn; Isar proofs of float lemmas are slow going. Free.

## Pros

- Strong automation for the parts of a proof that are not about floats.
- An IEEE model with FMA and executable code.

## Cons

- No path to Fortran, C++ or CUDA code.
- Smaller floating-point community than Rocq's.

## Verdict

**No.** Rocq covers the same ground with more numeric tooling.
