# FIFO_EMPTY_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`FIFO_EMPTY` tests whether the ring queue of `PT_FLD`'s incremental flooding is empty. It is an
internal procedure of `PT_FLD` (`CONTAINS`, `w3partmd.F90:1095`), so its only callers are
`PT_FLD`'s fictitious-pixel test (`:974`) and its new-minimum labelling loop (`:1040`). `W3PART`
calls `PT_FLD` once per spectrum, `W3CPRT` calls `W3PART` once per sea point (`w3iosfmd.F90:278`).
`w3partmd.F90` is in WW3's core source list (`model/src/cmake/src_list.cmake:39`), so every switch
compiles it; it runs only when partitioned output is requested.

## What was ported

| | |
|---|---|
| WW3 routine | `FIFO_EMPTY`, `model/src/w3partmd.F90:1122-1133` at NOAA-EMC/WW3 `03adda91`, 12 lines, no `#ifdef` |
| Kokkos code | [`../src/ww_kokkos/part_fld.hpp`](../src/ww_kokkos/part_fld.hpp), `ww::part::pt_fld_detail::Fifo::empty()`, translated with `PT_FLD` ([`PT_FLD_KOKKOS.md`](PT_FLD_KOKKOS.md)); already a separate member function, not duplicated and not changed here |
| Translated before | yes, with `PT_FLD` (`kokkos/ports.tsv`, row `w3partmd:pt_fld`) |
| C shim | none; it is internal to `PT_FLD` and has no caller outside it |

The Fortran's `IQ_START` and `IQ_END` are `PT_FLD` locals reached by host association (`:920-921`);
in the C++ they are the members `start` and `end` of the `Fifo` object `pt_fld` builds (`:85` of the
header), both starting at 1 as the Fortran does. `IEMPTY` (an `INTENT(OUT)` integer, 0 or 1) becomes
a `bool` return: both call sites only test `IEMPTY .EQ. 1` (`:975`, `:1041`), and in the object
`IEMPTY` is never stored, so no value of it is observable.

## Arithmetic

Reference: `w3partmd.F90` recompiled with the flags of the `switch_lab_shrd` CMake Release build
(`ref2/build_lab`, gfortran 16.2.0 in the pinned shell); its disassembly is identical to the CMake
object's (`cmp`, 2026-10-16). C++ throwaway unit calling `ww::part::pt_fld`, openmp-release flags,
g++ 16.2.0, warning-free.

- (v) `FIFO_EMPTY` has no symbol of its own in the Fortran object: `-O3` inlines it into
  `__w3partmd_MOD_pt_fld` at three sites (`DEBUG INLINE_ENTRY fifo_empty`, one at `:974` and the
  loop at `:1040` peeled into two). Each is one integer comparison
  `if (iq_end != iq_start)` / `if (iq_start != iq_end)` (`:1126`). The C++ GIMPLE has the same three
  sites, `if (fifo$end == fifo$start)` at the `:974` site and two at the `:1040` site, each a single
  integer equality on the same two values. Equality of two `int`s is symmetric, so the operand
  order cannot change a result.
- (v) No float operation, no libm call, no memory access: both queue indices stay in registers in
  both objects.
- (v) A full ring (`IQ_END` wrapped onto `IQ_START`) reads as empty in both, as the ring has exactly
  `NSPEC` slots (`:899`, `:1110`, `:1149`); `PT_FLD_KOKKOS.md` argues it cannot fill when
  `NSPEC > 9`.
- No `#ifdef` variant: the routine has none.
- ⚠ Not compiled for CUDA in this check; `PT_FLD_KOKKOS.md` records the CUDA compile of the same
  header.
- (v) Fresh review 2026-10-16: no defect. `w3partmd.F90` recompiled from `ref2/build_lab`'s flags, disassembly identical to the CMake object; its three inlined `fifo_empty` sites (one integer `!=` each) compared with the three `fifo$end == fifo$start` sites of the C++ `pt_fld` GIMPLE and `Fifo::empty` out of line (`cmp`, `sete`). Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
