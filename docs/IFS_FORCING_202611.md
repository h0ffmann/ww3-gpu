# IFS wind forcing for ww3-gpu's consumers (November 2026)

`just ifs-get` fetches one cycle of ECMWF's IFS (Integrated Forecasting System) deterministic
forecast and writes it as a netCDF file that `ww3_prnc` reads as wind forcing. It is written for the
two systems that run WW3 from this repository.

## Who consumes ww3-gpu

1. **ondaseventos**, the first consumer. LabECO/UFSC's operational wave forecast
   (<https://ondaseventos.ufsc.br/>, source `labeco/ondaseventos-forecast` on `codigos.ufsc.br`).
   Its web app lists "IFS / ECMWF · 9 km" as the atmospheric model behind its wind, gust and
   pressure layers, and "WAVEWATCH III v7.14" as the wave model of its textures `(v)`
   `src/App.tsx` and `mock_textures/metadata.json` served by the site, read 2026-11-11. Its image
   is `docker/Dockerfile.ondaseventos`.
2. **marola**, the second consumer ([marola-dev/marola](https://github.com/marola-dev/marola)).
   It reads WW3 output through NCEP's GFS-Wave today; running WW3 for its own bays is
   [MIP-0052](https://github.com/marola-dev/marola/blob/main/docs/MIPs/MIP-0052-wave-model-compute.md).

## What ondaseventos does, and what is not known

The repository `labeco/ondaseventos-forecast` is private: `https://codigos.ufsc.br/labeco/ondaseventos-forecast`
redirects to `/users/sign_in` and the GitLab API answers 404 for the project (both on 2026-11-11).
How ondaseventos fetches IFS was therefore **not read** ⚠. The recipe here follows the one hint the
repository holds: the `dev` image ondaseventos builds `FROM` installs the Python package
`ecmwf-opendata` (`docker/Dockerfile.ondaseventos`), the client of ECMWF Open Data. Whether
ondaseventos uses Open Data's 0.25° grid, or another IFS product at the 9 km the app names, which
parameters it takes and which cycles it runs, stays ⚠ until its fetch script is read.

## The source

ECMWF Open Data, `https://data.ecmwf.int/forecasts/<YYYYMMDD>/<HH>z/ifs/0p25/oper/`, anonymous,
licensed CC BY 4.0 `(v)` the portal's licence link and
<https://www.ecmwf.int/en/forecasts/datasets/open-data>, read 2026-11-11. No key is needed, so the
script reads none. `IFS_BASE_URL` points it at a mirror with the same directory layout.

- One GRIB2 file and one `.index` per step. The index has one JSON line per GRIB message with its
  `param`, `levtype`, `_offset` and `_length`, so the script downloads only the messages it needs
  with HTTP range requests `(v)` index of 2026-11-10 00z, step 3 h.
- Steps: the 00z cycle ran to 360 h (3-hourly to 144 h, 6-hourly after), the 06z cycle to 144 h
  `(v)` directory listings of 2026-11-10, read 2026-11-11. The script assumes 12z as 00z and 18z
  as 06z ⚠ (not listed).
- Surface parameters on that index include `10u`, `10v`, `10fg` (gust), `msl`, `2t`, `sp` and
  `sithick` `(v)` same index. `10u 10v` are the default; `-p "10u 10v msl 10fg"` adds pressure
  and gust, the other two fields ondaseventos shows.
- The grid is 1440 × 721, longitude -180 to 179.75, latitude 90 to -90 as served `(v)` `ncdump`
  of the file below before the recipe reversed the latitudes, so no longitude relabelling is
  needed (example 02's GFS needs one).

## What the recipe writes

```bash
just ifs-get -d 20261110 -c 00 -H 120 -b -52,-44,-32,-24 -o data/ifs/2026111000
```

`data/ifs/2026111000/ifs_2026111000_f120_box_-52_-44_-32_-24.nc` (the box is in the name since
2026-11-11; a global file has none) with `u10` and `v10` (`short`, packed with
`scale_factor` and `add_offset`) on `longitude`, `latitude`, `time` (41 steps, 3 h apart). The GRIB
messages stay in `grib/` so a rerun downloads nothing. That command took 1 min 33 s and 59 MB of
GRIB on a cloud container `(v)` run 2026-11-11 (one run, network-bound, not a benchmark).

`ww3_prnc` applies `scale_factor` and `add_offset` when it reads a field
(`model/src/ww3_prnc.F90:1533` at the `WW3/` pin `96892c95`) `(v)`. The names match example 02's
`ww3_prnc_wind.nml`, which reads `u10`/`v10` on `longitude`/`latitude`/`time`.

`ww3_prnc` of MetOcean's WW3 (`92e2cf66`) read the file on a triangular grid and on a regular
grid `(v)` 2026-11-11, [`RUN_COMMAND_202611.md`](RUN_COMMAND_202611.md), "What ran". The regular
grid found a defect: `ww3_prnc` stops when the latitudes of the file descend
(`model/src/ww3_prnc.F90:942` at that commit), and IFS is written from 90° to -90°. The recipe
now turns the latitudes south to north with NCO's `ncpdq`, so the file's latitudes ascend and the
`ncdump` above no longer shows the order ECMWF serves. `just run --case <dir> --wind ifs` calls
the recipe for a case's box and length; `just wind` is the same recipe.

Two traps the script works around: `cdo sellonlatbox` on the GRIB left `grib_to_netcdf` writing
fill values for longitude and latitude, so the box is cut after the conversion with `ncks`; and
`ncks` reads a bound without a decimal point (`-52`) as an index, so the bounds are printed as
`-52.0000`.

## Commands

| What | Command |
|---|---|
| Options | `just ifs-get --help` |
| Offline check of the parser and the step list | `just ifs-get --self-test` |
| URLs and byte ranges, no download | `just ifs-get -n -H 6` |
| The newest 00z cycle, five days, global | `just ifs-get` |

Tools: `curl`, ecCodes' `grib_to_netcdf`, `ncdump`, and NCO's `ncpdq`, with `ncks` for `-b`. The recipe runs
on the host when they are on `PATH` and in `just ww3` otherwise (pratico's shell carries ecCodes
and NCO).
