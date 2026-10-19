---
title: GFS-Wave as a target of the batched W3SRCE - Plan
type: feat
date: 2026-10-19
artifact_contract: ce-unified-plan/v1
product_contract_source: "owner, session of 2026-10-19 (goal be5bdbea-3817), 'go for GFS implementation'"
execution: code
---

# GFS-Wave as a target of the batched W3SRCE - Plan

## Goal Capsule

- **Objective:** a run with the switch of WW3's GFS-Wave regtests (`ww3_ufs1.1` to `ww3_ufs1.3`)
  takes the batched Kokkos W3SRCE and W3XYP3 path instead of falling back to the Fortran, gives
  the bits of the Fortran in the same executable, and has a profile and a timing.
- **Authority:** AGENTS.md ("Bit for bit", "Translate, don't improve", "Resident spectrum");
  ADR-0002 (order by wall time), ADR-0016 (regtests in the validation phase), ADR-0017 (MPI stays
  Fortran).
- **Stop conditions:** a branch of W3SRCE under the GFS switch whose translation cannot be shown
  to reproduce the Fortran's arithmetic statically; any `--gpu` run (never without the owner).
- **Execution profile:** large. The research round of 2026-10-19 (four read-only surveys: Kokkos
  levers, GPU coverage, switch ranking, GFS gap) stands in for `/ce-brainstorm`.
- **Defaults chosen without an owner answer:** the target is WW3 at the current pin `aa382393`
  (v7.14), not GFS.v16.3 or v17 (plan 004 moves the pin separately); the switch is the one the
  pin's `ww3_ufs1.2` uses; the first case is the cheapest `ww3_ufs1.1` grid set whose inputs exist
  on this machine.

## What GFS-Wave runs

Read at the pin `aa382393` (v, files named):

- **Driver:** `ww3_multi`; `regtests/bin/matrix.base` names `ww3_ufs1.2` the GFSv16 setup and
  `ww3_ufs1.3` GEFSv12.
- **Switch** (`regtests/ww3_ufs1.2/input/switch_MPI_OMPH`): `NCO NOGRB DIST MPI OMPG OMPH SCRIP
  SCRIPNC WRST PR3 UQ FLX0 SEED ST4 STAB0 NL1 BT1 DB1 MLIM FLD2 TR0 BS0 RWND WNX1 WNT1 CRX1 CRT1
  O0..O7 O14 O15 IC0 IS0 REF0`. No `LN1`.
- **Grids** (`regtests/ww3_ufs1.2/info`): aoc_9km, gnh_10m, gsh_15m, a points grid.

## The gap

The fork's gate for the batched W3SRCE (`WW3/model/src/w3kokkosmd.F90:1840-1846` at
`aa382393`) requires `BT1 DB1 LN1 MLIM` and refuses `FLD1 FLD2` and `SEED`. The ufs switch has
`FLD2` and `SEED` and lacks `LN1`, so a GFS-Wave run's W3SRCE is the Fortran today; the per-point
`W3SNL1`, `W3SIN4`, `W3SDS4` and `W3SPR4` already run on Kokkos under it (v, U2,
`docs/log/2026-10-19-gfs-wave-switch.md`). W3SRCE's branches
for those switches are at `w3srcemd.F90:531, 534, 826, 938, 2388, 2734, 2747` (v).

## Units

| Unit | What | Files | Gate now | Left for the validation phase | Size |
|---|---|---|---|---|---|
| U1 | Switch record; the `ports.tsv` rows still stamped `6d1bcdfe` re-checked at the pin | `docs/log/`, `kokkos/PORT_STATUS.md` | `port_score.py --check` | none | S |
| U2 | A ufs case that runs with `--ww3` and `--ww3-gpu --cpu` on one node; its inputs located | `scripts/ww3-gpu.sh`, `regtests/` | the scripts' self-tests, shellcheck | `test.comp` of the ufs entries | M |
| U3 | A gprof profile of that case, Fortran | `docs/data/ww3_gfs_gprof.tsv`, `docs/log/` | `port_score.py --check` | none | S |
| U4 | The W3SRCE team body for `SEED`, `FLD2` and no `LN1`, and the gate in the fork that admits them | `kokkos/src/ww_kokkos/`, `kokkos/src/fortran_iface/`, `PATCH.md`, the fork's `w3kokkosmd.F90` | `just kokkos-test serial-debug`, `openmp-release`; the static parity argument (ADR-0016) | `just ww3-gpu-matrix` on the ufs entries, `WW_KOKKOS_STRICT=1` with zero fallbacks | L |
| U5 | What U3 ranks next | per routine | per routine | per routine | M each |
| U6 | Timing: median of 3, `WW_KOKKOS_VA_POISON=0`, machine and command, "regtest pending ⚠" | `bench/results/` | `just results check` | the regtest pass | S |

U1 to U3 and U4 run in parallel: U4 is needed whatever the profile says, because without it no
source term of a GFS-Wave run reaches Kokkos.

## Open questions for the owner

1. GFS.v16.3 or v17 as the long-term target (plan 004 chose v17 before the v7.14 pin).
2. Where the forcing of `ww3_ufs1.2` lives on this machine, if U2 does not find it.
