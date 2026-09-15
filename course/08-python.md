# 08 — The Python ecosystem, and why this repo does not depend on it

There is a real Python ecosystem around WW3, and you should know what is in it. This
repo's rule: the lab code in `kokkos/`, `examples/`, `exercises/` and `bench/` is C++,
Fortran and shell; Python is used only by the two publishing scripts in `scripts/`. What
the tools do, what one taught us, what we use instead:

## The landscape

| Tool | Role |
|---|---|
| [`pyww3`](https://github.com/caiostringari/pyww3) | Generate namelists from typed dataclasses and run the WW3 executables. Thin and honest. |
| [`WW3-tools`](https://github.com/NOAA-EMC/WW3-tools) | NOAA's official post-processing and validation: altimeter, buoys, statistics, plots. |
| [`wavespectra`](https://github.com/wavespectra/wavespectra) | The xarray library for spectral data. Reading, partitioning, statistics, polar plots. |
| [`ww3tool`](https://pypi.org/project/ww3tool/) | More ambitious wrapper: namelists + run scripts + Slurm submission + plotting. Young. |
| [`bmi-wavewatch3`](https://pypi.org/project/bmi-wavewatch3/) | Download NOAA's *published* WW3 hindcasts as xarray. Not for running the model. |
| [`rompy`](https://github.com/rom-py/rompy) | General pydantic-validated ocean-model configuration. Strong SWAN/SCHISM plugins; ⚠ no WW3 plugin found. |

## What pyww3 gets right

Its design is worth holding in your head even if you never install it. Every WW3 program
gets a dataclass (`WW3Grid`, `WW3Prnc`, `WW3Shel`, `WW3Ounf`, `WW3Ounp`, `WW3Bounc`); every
namelist parameter becomes a constructor keyword with the `%` flattened to `_`, so
`SPECTRUM%FREQ1` is `spectrum_freq1`; validation of required files and compatible values
happens at construction; and a common base class renders the namelist text, writes it into
the run directory, runs the program there capturing its output, and can add or remove a
whole namelist block — needed because `ww3_grid` objects to blocks it does not want. Its
two gotchas are the two lessons: the generated file contains *every* block until you
remove the irrelevant ones, and after you mutate an attribute you must regenerate the text
or the file on disk keeps the old value. The author's own words: *work in progress, API not
stable*. Being made to name every parameter is what teaches you the namelist, and once you
understand it, writing your own generator for what you actually need is a two-hour job.
That is the job the Fortran generators in `examples/` do.

## Why this repo does not depend on any of it

The second half of the course is a benchmarking and porting project, and its rule is
that every number must be reproducible by the lab on its own machines from one pinned
toolchain (lesson 01). A Python *dependency* is a second toolchain, with its own resolver
and its own drift; the day it breaks is the day you cannot rerun last month's benchmark.
(Availability is another matter: the pinned `#ww3` shell ships `python3` with numpy and
xarray `(v)`; nothing in the lab code imports them.) WW4 did the same: it merged "Remove
python dependence from compile system" on 2026-08-19 `(v)` (lesson 14). And the lab code
has to be *inside* the parity gate — a generator, analyser or comparator outside the
compiled, tested tree is where a silent change hides. None of that is a criticism of the
tools above: `WW3-tools` remains the right way to validate against buoys and altimeters,
and `wavespectra` the best way to re-partition a spectrum offline.

## What we use instead

| Need | Python route | What this repo uses |
|---|---|---|
| Namelists | `pyww3` dataclasses, Jinja2 templates | By hand from the annotated templates in `$WW3/model/nml/`, or written by the Fortran generators (`examples/01-fetch-limited-growth/make_inputs.F90`, `examples/02-regional-real-forcing/make_bathy.F90`) — lesson 03 |
| Parameter sweeps and benchmark cases | scripted loops over `pyww3` objects | `ww_bench_case --size small\|medium\|large … -o DIR` (`kokkos/tools/bench_case/`), which writes a complete case directory; a shell loop over its arguments is the sweep — lesson 09 |
| Forcing download | `cdsapi` for ERA5 | `get_gfs.sh` + ecCodes `grib_to_netcdf` — lesson 04 |
| Reading output | `xarray`, `wavespectra` | `ncdump` (netcdf-c) and NCO's `ncks`; `ww_fetch_analyse` for the example-01 growth table — lesson 06 |
| "Did the answer change?" | ad-hoc `numpy.allclose` | `nccmp-tol REF TEST [TOLERANCES]` with a versioned tolerances file and an exit code — lesson 06, and every lesson after 09 |
| Per-routine tests | none | GoogleTest against captured-Fortran fixtures in `kokkos/tests/` — lesson 12 |

→ [`09-benchmark-profile-compile-run.md`](09-benchmark-profile-compile-run.md) — measure first.
