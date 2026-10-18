# ADR-0019: GoogleTest for the runtime infrastructure that ports no WW3 routine

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-18 |
| **Deciders** | Hoffmann |
| **Written by** | an agent, from the owner's question of 2026-10-18 ("is it possible to adopt GoogleTests in this repository, as an alternative way to test?") and the code review of PR #191 |
| **Scope** | Which C++ of `kokkos/` may have GoogleTest suites of its own: the runtime infrastructure (the resident spectrum's state machine and column caches, the run-time flags, presets and fallback counters, the package config). Not a ported routine, not its shim's arithmetic, not the Fortran half on the fork, not the scripts (they keep their `--self-test`s) |
| **Related** | [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md) (which this narrows, never loosens, for ported routines); [ADR-0016](ADR-0016-speed-first-regtests-postponed-one-person-loop.md); [plan 006](../compound/plans/2026-10-18-006-googletest-runtime-infrastructure-plan.md); [`kokkos/tests/CMakeLists.txt`](../../kokkos/tests/CMakeLists.txt) |

## Context

ADR-0010 (Accepted, 2026-10-14) removed every unit test of a ported routine: a GoogleTest of a
routine needs a fixture captured from the Fortran, a second copy of WW3's arithmetic that drifts
from the one WW3's own `-O3` build computes (gfortran expands `x**11` differently at `-O0` and
`-O3`, [`docs/log/2026-10-14-snl1-bitwise-af11.md`](../log/2026-10-14-snl1-bitwise-af11.md)). Its
title reads wider than its alternatives table: "Kokkos code in this repository has no tests of its
own", while it kept the tests "that test no port" (`kokkos/tests/CMakeLists.txt` at `main`
`5cd0140`: `L1_test_intro`, `L1_test_nccmp_tol`, `L1_test_bench_case`, `L1_test_fetch_analyse`,
`L1_test_timers`, all GoogleTest, run by `just kokkos-test` and CI's `kokkos` job, v).

Since then the port grew code that ports no WW3 routine and that every resident run depends on
(v, sizes at `5cd0140`, `wc -l`):

| File | Lines | What it decides |
|---|---|---|
| `kokkos/src/fortran_iface/resident_shim.cpp` | 599 | which copy of `VA` (host or device) holds the newer bits, every copy between them, the column reads and writes of `ww3_multi`, the output readers and `W3CPRT`, the per-grid column-list cache (`kMaxColSets = 8`), `WW_KOKKOS_VA_CHECK`, `_POISON`, `_RESIDENT`, `_COUNT` |
| `kokkos/src/fortran_iface/part_flags.cpp` | 135 | `ww_kokkos_flag` (a part's flag, the preset, `WW_KOKKOS_ALL`, the opt-in list), `WW_KOKKOS_PRESET`, `WW_KOKKOS_STRICT`, the fallback slots and the line printed at exit that the regtest driver reads |

A defect there changes no arithmetic: it hands a kernel stale bits, copies the wrong columns, or
lets a run report Kokkos while the Fortran ran. WW3's regtests catch it only when the stale bits
reach a compared file, and the review of PR #191 found such gaps (the report of run
`20261018-154456-d3a4d3cf`: the `W3OUTG` fallback outside strict mode, the missed-sync check off by
default, the column cache that can thrash, `WW_KOKKOS_STRICT` on only for the literal `1`). Each
has a known right answer that needs no Fortran reference.

## Decision

**Ported routines keep ADR-0010 unchanged: no unit test, no fixture, WW3's regtests are their only
tests. The runtime infrastructure that ports no WW3 routine may have GoogleTest suites of its own,
which test its contract through its public C ABI on a host backend (Serial, OpenMP), never on a
device without the owner, and never as parity evidence for a ported routine.**

In this repository that means:

1. A suite is allowed for code that computes nothing WW3 computes: the residency state machine, the
   column lists and their cache, flag, preset and strict-mode parsing, fallback slots and the exit
   lines, the package config. A file that translates a WW3 routine, or a shim's arithmetic around
   one, has none.
2. Suites call the C ABI the Fortran calls (`resident_c.hpp`, `ww_kokkos_c.hpp`); no test-only
   entry point is added to production code. A behaviour that depends on an environment variable
   read once per process gets its own CTest entry with that `ENVIRONMENT`.
3. Suites are named `infra_test_<area>` and carry the CTest label `infra`, so that a reader of
   `ctest -N` can tell them from the tool tests, and a CUDA build can exclude them (`-LE infra`)
   until the owner runs them on a device.
4. A suite enters only with evidence that it can fail: each test is shown, on a scratch copy, to
   fail against a planted defect in the code it guards (the method of the mutation audit of
   2026-10-17).
5. `just kokkos-test serial-debug` (sanitizers, bounds checks) and `openmp-release` run them, as
   CI's `kokkos` job does today; no new gate.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep ADR-0010 as read, no new suite | Leaves the infrastructure that decides every copy of the spectrum with no test faster than a regtest, and the review's gaps found by reading only |
| GoogleTest for ported routines too | ADR-0010's reason stands: a captured fixture is a second, drifting copy of WW3's arithmetic |
| Test hooks in production code (reset the state, peek at the device copy) | A hook is code the Fortran never calls; the public ABI is enough (plan 006, "How the state machine is observed") |
| Bash or CMake tests instead | The scripts keep theirs; the C++ state machine is best driven from C++ |

## Consequences

- A defect of the residency logic or of a flag rule shows in a fraction of a second, at the
  function, before a regtest run.
- ADR-0010's rule for ported routines is unchanged and now stated against a boundary; if this ADR
  is accepted, ADR-0010's index row gains "narrowed by ADR-0019", and `AGENTS.md` ("The port has no
  tests of its own"), `CONTRIBUTING.md` and the `ww3-port` skill say where the boundary is.
- Infrastructure suites add maintenance: a change of the C ABI's contract changes its suite in the
  same PR.
- On a CUDA build the suites would run kernels on the device, so they stay out of any agent's CUDA
  ctest (label `infra`), as the device rule requires.

## Revisit when

A suite catches nothing over several months of changes to its file, or a defect of the
infrastructure reaches a regtest first: then the boundary or the suite is wrong.
