# ADR-0020: MPI enters the Kokkos port, with the exchange built on the resident spectrum

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-11-01 |
| **Deciders** | Hoffmann |
| **Written by** | an agent, from the owner's words of 2026-11-01: "MPI will be implemented anyway, just tests required postponed until I get back to my i9 rtx 4090 workstation" |
| **Scope** | Which parallel layouts the Kokkos runtime supports, and how a `DIST MPI` build moves the spectrum. Not the physics each part covers, not the GPU parity rule ([ADR-0014](ADR-0014-gpu-runs-judged-by-tolerance.md)), not when regtests run ([ADR-0016](ADR-0016-speed-first-regtests-postponed-one-person-loop.md)) |
| **Related** | Supersedes [ADR-0017](ADR-0017-kokkos-port-is-single-node-mpi-stays-fortran.md) once accepted. Plan: [`2026-11-01-002-mpi-kokkos-plan.md`](../compound/plans/2026-11-01-002-mpi-kokkos-plan.md). Analysis: [`OPEN_MP_VS_MPI_202610.md`](../OPEN_MP_VS_MPI_202610.md), [`PRODUCTION_READINESS_202610.md`](../PRODUCTION_READINESS_202610.md) sections 3 and 11 |

## Context

ADR-0017 kept the port on `SHRD` and said "No MPI path is translated, wired or regtested in this
repository while this ADR stands". Its "Revisit when" asks the next ADR to start from the "MPI is
fast" alternative, because a path that is not fast does not serve the goal. The owner has now put
MPI in scope. Three things follow from reading the pin, `WW3@96892c95` (v, 2026-11-01):

- **The largest unwired costs need MPI.** `PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK` and
  `CALCARRAY_JACOBI_VEC` are 37.54 % and 20.37 % of the `pdlib` profile. `PDLIB` requires `MPI`
  (`model/src/cmake/switches.json:769-770`), and all 16 regtest switch files naming it have `MPI`.
  NOAA's GFS-Wave switches are `DIST MPI` too (`model/bin/switch_NCEP_st4`).
- **The exchange does no arithmetic.** Under `W3_MPI`, `W3GATH` and `W3SCAT` copy `REAL`s
  (`w3wavemd.F90:3293-3598`, `:3614-3913`), and each component starts from `FIELD = 0.` above both
  arms (`:3427`). So the order and grouping of the messages cannot change a bit of the propagation
  of one component, provided every value arrives where the Fortran puts it. One case is not yet
  settled: `SSTORE` is not cleared, and `W3SCAT` skips `MAPSTA = 0` points (`:3776`) ⚠, so the
  plan's unit M2 must show no sea point is in that state or reproduce the buffer.
- **The Fortran's exchange is bound to the host.** The persistent requests of `W3MPIP` name
  `VA(ISP,1)` (`w3initmd.F90:2013-2016`) and `GSTORE`, `SSTORE` (`:2036-2037`, `:2073-2076`), with
  only `MPIBUF = 6` buffers (`w3adatmd.F90:379`). This is what ties the exchange to one component
  at a time and to the host array.

## Decision

1. **`DIST MPI` is in scope.** Parts are translated and wired for it, one flag per part as today,
   and an MPI build may run them. Where a part does not yet cover `MPI`, the Fortran runs, as now.
2. **The MPI path keeps `VA` resident across the exchange.** The batched path packs and unpacks on
   the Kokkos side and aggregates one step's components into one message per pair of ranks. A
   per-component `W3XYP3` on `W3GATH`'s `FIELD` is not built: ADR-0017 weighed it, and it would
   copy the spectrum between host and device on every step.
3. **`ww_kokkos` stays free of MPI.** The MPI calls stay in the fork's Fortran, in a new module
   behind the `KOKKOS` switch; the shim packs, unpacks and hands buffer addresses over `bind(C)`.
   This keeps `ww_kokkos_c.hpp`'s "Phase 1 is deliberately MPI-free" and the package of ADR-0018.
4. **An exchange message is not a sync.** The rule "No full-spectrum copy to keep the copies in
   sync" (AGENTS.md, "Resident spectrum") is about copies made to keep host and device consistent.
   The values MPI must carry between ranks are the model's own communication, and the bytes are
   the Fortran's. They move out of the Kokkos-side `VA` directly, never through a whole-array
   host read, and `WW_KOKKOS_VA_COUNT=1` reports them on their own line, so that the whole-spectrum
   copies per step do not grow.
5. **Code before tests, flags off.** Units merge with the gates and the static argument of
   ADR-0016, every MPI path is off unless asked for by name, and `WW_KOKKOS_ALL` does not turn one
   on. No part defaults to Kokkos under MPI (ADR-0013) until its entries pass the matrix on the
   owner's i9-14900 and RTX 4090 workstation. The rows say `regtested` `no: postponed (ADR-0016)`.
   The plan keeps the list of what that session must run.

## Alternatives considered

- **Per-component `W3XYP3` with `W3GATH`, `W3SCAT` in Fortran ("MPI works").** Smallest change and
  bit for bit within reach. Rejected as the destination for the reason in ADR-0017. Its first half
  survives as unit M1: the parts that already compile under `MPI` get run.
- **Move the existing persistent requests to device buffers.** Keeps the Fortran's message pattern
  and needs CUDA-aware MPI from the start. Rejected: `MPIBUF` buffers still force one component at
  a time, which defeats the one launch per step that gives the measured gain.
- **MPI calls in C++.** Rejected: `ww_kokkos` would link MPI in every build, including the
  `SHRD` ones, and the package ADR-0018 keeps simple would need an MPI compiler wrapper.
- **`PDLIB` first.** It is the largest share of any committed profile. Rejected as the first unit
  only: ADR-0002 orders by the operational case, GFS-Wave runs structured grids, and the
  aggregated exchange is shared by what follows. It is scheduled, unit M6.

## Consequences

- ADR-0017's "single-node figure" limit stays true of every number already published. A page may
  claim MPI only after the regtest pass, with its ranks stated.
- The 43 MPI entries of the CPU validation plan return from "out of scope" to "pending". Gap 3 of
  `PRODUCTION_READINESS_202610.md` reopens.
- Code merges that no test has run. That is ADR-0016's bargain and is why decision 5 keeps every
  path off, but the first run of each unit will find defects the static argument missed ⚠.
- An MPI path may not end up faster than the Fortran on a GPU. The aggregated exchange removes
  host copies and message counts, not the bandwidth of the interconnect, and no timing of it exists
  ⚠. If the workstation's numbers say so, this ADR is what to revisit.
- More than one GPU per run becomes reachable (unit M8), and with it the device-per-rank problem of
  `PRODUCTION_READINESS_202610.md` section 3.

## Revisit when

The first timed comparison on the workstation shows the aggregated exchange slower than the
Fortran's per-component one on the same ranks, or the first matrix pass finds a bit difference
that message aggregation causes.
