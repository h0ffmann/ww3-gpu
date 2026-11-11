# Rules of every ww3-gpu-port seat

These rules add to the rules of every piabrota seat. They apply to work on h0ffmann/ww3-gpu, the
C++/Kokkos port of WAVEWATCH III (WW3), and on its fork h0ffmann/WW3. A human or Hermes can change
them only through a pull request to piabrota/nix-os.

## The repository binds you

- `ww3port init` made `ww3-gpu/` in your working folder: a clone of h0ffmann/ww3-gpu with a
  read-only token, its `WW3/` submodule at the pin, on the rolling branch. Fetch with
  `git -C ww3-gpu fetch`; never clone again by hand.
- Before any change, read `ww3-gpu/.omp/AGENTS.md`, then `ww3-gpu/AGENTS.md`, which it imports.
  They bind you. omp loads `.omp/` only from its own working folder, which is this folder and not
  the clone, so read them yourself. Read `ww3-gpu/.claude/skills/ww3-port/SKILL.md` and
  `ww3-gpu/.claude/commands/port-next.md` too: omp does not find them by itself here.
- `ww3-gpu/WW3/`, `WW3-metocean-7.14/`, `chaceon/`, `WW4/` and `picasso/` are references. Read
  them. Never edit them. Fork work happens in `ww3-gpu/WW3/` on a branch `ww3-gpu/port-<slug>` and
  leaves the box as a bundle, never as an edit to the pin.
- Never touch `pubs/proposal/`, `kokkos/tools/nccmp-tol/`, `test.comp`, `regtests/` or any
  tolerance. A task that needs one is blocked: `ww3port block <item> --reason "<what>"`.

## The queue is the only source of work

- Work only on the item `ww3port next` gave you. Run `ww3port beat --as <seat>` after each step,
  so your claim stays alive. A claim with no beat for 4 hours goes to another seat.
- Never take a routine by reading `kokkos/PORT_TABLE.tsv` yourself, and never edit
  `kokkos/port_claims.tsv`: `ww3port` does both.
- `ww3port next` exits 3 when nothing is free. Then say "queue empty" and stop. Do not invent work.

## What this box checks, and what it leaves for selection

- You run `just kokkos-test serial-debug`, `just kokkos-test openmp-release`,
  `python3 scripts/port_score.py --write` and `--check`, `shellcheck` on a changed script, and the
  static parity argument of ADR-0016 (the `-O3` GIMPLE or disassembly of the Fortran object and of
  the C++). Run each build under `flock "$WW3PORT_BUILD_LOCK"`, one at a time across the rig.
- You never run a WW3 regtest: no `just smoke*`, `full*`, `perf*`, `ww3-gpu-matrix*`, no
  `scripts/ww3-gpu.sh` run, no `mpirun`. They run only on the batch the owner selects, on the
  owner's workstation. The omp rules of this rig refuse them.
- Never run anything on a GPU. Never pass `--gpu`, `--device`, `WW3GPU_GPU_CONFIRMED` or
  `WW3GPU_IN_SHELL`.
- So every row you change keeps `regtested` as `no: postponed (ADR-0016)`, and every gate you did
  not run is in the port report with `⚠`.

## Memory

The daemon and three seats share the unit's memory. One build at a time (the lock above), with
`CMAKE_BUILD_PARALLEL_LEVEL=1`. If a build is killed for memory, block the item with the reason and
tell the reviewer; do not retry it in a loop.

## Branches and handoff

- Your branch is `goal/<issue>-port-<slug>` from the rolling tip, where `<issue>` is the umbrella
  issue `ww3port next` prints. Commit with this repository's conventions: a subject, a body, a
  `Tested:` trailer naming only the commands you ran on this box. Never a `Cost:` trailer.
- You never push. `ww3port submit` writes the bundle in `$WW3PORT_OUTBOX`; Hermes fetches it.

## Never

- Never change a reduction order, a limiter or an integration order: block and ask the owner.
- Never widen a tolerance, add a test of a ported routine (ADR-0010) or skip a gate.
- Never print the token that `ww3port init` uses.
