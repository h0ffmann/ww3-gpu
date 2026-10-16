# Propagation on Kokkos: PR3 with UQ as one unit (phase 1, translation only)

Status: **translated, reviewed, compiled; not wired, nothing run**, 2026-10-16 ([log](log/2026-10-16-propagation-kokkos-translation.md)), under [ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md).
Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42), the port planner.

The owner, 2026-10-16: "translate the parts of the code still to translate, with the same
quality ... the parts you think are easier first. Use subagents to parse multiple fortran routines
still to port. Dont run any test or write any Kokkos test. Bit-to-bit parity is non-negotiable."
Three consequences, recorded in the log entry:

- **Order.** ADR-0002 orders ports by wall time; the owner asked for the easiest first in this
  round. The two orders nearly agree here: what is left above 10 ms, by
  [`kokkos/PORT_RANKING.md`](../kokkos/PORT_RANKING.md), is the propagation package of the lab
  switch (`PR3`, `UQ`), and its routines go from simplest to hardest as `W3QCK1`, `W3QCK3`,
  `WAVNU1`, `W3KTP3`, `W3XYP3`.
- **No test runs.** Nothing is run: no ctest, no regtest. The C++ is compiled (serial-debug,
  openmp-release, cuda-release) to show it builds. Bit parity is argued statically from the `-O3`
  GIMPLE and assembly of both compilers (ADR-0011 item 4) until the unit's checkpoint.
- **Agents.** The parsers, translators and reviewers run on Fable at medium effort.

## Scope

| Rank | Routine | Lines at NOAA-EMC/WW3 `03adda91` | Self time | Share |
|---|---|---|---|---|
| 1 | `W3XYP3` | `w3pro3md.F90:641-1477` | 3.92 s | 2.98 % |
| 2 | `W3QCK3` | `w3uqckmd.F90:925-1336` | 1.50 s | 1.14 % |
| 3 | `W3QCK1` | `w3uqckmd.F90:117-480` | 1.19 s | 0.91 % |
| 5 | `W3KTP3` | `w3pro3md.F90:1519-1961` | 0.40 s | 0.30 % |
| 13 | `WAVNU1` | `w3dispmd.F90:85-202` | < 0.01 s | 0.00 % |
| (not in the profile) | `W3QCK2` | `w3uqckmd.F90:517-891` | — | — |

`W3KTP3` calls `W3QCK1` for the pass in direction and `W3QCK2` for the pass in wavenumber
(the `W3_UQ` branch of `w3pro3md.F90`, read by the W3KTP3 parser) (v), so `W3QCK2` is in the unit;
its time is inside `W3KTP3`'s row or below gprof's 10 ms sample. `WAVNU1` is not called per time
step: `W3INIT` and `W3ULEV` fill `WN` and `CG` with it, which the propagation reads (v).

Shares: the `bench_small` profile (`just profile-case full`, `WW3@6d1bcdfe`) (v). Out of scope:
`W3MAP3` and `W3MAPT` (set-up; their maps are copied from the Fortran), `W3CFLXY` (a diagnostic),
the MPI gather and scatter (`W3GATH`, `W3SCAT`), and the
wiring into `W3WAVE` (a later step: the rows stay "translated").

## Method

The ST4 and W3SRCE method ([`ST4_KOKKOS_PLAN_202610.md`](ST4_KOKKOS_PLAN_202610.md),
[`W3SRCE_KOKKOS_PLAN_202610.md`](W3SRCE_KOKKOS_PLAN_202610.md)):

1. One read-only parser per routine writes a spec from the Fortran at the pin and the `-O3`
   object of WW3's CMake Release build (confirmed identical): arguments, module state, branches
   under the lab switch, every floating-point statement in its emitted order, folded constants,
   libm and libmvec calls, vectorised reductions, and every `MIN`/`MAX` operand order read from
   the assembly (GCC swaps them after GIMPLE; the W3SRCE review found ten).
2. A common layer fixes the signatures (`prop_params.hpp`).
3. One translator per routine writes a header-only device function in `ww::prop`.
4. A fresh reviewer per routine compares the C++ object with the Fortran object.
5. Ledger rows `ported`, `wired` no, `regtested` no; port reports; glossary; log.
