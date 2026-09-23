# Exercise 11 — refactor a WW3-style routine, and prove you did not change it

**Lesson:** [10 — modern Fortran refactoring](../course/10-modern-fortran-refactoring.md).
**Time:** ~60 min. **Solution:** [`solutions/ex11_refactor.F90`](solutions/ex11_refactor.F90),
[`solutions/ex11_refactor_test.F90`](solutions/ex11_refactor_test.F90).

## Goal

Take a routine written the way most of WW3 is written and rewrite it the way lesson 10
asks, without changing its answer. Then write the test that says so: the smallest
instance of the parity discipline the whole port runs on.

The legacy routine (`w3sds_old` in the solution file, forty lines) is a Komen-type
whitecapping term in the shape of WW3's `W3SDS1`: from the action spectrum it forms a
mean energy and mean frequency, a steepness-like parameter, and a dissipation rate that
multiplies the spectrum. It has every habit the lesson names:

- an **external subroutine**: no module, so callers get an *implicit interface* and the
  compiler checks neither argument count nor type;
- a **flattened 1-D spectrum** indexed by hand, `isp = ith + (ik-1)*nth`;
- dummy arguments with **no `intent`**;
- an **automatic array** `e1(nk)` on the stack, sized by an argument.

## Steps

1. Read `w3sds_old` until you can say in one sentence what each loop computes. Write
   that sentence as a comment; it is the specification your rewrite must meet.

2. Write `w3sds_new` in a **module**: `pure`, `intent` on every argument, the spectrum
   as an assumed-shape `(theta, k)` array declared `contiguous`, and **no automatic
   array**: the band sum is a scalar. Keep every expression and every summation order
   identical; the point is the interface and the memory, not the arithmetic.

3. Write the test: a WW3 default spectral grid (32 bands from 0.04118 Hz, ratio 1.1,
   24 directions), random action densities from `random_number` after
   `random_init(repeatable=.true., image_distinct=.false.)`, both routines, and the
   maximum relative difference of `S` and `D`. Pass if ≤ 1e-6. The test must declare an
   `interface` block for the legacy routine: that is the only way `-Wall` can check the
   call, and noticing that is half the lesson.

4. Build with the strict flags and run:

   ```bash
   gfortran -O2 -std=f2018 -Wall -Wextra -fimplicit-none -o ex11_test ex11_refactor.F90 ex11_refactor_test.F90
   ./ex11_test
   ```

   or `cmake -S exercises/solutions -B exercises/solutions/build && cmake --build … && ctest …`.

## What to hand in

The two files, the test's output, and the answer to: your test passed with a difference
of exactly zero. What would you have to change for it to be 1e-7 instead, and would
that still be "the same routine"?

## Going further

- Add `-fcheck=all` and make `nk` disagree between the call and the array: the legacy
  version cannot notice, the module version cannot compile. That is what an explicit
  interface buys.
- Make the theta loop a `do concurrent` (`gpu/02_do_concurrent.f90` shows the syntax and
  the `LOCAL()` caveat). Does the parity test still pass at `-O3`? With `-ffast-math`?
