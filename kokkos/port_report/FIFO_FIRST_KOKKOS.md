# FIFO_FIRST_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Pops the oldest point of the ring queue `PT_FLD` uses to flood one level of the watershed
partitioning: `IV = IQ(IQ_START)`, then `IQ_START` advances and wraps at `NSPEC`. It is an internal
subroutine of `PT_FLD` (author B. Tracy), called at `w3partmd.F90:969`, `:980` and `:1042`; `PT_FLD`
is called only by `W3PART`, which `W3CPRT` (`w3iosfmd.F90:278`), `ww3_outp` (`ww3_outp.F90:1910`)
and `ww3_ounp` (`ww3_ounp.F90:2067`) call for partitioned output. `w3partmd.F90` is in every build
(`model/src/cmake/src_list.cmake:39`); no switch selects it.

## What was ported

| | |
|---|---|
| WW3 routine | `FIFO_FIRST`, `model/src/w3partmd.F90:1142-1152` at NOAA-EMC/WW3 `03adda91`, internal to `PT_FLD`; no `#ifdef` in its lines, so it has no variant |
| Kokkos code | [`../src/ww_kokkos/part_fld.hpp`](../src/ww_kokkos/part_fld.hpp), `ww::part::pt_fld_detail::Fifo::first()`, a `KOKKOS_INLINE_FUNCTION` member already written with `PT_FLD` ([`PT_FLD_KOKKOS.md`](PT_FLD_KOKKOS.md)); not duplicated, not changed. `Fifo` holds `IQ` (caller scratch, `NSPEC` slots), `NSPEC` and the host-associated `IQ_START`, `IQ_END` |
| C shim | none; it runs only inside `pt_fld` |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3partmd.F90` recompiled with the flags of WW3's CMake Release
build (`switch_lab_shrd`, gfortran 16.2.0, pinned shell); its disassembly is identical to the CMake
build's `w3partmd.F90.o`, checked by the translator, 2026-10-16.

- (v) gfortran emits no `fifo_first` symbol: the object's only symbol of this part is
  `__w3partmd_MOD_pt_fld`, and the optimised GIMPLE inlines `FIFO_FIRST` at its three call sites
  (`:1146` load of `IQ(IQ_START)` with index `IQ_START-1` in 64 bits, `:1148` `IQ_START+1`,
  `:1149` the wrap to 1 when greater than `NSPEC`). The C++ unit, `Fifo::first()` behind a
  non-inlined wrapper (openmp-release flags, warning-free), does the same: load `iq[start-1]`,
  `start+1`, `cmovg` to 1 when greater than `nspec`.
- (v) Integer only: no float, no libm call, no reduction, no MIN/MAX. For `1 <= IQ_START <= NSPEC`
  the 32-bit `start-1` of the C++ and the 64-bit `IQ_START-1` of the Fortran give the same index.
- (v) It is a separate function in the C++ already, so no factoring out was needed and the
  `PT_FLD` translation was not touched.
- ⚠ CUDA not compiled for this record; `PT_FLD_KOKKOS.md` records a CUDA compile of the whole
  header (`sm_89`, `--fmad=false`), never run.

## What WW3 does here at the pin

No defect found. `FIFO_FIRST` does not check for an empty queue; at `:969` the queue holds at least
the fictitious pixel added at `:966` or `:978`, at `:980` `FIFO_EMPTY` (`:974`) has just found it
non-empty, and at `:1042` `FIFO_EMPTY` is tested first (`:1040-1041`). The translation keeps the same contract.
- (v) Fresh review 2026-10-16: no defect. `w3partmd.F90` recompiled from `ref2/build_lab`'s flags, disassembly identical to the CMake object; its three inlined `fifo_first` sites (load at `IQ_START-1`, `+1`, signed `>` `NSPEC` to 1) compared with `Fifo::first` out of line (`movslq` index, `cmovg` 1) and inlined in `pt_fld`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
