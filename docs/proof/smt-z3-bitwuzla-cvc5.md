# SMT solvers over IEEE-754: Bitwuzla, cvc5, Z3

Checked 2026-10-01: Bitwuzla 0.9.1, cvc5 1.4.1, Z3 5.1.0 (pip packages), 4 cores. Licences:
Bitwuzla MIT, Z3 MIT, cvc5 BSD with optional LGPL/GPL parts `(v, licence files)`. Plan:
[`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §3 (c).

## What it is

Solvers for SMT-LIB's floating-point theory (`QF_FP`): binary32 and binary64 values with
rounding modes, ±0, infinities, NaN, `fp.fma` and `fp.sqrt`, decided bit-precisely. Two
expressions are equal for every input exactly when "they differ" is `unsat`; a `sat` answer
comes with the input that shows the difference.

## Applied to ww3-gpu

- `proof/smt/ep1_contraction.smt2`: `EP1` of `W3SNL1` section 3.a in Fortran order against the
  three-FMA form g++ emits without `-ffp-contract=off`, inputs restricted to physical ranges.
  `sat`, one ULP apart: the mechanism behind the 1.1e-5 drift in `PORT_STATUS.md`, as a concrete
  input `(v, measured)`.
- `proof/smt/s4_distribute.smt2`: the leading term of section 4, `-2.*(SA1+SA2)`. Commuting is
  `unsat` (exact); distributing is `sat` through overflow and, with overflow excluded, through
  `-0` against `+0`; with both excluded it is `unsat` `(v, measured)`.
- Next: one query per output of section 3 (`SA1`…`DA2M`) and section 4 (`S`, `D`), table reads
  as free variables, Fortran expression against C++ expression. Then every optimisation PR of
  phase 2 (a hoisted product, a reordered sum) is a query before it is a test.

## Wave model and physics fit

Loop bodies only, written out as expressions by hand or by a tool. No `exp`, `log`, `pow` or
`tanh`: they stay uninterpreted functions, which proves equality only when both sides call the
same function on equal arguments. No loops, no module state.

## Bit for bit

Exactly the IEEE operations written in the query, for every input. Says nothing about whether a
compiler emitted those operations. That link comes from the GIMPLE diff or from Alive2.

## Cost of the proof

Learning SMT-LIB's FP syntax: a day. A query per loop body: an hour or two. Measured times:
`ep1_contraction` Bitwuzla 0.4–1.6 s, cvc5 0.5 s, Z3 31 s; `s4_distribute` (four queries)
Bitwuzla 23 s, cvc5 79 s, Z3 did not prove the last `unsat` in 900 s `(v)`. Use Bitwuzla or
cvc5 for `unsat`. Maintenance: the queries are rewritten when the kernel changes. Writing one
query per body by hand does not scale to `W3SRCE`; CBMC or ESBMC generate them from C++.

## Pros

- A real proof over every input, with counterexamples when it fails.
- Finds the cases a relative tolerance hides (signed zero, overflow in an intermediate).
- Free, scriptable, small install.

## Cons

- The query is a hand-written model: it can drift from the code.
- No transcendental functions.
- `unsat` on wide expressions can be slow; solver choice matters by two orders of magnitude.

## Verdict

**Now** for the DIA's loop bodies and for every rewrite; at `W3SRCE` scale, through ESBMC.
