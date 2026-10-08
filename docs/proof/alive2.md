# Alive2, with flang for the Fortran side

Checked 2026-10-01 against Alive2's `README.md`, `ir/instr.h` and `llvm_util/cmd_args_list.h`
(master), and flang's `CompilerInvocation.cpp` and `LangOptions.def` (llvm-project main). Not
built or run. MIT licence `(v)`. Plan: [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §3 (d).

## What it is

Translation validation for LLVM IR: `alive-tv src.ll tgt.ll` checks that the target function
refines the source for every input, and prints a counterexample otherwise `(v, README)`. It models
`fadd fsub fmul fdiv frem`, min/max, `fabs fneg sqrt` and rounding operations, `fma` and
`fmuladd`, with fast-math flags and rounding modes `(v, ir/instr.h)`. Loops are unrolled to a bound
set by `--src-unroll`/`--tgt-unroll` `(v)`. It does not support inter-procedural transformations
`(v, README)`. An online instance runs at alive2.llvm.org/ce `(v, README)`.

## Applied to ww3-gpu

- Compile `cons_ref.F90` with flang and `cons_port.cpp` with clang++, both with
  `-ffp-contract=off` and `-S -emit-llvm` ⚠ (flag spelling for flang not tried), rename one
  function so the names match, and run `alive-tv ref.ll port.ll`.
- Then the section 3 loop body, unrolled once.
- Phase 2 optimisation PRs: the before and after IR of a kernel change, which is Alive2's home
  ground.

## Wave model and physics fit

Function-local arithmetic and short loops. No `exp` operation in its IR ⚠: a call to `expf` or
`llvm.exp.f32` has to match as a call. Module state appears as memory, which it models.

## Bit for bit

It proves that flang's Fortran and clang's C++ compute the same IEEE values, which is a different
claim from gfortran's Fortran: WW3 is built with gfortran (or ifx, nvfortran). flang defaults to
`-ffp-contract=fast` `(v, LangOptions.def: FPM_Fast)` and protects parentheses by default `(v)`,
so its IR must be produced with contraction off to say anything about the parity build. CPU IR
only; CUDA's NVVM path and its libdevice `expf` are out of scope.

## Cost of the proof

Building Alive2 for `alive-tv` needs Z3 and re2c; for the clang plugin it needs an LLVM built
from main with RTTI and exceptions `(v, README)`. Expect one to two weeks to a first result, plus
a flang build of the Fortran that WW3 itself has not been checked against ⚠. Maintenance:
Alive2 tracks LLVM main.

## Pros

- Reads compiler output, so nothing is hand-modelled.
- Built for exactly the "did this rewrite change the value" question of phase 2.
- Precise FP semantics, including `fmuladd` and fast-math flags.

## Cons

- Requires flang for the Fortran side, and WW3 is not built with flang here.
- Heavy build, coupled to LLVM main.
- No transcendentals, bounded loops, function-local only.

## Verdict

**Later**, for phase 2 rewrites in C++; the Fortran ↔ C++ step is cheaper with the GIMPLE diff.
