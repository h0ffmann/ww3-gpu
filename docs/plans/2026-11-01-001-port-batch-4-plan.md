---
title: Batch 4, the single-node compute still in Fortran - Plan
type: feat
date: 2026-11-01
artifact_contract: ce-unified-plan/v1
product_contract_source: "owner, session of 2026-11-01 (issue #42, goal/42-port-batch-4), 'check port status, what is missing? Create next big batch PR.'"
execution: code
---

# Batch 4, the single-node compute still in Fortran - Plan

**Parked,** 2026-11-01: the owner moved the session to MPI
([plan](2026-11-01-002-mpi-kokkos-plan.md), [ADR-0020](../ADRs.md#adr-0020)).
The ADR-0017 limits in this page hold until ADR-0020 is accepted.

**Units U1 and U2 translated,** 2026-11-12 (goal `UrLTXtPC-4817`, the owner's request "continue
working in the project goal, prepare a big PR"): `W3CSPC`, `W3GFCL_R8`, `W3CGDM_R4`, `W3UCUR`,
`W3UBPT`, `W3UICE`, one translator and one fresh reviewer per unit
([log](../log/2026-11-12-batch4-u1-u2-kokkos-translation.md)).

**Units U3 to U7 translated,** 2026-11-12 (goal `hr8xzdg1-6142`): `W3UNO2`, `W3UNO2R`, `W3UNO2S`,
`W3MAP1`, `W3MAP2`, `W3SIC2`, `W3SIC3`, `WN_PRECALC_CHENG`, `W3IC3WNCG_V1`, `W3SIS2`, `INSIS2`,
`DIAGONALIZE`, `W3ADDIG`, `W3SBS1`, `W3SBT8`, `W3SBT9`, `W3FLD1`, one translator and one fresh
reviewer per unit; not wired, not regtested
([log](../log/2026-11-12-batch4-u3-u7-kokkos-translation.md)).

## Goal Capsule

- **Objective:** translate the 22 routines below (7 763 lines at `WW3@96892c95`), each bit for
  bit against the object of WW3's own build, and correct the ledger rows the pin has outgrown.
- **Authority:** AGENTS.md ("Porting", "Bit for bit"), ADR-0002 (order by wall time), ADR-0010 (no
  tests of the port's own), ADR-0016 (regtests in the validation phase), ADR-0017 (no MPI path).
- **Stop conditions:** a routine whose object cannot be reproduced from C++ stays out of the batch
  and says why; a reduction, limiter or integration order that cannot be kept; any `--gpu` run.
- **Not done here:** no shim, no fork branch, no `WW3/` pin move, no test, no regtest, no timing.
  Nothing here changes what a WW3 run computes or how long it takes.
- **Execution profile:** large by line count, translation only. Translator and fresh reviewer per
  unit, as in [Batch 3](../BATCH3_SHIMS_KOKKOS_PLAN_202610.md).

## What is missing, 2026-11-01

Counts from `python3 scripts/port_score.py --check` (current) and `kokkos/PORT_TABLE.tsv` (v):

| Level | Run time, `full` | Routines of 1 023 | Lines of 263 375 |
|---|---|---|---|
| translated | 99.3 % | 316 | 69 024 |
| ported (WW3 at the pin calls it) | 99.3 % | 72 | 19 059 |
| regtested | 48.6 % | 14 | 3 151 |

- **Not wired.** 244 translated rows have no caller at the pin. The two largest are
  `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` (37.54 % of the `pdlib` profile) and `CALCARRAY_JACOBI_VEC`
  (20.37 %). They cannot be wired while ADR-0017 stands: `PDLIB` needs the `MPI` switch in all 16
  of the 721 regtest switch files that name it (v, read at the pin), and the ADR says "No MPI path
  is translated, wired or regtested in this repository while this ADR stands."
- **Not regtested.** 302 rows wait for the validation phase the owner calls (ADR-0016).
- **Not run on a device.** `PORT_STATUS.md`: "On a GPU: none yet"; the run is the owner's.
- **Not translated.** 707 routines at the pin by the script's inventory. 299 of them are
  single-node with no MPI call; 25 of those do file I/O; the rest of the 707 are `wm*`, `ww3_*`
  programs, `yow*`, namelist readers, ESMF and coupling.
- **Row 1 is not a routine.** `init` heads `PORT_TABLE.tsv` at 1.66 % of the `gfs` profile and no
  `SUBROUTINE` or `FUNCTION` of that name exists in `model/src` (v, grep at the pin). Which library
  it comes from was not checked ⚠. `finalise.0` and `main` are the same case. The first portable
  row is `w3cspcmd:w3cspc`.

## Why these routines

`PORT_STATUS.md` sets the order once the profiles rank only set-up and I/O: the profile, then what
NOAA's operational switches compile, then regtest coverage. Within ADR-0017 that gives:

1. **Measured** in a committed profile above one gprof sample: `W3CSPC` (0.20 %, `gfs`),
   `W3GFCL_R8` (0.16 %), `W3CGDM_R4` (0.08 %), `W3UCUR` (0.08 %). `W3UICE` and `W3UBPT` are in the
   `gfs` and `pdlib` profiles below one sample: unranked, and they sit beside `W3UCUR`.
2. **NOAA's operational switches:** `W3SBS1`, the `SBS` of `switch_NCEP_st4sbs`.
3. **Regtest coverage,** the count of the 456 `SHRD` switch files naming the package:

| Switch | Files | Routines |
|---|---|---|
| `UNO` | 116 | `W3UNO2`, `W3UNO2R`, `W3UNO2S` |
| `PR2`, `PR1` | 101, 64 | `W3MAP2`, `W3MAP1` |
| `IC3` | 38 | `W3SIC3`, `WN_PRECALC_CHENG`, `W3IC3WNCG_V1` |
| `IC2`, `IS2` | 10, 10 | `W3SIC2`, `W3SIS2`, `INSIS2` |
| `IG1` | 6 | `W3ADDIG` |
| `BT8`, `BT9` | 4, 2 | `W3SBT8`, `W3SBT9` |
| `FLD1` | 2 | `W3FLD1`, the one flux package the batched W3SRCE's gate still refuses |

These counts are of switch files, not of `matrix.base` entries or of what a run spends ⚠.

## What this batch does not buy

Translation removes no wall time: none of the 22 is wired here, and the four measured ones sum to
0.52 % of the `gfs` profile. It raises the *translated* level only. What moves the run time is the
owner's: the validation phase, the supervised device run, and any change to ADR-0017.

## Left out

- **`w3profsmd_pdlib`, `yow*`, `w3initmd:w3mpii`, `w3mpip`:** MPI, ADR-0017.
- **`w3profsmd`'s implicit explicit-unstructured solvers (`W3XYPFSNIMP`, `PGMRES`, `BCGSTAB`,
  `ILUT`):** not ranked by a profile, and the SPARSKIT callers have no regtest reach to rank.
- **`W3SNL4`, `W3SNL2`, `W3SNLS`, `W3SNLGQM`:** at or below 5 `SHRD` switch files where the count
  was taken (`NL4` 5, `NL2` 1); the next batch.
- **`w3gsrumd`'s other 75 routines, `w3timemd`, `w3servmd`, `w3arrymd`, the allocators of
  `w3odatmd`, `w3idatmd`, `w3wdatmd`:** below one gprof sample or not compute.
- **Readers and writers, `wm*`, `ww3_*`, ESMF, coupling:** not compute, or MPI.
- **Fork work:** the ledger rows of `W3PART`, `PT_FLD`, `PTMEAN`, `PTSORT`, `PTNGHB` and the FIFO
  helpers move to wired by correcting them, because `W3KOKKOS_CPRT` is called at the pin
  (`w3wavemd.F90:2696-2697`). No new fork branch.

## The batch

Lines at `WW3@96892c95`. Each unit is one translator and one fresh reviewer.

| Unit | Routine | Lines | Size | Reference switch |
|---|---|---|---|---|
| U1 | `w3cspcmd:w3cspc` | `w3cspcmd.F90:143-603` | 461 | `switch_lab_shrd` with `ww3_ufs1.1`'s set |
| U1 | `w3gsrumd:w3gfcl_r8` | `w3gsrumd.F90:1369-1669` | 301 | the same |
| U1 | `w3gsrumd:w3cgdm_r4` | `w3gsrumd.F90:4014-4363` | 350 | the same |
| U2 | `w3updtmd:w3ucur` | `w3updtmd.F90:171-472` | 302 | `switch_lab_shrd` |
| U2 | `w3updtmd:w3uice` | `w3updtmd.F90:1806-2052` | 247 | `switch_lab_shrd` |
| U2 | `w3updtmd:w3ubpt` | `w3updtmd.F90:1329-1545` | 217 | `switch_lab_shrd` |
| U3 | `w3uno2md:w3uno2` | `w3uno2md.F90:102-446` | 345 | `mww3_test_01/input/switch_PR3_UNO` |
| U3 | `w3uno2md:w3uno2r` | `w3uno2md.F90:472-805` | 334 | the same |
| U3 | `w3uno2md:w3uno2s` | `w3uno2md.F90:836-1224` | 389 | the same |
| U3 | `w3pro1md:w3map1` | `w3pro1md.F90:107-286` | 180 | `ww3_ts3/input/switch_ST4_PR1_FLD1` |
| U3 | `w3pro2md:w3map2` | `w3pro2md.F90:135-492` | 358 | `mww3_test_01/input/switch_PR2_UNO` |
| U4 | `w3sic2md:w3sic2` | `w3sic2md.F90:120-513` | 394 | `switch_lab_shrd` with `IC2` |
| U4 | `w3sic3md:w3sic3` | `w3sic3md.F90:96-654` | 559 | `switch_lab_shrd` with `IC3` |
| U4 | `w3sic3md:wn_precalc_cheng` | `w3sic3md.F90:2164-2417` | 254 | the same |
| U4 | `w3sic3md:w3ic3wncg_v1` | `w3sic3md.F90:657-836` | 180 | the same |
| U5 | `w3sis2md:w3sis2` | `w3sis2md.F90:652-1086` | 435 | `mww3_test_03/input/switch_PR3_UQ_MPI_IS2` with `SHRD` |
| U5 | `w3sis2md:insis2` | `w3sis2md.F90:103-620` | 518 | the same |
| U5 | `w3gig1md:w3addig` | `w3gig1md.F90:146-375` | 230 | `ww3_tig1.1/input/switch` |
| U6 | `w3sbs1md:w3sbs1` | `w3sbs1md.F90:112-462` | 351 | `switch_lab_shrd` with `SBS` |
| U6 | `w3sbt8md:w3sbt8` | `w3sbt8md.F90:111-492` | 382 | `ww3_tbt1.1/input/switch_BT8` |
| U6 | `w3sbt9md:w3sbt9` | `w3sbt9md.F90:118-406` | 289 | `ww3_tbt2.1/input/switch_BT9_MPI` with `SHRD` |
| U7 | `w3fld1md:w3fld1` | `w3fld1md.F90:93-779` | 687 | `ww3_ts3/input/switch_ST4_PR1_FLD1` |

## Method

As in [`docs/log/2026-10-16-easy-batch-kokkos-translation.md`](../log/2026-10-16-easy-batch-kokkos-translation.md):
a WW3 CMake Release build per reference switch, target `ww3_lib`, in the pinned shell; the
translator reads the object and the GIMPLE; a fresh reviewer compares the C++ object with the
Fortran's, operation by operation, and fixes only what it is sure of. One pass.

Every header is included from `wave_support.cpp`, so `serial-debug` and `openmp-release` compile
the batch together. Each routine gets a row in `kokkos/ports.tsv`, a port report and a glossary
entry; `python3 scripts/port_score.py --write` regenerates the tables.

## Gates

`just kokkos-test serial-debug`, `just kokkos-test openmp-release`, `python3 scripts/port_score.py
--write` and `--check`, `python3 -m unittest discover tests`, `just vale`. No WW3 regtest, per
ADR-0016. No CUDA compile unless the shell provides it.
