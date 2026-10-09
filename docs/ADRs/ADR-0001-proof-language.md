# ADR-0001: No proof language for the GPU port; SMT queries and exhaustive sweeps instead

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-08 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | Every route that moves WW3 work off the Fortran CPU path: the C++/Kokkos port, the Triton arms of `W3SDS4`, and the WeatherNext 3 wind forcing |
| **Related** | [`BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) (the evidence), [`docs/proof/`](../proof/README.md) (one page per tool), [#45](https://github.com/h0ffmann/ww3-gpu/issues/45) and [`W3SDS4_TRITON_PLANO_202610.pt.md`](../W3SDS4_TRITON_PLANO_202610.pt.md) (Triton), [WFIP-0001](../WFIPs/WFIP-0001-weathernext3-wind-rtx4090.md) (WeatherNext 3), #58 (Bend removed) |

## Context

The lab's claim is that WW3's expensive kernels can move to a GPU "without changing the answer".
The project proposal states that claim in two strengths. Options that leave the arithmetic alone
must reproduce the reference "bit a bit, arquivo a arquivo", and the Kokkos Serial backend must
reproduce the C reference bit for bit `(v, pubs/proposal/pt/07-methodology.md:14, :16)`.
Everything else is judged by tolerances agreed with the laboratory
`(v, pubs/proposal/pt/06-objective.md:4)`.

Today the strong claim rests on tests. `W3SNL1` is bit-identical to the Fortran on the Serial,
OpenMP and CUDA presets on the three fixture points `(v, kokkos/PORT_STATUS.md:13)`. Section 1 of
that kernel was also swept over all 2^32 float32 inputs with 0 differences at the parity flags
(`bash proof/snl1_cons/run.sh`, `BITWISE_PROOF` §4). The question is whether to go further and
adopt a proof language (Rocq, Lean 4, Isabelle, Agda, Idris 2, Liquid Haskell, TLA+, F\*, or the
since-removed Bend) as part of the port's workflow.

Four facts shape the answer:

1. **No proof assistant reads Fortran or C++.** Each one proves theorems about a model written by
   hand. Only Rocq reaches real C code, through CompCert's Clight and VST, and it does not reach
   C++ or Fortran `(v, BITWISE_PROOF §6)`. A proof about a hand-written model says nothing about
   the gfortran and nvcc binaries the lab runs unless someone shows that the model and the code
   agree, and that link is exactly what is in question.
2. **Bit equality is a property of two binaries, not of two sources.** The same three Fortran
   lines give different bits under `-O3` and under `-O3 -march=x86-64-v3` (FMA). A Kokkos build
   without `-ffp-contract=off` differs on 4 632 147 inputs of section 1 `(v, BITWISE_PROOF §4)`.
   A useful proof has to be tied to the flags it was taken with.
3. **IEEE-754 arithmetic is decidable for straight-line code.** SMT-LIB's `QF_FP` theory models
   binary32 and binary64 exactly, with rounding modes, ±0, NaN and FMA. Bitwuzla and cvc5 answered
   both pilot queries in 0.4–79 s, while Z3 did not finish one `unsat` in 15 min
   `(v, BITWISE_PROOF §3(c))`. For a loop body, equality "for every input" is therefore a solver
   query, not a proof someone has to write.
4. **A proof language multiplies what agents write and people review.** In this lab an agent
   writes most of the code and a person reviews all of it. A proof assistant adds a second artefact
   per routine: the hand-written model, its lemmas and the proof scripts, which have to be kept in
   step with the C++ and re-checked when it changes. Each of those is more tokens generated and
   more lines a reviewer must read, and the reviewer must also check that the model says what the
   code does, which Appendix A shows a checker cannot do. `BITWISE_PROOF` rates interactive proofs
   at months per routine against 1–3 days per loop body for an SMT query
   `(v, BITWISE_PROOF §7)`. The token and review cost of either route has not
   been measured ⚠; the claim is that it scales with the size of the formal artefact, which is a
   solver query for SMT and a whole theory for a proof assistant.

## Decision

**No proof assistant or dependently typed language is adopted.** Bit-for-bit claims are
established by three mechanised checks that read either the binaries or the operation DAG:

1. **Exhaustive sweeps.** Every function of one float32 input and every integer table (DIA
   addresses, `ISP` maps, `MAPSTA`) is swept over all of its inputs, comparing the Fortran build
   against the port build at the parity flags.
2. **GIMPLE diff.** gfortran and g++ share GCC's middle end, so the optimised IR of each routine is
   normalised and diffed. This shows the same floating-point operations in the same order.
3. **SMT-LIB `QF_FP` queries, decided by Bitwuzla or cvc5.** Each loop body is written as the
   Fortran DAG against the C++ DAG. Every rewrite an agent proposes gets one query too, and the
   answer must be `unsat` before the rewrite is merged.

SMT-LIB is the only formal language adopted. It is used as a query format checked by a decision
procedure, so nobody writes or maintains a proof script. Three tools stay on the bench for later
and are not adopted now:
- ESBMC builds the SMT queries from the C++ and CUDA sources itself.
- TLA+ can specify the phase-3 halo exchange and a fixed reduction schedule, which are protocol
  questions rather than arithmetic.
- VCFloat2, which runs on Rocq, can prove a round-off bound when a tolerance has to be defended in
  a paper.

## The decision per scenario

| Scenario | What "the same answer" means here | Bit-for-bit reachable? | What establishes it | Proof language? |
|---|---|---|---|---|
| **Kokkos/C++ on CPU** (Serial, OpenMP; `W3SNL1` now, `W3SDS4` next) | identical bits to gfortran at WW3's GNU flags | **Yes, provably**, for kernels without a reduction, with contraction off on both sides and the same libm entry points `(v, BITWISE_PROOF §2)` | sweeps, GIMPLE diff, SMT per loop body; L1 and L2 tests stay as the regression net | No |
| **Kokkos with reductions** (`W3SRCE`, ST4 integrals by `team_reduce`) | a fixed-order validation build equal to the Fortran; production within tolerance | **No** against a parallel reduction, by construction. Float addition is not associative. | `WW_DETERMINISTIC=ON` for validation (fixed order, then provable as above); `nccmp-tol` per field for production | No; TLA+ later for the reduction schedule, if phase 3 needs one |
| **Kokkos on CUDA** (RTX 4090, H100) | as on CPU for `+ − × ÷ sqrt` with `--fmad=false`; tolerance once a transcendental appears | **Partly.** CUDA's `expf` is another implementation (about 2 ULP ⚠), so only the routines with no transcendental are provable. | an exhaustive count of where device and glibc libm disagree on each routine's real input domain, then `nccmp-tol` | No; a VCFloat2 bound only if a reviewer asks why the tolerance is what it is |
| **Triton `W3SDS4`** (GPU, and `triton-cpu` as a parity twin) | the L1 tolerance gate, as for Kokkos | **Improbable** `(v, W3SDS4_TRITON_PLANO, "Resumo", item 4)`: `libdevice` transcendentals, FMA fusion by default, reduction order, and TF32 in `tl.dot` | the plan's own gates: `enable_fp_fusion=False`, `input_precision="ieee"`, ULP tests of `exp`/`log`/`tanh`/`atan2` against glibc, L1 and L2. SMT can still prove that a kernel's written DAG equals the Fortran's for the arithmetic-only parts. Triton lowers through MLIR and LLVM, so there is no GIMPLE diff; Alive2 on the LLVM IR is the only IR-level option ⚠ (not tried). | No |
| **WeatherNext 3 wind forcing** (WFIP-0001) | not the same answer: a different wind field gives a different `Hs` by design | **Not applicable** | a statistical comparison: bias and RMSE of `Hs` against PNBOIA buoys or altimeter tracks, against the GFS/IFS-driven run `(v, WFIP-0001 §7)`. The forcing pipeline (regrid, units, time interpolation) is checked by tests on known fields. | No. A proof cannot say whether one wind model forecasts better than another. |

Across the five rows, a proof language would only add something for theorems about a model of the
physics (energy conservation of the DIA quadruplet, positivity of a limiter). Those are true in
real arithmetic and generally false bit for bit in float32, so they stay as the L1 property tests
`kokkos/tests/` already runs (zero in, zero out; `A×2 → S×8`) `(v, BITWISE_PROOF §3(b))`.

## Alternatives considered

| Option | Why not now | Page |
|---|---|---|
| Rocq + Flocq (CompCert, VST) | It is the reference IEEE-754 theory and the only one that reaches real C, but it reaches no C++, Fortran, CUDA or Triton. The proofs take months for one routine and are written by hand. CompCert's licence is non-commercial. | [rocq-flocq](../proof/rocq-flocq.md) |
| Lean 4 (+ FloatSpec) | Core Lean now has a bit-level model of `+ − × ÷`, but `exp` is still `opaque` `(v, lean4 Init/Data/Float)`, and Lean has no path to C++. A Lean check of an agent-written model is only as faithful as that translation (Appendix A). | [lean4](../proof/lean4.md) |
| Isabelle/HOL | The AFP `IEEE_Floating_Point` entry includes FMA, but Isabelle has no C or C++ front end. | [isabelle-hol](../proof/isabelle-hol.md) |
| Agda, Idris 2 | `Float` is a postulate, so nothing can be proved about bits. | [agda-idris2](../proof/agda-idris2.md) |
| Liquid Haskell | It reads `Double` as an SMT real, which is unsound for bits. | [haskell-liquid-quickcheck](../proof/haskell-liquid-quickcheck.md) |
| TLA+ | It is the right tool for protocols and reductions, not for arithmetic. It is deferred to phase 3, not rejected. | [tla-plus](../proof/tla-plus.md) |
| F\* | Its verified code (HACL\*, Low\*) is integer and cryptographic, with nothing for floating point ⚠. | `BITWISE_PROOF` §6 |
| Bend | Its `F32` operations are axioms in its checker and it has no F64; it was removed from the repository in #58. | — |
| ESBMC, CBMC, Alive2 | These are not rejected. They are the next rung of the ladder, and they are tools rather than languages. | [cbmc-esbmc](../proof/cbmc-esbmc.md), [alive2](../proof/alive2.md) |

## Consequences

- **Nothing new to learn or install for the port.** The solvers are installed by the user with
  `pip install bitwuzla cvc5` and are never vendored. The sweeps and the GIMPLE diff use the
  gfortran and g++ the toolchain already pins.
- **Proofs are tied to a build.** A change of compiler, flags or libm means re-running the sweeps
  and the queries, so they belong in CI next to the L1 tests, not in a document. A proof result
  enters `kokkos/PORT_STATUS.md` as a "proved / tested" column (`BITWISE_PROOF` §4 step 6).
- **The queries are written by hand until ESBMC takes over.** That costs 1–3 days per loop body
  `(v, BITWISE_PROOF §7)`, which is affordable for the DIA and for `W3SDS4` and not for all of
  `W3SRCE`.
- **No theorem about the physics.** Conservation and sign properties stay as tests. This record
  makes no claim that the port is physically correct beyond agreeing with the Fortran.
- **Triton and WeatherNext 3 carry no bit-for-bit claim.** Their gates are tolerances and skill
  scores, and their write-ups must not borrow the Kokkos port's "bit-identical" label.

## Revisit when

- Lean 4 gains an `exp` model or a verified path to C or C++.
- A tolerance (the CUDA one, the Triton one, or ST4's reductions) has to be defended in a
  publication. Then a VCFloat2 or Gappa bound becomes worth its cost.
- Triton gains a documented bitwise-reproducible mode ⚠ (none known on 2026-10-08).
- NOAA-EMC/WW4 adopts a formal method for its own GPU port (`just ww4-status` would show it).

## Appendix A: evidence from autoformalisation

Bastounis, Circelli and Hansen (2026), *Navier-Stokes lost in translation: Why Lean verification
of AI autoformalisation does not guarantee correct natural language proofs*,
[arXiv:2610.08144](https://arxiv.org/abs/2610.08144) `(v, read on arxiv.org on 2026-10-09)`.

The paper studies proofs that an AI translates from prose into Lean and then has Lean check. Its
thesis is that "Lean acceptance (compilation) of translation does not imply semantic
preservation" (§4.3). §2 gives translations that turn a wrong proof into a correct Lean proof. §3
argues that the Lean code published with OpenAI's Navier-Stokes blow-up claim states weaker
results than the paper it was translated from. §4 argues that resolving the ambiguities of a text
is not computable in general (it sits at the top of the Solvability Complexity Index hierarchy).

This supports fact 1 of the context by analogy. A checked theorem is a statement about the
formal model, and its value depends on a translation step the checker does not see. Here that step
is from the gfortran and nvcc builds to a hand-written model; in the paper it is from prose to
Lean. The paper also covers the case of an agent writing the model, which is how a proof
language would enter this repository.

Its limits for this record:
- It is about mathematics translated from prose, not about code. It says nothing about floating
  point, compilers or GPUs.
- It gives worked examples and a complexity argument, not error rates.
- It does not bear on the SMT route of the decision, whose queries are written from the Fortran
  and C++ operation graphs rather than from prose. A hand-written query carries the same fidelity
  risk in a smaller form, which is why ESBMC, which builds the queries from the sources, is the
  next rung.
