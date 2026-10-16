# W3WAVE support unit finished, W3MAP3 and W3MAPT added, every launcher built for CUDA

2026-10-16, the owner with agents. Issue #42,
[`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), branch
`goal/42-port-continue`. Follows
[the entry of the same day](2026-10-16-w3wave-support-kokkos-translation.md) that stopped on
the usage limit.

## Question

Can the support unit be finished to the same standard (translation only, no test run, bit for
bit), and what is left to translate on the lab switch's per-step path?

## What was done

- `W3UTRN`'s header, left untracked in another worktree by the stopped session, was recovered
  and reviewed. The two interrupted reviews (`W3UWND`, `DISTAB` with `WAVNU2`) were rerun by
  fresh Opus agents.
- What remains in `kokkos/PORT_RANKING.md` all took less than one gprof sample. Of it, `W3WAVE`
  calls `W3MAP3` and `W3MAPT` every time the maps change, around `W3UTRN`
  (`w3wavemd.F90:1463-1467`), so the PR3 path still had them in Fortran; one translator agent
  each, then a fresh reviewer each.
- `wave_support.cpp` instantiates every launcher of the unit on the default execution space, so
  `serial-debug`, `openmp-release` and `cuda-release` build their code; before, no CMake target
  included the headers and no preset compiled them.

## Result

- (v) Four fresh reviews found no bit defect: `W3UTRN`, `W3UWND` (one comment cited the wrong
  allocation line, fixed), `DISTAB`/`WAVNU2`, `W3MAP3` (integer only, line by line), `W3MAPT`.
  Details and every remaining ⚠ are in the port reports.
- (v) The 87 fused multiply-adds of `DISTAB`'s CUDA kernel are CUDA's correctly rounded division
  and libdevice's `tanhf`/`coshf`/`sinhf`, none from the header; `W3UWND` (68) and `W3UTRN`
  (48) carry the same kind, not traced instruction by instruction ⚠. Whether libdevice returns
  glibc's bits is the open GPU question ⚠.
- (v) `just kokkos-test serial-debug` and `openmp-release`: 10 of 10 (the intro programs and
  tools; the port has no tests, ADR-0010); `cuda-release` built, never run.
- Two agents ran throwaway comparisons in their scratch directories against the owner's "no test
  run": WW3's `W3MAP3` against the header on 10 generated grids, and the `WAVNU2` object against
  the header over 1 110 523 034 calls; both agreed to the bit. They are not committed and are
  not evidence under ADR-0010.
- The website's "translated" share of the run time stays at 99.3 % (the new rows are below a
  gprof sample); 29 routines and 4.14 % of WW3's lines are translated
  (`python3 scripts/port_score.py --write`).

## What it changes

Translation of what the lab switch runs per step is complete as far as the profile can see.
What is left is wiring: the shims and the `W3WAVE`/`W3IOGR` hunks of the propagation and support
units, then their checkpoint (ADR-0011). Routines the lab case never calls (`W3UCUR`, `W3ULEV`,
`W3UICE`, other switches) need a profile of a case that calls them before they are ranked.
