# Awesome WW3

A curated, annotated list for people who actually want to *run* WW3, not just cite it.

**Verification status.** Entries marked `(v)` were fetched and read while building this list
on **2026-09-11**. Unmarked entries are from background knowledge: the project is real, but
the URL and current status may have drifted. Check before relying on them.

---

## Contents

- [The model itself](#the-model-itself)
- [WAVEWATCH IV — the successor](#wavewatch-iv--the-successor)
- [SWAN — the other one](#swan--the-other-one)
- [Documentation](#documentation)
- [Courses and tutorials](#courses-and-tutorials)
- [Grid and bathymetry tools](#grid-and-bathymetry-tools)
- [Python ecosystem](#python-ecosystem)
- [Forcing and validation data](#forcing-and-validation-data)
- [Coupled systems and forks](#coupled-systems-and-forks)
- [Performance, HPC, GPU](#performance-hpc-gpu)
- [Key papers](#key-papers)
- [Other wave models and simulators](#other-wave-models-and-simulators)
- [Adjacent: hydrodynamics, coastal, and rendering](#adjacent-hydrodynamics-coastal-and-rendering)
- [Community](#community)

---

## The model itself

- **[NOAA-EMC/WW3](https://github.com/NOAA-EMC/WW3)** `(v)` — the canonical repo. Solves the
  random-phase spectral action-density balance equation for wavenumber–direction spectra.
  Supports rectilinear, curvilinear, unstructured (triangular), and SMC grids; shallow-water
  and surf-zone options; wetting and drying.
  - The package has **two halves**: the git repo, plus a **binary data bundle from NOAA's
    FTP** that you pull with `./model/bin/ww3_from_ftp.sh`. Without it, the regression tests
    have no bathymetry or forcing. `(v)`
  - Tagged releases are stale: the newest GitHub *release* is still **6.07.1** (Apr 2019).
    `(v)` Real work lives on the `develop` and `main` branches; people in the wild are
    running **v7.14.x**. Clone a branch, don't download the release tarball.
  - Build is **CMake** (≥3.19) since v7: `cmake .. -DSWITCH=<name-or-path>` then `make -j`. `(v)`
- **[WW3 Discussions](https://github.com/NOAA-EMC/WW3/discussions)** `(v)` — the de-facto
  user support forum, and unusually good. Most "why won't my grid build" questions are
  already answered here. Search before posting.
- **[WW3 Issues](https://github.com/NOAA-EMC/WW3/issues)** `(v)` — 200+ open. Worth grepping
  when a regtest fails in a weird way; it's often a known bug.
- **[polar.ncep.noaa.gov/waves](https://polar.ncep.noaa.gov/waves/wavewatch/)** `(v)` — the
  legacy NOAA project page. Still hosts version documentation, errata, and the auxiliary
  packages (gridgen, genes_gmd). Partly out of date, still the only home for some things.

## WAVEWATCH IV — the successor

**WW4 is real, it is a ground-up rewrite rather than a new WW3 version, and as of
late 2026 it is pre-alpha.** Full treatment in
[`course/14-ww4-and-the-future.md`](course/14-ww4-and-the-future.md).

- **[NOAA-EMC/WW4](https://github.com/NOAA-EMC/WW4)** `(v)` — "Home of the WAVEWATCH IV ™
  (WW4 ™) third-generation wind wave modeling framework."
  Snapshot taken **2026-09-11**: default branch `develop`, **36 commits, no releases or
  tags**, 3 stars, 7 forks, 34 open issues, 1 open PR. Top level is
  `src/ tests/ tools/ templates/ externals/` plus `CMakeLists.txt`, `Doxyfile`,
  `ARCHITECTURE.md`, `AGENTS.md`, and the IP set
  (`INTENT.md`, `LICENSE.md`, `TRADEMARK.md`, `CONTRIBUTORS.md`).
  ⚠ Those numbers were true on one day. Re-check before quoting them.
- **[NCEP Office Note 525](https://doi.org/10.25923/h7j3-1h25)** `(v)` — Tolman, *The
  WAVEWATCH III® Software Modernization Project: Phase I report*, November 2025.
  **The single most useful document in this entire list right now.** Unusually candid: it
  publishes the disagreements inside the discussion group rather than smoothing them over.
  The essentials:
  - Decision is a **complete bottom-up rewrite in a new repository**, explicitly following
    the MOM6-vs-MOM4 precedent. No backward compatibility with WW3.
  - **Drivers**: the "shuffle" parallel decomposition dates to 2002 and won't reach
    exascale; data structures date to the Fortran 90 transition; fractional stepping
    (Yanenko 1971) fights the implicit schemes that unstructured grids need; optimisation
    now means memory access rather than FLOPs *because of GPUs*; UFS wants coupling at the
    level of functional units; the Fortran compiler pool is shrinking; and recompiling
    between regression tests has become unsustainable.
  - **Languages**: C++ (likely with Kokkos) as the initial core; **Rust** named by NOAA/NWS
    as "the modern language of choice for WW4" and developed in parallel; Fortran demoted
    to a *solver-only* language and a fast route to an IOC; Python for scripting, workflow,
    data and product generation but explicitly **not** core or solver; **Julia considered
    and declined** as a core language (small community, workforce risk), still allowed for
    non-operational solvers. The report states plainly there is no community consensus.
  - **Chosen path**: the "dual approach" (C++ core to operations-ready in ~2 years, Rust
    built alongside, ~5 years for a Rust core).
  - **Format changes coming**: consensus to drop big-endian unformatted binary for NetCDF,
    with interest in Zarr; the compile-time switch file is under review; the
    separate-executables workflow (`ww3_grid` / `ww3_prep` / `ww3_shel` / `ww3_ounf`) is
    explicitly listed as a design decision to revisit.
  - **Must survive**: full numerical convergence via the limiter formulation
    (Tolman 2002b), without which you can't separate numerical from physical error.
  - **Timeline**: Phase II began 1 Oct 2025; Phase IV with active community engagement
    expected summer/autumn 2026; **first public release hoped for summer 2027** on the C++
    path. ⚠ We are past the Phase IV date. Check the repo.
  - **WW3 will be sunset.** The report commits to "formally sunsetting most support for
    WW3 once WW4 is mature, with a clearly communicated transition period." Code with no
    owner willing to port it stays in WW3 and is obsolete for WW4.
- **[WW4 wiki](https://github.com/NOAA-EMC/WW4/wiki)** `(v)` — the WW4 portal. The repo
  README deliberately keeps documentation *out* of the repo root and points here.
- **`AGENTS.md` in the WW4 repo** `(v)` — worth reading on its own account. WW4 documents
  "an agentic AI approach used to create, translate or refactor code using AI agents such
  as Copilot or Jules, the latter of which has been used extensively in developing the WW4
  code from WW3", with the agent also enforcing coding standards, doxygen documentation and
  unit tests. This is a large, visible, government-operational experiment in AI-assisted
  translation of scientific Fortran.
- Related reading, all cited in Office Note 525 `(v)`:
  - Tolman (2025a), *What makes a successful community model for research and operations?
    Lessons learned from WAVEWATCH III®*, BAMS, doi:10.1175/BAMS-D-24-0223.1
  - Tolman (2025b), *Software modernization for the UFS: A position paper*,
    doi:10.25923/gfbx-pk53
  - Tolman & Meixner (2025), *Integrated Wind Wave Modeling at NWS*, NCEP Office Note 524,
    doi:10.25923/jzks-6g74
  - Shipman & Randles (2023), *An evaluation of risks associated with relying on Fortran
    for mission critical codes for the next 15 years*, LA-UR-23-23992, doi:10.2172/1970284
    (the Fortran-risk paper the whole discussion leans on).

**What to do about it:** learn WW3. The physics is identical and the concepts transfer
completely: action balance, source-term packages, spectral discretisation, CFL limits,
grids, nesting, partitioning. The *interfaces* won't transfer, and that's fine; they're
the cheap part. And if you were planning to GPU-port WW3 yourself, don't: WW4 Phase IV
targets CPU and GPU efficiency in a code architected for it from the start.

## SWAN — the other one

Full treatment in [`course/15-swan.md`](course/15-swan.md); build script at
`scripts/04_get_swan.sh`.

SWAN (Simulating WAves Nearshore), TU Delft. Same governing equation as WW3, deliberately
different numerics: **implicit, unconditionally stable, no CFL limit**, plus a stationary
mode. That is why coastal work runs WW3 offshore and SWAN nearshore. Current version
**Cycle III 41.51** `(v)`. Free.

| | |
|---|---|
| **Git (use this)** | **https://gitlab.tudelft.nl/citg/wavemodels/swan** `(v)` |
| Official site + downloads | https://swanmodel.sourceforge.io/download/download.htm `(v)` |
| SourceForge releases | https://sourceforge.net/projects/swanmodel/files/swan/ `(v)` |
| Release notes | https://swanmodel.sourceforge.io/modifications/modifications.htm `(v)` |
| Implementation manual (build guide) | https://swanmodel.sourceforge.io/download/zip/swanimp.pdf `(v)` |
| TU Delft group page | https://www.tudelft.nl/en/ceg/about-faculty/departments/hydraulic-engineering/sections/environmental-fluid-mechanics/research/swan `(v)` |

The git repository is recent enough that most tutorials still send you to a tarball. Ignore
the GitHub mirrors; they're stale snapshots.

- Builds with **CMake 3.12+**, and the implementation manual recommends **Ninja** over GNU
  make `(v)`. The older `make config && make ser|omp|mpi` route still works.
- Compile-time options are **specially-formatted comments inside the `.ftn` sources**
  rather than a separate switch file: `!/impi` for MPI in `swmod1.ftn`, `!ADC` for the
  ADCIRC coupling hooks `(v)`. Same rebuild-from-clean discipline as WW3.
- Parallelism: OpenMP and MPI, with **block-Jacobi** or **block-wavefront** strategies for
  the implicit sweeps. The manual's guidance: block Jacobi for non- or quasi-stationary
  runs `(v)`. Block wavefront preserves the sequential operation order and therefore the
  convergence properties, at some cost in parallel efficiency.
- **[SWASH](https://gitlab.tudelft.nl/citg/wavemodels/swash)** `(v)` — the same group's
  non-hydrostatic, phase-resolving model. Site: https://swash.sourceforge.io/ `(v)`
- **[rompy-swan](https://rom-py.github.io/rompy-swan/)** `(v)` — pydantic-validated,
  type-safe SWAN configuration from Python or YAML, with NetCDF/THREDDS data interfaces.
  Noticeably more mature than any equivalent for WW3.
- **[wavespectra](https://github.com/wavespectra/wavespectra)** `(v)` reads SWAN spectra
  natively (`read_swan`), so post-processing is shared across both models.
- **swantools** (PyPI) `(v)` — older and lighter; reads TABLE, SPECOUT and BLOCK output
  into pandas.
- **omuse-swan** (PyPI) `(v)` — SWAN packaged for the Oceanographic Multi-purpose Software
  Environment.
- **Delft3D-WAVE** wraps SWAN and couples it to Delft3D-FLOW. **ADCIRC+SWAN** is the US
  storm-surge standard. **COAWST** ships both SWAN and WW3.

## Documentation

- **The WW3 manual (PDF)** — the single most important document. ~450 pages. Chapter 2 is
  the physics (source terms, propagation schemes); Chapter 3 is the numerics and the switch
  catalogue; Chapter 4 documents every program's input file field by field; Chapter 5 is
  installation. Linked from the repo README and the NCEP page. Read Ch.4 with your `.nml`
  files open next to it.
- **[NOAA-EMC/WW3 Wiki](https://github.com/NOAA-EMC/WW3/wiki)** `(v)` — the developer portal:
  overview, quick-start guides split for users vs developers, FAQ, etiquette, technical notes.
- **[WW3 development best practices (Tolman, MMAB #286)](https://github.com/NOAA-EMC/WW3/wiki/files/guide.pdf)**
  `(v)` — programming style, how to add a source term, how regression testing works. Read
  this before you touch the Fortran.
- **WW3 doxygen** — a browsable rendering of the source. Linked from the repo README. Useful
  for tracing what `W3SRCEMD` actually calls. `(v)` that it exists; ⚠ URL not captured.
- **Annotated namelist templates** in the repo: `model/nml/ww3_*.nml`. `(v)` These are the
  *real* reference for the `.nml` interface: every parameter with its default and an inline
  explanation. `ww3_grid.nml` alone is ~790 lines of commented template. Copy from here, not
  from blog posts.
- **[ww3-docs.readthedocs.io](https://ww3-docs.readthedocs.io/en/latest/)** `(v)` — a
  community-written doc site (partly in Spanish). Covers the legacy `w3_make`/`w3_setup`
  script workflow, which predates CMake but is still what a lot of older material assumes.

## Courses and tutorials

- **[IFREMER WW3 short course](https://data-ww3.ifremer.fr/COURS/WAVES_SHORT_COURSE/)** `(v)` —
  the best free structured course I found. Directory of tutorials, each a PDF exercise plus
  a config directory: basic run, inputs/outputs, nesting, unstructured grids, wave tracking.
  The `TUTORIAL_INOUT` exercise walks through `OUTPUT_TYPE_NML` / `OUTPUT_DATE_NML` and all
  seven WW3 output types in detail.
- **[Sirocco / Toulouse WW3 tutorial (2023)](https://www5.obs-mip.fr/wp-content-omp/uploads/sites/12/2023/03/WaveTutorial2023.pdf)**
  `(v)` — modern (CMake-era, v7) hands-on. Reproduces the EZPONDA field campaign on
  structured grids, then nests a France grid inside a global one using `ww3_bounc`.
  Contains the best practical explanation I've seen of setting boundary segments in
  `ww3_grid.nml` (boundary points must be *inside* the grid, not on the first/last row).
- **[CHPC (South Africa) WW3 install tutorial](https://wiki.chpc.ac.za/research:wave_watch_3)**
  `(v)` — short, concrete, v7.14, CMake, ends with running `ww3_tp2.2`. Good sanity check
  that your build is real.
- **[NCEP WW3 workshop exercises](https://polar.ncep.noaa.gov/waves/workshop/)** `(v)` —
  the original 2013 workshop PDFs. Physics explanations still excellent; the *build*
  instructions are obsolete (`w3_make` era).
- **COMET MetEd modules** on WW3 and swell analysis — free registration, forecaster-oriented
  rather than modeller-oriented. Good for intuition about swell trains and wave partitions.

## Grid and bathymetry tools

- **[NOAA-EMC/gridgen](https://github.com/NOAA-EMC/gridgen)** `(v)` — MATLAB package for
  generating WW3 bathymetry, mask, and subgrid-obstruction grids for rectilinear and
  curvilinear grids. The obstruction grids matter more than people expect: they're how WW3
  represents unresolved islands. Needs MATLAB; the algorithms are documented in
  `grid_generation.pdf`.
- **GEBCO / SRTM15+ / ETOPO** — the usual bathymetry sources. GEBCO 2024+ at 15 arc-seconds
  is the default choice for regional grids.
- **OceanMesh2D / SMS / GMSH** — for unstructured (triangular) WW3 grids. WW3 reads GMSH
  `.msh` files directly via `UNST%FILENAME` `(v)`, confirmed in the `ww3_grid.nml` template.
- **[NOAA-EMC/genes_gmd](https://github.com/NOAA-EMC/genes_gmd)** `(v)` — genetic optimisation
  of the Generalized Multiple DIA free parameters (the `NL3`/GMD nonlinear interaction
  approximation). Niche, but the only tool for it. Designed for v5.16.

## Python ecosystem

- **[pyww3](https://github.com/caiostringari/pyww3)** `(v)` — the wrapper this repo's
  `exercises/` are built around. Wraps `ww3_grid`, `ww3_prnc`, `ww3_shel`, `ww3_ounf`,
  `ww3_ounp`, `ww3_bounc` by generating their `.nml` files from typed Python dataclasses,
  then shelling out. Every class has `to_file()`, `run()`, `update_text()`,
  `populate_namelist()`. Validation happens in `__post_init__`. Python 3.7+; only hard
  dependency is xarray + netCDF4.
  - Author's own framing: *work in progress, API not stable, use at your own risk.* `(v)`
    It's a thin, honest layer. That's exactly what makes it good for learning: you can
    always print the namelist it generated and read it.
  - [PyPI](https://pypi.org/project/pyww3/) `(v)` · [docs](https://pyww3.readthedocs.io/) `(v)`
    · [announcement thread](https://github.com/NOAA-EMC/WW3/discussions/470) `(v)` (includes a
    Colab notebook with a fuller example)
  - Deliberately does **not** support ASCII-input programs like `ww3_outf`. `(v)`
- **[NOAA-EMC/WW3-tools](https://github.com/NOAA-EMC/WW3-tools)** `(v)` — official post-
  processing and validation toolkit. Satellite altimeter collocation, NDBC buoy matching,
  scatter/QQ/Taylor diagrams, statistical metrics. `prep_ww3tools.py` sets paths, optionally
  downloads observations, and runs regtests. This is what you want for "is my run any good".
- **[wavespectra](https://github.com/wavespectra/wavespectra)** `(v)` — the standard xarray
  library for spectral wave data. Reads 15+ formats including WW3, SWAN, WWM, ERA5, NDBC,
  TRIAXYS, Spotter. 60+ methods: `hs()`, `tp()`, `dm()`, `dspr()`, `oned()`, `split()`,
  `rotate()`. Spectral partitioning with the same PTM1–PTM5 naming WW3 uses. Parametric
  spectrum construction (JONSWAP, TMA, Gaussian, Pierson-Moskowitz). Dask-backed. Polar
  plots. If you only install one Python wave package, install this one.
- **[rompy](https://github.com/rom-py/rompy)** `(v)` — "Relocatable Ocean Modelling in
  PYthon". Pydantic-validated, templated model configuration with a plugin system;
  `rompy-swan`, `rompy-schism`, `rompy-xbeach` exist. ⚠ I did not find a `rompy-ww3` plugin:
  the WW3 integration is on the *data* side (reading BoM WW3 catalogs). Still worth knowing
  as the most serious attempt at a general "configure ocean models from Python" framework.
- **[bmi-wavewatch3](https://pypi.org/project/bmi-wavewatch3/)** `(v)` — CSDMS package for
  *downloading* WW3 hindcast data (30-year Phase 1/2, production single- and multi-grid)
  and presenting it as an xarray Dataset. `ww3 fetch "2010-05-22"` from the CLI. Note: this
  is about consuming NOAA's output, not running the model.
- **[ww3tool](https://pypi.org/project/ww3tool/)** `(v)` — a newer, more ambitious wrapper:
  generates the full namelist set for v6.07.1 and v7.14, writes run scripts, does SSH upload
  + Slurm submission + job monitoring to an HPC, and post-processes (Hs maps, directional
  spectra, Jason-3 altimeter validation, NDBC matching). Optional Qt GUI. ⚠ Young project,
  small user base. Read the generated namelists before trusting them.
- **xarray + cfgrib + netCDF4** — the actual foundation. WW3's `ww3_ounf` writes CF-ish
  netCDF; `ww3_ounp` writes spectral netCDF that `wavespectra` reads natively.

## Forcing and validation data

- **ERA5** (Copernicus CDS) — 10 m winds at 0.25°/hourly; the default reanalysis forcing for
  hindcasts. Also has its own wave fields (from ECWAM) for comparison.
- **GFS / GEFS** — operational winds if you want to forecast. NOMADS serves GRIB2.
- **NDBC buoys** — the standard point validation set, including 1D and 2D spectra from some
  stations. `wavespectra` reads the formats.
- **Satellite altimetry** (Jason-3, Sentinel-6, SWOT, CryoSat-2) — along-track Hs. The
  IMOS/AODN and ESA Sea State CCI collections are the cleaned-up versions.
- **GFS-Wave / GEFS-Wave / Great Lakes Wave** `(v)` — NOAA's own operational WW3
  configurations, with output on NOMADS. Useful as a reference "what does a real
  configuration look like" and as boundary conditions for a nested regional run.

## Coupled systems and forks

- **[COAWST](https://code.usgs.gov/coawstmodel/COAWST)** `(v)` — USGS Coupled Ocean–Atmosphere–
  Wave–Sediment Transport: ROMS + SWAN + WRF + WW3 + sediment, coupled through MCT. Vendors a
  full WW3 tree, so it's also a convenient place to *read* regtest input files in a browser.
- **[UFS Weather Model](https://ufs-weather-model.readthedocs.io/)** `(v)` — NOAA's unified
  system; WW3 is the wave component, driven through NUOPC/ESMF with `ww3_shel.nml.IN`
  templates. This is how WW3 is run operationally now. Documents the multi-grid input
  structure (`NFGRIDS`, `WINDLINE`, `ICELINE`, `CURRLINE`, `UNIPOINTS`, `WW3GRIDLINE`).
- **[umr-lops/WW3](https://github.com/umr-lops/WW3)** `(v)` — IFREMER's fork, referenced by
  several European tutorials. Ships the `switch_Ifremer1` / `switch_Ifremer2` switch files.
- **[payu](https://github.com/payu-org/payu)** `(v)` — Australian workflow manager; its WW3
  issues are a good read on the pain of `mod_def.ww3` being an opaque binary generated at
  build-config time rather than run time.
- **NEMO–WW3, CROCO–WW3, SCHISM–WWM** — the main coupled-ocean routes. OASIS-MCT is the usual
  coupler outside NOAA.

## Performance, HPC, GPU

- **[Porting the WAVEWATCH III® (v6.07) wave action source terms to GPU](https://gmd.copernicus.org/articles/16/1445/2023/)**
  `(v)` — Ikuyajolu, Van Roekel, Brus, Thomas, Deng & Sreepathi, *Geosci. Model Dev.* 16,
  1445–1462, 2023. **Read this before you plan any GPU work on WW3.** The essential findings:
  - They profiled WW3 and found `W3SRCEMD` (source-term integration) dominates; that's what
    they offloaded, with OpenACC directives, keeping MPI.
  - Tested on Kodiak and Summit (V100), meshes of 59K and 228K nodes, 1–32 MPI ranks.
  - **Speedup was ~1.3× against 42 CPU cores**, roughly unchanged whether they packed 3 or 4
    MPI ranks per GPU. That's a 35–40% cut in wall time and resource-hours (real, but modest).
  - The limiter is **host↔device data-transfer bandwidth**, plus the fact that `W3SRCEMD` has
    so many local scalars and arrays that register pressure kills occupancy. Using `!$acc
    routine` properly would help but needs significant refactoring.
  - Broader lesson they state plainly: GPUs need far more parallelism exposed at once than
    CPUs, and WW3's structure (global module variables, deep call chains, per-point source
    term integration) fights that.
  - **This work is not merged into `NOAA-EMC/WW3`.** ⚠ Check whether that's changed.
- **[NVIDIA HPC SDK](https://developer.nvidia.com/hpc-sdk)** `(v)` — free. `nvfortran` supports
  Fortran 2003 + much of 2008, CUDA Fortran, OpenACC, OpenMP, and ISO Fortran parallel
  features. Current docs are at **26.5** `(v)`.
- **[OpenACC Getting Started Guide](https://docs.nvidia.com/hpc-sdk/compilers/openacc-gs/)**
  `(v)` — `-acc` to enable, `-gpu=ccXX` to target, `-acc=multicore` to fall back to CPU
  threads, `NVCOMPILER_ACC_NOTIFY=1` to print every kernel launch, `nvaccelinfo` to check
  your driver. The SDK bundles CUDA Toolkit components but **not** the driver.
- **[Accelerating Fortran DO CONCURRENT with GPUs](https://developer.nvidia.com/blog/accelerating-fortran-do-concurrent-with-gpus-and-the-nvidia-hpc-sdk/)**
  `(v)` — `-stdpar=gpu` offloads `do concurrent` with no directives at all. The cleanest
  on-ramp if you're writing new Fortran rather than porting old Fortran.
- **[FahrenheitResearch/wrf-gpu-port](https://github.com/FahrenheitResearch/wrf-gpu-port)**
  `(v)` — someone OpenACC-porting WRF 4.7.1 with NVHPC and reporting 6× on an RTX 5090,
  tested at 250 m real-data resolution, on Ubuntu under WSL2. Their compute-capability table
  confirms **cc89 = RTX 4090 / L40**. ⚠ Unreviewed third-party claim; the 6× is theirs, not
  verified here. Still the most directly relevant "consumer GPU + NVHPC + big Fortran
  geoscience model" example I found, and their `build_libraries.sh` approach (building
  NetCDF/HDF5 *with nvfortran* first) is exactly the gotcha you'll hit with WW3.
- **WW3's own parallelism**: MPI card-deck domain decomposition (`DIST MPI`), OpenMP
  (`OMPG`/`OMPH` switches), and PDLIB + ParMetis for domain decomposition on *unstructured*
  implicit schemes (added in v6.04). `(v)` On a workstation, MPI over your CPU cores is the
  boring answer that actually works.

## Key papers

Foundational reading, roughly in the order a newcomer should hit them:

- **The WAMDI Group (1988)** — the WAM model. WW3's ancestor; defines what "third generation"
  means (integrate the full spectral balance, don't prescribe the spectral shape).
- **Komen et al. (1994)**, *Dynamics and Modelling of Ocean Waves* — the textbook.
- **Tolman (1991)** — the third-generation model for wind seas on slowly varying currents and
  depths; the original WAVEWATCH formulation.
- **Hasselmann et al. (1985)** — the Discrete Interaction Approximation (DIA), i.e. WW3's
  `NL1`. Still the default, still the biggest approximation in the model.
- **Ardhuin et al. (2010)**, *JPO* — the `ST4` source-term package (swell dissipation,
  saturation-based whitecapping). The parameterisation most regional modellers now use. `(v)`
  referenced as the default in the Sirocco tutorial.
- **Rogers, Babanin & Wang (2012)** and the BYDRZ / `ST6` line — the observation-based
  alternative to ST4.
- **Tolman & Grumbine (2013)** — Generalized Multiple DIA (GMD, `NL3`).
- **Ikuyajolu et al. (2023)** — GPU port, above.
- **WW3 Development Group (2019)**, *User manual and system documentation of WAVEWATCH III
  version 6.07*, NOAA/NWS/NCEP/MMAB Technical Note 333 — the canonical citation.

## Other wave models and simulators

You asked for a list. Wave modelling splits into families that do genuinely different things:
a spectral model and a phase-resolving model are not substitutes.

### Third-generation spectral (phase-averaged) — WW3's direct peers

| Model | Origin | Notes |
|---|---|---|
| **SWAN** | TU Delft | The other one everybody uses. Implicit, unconditionally stable, no CFL limit, plus a stationary mode, so it eats the high-resolution coastal domains that WW3's explicit propagation makes ruinous. Free. **See the dedicated section above** and `course/15-swan.md`. |
| **WAM (Cycle 4.x)** | ECMWF / WAMDI | The original third-gen model. WW3 and ECWAM both descend from it. |
| **ECWAM** | ECMWF | WAM's operational descendant; produces the wave fields in ERA5 and IFS. Now open source as part of the ECMWF open IFS ecosystem. ⚠ check current licensing. |
| **WWM-III** | Roland et al. | Wind Wave Model III, designed to be coupled tightly to **SCHISM** on unstructured grids. Strong choice for estuary/shelf work. |
| **TOMAWAC** | EDF, part of **openTELEMAC** | Spectral model in the TELEMAC-MASCARET suite; couples naturally to TELEMAC-2D/3D hydrodynamics and SISYPHE sediment. |
| **MIKE 21 SW** | DHI | Commercial. Slick, well-supported, unstructured. What consultancies use. |
| **Delft3D-WAVE** | Deltares | Delft3D's wrapper around SWAN, coupled to Delft3D-FLOW. Open source. |
| **UnSWAN** | TU Delft | SWAN on unstructured grids — the same physics, triangular meshes. |

### Phase-resolving — individual waves, not spectra

| Model | Type | Notes |
|---|---|---|
| **SWASH** | Non-hydrostatic NSE | From the SWAN group. Wave-by-wave in the surf zone: breaking, runup, infragravity waves. Git: https://gitlab.tudelft.nl/citg/wavemodels/swash `(v)` |
| **XBeach** | Coupled short-wave + flow + morphology | The standard for storm-impact and dune-erosion modelling. Has surfbeat and non-hydrostatic modes. |
| **FUNWAVE-TVD** | Boussinesq | Nearshore wave transformation, nearshore circulation, tsunami runup. Well documented. |
| **Celeris** | Boussinesq on GPU | Interactive, real-time, renders while it solves. **If you want to see a GPU run waves tonight, this is the one.** Built for exactly the hardware you have. |
| **REEF3D::FNPF / ::CFD** | Fully nonlinear potential flow / CFD | Potential-flow solver for large wave tanks, plus a full CFD branch. Open source, actively developed. |
| **OceanWave3D** | Fully nonlinear potential flow | Efficient high-order finite-difference potential flow; good numerical-wave-tank engine. |
| **olaFlow / waves2Foam** | OpenFOAM wave generation | VOF two-phase CFD with wave boundary conditions. For structure interaction, overtopping, slamming. Expensive. |
| **DualSPHysics** | SPH, GPU-native | Smoothed-particle hydrodynamics, written for CUDA. Violent free-surface flows. Another genuinely good use of a 4090. |
| **Basilisk** | Adaptive octree NSE | Popek's/Popinet's solver. Beautiful breaking-wave DNS/LES work comes out of it. |

### Wave-energy and offshore-engineering (radiation/diffraction, not weather)

**WEC-Sim** (NREL/Sandia, MATLAB/Simulink), **Capytaine** (Python BEM, actively maintained,
the nicest modern option), **NEMOH** (Ecole Centrale Nantes BEM), **HAMS** (open-source BEM),
**BEMRosetta** (GUI/converter across BEM formats), **OrcaFlex** (commercial, industry
standard for moorings/risers).

### Coupled frameworks that include a wave model

**COAWST** (ROMS+SWAN+WRF+WW3), **UFS** (WW3 via NUOPC), **CROCO** (with WW3 or WWM),
**SCHISM+WWM**, **ADCIRC+SWAN** (the US storm-surge standard), **Thetis** (Firedrake-based,
adjoint-capable, interesting if you care about optimisation and differentiability).

### Machine-learned wave emulators

An active area: learned surrogates for spectral wave models, and wave components inside
data-driven weather models. ⚠ I deliberately haven't named specific systems here because
this field moved fast and I can't verify current state: search "data-driven wave forecasting"
and "ML emulator WAVEWATCH III" for the current picture. The interesting angle for you: a
learned emulator is a *dense tensor* workload, which is precisely what a 4090 is good at,
unlike WW3 itself.

## Adjacent: hydrodynamics, coastal, and rendering

- **Tessendorf / FFT ocean surfaces** — the Jerry Tessendorf "Simulating Ocean Water" notes
  are the origin of essentially every real-time ocean in games and film. Pick a directional
  spectrum (JONSWAP, Pierson-Moskowitz), inverse-FFT it into a heightfield. Physically it is
  the *same spectrum* WW3 computes: WW3 gives you the spectrum's evolution in space and
  time, Tessendorf gives you one realisation of the surface from it. Wiring `ww3_ounp`
  spectral output into an FFT ocean renderer is a genuinely fun weekend project and would
  hammer your GPU properly.
- **CDIP** (Coastal Data Information Program) — buoy spectra and an excellent public explainer
  set on what a directional spectrum actually is.

## Community

- GitHub Discussions on `NOAA-EMC/WW3` `(v)` — primary.
- The WW3 mailing list run by NCEP (`NCEP.List.WAVEWATCH@NOAA.gov`) `(v)` — for code-manager
  contact; slower.
- IFREMER's `data-ww3.ifremer.fr` `(v)` — course material and public hindcast output.
