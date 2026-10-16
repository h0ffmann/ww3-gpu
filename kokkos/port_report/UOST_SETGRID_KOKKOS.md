# UOST_SETGRID_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Host setup of the unresolved-obstacles source term (switch `UOST`, Mentaschi et al.): allocates
the module object `SRCTRM` on first use and points it at grid `IGRID`. Called by `W3INIT`
(`w3initmd.F90:545`) and on every entry of `W3WAVE` (`w3wavemd.F90:700`), both under `W3_UOST`.

## What was ported

| | |
|---|---|
| WW3 routine | `UOST_SETGRID`, `model/src/w3uostmd.F90:231-278` at NOAA-EMC/WW3 `03adda91`, 48 lines; compiled by nine regtest switches (`ww3_tp2.21` `switch_NO_PDLIB`, `switch_PDLIB`, `switch_PDLIB_METIS`; `ww3_ts4` `input_rg_multi`, `input_rg_shel`, `input_ug`, each with and without MPI; `git grep -lw UOST -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). Its only `#ifdef` block is `W3_S` (trace) |
| Kokkos code | [`../src/ww_kokkos/uost_uost_setgrid.hpp`](../src/ww_kokkos/uost_uost_setgrid.hpp), `ww::uost::uost_setgrid`, an `inline` host function: it allocates (`ALLOCATE(SRCTRM)`, `:274`), so it never runs in a kernel. `SRCTRM` is a caller-owned `std::optional`; `GRIDS`, `SGRDS` are passed with their Fortran lower bound `NLOW` (`w3gdatmd.F90:1544-1545`) |
| Not translated here | `SRCTRM%SETGRID` (`:277`), the type-bound `UOST_SOURCETERM_SETGRID` (`:606-667`, its own claim): the caller passes it as a callable. The object calls it through the vtable (slot `+0x40`); `SRCTRM` is only allocated with its declared type, so the target is fixed |
| C shim | none yet |
| Plan | [`EASY_BATCH_KOKKOS_PLAN_202610.md`](../../docs/EASY_BATCH_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3uostmd.F90` in WW3's CMake Release build (gfortran 16.2,
`switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled with the build's flags and
checked byte-identical in disassembly to the CMake object, 2026-10-16.

- (v) No floating-point operation. The only values written are the default initialisation of
  `ALLOCATE` (`:77-78`): `GAMMAUP = 10`, `GAMMADOWN = 20`, stored as the 64-bit immediate
  `0x41a0000041200000` (`1.0e+1`, `2.0e+1`); the C++ unit (openmp-release flags, no warning)
  stores the same immediate. `COSTH`, `SINTH` start unallocated in both.
- (v) No libm or libmvec call, no reduction, no MIN/MAX.
- ⚠ The object fills `GRD`, `SGD` of the new object from an uninitialised stack template
  (pointers with no default in the type, `:80-81`); they are undefined until `SETGRID`
  assigns them on the next line, so no value is read. The C++ leaves them to the type's own
  default.
- ⚠ CUDA not compiled: host-only code.
- (v) Fresh review 2026-10-16: no defect. `w3uostmd.F90` recompiled from `build_mixa`'s
  flags, identical in disassembly to the CMake object; its only `#ifdef` is `W3_S`. A C++ unit
  instantiating `uost_setgrid` with the real state type `UostSourceterm<G, S>` and
  `uost_sourceterm_setgrid` as the callable compiles warning-free with the openmp-release flags
  and stores the same immediate `0x41a0000041200000`; the indexing `grids[igrid - nlow]`
  matches the object's descriptor-offset addressing of `GRIDS(IGRID)`, `SGRDS(IGRID)`. Nothing
  changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
