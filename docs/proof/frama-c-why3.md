# Frama-C with Why3

Checked 2026-10-01 by search only: Frama-C 32.0 is on opam, and its WP plugin's float model is
built on Why3's `ieee_float` theories (`Float32`, `Float64`) `(v, ocaml.org package listing and
Frama-C commits)`. Core licence LGPL-2.1 `(v)`. Not installed or run. Plan:
[`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §3 (d).

## What it is

A platform for C. Eva computes sound value ranges (floats included); WP turns ACSL contracts
(`requires`, `ensures`, loop invariants) into proof obligations for Why3, which dispatches them
to SMT solvers or to Rocq. C++ goes through the separate Frama-Clang plugin ⚠ (state not checked).

## Applied to ww3-gpu

Only if the lab keeps a literal C stage between Fortran and Kokkos, as the FESOM2 recipe and
`KOKKOS_H100_PLAN` §6 propose for `W3SRCE` (`course/13` `(v)`):

- Eva on the C reference of a source term, with input ranges taken from the fixtures: proves no
  NaN, no infinity and no overflow on those ranges, and prints the range of every output. For
  `W3SNL1` section 1, "for `KDMEAN` in `[0, 1e4]`, `CONS` is finite and within these bounds".
- WP with an `ensures \result == <the Fortran expression>` contract states the translation
  property at the C level, discharged through `ieee_float` by the same solvers as the SMT queries.

## Wave model and physics fit

C code with loops, globals and arrays, which is what a literal C stage looks like. Loop
invariants for spectral loops have to be written by hand, and libm calls are axiomatised by ACSL
specifications, not executed.

## Bit for bit

At best, C reference ↔ ACSL specification. No Fortran and no Kokkos; C++ only through
Frama-Clang. Eva's ranges are a robustness result (no NaN, no overflow), not an equivalence.

## Cost of the proof

ACSL and WP take weeks to learn; Eva can run in a day on a small C file. Contracts and
invariants rot when the code changes. Free. Fits an undergraduate timeline only if the C stage
exists, which the lab skipped for `W3SNL1` `(v, course/13)`.

## Pros

- Sound ranges with no annotations at all (Eva).
- Mature, documented, free; WP sits on the same IEEE theory the SMT solvers use.

## Cons

- C only in practice.
- Contracts and invariants for spectral loops are real proof engineering.
- Says nothing about the compiled binary's flags.

## Verdict

**Later**, and only for a literal C stage of `W3SRCE`, where Eva's NaN and overflow ranges pay
for themselves.
