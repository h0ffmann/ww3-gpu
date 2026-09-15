# 01 — Getting it built

## Two halves

The WW3 package is **the git repo plus a binary data bundle**. The repo has the Fortran and
the regression-test *configurations*; the bundle has the bathymetry, forcing, and reference
output the tests need. The bundle lives on NOAA's FTP, not in git.

```bash
git clone --branch develop https://github.com/NOAA-EMC/WW3.git ~/src/WW3
cd ~/src/WW3
./model/bin/ww3_from_ftp.sh     # the other half. Slow. Required for most regtests.
export WW3=$PWD
```

`just get` does the clone for you (into `~/src/WW3`; `WW3_DATA=1 just get` also fetches the
bundle). The repo also carries a fork of WW3 as the `WW3/` submodule, pinned to one
revision — that is the tree the Kokkos port in lessons 12–13 is written against.

**Don't use the GitHub release tarball.** The newest tagged release is 6.07.1, from April
2019. Real work happens on `develop` and `main`, and the wider community is running v7.14.x.
Clone a branch.

## Dependencies

- Fortran 90 compiler — gfortran, ifort/ifx, nvfortran, or Cray
- **CMake ≥ 3.19**
- **NetCDF ≥ 4.1.1 with Fortran bindings.** The C library alone won't do; `ww3_ounf` and
  friends need `netcdf.mod`. On Debian/Ubuntu: `libnetcdff-dev`.
- MPI for anything realistic

`bash scripts/00_prereqs.sh` (`just prereqs`) installs all of it on Debian/Ubuntu. The
recommended route, though, is the next section: every number in this course was produced
inside one pinned toolchain, and "it works on my machine" is exactly the disease a
benchmarking project cannot afford.

## Building inside the pinned toolchain

The compilers and libraries come from one locked nixpkgs revision, via the
`nix-config/labs/pratico` flake (a sparse git submodule; `just submodule-init` after a
fresh clone). Three recipes cover almost everything:

```bash
just ww3               # enter the toolchain-only shell  (== nix develop ./nix-config/labs/pratico#ww3)
just toolchain         # print the exact pinned versions
just build [switch]    # full rebuild of $WW3 with a switch file (default: switches/switch_lab_shrd)
```

What the shell pins `(v)` — this is `just toolchain` on the lab machine, see the root
`README.md`:

| Component | Pinned |
|---|---|
| gfortran | GNU Fortran (GCC) 15.3.0 |
| MPI | Open MPI 5.0.10 |
| NetCDF | netcdf-c 4.10.1, netcdf-fortran 4.4.6-development, HDF5 1.14.6 |
| METIS / ParMETIS | 5.2.1 / 4.0.3 (for `PDLIB` unstructured runs) |
| ecCodes | 2.48.0 (GRIB → netCDF, lesson 04) |
| CMake | 4.4.2 |
| Kokkos / GoogleTest | 5.2.0 / 1.18.0 (lessons 11–12; see `kokkos/README.md`) |

`just build` is `scripts/02_build_ww3.sh` run inside that shell: it deletes `build/`,
configures with `-DSWITCH=<file> -DCMAKE_BUILD_TYPE=Release`, and runs `make -j`. It always
starts from scratch, on purpose — see the next section for why. A third argument selects
`Debug`. `just rt` builds with a regtest's *own* switch file and runs the test (~30 s for
`ww3_tp1.1`), which is the fastest "is the toolchain sane" check there is.

The same shell carries Kokkos and GoogleTest, so `just kokkos-test serial-debug` (lesson 11)
builds against exactly the compilers WW3 was built with. That is not a convenience; when
lesson 12 claims the C++ kernel reproduces the Fortran *bit for bit*, the claim is only
meaningful because both sides came out of the same pinned gfortran and the same flags.

## Switch files are compile options

Before you build you pick a **switch file**: a whitespace-separated list of CPP keys that
selects the physics and the parallelism at compile time. See [`../switches/README.md`](../switches/README.md)
for the full group-by-group breakdown. Here is `switches/switch_lab_shrd`, the lab default,
one key at a time:

| Key | Group | What you are choosing |
|---|---|---|
| `F90` | language | Fortran 90 style and system calls. Always. |
| `NOGRB` | GRIB output | None. The alternatives need NCEP's GRIB libraries. |
| `NOPA` | coupling | No external coupler (NUOPC/OASIS off). |
| `LRB4` | record length | 4-byte record-length units for the binary files (`mod_def.ww3`, `restart.ww3`). Must match between programs. |
| `NC4` | netCDF | **Enable netCDF-4 output.** Without it `ww3_ounf`/`ww3_ounp` build but write nothing useful. |
| `SHRD` | parallelism | Shared memory, i.e. a serial binary. The alternative is `DIST MPI`; `OMPG OMPH` add OpenMP on top. |
| `PR3 UQ` | propagation | Third-order ULTIMATE QUICKEST with the Garden Sprinkler correction. The standard choice. |
| `FLX2` | air–sea flux | Tolman & Chalikov (1996) friction-velocity flux (`w3flx2md.F90` `(v)`), the companion of `ST2`. **Upstream pairs `ST4` with `FLX0`** — `switch_NCEP_st4`, `switch_Ifremer2`, `switch_NCEP_glwu` and all 83 regtest switch files containing `ST4` `(v)` — because `ST4` computes its own stress. `FLX2`+`ST4` here is this repo's own choice, inherited from `switches/README.md`; ⚠ whether the flux module is consulted at all under `ST4` is not verified against the manual, so treat `FLX0` as the safe edit. `FLX4` goes with `ST6`. |
| `LN1` | linear input | Cavaleri & Malanotte-Rizzoli seeding, so a spectrum can grow from calm. |
| `ST4` | **source terms** | Ardhuin et al. 2010 input and dissipation. Lesson 07. |
| `STAB0` | stability | No air–sea stability correction on the wind input. |
| `NL1` | nonlinear | The Discrete Interaction Approximation — `W3SNL1`, the first kernel this course ports (lesson 12). |
| `BT1` | bottom friction | JONSWAP empirical. |
| `DB1` | depth breaking | Battjes–Janssen. |
| `MLIM` | limiter | Miche-style shallow-water `Hs` limiter. |
| `TR0 BS0` | triads, bottom scattering | Off. |
| `IC0 IS0` | sea ice | No ice dissipation, no ice scattering. |
| `REF0` | reflection | No shoreline reflection. |
| `XX0` | extra source term | No user-defined/experimental term compiled in. |
| `WNT1 WNX1` | wind interpolation | Linear in time, linear in space. |
| `RWND` | wind | Wind is taken relative to the current. |
| `CRT1 CRX1` | current interpolation | Linear in time, linear in space. |
| `O0`–`O7`, `O14` | output/log verbosity | Extra diagnostics in `log.ww3` and stdout. Harmless; `O7` gives point-output details. |

Two things to internalise:

1. **Every key is a compile-time decision.** The build system preprocesses the `.F90`
   sources against these keys, so **changing the switch file requires a full rebuild** —
   not `make`, a full `rm -rf build` (which is what `just build` does). "I turned on ST4
   and nothing changed" is nearly always a stale build directory.
2. **That makes the switch file the first rung of the optimisation ladder.** Together with
   the compiler flags it is a *matrix* — `SHRD` vs `DIST MPI` vs `OMPG OMPH`, `-O2` vs
   `-O3 -march=native`, `NC4` on or off — and each cell of that matrix either reproduces
   the reference run bit for bit or it doesn't. Lesson 09 builds that matrix and shows how
   WW3's own regression suite is used as the gate.

## Build by hand

If you would rather not use `just`:

```bash
cd $WW3
rm -rf build && mkdir build && cd build
cmake .. -DSWITCH=/abs/path/to/ww3-lab/switches/switch_lab_shrd
make -j$(nproc)
ls bin/
```

`-DSWITCH` takes either a bare name resolved inside `$WW3/model/bin/`, or an absolute path
to your own file.

Useful variants:

```bash
cmake --build . --target ww3_shel            # rebuild one program
cmake .. -DCMAKE_BUILD_TYPE=Debug -DSWITCH=… # bounds checking, backtraces
export NetCDF_ROOT=$(nf-config --prefix)     # if CMake can't find NetCDF
```

## Prove it works

```bash
just regtest ww3_tp2.2                 # or: bash scripts/03_run_regtest.sh $WW3 ww3_tp2.2
```

That script runs each program in turn and tees its stdout, rather than hiding everything
behind the `run_test` harness. Watch what each one prints. `ww3_grid` in particular emits a
long, genuinely useful summary of the grid it built and every namelist it read — always
keep it: `ww3_grid | tee ww3_grid.out`.

## Things that go wrong

| Symptom | Cause |
|---|---|
| `Cannot open include file 'netcdf.inc'` / undefined netCDF symbols | NetCDF Fortran bindings missing, or built with a *different* compiler. `.mod` files are compiler-specific and not interchangeable. |
| `ww3_ounf` runs but writes no `.nc` | `NC4` missing from the switch file. |
| `ww3_grid` demands an obstruction file you don't have | `FLAGTR` in `namelists.nml` isn't 0. |
| Model runs, output is all zeros | Forcing never arrived. Check `log.ww3` — it prints a per-timestep table showing which inputs updated. |
| Changed physics, nothing changed | Stale `build/`. `rm -rf build`, or `just build`. |
| Changed the grid, downstream programs behave oddly | Stale `mod_def.ww3`. Rerun `ww3_grid`. |
| `error reading input file` | `.inp` vs `.nml` mismatch. |
| `nix develop` complains the submodule is missing | `just submodule-init`. |

## Building with nvfortran

Possible and worth doing as a CPU build, but you must rebuild HDF5 and NetCDF with
`nvfortran` first — distro packages are gfortran-built and their `.mod` files are unusable.
[`../gpu/build_netcdf_nvfortran.sh`](../gpu/build_netcdf_nvfortran.sh) sketches it and
[`../gpu/README.md`](../gpu/README.md) explains the sandbox around it. Lesson 10 uses the
`gpu/` directive examples as the contrast that motivates Kokkos; the GPU port itself
(lessons 11–13) does not need `nvfortran` at all — it is C++ compiled with `nvcc` inside
the pinned `#cuda` shell.

→ [`02-anatomy-of-a-run.md`](02-anatomy-of-a-run.md)
