# `docs/proof/`: one page per proof option

The evidence behind [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md), the plan for
proving the Fortran → C → C++/Kokkos port bit for bit. Each page has the same parts: what the tool
is and the version checked, where it applies in this repo, how it fits the wave model's physics,
how far it reaches toward a bit-for-bit claim, what the proof costs, pros, cons and a verdict.
`(v)` and `⚠` as everywhere in the repo; all pages checked 2026-10-01.

The full side-by-side comparison, with every criterion, is
[`BITWISE_PROOF_202610.md` §9](../BITWISE_PROOF_202610.md#9-all-proof-options-side-by-side).
This index is the short form.

| Option | Bit-for-bit reach | Physics fit | Cost | Verdict |
|---|---|---|---|---|
| [Exhaustive + property testing](testing-exhaustive-property.md) | proof for one-input functions, per build | real code | hours per function | **now** |
| [GIMPLE diff](gimple-diff.md) | Fortran ↔ C++ under GCC, structural | real code | 0.5 d script | **now** |
| [SMT: Bitwuzla, cvc5, Z3](smt-z3-bitwuzla-cvc5.md) | ∀ inputs of a written expression | loop bodies, no `exp` | 1–3 d per body | **now** |
| [CBMC, ESBMC](cbmc-esbmc.md) | C ↔ C++ (ESBMC: + CUDA), bounded | small kernels | ~1 wk | **later** |
| [Alive2 (+ flang)](alive2.md) | flang ↔ clang IR | function-local | 1–2 wk | **later** |
| [KLEE-Float](klee-float.md) | none (LLVM 3.4) | — | — | **no** |
| [Frama-C + Why3](frama-c-why3.md) | C stage ↔ contract | C only | weeks | **later** |
| [Rocq + Flocq, CompCert, VST, VCFloat2](rocq-flocq.md) | C via CompCert; error bounds | hand model | months | **no** (bounds: later) |
| [Isabelle/HOL](isabelle-hol.md) | model only | hand model | months | **no** |
| [Lean 4 + FloatSpec](lean4.md) | model only | hand model | months | **no** (watch) |
| [Agda, Idris 2](agda-idris2.md) | none (postulated floats) | hand model | weeks | **no** |
| [Liquid Haskell, QuickCheck](haskell-liquid-quickcheck.md) | none (reals) / testing | hand model | weeks | **no** |
| [TLA+](tla-plus.md) | operation order, not values | protocols | 1–2 wk | **later** |

Dropped: F\*, whose verified-code tooling (HACL\*, Low\*) is integer and cryptographic, with no
floating-point story ⚠. Its row would repeat Isabelle's.

Runnable pieces live outside `docs/`: `proof/snl1_cons/` (the exhaustive pilot) and `proof/smt/`
(two worked SMT queries).
