# CALCARRAY_JACOBI_VEC_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The geographic-space part of the PDLIB implicit scheme's sparse matrix (`ASPAR_JAC`) and right-hand
side (`B_JAC`): advection coefficients per element, assembled into each node's CSR row, for every
spectral component. It is 20.4 % of the attributed time of the PDLIB profile
(`docs/data/ww3_pdlib_gprof.tsv`, `ww3_tp2.17` grid c), the third routine in that profile.

## What was ported

| | |
|---|---|
| WW3 routine | `CALCARRAY_JACOBI_VEC`, `model/src/w3profsmd_pdlib.F90:3531-3787` at NOAA-EMC/WW3 `03adda91`, with and without `W3_REF1` |
| Kokkos code | [`../src/ww_kokkos/pdlib_jacobi_vec.hpp`](../src/ww_kokkos/pdlib_jacobi_vec.hpp), `calcarray_jacobi_vec<kRef1>`; `kRef1 = false` for `switch_PDLIB`, `true` for switches with `REF1` (`ww3_tp2.17/input_deep`, NOAA's `switch_NWS_rwps`) |
| C shim | none yet; `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` will call it after zeroing the matrices and adding `SOURCE_1` (`:5741-5767`) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` objects of `w3profsmd_pdlib.F90` (WW3's CMake Release flags, gfortran 16.2)
built with `ww3_tp2.17/input/switch_PDLIB` and with `input_deep/switch_PDLIB` (`REF1`, `BT1`,
`DB1`, `TR1`, `IC4`; byte-identical to `switch_NWS_rwps`), read by the translator and by two fresh
reviews, 2026-10-16.

- (v) The assembly runs in parallel over (component, node) with no atomics: `PDLIB_POSI(1:3,J)` for
  node `IP` always points into CSR row `IP` (the search covers `PDLIB_IA(IP)..IA(IP+1)-1`,
  `yowpdlibmain.F90:1600-1630`), so no two threads write one entry; within a node the element loop
  is serial in the Fortran's order, and `J` starts at `cell_off(IP-1)` in both branches.
- (v) `CRFS` (`:3715-3717`) in double then narrowed; everything else `REAL`; the `+ 0.` of each `SUM`
  kept; `MIN`/`MAX` as the object's `maxss`/`minss`; no FMA; only element-wise vectorisation in the
  Fortran.
- (v) `W3_REF1` (`:3731-3751`): `IBR = (1-IOBP_LOC)*(1-IOBPD_LOC(ITH,IP))*(1-IOBPA_LOC)` per node and
  direction; when `IBR = 1`, `DTK = KP*DTG` and `B_JAC += TRIA03*VA` without the `IB1`, `IB2`
  factors. The first review found this branch missing; it was added and reviewed again. The
  `DB1`/`BT*`/`IC*`/`TR1` blocks (`:3605-3631`) are only `USE` statements; `MGP` is in no switch file
  with `PDLIB`; `NOCGTABLE` cannot be set by a switch.
- (v) The work arrays `CXY`, `KP`, `DELTAL` are rewritten for each component before they are read,
  so any batch size gives the same bits.
- ⚠ CUDA (compiled for `sm_89` with `--fmad=false`, no `fma` in the PTX, both variants), never run;
  whether `max.f32(−0,+0)` gives +0 for `KP` as `maxss` does was not read, though a −0 cannot change
  a non-zero sum here. `switch_RWPS` and `switch_RWPSBS1` were not built.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
