# W3WAVE support routines translated to Kokkos; the two translation units joined on one branch

2026-10-16, the owner with agents. Issue #42,
[`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md),
branch `kimi/42-combined-translation` (the support unit came in as PR #144).

## Question

With the propagation package translated (the entry above), can the rows the profile has next,
the per-step support routines `W3WAVE` calls around the propagation and `DISTAB` at set-up, be
translated to the same standard: easiest first, no test run, bit-for-bit non-negotiable?

## What was done

- One session (Opus subagents, low to medium effort) translated `W3GATH` and `W3SCAT`
  (`wave_gath_scat.hpp`), `W3DZXY` (`updt_dzxy.hpp`), `W3UWND` (`updt_uwnd.hpp`) and `DISTAB`
  with `WAVNU2` (`disp_distab.hpp`), each a header-only device function, each against the `-O3`
  GIMPLE and object of its module built with WW3's CMake Release flags under `switch_lab_shrd`
  (byte-identical to the CMake build's inside the pinned `ww3` shell; outside it the same
  gfortran wrapper gave another `w3dispmd.o` (v)). A fresh reviewer per routine compared the
  objects. Nothing was run: no ctest, no regtest.
- The session hit its usage limit with three agents in flight: the `W3UWND` and `DISTAB/WAVNU2`
  reviews and the `W3UTRN` translation were stopped; `W3UTRN`'s header was left uncommitted and
  is not on the branch.
- The same day, a Kimi Code session cherry-picked this unit and the propagation unit onto one
  branch from `main`, wrote the six port reports and the `kokkos/ports.tsv` rows the plan had
  deferred, added the missing glossary entries (`W3DZXY`, `WAVNU2`) and regenerated the scores
  (`python3 scripts/port_score.py --write`, then `--check`).

## Result

- (v) `W3GATH`/`W3SCAT`: no defect in review; the copy loops do no float arithmetic, so the
  bits move unchanged; `MAPSF(:,3)` injective on a rectilinear grid (SMC and unstructured not
  checked ⚠).
- (v) `W3DZXY`: no bit change for finite inputs; three products commuted in the assembly either
  way the source is written, a NaN-payload difference only with a NaN grid metric ⚠.
- (v) `W3UWND`: the translator matched the object, `MOD` an inline x87 `fprem` loop in both
  compilers; its review never reported ⚠.
- (v) `DISTAB`/`WAVNU2`: the translator matched the object; the header compiles for CUDA, whose
  SASS has 87 fused multiply-adds not yet traced to CUDA's math library ⚠; its review never
  reported ⚠.
- The website's "translated" share moves from 99.0 % to 99.3 % of the profiled run time
  (`W3SCAT` 0.2 %, `W3GATH` 0.1 %, the rest below a gprof sample; `scripts/port_score.py`);
  "ported" (87.4 %) and "regtested" (48.4 %) do not move.

## What it changes

Before the unit's checkpoint: rerun the two stopped reviews, translate `W3UTRN`, compile the
three non-CUDA headers for CUDA and trace the 87 FFMA in `disp_distab.hpp`, then the shims and
the `W3WAVE` and `W3IOGR` hunks. The checkpoint itself is `just smoke-ww3-gpu`,
`just full-ww3-gpu` and the regtest matrix, as for propagation.
