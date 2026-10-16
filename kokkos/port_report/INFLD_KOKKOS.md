# INFLD_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Initialisation of the Reichl flux parametrisations (switches `FLD1`, `FLD2`): copies the grid's
spectral tail parameters (`TAIL_ID`, `TAIL_LEV`, `TAIL_TRAN1`, `TAIL_TRAN2` of `W3GDATMD`) into the
`SAVE` variables of `W3FLD1MD` that `APPENDTAIL` and `W3FLD1`/`W3FLD2` read. No arithmetic.

## What was ported

| | |
|---|---|
| WW3 routine | `INFLD`, `model/src/w3fld1md.F90:781-875` at NOAA-EMC/WW3 `03adda91` (95 lines); state declared at `:76-88`; its only `#ifdef` blocks are `W3_S` tracing |
| Callers | `W3FLD1` (`w3fld1md.F90:258-261`) and `W3FLD2` (`w3fld2md.F90:231-234`), each on its own first call (`FIRST`, `SAVE`, `threadprivate`) |
| Compiled by | `FLD1`: `ww3_ts3/input/switch_ST4_PR1_FLD1(_MPI)`, `switch_ST6_PR1_FLD1(_MPI)`; `FLD2`: `ww3_ts3/input/switch_ST4_PR1_FLD2(_MPI)`, `switch_ST6_PR1_FLD2(_MPI)`, `ww3_ufs1.1/input/switch_MPI`, `switch_MPI_OMPH`, `ww3_ufs1.1/input_unstr/switch_PDLIB`, `switch_PDLIB_TRNK`, `ww3_ufs1.2/input/switch_MPI_OMPH`, `ww3_ufs1.3/input/switch_MPI_OMPH`, `model/bin/switch_multi_esmf` (`git grep -lw FLD1 -- '*switch*'`, same for `FLD2`); no `switch_NCEP_*` or `switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/fld1_infld.hpp`](../src/ww_kokkos/fld1_infld.hpp), host function `ww::fld1::infld(tail_id, tail_lev, tail_tran1, tail_tran2, TailState&)`; the module state is the struct `TailState` |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3fld1md.F90` recompiled with the flags of WW3's CMake Release
build (gfortran 16.2.0, pinned shell), identical to the CMake object of `build_mixa`
(`switch_lab_shrd` + `FLD2`) and of `build_fld1` (`ww3_ts3/input/switch_ST4_PR1_FLD1`); the C++
unit compiled with the `openmp-release` flags, warning-free, 2026-10-16.

- (v) The GIMPLE of `infld` is the same under `FLD1` and `FLD2`: four loads through the
  `W3GDATMD` pointers and four stores (`:861-864`). Its assembly is `mov`/`movss`, the C++ one
  `mov`/`vmovss`/`vinsertps`/`vmovlps`: bit copies on both sides, no arithmetic, no libm.
- ⚠ The `threadprivate` copies (`:79`, `:87-88`) exist only under `W3_OMPG` (`:78`, `:86`), which
  the `OMPG OMPH` switches set, compiled with OpenMP;
  neither reference build has `-fopenmp`. The port leaves one `TailState` per thread to the caller.
- ⚠ WW3 behaviour, reproduced by the caller and not by this function: `FIRST` is set once per
  thread and per module (`W3FLD1`, `W3FLD2` separately), so under `ww3_multi` the tail parameters
  are those of the grid current at the first call, not of the grid being computed.
- CUDA not compiled: host function, never in a kernel.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3fld1md.F90` under `build_mixa` (`FLD2`) and
  `build_fld1` (`FLD1`), both disassemblies identical to the CMake objects; `infld` GIMPLE identical
  between the two and its assembly four pointer loads and four stores (`:861-864`); the C++ unit
  (`openmp-release` flags, warning-free) does the same four copies with `mov`/`vmovss`/`vinsertps`/
  `vmovlps`, bit moves only. Argument types match `W3GDATMD` (`INTEGER` `TAIL_ID`, `REAL` the other
  three, `w3gdatmd.F90:1274-1275`). Fixed: the `threadprivate` guard is `W3_OMPG`, not `OMPH`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
