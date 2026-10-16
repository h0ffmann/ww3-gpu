---
kind: fact
abstract: "How the ST4 package of WW3 (W3SDS4, W3SIN4, W3SPR4, CALC_USTAR, INSIN4, TABU_*) was ported to Kokkos as one unit under ADR-0011 on 2026-10-15: about 2.6 hours from plan to merged PR against 30 days for W3SNL1; the checkpoint so far byte-identical on ww3_ts1 and bench_small; the lessons (libmvec at -O3, compile-time folding, MIN/MAX operand order from the assembly, uninitialised LLWS in WW3); and the next ports by wall time."
entities:
- st4
- w3sds4
- w3sin4
- w3spr4
- w3srce
- kokkos
- adr-0011
- libmvec
- gimple
- w3xyp3
tags:
- publish
- port
- st4
- kokkos
tier: semantic
type: Fact
generated:
  by: process:ai-memory/3.0.0
  at: 2026-10-16T02:15:29Z
---
# The ST4 Kokkos port: how it went, what it cost, what it taught, what comes next

A lead, not evidence: every number below has its record in the repository, named in brackets.

## What was done

- **The unit.** All of ST4 in `w3src4md.F90` (NOAA-EMC/WW3 `03adda91`): `W3SDS4`, `W3SIN4`,
  `W3SPR4`, `CALC_USTAR`, `INSIN4`, `TABU_STRESS`, `TABU_TAUHF`, `TABU_TAUHF2`, eight routines,
  2 419 Fortran lines, 39.0 % of the `bench_small` profile's self time (`kokkos/ports.tsv`,
  `kokkos/PORT_RANKING.md`). Phase 1: one per-point device function per routine, copied in and
  out per call, behind `WW_KOKKOS_SDS4/SIN4/SPR4`, all off by default. PR #135, issue #45, plan
  `docs/ST4_KOKKOS_PLAN_202610.md`, log `docs/log/2026-10-15-st4-kokkos-translation.md`.
- **The method (ADR-0011).** Port in units, regtest at checkpoints. A read-only subagent wrote
  the `-O3` inventory of the Fortran first; a common layer fixed every signature; one subagent per
  routine translated in parallel in its own worktree; fresh reviewers compared the C++ with the
  Fortran's optimised GIMPLE and assembly; the fork wiring (`ww3-gpu/kokkos-st4`, `c9a6a0c6`) ran
  in parallel against the same C ABI.

## What it cost

- **Wall clock.** The plan was committed at 22:12 UTC on 2026-10-15 and PR #135 merged at
  00:48 UTC on 2026-10-16: about 2.6 hours from plan to merge for eight routines, against the
  plan's 8 hours, and against 30 days for `W3SNL1` and `INSNL1` (ADR-0011's table). Read from the
  commit and PR timestamps, not timed.
- **The checkpoint**, 2026-10-15 evening, CPU, the owner's i9-14900 (log
  `docs/log/2026-10-15-st4-checkpoint.md`): `just smoke` and `smoke-ww3-gpu` on `ww3_ts1` and
  `full-ww3-gpu` on `bench_small`, each flag alone, then all three, then with `SNL1`: every
  Kokkos run byte-identical to the Fortran of the same executable, and `nccmp-tol` PASS against
  `--ww3`. The 137-entry ST4 matrix was still running when this page was written.
- Phase 1 is slower than the Fortran on the full case: one copy in and out per point per call.
  No timing is quoted: no number before the matrix passes and three runs are taken.

## Lessons

1. **Read the build, not the source.** gfortran `-O3` calls glibc's libmvec
   (`_ZGVbN4vv_powf`, `_ZGVbN4v_expf`) in five ST4 loops, in groups of four, and libmvec differs
   from scalar libm by up to 3 ULP; the port calls the same functions with the same grouping.
   gfortran also folds `TANH(1.01**-999)` at compile time to a value that differs from the run
   time's. Both are invisible in the source.
2. **MIN/MAX operand order comes from the assembly.** GCC can swap the operands of a
   `MIN_EXPR`/`MAX_EXPR` after GIMPLE; `maxss a,b` returns `a` on a NaN or a `+0`/`-0` tie. The
   W3SRCE review found ten sites where the GIMPLE order was wrong, one reachable (W3SLN1 gave NaN
   where WW3 gives 0).
3. **WW3 has undefined behaviour of its own.** With `TAUWBUG = 0` and `IT > 0`, `W3SRCE` reads its
   local `LLWS` before setting it. That cannot be translated; the Kokkos W3SRCE leaves the case
   to the Fortran.
4. **Parallel subagents work when the interfaces come first.** The common layer, written before
   any routine, let eight translations and the fork wiring proceed at once and cherry-pick
   cleanly.
5. **Static parity is a filter, not a verdict.** The GIMPLE and assembly comparison found real
   defects before any regtest ran; the checkpoint is still what decides.

## Next

1. Finish the ST4 checkpoint: the 137-entry matrix with `WW3GPU_ROUTINES=SDS4,SIN4,SPR4`, strict;
   then the ST4 rows move to `regtested`.
2. `W3SRCE` with `W3SLN1`, `W3SBT1`, `W3SDB1` is translated (PR #136, fork
   `ww3-gpu/kokkos-srce`); its checkpoint needs `SRCE` in `WW3GPU_ROUTINES` and the pin moved.
3. By wall time, the next unit is propagation: `W3XYP3` (2.98 %), `W3QCK3` (1.14 %), `W3QCK1`
   (0.91 %), `W3KTP3` (0.30 %) (`kokkos/PORT_RANKING.md`).
4. Phase 2: keep the spectrum resident on the device; only then do the GPU numbers mean anything,
   and a GPU run stays the owner's, supervised.
