# FIFO_ADD_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`FIFO_ADD` pushes one spectral bin onto the ring queue `IQ(NSPEC)` that `PT_FLD` uses for the
incremental flooding of the watershed partitioning (`w3partmd.F90:1103-1113`). It is an internal
procedure of `PT_FLD` (`CONTAINS`, `:1095`), called only from there (`:950`, `:966`, `:978`,
`:1012`, `:1034`, `:1047`). `W3PART` calls `PT_FLD`; `W3CPRT` calls `W3PART` once per sea point
for partitioned output (`w3iosfmd.F90:278`). `w3partmd.F90` is in WW3's unconditional source list
(`model/src/cmake/src_list.cmake:39`), so every switch compiles it; it runs only when a regtest
asks for partitioned output ⚠ (the entries were not enumerated).

## What was ported

| | |
|---|---|
| WW3 routine | `FIFO_ADD`, `model/src/w3partmd.F90:1103-1113` at NOAA-EMC/WW3 `03adda91`, 11 lines, no `#ifdef` |
| Kokkos code | [`../src/ww_kokkos/part_fld.hpp`](../src/ww_kokkos/part_fld.hpp), `ww::part::pt_fld_detail::Fifo::add`, translated with `PT_FLD` ([`PT_FLD_KOKKOS.md`](PT_FLD_KOKKOS.md)); already a separate member function, so no code was written or moved for this report |
| Module state it reads | `IQ`, `IQ_END` (locals of `PT_FLD`, `:899`, host-associated) and `NSPEC` (`W3GDATMD`), held in the `Fifo` struct as `iq`, `end` and `nspec` |
| C shim | none; it is reached only through `pt_fld` |

## Arithmetic

Reference: the `-O3` object of `w3partmd.F90` recompiled with the flags of WW3's CMake Release
build under `switches/switch_lab_shrd` (gfortran 16.2.0, pinned shell), disassembly identical to
the CMake object, read 2026-10-16.

- (v) gfortran inlines `FIFO_ADD` at all six call sites; the object has no symbol of its own (only
  `__w3partmd_MOD_pt_fld`). The GIMPLE at each site is the same three steps: store `IV` at
  `IQ[IQ_END-1]` (`:1107`), `IQ_END + 1` (`:1109`), and a signed `>` against `NSPEC` that resets to 1
  (`:1110`, `cmovg` with the constant 1 in the assembly).
- (v) A throwaway unit with `Fifo::add` out of line, compiled with the openmp-release flags (no
  warning), gives the same sequence: store at `end-1` (`cltq` index), `add $1`, `cmp` against
  `nspec`, `cmovg` 1. Integer only; `end <= NSPEC` before the add, so `end + 1` cannot overflow.
- (v) `NSPEC` is read from module memory in the Fortran (`-O3` hoists the load out of the loops, e.g.
  before the `:946` loop) and from the struct in the C++; it does not change during `PT_FLD`, so the
  value is the same.
- The ring holds exactly `NSPEC` slots and a full ring reads as empty, as in the Fortran; why it
  cannot fill when `NSPEC > 9` is argued in [`PT_FLD_KOKKOS.md`](PT_FLD_KOKKOS.md).
- ⚠ CUDA: compiled with `PT_FLD` (its report), never run.
- (v) Fresh review 2026-10-16: no defect. `w3partmd.F90` recompiled from `ref2/build_lab`'s flags, disassembly identical to the CMake object; the six inlined `fifo_add` sites of its `-O3` GIMPLE (store at `IQ_END-1`, `+1`, signed `>` `NSPEC` to 1) compared with `Fifo::add` out of line and inlined in `pt_fld` (openmp-release flags, warning-free): same operations, call order in `pt_fld` matches `:950`-`:1047`. Fixed the report's wording on where `NSPEC` is loaded; the header was not changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
