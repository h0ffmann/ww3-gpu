# Proving the port bit for bit
## A plan to formalise Fortran → C → C++/Kokkos equivalence for WW3 kernels, from captured-input tests to solver-checked proofs

Prepared 1 October 2026. A plan with one pilot already run. `(v)` marks a claim checked on that
date against a source named in §10 or a command run for this document; `⚠` marks one that was not
checked. "Measured" means run in this document's sandbox (x86-64, 4 cores, GCC/gfortran 13.3,
clang 18.1, glibc 2.39, Z3 5.1.0, cvc5 1.4.1, Bitwuzla 0.9.1), not on the lab workstation that
`kokkos/PORT_STATUS.md` reports.

Per-tool evidence (one page each: what it is, where it fits this repo, physics fit, bit-for-bit
reach, cost, verdict) is in [`docs/proof/`](proof/README.md). This file is the plan; the
side-by-side comparison of every option is §9.

## 1. Summary and recommendation

The port already has rung (a): the `W3SNL1` kernel reproduces the Fortran fixture with a maximum
relative error of exactly 0.0 on all three presets `(v, PORT_STATUS.md)`. That is a test on three
sea points. The plan turns it into proofs where a proof is cheap, and says plainly where none is
possible.

- **Bit for bit compares two binaries.** The committed fixture is
  the output of gfortran 15.3 at `-g` (the serial-debug build), WW3 runs gfortran at `-O3`, and
  the measured pilot shows the same three Fortran lines giving different bits under `-O3` and
  under `-O3 -march=x86-64-v3` (§2, §4).
- **Do now, in this order:** exhaustive sweeps of every function with a single float32 input (a
  sweep of all 2^32 inputs takes 17 s here, and covering every input makes it a proof for that
  build); a GIMPLE diff of gfortran against g++, because both compilers share GCC's middle end; and
  SMT queries over IEEE-754 for loop bodies and for every rewrite an agent proposes. All three are
  free tools, run in days, and fit the proposal's one-month Kokkos rung (`08-schedule.md`: rung 4
  in 03/2027 `(v)`).
- **Do later, at `W3SRCE` scale:** ESBMC or CBMC on the C++ (they read the real code), Alive2 on
  flang and clang IR, and a fixed-order reduction mode for every kernel that sums.
- **Bit for bit is impossible by construction** between a sequential Fortran sum and a parallel
  reduction, and between glibc's `expf` and CUDA's. For those the fallback is what the proposal
  already says (`07-methodology.md`: CUDA "comparado estatisticamente" `(v)`): `nccmp-tol`
  tolerances, now backed by an exhaustive count of where the two libm functions disagree on the
  routine's actual input domain.
- **Interactive provers** (Rocq + Flocq, Isabelle, Lean 4) and dependently typed languages (Agda,
  Idris 2) cannot read Fortran or C++. They prove things about a model written by hand. Not for
  this project's timeline. **TLA+** fits the MPI and halo protocol of phase 3, not arithmetic.
- **Bend** stays where `BEND_TRYOUT_202609.md` put it: off the port's route. F64 is not the reason.
  Most WW3 physics runs in default `REAL` and is F64-free `(v, §5)`. Upstream now lists F64 and a
  library target as planned (§5), but Bend's `F32` operations are axioms in its checker, so it
  cannot prove anything about this arithmetic today.

## 2. What "bit for bit" can and cannot mean, stage by stage

IEEE-754 makes `+ − × ÷ sqrt` and FMA deterministic: given the same operands, rounding mode and
format, every conforming unit returns the same bits. So two programs give the same bits exactly
when they perform the same correctly rounded operations, in the same order, on the same operands,
call the same transcendental implementations, and run with the same floating-point environment.
Everything below is a way that one of those conditions breaks.

| Source of difference | Where it bites | Fact |
|---|---|---|
| The language | Fortran ↔ anything | The Fortran standard lets a processor evaluate any mathematically equivalent expression as long as parentheses are respected ⚠ (F2018 §10.1.5.2.4, not reopened). gfortran honours parentheses and does not reassociate unless `-Ofast` or `-fno-protect-parens` `(v, gcc/fortran/invoke.texi)`; flang also protects parentheses by default `(v, flang CompilerInvocation.cpp)`. |
| FMA contraction | every stage | GCC defaults to `-ffp-contract=fast` except for C in ISO mode `(v, gcc/doc/invoke.texi)`. So C++ under `-std=c++20` fuses too. Flang's default is `fast` `(v, LangOptions.def)`. Measured on `EP1` (four products summed, `snl1_dia.cpp:154`): g++ `-O3 -march=x86-64-v3` emits 3 FMAs, 0 with `-ffp-contract=off`, 0 on baseline x86-64 (no FMA instruction), 0 for `gcc -std=c11`, 3 for `-std=gnu11`; clang 18 emits 3 at `-march=x86-64-v3` and on aarch64 `(v, measured)`. |
| The Fortran build itself | Fortran ↔ C | WW3's GNU flags are `-g -fno-second-underscore -ffree-line-length-none` plus `-O3`, with no `-march`, so on x86-64 the reference has no FMA. Its Intel flags are `-no-fma -fp-model precise`, and its PGI flags carry `-Kieee` `(v, WW3 model/src/CMakeLists.txt @761cf79)`. Add `-march=x86-64-v3` to gfortran and section 1 of `W3SNL1` alone gains two fused operations `(v, measured)` (§4). |
| x87 vs SSE | 32-bit x86 only | GCC's default `-fexcess-precision=fast` lets x87 compute in 80 bits `(v, invoke.texi)`. x86-64 does scalar float arithmetic in SSE registers, so this does not arise on the lab's machines ⚠ (`FLT_EVAL_METHOD == 0` assumed, not printed). |
| Denormals, FTZ/DAZ | CPU with fast-math, CUDA | `-funsafe-math-optimizations` (part of `-ffast-math`/`-Ofast`) "may include libraries or startup files that change the default FPU control word" `(v, invoke.texi)`. nvcc's `--use_fast_math` implies `--ftz=true --prec-div=false --prec-sqrt=false --fmad=true`, and `--fmad` defaults to true `(v, NVIDIA Best Practices Guide, via search)`. The DIA fixture has denormal bins, which is why its L1 gate has a 1e-30 floor `(v, course/12)`: an FTZ build changes those bits. |
| `-fp-model precise`/`strict` (Intel) | ifx builds of WW3 | `precise` forbids value-changing optimisations; `strict` also honours the FP environment and exceptions ⚠ (Intel docs not reopened). WW3 uses `precise` with `-no-fma` `(v)`. |
| libm | every stage with `exp`, `log`, `pow`, `tanh`… | gfortran lowers `EXP` on default `REAL` to `call expf` `(v, measured asm)`; `Kokkos::exp(float)` is `std::exp(float)` on the host `(v, Kokkos 5.2.0 Kokkos_MathematicalFunctions.hpp)`, so both reach glibc's `expf`. CUDA's device `expf` is another implementation with a bound of about 2 ULP ⚠, and `__expf` under fast-math is coarser. Bend computes `(float)exp((double)x)` `(v, BEND_TRYOUT §4)`, a different entry point: 3 350 physical inputs of section 1 change by it `(v, measured, §4)`. |
| `MAX`/`MIN` on NaN | Fortran ↔ C++ | Measured on section 1: gfortran `-O0` and `-O3 -march=x86-64-v3` return `KDMN` for `MAX(NaN, KDMN)`, gfortran `-O3` and the port (`Kokkos::max(a, b)` is `(a < b) ? b : a` `(v, Kokkos_MinMax.hpp)`) return NaN `(v, measured)`. No physical input is NaN, so this is a domain restriction to state, not a bug. |
| Signed zero | rewrites | `-2*(a+b)` and `-2*a + -2*b` agree on every input with no overflow and a non-zero result, and differ when `a = -b` (`-0` against `+0`) `(v, Bitwuzla and cvc5, §4)`. A relative-tolerance test never sees this; a bitwise one does. |
| Reductions | Serial ↔ OpenMP ↔ CUDA | Float addition is not associative. A parallel reduction's order depends on the thread or team layout, so it cannot equal the Fortran's left-to-right sum bit for bit. `WW_DETERMINISTIC` exists to pin the order `(v, kokkos/README.md)`. The DIA has no reduction `(v, snl1_dia.cpp)`; `W3SRCE` and ST4 will (`team_reduce` for integrals `(v, course/13)`). |

What follows for each pair of stages:

| Pair | Bit equality is | Under these conditions | Fallback when it fails |
|---|---|---|---|
| Fortran ↔ literal C/C++ | **provable** per routine | same operation DAG; contraction off on both sides (or no FMA hardware); same libm entry points; no FTZ; inputs restricted to the domain the model sees (finite, no NaN) | exhaustive or SMT check of the routine; then L1 tolerance |
| C ↔ Kokkos Serial | **provable** | the Kokkos wrappers expand to the same `std::` calls and comparisons `(v, §2 rows above)`; View indexing moves no values | none needed |
| Serial ↔ OpenMP | **provable** without reductions; **testable only** with them | `WW_DETERMINISTIC=ON` forces a fixed order | fixed-order reduction in validation builds, tolerance in production |
| CPU ↔ CUDA | **provable** for `+ − × ÷ sqrt` kernels with `--fmad=false`, no `--use_fast_math`, no FTZ; **testable only** once a transcendental is involved | CUDA's `expf` agrees with glibc's on the routine's input domain, which is a finite set that can be swept | exhaustive count of disagreements on the domain, then `nccmp-tol` per-field tolerances (the proposal's statistical comparison) |
| Any ↔ unpinned parallel reduction | **impossible** by construction | n/a | compensated summation in float32 (Kahan) narrows the gap ⚠ (not evaluated); otherwise tolerance |

Two notes on the existing record. The "bit-identical" CUDA cell of the ledger is true on the three
fixture points; at point 0 `EXP(X2)` underflows to zero on both sides `(v, fixtures/README.md)`,
so only two `expf` inputs were ever compared on the GPU. And WW3 itself promises thread-count
reproducibility only under its `B4B` switch, which affects SMC-grid code `(v, GLOSSARY)`.

## 3. The proof ladder

Each rung is stronger than the one below it and costs more to build. A rung is a gate only for
the kernels it was applied to.

**(a) Differential testing on captured inputs.** Exists: `snl1_ref.F90` (verbatim Fortran), the
committed fixture, `L1_test_snl1_*`, `shim_roundtrip`, the L2 replay with `nccmp-tol` `(v)`. It
shows equality on the captured inputs and nothing about any other input.

**(b) Exhaustive and property-based testing.** A function of one float32 input has 2^32 inputs,
and a sweep over all of them is a proof for that binary pair: the pilot does it in 17 s on 4 cores
`(v, measured)`. Integer tables (`INSNL1` sections 3–7, `ISP = ITH + (IK-1)*NTH`, the
`MAPSTA`/`MAPFS`/`MAPSF` maps, `INIT_GET_ISEA`/`INIT_GET_JSEA_ISPROC` in `w3parall.F90:1152-1504`
`(v)`) are exhaustively checkable for every grid size the lab runs. Two float inputs make 2^64
cases, so the method stops there. For those, generated inputs with deliberate edge cases (±0,
denormals, the largest finite value, values one ULP either side of a `MAX` threshold) plus the
physics properties the L1 suite already checks (zero in → zero out; `A×2 → S×8`) `(v)`, in
GoogleTest, with no new dependency. [`proof/testing-exhaustive-property.md`](proof/testing-exhaustive-property.md).

**(c) Translation validation per routine.**
- *GIMPLE diff.* gfortran and g++ lower to the same GCC middle end. With
  `-fdump-tree-optimized`, section 1 of `W3SNL1` shows the same twelve floating-point operations
  in the same order on both sides `(v, measured)`; the only differences are SSA names,
  commuted operands of `*` and `+` (exact in IEEE), and `MAX_EXPR` against a compare-and-select,
  which is the NaN difference of §2. Normalising the dumps and diffing them is a script, and it
  covers every routine the two compilers can both build. [`proof/gimple-diff.md`](proof/gimple-diff.md).
- *SMT over IEEE-754.* SMT-LIB's `QF_FP` theory models binary32 and binary64 bit for bit, with
  rounding modes, ±0, NaN and FMA. A loop body written out as two expressions is a query: `unsat`
  is a proof of equality for every input, `sat` comes with a counterexample. Measured on two
  pilot queries: Bitwuzla and cvc5 answer in 0.4–79 s; Z3 needed 31 s for one and did not finish
  the last `unsat` of the other within 15 minutes `(v)`. No SMT solver has `exp`, so a body containing one is
  checked with the call left uninterpreted (both sides call the same function on equal
  arguments). [`proof/smt-z3-bitwuzla-cvc5.md`](proof/smt-z3-bitwuzla-cvc5.md).

**(d) Tools that read the code and build the query themselves.**

| Tool | Reads | Floating point | Fit here |
|---|---|---|---|
| CBMC | C, C++ `(v, README)` | bit-precise; its `sqrtf` model is exact, but its `expf` is a Schraudolph approximation, not glibc's `(v, src/ansi-c/library/math.c)` | equivalence harness for small C++ kernels; transcendentals need care |
| ESBMC | C, C++, CUDA via clang `(v, README)` | IEEE through Z3, Bitwuzla or cvc5 `(v)` | the same, plus CUDA kernels; maintained |
| Alive2 | LLVM IR | `fadd fsub fmul fdiv frem`, min/max, `sqrt`, `fma`/`fmuladd`, fast-math flags `(v, ir/instr.h)`; bounded loop unrolling `--src-unroll/--tgt-unroll` `(v)`; no `exp` op ⚠ | flang IR against clang IR: proves *flang's* compilation of the Fortran, not gfortran's |
| KLEE-Float | LLVM 3.4 bitcode `(v, .travis.yml)` | symbolic floats via Z3 | last commit 2022-02-15 `(v)`: unusable with today's compilers |
| Frama-C + Why3 | C only (C++ through Frama-Clang ⚠) | WP float model on Why3's `ieee_float` ⚠ | contracts on a C reference stage, if the lab keeps one |

Realistic for an undergraduate: (b) and (c) fully, on the pilot and on every new kernel; one tool
from (d) tried on the pilot (ESBMC first: it reads C++ and CUDA and drives the same solvers).
Nothing from (d) at `W3SRCE` scale inside the 2026/27 schedule.

## 4. Pilot: `W3SNL1` section 1, plus two SMT queries from sections 3 and 4

The target is section 1 of `W3SNL1`, the propagation constant, the smallest real piece of the
only ported kernel: three Fortran lines (`w3snl1md.F90:340-342` at 761cf79 `(v)`; `snl1_ref.F90:343-345`;
`snl1_dia.cpp:108-112`), one runtime input (`KDMEAN`), and the kernel's only libm call. One float32
input means the sweep is exhaustive.

What was added on this branch (no Kokkos, no CMake; gfortran and g++ only):

| File | What |
|---|---|
| `proof/snl1_cons/cons_ref.F90` | the three Fortran lines, verbatim, in a `BIND(C)` function |
| `proof/snl1_cons/cons_port.cpp` | the three C++ lines, with `Kokkos::max`/`exp` expanded to their 5.2.0 definitions |
| `proof/snl1_cons/sweep.cpp` | calls both on all 2^32 `KDMEAN` bit patterns with the fixture's `&SNL1` parameters; exits 0 iff no physical input differs |
| `proof/snl1_cons/run.sh` | builds each side as its own translation unit with explicit flags and runs five configurations |
| `proof/smt/ep1_contraction.smt2`, `s4_distribute.smt2` | the two worked SMT queries |

Result of `proof/snl1_cons/run.sh` (measured 2026-10-01, ~17 s per configuration):

| Config | Fortran flags | C++ flags | Physical inputs that differ | Other |
|---|---|---|---|---|
| `parity` | WW3's GNU flags, `-O3` | `-std=c++20 -O3 -march=x86-64-v3 -ffp-contract=off` | **0** of 2^32 | NaN and ±inf inputs agree too |
| `fixture` | `-O0 -g` (serial-debug, which wrote the fixture) | same | **0** | 16 777 214 NaN inputs: Fortran returns a number, the port NaN |
| `fma-cxx` | `-O3` | without `-ffp-contract=off` | 4 632 147 (1 ULP; ~7 % of the 6.2e7 floats between 2/3 and 111 where `CONS` varies) | |
| `fma-f90` | `-O3 -march=x86-64-v3` | parity flags | 4 632 147 | plus the NaN inputs |
| `bend-exp` | `-O3` | parity flags, `exp` as `(float)exp((double)x)` | 3 350 (1 ULP) | |

So section 1 of the port is proven bit-identical to the Fortran for every input on this toolchain,
and both ways of losing that are measured. Steps, with effort for one person:

| # | Step | Command | Effort | State |
|---|---|---|---|---|
| 1 | Exhaustive sweep, five configurations | `bash proof/snl1_cons/run.sh` | 0.5 d | done here; rerun under `just ww3` (gfortran 15.3) to make it a lab number |
| 2 | GIMPLE diff of the same pair | `gfortran -O3 -c -fdump-tree-optimized=ref.gimple proof/snl1_cons/cons_ref.F90` and the same with `g++ $CXX_PORT` on `cons_port.cpp`; normalise SSA names and diff | 0.5 d to script | done by hand here |
| 3 | Close the copy gap: move `snl1_dia.cpp:108-112` into one `KOKKOS_INLINE_FUNCTION Real snl1_cons(...)` in a header that the kernel and `sweep.cpp` both include | `just kokkos-test serial-debug` and `openmp-release` must still read 0.0 | 0.5 d | not done: it touches the kernel, and this sandbox has no Kokkos |
| 4 | CUDA: the same sweep as a `parallel_for` over 2^32 on the RTX 4090, against the host's answers | a `--fmad=false` build in `#cuda` | 1 d | not done; it replaces "bit-identical on two `expf` inputs" with a count over the whole domain |
| 5 | SMT for the rest of the kernel: one query per output of section 3 (`SA1`…`DA2M`) and section 4 (`S`, `D`), Fortran DAG against C++ DAG, with table reads as free variables | `z3 q.smt2`, or Bitwuzla through its Python API | 2–3 d | two examples done: `ep1_contraction.smt2` (sat, the FMA counterexample, 0.4–31 s by solver), `s4_distribute.smt2` (commuting `unsat`, distributing `sat` by overflow and by `-0`, `unsat` once those are excluded) |
| 6 | Write the result into `PORT_STATUS.md` as a third parity column ("proved" / "tested") | | 0.5 d | |

About a week in total, inside the course-12 material and without new dependencies. A solver is
installed by the user, never vendored: `pip install z3-solver` ships a `z3` command;
`pip install bitwuzla cvc5` ships Python APIs only, which is how their timings above were taken.

## 5. Bend for the modules that do not need F64

What Bend offers is restated in `BEND_TRYOUT_202609.md` §2–3 and not repeated here: pure functions,
fork-join parallelism with no data races by construction, `Nat`, `U32` and `F32` as its only
numbers, laws proved by evaluation in its checker. Two points matter for proofs and both are
measured or read for this document.

**F64 is not what keeps Bend out of WW3.** A scan of the pinned source (`model/src` @761cf79,
counting non-comment lines with `DOUBLE PRECISION`, `REAL(8)`, `REAL*8`, `REAL(KIND=8)`, `DBLE(`,
`_R8` or a `D`-exponent literal) gives `(v)`:

| No F64 at all | F64 inside |
|---|---|
| `w3src4md` (ST4), `w3sln1md`, `w3sbt1md`, `w3pro3md`, `w3uqckmd`, `w3dispmd`, `w3wavemd`, `w3iogomd`, `w3partmd`, `w3iorsmd`, `w3iogrmd`, `w3fldsmd`, `w3initmd`, `w3adatmd`, `w3wdatmd`, `w3odatmd`, `w3str1md`; and `W3SNL1` + `INSNL1` (`w3snl1md.F90:115-786`) | `w3sdb1md` (DB1, in the lab switch: `REAL*8` Battjes–Janssen internals); `W3SNLGQM` in the same file as the DIA (lines 789–1182); `w3srcemd` (ice attenuation `ATT`, `IS2`, a `DB1` implicit branch); `w3gdatmd` (`XGRD`/`YGRD` grid coordinates, unstructured-mesh arrays); `w3timemd` (`TIME2HOURS`, Julian days); `w3updtmd` (tidal arguments); `constants.F90` (Bessel functions); `w3servmd` (rotated-pole transforms); `w3parall` (timers); `w3gsrumd`, `w3profsmd`, `w3triamd` (grid search, unstructured) |

So almost all of the lab's physics is F32 and would type-check against Bend's `F32`. The blockers
are the ones `BEND_TRYOUT` §9 lists: no C ABI, single-owner arrays that parallel branches must
clone, and a toolchain that changes daily. Since 18 September the first of these, and F64, have
moved on paper: upstream's `WONTFIX.txt` now lists, under "SOON (we will add it; do not open an issue)", F64 (#1120: "It needs
U64's 64-bit word design; we add both together, and we do not merge PRs for F64"), the native
library target (#813: "planned, not scheduled"), and "F32 that computes in the checker" (#1017:
"F32 operations are axioms today; bit-level definitions are planned") `(v, bendlang/bend main,
2026-10-01)`. `BEND_TRYOUT` §3.1 and §9 read #813 as refused under CAPACITY and F64 as without a
roadmap; both were correct on 18 September and are now out of date. None of the three has shipped.

**For proofs Bend adds less than it seems.**
- Purity and affinity give race freedom and termination by construction. The C++ already gets
  race freedom for the DIA from its structure (no reduction, one writer per element `(v)`), and
  a test catches the rest.
- `F32` operations are axioms in the checker (#1017), so no law about float arithmetic can be
  proved today, and none of the physics laws a modeller wants is true in float arithmetic anyway
  (`BEND_TRYOUT` §8).
- What Bend can prove is closed integer claims decided by evaluation: "every pre-shifted DIA
  table entry lies in `[0, 1024)` for this grid", "the ISP map is a bijection for NK=25, NTH=24",
  "the card-deck map `ISEA → (JSEA, ISPROC)` round-trips for NSEA=N, NAPROC=P". The same claims are
  exhaustive tests in C++ in an afternoon. `U32` has no sign `(v, BEND_TRYOUT §3.1)`, so WW3's
  negative DIA addresses must be shifted first.
- Floating-point control: Bend's CPU build is `clang -std=c11 -O3` `(v, bend2/main.ts)`. On
  baseline x86-64 that emits no FMA; on `-march=x86-64-v3` or aarch64 (Apple silicon) clang 18
  contracts within an expression `(v, measured)`, and Bend exposes no flag. This settles the ⚠ in
  `BEND_TRYOUT` §4 item 1 for the workstation: no contraction there unless `CC` adds `-march`.

Verdict: a research curiosity for this repo's proofs. Run the tryout week as written; if it
reaches its step 5, write the integer-table law there. Do not plan proof work around Bend.
[`proof/bend.md`](proof/bend.md).

## 6. Proof languages and assistants

None of these reads Fortran or C++, except Rocq via CompCert's C semantics. Each proves
properties of a model someone writes, so for this port each is a specification tool at best. One
line each here; the evidence is in the linked files.

- **Rocq + Flocq** — the reference IEEE-754 formalisation; CompCert's C semantics are built on it
  `(v, CompCert flocq/ directory)`; VST verifies C programs through CompCert's Clight ⚠; VCFloat2 bounds round-off
  automatically `(v, CPP 2024)`. The only route that reaches real C, at research-group cost.
  [`proof/rocq-flocq.md`](proof/rocq-flocq.md)
- **Isabelle/HOL** — AFP `IEEE_Floating_Point` (with FMA, code generation) `(v)`; no C++ front end.
  [`proof/isabelle-hol.md`](proof/isabelle-hol.md)
- **Lean 4** — core now gives `Float` and `Float32` a bit-level logical model for the basic operations, with
  `exp` still `opaque` `(v, lean4 master Init/Data/Float)`; FloatSpec ports Flocq and proves the
  native operators correctly rounded `(v, its README)`. Promising, young.
  [`proof/lean4.md`](proof/lean4.md)
- **Agda, Idris 2** — floats are postulated primitives (`postulate Float`, `primFloatExp` `(v)`);
  good for writing the reference semantics of a kernel as a total function, nothing about bits.
  [`proof/agda-idris2.md`](proof/agda-idris2.md)
- **Liquid Haskell + QuickCheck** — Liquid Haskell reads `Double` as an SMT real `(v, tech
  report)`, so its float proofs are unsound for bits; QuickCheck-style generation is the useful
  part, and GoogleTest can already do it. [`proof/haskell-liquid-quickcheck.md`](proof/haskell-liquid-quickcheck.md)
- **TLA+** — model checks concurrent protocols over integers and finite sets ⚠ (TLC has no floats);
  the right tool for phase 3's halo exchange and for a deterministic-reduction schedule, not
  for arithmetic. [`proof/tla-plus.md`](proof/tla-plus.md)
- **F\*** — dropped: its verified-code story (HACL\*, Low\*) is integer and cryptographic ⚠, with
  nothing for floating point.

Bend relates to this family as a pure functional language whose laws are checked by evaluation,
like Agda's `refl` on closed terms, and it inherits the same limit: floats are opaque to the
checker.

## 7. Decision table

| Stage | Technique | Provable or testable | Effort (one person) | When |
|---|---|---|---|---|
| Fortran ↔ C++ | differential on captured inputs (L1, `shim_roundtrip`, L2 + `nccmp-tol`) | testable | exists | now (keep) |
| Fortran ↔ C++ | exhaustive sweep, one float32 input or integer tables | provable for that build | hours per function | **now** (pilot done) |
| Fortran ↔ C++ | edge-case and property generation in GoogleTest | testable | 1–2 d per kernel | **now** |
| Fortran ↔ C++ | GIMPLE diff (gfortran vs g++) | provable modulo GCC's back end | 0.5 d script, minutes per routine | **now** |
| Fortran ↔ C++, loop bodies | SMT `QF_FP` (Bitwuzla, cvc5) | provable for the written DAG | 1–3 d per body | **now** for the DIA; later at scale |
| C++ ↔ C++ rewrites (phase 2 optimisations) | SMT or Alive2 per rewrite | provable | hours per rewrite | later |
| C++ kernel ↔ spec | ESBMC / CBMC harness | bounded proof | ~1 wk setup | later (try ESBMC on the pilot) |
| Fortran (flang) ↔ C++ (clang) | Alive2 on IR | provable for flang's build | 1–2 wk setup | later |
| Serial ↔ OpenMP | no reduction: inspection; with reductions: `WW_DETERMINISTIC` | provable / testable | per kernel | **now** (rule for every new kernel) |
| CPU ↔ CUDA | exhaustive sweep of device libm on the routine's domain; then `nccmp-tol` | testable; provable for `+ − × ÷ sqrt` only | 1 d per function | **now** for the DIA |
| CPU ↔ CUDA | rigorous error bound (VCFloat2, Gappa) to justify a tolerance | provable bound | 2+ wk | later |
| MPI / halo protocol | TLA+ model | provable for the model | 1–2 wk | later (phase 3) |
| Integer bookkeeping | Bend or Lean laws | provable | days | no (C++ exhaustive tests do it) |
| Physics properties (conservation) | Rocq / Isabelle / Lean on a model | provable for the model | months | no |

## 8. What was not verified

- ⚠ The Fortran standard's clause on equivalent expressions, Intel's `-fp-model` definitions, and
  CUDA's documented `expf` bound: their documentation hosts are blocked from this sandbox.
- ⚠ The pilot ran with GCC 13.3 and glibc 2.39, not the lab's GCC 15.3 under nix. Step 1 of §4
  reruns it there. A different glibc `expf` could change the `bend-exp` count, and the `parity`
  result holds for the lab only once that rerun reads 0.
- ⚠ Alive2's handling of `llvm.exp.*`, ESBMC's `expf` model, and Frama-Clang's state were not
  run or read.
- ⚠ The F64 scan of §5 is a regular expression over source lines, not a type analysis: it can miss
  a kind parameter defined elsewhere and counts declarations in inactive `#ifdef` branches.
- ⚠ The ~7 % share in §4 assumes `CONS` is constant outside `2/3 < KDMEAN < 111`; the bounds
  come from `KDMN/KDCON` and from where `expf` underflows, not from a measurement.
- ⚠ Z3's failure on `s4_distribute.smt2` query (d) is one run with a 15-minute limit.

## 9. All proof options side by side

Rows are options, columns the same criteria for each. Cells are short; the reasons are in the
linked files. "b4b" is the reach for a bit-for-bit claim across Fortran ↔ C ↔ Kokkos Serial ↔
OpenMP ↔ CUDA. Effort is for one undergraduate. Verdict: **now**, **later** or **no**.

| Option | Proves or checks | b4b reach | IEEE / F64 / libm | Reads real code? | WW3 physics fit | Pilot here | Pilot effort | Scale to `W3SRCE` | Maintenance | Licence | Undergrad fit | Verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Differential L1/L2 (exists) | equality on captured inputs | all stages, sampled | native, real libm | yes, the binaries | full | fixture, 3 points | done | per kernel fixture | regenerate fixture | MIT (repo) | yes | **now** |
| [Exhaustive + property tests](proof/testing-exhaustive-property.md) | all inputs (1 float) or generated cases | proof for 1-input functions | native, real libm | yes, the binaries | full | `W3SNL1` §1 | done, 17 s run | integer maps, 1-input helpers | rerun in CI | none needed | yes | **now** |
| [GIMPLE diff](proof/gimple-diff.md) | same FP op DAG after GCC's middle end | Fortran ↔ C++ (GCC only) | n/a (structural) | yes, compiler IR | full | `W3SNL1` §1 | 0.5 d | every routine | script only | GPL compiler, nothing new | yes | **now** |
| [SMT: Bitwuzla, cvc5, Z3](proof/smt-z3-bitwuzla-cvc5.md) | ∀ inputs of a written DAG | any pair, per body | exact binary32/64; no `exp` | no, hand-written query | loop bodies only | §3.a, §4 queries | 2–3 d | too many bodies by hand | redo per change | MIT / BSD | yes | **now** |
| [CBMC](proof/cbmc-esbmc.md) | bounded ∀ on C/C++ harness | C ↔ C++ | bit-precise; own `expf` model | C, C++ | small kernels | §1 harness | 3–5 d | loops too large ⚠ | harness per kernel | BSD-style | maybe | **later** |
| [ESBMC](proof/cbmc-esbmc.md) | bounded ∀ on C/C++/CUDA | C ↔ C++ ↔ CUDA source | via Z3/Bitwuzla/cvc5 | C, C++, CUDA | small kernels | §1 harness | 3–5 d | loops too large ⚠ | harness per kernel | permissive core, mixed solvers | maybe | **later** (first of (d)) |
| [Alive2 (+ flang)](proof/alive2.md) | IR refinement, bounded loops | flang Fortran ↔ clang C++ | `fadd`…`fma`, `sqrt`; no `exp` ⚠ | LLVM IR | per function | §1 IR | 1–2 wk (LLVM build) | function-local only | tied to LLVM main | MIT | stretch | **later** |
| [KLEE-Float](proof/klee-float.md) | symbolic paths with FP | none today | Z3 FP | LLVM 3.4 bitcode | none | — | — | — | dead since 2022 | NCSA | no | **no** |
| [Frama-C + Why3](proof/frama-c-why3.md) | ACSL contracts on C | C reference only | `ieee_float` via WP ⚠ | C (C++ ⚠) | if a C stage exists | — | 1–2 wk | heavy | contracts rot | LGPL-2.1 | stretch | **later** |
| [Rocq + Flocq (CompCert, VST, VCFloat2)](proof/rocq-flocq.md) | theorems on a model; C via VST | C (CompCert) only | complete IEEE theory | C via Clight; no C++/Fortran | hand model | — | months | no | high | LGPL; CompCert non-commercial | no | **no** (VCFloat2 bounds: later) |
| [Isabelle/HOL](proof/isabelle-hol.md) | theorems on a model | none | AFP IEEE incl. FMA | no | hand model | — | months | no | high | BSD-style ⚠ | no | **no** |
| [Lean 4 (+ FloatSpec)](proof/lean4.md) | theorems on a model | none | core model for `+ − × ÷`; `exp` opaque | no | hand model | — | weeks | no | high, fast-moving | Apache-2.0 | no | **no** (watch) |
| [Agda](proof/agda-idris2.md) | reference semantics as total functions | none | postulated `Float` | no | hand model | — | weeks | no | medium | MIT-style | no | **no** |
| [Idris 2](proof/agda-idris2.md) | same as Agda | none | primitive `Double` ⚠ | no | hand model | — | weeks | no | medium | BSD-style | no | **no** |
| [Liquid Haskell + QuickCheck](proof/haskell-liquid-quickcheck.md) | refinements over reals; random tests | none (reals ≠ floats) | `Double` as SMT real | Haskell only | hand model | — | weeks | no | medium | BSD-style | no | **no** (use GoogleTest) |
| [TLA+](proof/tla-plus.md) | protocol safety/liveness | ordering specs, not values | none ⚠ | no | halos, reductions as protocols | — | 1–2 wk | phase 3 | low | MIT | yes, later | **later** |
| [Bend](proof/bend.md) | closed integer laws by evaluation | none for floats | `F32` axioms; F64 planned | no, own language | F32 kernels as programs | INSNL1 tables | in tryout week | no | daily churn | Apache-2.0 | as a tryout | **no** for proofs |

## 10. Sources

This repository, read 2026-10-01: `README.md`, `CONTRIBUTING.md`, `docs/KOKKOS_H100_PLAN_202609.md`,
`docs/AGENTS_KOKKOS_202609.md`, `docs/BEND_TRYOUT_202609.md`, `docs/GLOSSARY.md`,
`course/12-porting-a-kernel-w3snl1.md`, `course/13-bulk-porting-with-agents.md`,
`kokkos/README.md`, `kokkos/PORT_STATUS.md`, `kokkos/CMakeLists.txt`, `kokkos/CMakePresets.json`,
`kokkos/src/ww_kokkos/{CMakeLists.txt,real.hpp,snl1_dia.cpp}`, `kokkos/tests/fixtures/{README.md,CMakeLists.txt,snl1_ref.F90}`,
`kokkos/tools/nccmp-tol/README.md`, `pubs/proposal/pt/07-methodology.md`, `pubs/proposal/en/08-schedule.md`,
`switches/switch_lab_shrd`.

WW3, `h0ffmann/WW3` @761cf79 (sparse `model/src`): `CMakeLists.txt`, `model/src/CMakeLists.txt`,
`w3snl1md.F90`, `w3srcemd.F90`, `w3parall.F90`, and the F64 scan of §5 over all of `model/src`.

Compilers and libraries, fetched from GitHub mirrors on 2026-10-01: `gcc/doc/invoke.texi` and
`gcc/fortran/invoke.texi` (gcc-mirror master); `flang/lib/Frontend/CompilerInvocation.cpp` and
`flang/include/flang/Support/LangOptions.def` (llvm-project main); Kokkos 5.2.0
`core/src/Kokkos_MinMax.hpp`, `Kokkos_MathematicalFunctions.hpp`; NVIDIA CUDA Best Practices
Guide, "nvcc Compiler Switches" (search result, page blocked).

Tools: Alive2 `README.md`, `ir/instr.h`, `llvm_util/cmd_args_list.h`; CBMC `README.md`,
`src/ansi-c/library/math.c`; ESBMC `README.md`; KLEE-Float `.travis.yml` and last commit;
Bitwuzla, cvc5, Z3 licences; Lean 4 `src/Init/Data/Float/{Float.lean,Model/Float.lean}`;
FloatSpec `README.md`; Agda `Agda/Builtin/Float.agda`; Bend `WONTFIX.txt`, `bend2/main.ts`,
`bend2/base.bend` (main); CompCert `LICENSE` and `flocq/`; search results for VCFloat2 (CPP 2024),
Isabelle AFP `IEEE_Floating_Point`, Frama-C WP float model, Liquid Haskell's real encoding of
`Double`, Flocq 4.2.

Measurements: `proof/snl1_cons/run.sh`, the two `proof/smt/*.smt2` files, the contraction probe
on `EP1` (g++, gcc, clang++ at the flags in §2), `gfortran -S` on `cons_ref.F90`, and
`-fdump-tree-optimized` on both pilot sources.
