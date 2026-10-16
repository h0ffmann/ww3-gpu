# The ST4 checkpoint, first half: byte-identical on ww3_ts1 and bench_small, the matrix running

2026-10-15, the owner with an agent. Issue #45, [ADR-0011](../ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md),
[`ST4_KOKKOS_PLAN_202610.md`](../ST4_KOKKOS_PLAN_202610.md) §10.3 and §12.

## Question

Does the ST4 port (PR #135) give the Fortran's bytes inside WW3, one flag at a time and all
together, at the checkpoint ADR-0011 asks for before the next unit?

## What was done

- The `WW3/` pin moved from `bb08623a` (`hoff/kokkos-snl1`) to `c9a6a0c6` (`ww3-gpu/kokkos-st4`),
  which carries the `W3SNL1` hunks and the ST4 hunks; the fork's §10.2 changes were already
  there (`KOKKOS` no longer requires `NL1`, `find_package(ww_kokkos 0.2)`, `FLX5` refused at
  configure time).
- `WW3GPU_ROUTINES` (a subset of `SNL1 SDS4 SIN4 SPR4`, default `SNL1`) selects the Kokkos
  routines in `scripts/ww3-gpu.sh`, `scripts/docker.sh` and the matrix driver in `regtests/`: the
  Fortran run sets every `WW_KOKKOS_<R>` to 0, the Kokkos run sets those of the list to 1; the
  verdict asks for W3INIT's log line of each routine the entry's switch builds; the matrix selects
  the entries whose switch has the routines' physics word (`NL1` for `SNL1`, `ST4` for the others)
  and records a `kokkos_spectra` count per routine.
- Machine: the owner's i9-14900, CPU only, OpenMP backend, `picasso/labs/pratico`
  (gfortran 16.2.0, Kokkos 5.2.2). Commit: ww3-gpu `4b496a4` with these changes uncommitted.

## Result

- (v) `just smoke` (`--ww3`, then `--ww3-gpu` with `SNL1`): byte-identical, `nccmp-tol` PASS
  against `--ww3`. The DIA's verdict holds at the new pin.
- (v) `WW3GPU_ROUTINES=<R> bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu` (`ww3_ts1`) with
  `SDS4`, `SIN4`, `SPR4`, `SDS4,SIN4,SPR4`, `SNL1,SDS4,SIN4,SPR4`: the two outputs byte-identical
  in all five, and `nccmp-tol` PASS against `--ww3`.
- (v) The same five with `--ww3-gpu full --cpu` (`bench_small`, 120 x 80, 12 h): byte-identical in
  all five, `nccmp-tol` PASS against `--ww3`.
- The matrix, `WW3GPU_ROUTINES=SDS4,SIN4,SPR4 just ww3-gpu-matrix 8 all` (137 entries, strict),
  started at 23:03 local time and was running when this entry was written ⚠. Its record will be
  `docs/data/` and an update below.
- No timing is quoted: the matrix has not passed, and no run was repeated three times.
- `just ww3-gpu-matrix-selftest` ok (new fixtures for the `ST4` selection); `ww3-gpu-matrix-list`
  gives 47 serial, 10 OpenMP, 80 MPI, 137 entries with `ST4` at `c9a6a0c6` (v), the plan's counts.

## What it changes

`kokkos/ports.tsv`: every ST4 row is now `wired` (WW3 at the pin calls it); `regtested` stays `no`
until the matrix is recorded. The website's "ported" share moves from 48.4 % to 87.4 % of the run
time; "regtested" stays 48.4 % (`scripts/port_score.py`). Next: the matrix record, then the ST4
rows to `regtested`, then the W3SRCE checkpoint (PR #136).
