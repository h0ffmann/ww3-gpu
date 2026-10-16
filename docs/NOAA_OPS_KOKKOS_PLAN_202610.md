# NOAA's operational switches on Kokkos: the next translation batch (phase 1)

Status: **batches A, B and C translated and reviewed**, 2026-10-16 (PR #152); `kokkos/PORT_TABLE.tsv` has each routine's state. Not wired, not regtested. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
the port planner. Translation only, bit for bit, no test run, under
[ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md); the port has no tests of its own
([ADR-0010](ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)).

## Why these routines

The physics of GFS-Wave's switches is what this repository already translated: `switch_NCEP_st4`
and the `ww3_ufs1.x` regtests run `PR3 UQ ST4 NL1 LN1 BT1 DB1 IC0 IS0 REF0`, the lab switch's set
(v, `WW3-noaa/model/bin/switch_NCEP_st4`, `WW3-noaa/regtests/ww3_ufs1.1/input/switch_MPI`,
read 2026-10-16). What NOAA runs and this repository has not translated is elsewhere:

- the unstructured-grid solver `PDLIB` of `switch_NWS_rwps` (the NWS Regional Wave Prediction
  System switch) and of `ww3_ufs1.1`'s `input_unstr`, with `IC4`, `REF1`, `TR1` in the former;
- `BT4` in `ww3_tp2.17`'s switches; `NL3` in `switch_NCEP_glwu`.

Port order is wall time ([ADR-0002](ADRs/ADR-0002-port-order-wall-time.md)). The committed profile
is the lab switch's, where none of these runs, so a gprof profile of `ww3_tp2.17` with
`switch_PDLIB` on one MPI rank is being added (`scripts/profile_case.sh pdlib`,
`docs/data/ww3_pdlib_gprof.tsv`); until it is committed, the order below is by what the
operational switches compile, not by measured time ⚠.

## Batch A: this branch (Claude Code, `goal/42-port-noaa-ops`)

`WW3-noaa/model/src` at NOAA-EMC/WW3 `03adda91`:

| Routine | Lines | Size |
|---|---|---|
| `PDLIB_W3XYPUG` | `w3profsmd_pdlib.F90:626-866` | 241 |
| `PDLIB_W3XYPFSN2` | `w3profsmd_pdlib.F90:868-1200` | 333 |
| `PDLIB_W3XYPFSPSI2` | `w3profsmd_pdlib.F90:1202-1502` | 301 |
| `PDLIB_W3XYPFSFCT2` | `w3profsmd_pdlib.F90:1504-1877` | 374 |
| `CALCARRAY_JACOBI_VEC` | `w3profsmd_pdlib.F90:3531-3787` | 257 |
| `APPLY_BOUNDARY_CONDITION` | `w3profsmd_pdlib.F90:5150-5336` | 187 |
| `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` | `w3profsmd_pdlib.F90:5463-6406` | 944 |
| `PDLIB_EXPLICIT_BLOCK` | `w3profsmd_pdlib.F90:6408-6685` | 278 |
| `W3SIC4` | `w3sic4md.F90:121-803` | 683 |
| `W3SREF` (`REF1`) | `w3ref1md.F90:111-670` | 560 |

The PDLIB routines exchange data between MPI ranks: phase 1 translates the per-rank arithmetic
and keeps every exchange and the Gauss-Seidel order as the Fortran has them. The profile may move
rows in or out of this batch.

## Batch B: this branch too (simple, self-contained)

Small routines that read their inputs, compute, and return, with a model already in
`kokkos/src/ww_kokkos/` to copy the style from. None is in Batch A. First split off for Kimi Code;
the owner moved it to this branch on 2026-10-16, before any work on it had started.

| Routine | Lines | Size | Switch | Model to follow |
|---|---|---|---|---|
| `WAVNU3` | `w3dispmd.F90:348-454` | 107 | used by several switches | `prop_wavnu1.hpp`, `disp_distab.hpp` |
| `WAVNU_LOCAL` | `w3dispmd.F90:456-547` | 92 | | `prop_wavnu1.hpp` |
| `TABU_ERF` | `w3sbt4md.F90:242-312` | 71 | `BT4` | `st4_tabu_stress.cpp` |
| `INSBT4` | `w3sbt4md.F90:145-231` | 87 | `BT4` | `st4_insin4.cpp` |
| `W3SBT4` | `w3sbt4md.F90:340-636` | 297 | `BT4` (`ww3_tp2.17`) | `srce_bt1.hpp` |
| `W3SIS1` | `w3sis1md.F90:74-227` | 154 | `IS1` (`ww3_ts1` regtests only) | `srce_db1.hpp` |
| `W3SIC1` | `w3sic1md.F90:93-369` | 277 | `IC1` (`ww3_tic*`, `ww3_ts1`, `mww3_test_08` regtests only) | `srce_db1.hpp` |
| `W3STR1` | `w3str1md.F90:183-543` | 361 | `TR1` (`switch_NWS_rwps`) | `srce_ln1.hpp` |

Each is one header, one port report, one `kokkos/ports.tsv` row, then
`python3 scripts/port_score.py --write`, as in the units before
([`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), "Method").

## Batch C: this branch (partitioning and `NL3`)

With batches A and B in, the most expensive Fortran left in the PDLIB profile, after the `W3WAVE` driver itself, is the spectral
partitioning `W3PARTMD` (`pt_fld`, `ptmean`, `ptsort`, `w3part` in `docs/data/ww3_pdlib_gprof.tsv`),
which every switch compiles and GFS-Wave's partitioned output calls. `NL3` is the nonlinear term
of `switch_NCEP_glwu`, the Great Lakes system.

| Routine | Lines | Size |
|---|---|---|
| `W3PART` | `w3partmd.F90:138-497` | 360 |
| `PTSORT` | `w3partmd.F90:511-613` | 103 |
| `PTNGHB` | `w3partmd.F90:630-821` | 192 |
| `PT_FLD` | `w3partmd.F90:839-1156` | 318 |
| `PTMEAN` | `w3partmd.F90:1175-1538` | 364 |
| `INSNL3` | `w3snl3md.F90:788-1649` | 862 |
| `W3SNL3` | `w3snl3md.F90:181-777` | 597 |

## The fork

What the h0ffmann/WW3 fork needs before any of this is wired is in
[`kokkos/src/fortran_iface/PATCH.md`](../kokkos/src/fortran_iface/PATCH.md), "The fork today".
