---
title: MPI on the Kokkos port - Plan
type: feat
date: 2026-11-01
artifact_contract: ce-unified-plan/v1
product_contract_source: "owner, session of 2026-11-01 (issue #42, goal/42-port-batch-4), 'start working in the MPI, plan next MPI steps. MPI will be implemented anyway, just tests required postponed until I get back to my i9 rtx 4090 workstation'"
execution: code
---

# MPI on the Kokkos port - Plan

## Goal Capsule

- **Objective:** a `DIST MPI` build of WW3 runs the parts of `--kokkos resident` with `VA` kept on
  the Kokkos side across the exchange, gives the bits of the Fortran of the same executable, and
  then reaches `ww3_multi`, `PDLIB` and more than one GPU.
- **Authority:** [ADR-0020](../../ADRs/ADR-0020-mpi-in-scope-exchange-on-the-resident-spectrum.md)
  (Proposed, supersedes ADR-0017 once accepted); AGENTS.md ("Bit for bit", "Translate, don't
  improve", "Resident spectrum"); ADR-0002 (order), ADR-0016 (regtests later), ADR-0014 (GPU runs).
- **Stop conditions:** a unit that would need a whole-`VA` host copy per step; an order of a
  reduction that cannot be kept; any `--gpu` run without the owner.
- **Tests:** none here. Every run is on the workstation, listed in
  [What the workstation session runs](#what-the-workstation-session-runs).
- **Execution profile:** large. The reading of 2026-11-01 stands in for `/ce-brainstorm`; each unit
  is then the medium loop, its own PR, its fork branch off the current pin.

## What runs under MPI today

Read at `WW3@96892c95` (v, 2026-11-01), line numbers from that commit.

| Part | Under `MPI` | Where |
|---|---|---|
| `SRCE`, batched | compiles, no `SHRD` guard, never run | `W3KOKKOS_SRCE_BATCH` already reads `JSEA` local and `ISEA` global arrays correctly; refused only under `PDLIB` (`w3wavemd.F90:452`) |
| `KTP3` | compiles, ran on `ww3_tp2.15` | loops `JSEA = 1, NSEAL` with `INIT_GET_ISEA` |
| per-point `SNL1`, `SDS4`, `SIN4`, `SPR4` | ran byte-identical on 36 MPI entries at `c9a6a0c6` | `docs/log/2026-10-17-st4-matrix-partial.md` |
| `XYP3` | refused | `w3kokkos_prop3md.F90:159-161` |
| `GATH`, `SCAT` | `SHRD` arm only | `wave_gath_scat.hpp`, `W3KOKKOS_GATH` and `SCAT` |
| `OUTG` | refused, `NSEAL /= NSEA` | `w3kokkos_outgmd.F90:81-86` |
| `CPRT`, `IOPE`, `UPDV` | refused | `w3kokkos_cprtmd.F90:71-76`, `w3kokkos_pntmd.F90:54-59`, `w3kokkos_updvmd.F90:58-63` |
| `PDLIB` solver | header only, no shim | `pdlib_jgs_block.hpp`; every MPI call is already a host hook in the Fortran's order |

Open, from the 2026-10-18 record: `mww3_test_08` differed under `SRCE,KTP3` in 6 files, `hs` by up
to 1.07e-2 m, while two Fortran runs agree. Never explained ⚠. The `ww3_multi` sync was fixed
afterwards, and this entry was "read and not run" on that fix
(`docs/log/2026-10-18-ww3-multi-resident-va.md:96`), so whether the fix cures it is open ⚠.

## The exchange, as WW3 does it

For one propagation step on one rank, `w3wavemd.F90:2041-2152`:

1. `MPI_STARTALL` of `IRQSG1(:,1:2)`: this rank's `VA(ISP,:)` of each component it does not own
   goes to the owner `IAPPRO(ISP)`, and the post-propagation column comes back into the same
   place. The type `WW3_SPEC_VEC` is `NSEALM` `REAL`s with stride `NSPEC`
   (`w3initmd.F90:1964`, `:2013-2016`).
2. For each owned component, `W3GATH` starts receives into `GSTORE(IP,IBFLOC)`, one per other
   rank, with `WW3_FIELD_VEC` strided by `NAPROC`, copies its own columns in, waits, and fills
   `FIELD` by `MAPSF(ISEA,3)`.
3. `W3XYP3` propagates `FIELD`.
4. `W3SCAT` writes `SSTORE`, starts the sends back, copies its own columns into `VA`.
5. `MPI_WAITALL` of `IRQSG1`.

`NRQSG1 = NSPEC - NSPLOC`, and each owned component costs `NAPROC - 1` messages each way. With
balanced `IAPPRO`, 4 ranks and `NSPEC = 576`, one rank sends 864 messages per step (432 columns
out, 144 x 3 back). The aggregated design below sends 6, two per peer ⚠ (arithmetic, not measured).

## Design of the aggregated exchange

One propagation step on one rank. This has not been built or run ⚠.

1. Pack, on the Kokkos side, the rank's values of the components each peer owns, one buffer per
   peer, from the resident `VA`.
2. Exchange, in Fortran, one send and one receive per peer.
3. Unpack the received values and the rank's own columns into a block of `NSPLOC` fields.
4. Run the `W3XYP3` batch, one launch over the owned components.
5. Pack the propagated values, exchange them back, and unpack them into the resident `VA`, with
   the `MAPSTA` test of `W3SCAT`. The rank's own columns skip the exchange in both directions.

- **New fork module `W3KOKKOS_EXCHMD`**, compiled under `W3_MPI` and `W3_KOKKOS`, set up after
  `W3MPIP`. It owns its requests and never starts `IRQSG1` or `IRQSG2`. `W3WAVE`'s
  `MPI_STARTALL` at `:2041-2044` and `MPI_WAITALL` at `:2150-2155` get `.NOT. KOKKOS_XYP3` guards.
- **Shim**: `ww_xyp3_batch_va` already has the `ON` list of owned components
  (`W3KOKKOS_XYP3`, `IAPPRO(ISP) .EQ. IAPROC`) and fills each `field` from `vad`. The gather and
  scatter lambdas take a source: the local column for `SHRD`, the component block under `MPI`.
- **Transport**: staged first, device pack into a host buffer, then CUDA-aware behind a flag
  (unit M8). On the OpenMP backend the buffers are host memory and nothing is staged.
- **Memory**: `NSPLOC x NAPROC x NSEALM` `REAL`s each way, about `NSPEC x NSEA / NAPROC`, against
  `MPIBUF x NAPROC x NSEALM` in the Fortran. On `bench_small` with 4 ranks, 5.5 MB against 0.9 MB
  per direction ⚠ (arithmetic on WW3's array sizes).

### Why the bits cannot move, and where they could

(v) `FIELD = 0.` runs at the top of `W3GATH`, above both arms (`w3wavemd.F90:3427`), so no
component inherits land values from the one before, and the C++ already zeroes per component.
Messages carry `REAL`s unchanged. So the propagation of one component depends only on the values
gathered. Two things still need the static argument of M2, and one input list needs finishing:

- ⚠ **`SSTORE` is not cleared.** `W3SCAT` writes `SSTORE(ISEA,IBFLOC)` only where
  `MAPSTA(IXY) .NE. 0` (`:3776`) and guards the owner's own column the same way (`:3799`), but the
  receive on a non-owner rank overwrites every `NSEALM` entry of the column. A sea point with `MAPSTA = 0` on a non-owner would take what an
  earlier component left in that `IBFLOC`, which depends on the component order and on `MPIBUF`.
  Either show no sea point has `MAPSTA = 0` at that moment (read `W3ULEV`, `W3UICE`,
  `w3gridmd.F90`) or reproduce the buffer contents. This is the one possible order dependence.
- ⚠ **The short rank's last column.** Round-robin leaves `NSEAL = NSEALM - 1` on some ranks, and
  `WW3_SPEC_VEC` still carries `NSEALM` entries. Keep them transported until `VA(:,NSEALM)` on such
  a rank is shown to have no reader.
- **Inputs the batch stages.** (v) `CX`, `CY`, `CG` and `WN` are allocated `0:NSEA`, `ATRNX` and
  `ATRNY` `NY*NX` (`w3adatmd.F90:1008`, `:1312`, `:1325`, `:1370`), global under `DIST` as the batch
  expects. ⚠ The boundary arrays `BBPI0`, `BBPIN`, `ISBPI` were not read.

## Units

Order: ADR-0002 within what depends on what. The shares are of the `gfs` profile, whose case runs
`ww3_multi`; `XYP3` with `W3QCK3` is 14.66 % (v, `docs/data/ww3_gfs_gprof.tsv`).

| Unit | What | Fork | Repo | Gate now | Size |
|---|---|---|---|---|---|
| M0 | ADR-0020, this plan | none | `docs/` | `just vale`, links | S |
| M1 | The case runner launches ranks: `WW3GPU_MPI_RANKS` in `scripts/ww3-gpu.sh`, on `--ww3`, the Fortran and the Kokkos run alike; the first `SRCE,KTP3` run under `MPI` follows on the workstation | none | `scripts/ww3-gpu.sh` | `--self-test`, shellcheck | S |
| M2 | The aggregated exchange and `XYP3` under `MPI` | `W3KOKKOS_EXCHMD`, `w3wavemd.F90`, `w3kokkos_prop3md.F90` | `prop3_shim.cpp`, a pack header, `PATCH.md` | build, the three arguments above | L |
| M3 | `UPDV`, `IOPE`, `CPRT` under `MPI`: columns are `JSEA`, the exchanges stay Fortran after a read of the columns | their modules | their shims | build | M |
| M4 | `OUTG` under `MPI`: the `NSEAL /= NSEA` statements of `W3OUTG` translated, `W3IOGO`'s gathers untouched | `w3kokkos_outgmd.F90` | `iogo_w3outg.hpp`, `outg_shim.cpp` | build, GIMPLE under `switch_lab_mpi` | M |
| M5 | Restart and whole-field reads under `MPI`: `W3IORS` and the read at output (`PRODUCTION_READINESS` section 3) | `w3wavemd.F90` hunks | `resident_shim.cpp` | build | M |
| M6 | `PDLIB`: shim of the block solver with its hooks calling `PDLIB_exchange*` and the `MPI_ALLREDUCE`s; the `U_JAC` contract; `W3SRCE` batched under `PDLIB` | `W3KOKKOS_PDLIBMD`, `w3wavemd.F90` | `pdlib_shim.cpp` | build, static | L |
| M7 | `ww3_multi` under `MPI`: per-grid communicators, the `WMIO*` writes synced by column, `mww3_test_08` explained | `wmwavemd.F90` hunks | `resident_shim.cpp` | build | L |
| M8 | Several GPUs: CUDA-aware transport behind a flag; node-local rank from `MPI_COMM_SPLIT_TYPE` in Fortran, passed to `ww_kokkos_init`, device id from it when `WW_KOKKOS_DEVICE_ID` is unset (every rank on device 0 is right while one GPU is all there is); thread budget per rank (`PRODUCTION_READINESS` section 9) | `W3KOKKOS_EXCHMD`, `w3initmd.F90` | `snl1_shim.cpp`, `ww_kokkos_c.hpp` | `--build-only` | M |

M1 and M2 come first and are enough to answer whether the design pays: `SRCE`, `KTP3` and `XYP3`
are the large part of the structured case. M3 to M5 make it a whole run. M6 and M7 are independent
of each other. Nothing past M2 should start before the workstation has timed M2, since a bad
result changes ADR-0020.

## What the workstation session runs

All on the CPU unless marked. `WW_KOKKOS_VA_POISON=1` is on by default in these drivers.

1. `git submodule update --init`, `just kokkos-test serial-debug`, `just kokkos-test
   openmp-release`, then the `-O3` comparison of each translated statement.
2. M1: `WW3GPU_SWITCH=switch_lab_mpi WW3GPU_MPI_RANKS=4 bash scripts/ww3-gpu.sh --ww3-gpu full --cpu
   --kokkos SRCE,KTP3`; the batched `W3SRCE` has never run under `MPI`.
3. M1: `WW3GPU_KOKKOS=all WW3GPU_MPI_RANKS=4 WW3GPU_STRICT=ON just ww3-gpu-matrix <jobs> mpi`, the
   43 entries first, `mww3_test_08` on its own with `-R`.
4. M2: the same with `XYP3`, at 2 and 4 ranks, and one rank to compare with `SHRD`; `test.comp`
   finds no file different. `WW_KOKKOS_VA_COUNT=1` before and after: whole-spectrum copies per
   step do not grow.
5. M2: the timing, `--repeat 3`, Fortran against aggregated, same ranks, CPU. This number decides
   whether M3 onward goes ahead (ADR-0020, "Revisit when").
6. GPU, with the owner at the terminal: `just smoke-ww3-gpu gpu` is still the first device run of
   the repository, and comes before any MPI run on a device.

## Not in this plan

Batch 4, the 22 translation routines, is parked: see
[`2026-11-01-001-port-batch-4-plan.md`](2026-11-01-001-port-batch-4-plan.md). It removes no wall
time, and MPI does.
