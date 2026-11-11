# One run command, six runtimes (November 2026)

`just run` is `scripts/ww3-gpu.sh`: one command that runs a wave case in any of six runtimes, on a
regular or a triangular grid, with wind fetched fresh from ECMWF and sea surface temperature from
the Copernicus Marine Service. The owner asked for it on 11 November 2026, for the two systems
that run WAVEWATCH III from this repository: ondaseventos (LabECO/UFSC) first, marola second
([`IFS_FORCING_202611.md`](IFS_FORCING_202611.md), "Who consumes ww3-gpu").

Everything below marked `(v)` was run or read on 2026-11-11 in a cloud container: 4 cores,
x86_64, Ubuntu 24.04, gfortran 13.3.0, CMake 3.28.3, netCDF, NCO and ecCodes from apt, cargo
1.97.0, with `WW3GPU_IN_SHELL=1` so the script skips `nix develop`. That is the host alternative
of [`TOOLCHAIN.md`](TOOLCHAIN.md), not the pinned toolchain: the pinned shell could not be built
there (the network policy of the container refused one source archive of the Nix closure) ⚠. So
nothing here is a reference time or a bit-for-bit verdict, and no GPU was used.

## The six runtimes

```bash
just run --ww3      <case> [--cpu]
just run --ww3-gpu  <case> [--cpu|--gpu] [--kokkos all|none|<PART,...>]
just run --chaceon  <case> [--cpu|--gpu]          # --cceon is the same flag
just run --ww4      smoke|full [--cpu]
# <case> is smoke, full, coast, or --case <dir> [--wind ifs|<file.nc>] [--temp cmems|<file.nc>]
```

| Runtime | Source | What runs | State on 2026-11-11 |
|---|---|---|---|
| `--ww3` | `WW3-metocean-7.14/`: metocean/WW3 `92e2cf66`, tag `7.14.mos-92e2cf6` | MetOcean's WW3 as written, Fortran, `switches/switch_lab_shrd` | `(v)` `smoke`, `full`, a regular grid and a triangular grid on IFS wind reached "End of program" |
| `--ww3-gpu --cpu` | `WW3/`: the h0ffmann/WW3 fork, plus `kokkos/` | the Fortran and the Kokkos port in one executable, Kokkos on OpenMP, compared with each other and with `--ww3-noaa` | ⚠ not built in that container (no Kokkos 5 without the pinned toolchain); `health.yml` runs `smoke` on every pull request |
| `--ww3-gpu --gpu` | the same | the same on Kokkos' CUDA backend | ⚠ not run: only with the owner supervising (AGENTS.md, "Running on the GPU") |
| `--chaceon --cpu` | `chaceon/`: h0ffmann/chaceon `89a11e4` | WW3 in Rust (`ww3_grid`, `ww3_strt`, `ww3_prnc`, `ww3_shel`), compared with `--ww3` built with chaceon's switch | `(v)` `smoke` and a regular grid on IFS wind ran; the comparison is counted, not judged, on x86_64 (below) |
| `--chaceon --gpu` | the same | nothing: exit 5 | `(v)` "Nothing has run on a real GPU, the PTX has never executed" (`chaceon/README.md:37` at `89a11e4`; its SPIR-V kernels run on CPU Vulkan), so there is no GPU run to start |
| `--ww4` | `WW4/`: h0ffmann/WW4 | WAVEWATCH IV, when it is released: exit 5 while a part the case needs is not ported (`scripts/ww4_parts.py`) | unchanged by this work |

A seventh mode is not a runtime. `--ww3-noaa` builds pure NOAA-EMC/WW3 at `03adda91`, the commit
the fork branched from, and is what `--ww3-gpu` is compared with pair to pair. NOAA's WW3 has no
submodule since 11 November 2026 (the owner: "should be only metocean for now"): the commit is in
the fork's own history, `kokkos/src/fortran_iface/WW3_BASE` names it, and `scripts/ww3_base.sh`
(`just ww3-base export`) writes its tree to `WW3-noaa/`, a directory git ignores, before
`--ww3-noaa` builds `(v)` the tree it writes is the removed submodule's, `diff -rq`, 2026-11-11. Until 11 November 2026 that mode was named `--ww3`; a log entry, a plan or an ADR written
before that date means NOAA's WW3 by `--ww3`.

`--cpu` is the default everywhere. `--ww3 --gpu`, `--ww3-noaa --gpu` and `--ww4 --gpu` are
refused (exit 64): none has a GPU path.

## Which MetOcean WW3: 7.14

The owner's words were "--WW3 always on 7.13 MetOcean". metocean/WW3 has no 7.13: its tags are
`7.14`, `7.14.mos-92e2cf6` and `7.14.mos-cfcd089`, and its branches are `develop` and `mos-custom`
`(v)` `git ls-remote https://github.com/metocean/WW3`. NOAA-EMC/WW3 has no 7.13 tag either (its
tags are `6.07` and `6.07.1`) `(v)`. The submodule is therefore pinned to `7.14.mos-92e2cf6`
(`92e2cf66`, 2026-09-01), the ref the owner chose for ondaseventos on 2026-10-29
([log](log/2026-10-29-metocean-ww3-714-mos.md)), the one `docker/Dockerfile.ondaseventos` builds
and the one chaceon's own `WW3/` submodule holds `(v)` `git -C chaceon ls-tree HEAD WW3`. Asked
which ref "7.13" meant, the owner answered "7.14" on 11 November 2026 and asked that the
submodule's name say so: it is `WW3-metocean-7.14/`. `model/src/w3initmd.F90:129` at that commit
reads `WWVER = '7.14'`.

## Why `--ww3-gpu` is still compared with NOAA's WW3

The fork `WW3/` is NOAA's `03adda91` plus the KOKKOS patch. MetOcean's `92e2cf66` is NOAA's
`3eb8161` of June 2023 plus ten commits that change the arithmetic of ST4
([log](log/2026-10-29-metocean-ww3-714-mos.md)). The Kokkos kernels translate NOAA's 2026
arithmetic, so `--ww3` cannot be their bit-for-bit pair. Moving the fork is unit U1 of the
[plan of 2026-10-29](plans/2026-10-29-001-metocean-upstream-plan.md), and it is not a mechanical
rebase: the fork's diff applied to `92e2cf66` leaves 23 conflict hunks in four files
(`w3srcemd.F90` 8, `w3wavemd.F90` 13, `w3initmd.F90` 1, `w3iotrmd.F90` 1) `(v)`:

```bash
git -C WW3 diff 03adda91 96892c95 > patch.diff      # WW3/ needs both commits fetched
git -C <a clone of metocean/WW3 at 92e2cf66> apply --3way patch.diff
grep -c '^<<<<<<<' model/src/w3{srce,wave,init,iotr}md.F90
```

`w3srcemd.F90` and `w3wavemd.F90` are where the kernels are called, and the kernels themselves
would then have to be translated again from MetOcean's Fortran (plan units U3 to U5). This change
does unit U2 only: the reference `--ww3` builds is MetOcean's, in the one WW3 reference submodule,
and the fork keeps its base. Until U1, a difference between `--ww3` and `--ww3-gpu` on the same case is
MetOcean's ten commits and three years of NOAA's development, not the port ⚠.

## A case of your own: `--case <dir>`

`<dir>` holds `ww3_grid.nml`, `ww3_shel.nml` and the files they name, with paths relative to the
directory, and optionally `ww3_strt.inp`, `ww3_ounf.nml`, `ww3_ounp.nml`, `points.list` and
`ww3_prnc_<field>.nml`. The script copies it to `build/runs/<mode>-<name>/`, where `<name>` is
the directory's name, and never writes into it.

| Grid | `GRID%TYPE` | `--ww3` | `--ww3-gpu` | `--chaceon` |
|---|---|---|---|---|
| regular | `RECT` | `(v)` ran | runs every part ⚠ not run here | `(v)` ran |
| curvilinear | `CURV` | ⚠ not run | ⚠ not run | ⚠ not run |
| triangular | `UNST`, a Gmsh `.msh` | `(v)` ran | the per-point parts run; the `resident` preset refuses the grid (`kokkos/src/fortran_iface/PATCH.md`, section 30) ⚠ not run here | refused, exit 5: chaceon has no Rust for the unstructured propagation at its pin (`chaceon/README.md:50`, 0 of 10 108 lines) |

The triangular grid is WW3's unstructured grid, whose explicit and implicit schemes on triangles
are in `model/src/w3profsmd.F90`; the owner's "triangle grids like the one proposed by dr Aron
from NOAA" is read as that grid type ⚠ (asked). The script takes the type from the namelist, so a
case needs no flag for it.

## Wind: `--wind ifs` is `just wind`

`just wind` (an alias of `just ifs-get`, [`IFS_FORCING_202611.md`](IFS_FORCING_202611.md)) fetches
one ECMWF IFS cycle from ECMWF Open Data and keeps what it fetched: a GRIB step is never
downloaded twice, the netCDF file is rebuilt only when its parameters change, and each box has
its own file, so the `wind.nc` of an earlier run directory keeps pointing at that run's wind.
`--wind ifs` calls it for the case:

1. The box is the grid's, padded by 1°: from `RECT%NX`, `NY`, `SX`, `SY`, `X0`, `Y0` of a regular
   grid, or from the nodes of the `.msh` of a triangular one. Only a spherical grid inside
   -180° to 180° gets a box ⚠; any other gets the whole globe.
2. The length is `DOMAIN%START` to `DOMAIN%STOP` of `ww3_shel.nml`, rounded up to an IFS step.
3. The cycle is the newest 00z whose last step is published (`WW3GPU_IFS_DATE` and
   `WW3GPU_IFS_CYCLE` choose another). Without a network, the newest cycle already in `data/ifs`
   is used, and the script says so.
4. The file is linked as `wind.nc`. A `ww3_prnc_wind.nml` of the case is kept and pointed at it;
   without one the script writes it (`u10`, `v10` on `longitude`, `latitude`).
5. Every quoted date of `ww3_shel.nml`, `ww3_ounf.nml`, `ww3_ounp.nml` and `ww3_prnc_*.nml` moves
   by the same amount, so the run starts at the cycle and keeps its length and its output steps.

`--wind <file.nc>` takes a file. One named as `ifs_get` names it,
`ifs_<YYYYMMDDHH>_f<HHH>[_box_<W>_<E>_<S>_<N>].nc`,
retimes the case the same way; any other leaves the dates alone. So these two are the same run:

```bash
just run --ww3 --case <dir> --wind ifs
just run --ww3 --case <dir> --wind "$(just wind --print-path -H 6 -b -53,-43,-33,-23)"
```

`ww3_shel.nml` must say `INPUT%FORCING%WINDS = 'T'`; the script stops when it does not.

**`ww3_prnc` read the file** `(v)`, which closes the open point of `IFS_FORCING_202611.md`, and it
found a defect on the way. On a regular grid `ww3_prnc` stops with "LATITUDE VALUES MUST BE
REVERSED" when the latitudes of the file descend (`model/src/ww3_prnc.F90:942` at `92e2cf66`),
and IFS is written north to south. A triangular grid does not take that branch, so the first test
passed and the second did not. `scripts/ifs_get.sh` now turns the latitudes south to north with
`ncpdq`, and a file cached before that is rebuilt from its GRIB.

## Temperature: `--temp cmems` is `just temp`

`just temp` (`scripts/cmems_get.sh`) asks the Copernicus Marine Toolbox for the sea surface
temperature of the same box and time span, and keeps the file while the request is the same:

- Product `GLOBAL_ANALYSISFORECAST_PHY_001_024` (Global Ocean Physics Analysis and Forecast,
  1/12°), dataset `cmems_mod_glo_phy-thetao_anfc_0.083deg_PT6H-i`, variable `thetao` (potential
  temperature, `sea_water_potential_temperature`, °C), every 6 h, longitude -180° to 179.92°,
  latitude -80° to 90°, times to 2026-11-21 on the day it was read; the shallowest level only is
  taken (0 to 1 m) `(v)` `copernicusmarine describe --dataset-id <the id>`, toolbox 2.5.1, which
  answers without an account. `CMEMS_DATASET` names another.
- It needs a free Copernicus Marine account: `copernicusmarine login` once, or
  `COPERNICUSMARINE_SERVICE_USERNAME` and `COPERNICUSMARINE_SERVICE_PASSWORD` in the environment.
  The script reads neither and stores nothing.
- ⚠ The download was not run: the container had no account. `just temp --self-test` passed,
  every option of the command that `just temp -n -b -53,-43,-33,-23` prints is in
  `copernicusmarine subset --help` of toolbox 2.5.1 `(v)`, and `--temp <file.nc>` was run with a
  stand-in file.

The file is linked as `temperature.nc` in the run directory, and **WW3 does not read it**. In
WW3 the temperature enters the wind input only as the air-sea temperature difference, a field
`ww3_prnc` writes from `FORCING%FIELD%WINDS_AST`, and ST4 uses it only under the `STAB3` switch
(`model/src/w3src4md.F90:668`, `:682`, `:796` at `92e2cf66`). Every lab switch and MetOcean's own
`switch_ST4` have `STAB0`. Feeding it to the model needs an air temperature too (IFS `2t`, which
`just wind -p "10u 10v 2t"` already fetches), a file of the difference, and a build with `STAB3`,
which changes the physics of the reference. That is the owner's decision ⚠; until then the file
is beside the run for whoever reads the directory (a product layer of ondaseventos or marola).

## What ran

One run each, the container above, 2026-11-11. The wall times say that the pieces work; they are
not benchmarks, and no table quotes them.

| Command | Result |
|---|---|
| `just run --ww3 smoke` | `(v)` "End of program"; `ww3_ts1` with MetOcean's own inputs |
| `just run --ww3 full` | `(v)` "End of program"; `bench_small`, 4 min 27 s of `ww3_shel`, serial |
| `just run --ww3 coast` | `(v)` fails: MetOcean's `ww3_bounc` stops with a segmentation fault at `model/src/ww3_bounc.F90:556` on the fixture's boundary spectra; cause not found ⚠. `just coast-ww3` runs `--ww3-noaa` |
| `just run --ww3 --case <LIMON> --wind ifs` | `(v)` triangular grid (1 778 nodes, 3 328 triangles, `ww3_tp2.6` of the MetOcean tree with `INPUT%FORCING%WINDS = 'T'`), IFS 2026-11-10 00z, 1 h: `ww3_prnc` read an 8 × 8 input grid, `ww3_shel` reached "End of program" in 321 s, `ww3_ounf` wrote 12 fields |
| `just run --ww3 --case <southern Brazil> --wind ifs` | `(v)` regular grid (81 × 81 at 0.1°, flat test bathymetry ⚠), IFS 2026-11-10 00z, 3 h: `ww3_prnc` read a 41 × 41 input grid at two times, `ww3_shel` reached "End of program" in 180 s, `ww3_ounf` and `ww3_ounp` wrote `ww3.field.nc` and `ww3.spec.nc`. The model's `uwnd` at 00z has the minimum of the file's `u10` (-9.2255 m s⁻¹), and `hs` grows from rest to a maximum of 0.80 m |
| `just run --chaceon smoke` | `(v)` ran; against `--ww3` built with chaceon's switch and `-ffp-contract=off`: `mod_def.ww3` (28 497 841 bytes) and `restart.ww3` byte-identical, `out_grd.ww3` different in 156 of 47 762 bytes, `out_pnt.ww3` in 1 134 608 of 7 652 218 |
| `just run --chaceon --case <southern Brazil> --wind ifs` | `(v)` ran: chaceon's `ww3_prnc` read the IFS file and its `ww3_shel` finished; against `--ww3` built chaceon's way, `mod_def.ww3` (31 031 761 bytes) byte-identical, `out_grd.ww3` different in 12 917 of 2 805 674 bytes, `out_pnt.ww3` in 2 314 of 93 913 |
| `just run --ww3-noaa smoke` | `(v)` "End of program"; the fork differs from `WW3-noaa/` only by the KOKKOS patch; `switch_lab_ompg` |
| `just run --chaceon smoke --gpu` | `(v)` exit 5, before any build |
| `just run --ww3-gpu ...` | ⚠ not run in the container; the self-test checks its arguments |

The two cases are staged by [`examples/03-fresh-forcing/stage.sh`](../examples/03-fresh-forcing/README.md)
from files the repository already holds:

```bash
examples/03-fresh-forcing/stage.sh triangular build/cases/limon
examples/03-fresh-forcing/stage.sh regular build/cases/sbrazil
```

- **LIMON**, triangular: `WW3-metocean-7.14/regtests/ww3_tp2.6/input` (`limon_ll.msh`,
  `namelists_LIMON.nml`, `points.list`, `ww3_grid.nml`, `ww3_shel.nml`, `ww3_ounf.nml`,
  `ww3_strt.inp`) copied flat, the `../input/` paths made `./`, the homogeneous wind of
  `ww3_shel.nml` replaced by `INPUT%FORCING%WINDS = 'T'`, one hour.
- **Southern Brazil**, regular, 81 × 81 at 0.1°: `examples/02-regional-real-forcing` with its
  boundary points removed, three hours, and a flat 100 m test bathymetry ⚠ in place of the GEBCO
  file the example asks for. The run proves the wind path on a regular grid; its waves mean
  nothing on a flat bottom.

Running example 02's namelists found a key no WW3 pin reads: `FIELD%TIMESTOP` and
`POINT%TIMESTOP` in the `ww3_ounf.nml` and `ww3_ounp.nml` of examples 01 and 02 (`ww3_ounf`
stops with "Cannot match namelist object name %timestop"; `model/src/w3nmlounfmd.F90:27` has
`TIMESTART`, `TIMESTRIDE` and `TIMECOUNT` at both pins) `(v)`. The examples now use `TIMECOUNT`.

chaceon's bit-for-bit claim is against its reference image on linux/arm64, whose libm it
reproduces (`chaceon/docs/ADRs/ADR-0005-three-verdicts.md`). So the script judges a difference
only on aarch64, where it fails the run; on any other machine it counts the bytes and says "not
judged". The x86_64 differences above are therefore not a finding against chaceon ⚠.

## What MetOcean's WW3 does not do here

- **No OpenMP build.** With `switches/switch_lab_ompg` the compile stops at
  `model/src/w3iogomd.F90:2380`, "Unexpected !$OMP PARALLEL DO statement": the directive at
  `:2374` is opened again `(v)` gfortran 13.3.0. `--ww3` and `--chaceon` build the serial
  `switch_lab_shrd`, and `--ww3` with an `OMPG` switch is refused (exit 64).
- **Not the fork's 2026 inputs.** The fork's `ww3_ts1/input/ww3_shel.nml` asks for output fields
  the 2023 code does not have (`SKW`, `EMB`, `EMC`), and MetOcean's tree has no
  `ww3_grid_ST4_T475.nml`, so `smoke` takes each tree's own `ww3_ts1` inputs `(v)`.
- **Not `coast`** (the table above).

## What is not done

- The fork on MetOcean's WW3, and with it `--ww3` as the pair of `--ww3-gpu` (plan units U1, U3
  to U5).
- MetOcean's WW3 in the Docker images: they hold NOAA's WW3 (`--ww3-noaa`) and ww3-gpu, and
  `--ww3` in an image stops with exit 64 and says so. `docker/Dockerfile.ondaseventos` builds
  MetOcean's WW3 on its own, as before.
- A longitude convention of 0° to 360°, and a grid that crosses the date line: the box is not
  derived, the whole globe is fetched, and whether `ww3_prnc` then places the wind is not
  checked ⚠.
- The temperature as model forcing (above).
- `docs/ww3-manual/` still cites `model/src` lines at NOAA's `03adda91`, the fork's base. Those
  lines describe the fork and `--ww3-noaa`; they are not re-read at `92e2cf66` ⚠.
