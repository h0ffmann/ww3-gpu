# Example 03: one command, fresh wind, two grid types

`just run` runs a case directory in any runtime and gives it the newest ECMWF IFS wind
([`docs/RUN_COMMAND_202611.md`](../../docs/RUN_COMMAND_202611.md)). This example stages two case
directories from inputs the repository already holds, one on a triangular grid and one on a
regular grid, so the command can be tried before you write a case of your own.

```bash
examples/03-fresh-forcing/stage.sh triangular build/cases/limon
just run --ww3 --case build/cases/limon --wind ifs        # MetOcean's WW3, Fortran

examples/03-fresh-forcing/stage.sh regular build/cases/sbrazil
just run --ww3 --case build/cases/sbrazil --wind ifs
just run --chaceon --case build/cases/sbrazil --wind ifs  # WW3 in Rust, compared with --ww3
just run --ww3-gpu --case build/cases/sbrazil --wind ifs  # the Kokkos port on the CPU; not run yet ⚠
```

| Case | Grid | From | Length |
|---|---|---|---|
| `triangular` | LIMON: 1 778 nodes and 3 328 triangles of a Gmsh mesh, `GRID%TYPE = 'UNST'`, near 83°W 10°N | WW3's regtest `ww3_tp2.6`, read from the `WW3-metocean/` submodule when the script runs | 1 h |
| `regular` | southern Brazil, 81 × 81 points at 0.1°, `GRID%TYPE = 'RECT'` | example 02's namelists, without its boundary points | 3 h |

The script changes only what a case directory needs: the regtest's `../input/` paths become
`./`, and its homogeneous wind becomes `INPUT%FORCING%WINDS = 'T'`, so that `ww3_shel` reads the
wind `ww3_prnc` wrote. The dates stay those of the inputs (2010 and 2024): `--wind ifs` moves
them to the IFS cycle it fetched.

⚠ **The regular case has a flat 100 m test bathymetry.** Example 02 needs a GEBCO tile that this
repository does not hold, so `stage.sh` writes `bathy.inp` and `mask.inp` with every point at
100 m and open sea. The run shows that the wind reaches the model on a regular grid; its waves
mean nothing. For real depths, follow example 02's README to run `make_bathy`, then copy its
`bathy.inp` and `mask.inp` over the two files; that path was not run ⚠.

⚠ The triangular case keeps the regtest's 1 s time step, so one model hour took about five
minutes of `ww3_shel` on one core of a cloud container (one run, 2026-11-11, not a benchmark).

What ran, on which machine, and what did not: `docs/RUN_COMMAND_202611.md`, "What ran".

## What to look at

```bash
ls build/runs/ww3-limon/              # the run directory: namelists as run, wind.nc, ww3.*.nc
grep "Time :" build/runs/ww3-limon/ww3_prnc_wind.out     # the wind times ww3_prnc read
ncdump -h build/runs/ww3-sbrazil/ww3.nc | head -40
```

`--temp cmems` adds the sea surface temperature of the Copernicus Marine forecast as
`temperature.nc` beside the run. It needs a Copernicus Marine account, and WW3 does not read the
file (`docs/RUN_COMMAND_202611.md`, "Temperature").
