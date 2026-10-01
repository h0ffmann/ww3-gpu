# 02 — Anatomy of a run

Work through [`examples/01-fetch-limited-growth`](../examples/01-fetch-limited-growth/) with
this open. Every claim here is visible in those files.

## ww3_grid — the most important program

It reads `ww3_grid.nml` plus your ASCII bathymetry/mask, and writes `mod_def.ww3`.
Its namelist blocks, in dependency order:

**`&SPECTRUM_NML`**: the spectral discretisation.
```
SPECTRUM%FREQ1 = 0.04118   ! lowest frequency, Hz
SPECTRUM%XFR   = 1.1       ! geometric ratio between bins
SPECTRUM%NK    = 32        ! number of frequencies
SPECTRUM%NTH   = 24        ! number of directions
```
`f_max = FREQ1 × XFR^(NK−1)`. With the defaults that's 0.04118 → 0.76 Hz, i.e. periods from
24 s down to 1.3 s. **Energy outside this range does not exist in your model.** If you care
about 26-second Southern Ocean swell, lower `FREQ1`. Nothing warns you.

`NTH = 24` means 15° bins. That's coarse for swell. Coarse direction resolution produces the
**Garden Sprinkler Effect**: a smooth swell field propagating far from its source breaks up
into discrete beams, because each direction bin travels as an independent ray. The `PR3`
propagation scheme includes a correction; more directions helps more.

**`&RUN_NML`**: switch individual terms of the balance equation on and off. Turning
`FLSOU = F` gives pure propagation, which is how the `ww3_tp*` regression tests isolate
numerics from physics. Extremely useful for debugging.

**`&TIMESTEPS_NML`**: four numbers, and they are not tuning knobs.

```
DTMAX   global timestep: everything else divides into it
DTXY    spatial propagation, CFL-limited
DTKTH   refraction / wavenumber shift
DTMIN   minimum source-term integration step
```

The CFL limit comes from the fastest group velocity, which belongs to the *lowest*
frequency (deep water: `cg = g/(4πf)`, so low f → fast):

```
cg_max = g / (4π·FREQ1)
T_cfl  = Δx_min / cg_max
DTXY  ≈ 0.9 · T_cfl        (round DOWN)
DTMAX ≈ 3 · DTXY
DTKTH ≈ DTMAX / 2          (÷10 if you have strong currents)
DTMIN ≈ 10 s
```

Keep them integer multiples of each other. Round down, never up.

**`&GRID_NML`**: `TYPE` is `RECT` / `CURV` / `UNST`; `COORD` is `SPHE` / `CART`; `CLOS` is
`NONE` / `SMPL` (periodic in i) / `TRPL` (tripole, for global grids that need to handle the
North Pole). `ZLIM` is the depth above which a point is permanently land; `DMIN` is the
floor applied to depth in the physics so shallow points don't blow up.

**`&DEPTH_NML` / `&MASK_NML` / `&OBST_NML`**: ASCII input arrays. Three fields to get right:

- `SF`: scale factor, **multiplied** into the values you supply. **Depths must be negative
  below mean sea level.** If your file has positive depths, set `SF = -1.`
- `IDLA`: layout. `1` = line by line from the *bottom* row; `3` = line by line from the
  *top*. Getting this wrong flips your continent upside down and is not always obvious.
- `IDFM` / `FORMAT`: free vs fixed format.

Mask legend: `0` land, `1` sea, `2` **active open boundary**, `3` excluded, `7` ice,
`-1`/`-2` ice-excluded.

**`&INBND_COUNT_NML` / `&INBND_POINT_NML`**: where boundary spectra enter. Two rules:
boundary points must be strictly *inside* the grid (never the first or last row/column,
the propagation stencil needs a cell outside them), and `CONNECT = T` auto-fills every
point on the straight line from the previous entry.

**`GRID%NML = 'namelists.nml'`**: points at a second file holding the physics tuning
namelists (`&MISC`, `&SIN4`, `&SDS4`, `&PRO3`, `&UNST`, …). Blocks not needed by your switch
settings are skipped automatically. If you have no obstruction grid, `&MISC FLAGTR = 0`.

Run it, always keeping stdout:
```bash
ww3_grid | tee ww3_grid.out
```
It prints the whole grid summary, every namelist value it read, and the count of sea/land/
boundary points. That count is your first sanity check.

Outputs: `mod_def.ww3` (binary, the model definition), `mapsta.ww3` (status map),
`mask.ww3` (ASCII land/sea; open it and look at it).

## ww3_strt — initial conditions

Writes `restart.ww3`. If it's absent, `ww3_shel` cold-starts from a calm sea and says so in
the log, which is usually what you want for a spin-up run. For a specific initial spectrum
(the propagation tests seed a Gaussian packet and watch it travel), copy an `ww3_strt` input
from a regtest: `$WW3/regtests/ww3_tp1.1/input/` is the canonical one.

## ww3_prnc — forcing

netCDF → WW3 binary, interpolated onto your grid. One invocation per forcing type; you swap
`ww3_prnc.nml` between runs. The failure modes are boring and universal: wrong variable
names, wrong coordinate names, wrong longitude convention (0–360 vs −180–180), wrong time
units. `ncdump -h` before you start.

## ww3_shel — the model

Reads `ww3_shel.nml` and `mod_def.ww3`.

- **`&DOMAIN_NML`**: start and stop, `'YYYYMMDD HHMMSS'`.
- **`&INPUT_NML`**: per forcing: `'T'` read from file, `'H'` homogeneous (given below),
  `'C'` from a coupler. Boundary spectra are *not* flagged here: `nest.ww3` is picked up
  automatically if present.
- **`&OUTPUT_TYPE_NML`**: what to output. Seven output types exist: gridded fields, point
  spectra, track output, restarts, boundary data, separated wave fields, coupling fields.
- **`&OUTPUT_DATE_NML`**: when. Each entry is `START  STRIDE_SECONDS  STOP`. A stride of
  `'0'` disables that output type.
- **`&HOMOG_COUNT_NML` / `&HOMOG_INPUT_NML`**: constant forcing without any data files.
  For `'WND'`: speed, direction, air-sea ΔT.

Watch `log.ww3` while it runs. It prints a timestep table with a column per input showing
which forcings updated when: `X` for an update, `F` for a file read. If a column never
moves, your forcing isn't arriving.

For MPI builds: `mpirun -np N ww3_shel`.

## ww3_ounf / ww3_ounp — output

`ww3_ounf` turns `out_grd.ww3` into gridded netCDF. `ww3_ounp` turns `out_pnt.ww3` into
spectral netCDF.

`FIELD%TIMESPLIT` controls file chunking: `0` one file, `4` yearly, `6` monthly, `8` daily,
`10` hourly. `FIELD%TYPE` is `2` (packed SHORT), `3` (mixed), `4` (REAL). Use 4 while
learning so you never wonder whether an odd value is a packing artefact.

The field name list is long and documented at the top of `$WW3/model/nml/ww3_ounf.nml`. The
ones you'll use constantly:

```
HS    significant wave height          T01/T02/T0M1  mean periods
FP    peak frequency                   DIR  SPR      mean direction / spread
WND   wind (echo it back, always)      DPT           depth
PHS PTP PDIR PSPR   partitioned wind sea + swell systems
EF    1D frequency spectrum            USS  TUS      Stokes drift
SXY   radiation stresses               UST            friction velocity
```

Always output `WND` and `DPT`. They cost nothing and they catch forcing and grid errors
immediately.

→ [`03-grids.md`](03-grids.md)
