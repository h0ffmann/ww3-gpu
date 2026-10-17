# ADR-0013: The parts of GFS-Wave's physics default to Kokkos once their checkpoint and a timing with parity pass

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-16 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | Which `WW_KOKKOS_<PART>` flags are on when neither the part's own flag nor `WW_KOKKOS_ALL` is set, in a WW3 built with the `KOKKOS` switch and `-DWW_KOKKOS=ON`. Not a WW3 built without that switch (`--ww3`, unchanged), not the bit-for-bit rule, not "no timing without parity", not the GPU rule, not the port order (ADR-0002) |
| **Related** | [ADR-0011](ADR-0011-port-in-units-regtest-at-checkpoints.md) (its "all off by default", which this ADR would amend for the parts below); [ADR-0012](ADR-0012-translate-and-wire-now-regtest-matrix-on-the-h100.md) (the matrix on the H100); [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md); [`kokkos/src/fortran_iface/part_flags.cpp`](../../kokkos/src/fortran_iface/part_flags.cpp) (where a flag is resolved); [`H100_OPTIMIZATION_STRATEGY_202610.md`](../H100_OPTIMIZATION_STRATEGY_202610.md) |

## Context

The owner, 2026-10-16, asked that a `--ww3-gpu` build route WW3 to Kokkos by default on the
expensive parts NOAA runs operationally (forecasts and ensembles), so that a run with no flag set
is the configuration expected to be faster than `--ww3`, and asked for no regtest to be run for
it. ADR-0011, rule 1, says every flag is off by default and "a unit is never the default path
before its checkpoint passes"; this ADR records the change of default the owner asked for and the
conditions it takes effect under, and does not flip a flag by itself.

What NOAA runs. `switch_NCEP_st4` is `NCO NOGRB DIST MPI PR3 UQ ST4 STAB0 FLX0 LN1 NL1 IC0 IS0
REF0 BT1 DB1 MLIM TR0 BS0 ...` (v, `WW3-noaa/model/bin/switch_NCEP_st4` at `03adda91`, read
2026-10-16), the physics of GFS-Wave (`docs/GLOSSARY.md`, GFS-Wave) and of the lab switch
`switches/switch_lab_shrd`, which differs in `SHRD` for `DIST MPI` and in its output switches.
That GEFS-Wave, the ensemble, compiles the same switch is not checked here ⚠.

Where the time goes, one thread, lab switch, `bench_small` (v, `docs/data/ww3_full_gprof.tsv`,
`just profile-case full`, WW3@6d1bcdfe, 2026-10-14):

| Part (flag) | Routine | Self % | Wired at the `WW3/` pin | Regtested |
|---|---|---|---|---|
| `SNL1` | `W3SNL1` | 48.35 | yes | yes (L2, `kokkos/ports.tsv`) |
| `SDS4` | `W3SDS4` | 23.23 | yes | no in `kokkos/ports.tsv`; ran byte-identical in the `W3WAVE` support checkpoint per `kokkos/src/fortran_iface/PATCH.md` §8 ⚠ |
| `SIN4` | `W3SIN4` | 12.66 | yes | as `SDS4` ⚠ |
| `SRCE` | `W3SRCE` | 5.60 | yes (fork `8e433f99`, PATCH.md) | as `SDS4` ⚠ |
| `SPR4` | `W3SPR4` | 3.12 | yes | as `SDS4` ⚠ |
| `ST4_TABLES` | `INSIN4`, `TABU_*` | set-up | yes | as `SDS4` ⚠ |
| (none yet) | `W3XYP3` | 2.98 | no: header-only | no |

These five parts hold 92.96% of the self time of that profile.

### What crosses between host and device

Phase 1 moves one sea point per call. `W3SRCE`'s shim copies the point's spectrum, `WN`, `CG`
and its scalars to the device, runs the kernel, and copies them back, and the Fortran caller
serialises the calls under `!$OMP CRITICAL (WW_KOKKOS_SRCE)` (v,
`kokkos/src/fortran_iface/srce_shim.cpp:5-10,111-115`); `W3SNL1`'s and ST4's shims do the same
per call (v, `snl1_shim.cpp:376-384`, `st4_shim.cpp:369-385`). On a GPU each call is therefore
about ten host-to-device and device-to-host copies of a few kilobytes and one kernel launch, for
one point's work, which a GPU runs on a single thread. Each of those costs microseconds of fixed
latency before any arithmetic, and the points never run side by side, so a phase 1 GPU run is
expected to be slower than `--ww3`, not faster ⚠ (an argument from the shims' structure; no
timing of `--ww3-gpu --gpu` exists, and none may be taken without parity).

Two things follow for which parts are worth routing to a GPU:

- **One unit, not its pieces.** Routed one by one inside a Fortran `W3SRCE`, `SNL1`, `SDS4`,
  `SIN4` and `SPR4` each make their own round trip per point and per source-term sub-step.
  `SRCE` routed as a whole makes one round trip per point per time step and keeps the DIA and ST4
  on the device between them (its kernel reads the `W3SNL1` and ST4 tables of the grid,
  `srce_shim.cpp:9-10`). On a device build, `SRCE` is the part to route; the separate flags are
  for bisecting a difference on the CPU.
- **Batches, not points.** The transfer only pays when a call carries every sea point of the
  grid, or when the spectrum stays resident between time steps (phase 2,
  [`H100_OPTIMIZATION_STRATEGY_202610.md`](../H100_OPTIMIZATION_STRATEGY_202610.md)). Until then
  no part has a transfer cost a GPU can win back.

## Decision

From its acceptance by the owner on 2026-10-16, and for each part only once the conditions below hold for it, the
default (part flag unset, `WW_KOKKOS_ALL` unset) becomes on for `SRCE` in a device (CUDA) build,
and for `SNL1`, `SDS4`, `SIN4`, `SPR4`, `ST4_TABLES` and `SRCE` in a host (OpenMP) build. Every
other part stays off by default. An explicit
`WW_KOKKOS_<PART>=0` or `WW_KOKKOS_ALL=0` still selects the Fortran, so `--ww3-gpu`'s Fortran run
and every comparison of the matrix keep the Fortran in the same executable.

1. **Checkpoint.** The part's row of `kokkos/ports.tsv` says `regtested` yes, from the WW3 regtest
   matrix of ADR-0012 with `WW3GPU_STRICT=ON` on the entries that compile the part's switch.
2. **Timing with parity.** A record in `bench/results/` on the target machine, on the same
   commit as a passed regtest replay, shows `--ww3-gpu` with the part on faster than `--ww3`,
   median of three runs, end to end (`scripts/results.py check` refuses it otherwise).
3. **On a GPU, a batched shim.** The device default needs `SRCE`'s shim to carry all sea points
   of a call, or the spectrum to stay resident (phase 2), and the GPU run's bits to match the
   Fortran's (`AGENTS.md`, "Bit for bit", the `expf`/`logf` caveat), under the owner's
   supervision.

The PR that flips a default edits the default list in `part_flags.cpp`, names the record of
condition 2, and updates ADR-0011's rule 1 by a superseding note, not by an edit.

The PR1, PR2 and unstructured explicit propagation parts (`XYP1`, `KTP1`, `XYP2`, `KTP2`,
`XYPUG`) are outside this decision: they are opt-in only, ignored by `WW_KOKKOS_ALL`, by the
owner's request of the same day.

## Alternatives considered

- **Flip the defaults now, without regtests.** What was asked first. Rejected for now: five of the
  six parts have no `regtested` yes in the ledger, and a default that changes a bit changes every
  run built with the switch; no measurement says the GPU run is faster.
- **Keep ADR-0011's all-off default for good.** Keeps every run the Fortran unless asked, but makes
  the operational configuration a list each user has to know; `--kokkos all` already turns on
  parts outside NOAA's physics.
- **A named opt-in list (`--kokkos ops`) instead of a default.** Smaller, and possible as a first
  step; it does not give the "no flag set" run the owner asked for.

## Consequences

- Nothing changes until a part meets both conditions: today every default is the Fortran.
- Once a part flips, a `--ww3-gpu` build with no flag set runs it on Kokkos; reproducing the
  Fortran from that build needs `WW_KOKKOS_ALL=0`, and `scripts/ww3-gpu.sh` already sets it for the
  Fortran run.
- NOAA's operational switch is `DIST MPI`; the `W3GATH`/`W3SCAT` Kokkos path covers only `SHRD`
  (PATCH.md), so the default list holds no support routine.

## Revisit when

- the H100 matrix (ADR-0012) has run, or the first `bench/results/` record of `--ww3-gpu --gpu`
  exists;
- propagation (`W3XYP3`, `W3KTP3`) is wired, since it is the next cost after the source terms;
- GEFS-Wave's switch is read and differs from `switch_NCEP_st4`.
