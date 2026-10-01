# GIMPLE diff: gfortran against g++

Checked 2026-10-01 with GCC 13.3. No new tool. Plan: [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §3 (c).

## What it is

gfortran and g++ are two front ends on one middle end. `-fdump-tree-optimized` prints each
function as GIMPLE after all middle-end optimisations, with every floating-point operation, libm
call and fused multiply-add spelled out. If the Fortran routine and its port produce the same
sequence of IEEE operations on the same operands, they produce the same bits, because each
operation is correctly rounded and therefore deterministic.

## Applied to ww3-gpu

- `W3SNL1` section 1, done by hand: both dumps contain the same twelve operations in the same
  order (mul, max, mul, max, div, mul, sub, mul, `__builtin_expf`, mul, add, mul) `(v, measured)`.
  The differences are SSA names, commuted operands of `*` and `+` (exact in IEEE), and gfortran's
  `MAX_EXPR` against the port's compare-and-select, which is where the NaN behaviour of the
  exhaustive sweep comes from.
- With `-march=x86-64-v3`, gfortran's dump shows `.FMA` and `.FNMA` `(v, measured)`, so the diff
  catches contraction before any test runs.
- Next: the section 3 and 4 loop bodies. Kokkos lambdas inline into large functions, so the
  practical unit is a literal C or Serial-only C++ version of a routine, which is the C stage the
  FESOM2 recipe keeps (`course/13`).
- `x**n`: gfortran's integer-power chain against `std::pow` shows up as a different call or
  sequence, the bug lesson 12 found by testing `(v, kokkos/README.md)`.

## Wave model and physics fit

Any routine both compilers can build: loops, module state, libm calls. It needs the C++ to be a
literal translation; a restructured kernel produces a different graph even when it is correct.

## Bit for bit

Fortran ↔ C++ on CPU, under GCC only. The argument stops at GIMPLE: register allocation and
instruction selection come after, and are assumed not to change values (true on x86-64 SSE
⚠, not proved). Nothing for CUDA or clang/flang. Calls to `expf` match by name, not by behaviour.

## Cost of the proof

Half a day to script the normalisation (rename SSA values, sort commutative operands) and the
diff; then minutes per routine. Maintenance: GIMPLE text changes between GCC versions, so pin the
compiler, which the nix shell already does. Free.

## Pros

- Cheapest translation validation available: the compiler does the extraction.
- Shows contraction, `pow` lowering and reassociation directly.
- Scales to every routine both compilers can build.

## Cons

- Structural: any legitimate reordering fails the diff and needs an SMT query to settle.
- Trusts GCC's back end and the dump's fidelity.
- gfortran ↔ g++ only.

## Verdict

**Now**, as pilot step 2 and then as a CI check on the literal stage of each new kernel.
