# Two shapes for one rule {#sec:shapes}

|                         | ww3-gpu (C++/Kokkos)                                                  | chaceon (Rust)                                   |
|:---------------------|:---------------------------------------|:-------------------------------------|
| Shape                   | Inside WW3: the Fortran calls C++ through a `bind(C)` shim, one flag for each routine | Replaces WW3: no Fortran in a chaceon process    |
| Reference               | NOAA-EMC/WW3 `03adda91`, through a fork that adds 5 687 lines and removes none | MetOcean WW3 7.14, `92e2cf66`                    |
| CPU parity against      | the Fortran path of the same executable                               | a reference that is compiled separately          |
| Device targets          | Kokkos 5.2.2, CUDA backend (`sm_89`); OpenMP on the CPU               | rust-gpu (SPIR-V, Vulkan) and Rust-CUDA (PTX)    |
| Spectrum resident on the device | yes, on the OpenMP backend                                    | yes, in SPIR-V on CPU Vulkan (lavapipe)          |
| Size of the port        | 62 257 lines of C++, 355 files (2026-10-14)                           | 29 229 lines of Rust port, 5 936 lines of tools  |
| Formal methods          | SMT queries, exhaustive sweeps                                        | one Lean 4 proof about `f32` division            |
| Licence                 | MIT, LGPL-3.0 kernels                                                 | MPL-2.0                                          |

: The two ports. Sources: chaceon `docs/WW3_GPU_PARITY_202611.md` (2026-11-09) and `README.md`
at `0c098fe` [@chaceon]; ww3-gpu `kokkos/PORT_STATUS.md` at `58167d7` [@ww3gpu]. {#tbl:shapes}

| Metric                      | ww3-gpu                         | chaceon                         |
|:----------------------------|:--------------------------------|:--------------------------------|
| Repository created          | 2026-09-12 01:31 UTC            | 2026-10-23 03:58 UTC            |
| First commit on `main`      | 2026-09-11 13:47 UTC, `b43a6d3` | 2026-10-23 03:58 UTC, `3bad31a` |
| Last commit on `main`       | 2026-11-11 03:16 UTC, `efc0211` | 2026-11-09 08:53 UTC, `0c098fe` |
| Last push, any branch       | 2026-11-11 03:31 UTC            | 2026-11-09 08:53 UTC            |
| Commits on `main`           | 800                             | 110                             |

: The two repositories. Source: the GitHub REST API (`repos/h0ffmann/<repo>` for `created_at`
and `pushed_at`, `commits?sha=main` for the commits), read on 2026-11-11 at 03:35 UTC. The first
commit of ww3-gpu is older than the repository because it was made before the first push.
{#tbl:repos}

The Kokkos route keeps every WW3 program and replaces one routine at a time. Thus one regtest
can isolate the effect of one routine. The Rust route must rebuild the programs, the file formats
and the time loop before it can compare a whole run. After that, no Fortran remains that can
share state with the port.

The CPU results of the whole ww3-gpu runtime, with the resident spectrum, and their limits are
in its NOAA progress record [@noaarecord]. The DIA port is also published alone as
ww3-dia-gpu-v2 [@diagpuv2]. That repository compares the port with an earlier CUDA kernel of the
same routine.

@fig:runtimes shows the six runtimes of one command, `scripts/ww3-gpu.sh`. `--ww3` runs the
Fortran of WW3 and is the reference: each other runtime must give its bits. `--ww3-gpu` builds
the fork with the KOKKOS switch and links the Kokkos port. With `--cpu` it runs on OpenMP, and
with `--gpu` it runs on CUDA. In that executable, one flag for each routine selects the Fortran
or the Kokkos code. `--chaceon`, or `--cceon`, runs WW3 rewritten in Rust, with no Fortran in
the process, on the CPU or on a GPU. `--ww4` is for WAVEWATCH IV when it is released. Today it
builds the development code from a fork, and it stops with exit code 5 when the case needs a part
that is not ported. `--ww3` builds MetOcean WW3 7.14 at `92e2cf66`, and the script compares the
files of a `--chaceon` run with those of `--ww3`, built with chaceon's switch. Two arrows of the
figure are the design and not what the script does yet ⚠. The script compares `--ww3-gpu` with
NOAA WW3 `03adda91`, the base of its fork, until the fork is moved onto MetOcean WW3. And
`--chaceon --gpu` stops with exit code 5, because chaceon has not run on a real GPU.

![The six runtimes of ww3-gpu. A dashed arrow points to the run that must give the same bits: all
five point to `--ww3`, the reference. Today the script compares `--ww3-gpu` with NOAA WW3
`03adda91`. Drawn with Archify from `docs/archify/ww3-gpu-runtimes.json`
[@ww3gpu].](figures/runtimes.pdf){#fig:runtimes width=100%}
