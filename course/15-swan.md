# 15 — SWAN: the other one you should know

If you only ever learn WW3 you will eventually reach a coastal problem where WW3 is the
wrong tool and you won't recognise it. SWAN is that tool. It's free, it's a few hours to
build, and knowing when to reach for it is a real skill.

## What it is

**SWAN** — Simulating WAves Nearshore — is a third-generation spectral wave model from
**Delft University of Technology** (Booij, Ris & Holthuijsen). Same governing equation as
WW3, same family of source terms, deliberately different numerics.

Current version: **Cycle III version 41.51** `(v)`. Free and open source.

## Where the source actually lives

Three places, and the third is new enough that most tutorials don't mention it:

| Source | URL |
|---|---|
| Official site + downloads | https://swanmodel.sourceforge.io/download/download.htm `(v)` |
| SourceForge file releases | https://sourceforge.net/projects/swanmodel/files/swan/ `(v)` |
| **Git — TU Delft GitLab** | **https://gitlab.tudelft.nl/citg/wavemodels/swan** `(v)` |
| Release notes / modifications | https://swanmodel.sourceforge.io/modifications/modifications.htm `(v)` |
| Implementation manual (build guide) | https://swanmodel.sourceforge.io/download/zip/swanimp.pdf `(v)` |

```bash
git clone https://gitlab.tudelft.nl/citg/wavemodels/swan.git && cd swan
```

Its sibling **SWASH** (non-hydrostatic, phase-resolving, same group) is at
https://swash.sourceforge.io/ and https://gitlab.tudelft.nl/citg/wavemodels/swash `(v)`.

Ignore the various GitHub mirrors and vendored copies you'll find. They're snapshots of
whatever version someone needed years ago.

## Building it

Modern SWAN builds with **CMake 3.12+**, and the implementation manual recommends **Ninja**
as the generator because it's faster than GNU make `(v)`.

```bash
git clone https://gitlab.tudelft.nl/citg/wavemodels/swan.git && cd swan
cmake -G Ninja -B build -S .
cmake --build build
```

The older workflow (`make config` then `make ser` / `make omp` / `make mpi`) still exists
and is what most older tutorials assume. Either works; CMake is where it's going.

`scripts/04_get_swan.sh` in this repo does the clone and build.

### Switches, SWAN-style

SWAN has the same *idea* as WW3's switch file — compile-time feature selection — but a
different mechanism. Options live as **specially-formatted comments inside the `.ftn`
sources**, stripped or activated by a preprocessing step. For example `!/impi` marks MPI
code in `swmod1.ftn`, and `!ADC` marks the ADCIRC-coupling hooks scattered across several
files `(v)`.

Practical consequence, identical to WW3: if you change what's enabled, you rebuild. And
the same class of confusion applies — "I turned on MPI and nothing happened" is a stale
build directory.

## SWAN versus WW3: the actual difference

Both solve the action balance equation. Both have DIA quadruplets, whitecapping, bottom
friction, depth-induced breaking. The difference is **numerics**, and it decides
everything else.

| | WW3 | SWAN |
|---|---|---|
| Propagation scheme | **Explicit** (`PR3 UQ`, third-order ULTIMATE QUICKEST) | **Implicit** sweeps; unconditionally stable |
| Timestep | **CFL-limited.** Halve Δx, halve Δt. | No CFL constraint. Stationary mode has no timestep at all. |
| Sweet spot | Global and basin scale, deep water, long integrations | Coastal, high resolution, small domains |
| Stationary mode | no | **yes** — solve directly for the steady state |
| Triads (3-wave) | `TR0`/`TR1`, limited | Mature; matters in the surf zone |
| Diffraction | no | approximate (phase-decoupled refraction-diffraction) |
| Obstacles | subgrid obstruction grids | explicit obstacle lines with transmission/reflection coefficients |
| Ice physics | `IC1`–`IC5`, `IS1`/`IS2` — far ahead | minimal |
| Multi-grid mosaic | `ww3_multi`, two-way | no |
| Parallelism | MPI "shuffle" decomposition, OpenMP | OpenMP and MPI; block-Jacobi or block-wavefront strategies for the implicit sweeps `(v)` |

**The one sentence that matters:** WW3's explicit scheme means the timestep shrinks with
the grid spacing, so a 50 m coastal grid becomes ruinously expensive; SWAN's implicit
scheme doesn't care. That is why the standard architecture in coastal work is
**WW3 offshore → SWAN nearshore**, with WW3 spectra as SWAN's boundary condition.

Note the trade: SWAN buys unconditional stability at the cost of numerical diffusion and,
in non-stationary mode, accuracy that depends on how many sweeps you let it do. The
implementation manual's advice on block-Jacobi versus block-wavefront is precisely about
where that trade lands on a parallel machine. WW3's convergence property — the thing
Office Note 525 insists WW4 must preserve — is not free, and SWAN made the other choice.

## When to use which

| Situation | Reach for |
|---|---|
| Global or basin-scale hindcast/forecast | WW3 |
| Operational forecasting inside UFS/NOAA | WW3 |
| Sea ice, marginal ice zone | WW3 |
| Bay, estuary, harbour, surf zone at 10–200 m | **SWAN** |
| Stationary design wave conditions for engineering | **SWAN** (stationary mode) |
| Nearshore circulation via radiation stress | SWAN, or SWAN inside Delft3D |
| Wave-by-wave runup, overtopping, infragravity | **SWASH** or XBeach, not either of these |
| Coupled surge + waves, US practice | ADCIRC+SWAN |
| Coupled ocean–atmosphere–wave–sediment | COAWST (has both) |

## The ecosystem around it

- **Delft3D-WAVE** wraps SWAN and couples it to Delft3D-FLOW. Open source.
- **ADCIRC+SWAN** is the US storm-surge standard; the coupling hooks are in SWAN's source
  behind the `!ADC` switch.
- **[rompy-swan](https://rom-py.github.io/rompy-swan/)** `(v)` — pydantic-validated,
  type-safe SWAN configuration from Python or YAML, with data interfaces for NetCDF and
  THREDDS inputs. This is the most polished Python front-end for *any* spectral wave
  model, and noticeably more mature than anything equivalent for WW3.
- **[wavespectra](https://github.com/wavespectra/wavespectra)** `(v)` reads SWAN spectra
  natively (`read_swan`), so your post-processing is shared between the two models.
- **swantools** (PyPI) — older, lighter; reads TABLE, SPECOUT and BLOCK output into pandas.
- **OMUSE** packages SWAN for the Oceanographic Multi-purpose Software Environment.

## Exercise

Take `examples/02-regional-real-forcing` and extend it into a two-model chain:

1. Run WW3 on the 0.1° shelf grid as you already do.
2. Use `ww3_ounp` to write 2D spectra along a line just offshore of Florianópolis.
3. Build SWAN, set up a ~50 m nested grid over the bay, and feed it those spectra as a
   boundary condition.
4. Compare nearshore `Hs` against running WW3 alone at the same 50 m resolution.

You will learn three things: that the WW3-only run takes far longer for the same domain
(the CFL limit, made personal); that they don't agree; and that converting spectral
boundary data between two models' conventions is where the real work in coastal modelling
actually goes.

→ Back to [`README.md`](README.md).
