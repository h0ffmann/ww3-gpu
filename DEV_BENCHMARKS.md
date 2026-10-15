# DEV_BENCHMARKS

A checkpoint sheet for the smoke tests of `scripts/hoff.sh` (`just smoke-ww3`, `just smoke-hoff`)
on CPU, GPU and Docker: which ran, which did not and why, and how long each took. The times are
developer wall times for a smoke test, not port timings: nothing here enters `kokkos/PORT_STATUS.md`
or `bench/results/`, which need a passed WW3 regtest replay on the same commit and the protocol of
`kokkos/tests/bench_snl1.cpp` (AGENTS.md, "No timing without parity").

The case is `smoke`: regtest `ww3_ts1`, one point, ST4 + NL1, NK=36 NTH=24, 10 model days
(`scripts/hoff.sh` header `(v)`). `--ww3` passes when `ww3_shel` reaches "End of program";
`--hoff` runs the same KOKKOS executable with `WW_KOKKOS_SNL1=0` and `=1` and compares the two with
`nccmp-tol` (the L2 replay).

## Checkpoint 2026-10-15, cloud container

Machine: a cloud agent container, 4 vCPU Intel Xeon @ 2.10 GHz, 15 GiB RAM, no GPU (`nvidia-smi`
absent, `lspci` shows no NVIDIA device) `(v)` `nproc`, `lscpu`, `free -g`. ww3-gpu `7fb9ee2`,
`WW3/` at `6d1bcdfe` (h0ffmann/WW3 `hoff/kokkos-snl1`), `nix-config/` at `52c5772`.

| # | Step | Environment | Command | Status | Wall time | Date (UTC) |
|---|---|---|---|---|---|---|
| 1 | Argument and refusal checks | host | `bash scripts/hoff.sh --self-test` | pass | 0.08 s | 2026-10-15 20:11 |
| 2 | `smoke-ww3` | CPU, pinned Nix `#ww3` | `just smoke-ww3` | blocked | 49 s to the error | 2026-10-15 20:11 |
| 3 | `smoke-hoff` | CPU, pinned Nix `#ww3`, Kokkos OpenMP | `just smoke-hoff cpu` | blocked | 12 s to the error | 2026-10-15 20:12 |
| 4 | `smoke-hoff --gpu`, run | GPU, Kokkos CUDA | `just smoke-hoff gpu` | not run | — | 2026-10-15 |
| 5 | `smoke-hoff --gpu`, compile only | GPU toolchain, Nix `#cuda` | `bash scripts/hoff.sh --hoff smoke --gpu --build-only` | blocked | 13 s to the error | 2026-10-15 20:13 |
| 6 | `smoke-ww3`, cold (image build + WW3 build + run) | Docker, Ubuntu 24.04 apt toolchain | see [Docker](#docker) | pass | 89 s image, 71 s build + run | 2026-10-15, about 20:20 |
| 7 | `smoke-ww3`, warm, median of 3 | Docker, same image | see [Docker](#docker) | pass | 19.0 s wall (17.9, 19.0, 19.4); `ww3_shel` 4.64 s (4.44, 4.64, 5.24) | 2026-10-15, about 20:25 |
| 8 | `smoke-hoff` | Docker | — | not run | — | 2026-10-15 |
| 9 | `smoke-hoff cpu` (h1), reference | GitHub Actions `ubuntu-latest`, pinned Nix `#ww3` | `health.yml`, `bash scripts/hoff.sh --hoff smoke --cpu` | pass, `nccmp-tol: PASS (1 judged, 25 unlisted)`, every field max\|d\| = 0 | 4 min 52 s for the step (toolchain from cache, both builds, both runs) | 2026-10-15 18:04 |

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
- **Row 4 (GPU, run).** The container has no GPU, and the script refuses `--hoff --gpu` without a
  human typing `yes` at a terminal (exit 7, AGENTS.md hard rule). It stays a step for a supervised
  run on the RTX 4090.
- **Row 5 (GPU, compile).** The `#cuda` shell fetches the CUDA 12.9 redistributables from
  `developer.download.nvidia.com`, which the network policy refuses at CONNECT `(v)` the agent proxy's
  failure log, 12 refusals for that host.
- **Row 8 (Docker, `--hoff`).** The Kokkos port needs the pinned Kokkos 5.2.2 and CMake ≥ 4.1
  (presets schema 10, `kokkos/CMakePresets.json`); Ubuntu 24.04 ships Kokkos 3.7.01 and CMake
  3.28.3 `(v)` `apt-cache policy libkokkos-dev`, `cmake --version` in the image. Building Kokkos
  from source needs github.com, refused as in rows 2 and 3.
- **`env/Dockerfile`** was not built: it clones NOAA-EMC/WW3 from github.com (refused here) and
  says itself it is untested. A Docker setup that runs `scripts/hoff.sh` is expected from a
  separate change; until it lands, row 6 is the only Docker evidence.

### Docker

Rows 6 and 7 used a throwaway image with the apt toolchain (gfortran 13.3.0, CMake 3.28.3,
netCDF-Fortran 4.5.4, Open MPI 4.1.6 `(v)` printed in the image) and the repository mounted, with
`HOFF_IN_SHELL=1` so the script skips `nix develop`. That is used for `--ww3 --cpu` only; AGENTS.md
forbids it for `--hoff --gpu`. The toolchain is therefore not the pinned one,
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
docker run --rm --network host -v "$PWD:/work" -w /work -e HOFF_IN_SHELL=1 \
  ww3-smoke:noble bash scripts/hoff.sh --ww3 smoke --cpu
```

Output of the cold run: `>> --ww3 smoke: ww3_shel finished in 4.72 s; output: ww3.196806.nc`.
Wall time is `date +%s.%N` around `docker run`, so it includes the container start and the
incremental `cmake --build`; the `ww3_shel` time is the one the script prints.

## Next checkpoint

On the RTX 4090 workstation, with the pinned Nix toolchain: rows 2, 3 and 5 again, then row 4
under supervision, and rows 6 to 8 once the Docker setup lands. Add a dated section above this one
with the same columns.
