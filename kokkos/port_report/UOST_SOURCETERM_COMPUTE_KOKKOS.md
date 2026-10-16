# UOST_SOURCETERM_COMPUTE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The `COMPUTE` method of the unresolved-obstacles source term (switch `UOST`), reached from
`UOST_SRCTRMCOMPUTE` (`w3uostmd.F90:342`) through the vtable of `SRCTRM`. When
`GRD%UOSTENABLED` is false it sets `S = 0` and returns; otherwise `S` and `D` are the sums of the
local dissipation (`COMPUTE_LD`) and the shadow effect (`COMPUTE_SE`).

## What was ported

| | |
|---|---|
| WW3 routine | `UOST_SOURCETERM_COMPUTE`, `model/src/w3uostmd.F90:1056-1128` at NOAA-EMC/WW3 `03adda91` (73 lines); compiled by the `UOST` regtest switches of `ww3_tp2.21` (3) and `ww3_ts4` (6) (`git grep -lw UOST -- '*switch*'`), by no NOAA operational switch in `model/bin`. Its only `#ifdef` blocks are `W3_S` (`STRACE`, output only) |
| Kokkos code | [`../src/ww_kokkos/uost_uost_sourceterm_compute.hpp`](../src/ww_kokkos/uost_uost_sourceterm_compute.hpp), `ww::uost::uost_sourceterm_compute`, a template over two callables standing for `THIS%COMPUTE_LD` and `THIS%COMPUTE_SE` |
| Structural change | the four automatic arrays `S_LD`, `S_SE`, `D_LD`, `D_SE` (`:1105-1106`), which the object `malloc`s and `free`s on every call, are caller-provided work arrays |
| Not translated here | the callees `UOST_SOURCETERM_COMPUTE_LD` (`:793-898`), `UOST_SOURCETERM_COMPUTE_SE` (`:920-1034`) and through them `COMPUTE_REDUCTION_PSI` (`:688-770`): no header for them existed when this one was written, so the caller passes them |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3uostmd.F90` in WW3's CMake Release build, gfortran 16.2.0,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8` (build `mixa`), 2026-10-16.

- (v) The module recompiled from the build's `flags.make` gives a disassembly identical to the
  build's `w3uostmd.F90.o` (`cmp`).
- (v) The two calls are indirect, through vtable slots `+0x50` and `+0x48`; the `.data.rel`
  relocations put `_compute_ld` and `_compute_se` there. `SRCTRM` is only allocated with its
  declared type (`:274`), so the dispatch always reaches those two routines.
- (v) Control flow matches the object: disabled, `memset(S)` only when `NSPEC > 0` and `D`
  untouched; enabled, `S_LD` and `S_SE` zeroed (when `NSPEC > 0`), `COMPUTE_LD` then
  `COMPUTE_SE` called also when `NSPEC <= 0`, then `S = S_LD + S_SE` and `D = D_LD + D_SE`, one
  `addss`/`addps` per element. The C++ unit (openmp-release flags, external callees, warning-free)
  has the same calls in the same order and one add per element.
- (v) Operand order of the add: the Fortran object puts `S_LD` (`D_LD`) first in both the vector
  body and the scalar tail. g++ swapped the operands in its vector body (`S_SE + S_LD`), which
  differs from the Fortran when both operands are NaN (x86 returns the first operand's NaN). The
  two loops carry `#pragma GCC novector` (g++ 14 or later, host only); the object then has one
  scalar `vaddss` per element with `S_LD` (`D_LD`) first.
- (v) No libm or libmvec call, reduction or MIN/MAX in this routine.
- ⚠ When `UOSTENABLED` is false, `D` is `INTENT(OUT)` and is not written (`:1114-1117`); the
  translation keeps `d` unwritten. `W3SRCE` zeroes `VDUO` before the call (`w3srcemd.F90:920`).
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: the module recompiled from `build_mixa` flags gives an object
  identical to the build's (`cmp`); the Fortran's control flow, calls (vtable `+0x50` then
  `+0x48`, `S_LD`/`D_LD` and `S_SE`/`D_SE` as their `S`, `D`), zeroing and adds compared with the
  C++ unit. Found g++'s vector body adding `S_SE + S_LD` (NaN operand order); fixed with
  `GCC novector` on the two sum loops and read again in the object. `COMPUTE_LD` and `COMPUTE_SE`
  write all of `S` and `D` (`:853-854`, `:983-984`) before any early return, so `D_LD`, `D_SE` are never read undefined.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
