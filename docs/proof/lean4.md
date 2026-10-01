# Lean 4 (with FloatSpec)

Checked 2026-10-01 against lean4 `master` (`src/Init/Data/Float/Float.lean`,
`Model/Float.lean`) and FloatSpec's `README.md` (Lean and Mathlib `v4.34.0`, Apache-2.0)
`(v)`. Other 2026 Lean float libraries turned up by search (an "FP" error-analysis library on
Zenodo, "FloatLib" on arXiv) were not opened ⚠. Plan: [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §6.

## What it is

A dependently typed language and proof assistant with Mathlib. Lean core now gives `Float`
(binary64) and `Float32` (binary32) a logical model (files copyright 2026 `(v)`; first release
carrying it not checked ⚠): `Float.add` is defined as
`.ofModel (a.toModel + b.toModel)` over a bit-level `Float.Model` in which every NaN is one
canonical NaN, and "is compiled to the C addition operator" `(v, Float.lean)`. `Float.exp` is still
`@[extern "exp"] opaque` `(v)`: no model. FloatSpec ports Flocq to Lean and proves that the native
`+ − × ÷` and `sqrt` on `Float` and `Float32` are correctly rounded, with no named `sorry` left
`(v, README)`.

## Applied to ww3-gpu

- Write `W3SNL1` section 3.b as a Lean function over `Float32` and prove, with FloatSpec's
  rounding lemmas, an error bound for `SA1` against exact reals, the same job as VCFloat2 in Rocq.
- Prove rewrites equal or unequal on the model, as the SMT queries do, but by hand.
- Section 1 cannot be evaluated in the kernel: `exp` is opaque.

## Wave model and physics fit

A hand-written model, typed precisely (`Fin nspec` indices make the DIA's table reads provably
in range). `exp`, `tanh`, `pow` are opaque. Module state is whatever structure is written.

## Bit for bit

On the model, `+ − × ÷ sqrt` are bit-exact by definition. The executable link is the reverse of
what the port needs: Lean compiles its own floats to C, so Lean's runtime results depend on the C
compiler's flags too ⚠ (Lean's C build flags not checked). Nothing about gfortran, g++ or nvcc.

## Cost of the proof

Months to be productive with Mathlib-style proofs. A first FloatSpec error bound on a four-term
dot product: weeks ⚠. The libraries move fast (FloatSpec pins a toolchain per release), so
proofs need upkeep. Free.

## Pros

- The newest IEEE model, in core, for both widths the port uses.
- Dependent types make index safety a type-checking matter.
- Active community, and a direct counterpart of Flocq through FloatSpec.

## Cons

- No reading of Fortran or C++.
- Transcendentals opaque.
- Young libraries; toolchain churn.

## Verdict

**No** for this project; watch it. Revisit if a verified Lean → C++ path or an `exp` model appears.
