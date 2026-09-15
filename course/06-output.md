# 06 — Output and post-processing

## The seven output types

Configured in `&OUTPUT_TYPE_NML` (what) and `&OUTPUT_DATE_NML` (when) in `ww3_shel.nml`.
A stride of `'0'` disables a type.

1. **Gridded fields** — `Hs`, periods, direction, etc. on the model grid. → `out_grd.ww3`
   → `ww3_ounf` → netCDF. The one you'll use 90% of the time.
2. **Point spectra** — full 2D frequency-direction spectra at named locations.
   → `out_pnt.ww3` → `ww3_ounp` → netCDF. This is the model's actual state, not a summary.
3. **Track output** — fields along a moving track. For satellite collocation.
   → `ww3_trnc`.
4. **Restart files** — the complete model state, for continuing a run.
5. **Boundary data** — spectra along declared output boundaries, for feeding a child grid.
   Mostly superseded by using point output + `ww3_bounc` instead.
6. **Separated wave fields** — spatially and temporally coherent wave *systems* tracked
   across the domain. See the `ww3_systrk` program and IFREMER's `TUTORIAL_WAVETRACK`.
7. **Coupling fields** — for NUOPC/ESMF/OASIS coupled runs.

## Fields worth knowing

```
DPT CUR WND AST WLV ICE IBG D50 IC1 IC5
HS LM T02 T0M1 T01 FP DIR SPR DP HIG
EF TH1M STH1M TH2M STH2M WN
PHS PTP PLP PDIR PSPR PWS PDP PQP PPE PGW PSW PTM10 PT01 PT02 PEP TWS PNR
UST CHA CGE FAW TAW TWA WCC WCF WCH WCM FWS
SXY TWO BHD FOC TUS USS P2S USF P2L TWI FIC
ABR UBR BED FBB TBB
MSS MSC WL02 AXT AYT AXY
```
(the authoritative list with descriptions is at the top of `$WW3/model/nml/ww3_ounf.nml`)

Ones you'll actually reach for:

| Field | What |
|---|---|
| `HS` | significant wave height — 4√(total energy) |
| `T01`, `T02`, `T0M1` | mean periods from different spectral moments. **They are not interchangeable**; buoy products and models frequently compare the wrong pair. `T0M1` (energy period) is what most engineering work wants. |
| `FP`, `DP` | peak frequency and peak direction |
| `DIR`, `SPR` | mean direction and directional spread |
| `EF` | the 1D frequency spectrum on the model grid |
| `PHS PTP PDIR PSPR PWS` | **partitioned**: wind sea + N swell systems, separately |
| `USS`, `TUS` | Stokes drift — what you hand to an ocean model |
| `SXY` | radiation stresses — what drives nearshore circulation |
| `UST`, `CHA` | friction velocity, Charnock — what you hand back to an atmosphere model |
| `WND`, `DPT` | echo the inputs back. Free, and catches errors instantly. |

## Partitioning: the thing to actually understand

A total `Hs` of 2 m can be a 2 m wind sea, or a 2 m swell, or a 1.4 m sea plus 1.4 m of
swell from a completely different direction. These are different oceans and different
engineering problems, and a single number cannot distinguish them.

WW3 partitions the 2D spectrum into a wind sea and up to `NOSWLL` swell systems using a
watershed algorithm on the spectral surface, and reports `PHS(n)`, `PTP(n)`, `PDIR(n)` for
each. `FIELD%PARTITION = '0 1 2 3'` in `ww3_ounf.nml` selects which to write (0 = wind sea).

Re-partitioning offline with different parameters, without rerunning the model, is
something the third-party spectral libraries do well (lesson 08). In this repo the model's
own partition is the one you get, and `ww3_ounp` gives you the full 2D spectrum if you want
to do better by hand.

## Reading the output

Everything `ww3_ounf` and `ww3_ounp` write is plain netCDF-4, and the tools that ship with
netCDF itself are enough to read it. No library, no interpreter:

```bash
ncdump -h ww3.nc                          # header: dimensions, variables, attributes, units
ncdump -v time ww3.nc | tail -5           # the time axis, decoded by hand from its units
ncdump -v hs ww3.nc | less                # the whole field, row-major, last time last
ncdump -h ww3.20240701_spec.nc            # the spectral file: (time, station, frequency, direction)
```

`ncdump` ships with netcdf-c and is in the pinned toolchain `(v)`. For slicing, NCO's `ncks`
is the tool — `ncks -v hs -d time,-1 ww3.nc` prints the last time step only, and
`-d longitude,40 -d latitude,40` picks one point — ⚠ NCO is not in the `just toolchain`
listing; install it on the host or fall back to `ncdump` and patience.

Two habits worth forming:

- Read `:units` and `:scale_factor` before you read a number. `FIELD%TYPE` in
  `ww3_ounf.nml` is `[2 = SHORT, 3 = it depends, 4 = REAL]`, template default `3` `(v)`.
  With `2` the fields are packed short integers with a scale factor, and `ncdump` prints
  the raw packed values — it never unpacks (`ncks --unpack` does). Both course examples set
  `FIELD%TYPE = 4` `(v)`: plain floats, and the problem goes away.
- The 2D spectrum is `efth(time, station, frequency, direction)`, in m²/Hz/rad. `Hs` from
  it is `4 sqrt(ΣΣ efth Δf Δθ)`. Computing that once from `ncdump` output — by hand or in
  twenty lines of Fortran — and checking it against the `HS` field is the consistency check
  that catches a wrong frequency range or a wrong `Δθ`. If they disagree, one of them is
  being integrated over a different range than you think.

### `ww_fetch_analyse`: the example-01 payoff

Example 01 is the one case with an analytic answer, and its analysis is a C++ program
against netcdf-c rather than a notebook: [`kokkos/tools/fetch_analyse/`](../kokkos/tools/fetch_analyse/),
built by `just kokkos-build openmp-release` and run by the example's `run.sh`.

```bash
ww_fetch_analyse ww3.nc [u10]        # u10 defaults to 10 m/s and must match HOMOG_INPUT(1)%VALUE1
```

It reads `hs(time, y, x)` (detecting `x` or `longitude` as the fetch axis), takes the last
time step on the centre row, and prints one line per ~12 fetch bins:

| Column | Meaning |
|---|---|
| `fetch [km]` | distance from the coastline at `i = 1` |
| `WW3 Hs [m]` | what the model produced at steady state |
| `K&C92 Hs [m]` | Kahma & Calkoen (1992) fetch law, `ê = 5.2e-7 x̂^0.9`, converted to `Hs` |
| `ratio` | model / empirical — should sit near 1 while the sea is still growing |

The header line prints the Pierson–Moskowitz fully developed limit, `Hs = 0.0246 U10²`
(2.46 m at 10 m/s); nothing should meaningfully exceed it at steady state, and a ratio that
drifts *down* with fetch is the sea approaching full development, not an error. An `Hs`
that *decreases* with fetch means your wind direction convention is flipped.

### Compare two runs with `nccmp-tol`

The second half of the course is built on one question: *did this change alter the
answer?* Compile flags, an OpenMP layout, a refactored routine, a Kokkos kernel — every rung
of the ladder is gated by that question, and "eyeball two `ncdump`s" is not an answer.
[`kokkos/tools/nccmp-tol/`](../kokkos/tools/nccmp-tol/) is the comparator the proposal
calls "comparador por campo":

```bash
nccmp-tol REF.nc TEST.nc [TOLERANCES]       # default: kokkos/tools/nccmp-tol/tolerances.txt
```

Its contract:

- Every numeric variable present in **both** files is compared over the values that are
  neither NaN nor `_FillValue`; per variable it prints `n`, `max_abs`, `rms` and `max_rel`.
- A variable is **judged** only if it is listed in the tolerances file, one line each:
  `name abs rel`, `#` comments allowed. A value passes if `|d| ≤ abs` **or**
  `|d| / max(|ref|, eps) ≤ rel`. Unlisted variables are reported, not judged.
- Exit **0** only if every judged variable passes; **1** if any fails; **2** on an I/O error.
  So it goes straight into a shell `if`, a CI step, or `L2_replay.sh` (lesson 12).

The default file judges `hs`, `fp`, `dir`, `dp` and `t0m1`: `1e-4` relative everywhere,
`1e-4` absolute on the scalar fields and `1e-2` absolute on the two directions (they are in
degrees). Those numbers are a starting point for a bit-reproducible change, not a
scientific statement; the proposal's rule is that the tolerances live in a versioned file,
are proposed by the student and approved by the co-advisor, and a change that fails them is
not merged. Tightening or loosening them is a commit with a reason, not a command-line
flag.

Two things it is *not*: it is not `nccmp` (the C tool of that name compares bit for bit
and knows nothing about tolerances), and it is not a validator — it compares a run against
another run, never against the sea.

## Validation

[`NOAA-EMC/WW3-tools`](https://github.com/NOAA-EMC/WW3-tools) is the official toolkit:
altimeter collocation, NDBC buoy matching, scatter plots, QQ plots, Taylor diagrams, and
the standard metric set. It is Python, and it is the right tool for that job (lesson 08).

Rough expectations for a regional run with default tuning and decent winds: `Hs` bias
within ±10%, scatter index 15–25%. Periods are worse — `Tp` in particular is a noisy
statistic and comparing it point-to-point against a buoy is a good way to feel bad about
yourself. Compare `T0M1` instead, and compare *distributions* as well as time series.

If your first attempt looks dramatically better than that, check that you aren't
accidentally comparing the model against itself.

→ [`07-physics-choices.md`](07-physics-choices.md)
