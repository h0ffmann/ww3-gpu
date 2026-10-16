# UOST_SRCTRMCOMPUTE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Public entry of the unresolved-obstacles source term (switch `UOST`): `W3SRCE` calls it once per
sea point (`w3srcemd.F90:1361-1362`) and it forwards to the type-bound `SRCTRM%COMPUTE`. It has no
arithmetic of its own.

## What was ported

| | |
|---|---|
| WW3 routine | `UOST_SRCTRMCOMPUTE`, `model/src/w3uostmd.F90:298-343` at NOAA-EMC/WW3 `03adda91` (46 lines); compiled by the `UOST` regtest switches of `ww3_tp2.21` (3) and `ww3_ts4` (6) (`git grep -lw UOST -- '*switch*'`), by no NOAA operational switch in `model/bin`. Its only `#ifdef` block is `W3_S` (`STRACE`, output only) |
| Kokkos code | [`../src/ww_kokkos/uost_uost_srctrmcompute.hpp`](../src/ww_kokkos/uost_uost_srctrmcompute.hpp), `ww::uost::uost_srctrmcompute`, a template over a callable `compute` that stands for `SRCTRM%COMPUTE` |
| Not translated here | the callee `UOST_SOURCETERM_COMPUTE` (`:1056-1128`) and through it `_COMPUTE_LD`, `_COMPUTE_SE`, `COMPUTE_REDUCTION_PSI`: no `uost_*.hpp` header existed when this one was written, so the caller passes the callee |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3uostmd.F90` in WW3's CMake Release build, gfortran 16.2.0,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8` (build `mixa`), 2026-10-16.

- (v) The module recompiled from the build's `flags.make` gives a disassembly identical to the
  build's `w3uostmd.F90.o` (`cmp`).
- (v) The object's `__w3uostmd_MOD_uost_srctrmcompute` builds the class container of `SRCTRM` on
  the stack and makes one indirect call through vtable slot `+0x38`; the relocations of
  `.data.rel` put `__w3uostmd_MOD_uost_sourceterm_compute` in that slot. `SRCTRM` is only ever
  allocated with its declared type (`:274`), so the dispatch always reaches that routine.
- (v) No floating-point instruction, libm or libmvec call, reduction, or MIN/MAX in the
  Fortran object; the C++ unit instantiated with an external callee compiles (openmp-release
  flags, warning-free) to a single tail jump with the arguments in place.
- ⚠ CUDA not compiled.
- ⚠ In the callee, when `GRD%UOSTENABLED` is false, `S = 0` and it returns without writing the
  `INTENT(OUT)` `D` (`w3uostmd.F90:1114-1117`); `W3SRCE` zeroes `VDUO` before the call
  (`w3srcemd.F90:920`), so the value used is 0 in practice. A translation of the callee must keep
  `d` unwritten on that path.
- ⚠ If `UOST_SETGRID` (`w3initmd.F90:545`, `w3wavemd.F90:700`) has not run, `SRCTRM` is
  unallocated and the vtable read at `:342` dereferences an unset pointer.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3uostmd.F90` from build `mixa`'s `flags.make`: disassembly identical to the build's object; its GIMPLE makes one call through `_vptr->compute` with the nine dummies in order, and `.rela.data.rel` puts `uost_sourceterm_compute` at vtable `+0x38`; the C++ unit (openmp-release flags) is one tail jump. Callers, line cites and the 9 `UOST` regtest switches re-read at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
