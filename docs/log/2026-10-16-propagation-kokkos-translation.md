# Propagation (PR3 with UQ) translated to Kokkos, six routines, nothing run

2026-10-16, the owner with an agent. Issue #42, [`PROPAGATION_KOKKOS_PLAN_202610.md`](../PROPAGATION_KOKKOS_PLAN_202610.md),
branch `claude/42-port-propagation`.

## Question

The owner asked to translate what is left, the easiest first, with subagents on Fable at medium
effort, no test run and no test written, and bit-for-bit parity non-negotiable. Can the
propagation package of the lab switch be translated to that standard without running anything?

## What was done

- What is left above 10 ms in `kokkos/PORT_RANKING.md` is the propagation of `switch_lab_shrd`
  (`PR3`, `UQ`): `W3XYP3`, `W3QCK3`, `W3QCK1`, `W3KTP3` (v). `WAVNU1` was added as the simplest,
  and `W3QCK2` when the W3KTP3 parser found that `W3KTP3` calls it for the wavenumber pass (v); the
  plan had left it out.
- The `-O3` objects of `w3uqckmd.F90`, `w3pro3md.F90`, `w3dispmd.F90` were rebuilt with WW3's CMake
  Release flags and `-fdump-tree-optimized` (two confirmed byte-identical to the CMake build's,
  the third by its parser). Five read-only parsers (Fable, medium) wrote one spec each; six
  translators wrote one header-only device function each (`prop_*.hpp`, serial, the Fortran's
  structure and order); four fresh reviewers compared the C++ objects with the Fortran objects; a
  fifth checked one fix. The session ran on the owner's personal account; the account rule was
  removed by the owner the same day (PR #141).
- Nothing was run: no ctest, no regtest. The three presets compiled the headers through
  `prop.cpp`.

## Result

- (v) No libmvec call in any of the six; no FMA; the only libm calls are `lroundf` (`W3XYP3`) and
  an inline `sqrtf` (`WAVNU1`). Every `MIN`/`MAX` takes the operand order of the object's
  `minss`/`maxss`, read from the assembly, in the vector body and in every scalar copy.
- (v) The reviewers found no defect that changes bits on a reachable input. They found one
  NaN-only difference, the vectorised `CFLKMAX` maximum of `W3KTP3`, now translated with the
  object's lane grouping, and cosmetic defects in comments and `static_assert` messages, fixed.
- ⚠ Parity is static. Nothing calls the functions yet, so the compilers did not generate their
  code; the CUDA path is compiled for syntax only. `W3XYP3` keeps an out-of-bounds write of the
  Fortran (one element below `VQ` for sea at row 1, column 1), and the maps must be copied from
  the Fortran because `W3MAP3` reads `MAPSTA` out of bounds.
- The website's "translated" share moves from 93.7 % to 99.0 % of the profiled run time
  (`scripts/port_score.py`); "ported" (87.4 %) and "regtested" (48.4 %) do not move.

## What it changes

What is left in Fortran above one gprof sample is `init` (0.65 %), the MPI gather and scatter
and the set-up routines. The propagation needs a shim (the module maps and tables copied at
init, one spectral component or one sea point per call) and the `W3WAVE` hunks, then its
checkpoint: `just smoke-ww3-gpu`, `just full-ww3-gpu` and the regtest matrix on the `PR3` `UQ`
entries.
