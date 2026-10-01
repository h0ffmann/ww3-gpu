# Bend 2

Checked 2026-10-01 against `bendlang/bend` `main`: `WONTFIX.txt`, `bend2/main.ts`,
`bend2/base.bend` `(v)`; everything else from [`../BEND_TRYOUT_202609.md`](../BEND_TRYOUT_202609.md)
(2026-09-18). Apache-2.0. Plan: [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §5.

## What it is

A pure, affine, dependently typed language whose compiler emits one C file for the CPU and the
GPU, parallel by binary fork-join, with laws checked by its own checker. Numbers are `Nat`, `U32`
and `F32`; `base.bend` still has no `F64` and no signed integer `(v, grep)`.

## What changed since BEND_TRYOUT

`WONTFIX.txt` now has a SOON section ("we will add it; do not open an issue") listing F64 (#1120:
"It needs U64's 64-bit word design; we add both together, and we do not merge PRs for F64"), a
native library target (#813: "planned, not scheduled"), and "F32 that computes in the checker"
(#1017: "F32 operations are axioms today; bit-level definitions are planned") `(v)`. On
18 September `BEND_TRYOUT` read #813 as refused and F64 as having no roadmap. None has shipped.

## Applied to ww3-gpu

- F32 kernels are not the problem: the scan of WW3's `model/src` finds no F64 in ST4, the DIA,
  `W3SLN1`, `W3SBT1`, propagation (`w3pro3md`, `w3uqckmd`), dispersion, output integrals or
  partitioning, and F64 in DB1, `W3SNLGQM`, grid coordinates, time, tides, unstructured grids and
  ice terms (`BITWISE_PROOF` §5 `(v)`).
- What Bend can *prove* is integer: "every pre-shifted DIA address for this grid lies in
  `[0, 1024)`", "`ISP` is a bijection for NK=25, NTH=24", "the card-deck map round-trips for
  these NSEA and NAPROC". Its checker decides such closed claims by evaluation. `U32` is unsigned,
  so WW3's negative DIA addresses are shifted first `(v, BEND_TRYOUT §3.1)`.
- The DIA itself as a Bend program is the tryout's experiment; it is gated by the L1 tolerance,
  never by bit equality (`BEND_TRYOUT` §4).

## Wave model and physics fit

Per-point F32 kernels type-check; arrays cannot be shared between parallel branches without an
O(n) clone, there is no C ABI to call from WW3, and module state has to become arguments
(`BEND_TRYOUT` §3, §5). `exp` lowers to `(float)exp((double)x)`.

## Bit for bit

No proof is available: `F32` operations are axioms in the checker (#1017). Measured differences
from WW3's arithmetic: the `exp` entry point changes 3 350 physical `CONS` values by one ULP
`(v, proof/snl1_cons, bend-exp)`. Bend's CPU build line is `clang -std=c11 -O3` `(v, main.ts)`; clang 18
emits no FMA for `EP1` on baseline x86-64 and three on `-march=x86-64-v3` or aarch64 `(v,
measured)`, with no flag exposed in Bend to turn it off.

## Cost of the proof

The tryout week as planned. Integer laws: a day inside that week (`BEND_TRYOUT` step 5).
Maintenance: two releases on 18 September alone `(v, BEND_TRYOUT)`, so pin and expect churn. Free.

## Pros

- Purity and affinity give race freedom and termination by construction.
- Integer laws over tables are cheap to state and check.

## Cons

- Nothing provable about floats today; no FP control knobs.
- The same integer claims are exhaustive C++ tests in an afternoon.
- No library target yet, so no path into WW3.

## Verdict

**No** for proofs; the tryout stays a tryout. Revisit when #1017 (bit-level F32 in the checker)
ships, because that is the first change that would let Bend prove something about this
arithmetic.
