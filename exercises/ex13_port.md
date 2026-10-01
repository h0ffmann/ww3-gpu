# Exercise 13 — replay `ww3_ts1` through the ported kernel and read the table

**Lessons:** [12 — porting a kernel: W3SNL1](../course/12-porting-a-kernel-w3snl1.md),
[13 — bulk porting with agents](../course/13-bulk-porting-with-agents.md).
**Time:** ~45 min. **Solution:** [`solutions/ex13_compare.sh`](solutions/ex13_compare.sh).

## Goal

Run the validation ladder's second rung yourself. The L1 tests in `kokkos/tests/` say the
Kokkos `W3SNL1` reproduces the Fortran on captured inputs; the L2 replay says the *model*
reproduces itself when the kernel is swapped at run time. You produce the table the
replay appends to `kokkos/PORT_STATUS.md` and a reading of it.

## Steps

1. **The reference run.** `just rt ww3_ts1 ST4`: source terms in isolation, so `W3SNL1` is a
   large share of the work and any drift shows in `hs` and `fp`. The second argument
   names the switch file: `ww3_ts1/input/` ships `switch_ST1`…`switch_ST6` (no
   `switch_PR3_UQ`, the `just rt` default), and `ST4` is the physics the port targets.

2. **The tools.** `just kokkos-build openmp-release` builds `nccmp-tol`; the runtime
   switch lives in the fork's patched `ww3_shel` (see `kokkos/src/fortran_iface/PATCH.md`; without it both
   runs take the Fortran path, and step 3 proves the harness rather than the kernel,
   which is still the first thing to establish).

3. **The replay.** `bash kokkos/tests/L2_replay.sh $WW3 ww3_ts1` copies `work_lab/` to
   `work_a/` and `work_b/`, runs `ww3_shel` with `WW_KOKKOS_SNL1=0` and `=1`, converts
   both with `ww3_ounf`, and runs `nccmp-tol` with the default tolerance table.

4. **Read the table.** nccmp-tol prints one row per variable, columns `variable  n
   dropped  max|d|  rms  max rel  verdict`, where the verdict is `PASS`, `FAIL`,
   `FAIL (n=0)` (judged but nothing left to compare, a failure) or `unlisted` (reported,
   not judged), then a summary line `nccmp-tol: PASS|FAIL (k judged, m unlisted)`.
   Then answer, in writing:
   - Which fields are judged and which merely reported? Why is `dir` given a looser
     absolute tolerance than `hs`?
   - Is `max rel` on `hs` zero, ~1e-7, ~1e-5, or larger? What does each of those
     say about the port (bit-identical; float32 rounding; reduction order or contraction;
     a bug)?
   - If the two runs are identical to the last bit, what have you *not* shown?

## What to hand in

`out/ex13/l2_table.md`, your answers, and the one line the solution script prints
(`parity holds` / `outside tolerance`).

## Going further

- Loosen `kokkos/tools/nccmp-tol/tolerances.txt` until a deliberately broken kernel
  passes (edit a coefficient in `snl1_dia.cpp`, rebuild, replay). How loose is too
  loose? That number is the argument you will have with whoever accepts the port.
- Replay `ww3_tp2.2` as well. `W3SNL1` is a smaller share there; does the table say
  anything at all?
- Lesson 13's recipe: pick the next routine on the ranked list in
  `docs/AGENTS_KOKKOS_202609.md` and write (not run) the L1 fixture generator and the
  L2 replay entry it would need. That is a port PR's skeleton.
