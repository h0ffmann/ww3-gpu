# Toolchain: reproducible gfortran, OpenMPI and NetCDF

The compilers and libraries come from one locked nixpkgs revision, through the
[`nix-config/labs/pratico`](https://github.com/h0ffmann/nix-config/tree/main/labs/pratico) flake in
`nix-config`, a sparse git submodule. `scripts/00_prereqs.sh` (`just prereqs`) is the host-package
alternative on Debian/Ubuntu. The `justfile` at the repo root wraps the flake:

```bash
just up          # bump the submodule pin to origin/main and stage it  (just st = status)
just ww3         # enter the toolchain-only shell  (== nix develop ./nix-config/labs/pratico#ww3)
just ww3-run …   # run one command inside that shell, e.g. just ww3-run gfortran --version
just toolchain   # exact pinned versions
just smoke       # Fortran 2008 + MPI + NetCDF-4 build-and-run in the Nix sandbox
```

Build and run WW3 itself in that shell. The source tree is `$WW3`, else `~/src/WW3`, the
plain upstream NOAA-EMC clone; pass `WW3` as the last argument to use the fork submodule:

```bash
just get                 # clone upstream develop into ~/src/WW3 (WW3_DATA=1 to also fetch the FTP bundle)
just rt                  # the simple regtest: build with ww3_tp1.1's own switch, run it (~30 s on 32 cores)
just rt ww3_tp2.2 PR3_UQ # another test / switch_<sw> from its input/ dir;  ... PR3_UQ WW3 = on the fork
just build [switch]      # full rebuild with a switch file (default switches/switch_lab_shrd)
just regtest [test]      # rerun a test step by step against the current build (no rebuild)
```

`ww3_tp1.x` and `ww3_tp2.2` need no FTP data. Output lands in `<ww3>/regtests/<test>/work_lab/`;
for `ww3_tp1.1` the gridded `ww3.196806.nc` should show `hs` starting at 2.5 m on the equator row.

The C++/Kokkos tree and the benchmark tooling run in the same shell:

```bash
just kokkos-test serial-debug        # configure + build + ctest (sanitizers, deterministic reductions)
just kokkos-test openmp-release      # the same on the OpenMP backend, -O3
just kokkos-cuda-test                # cuda-release in the #cuda shell (RTX 4090)
just bench-case --size small -o bench/case_small   # a self-contained WW3 benchmark case
just bench                           # kernel proxies + real WW3 MPI scaling (bench/run_all.sh)
```

What `just toolchain` prints on the lab machine `(v)`:

```
$ just toolchain
ww3 toolchain: GNU Fortran (GCC) 15.3.0 | mpirun (Open MPI) 5.0.10 | netcdf-c 4.10.1 / netcdf-fortran 4.4.6-development
nixpkgs      eaad089433ca2bb662274377d33df3d0e51ef28b
gfortran     GNU Fortran (GCC) 15.3.0
openmpi      mpirun (Open MPI) 5.0.10
netcdf-c     netCDF 4.10.1
netcdf-f     netCDF-Fortran 4.4.6-development
hdf5         h5dump: Version 1.14.6
metis        /nix/store/kakzikafyrpx9pp49kxxgmjvvnymm6vr-metis-5.2.1
parmetis     /nix/store/rdkv3jg7b52dw0qlkwx9f1aihjsm7xwb-parmetis-4.0.3-unstable-2023-03-26
eccodes      2.48.0
cmake        cmake version 4.4.2
python       Python 3.14.7 numpy 2.5.1 xarray 2026.7.0
```

Nix prints `warning: Git tree '…/nix-config' has uncommitted changes` because of the sparse
checkout. There are no real edits, and `git -C nix-config status` is clean.

The `WW3/` submodule is the fork, kept in step with upstream by `just src-sync`
(`just src-st` shows pinned vs. fork vs. upstream). Pass `WW3` as the last argument of
`build`, `regtest` or `rt` to build the fork instead of `~/src/WW3`.
