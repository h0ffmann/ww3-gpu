# ADR-0002: Port order is measured wall time, most expensive first

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-08 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | The order in which WW3 routines are ported off the Fortran CPU path, by any route: the C++/Kokkos port, the Triton arms, and any later experiment arm |
| **Related** | [`CONTRIBUTING.md`, "Port order is wall time"](../../CONTRIBUTING.md#port-order-is-wall-time) (the rule), [`AGENTS.md`, "Porting"](../../AGENTS.md#porting-hard-rule), [`AGENTS_KOKKOS_202609.md` §2.2](../AGENTS_KOKKOS_202609.md#22-ranked-port-list), [`ww3_ts1_gprof_202610.md`](../data/ww3_ts1_gprof_202610.md) (the first measurement), #46 and #69 (the change), #42 (port planner), #45 (`W3SDS4`) |

## Context

Before this decision, `AGENTS_KOKKOS` §2.2 ranked routines by one score:
(runtime share) × (Kokkos suitability) × (ensemble-batching payoff) ÷ (engineering effort +
validation risk). The score let a routine that is cheap to validate, already has a fixture, or maps
well to a GPU move ahead of one that costs more wall time. The point of the port is to cut the
wall-clock time of the forecast `(v, docs/AGENTS_KOKKOS_202609.md:5)`, so porting a cheap routine
first spends effort where it buys little.

The shares behind the score were priors, not measurements: the §2.1 table gives ranges such as
40–65 % for the source terms, taken from published profiling of WW3 6.07 on Summit (Ikuyajolu et
al., GMD 2023), and marks them as priors to confirm `(v, docs/AGENTS_KOKKOS_202609.md:164)`.

The first measurement in this repository already disagrees with the prior order. In `regtests/ww3_ts1`
at `WW3@761cf79d` with `switch_lab_shrd` (ST4, NL1), gprof attributes 2.01 s of the 2.98 s of
source-term self time to `W3SDS4` and 0.42 s to `W3SNL1`; with `SDSCUM=0` the figures are 0.51 s
and 0.45 s of 1.33 s `(v, docs/data/ww3_ts1_gprof_202610.md)`. That table is ⚠: transcribed from
#45 and not yet reproduced by a committed command. §2.2 had `W3SNL1` first.

`AGENTS.md` already stated the rule ("Order follows wall time", the rule of #46) while §2.2 and
lesson 13 still used the score, so the repository contradicted itself until #69.

## Decision

**Routines are ported in descending order of the wall-clock time they take in the operational
case, most expensive first. Nothing else sets the order.**

1. The measurement is the phase-0 profile (`AGENTS_KOKKOS` §2.4, task P0.1 of #42): inclusive and
   exclusive wall time per routine on 1, 4 and 16 ranks, committed with the command that produced
   it.
2. Until that profile is committed, the order follows the best evidence there is, marked `⚠`: the
   `ww3_ts1` gprof table inside the source terms, and the §2.1 priors outside them. The queue is
   re-sorted the day the profile lands.
3. A routine that cannot start because of a dependency keeps its place, and the next one down
   starts meanwhile. `W3SRCE` is the case: counted inclusively it is the most expensive source-term routine,
   because it calls the source terms, but its device version needs those source terms first, so
   `W3SDS4` and `W3SIN4` go before it.
4. Effort, Kokkos suitability and ensemble payoff stay in the §2.2 table to plan each task. They do
   not change the order.
5. Experiment arms (Triton, #45) follow the same rule: they target the routine at the top of the
   ranking, not the one that is cheapest to try.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep the §2.2 score | It mixes cost with convenience, and its runtime factor was a prior. It ranked `W3SNL1` above `W3SDS4`, which the first measurement contradicts. |
| Easiest to validate first | It builds tests and fixtures quickly but spends the port's effort where the wall time is not. `W3SNL1` was ported first on these grounds; that is history, not a precedent. |
| Best GPU fit first | A good GPU kernel for a routine that takes little time does not move the forecast's wall time, and phase 1 copies per call, so the transfer cost can exceed the saving. |
| Wait for the full profile before porting anything | It stalls the work. The gprof table and the priors are enough to pick the next routine, provided they are marked `⚠` and the order is re-sorted when the profile lands. |

## Consequences

- **`W3SDS4` is the next port after `W3SNL1`**, before `W3SIN4` `(v, AGENTS.md:158)`.
- **The profile becomes a gate, not a nice-to-have.** Every re-sort of the queue points at a
  committed profile and its command; a port proposal that skips the top of the ranking has to say
  which dependency blocks it.
- **The ranking can change under a running task.** A task already started is finished; the next
  pick follows the new order.
- **Wall time on which machine.** The ranking is taken on the operational case and the CPU build;
  a routine that is cheap on CPU and expensive once the rest is on the GPU (a transfer, a
  gather/scatter) re-enters through the next profile, not through a guess.

## Revisit when

- The phase-0 profile (#42, P0.1) lands and contradicts the `ww3_ts1` order.
- The operational case changes (a new grid, physics switch or ensemble size), since the ranking is
  per case.
- After phase 2, when the spectrum stays on the device and the cost that matters is the remaining
  CPU work plus transfers, not the CPU profile.
