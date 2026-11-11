# Versions

Every pinned version this repository builds and runs with, where each is pinned, and the command
that reads it again. Read on 2026-10-15 at the pins below (v). A version changes only where the
"Pinned in" column says; this file then follows, in the same pull request. A value with no
command is ⚠.

There are two toolchains. **picasso** (the `picasso/` submodule, `labs/pratico`) is the reference:
the bit-for-bit verdicts, the regtest matrix and every timing come from it. **The Docker image**
(`docker/Dockerfile`) uses Ubuntu's packages, so its compilers differ; a verdict inside one image
executable holds, but the image is not the reference (README, "build with picasso instead of
Docker").

## Sources

| Component | Version | Pinned in | Read with |
|---|---|---|---|
| WW3-metocean-7.14 (the reference, built by `--ww3`) | metocean/WW3 tag `7.14.mos-92e2cf6`, commit `92e2cf66` (2026-09-01), branch `mos-custom`: NOAA-EMC/WW3 `3eb8161` (2023-06-08) plus MetOcean's ten commits. metocean/WW3 has no 7.13 tag or branch (its tags are `7.14`, `7.14.mos-92e2cf6` and `7.14.mos-cfcd089`), read on 2026-11-11 (v); the owner asked for "7.13 MetOcean" that day, named this ref on 2026-10-29 and, asked which he meant, answered "7.14" on 2026-11-11 | the `WW3-metocean-7.14/` submodule; `.gitmodules` | `git submodule status WW3-metocean-7.14`; `git ls-remote --tags https://github.com/metocean/WW3` |
| chaceon (WW3 in Rust, built by `--chaceon`) | h0ffmann/chaceon `89a11e4` (2026-11-11), branch `main`; its own `WW3/` submodule is the WW3-metocean-7.14 commit above (v, 2026-11-11) | the `chaceon/` submodule; `.gitmodules` | `git submodule status chaceon`; `git -C chaceon ls-tree HEAD WW3` |
| NOAA's WW3 (pure WW3, built by `--ww3-noaa`; not a submodule since 2026-11-11) | NOAA-EMC/WW3 `03adda91` (2026-10-05), unchanged; moves only with the fork's base | `kokkos/src/fortran_iface/WW3_BASE`; the commit is in the fork's history, and `scripts/ww3_base.sh export` writes its tree to `WW3-noaa/`, which git ignores | `just ww3-base rev`; `scripts/ww3-gpu.sh --ww3-noaa` checks it against the fork |
| WW3 (the fork, built by `--ww3-gpu`) | `c9a6a0c6` (2026-10-15), branch `ww3-gpu/kokkos-st4` of the h0ffmann/WW3 fork, on upstream NOAA-EMC/WW3 `03adda91` | the `WW3/` submodule; `.gitmodules` | `git submodule status WW3`; `git -C WW3 merge-base HEAD origin/develop` |
| WW3 model version | 7.14: the `VERSION` file and `manual/defs.tex:1` of NOAA-EMC/WW3 `03adda91`, and `model/src/w3initmd.F90:129` (`WWVER = '7.14'`) of metocean/WW3 `92e2cf66`, which is a 2023 state of that version (v, 2026-11-11). Upstream has no 7.14 tag (its tags are `6.07` and `6.07.1`), so the version is pinned by the two commits above; read on 2026-11-11 (v) | `kokkos/src/fortran_iface/WW3_BASE` and the `WW3-metocean-7.14/` submodule | `just ww3-base export && cat WW3-noaa/VERSION WW3/VERSION`; the manual of this version is summarised in [docs/ww3-manual/](docs/ww3-manual/README.md) |
| WW3 regtest data | 7.14.2 (`ww3_from_ftp.v7.14.2.tar.gz`, SHA-256 pinned) | `WW3/model/bin/ww3_from_ftp.sh:23`; `ww3RegtestData` in `picasso/labs/pratico/flake.nix:147` | `grep ww3ver= WW3/model/bin/ww3_from_ftp.sh` |
| WW3 for ondaseventos (the `docker/Dockerfile.ondaseventos` image only) | metocean/WW3 tag `7.14.mos-92e2cf6`, commit `92e2cf66` (2026-09-01): NOAA-EMC/WW3 `3eb8161` (2023-06-08) plus MetOcean's ten `mos-custom` commits, which change ST4's arithmetic; not the WW3-noaa physics. Read 2026-10-29 (v); the ref is the owner's choice of 2026-10-29; that ondaseventos builds it was not read in its repository ⚠ | `WW3_REPO`, `WW3_REF` in `docker/Dockerfile.ondaseventos` | `docker run --rm --entrypoint cat ww3-ondaseventos /usr/local/share/ww3/BUILD_INFO`; `gh api repos/metocean/WW3/git/refs/tags/7.14.mos-92e2cf6` |
| WW4 | `74ea792` (2026-10-01), branch `develop` of the h0ffmann/WW4 fork | the `WW4/` submodule | `git submodule status WW4` |
| picasso | `be76132` (`v0.1.1-20-gbe76132`), branch `main` | the `picasso/` submodule | `git submodule status picasso` |
| nixpkgs | `6510408d147d` (nixos-unstable, 2026-10-10) | `picasso/labs/pratico/flake.lock` | `nix flake metadata ./picasso/labs/pratico` |

## The reference toolchain: picasso `labs/pratico`

All from the one nixpkgs lock above, so no version is chosen one by one.

| Component | Version | Shell | Read with |
|---|---|---|---|
| gfortran, gcc, g++ | GCC 16.2.0 | `#ww3` | `just toolchain`; `g++ --version` |
| Open MPI | 5.0.11 | `#ww3` | `just toolchain` |
| netCDF-C / netCDF-Fortran | 4.10.1 / 4.4.6-development | `#ww3` | `just toolchain` |
| HDF5 | 1.14.6 | `#ww3` | `just toolchain` |
| PT-Scotch (static) | 7.0.12 | `#ww3` | `just toolchain` |
| ParMETIS | 4.0.3-unstable-2023-03-26 | `#ww3` | `just toolchain` |
| METIS | 5.2.1 ⚠ (read from a patch name in the Nix store, not from the package) | `#ww3` | `just toolchain` prints its store path only |
| ecCodes | 2.48.0 | `#ww3` | `just toolchain` |
| CMake | 4.4.3 | `#ww3` | `just toolchain` |
| Ninja | 1.13.2 | `#ww3` | `ninja --version` |
| Kokkos | 5.2.2: Serial and OpenMP, C++20, tests off, deprecated code 4 off | `#ww3` | `just toolchain`; options in `picasso/labs/pratico/flake.nix:81-91` |
| Kokkos (CUDA) | 5.2.2: Serial, OpenMP and CUDA for `Kokkos_ARCH_ADA89` only; `nvcc_wrapper` | `#cuda` | `picasso/labs/pratico/flake.nix:92-115` |
| CUDA toolkit | 12.9 (nvcc V12.9.86), from nixpkgs' `cudaPackages`, not pinned by version | `#cuda` | `nvcc --version` |
| GoogleTest | 1.18.0 | `#ww3` | `just toolchain` |
| gdb / Valgrind | 17.2 / 3.27.1 | `#ww3` | `just toolchain` |
| Python (tooling only) | 3.14.7, numpy 2.5.2, xarray 2026.7.0 | `#ww3` | `just toolchain` |
| just | 1.58.0 | `#ww3` | `just --version` |

`just toolchain` runs `picasso/labs/pratico/justfile`'s `toolchain` recipe; `just ww3` enters the
`#ww3` shell.

## The Docker image (`docker/Dockerfile`)

| Component | Version | Stage | Pinned in |
|---|---|---|---|
| Base | Ubuntu 26.04.1 LTS, `ubuntu:26.04@sha256:f144425ff09be612d6d9ad965196e9cdc23dae1f42110a8a11a3e9a8198759f7` | all | `docker/Dockerfile:16` |
| gfortran, g++ | 15.2.0 (`15.2.0-16ubuntu1`) | `toolchain`, `build`, `dev` | not pinned: the apt archive on the day of the build |
| CMake | 4.2.3 | `toolchain` | not pinned: the apt archive on the day of the build |
| Ninja | 1.13.2 | `toolchain` | not pinned: the apt archive on the day of the build |
| netCDF-C / netCDF-Fortran | 4.9.3 (`1:4.9.3-1build2`) / 4.6.2 (`4.6.2+ds-1`) | `toolchain` | not pinned: the apt archive on the day of the build |
| HDF5 | 1.14.6 (`1.14.6+repack-2`) | `toolchain` | not pinned: the apt archive on the day of the build |
| GoogleTest | 1.17.0 | `toolchain` | not pinned: the apt archive on the day of the build |
| Kokkos | 5.2.2, Serial and OpenMP, C++20, built from the release tarball with SHA-256 `d6557aae…46b9f` | `toolchain` | `docker/Dockerfile:19-20` |
| MPI, CUDA | none | | |
| Nix | 2.34.3 (`nix-bin 2.34.3+dfsg-1`), single-user | `dev` | not pinned: the apt archive on the day of the build |
| just, ShellCheck, Python | 1.45.0, 0.11.0, 3.14.4 (`python3.14 3.14.4-1ubuntu0.2`, from `-updates`) | `dev` | not pinned: the apt archive on the day of the build |
| Trivy (the scan, not the image) | 0.76.0 | CI | `.github/workflows/docker-scan.yml:47` |

Read with: `docker run --rm --entrypoint bash ww3-gpu:dev -c 'gfortran --version; cmake --version;
dpkg-query -W libnetcdf-dev libnetcdff-dev libhdf5-dev libgtest-dev nix-bin'` after
`docker build -f docker/Dockerfile --target dev -t ww3-gpu:dev .`. The base image is pinned by digest, but
the apt packages are not: `apt-get update` in each stage installs what Ubuntu's archive serves on
the day of the build, so a rebuild can change every row marked "not pinned". Python shows it: this
table first said 3.14.3, and a dev container built later the same day had 3.14.4 from `-updates` (v, Dev Containers CLI test, 2026-10-15). Pinning them would take a dated
`snapshot.ubuntu.com` source ⚠ (not tried).

## CMake

| Item | Value | Where |
|---|---|---|
| Kokkos port (`ww_kokkos`) | `cmake_minimum_required(VERSION 3.25)`; `project(ww3_gpu_kokkos VERSION 0.1.0)`; C++20; `find_package(Kokkos REQUIRED)` with no version, so the shell's Kokkos is the pin | `kokkos/CMakeLists.txt:11-14`, `:26` |
| Presets | schema version 10: `serial-debug` (Debug, ASan and UBSan), `openmp-release` (Release, `-O3 -march=x86-64-v3`), `cuda-release` (Release, `CMAKE_CUDA_ARCHITECTURES` 89) | `kokkos/CMakePresets.json` |
| Contraction | `-ffp-contract=off` on the host, `--fmad=false` on the device | `kokkos/src/ww_kokkos/CMakeLists.txt` |
| WW3 | `cmake_minimum_required(VERSION 3.19)`; Release `-O3` for GNU | `WW3/CMakeLists.txt:4`; `WW3/model/src/CMakeLists.txt:87-89` |
| Regtest matrix | `cmake_minimum_required(VERSION 3.21)` | `regtests/CMakeLists.txt:38` |

## The workstation (not pinned)

| Component | Value | Read with |
|---|---|---|
| CPU | Intel Core i9-14900, 24 cores, 32 threads | `lscpu` |
| GPU | NVIDIA GeForce RTX 4090, 24 GiB, compute capability 8.9 (`sm_89`, `ADA89`) | `nvidia-smi --query-gpu=name,compute_cap --format=csv` |
| NVIDIA driver | 615.71.09 (the record of 2026-09-15 says 595.84) | `nvidia-smi --query-gpu=driver_version --format=csv` |

The driver comes from the host, not from picasso: the `#cuda` shell puts the host's driver
libraries on `LD_LIBRARY_PATH`. A bench record names the driver it ran with.

## CI

| Tool | Version | Where |
|---|---|---|
| Vale | 3.15.2 | `.github/workflows/ci.yml:65` |
| Trivy | 0.76.0 | `.github/workflows/docker-scan.yml:47` |
| `actions/checkout` | v7 | every workflow |
| Nix installer | `cachix/install-nix-action@v31` (upstream Nix) | `ci.yml:147`, `health.yml:44`, `pages.yml:46` |
| Nix store cache | `nix-community/cache-nix-action` v7.0.2, pinned by SHA; saved only on a push to `main` | `ci.yml:148`, `health.yml:45` |
