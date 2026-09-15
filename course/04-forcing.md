# 04 — Forcing: winds, currents, ice, water levels

## What WW3 will accept

| Forcing | Why it matters | `INPUT%FORCING%…` |
|---|---|---|
| **10 m winds** | The engine. Everything else is a correction. | `WINDS` |
| Currents | Refraction, frequency shift, and the Agulhas/Gulf Stream giant-wave problem. | `CURRENTS` |
| Water levels | Tides. Matters in shallow water, irrelevant offshore. | `WATER_LEVELS` |
| Ice concentration | Attenuates and scatters. | `ICE_CONC` |
| Ice thickness/floe size | Needed by the more sophisticated ice physics (`IC2`–`IC5`). | `ICE_PARAM1`…`5` |
| Mud | Dissipation over muddy bottoms. Niche. | `MUD_DENSITY`, … |
| Air density, momentum | Mostly for coupled configurations. | — |

Each takes `'T'` (from a file), `'H'` (homogeneous constant, given in
`&HOMOG_INPUT_NML`), or `'C'` (from a coupler).

## Winds are everything

Wave model error is dominated by wind error, not by wave physics. A 10% wind speed error
becomes roughly a 20% error in `Hs` in a growing sea (energy goes roughly as `U²` in the
fetch-limited laws, and worse than that in the growth phase). Before you spend a week
tuning `BETAMAX`, check your winds against scatterometer data.

Practical consequences:

- **Resolution.** GFS or ERA5 at 0.25° cannot represent a squall line or a tight tropical
  cyclone core. For storm cases you want a higher-resolution atmospheric model, not a finer
  wave grid.
- **Temporal frequency.** Hourly is the minimum for anything with a front in it. 6-hourly
  winds smear out exactly the events you care about; the 3-hourly GFS output used below is
  a compromise.
- **Height.** WW3 wants 10 m winds. If your source gives a different level, convert with a
  log profile — or better, use `&SIN4 ZWND` / the equivalent for your source-term package
  to tell WW3 what height it's being given.
- **Stability.** `HOMOG_INPUT(i)%VALUE3` and the `STAB` switch handle air-sea temperature
  difference. A cold-air outbreak over warm water generates markedly more wave energy than
  the same wind speed in stable conditions.

## Getting the winds: GFS via `get_gfs.sh` and ecCodes

Example 02 takes its winds from NOAA's operational **GFS** at 0.25°, because it is free,
needs no account, and the whole pipeline is shell plus ecCodes — both of which are in the
pinned toolchain (`eccodes 2.48.0` `(v)`, lesson 01). One sentence on the alternative: ERA5
is the better *hindcast* product, but the only supported way to fetch it is the Copernicus
CDS Python client with a registered API key, and this repo's lab code does not depend on
Python (lesson 08), so ERA5 is not used here.

```bash
cd examples/02-regional-real-forcing
./get_gfs.sh 20240701 00        # cycle date and hour: YYYYMMDD HH
ncdump -h gfs_winds.nc | head -40
```

What the script does, step by step — read it, it is short:

1. Asks NOMADS' `filter_gfs_0p25.pl` for the subset `leftlon=-52 rightlon=-44 toplat=-24
   bottomlat=-32` with `var_UGRD=on var_VGRD=on lev_10_m_above_ground=on`, so you download
   two fields over one box instead of a 500 MB global GRIB.
2. Repeats that for forecast hours 0–192 every 3 h of the chosen cycle.
3. Concatenates the GRIB messages (`grib_copy`, or `cdo mergetime` where available) and
   converts with `grib_to_netcdf` → `gfs_winds.nc`.

Two things to notice about what you just got. First, it is a **forecast**, not an
analysis: 192 h issued from one cycle. That is fine for learning the pipeline and for
"what did the model think would happen"; for a hindcast you stitch the `f000` analyses of
successive cycles instead, which is a five-line change to the script. Second, the variable
names changed on the way through: GRIB calls them `10u`/`10v`, and `grib_to_netcdf` writes
them as `u10`/`v10` ⚠ — verify with `ncdump -h` before trusting the namelist below, and
`grib_ls gfs_*.grb2` shows you the GRIB side of the same fields.

## ww3_prnc

```bash
cp ww3_prnc_wind.nml ww3_prnc.nml
ww3_prnc | tee ww3_prnc_wind.out
```

It interpolates a netCDF field onto your model grid and writes WW3's binary format
(`wind.ww3`, `current.ww3`, `ice.ww3`, `level.ww3`). Key namelist fields:

```
&FORCING_NML
  FORCING%FIELD%WINDS = T
  FORCING%GRID%LATLON = T
  FORCING%TIMESTART   = 'YYYYMMDD HHMMSS'
  FORCING%TIMESTOP    = 'YYYYMMDD HHMMSS'
/
&FILE_NML
  FILE%FILENAME  = 'gfs_winds.nc'
  FILE%LONGITUDE = 'longitude'
  FILE%LATITUDE  = 'latitude'
  FILE%VAR(1)    = 'u10'
  FILE%VAR(2)    = 'v10'
/
```

### The four things that go wrong

1. **Variable names.** `u10` vs `U10` vs `10u` vs `eastward_wind`. `ncdump -h` first.
2. **Coordinate names.** A GRIB-derived file may carry `time` plus `step` rather than one
   valid-time axis, and ERA5 has shipped both `time` and `valid_time` across CDS format
   revisions. Look before you point the namelist at anything.
3. **Longitude convention.** 0–360 vs −180–180. GFS is served on 0–360, so the box above
   may come back as 308°–316° even though you asked for −52°…−44°; if your grid is at −52°
   and your forcing is at 308°, you may get a silent field of zeros rather than an error.
   Check with `ncdump -v longitude gfs_winds.nc`. If it needs shifting, NCO does it in
   place — `ncap2 -O -s 'longitude=longitude-360.' gfs_winds.nc gfs_winds.nc` ⚠ (NCO is
   not in the `just toolchain` listing; `ncdump` from netcdf-c is).
4. **Coverage.** The forcing must cover the model domain *and* the model time window with a
   margin. WW3 will not extrapolate off the end of your winds; it will stop, or hold the
   last field, depending on version.

### Verify before you run the model

Always output `WND` as a model field and look at it. If `ww3_prnc` silently produced
nothing useful, a map of the wind WW3 *actually used* shows it in two seconds. This is the
cheapest debugging habit in wave modelling. `ncdump -v wnd ww3.nc | head` is enough to
tell zeros from weather.

## Boundary spectra

Locally generated wind sea is only part of the picture. Swell arrives from thousands of
kilometres away, and on many coasts it dominates.

```
parent run  →  ww3_ounp (spectral netCDF at points along the child's boundary)
                  ↓
            ww3_bounc  →  nest.ww3  →  read automatically by ww3_shel
```

`ww3_bounc` reads a `spec.list` file naming the spectral netCDF files to use:

```bash
ls ../configs/GLOBAL_30MIN/SPEC/ww3.*spec.nc > spec.list
ww3_bounc
```

There is no flag in `ww3_shel.nml` to enable boundaries — if `nest.ww3` exists in the run
directory it is used. Check `log.ww3`: the input timeline shows boundary updates.

The alternative for global-scale sources: use NOAA's operational GFS-Wave output, or an
IFREMER hindcast, as your parent. You don't have to run the global grid yourself.

## Exercise

Take example 02, run it twice — once with boundary spectra, once without — and map the
difference in `Hs`. The result is a map of "how much of the wave climate here comes from
somewhere else". For the southern Brazil shelf in winter, it is most of it. `nccmp-tol`
(lesson 06) gives you the max and RMS of that difference per field in one line; it will
"fail", and the size of the failure is the answer.

→ [`05-nesting.md`](05-nesting.md)
