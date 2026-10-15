# DEV_BENCHMARKS

A checkpoint sheet for the smoke tests of `scripts/ww3-gpu.sh` (`just smoke-ww3`, `just smoke-ww3-gpu`)
on CPU, GPU and Docker: which ran, which did not and why, and how long each took. The times are
developer wall times for a smoke test, not port timings: nothing here enters `kokkos/PORT_STATUS.md`
or `bench/results/`, which need a passed WW3 regtest replay on the same commit and the protocol of
`kokkos/tests/bench_snl1.cpp` (AGENTS.md, "No timing without parity").

The case is `smoke`: regtest `ww3_ts1`, one point, ST4 + NL1, NK=36 NTH=24, 10 model days
(`scripts/ww3-gpu.sh` header `(v)`). `--ww3` passes when `ww3_shel` reaches "End of program";
`--ww3-gpu` runs the same KOKKOS executable with `WW_KOKKOS_SNL1=0` and `=1` and compares the two with
`nccmp-tol` (the L2 replay).

## Checkpoint 2026-10-15, cloud container

Machine: a cloud agent container, 4 vCPU Intel Xeon @ 2.10 GHz, 15 GiB RAM, no GPU (`nvidia-smi`
absent, `lspci` shows no NVIDIA device) `(v)` `nproc`, `lscpu`, `free -g`. ww3-gpu `7fb9ee2`,
`WW3/` at `6d1bcdfe` (h0ffmann/WW3 `hoff/kokkos-snl1`), `nix-config/` at `52c5772`.

| # | Step | Environment | Command | Status | Wall time | Date (UTC) |
|---|---|---|---|---|---|---|
| 1 | Argument and refusal checks | host | `bash scripts/ww3-gpu.sh --self-test` | pass | 0.08 s | 2026-10-15 20:11 |
| 2 | `smoke-ww3` | CPU, pinned Nix `#ww3` | `just smoke-ww3` | blocked | 49 s to the error | 2026-10-15 20:11 |
| 3 | `smoke-ww3-gpu` | CPU, pinned Nix `#ww3`, Kokkos OpenMP | `just smoke-ww3-gpu cpu` | blocked | 12 s to the error | 2026-10-15 20:12 |
| 4 | `smoke-ww3-gpu --gpu`, run | GPU, Kokkos CUDA | `just smoke-ww3-gpu gpu` | not run | — | 2026-10-15 |
| 5 | `smoke-ww3-gpu --gpu`, compile only | GPU toolchain, Nix `#cuda` | `bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --build-only` | blocked | 13 s to the error | 2026-10-15 20:13 |
| 6 | `smoke-ww3`, cold (image build + WW3 build + run) | Docker, Ubuntu 24.04 apt toolchain | see [Docker](#docker) | pass | 89 s image, 71 s build + run | 2026-10-15, about 20:20 |
| 7 | `smoke-ww3`, warm, median of 3 | Docker, same image | see [Docker](#docker) | pass | 19.0 s wall (17.9, 19.0, 19.4); `ww3_shel` 4.64 s (4.44, 4.64, 5.24) | 2026-10-15, about 20:25 |
| 8 | `smoke-ww3-gpu` | Docker | — | not run | — | 2026-10-15 |
| 9 | `smoke-ww3-gpu cpu` (h1), reference | GitHub Actions `ubuntu-latest`, pinned Nix `#ww3` | `health.yml`, `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu` | pass, `nccmp-tol: PASS (1 judged, 25 unlisted)`, every field max\|d\| = 0 | 4 min 52 s for the step (toolchain from cache, both builds, both runs) | 2026-10-15 18:04 |

Row 9 is not a run of this session: it is the `health` workflow's h1 job on `afde9d7`, whose only
difference from `7fb9ee2` is the rebuilt PDFs `(v)` `git diff --stat afde9d7 7fb9ee2`, job
[116585885594](https://github.com/h0ffmann/ww3-gpu/actions/runs/38782460050/job/116585885594), step
times read from the job record. It stands in for rows 2 and 3 until they run on a machine whose
network reaches github.com.

Why each row that did not pass:

- **Rows 2, 3 (CPU, Nix).** Building the `ww3-env` shell fetches ParMETIS from
  `github.com/KarypisLab/ParMETIS` (a source build, not in cache.nixos.org), and the container's
  network policy answers 403 for it `(v)` the log line `trying https://github.com/KarypisLab/...`
  then `curl: (22) ... 403`, and the same 403 from `curl` on the host. Nothing of WW3 was built.
  This is the container, not the repository: on a machine with open network the same command is
  the agents' regtest.
- **Row 4 (GPU, run).** The container has no GPU, and the script refuses `--ww3-gpu --gpu` without a
  human typing `yes` at a terminal (exit 7, AGENTS.md hard rule). It stays a step for a supervised
  run on the RTX 4090.
- **Row 5 (GPU, compile).** The `#cuda` shell fetches the CUDA 12.9 redistributables from
  `developer.download.nvidia.com`, which the network policy refuses at CONNECT `(v)` the agent proxy's
  failure log, 12 refusals for that host.
- **Row 8 (Docker, `--ww3-gpu`).** The Kokkos port needs the pinned Kokkos 5.2.2 and CMake ≥ 4.1
  (presets schema 10, `kokkos/CMakePresets.json`); Ubuntu 24.04 ships Kokkos 3.7.01 and CMake
  3.28.3 `(v)` `apt-cache policy libkokkos-dev`, `cmake --version` in the image. Building Kokkos
  from source needs github.com, refused as in rows 2 and 3.
- **`env/Dockerfile`** was not built: it clones NOAA-EMC/WW3 from github.com (refused here) and
  says itself it is untested. A Docker setup that runs `scripts/ww3-gpu.sh` is expected from a
  separate change; until it lands, row 6 is the only Docker evidence.

### Docker

Rows 6 and 7 used a throwaway image with the apt toolchain (gfortran 13.3.0, CMake 3.28.3,
netCDF-Fortran 4.5.4, Open MPI 4.1.6 `(v)` printed in the image) and the repository mounted, with
`WW3GPU_IN_SHELL=1` so the script skips `nix develop`. That is used for `--ww3 --cpu` only; AGENTS.md
forbids it for `--ww3-gpu --gpu`. The toolchain is therefore not the pinned one,
which makes row 7 a check that WW3 builds and runs, not a reference time. The Dockerfile, with the
proxy CA lines this container needed:

```dockerfile
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
COPY ca-bundle.crt /usr/local/share/ca-certificates/proxy.crt
ARG HTTP_PROXY
ARG HTTPS_PROXY
RUN apt-get -o Acquire::https::CAInfo=/usr/local/share/ca-certificates/proxy.crt update \
 && apt-get install -y --no-install-recommends ca-certificates build-essential gfortran g++ cmake ninja-build git bc \
      libnetcdf-dev libnetcdff-dev netcdf-bin libopenmpi-dev openmpi-bin python3 python3-pip \
 && update-ca-certificates && rm -rf /var/lib/apt/lists/*
```

```bash
docker build --network host -t ww3-smoke:noble <dir with the Dockerfile>
docker run --rm --network host -v "$PWD:/work" -w /work -e WW3GPU_IN_SHELL=1 \
  ww3-smoke:noble bash scripts/ww3-gpu.sh --ww3 smoke --cpu
```

Output of the cold run: `>> --ww3 smoke: ww3_shel finished in 4.72 s; output: ww3.196806.nc`.
Wall time is `date +%s.%N` around `docker run`, so it includes the container start and the
incremental `cmake --build`; the `ww3_shel` time is the one the script prints.

## Checkpoint 2026-10-15, RTX 4090 workstation, CPU only

Machine: Intel Core i9-14900, 32 hardware threads, NVIDIA GeForce RTX 4090 (not used: the image
has no CUDA build, and `--ww3-gpu --gpu` runs only under the owner's supervision) `(v)` `nproc`,
`lscpu`, `nvidia-smi`. ww3-gpu `7b8c0fb`, `WW3/` at `bb08623a` (h0ffmann/WW3 `hoff/kokkos-snl1`),
`WW4/` at `74ea792`. The image `ww3-gpu:local` (234 MB): Ubuntu 26.04, gfortran 15.2.0, CMake
4.2.3, Kokkos 5.2.2 built from its tarball, no Nix `(v)` `/opt/ww3-gpu/BUILD_INFO`. Wall time is
`date +%s.%N` around the `just` command, so it includes the container start and, on the host,
the incremental build; `ww3_shel` times are the ones the script prints. Each row is the median
of three runs, the three in parentheses; all ran between 21:55 and 22:33 UTC with nothing else
on the machine.

| # | Step | Environment | Command | Status | Wall time | `ww3_shel` | Date (UTC) |
|---|---|---|---|---|---|---|---|
| 1 | Image build, cold | Docker, `docker build --no-cache` | `just dbuild` with `--no-cache` | pass | 154.1 s (once) | — | 2026-10-15 |
| 2 | `smoke-ww3` | Docker image | `just dsmoke-ww3` | pass | 10.5 s (10.3, 10.5, 10.5) | 2.47 s (2.44, 2.47, 2.51) | 2026-10-15 |
| 3 | `smoke-ww3-gpu` (Kokkos OpenMP) | Docker image | `just dsmoke-ww3-gpu` | pass, byte-identical, nccmp-tol PASS | 30.5 s (30.4, 30.5, 30.6) | Fortran DIA 2.53 s (2.52, 2.53, 2.54); Kokkos DIA 2.68 s (2.67, 2.68, 2.69) | 2026-10-15 |
| 4 | `full-ww3` (bench_small, 9 520 sea points, 12 h) | Docker image | `just dfull-ww3` | pass | 146.8 s (143.4, 146.8, 148.8) | 138.12 s (135.37, 138.12, 140.64) | 2026-10-15 |
| 5 | `full-ww3-gpu` (Kokkos OpenMP) | Docker image | `just dfull-ww3-gpu` | pass, byte-identical | 508.9 s (507.7, 508.9, 509.8) | Fortran DIA 138.74 s (136.99, 138.74, 139.62); Kokkos DIA 210.54 s (209.07, 210.54, 210.73) | 2026-10-15 |
| 6 | `smoke-ww4` | Docker image | `just dsmoke-ww4` | exit 5: a part the case needs is not ported to WW4 | 0.2 s | — | 2026-10-15 |
| 7 | `smoke-ww3` | host, pinned Nix `#ww3` | `just smoke-ww3` | pass | 12.8 s (12.5, 12.8, 13.0) | 2.85 s (2.80, 2.85, 2.88) | 2026-10-15 |
| 8 | `smoke-ww3-gpu` (Kokkos OpenMP) | host, pinned Nix `#ww3` | `just smoke-ww3-gpu` | pass, byte-identical, nccmp-tol PASS | 33.1 s (33.0, 33.1, 37.7) | Fortran DIA 2.71 s (2.68, 2.71, 2.93); Kokkos DIA 2.87 s (2.86, 2.87, 3.28) | 2026-10-15 |
| 9 | Vulnerability scan | Trivy 0.76.0 on the image | `just dscan` | clean: no HIGH or CRITICAL with a fix | 10.4 s | — | 2026-10-15 |

Rows 7 and 8 are rows 2 and 3 of the cloud checkpoint, now run on a machine whose network
reaches github.com. On the CPU the Kokkos DIA is slower than the Fortran inside `ww3_shel` (row
5: 210.5 s against 138.7 s); the image's times are
not the pinned toolchain's (Ubuntu's gfortran 15.2 against Nix's 16.2), and none of these is a
port timing (see the top of this file).

## Next checkpoint

On the RTX 4090 workstation, run from the Claude CLI with the owner supervising: the GPU rows
(`just smoke-ww3-gpu gpu`, `just full-ww3-gpu gpu`), and the full 311-entry regtest matrix
(`just ww3-gpu-matrix 3 all`, WFIP-0003), which was skipped in #125 by the owner's decision and
will be recorded then, with its `docs/log` entry. Add a dated section above this one with the
same columns.
