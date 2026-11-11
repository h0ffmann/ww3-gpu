# OpenMP or MPI for the Kokkos runtime: why the port is single node, and why MPI could slow it down

2026-10-18. Status: **rationale of [ADR-0017](ADRs.md#adr-0017)
(Accepted).** Every WW3 line is at the fork pin `WW3@80644ba9` (h0ffmann/WW3, `ww3-gpu/kokkos-pnt`).
The measurements quoted are one run each on a loaded machine ⚠ and are not speed figures for a
table (`bench/README.md`). The projections in the last sections are arithmetic on WW3's array
sizes, not measurements ⚠.

## The two layouts WW3 can be built in

WW3 chooses its parallel layout at compile time, by switch.

| | Shared memory: `SHRD`, threads from `OMPG` or `OMPH` | Distributed memory: `DIST MPI` |
|---|---|---|
| Processes | one | `N` ranks (`mpirun -np N`) |
| The spectrum `VA(NSPEC, sea points)` | one array holding every sea point | each rank holds its own sea points only |
| Source terms | `W3WAVE`'s sea-point loop split over threads (`!$OMP DO`) | each rank runs its own sea points |
| Spatial propagation | one call per spectral component over the whole grid | each rank owns some components (`IAPPRO`, set under `W3_DIST` at `model/src/w3initmd.F90:983-1000`) and must first collect each one from every rank |
| Data movement | none: threads read the same memory | messages: `W3GATH` collects a component into `FIELD`, `W3XYP3` propagates it, `W3SCAT` sends it back, inside persistent `MPI_STARTALL`/`MPI_WAITALL` requests (`model/src/w3wavemd.F90:2040-2149`) (v) |
| Reach | one node | many nodes; NOAA's GFS-Wave runs 448 MPI tasks for the wave component (`H100_OPTIMIZATION_STRATEGY_202610.md`, "How NOAA runs WW3") |

`OMPH` adds threads inside each MPI rank (hybrid). `PDLIB`, the unstructured-grid solver, exists
only with MPI (v, `model/src/cmake/switches.json:769-770`, `"requires": ["MPI"]`).

## What the port was built for

The plan that started the port assumed one process. It lists `W3GATH`, `W3SCAT` and `IAPPRO` as
"Delete; identity mappings" (`KOKKOS_H100_PLAN_202609.md`, §4). The shim links no MPI by design:
"Phase 1 is deliberately MPI-free" (v, `kokkos/src/fortran_iface/ww_kokkos_c.hpp:16`, and
`:61-62` for the communicator it accepts and ignores).

The configuration with the measured gain is `--kokkos resident`, six parts on Kokkos (`SRCE`,
`XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV`). It keeps `VA` on the Kokkos side between steps and copies
it to the host only when a Fortran routine reads it (`kokkos/src/fortran_iface/PATCH.md`,
sections 13 to 18). On the `full` case (`bench_small`: 120 x 80 grid, 9 520 sea points, NK=24,
NTH=24, 12 hours) with `switch_lab_ompg`, `ww3_shel` took 15.51 s with that preset, 23.38 s with
the Fortran of the same executable and 24.81 s for NOAA's WW3 (`--ww3`), on the i9-14900 with 32
threads, one run each ⚠ (`bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident`,
2026-10-18, [log](log/2026-10-18-resident-regtest-lab-switch.md)). Every model file of the Kokkos
run was byte-identical to the Fortran's (v, same log).

## What happens under MPI today

Four of the six resident parts cover `SHRD` only, and asking for one by name on an MPI build stops
the model in `W3INIT` (v, read at the pin):

| Part | Under MPI | Where |
|---|---|---|
| `SRCE` (source terms) | can run | its switch gate has no `MPI` or `DIST` (`model/src/w3kokkosmd.F90:1770-1777`) |
| `KTP3` (intra-spectral propagation) | can run | needs `PR3 UQ` and a regular or curvilinear grid (`model/src/w3kokkos_prop3md.F90:122-133`) |
| `XYP3` (spatial propagation) | refused | needs `SHRD` (`w3kokkos_prop3md.F90:127-129`) |
| `OUTG` (gridded output fields) | refused | `w3kokkos_outgmd.F90:61-66` |
| `IOPE` (point output) | refused | `w3kokkos_pntmd.F90:49-54` |
| `UPDV` (ice update of `VA`) | refused | `w3kokkos_updvmd.F90:57-62` |

Four MPI regtest entries with the resident preset all stopped at `OUTG` (v,
[`docs/log/2026-10-18-resident-mpi-lab-switch.md`](log/2026-10-18-resident-mpi-lab-switch.md)).
With `SRCE,KTP3` alone, `ww3_tp2.15` matched the Fortran in every model file, and the `ww3_multi`
case `mww3_test_08` did not: 6 files differ, hs by up to 1.07e-2 m, while two Fortran runs agree
(v, same log). That difference is not explained yet ⚠. `ww3_multi` writing the host `VA` between
`W3WAVE` calls without a sync ([`PRODUCTION_READINESS_202610.md`](PRODUCTION_READINESS_202610.md),
gap 2) is one candidate, and it would apply to single-node runs too.

## Why MPI could slow ww3-gpu down

The gain comes from not moving `VA`. MPI's propagation step is built on moving it, one component
at a time, through Fortran routines that read and write the host array. Four effects follow.

**1. `VA` goes back to the host every propagation step.** `W3GATH` reads `VA` on the host and
`W3SCAT` writes it there. With `SRCE` on the device, each step then needs `VA` copied to the
host before the exchange and back after it. For `bench_small`, `VA` is NSPEC x sea points x 4 bytes =
576 x 9 520 x 4 = 21.9 MB, split over the ranks, so each step moves about 44 MB in all (out and
back) ⚠ (arithmetic on the case's sizes). On a GPU, those copies cross the PCIe or NVLink bus. On
the CPU's OpenMP backend they are host to host, but they still cost memory bandwidth that the
resident path avoids.

**2. One launch per component instead of one per step.** Under `SHRD` the Kokkos `W3XYP3` runs
every component of a step in one call (`ww_xyp3_batch_va`). Under MPI each component arrives
separately from `W3GATH`, so a Kokkos `W3XYP3` would be called once per owned component: up to
NSPEC = 576 calls per step for `bench_small`, each with its own launch, argument checks and fence
⚠ (not built or timed). A prototype of that path (`ww_xyp3_field`) was written and discarded on
2026-10-18 (ADR-0017, "Alternatives considered").

**3. The exchanges stay serial Fortran, and Amdahl's law applies.** The `MPI_STARTALL`, the
per-component waits inside `W3GATH`, and `MPI_WAITALL` (`w3wavemd.F90:2040-2149`) are not
accelerated by Kokkos. The faster the kernels get, the larger the share of each step that is
spent waiting on messages, so the end-to-end ratio tends toward the Fortran's ⚠.

**4. Threads compete inside each rank.** In a hybrid run, each rank's Kokkos OpenMP backend,
WW3's own `OMPH` threads and the per-point path's `!$OMP CRITICAL (WW_KOKKOS_SRCE)` share the
rank's cores (`PRODUCTION_READINESS_202610.md`, gap 9). With 448 ranks, every rank also starts its
own Kokkos runtime, and on GPUs every rank uses device 0 unless `WW_KOKKOS_DEVICE_ID` is set
(`kokkos/src/fortran_iface/snl1_shim.cpp:132-145`) ⚠.

So an MPI path could be bit for bit with the Fortran and still be little faster than it. A fast
MPI path needs the exchanges to work on the device copy of `VA`: gathers and scatters as Kokkos
kernels, with CUDA-aware MPI sending device buffers directly, or every component's receive
completed before one batched launch. That is a large unit of its own (ADR-0017, "Revisit when").

## The decision and what it keeps

- The Kokkos runtime supports `SHRD` only, with or without `OMPG`/`OMPH`, on the CPU and on one
  GPU. An MPI build runs the Fortran for every part that covers `SHRD` only (ADR-0017).
- Nothing in the timed configuration depends on MPI code. The MPI-path translations already in
  `kokkos/` (`pdlib_*`, `wavset_*`, `init_w3mpio`) are header-only, have no shim and no WW3 hook,
  and are not reachable from any shim the resident preset uses (v, an include trace from
  `srce_shim.cpp`, `prop3_shim.cpp`, `outg_shim.cpp`, `resident_shim.cpp`, `snl1_shim.cpp` and
  `st4_shim.cpp`, 2026-10-18). They stay as they are, frozen, as a starting point if ADR-0017 is
  superseded.
- What the decision gives up: runs larger than one node, NOAA's operational MPI configuration
  (`switch_NCEP_st4` builds with KOKKOS, but its `DIST MPI` turns off four of the six parts), and
  more than one GPU per run, which would also need one MPI rank per GPU.
