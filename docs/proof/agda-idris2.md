# Agda and Idris 2

Checked 2026-10-01: Agda's `Agda/Builtin/Float.agda` (master) declares `postulate Float : Set`
with primitives such as `primFloatPlus`, `primFloatExp`, `primFloatEquality` and
`primFloatToWord64`, and no 32-bit float type `(v)`. Agda is MIT-style, Idris 2 BSD-style `(v,
LICENSE files)`. Idris 2's numeric types were not opened ⚠ (a primitive `Double` and no
single-precision type, from memory). Plan: [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §6.

## What they are

Dependently typed, total functional languages. Agda is mostly a proof assistant; Idris 2 is a
programming language with dependent types and compiles to Chez Scheme and C ⚠. In both, floats
are primitive: operations compute on closed terms, but there is nothing to prove a general
property with.

## Applied to ww3-gpu

- A reference semantics of `W3SNL1` sections 1–4 as one total function from `(Config, Tables,
  A, CG, KDMEAN)` to `(S, D)`, with indices typed as `Fin (nspecy + nth)` so every table read is
  in bounds by construction. That is the property `L1_test_snl1_tables` checks for one grid
  `(v)`, proved for all grids the types admit.
- Run that function on the committed fixture as one more differential arm. It computes in
  binary64 against WW3's binary32, so it would disagree by design; useful only as an
  executable specification.

## Wave model and physics fit

A hand-written model. Pure functions fit the per-point source terms well; module globals become
explicit arguments, which the C++ port already does (`Config`, `Tables`).

## Bit for bit

No. Floats are postulated; Agda has no binary32 at all.

## Cost of the proof

Weeks to learn either language for an engineering student. The index-safety proofs are
tractable; anything numeric is not. Free.

## Pros

- Index and shape safety as a type-checking result.
- A readable, executable specification of a kernel.

## Cons

- Nothing about floating-point values.
- No path to the Fortran or C++ code; a second implementation to maintain.

## Verdict

**No.**
