# Restart with the resident spectrum: the `SHRD` restart pair, run for the first time

2026-10-18, an agent for M. Hoffmann (goal `5afd9379-8152`); gap 10 of
[`PRODUCTION_READINESS_202610.md`](../PRODUCTION_READINESS_202610.md). The pair was written in
[the `abbb0456` handover](2026-10-18-goal-abbb0456-handover.md) ("Waiting on the owner") and listed
again in [the `c3e7eecf` handover](2026-10-18-goal-c3e7eecf-handover.md); neither had run it.

## Question

Does a run restarted from its own restart file give the bytes of the run it was cut from, with the
resident Kokkos preset, and do the Kokkos runs give the Fortran's bytes of the same executable?
No `matrix.base` pair answers it: every restart-reproduction entry at the pin is `MPI` (`ww3_tp2.17`,
`ww3_ufs1.1`), where the resident parts do not run (ADR-0017).

## What was done

On the owner's machine (Intel Core i9-14900, shared with two other sessions), in the pinned `ww3`
shell (gfortran 16.2.0), Kokkos OpenMP backend, never `--gpu`, from a clean worktree at `b828041`
(`main`) with `WW3/` at `50a10730`. The wrapper `test-191-restart.sh` of the earlier handovers
points at the checkout of PR #191, so `scripts/restart_pair.sh` ran from this worktree instead, with
the wrapper's arguments:

```bash
WW3GPU_SWITCH=switch_lab_shrd nix develop ./picasso/labs/pratico#ww3 --command \
  bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident --build-only
WW_KOKKOS_VA_POISON=1 OMP_NUM_THREADS=4 nix develop ./picasso/labs/pratico#ww3 --command \
  bash scripts/restart_pair.sh
```

The build's switch was `SHRD ... KOKKOS` (`build/ww3-gpu-openmp/switch`). The script runs
`bench_small` (120 x 80, 9 520 sea points, 12 hours) four times with that one executable: `a` from
00:00 with restarts at 06:00 and 12:00, and `b` from 06:00 on `a`'s restart, each for the Fortran
(`WW_KOKKOS_ALL=0`) and for the resident preset (`WW_KOKKOS_STRICT=1`, every `*_COUNT=1`). It
compares with `cmp`.

The first attempt exited 1 after `fortran/b` with no message. The cause is the script's plumbing:
it read the "Kokkos parts" line from `ww3_shel.out`, where WW3 does not print it (it is in
`log.ww3`, as `scripts/ww3-gpu.sh:658` reads it), and `grep` under `pipefail` ended the script
silently. The fix reads `log.ww3` and tolerates a missing line, which the next test then reports.
What is compared did not change.

## Result

Exit 0 (v, the command above, 4 threads; the four runs took about 50 s each):

| Comparison | Files | Result |
|---|---|---|
| restart reproduction, Fortran: `b` against `a` | `out_grd.ww3` (2 594 586 bytes), the 12:00 restart (21 618 432 bytes) | identical |
| restart reproduction, resident Kokkos: `b` against `a` | the same two | identical |
| Kokkos against Fortran, `a` | `out_grd.ww3`, `restart001.ww3`, `restart002.ww3` | identical |
| Kokkos against Fortran, `b` | `out_grd.ww3`, `restart001.ww3` | identical |

The Kokkos runs had `SRCE`, `CPRT`, `OUTG`, `IOPE`, `XYP3`, `KTP3`, `UPDV` on Kokkos (7 of 20, the
"Kokkos parts" line of `log.ww3`), no call left on the Fortran (`SRCE=0 CPRT=0`), one host-to-device
copy of `VA` per run, two device-to-host copies in `a` (the two restart writes) and one in `b`; 81
host copies poisoned with NaN in `a` and none read back. So a missed sync after the restart read
would have shown as NaN or a different byte.

## Open questions

- One `SHRD` case, one restart, OpenMP backend. The CUDA backend, `ww3_multi`, `MPI` and a restart
  that moves the grid are not covered ⚠; ADR-0017 stops the resident preset under `MPI`.
- The pair compares bytes of a 12-hour case at 4 threads; other thread counts were not run ⚠.
- The matrix's `rstrt_b4b` pairs remain `MPI` and out of reach of the preset.
- `README.md` and `docs/NOAA_PROGRESS_RECORD_202610.md` may still say the pair was never run; not
  edited here.
