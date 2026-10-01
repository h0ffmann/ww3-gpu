# Rocq (Coq) with Flocq, CompCert, VST and VCFloat2

Checked 2026-10-01: Flocq 4.2.x ships in current Fedora and openSUSE `(v, package indexes)`;
CompCert's source carries Flocq in `flocq/` and its `LICENSE` is the INRIA Non-Commercial License
with some files dual-licensed LGPL `(v)`; Rocq is LGPL-2.1 `(v)`; VCFloat2 was published at CPP
2024 by Appel and Kellison `(v, search)`. Nothing installed. Plan:
[`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §6.

## What it is

Rocq is an interactive proof assistant. Flocq is its formalisation of floating-point arithmetic,
IEEE-754 binary formats included, and is the reference library of the field. CompCert, a C
compiler proved correct in Rocq, uses Flocq for its floating-point semantics. VST proves C programs
correct against CompCert's Clight ⚠ (details not reread). VCFloat2 computes round-off error bounds
for floating-point expressions automatically, with certificates checked in Rocq `(v, search)`.

## Applied to ww3-gpu

- **Error bounds, not equality.** VCFloat2 or Gappa ⚠ on `EP1` and `SA1` of section 3: a proved
  bound on the float32 result against the exact real value. That bound is what would justify
  the 1e-5 L1 gate (`course/12` derives it informally `(v)`) and a CUDA tolerance, the one place
  where bit equality is impossible.
- **CompCert as the C reference compiler.** CompCert's correctness proof carries its source
  semantics to the assembly, so a literal C stage compiled with it computes exactly what Flocq's
  semantics say ⚠ (no contraction under its default flags, not checked). That pins one end of the
  ladder formally.
- Bit-for-bit proofs between gfortran, g++ and nvcc are out of reach: none of them is modelled.

## Wave model and physics fit

Anything, as a model written by hand in Rocq: spectra as functions, sums as folds, `exp` as an
axiomatised function. None of it is the WW3 code.

## Bit for bit

Only for C compiled by CompCert, through VST. Not Fortran, not C++, not CUDA.

## Cost of the proof

Months to become productive in Rocq; VST proofs of a numeric kernel are research projects.
VCFloat2 on one expression is the realistic slice, at one to two weeks for someone who already
knows Rocq ⚠. CompCert is free for research and teaching and licensed commercially otherwise
`(v, LICENSE)`.

## Pros

- The most complete and trusted IEEE-754 theory available.
- The only route from a proof to real compiled C.
- Error-bound tools that answer "how big a tolerance is honest".

## Cons

- Does not touch Fortran, C++ or CUDA.
- Expert-level cost for any end-to-end result.
- CompCert's licence is not free for commercial use.

## Verdict

**No** for equivalence; **later** for a VCFloat2 bound that justifies the CPU ↔ CUDA tolerance.
