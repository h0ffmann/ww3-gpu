# `W3SIN4`'s stress scan with one sea point per vector lane: bit-identical, 5.1 % off the one-thread `W3SRCE` kernel

2026-10-18, for M. Hoffmann (goal `c3e7eecf-6142`), unit L2 of
[the points-in-lanes plan](../compound/plans/2026-10-18-005-srce-points-in-lanes-plan.md).

## Question

Does the stress scan of `W3SIN4` (`w3src4md.F90:696-797` at `WW3@50a10730`, the loop over `IK`
and `ITH` that builds `DSTAB`, `LLWS` and the wave-supported stress) give the Fortran's bits and
take less time when eight sea points run it at once, one per vector lane?

## What was done

- `w3sin4_point` (`kokkos/src/ww_kokkos/st4_sin4.hpp`) split into `sin4_prep`, `sin4_scan` and
  `sin4_tail`, whose bodies are the routine's lines moved without a change (v, each of the three
  ranges found verbatim, in order, by a line comparison); `sin4_detail::Mid` carries the locals
  from one part to the next by copy.
- `sin4_scan_lanes<8>`: the scan for up to eight points, the `IK` loop outside, then per `IK` a
  loop over the lanes for the per-`IK` values (`powf`, `atan2f`, `sincosf`, `logf` called per
  lane), then the `ITH` loop on GCC vector types of eight `REAL` (`V8`, `M8`). Each vector
  operation is the scalar statement's, lane by lane, operands in the same order; a branch is a
  select of the value its taken side computes; `expf` and `powf` are called per lane, for the
  lanes whose branch calls them; each lane keeps its own stress sums, added in `ITH` order.
- `w3sin4_lanes<8>` runs `sin4_prep` and `sin4_tail` per point around the lane scan, and
  `w3srce_lanes` (`srce_kernel.hpp`) calls it at both `W3SIN4` sites of the integration step
  (`:1239` and `:2007`) for the points still in the loop. The call in the initialisation
  (`:1100`, only with `SINTAIL4 > 0.5`) stays per point.
- Why explicit vectors: written as plain loops over the lanes, g++ left the `ITH` body scalar
  ("unsupported control flow in loop", `-fopt-info-vec-all`). It sinks the values only one side of
  a select uses into a branch and, under its default `-ftrapping-math`, does not speculate them
  back (v: the same loop in a standalone file vectorises with `-fno-trapping-math` and not
  without). Changing that flag would change it for every kernel of the library; the vector types
  change only this loop.

## Result

- (v) Bits, 8 points per team (`WW_KOKKOS_SRCE_LANES=8`), `WW_KOKKOS_STRICT=1
  WW_KOKKOS_VA_POISON=1`: `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident` and the
  same on `full` give every model file byte-identical to the Fortran of the same executable
  (`out_grd.ww3`, `out_pnt.ww3`, `restart.ww3`, `ww3.196806.nc`; `out_grd.ww3`, `ww3.field.nc`), and
  every file but the NetCDF byte-identical to `--ww3`'s, `nccmp-tol` PASS. Both the scalar
  transcription and the vector form were run.
- (v) Time: `srce.batch.kernel` (`WW_KOKKOS_TIMERS=1`), `full`, one thread pinned to a performance
  core (`taskset -c 2`), runs alternated, Intel Core i9-14900, 2026-10-18 21:33 to 21:57 UTC, load
  9.8 at the first run's start and 1.9 to 3.5 after:

| `WW_KOKKOS_SRCE_LANES` | Runs (s) | Median |
|---|---|---|
| 1 | 105.21, 105.17, 105.11 | 105.17 s |
| 8 | 98.67, 100.42, 99.77 | 99.77 s |

- (v) Every run's `out_grd.ww3` is byte-identical to the Fortran-matched output of the timing
  build (`cmp`).
- ⚠ 5.4 s of the kernel's 105.2 s, 5.1 %. `W3SIN4`'s own code was 12.9 % of the batch in the S0
  profile ([2026-10-18-srce-lanes-s0-profile](2026-10-18-srce-lanes-s0-profile.md)), so the lane
  scan removes about 40 % of `W3SIN4`'s time: derived from S0's share, not measured on its own.
- (v) `bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --kokkos resident --build-only`: the CUDA
  build compiles and links with no warning. The lane code is host-only; nothing ran on a device.
- (v) `just kokkos-test serial-debug` and `openmp-release`: 11/11 each, no warning. The first
  `serial-debug` build warned `-Wpsabi` (a function returning a 32-byte vector without AVX, as
  that preset has no `-march`); `vload` now writes through a reference, and the bits and CUDA
  checks above were run again on that final code.
- ⚠ The timing below ran on the code before that change to `vload`, which only changes how an
  inline helper hands back its vector; it was not timed again.
- ⚠ An unpinned one-thread run is no measure on this CPU: the same code took 113 s to 155 s in four
  earlier runs, as the thread moved between performance and efficiency cores. No 32-thread
  figure is given: the machine was shared during those runs.

## What it changes

The lane layer pays where a per-point loop has ordered sums and branches g++ will not vectorise.
`W3SDS4` (24.8 % of the batch in S0) is the next unit.

## How to repeat

`lanes_1t.sh`, after `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident` (which
builds and stages the case):

```bash
#!/usr/bin/env bash
# One-thread W3SRCE kernel time, WW_KOKKOS_SRCE_LANES 1 against 8, alternated, full case.
set -euo pipefail
repo=/home/hoffmann/code/ww3-gpu-2
exe="$repo/build/ww3-gpu-openmp/ww3/bin/ww3_shel"
case_dir="$repo/build/runs/ww3-gpu-full-openmp/kokkos"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/lanes-1t"
mkdir -p "$here"
for rep in 1 2 3; do
  for w in 1 8; do
    d="$here/w$w-r$rep"; rm -rf "$d"; mkdir -p "$d"
    for f in ST4TABUHF2.bin depth.inp mask.inp mod_def.ww3 namelists.nml ww3_shel.nml; do cp "$case_dir/$f" "$d/"; done
    l0=$(cut -d' ' -f1 /proc/loadavg)
    (cd "$d" && taskset -c 2 env OMP_NUM_THREADS=1 OMP_PROC_BIND=false WW_KOKKOS_ALL=0 WW_KOKKOS_PRESET=resident \
       WW_KOKKOS_STRICT=1 WW_KOKKOS_TIMERS=1 WW_KOKKOS_SRCE_LANES=$w "$exe" > ww3_shel.out 2> ww3_shel.err)
    k=$(grep -E "srce.batch.kernel" "$d/ww3_shel.err" | head -1 | tr -s " ")
    same=$(cmp -s "$d/out_grd.ww3" "$case_dir/out_grd.ww3" && echo identical || echo DIFFERENT)
    echo "rep $rep lanes $w: [$k], out_grd.ww3 $same, load $l0 -> $(cut -d' ' -f1 /proc/loadavg)"
  done
done
```
