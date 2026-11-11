# One WW3 reference submodule, MetOcean's 7.14; NOAA's WW3 from the fork's own history

2026-11-11, agent session for the owner; no issue. Goal branch
`goal/Z8BeBtzW-4817-metocean-714-submodule`. It follows
[the entry of the same day](2026-11-11-run-command-metocean-reference.md), which added the
MetOcean submodule beside the NOAA one.

## Question

The owner, on seeing three WW3 gitlinks in the repository (`WW3`, `WW3-metocean`, `WW3-noaa`):
"cleanup submodules, ios confusing, 1 metocean 1 noaa, should be only metocean for now", then
"7.14" and "change the name contain explicitly 7.14". Can the NOAA submodule go without losing
the pair of `--ww3-gpu`, which is pure NOAA WW3 at the commit the fork branched from
(AGENTS.md, "Running on the GPU")?

## What was done

Machine: the cloud container of the earlier entry (4 cores, x86_64, Ubuntu 24.04, gfortran
13.3.0, host packages, `WW3GPU_IN_SHELL=1`, no GPU).

- Removed the `WW3-noaa` submodule and renamed `WW3-metocean` to `WW3-metocean-7.14`
  (`.gitmodules`, the gitlink, every living page; this log's earlier entries keep their text).
- `kokkos/src/fortran_iface/WW3_BASE` now names the fork's base, NOAA-EMC/WW3 `03adda91`, and
  `scripts/ww3_base.sh export` (`just ww3-base export`) writes that commit's tree from `WW3/`'s
  history to `WW3-noaa/`, which `.gitignore` lists. `--ww3-noaa` calls it before it builds;
  `scripts/docker.sh` calls it where it initialised the submodule.
- Compared the tree it writes with the submodule's checkout before removing it:
  `git -C WW3 archive 03adda91 | tar -x -C <dir>; diff -rq --exclude=.git <dir> WW3-noaa`.
- In a scratch directory, cloned the fork one commit deep with the ssh URL of `.gitmodules` as
  its origin, as CI has it, and ran `scripts/ww3_base.sh export` there.
- Ran `scripts/ww3-gpu.sh --ww3-noaa smoke` with `WW3-noaa/` absent, and `--ww3 smoke`.

## Result

- The exported tree is the submodule's: `diff -rq` prints nothing `(v)`.
- In the one-commit clone the base commit was absent, the script fetched it by its SHA over
  https and wrote the same tree, in 1.7 s `(v)`.
- `--ww3-noaa smoke` wrote `WW3-noaa/`, passed the check that the fork is that tree plus the
  KOKKOS patch, built and reached the end of `ww3_shel` (4.93 s, one run, `switch_lab_ompg`);
  `--ww3 smoke` built from `WW3-metocean-7.14/` and reached it too (0.77 s, one run,
  `switch_lab_shrd`) `(v)`. Neither number is a benchmark.
- Not run ⚠: `--ww3-gpu` (no Kokkos 5 outside the pinned shell; CI's h1 runs its smoke, which
  now gets NOAA's WW3 this way), the Docker images (`scripts/docker.sh --self-test` passes; the
  Dockerfiles are unchanged and copy `WW3-noaa/` as before).
- A clone made before this change keeps its old `WW3-noaa/` checkout as an ignored directory;
  the script uses it as it is while it is at `WW3_BASE` (covered by `--self-test`).

## What it changes

- The repository has one WW3 reference submodule, `WW3-metocean-7.14/`, and the fork `WW3/`.
  "7.14" is the owner's answer to which MetOcean WW3 `--ww3` builds: tag `7.14.mos-92e2cf6`.
- `--ww3-gpu` is still compared with NOAA's WW3, because the fork is still NOAA's `03adda91`
  plus the KOKKOS patch ⚠. "Only MetOcean" for the port as well is unit U1 of
  [the plan of 2026-10-29](../plans/2026-10-29-001-metocean-upstream-plan.md): the fork moved
  onto `92e2cf66` (23 conflict hunks) and the kernels translated again from MetOcean's Fortran.
  It is asked of the owner, not started.
- The kernels' heritage headers and the glossary still cite `WW3-noaa/model/src/<file>.F90`;
  the path opens after `just ww3-base export`. They were not rewritten: a cleanup does not touch
  translated code.
- The drawings `docs/archify/ww3-gpu-runtimes*.json`, `ww3-gpu-at-a-glance*.json`,
  `ww3-gpu-vs-noaa-architecture.json` and `ww3-gpu-parity-workflow.json` still label a
  `WW3-noaa/` or `WW3-metocean/` box ⚠; a redraw is the figures' work.
