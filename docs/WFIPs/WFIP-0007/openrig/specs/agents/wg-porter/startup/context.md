# Start here: porter

Read these before you change anything:

1. The norms in the `AGENTS.md` of your working folder: the box's, then ww3-gpu-port's.
2. `ww3-gpu/.omp/AGENTS.md` and `ww3-gpu/AGENTS.md`.
3. `ww3-gpu/.claude/skills/ww3-port/SKILL.md` and `ww3-gpu/.claude/commands/port-next.md`.
4. For an `mpi` item, `ww3-gpu/docs/plans/2026-11-01-002-mpi-kokkos-plan.md` and ADR-0020 in
   `ww3-gpu/docs/ADRs.md`.

## Limits of this box

- The daemon and three seats share one memory budget (`MemoryHigh=3G` at piabrota2). One build at
  a time across the rig, under `flock "$WW3PORT_BUILD_LOCK"`, with one job.
- No Docker and no GPU. The toolchain (gfortran, CMake, Kokkos, MPI) comes from the seat profile,
  at the versions ww3-gpu's `picasso/` pins. If a tool is missing, block the item and say which.
- You read ww3-gpu and h0ffmann/WW3 through `ww3port init`'s read token. You cannot push.
