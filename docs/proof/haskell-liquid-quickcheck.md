# Haskell: Liquid Haskell and QuickCheck

Checked 2026-10-01: Liquid Haskell encodes Haskell's `Int` and `Double` as SMT unbounded
integers and reals `(v, Vazou et al., "Refinement types for Haskell" tech report, via search)`.
Liquid Haskell is BSD-style `(v, LICENSE)`; QuickCheck's licence not reread ⚠. Plan:
[`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §6.

## What they are

Liquid Haskell adds refinement types (`{v:Int | 0 <= v && v < n}`) checked by an SMT solver.
QuickCheck generates random inputs for stated properties and shrinks a failing input to a small
one.

## Applied to ww3-gpu

- A Haskell model of the DIA with refined index types would prove table reads in range, like
  Agda's `Fin`, with more automation.
- QuickCheck properties on that model, or on the C++ through the FFI ⚠: zero spectrum → zero
  source, `A×2 → S×8`, and edge-case generators for ±0, denormals and extreme magnitudes.

## Wave model and physics fit

A hand-written model. Haskell has both `Float` and `Double`, so a binary32 model is possible;
GHC computes them with the hardware.

## Bit for bit

Liquid Haskell: no. A refinement over `Double` is a statement about real numbers, so a "proof"
that a rewrite is equal is unsound for IEEE values, and exactly wrong for the FMA and signed-zero
cases. QuickCheck: testing only, never a proof.

## Cost of the proof

Weeks to learn Haskell and Liquid Haskell. A QuickCheck harness: days. Free.

## Pros

- QuickCheck's shrinking turns a failing spectrum into a minimal one.
- Liquid Haskell automates index-bound proofs well.

## Cons

- Real-number semantics make Liquid Haskell's float proofs misleading here.
- A Haskell model is another implementation to keep in step.
- GoogleTest already gives property tests in the port's own language.

## Relation to Bend

Bend is pure and functional like Haskell; the same reasoning style applies, and the same gap
between reals and floats.

## Verdict

**No.** Borrow the idea instead: edge-case generators and input shrinking in GoogleTest.
