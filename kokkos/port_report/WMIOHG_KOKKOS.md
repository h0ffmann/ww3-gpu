# WMIOHG_KOKKOS

**Status:** ported, the store of `VA` only, wired and regtested on the CPU (2026-10-18) on the
fork branch h0ffmann/WW3 `ww3-gpu/u1-multi-va` (`f330dc53`), under the resident `VA`; not at the
`WW3/` pin until the integrator moves it ⚠.

`WMIOHG` gathers the spectra a higher-ranked grid sends to grid `IMOD` (two-way nesting),
averages and converts them in Fortran, and stores them into `VA(:,JSEA)` of grid `IMOD`. With the
resident `VA` the device copy must get the same columns. What runs on the device is the store's
data movement, nothing of the averaging or the conversion.

## What was ported

| | |
|---|---|
| WW3 routine | `WMIOHG`, the store loop 2.e, `model/src/wminiomd.F90:2260-2266` at NOAA-EMC/WW3 `03adda91`: `VA(I,JSEA) = SPEC2(I,IS) / SIG2(I) * CG(1+(I-1)/NTH,ISEA)`, `JSEA = HGSTGE(IMOD,J)%LJSEA(IS)`, every `I = 1..NSPEC` |
| Kokkos code | `ww_va_host_wrote_cols` in [`../src/fortran_iface/resident_shim.cpp`](../src/fortran_iface/resident_shim.cpp) (`host_wrote_cols`, kernel `ww_va.scatter_cols`), declared in [`../src/fortran_iface/resident_c.hpp`](../src/fortran_iface/resident_c.hpp) |
| Fortran side | `W3KOKKOS_VA_WRITE_COLS` of the fork's `w3kokkos_residentmd.F90`, called after the loop with `LJSEA(1:NTOT)` ([`PATCH.md`](../src/fortran_iface/PATCH.md), section 26) |
| Plan | [the production single-node plan](../../docs/plans/2026-10-18-002-production-single-node-plan.md), unit U1 |

## Arithmetic

None on the device. The Fortran statement runs unchanged on the host array. The shim then copies
the host columns `LJSEA(1:NTOT)` into a buffer (`memcpy`), the buffer to the device
(`deep_copy`), and stores each element into `d_va(s, col)` (`ww_va.scatter_cols`). A copy of a
`float` moves its bits, so each device column holds the host column's bits (v, by reading the
code: no arithmetic in the kernel). The list is the JSEA of the Fortran loop itself, so no column
is missed or added; `WW_KOKKOS_VA_CHECK=1` would stop a run where another column changed, and
`WW_KOKKOS_VA_POISON=1` would put NaN into the output if a host reader missed a column.

## Benchmarks

| Case | Machine | Command | Result |
|---|---|---|---|
| `mww3_test_02`, `_03`, `_04` (`grdset_a`), resident preset, lab switch | i9-14900, OpenMP, 4 threads | `docs/log/2026-10-18-ww3-multi-resident-va.md`, step 3 | columns stored over the run: 18 432 (44.24 MB), 49 680 (95.39 MB), 71 388 (137.07 MB), with `WMIOEG`'s; not timed ⚠ |

No timing: the matrix runs are not timed runs, and the machine was shared.

## How it was regtested

WW3's own regtests through the matrix driver (`WW3GPU_LAB_SWITCH=ON`, `WW3GPU_STRICT=ON`), the
Fortran and the Kokkos run of one executable compared by `test.comp`: `mww3_test_01` to `_04`
(`PR3_UQ`, resident preset) and `mww3_test_05` (`ST1`/`ST4_PR3_UQ_OMP`, `SRCE,OUTG,IOPE,UPDV`),
0 files different, plain and with the host copy poisoned (v, 2026-10-18; summaries in
[`ww3gpu_matrix_multi_va_2026-10-18.tsv`](../../docs/data/ww3gpu_matrix_multi_va_2026-10-18.tsv)).
Before the change `_02`, `_03`, `_04` differed in 5, 10 and 4 files. Under MPI it builds and was
not run (ADR-0017).
