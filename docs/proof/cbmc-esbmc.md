# CBMC and ESBMC: bounded model checking of C and C++

Checked 2026-10-01 against each project's `README.md` and, for CBMC,
`src/ansi-c/library/math.c` (develop). Neither was run. Plan:
[`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §3 (d).

## What it is

Both read real source code, unroll loops to a bound, and turn "this assertion can fail" into a
SAT/SMT query. CBMC handles C and C++ `(v, README)`. ESBMC handles C, C++ and CUDA through clang,
plus Python, Rust, Solidity and others, and "supports IEEE floating-point arithmetic for various
SMT solvers": Z3 4.13+, Bitwuzla, CVC5 `(v, README)`. CBMC is BSD-style; ESBMC's licensing is
"complex", with some bundled solvers carrying non-commercial clauses `(v, COPYING)`.

## Applied to ww3-gpu

An equivalence harness, compiled by the checker instead of by g++:

```c
float kd = nondet_float();
__ESBMC_assume(isfinite(kd) && kd >= 0.0f);       /* __CPROVER_assume for CBMC */
assert(bits(cons_ref_c(kd, P)) == bits(cons_port(kd, P)));
```

- `W3SNL1` section 1 (`proof/snl1_cons/cons_port.cpp` against a C transliteration of
  `cons_ref.F90`), then one iteration of the section 3 and 4 loop bodies with the table reads
  as nondeterministic inputs.
- ESBMC's CUDA front end could check the kernel body as CUDA source ⚠ (not tried).
- Neither reads Fortran. The harness proves C ↔ C++; the Fortran ↔ C step needs the GIMPLE diff
  or the exhaustive sweep.

## Wave model and physics fit

Small, loop-free or short-loop kernels. Section 3 iterates 720 times over gathered table reads
`(v, BEND_TRYOUT §3.2)`; unrolling that with symbolic floats is far beyond what bit-precise FP
solving handles ⚠, so the unit is one iteration. `W3SRCE`'s sub-stepping loop with a data-dependent
trip count is out of reach. Module state becomes globals the harness sets.

## Bit for bit

Bit-precise for `+ − × ÷` and conversions. Transcendentals are models: CBMC's `expf` is
Schraudolph's approximation, not glibc's, and its `sqrtf` is constrained to the correctly
rounded root `(v, math.c)`. A proof through `expf` is a proof about CBMC's `expf`. Keep the call
abstract (same function, same argument) instead. The checker's view of contraction is the C
semantics; whether g++ contracts is outside it ⚠.

## Cost of the proof

Install from a release (both ship binaries ⚠). A first harness on section 1: 3–5 days including
learning. Maintenance: one harness per kernel, updated when signatures change. Runtime per check:
seconds to hours, depending on solver and bound ⚠.

## Pros

- Reads the actual C++, so no hand-written query can drift from the code.
- Counterexamples come out as concrete inputs.
- ESBMC drives Bitwuzla, the fastest solver on the pilot queries, and knows CUDA.

## Cons

- No Fortran front end.
- Loop bounds and FP make queries explode quickly.
- Library models (CBMC's `expf`) silently replace the real libm.

## Verdict

**Later.** ESBMC first, on pilot section 1, once steps 1–3 of the pilot are done.
