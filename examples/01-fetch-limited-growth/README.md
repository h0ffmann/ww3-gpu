# Example 01 — fetch-limited growth

**Runtime:** about a minute on any laptop. **Needs:** a WW3 build with `NC4` in the switch.

## The physics

Wind blows off a straight coastline over deep water at a constant 10 m/s. Waves start at
zero and grow with distance downwind. Two things limit how big they get: *duration* (how
long the wind has blown) and *fetch* (how far from the coast you are). Run long enough and
duration stops mattering, and you're left with pure fetch-limited growth: a one-dimensional
problem with a well-known empirical answer.

This is the cleanest possible test that your WW3 build is doing physics rather than
producing plausible-looking noise.

## Run it

```bash
just example01                  # from the repo root: builds ww_fetch_analyse, then runs run.sh in the toolchain shell
```

or by hand, inside `just ww3`:

```bash
export WW3=$HOME/src/WW3        # your clone, already built
./run.sh                        # compiles make_inputs.F90, runs the three WW3 programs, prints the table
../../kokkos/build/openmp-release/tools/fetch_analyse/ww_fetch_analyse ww3.nc [u10]   # the table again
```

## What each step produces

| Program | Reads | Writes |
|---|---|---|
| `make_inputs` (from `make_inputs.F90`) | — | `depth.inp`, `mask.inp` |
| `ww3_grid` | `ww3_grid.nml`, `namelists.nml`, the two ASCII files | **`mod_def.ww3`**, `mapsta.ww3`, `mask.ww3` |
| `ww3_shel` | `ww3_shel.nml`, `mod_def.ww3` | `out_grd.ww3`, `restart.ww3`, `log.ww3` |
| `ww3_ounf` | `ww3_ounf.nml`, `mod_def.ww3`, `out_grd.ww3` | `ww3*.nc` |
| `ww_fetch_analyse` | `ww3.nc` | a table on stdout: WW3 `Hs` vs Kahma & Calkoen (1992) along the centre row, and the Pierson–Moskowitz limit |

`make_inputs.F90` is fifty lines of Fortran with three parameters at the top (`nx`,
`ny`, `depth_m`); `run.sh` recompiles it when the source is newer than the binary, so
edit-and-rerun works. `ww_fetch_analyse` lives in
[`kokkos/tools/fetch_analyse/`](../../kokkos/tools/fetch_analyse/) and reads the file with
netcdf-c: last time step (steady state), centre row (away from the edges), `x` or
`longitude` as the fetch axis. Its second argument is `U10` and must match
`HOMOG_INPUT(1)%VALUE1` in `ww3_shel.nml`. There is no plot; `ncview ww3.nc` or
`cdo outputtab,value -selname,hs ww3.nc` if you want to look at the field.

`mod_def.ww3` is the thing to understand. It's an opaque binary blob containing the whole
model definition: grid, spectral discretisation, timesteps, physics configuration. Every
other program reads it. Change `ww3_grid.nml` and you must regenerate it, and everything
downstream is stale until you do. Half of all confusing WW3 behaviour is a stale `mod_def`.

## Exercises

1. **Nail down the wind direction convention.** `ww3_shel.nml` sets `VALUE2 = 270.`
   Look at `ww3.nc`: does Hs increase with x, or decrease? Flip to `90.` and rerun. Which
   one means "blowing east"? Write the answer in a comment so you never have to think about
   it again. Cross-check against the `DIR` output field and against the manual's section on
   direction conventions: WW3 is not consistent between input and output here, and it
   catches everyone.

2. **Duration vs fetch.** Shorten `DOMAIN%STOP` to 6 hours. Where along the fetch does the
   answer change, and where doesn't it? The crossover is the duration-limited/fetch-limited
   boundary, and you can predict it from the group velocity.

3. **Break the CFL condition.** Set `TIMESTEPS%DTXY = 3000.` (well above the ~1055 s limit
   computed in the namelist comments) and rerun. What does WW3 do: crash, warn, or quietly
   produce garbage? Now set `DTXY = 200.` How much slower, and is the answer any different?
   This teaches you what those four numbers actually buy.

4. **Resolution.** Change `RECT%SX` to 10 km and `RECT%NX` to 121 (same 1200 km fetch).
   Recompute the CFL timestep, update `TIMESTEPS`, rerun. Does Hs(fetch) converge?

5. **ST4 vs ST6.** Build a second time with `switches/switch_lab_st6` into a separate build
   directory and run the same case. Plot both. The spread between two current source-term
   packages on the simplest problem in wave modelling is a useful humility check.

6. **Spectral resolution.** Drop `SPECTRUM%NTH` from 24 to 12. Directional spread is the
   field that suffers first. Then drop `NK` from 32 to 16 and watch what happens to `FP`
   and to the high-frequency tail.

## ⚠ Caveats on this example

- It is written from the documented namelist reference and the upstream regression-test
  templates, but has **not been executed**. If `ww3_grid` complains, read its stdout. It
  is unusually good at saying which namelist block it disliked.
- The wind-direction convention issue above is real and unresolved in this file on purpose.
- If `ww3_ounf` produces no `.nc`, your switch file is missing `NC4`.
- Common failure: `ww3_grid` looking for an obstruction file. That means `FLAGTR` in
  `namelists.nml` isn't 0.
