# ADR-0017: The Kokkos port is single node; an MPI build of WW3 keeps the Fortran path

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-18 |
| **Deciders** | Hoffmann |
| **Written by** | an agent, from the owner's answer of 2026-10-18 ("Out of scope, write ADR"); accepted by the owner the same day |
| **Scope** | Which parallel layouts of WW3 the Kokkos runtime supports: the shared-memory one (`SHRD`, with or without `OMPG`/`OMPH`) on the CPU and on one GPU. Not the physics switches each part covers, not the GPU parity rule ([ADR-0014](ADR-0014-gpu-runs-judged-by-tolerance.md)) |
| **Related** | [ADR-0016](ADR-0016-speed-first-regtests-postponed-one-person-loop.md); [ADR-0013](ADR-0013-operational-parts-default-to-kokkos.md); [`KOKKOS_H100_PLAN_202609.md`](../KOKKOS_H100_PLAN_202609.md); [`H100_OPTIMIZATION_STRATEGY_202610.md`](../H100_OPTIMIZATION_STRATEGY_202610.md); [`PRODUCTION_READINESS_202610.md`](../PRODUCTION_READINESS_202610.md); [the 2026-10-18 regtest log](../log/2026-10-18-resident-regtest-lab-switch.md) |

## Context

WW3 runs in one of two layouts, chosen by switch. With `SHRD` one process holds the whole
spectrum `VA(NSPEC,NSEA)`, and `OMPG`/`OMPH` split its loops over threads. With `DIST MPI` each
rank holds the spectra of its own sea points. Spatial propagation then moves one component at a
time between ranks: `W3GATH` gathers it into `FIELD`, `W3XYP3` propagates it, and `W3SCAT` sends
it back, with persistent MPI requests around the loop (`model/src/w3wavemd.F90:2040-2149` at
`WW3@80644ba9`).

The port was designed for the first layout. The plan lists `W3GATH`, `W3SCAT` and `IAPPRO` under
"Delete; identity mappings" (`KOKKOS_H100_PLAN_202609.md`, §4). The goal is WW3's run time with
Kokkos and CUDA on this lab's machine and one H100 (ADR-0016), and the strategy says "A
single-H100 WW3 serves this lab's use" (`H100_OPTIMIZATION_STRATEGY_202610.md`, top of the page).

The 2026-10-18 validation found where that design stops (v, read at `WW3@80644ba9`):

- Four of the six parts of `--kokkos resident` cover `SHRD` only: `XYP3`
  (`model/src/w3kokkos_prop3md.F90:127-129`), `OUTG` (`w3kokkos_outgmd.F90:61-66`), `IOPE`
  (`w3kokkos_pntmd.F90:49-54`) and `UPDV` (`w3kokkos_updvmd.F90:57-62`). Asked for by name on an
  MPI switch, each stops the model (`w3kokkosmd.F90:1188`, `KOKKOS_PART`).
- The measured gain of the resident preset comes from keeping `VA` on the Kokkos side between
  steps. Under MPI the exchanges stay in Fortran and read and write the host `VA` once per
  component, so an MPI path that keeps the gain needs device-side gathers and scatters or
  CUDA-aware MPI. A per-component `W3XYP3` on `FIELD` would be bit for bit but not fast ⚠ (not
  built or timed).
- NOAA's GFS-Wave runs with MPI on CPU nodes (`H100_OPTIMIZATION_STRATEGY_202610.md`, "How NOAA
  runs WW3"). Operations at a centre are therefore the use that MPI would serve.

## Decision

The Kokkos runtime supports the shared-memory layout only: `SHRD`, with or without `OMPG` or
`OMPH`, on the CPU (Kokkos OpenMP or Serial) and on one GPU. An MPI build of WW3 runs the Fortran
for every part that covers `SHRD` only. It stops when such a part is asked for by name, as it
does today. No MPI path is translated, wired or regtested in this repository while this ADR stands.

## Alternatives considered

- **MPI in scope, phase A ("MPI works").** `W3XYP3` alone on `W3GATH`'s `FIELD`, one component per
  call, with `W3GATH` and `W3SCAT` left in Fortran, then regtested on MPI entries. Bit for bit is
  within reach, since the C++ already separates the propagation from the shared-memory gather.
  Rejected for now: it adds an untimed path that does not serve the goal. A prototype was written
  and discarded on 2026-10-18.
- **MPI in scope, full ("MPI is fast").** Device-side gathers and scatters, or CUDA-aware MPI, with
  the resident `VA` kept across the exchanges. Rejected for now: a large unit, needed only for a
  centre's operations or for several GPUs.

## Consequences

- The regtest matrix's resident selection needs `SHRD` and has no MPI entry
  (`regtests/CMakeLists.txt`, `WW3GPU_SWITCH_WORDS`). The 43 MPI entries deferred by the CPU
  validation plan are out of scope, not pending.
- A speed figure of this repository is a single-node figure. No page may claim the port is
  ready for an operational centre's MPI runs. Gap 3 of `PRODUCTION_READINESS_202610.md` stays
  open by decision.
- Single-node gaps stay in scope. The most pressing is `ww3_multi` writing the host `VA` between
  `W3WAVE` calls without a sync (`PRODUCTION_READINESS_202610.md`, gap 2), because it applies to
  `SHRD`.
- Several GPUs would also need MPI (one rank per GPU), so this ADR also limits the port to one
  GPU per run.

## Revisit when

The owner sets operational use at a centre, an upstream WW3 runtime that must cover NOAA's
switches, or more than one GPU per run as a goal. A new ADR then supersedes this one and starts
from the "MPI is fast" alternative, since a path that is not fast does not serve those goals
either.
