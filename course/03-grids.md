# 03 — Grids, bathymetry, masks

## Four grid types

| Type | `GRID%TYPE` | When |
|---|---|---|
| Rectilinear | `RECT` | Regular lat-lon or Cartesian. Default. Cheapest per point. |
| Curvilinear | `CURV` | Body-fitted logically-rectangular mesh; you supply x and y coordinate arrays. Needed for tripole global grids. |
| Unstructured | `UNST` | Triangular mesh from GMSH `.msh`. Resolution where you need it, and only there. Reads open-boundary lists from a separate `UGOBCFILE`. |
| SMC | via `&SMC_NML` | Spherical Multiple-Cell: quad-tree refinement on a sphere. Used by the UK Met Office. Its own cell/face array files. |

Unstructured is where coastal work is going, because the resolution mismatch between a
1000 km shelf and a 200 m inlet is brutal on a rectilinear grid. The cost: unstructured WW3
uses implicit schemes and needs PDLIB + ParMetis for domain decomposition (added v6.04),
which is a build-time complication.

## Grid closure

`GRID%CLOS` matters only for global spherical grids:

- `NONE` — no wrap. Regional.
- `SMPL` — periodic in i; `(NX+1, J) → (1, J)`. Forces `SX = 360/NX`.
- `TRPL` — tripole. Periodic in i *and* folded at `j = NY+1`: `(I, NY+1) → (NX−I+1, NY)`.
  Requires `NX` even and a curvilinear grid. This is how you avoid the North Pole
  singularity on a global grid.

## Bathymetry

**Sign convention: negative below mean sea level.** This is opposite to how most people
think about "depth" and it is the single most common grid error. GEBCO's `elevation` is
already negative below sea level, so `DEPTH%SF = 1.` For a file of positive depths,
`DEPTH%SF = -1.`

**`IDLA` decides which way is up.** `1` = bottom row first, `3` = top row first. Check by
comparing `mask.ww3` (ASCII, written by `ww3_grid`) against a map. Do it visually, once,
every time you build a new grid.

**`ZLIM` vs `DMIN`.** `ZLIM` (e.g. `-0.10`) is the coastline: points shallower than this are
permanently excluded and will never be wet even if water level rises. `DMIN` (e.g. `2.50`) is
a floor applied to the depth used in the physics, so a nearly-dry cell doesn't produce
division-by-zero. They do different things and both matter.

## Generating the files

`ww3_grid` does not read netCDF bathymetry. It reads plain ASCII arrays — one row per
line, in the order `IDLA` declares — and you are expected to produce them. The two examples
each ship a small Fortran program for that, and the point of generating the files rather
than committing them is that you can change `NX`, `NY` or the depth and rerun, which is
exactly what the exercises ask you to do.

| Example | Program | Writes | How |
|---|---|---|---|
| `01-fetch-limited-growth` | [`make_inputs.F90`](../examples/01-fetch-limited-growth/make_inputs.F90) — no arguments | `depth.inp`, `mask.inp` | A 61 × 5 box of flat 250 m water (positive numbers; `ww3_grid.nml` sets `DEPTH%SF = -1.` to flip the sign). Mask row: one `0` (the coastline at `i = 1`) followed by sixty `1`s. `IDLA = 1`. |
| `02-regional-real-forcing` | [`make_bathy.F90`](../examples/02-regional-real-forcing/make_bathy.F90) — `make_bathy gebco.nc` | `bathy.inp`, `mask.inp` | Reads a GEBCO netCDF subset with netcdf-fortran and samples it onto the 81 × 81, 0.1° grid of `&RECT_NML` by **nearest neighbour**. GEBCO's `elevation` is negative below sea level, so `DEPTH%SF = 1.` Points deeper than `ZLIM` become `1`; the southern and eastern edges, one cell in, become `2` where wet, matching the `INBND_POINT` blocks in `ww3_grid.nml`. |

Each example's `run.sh` compiles its generator with `gfortran` if the binary is missing,
runs it, and then runs the WW3 programs. Read the sources: they are short, and the mask
legend and the `IDLA` convention are spelled out in the comments.

Nearest-neighbour sampling is a deliberate downgrade from interpolation. Point-sampling a
15-arc-second dataset onto a 0.1° grid throws away most of the information and can miss a
whole shoal; for production work you want area-weighted (conservative) regridding, which
is a job for a proper regridding tool, not a forty-line program. For a course grid it is
fine, and it has one virtue interpolation lacks: every depth in `bathy.inp` is a number
GEBCO actually reported.

## Masks

```
 -2  excluded boundary point (ice-covered)
 -1  excluded sea point (ice-covered)
  0  excluded land point
  1  sea point
  2  active boundary point
  3  excluded grid point
  7  ice point
```

Note `2` — boundary points are declared in the *mask*, and/or via `&INBND_POINT_NML` in
`ww3_grid.nml`. `ww3_grid` will promote active points on the declared boundary segments to
status 2 and report how many.

## Obstruction grids — the underrated one

At 0.1° you cannot resolve a 3 km island. But that island blocks waves. WW3's answer is
**subgrid obstruction**: a per-cell transparency in x and y, between 0 (fully blocking) and
1 (fully transparent), applied during propagation.

Controlled by `&MISC FLAGTR` in `namelists.nml`:

```
0  no obstruction (and then you must NOT supply an obstruction file)
1  transparency at cell boundaries
2  transparency at cell centres
3  as 1, with continuous ice
4  as 2, with continuous ice
```

Generate these with [`gridgen`](https://github.com/NOAA-EMC/gridgen) (MATLAB). Without them,
archipelagos are invisible and your lee-side wave heights are badly wrong. With them, a
coarse global grid produces surprisingly decent results in the Indonesian seas and the
Aegean.

## Choosing resolution

Two competing constraints:

1. **Physics.** Resolve the bathymetric features that refract the waves you care about.
2. **CFL.** `Δt ∝ Δx`. Halve the grid spacing and you double the timestep count *and*
   quadruple the point count — 8× the cost for a 2D grid.

The standard resolution is a nested mosaic: a coarse global grid (0.5°) feeding a regional
grid (0.1°) feeding a coastal grid (0.01°). Either as separate runs chained through
`ww3_bounc`, or as a single `ww3_multi` run with two-way interaction. See lesson 05.

## Exercise

Build the same domain three ways — 0.2°, 0.1°, 0.05° — with correctly recomputed timesteps
(edit `NX`, `NY`, `SX`, `SY` in `make_bathy.F90` and `ww3_grid.nml` together), and run
example 02's forcing through all three. Pull `Hs` at the shelf-break point out of each
`ww3.nc` with `ncks -v hs -d longitude,<i> -d latitude,<j>` (or `ncdump -v hs` and your
eyes) and tabulate the three time series side by side. Where does it converge? Where
doesn't it? That answer is specific to your coastline and it's the only honest way to pick
a resolution. Lesson 06 shows the tools, and `nccmp-tol` is the wrong tool for this one —
the grids differ, so the fields aren't comparable point by point.

→ [`04-forcing.md`](04-forcing.md)
