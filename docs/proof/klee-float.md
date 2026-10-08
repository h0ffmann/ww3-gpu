# KLEE-Float

Checked 2026-10-01: `srg-imperial/klee-float`, default branch
`tool_exchange_03.05.2017_rebase_extra_bug_fixes`, last commit 2022-02-15, CI configured for
LLVM 3.4 `(v, git log and .travis.yml)`. Upstream KLEE's licence is University of Illinois/NCSA
`(v)`. Not built. Plan: [`../BITWISE_PROOF_202610.md`](../BITWISE_PROOF_202610.md) §3 (d).

## What it is

A fork of the KLEE symbolic executor that keeps floating-point values symbolic and sends them to
Z3's FP theory, built for n-version comparison of floating-point programs ⚠ (from the authors'
ASE 2017 paper, not reread).

## Applied to ww3-gpu

In principle: run `cons_ref` and `cons_port` on a symbolic `KDMEAN` and ask for an input where
they differ, the same question as the exhaustive sweep. In practice nothing in this repo can be
fed to it: LLVM 3.4 bitcode readers reject what today's clang and flang emit ⚠.

## Wave model and physics fit

Path explosion on loops; the DIA's 720-iteration gathers would not finish ⚠. Transcendentals
reach the symbolic libm models of KLEE's uclibc ⚠.

## Bit for bit

Would cover C ↔ C++ under its own models. None of it is reachable with current compilers.

## Cost of the proof

Reviving an LLVM 3.4 toolchain or porting the fork forward: weeks of compiler archaeology with
no WW3 content. Not an undergraduate task.

## Pros

- The right idea: symbolic floats for n-version programs.

## Cons

- Unmaintained since 2022, on a compiler from 2013.
- Everything it would do, the exhaustive sweep, SMT queries and ESBMC already do.

## Verdict

**No.**
