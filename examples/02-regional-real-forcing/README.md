# Example 02 — regional run, real bathymetry, real winds

Southern Brazil shelf: 52°W–44°W, 32°S–24°S at 0.1°. Florianópolis sits in the middle of it.

This is where WW3 stops being a toy. Example 01 had no geography, no data files, and no way
to be wrong about coordinate conventions. This one has all three.

## What you have to supply

| File | Where from |
|---|---|
| `gebco.nc` | [GEBCO 2024 subsetted download](https://download.gebco.net/) for the box above (52°W–44°W, 32°S–24°S; netCDF; keep the default variable names `lat`, `lon`, `elevation`) |
| `gfs_winds.nc` | `./get_gfs.sh YYYYMMDD HH`: GFS 0.25° 10 m winds from NOMADS, no account needed |

`get_gfs.sh` asks NOMADS' `filter_gfs_0p25.pl` for `UGRD`/`VGRD` at 10 m above ground,
subset to the box, forecast hours 0–192 every 3 h of one cycle (65 small GRIB2 files under
`gfs.YYYYMMDDHH/`), merges them with `cdo mergetime`, converts with ecCodes'
`grib_to_netcdf`, and relabels longitudes to −180..180. NOMADS keeps about ten days, so
pick a recent cycle; the namelists in this directory are dated 2024-07-01 00Z + 8 days, and
the script prints the one-line `sed` that retimes them to the cycle you fetched.

**Verify the file before running `ww3_prnc`**: it has not been possible to download in
the environment this example was written in, so the names in `ww3_prnc_wind.nml` follow
ecCodes' documented output and need a look:

```bash
ncdump -h gfs_winds.nc | head -40
```

Expect variables `u10(time, latitude, longitude)` and `v10(...)` (grib_to_netcdf turns the
shortNames `10u`/`10v` into `u10`/`v10`), coordinates `longitude`, `latitude`, `time`, and
65 time steps 3 h apart. If your ecCodes emits something else, change `FILE%VAR(1)`,
`FILE%VAR(2)`, `FILE%LONGITUDE` and `FILE%LATITUDE` in `ww3_prnc_wind.nml` to match.
Longitudes must read −52..−44, not 308..316.

## Run

```bash
export WW3=$HOME/src/WW3
just ww3                        # the toolchain shell: gfortran, netcdf-fortran, cdo, ecCodes, nco
./get_gfs.sh 20260910 00        # once; then the sed it prints
./run.sh
```

`run.sh` compiles `make_bathy.F90` against the shell's netcdf-fortran (`nf-config`) the
first time, and again whenever the source is newer than the binary.

### Bathymetry sampling — nearest neighbour, and why that is a downgrade

`make_bathy` takes, for each of the 81 × 81 model points, the GEBCO cell whose centre is
closest. The Python it replaced interpolated bilinearly between the four surrounding
cells. On a 15-arcsecond source and a 0.1° target the two differ by a fraction of a
cell's relief and you will not see it in `Hs`; but both throw away ~99 % of the source
cells and both can miss an entire shoal or islet between samples, and bilinear at least
averaged four of them. For production work do neither: area-weighted (conservative)
averaging onto the model cells, e.g. `cdo remapcon` onto a grid description of the
`RECT_NML` block, then `make_bathy` on the result. Model points more than one source
cell outside the tile, and any NaN, become land (`100.0`), which fails safe.

## New concepts, in the order you hit them

**Spherical coordinates.** `GRID%COORD = 'SPHE'` and `RECT%SX/SY` are now degrees, not
metres. The CFL calculation has to use the *smallest* physical cell size, which at 30°S is
the zonal one: `0.1° × 111320 × cos(30°) ≈ 9.6 km`. Get this wrong and the model is either
unstable or needlessly slow.

**Lower `FREQ1`.** The default 0.04118 Hz corresponds to a 24 s period. South Atlantic
swell from the Southern Ocean is routinely longer than that. Energy below your lowest bin
doesn't get truncated with a warning. It just never exists. Choosing `FREQ1` is a physical
decision about what you're modelling.

**Open boundaries.** Swell generated thousands of kilometres away has to enter the domain
somehow. `INBND_POINT` in `ww3_grid.nml` marks where, and `ww3_bounc` fills them with
spectra from a coarser parent run. Two rules that bite everyone:
- Boundary points must be *inside* the grid, never on the first or last row/column.
- A `CONNECT = T` flag fills in every point on the straight line from the previous point,
  which is how you specify an edge with two entries instead of eighty.

For a first run you can skip boundaries entirely (delete the `INBND_*` blocks). You'll get
locally generated wind sea only. That's actually a useful comparison, because the
difference between that and the boundary-fed run *is* the remote swell contribution.

**`ww3_prnc`.** Converts netCDF forcing onto the model grid and into WW3's binary format.
One run per forcing type; you swap the namelist between invocations. The failure mode to
watch for is longitude convention: GFS comes on 0–360 and your grid is on −180–180, and
if the file still is when `ww3_prnc` reads it you may get a silent field of zeros rather
than an error. `get_gfs.sh` relabels; `ncdump -v longitude gfs_winds.nc` proves it.

**Partitioned output.** `PHS`, `PTP`, `PDIR` split the spectrum into wind sea plus swell
systems. On this coast you'll typically see a local sea plus one or two distinct Southern
Ocean swell trains. Total `Hs` blends them into one number that describes neither.

**Point/spectral output.** `points.list` names five sites; `ww3_ounp` writes their full
2D spectra to netCDF, which [`wavespectra`](https://github.com/wavespectra/wavespectra)
reads natively with `read_ww3()`.

## Exercises

1. **Where does the swell come from?** Take the deep offshore point's 2D spectrum at a time
   when a swell train is present. Read off peak period and mean direction, compute the
   deep-water group velocity `cg = g/(4πf)`, and back out how far away and how long ago it
   was generated. Then check a surface-pressure chart for that date. This is the single most
   satisfying thing you can do with a wave model.

2. **Resolution matters where?** Rerun at 0.05°. Where does the answer change: offshore or
   over the shelf? What does that tell you about where the resolution budget should go?

3. **Boundaries on vs off.** Run with and without `nest.ww3`. Map the difference in `Hs`.
   How far into the domain does the boundary influence reach, and how fast?

4. **Validate.** Pull the nearest available buoy or altimeter track for the period and use
   [`WW3-tools`](https://github.com/NOAA-EMC/WW3-tools) to compute bias, RMSE, scatter index.
   A regional run with default tuning typically lands within 10–20% on `Hs` and does worse
   on period. Anything dramatically better on a first attempt is suspicious.

5. **Obstruction grids.** The small islands off Florianópolis are unresolved at 0.1°.
   Generate an obstruction grid with `gridgen`, set `FLAGTR = 1`, and see what changes in
   the lee.

## ⚠ Caveats

Written from documentation, not executed. The `ww3_prnc` and `ww3_ounp` namelists in
particular have more optional blocks than shown here. Check them against
`$WW3/model/nml/ww3_prnc.nml` and `$WW3/model/nml/ww3_ounp.nml` in your clone, which are
the authoritative annotated templates.
