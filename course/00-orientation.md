# 00 — Orientation: what WW3 actually computes

## The one equation

WW3 solves the **spectral action density balance equation**. At every point in space and
every moment in time, it carries a two-dimensional spectrum: how much wave energy sits at
each frequency and each direction.

$$\frac{\partial N}{\partial t} + \nabla_{\mathbf{x}} \cdot (\dot{\mathbf{x}} N)
+ \frac{\partial}{\partial k}(\dot{k} N) + \frac{\partial}{\partial \theta}(\dot{\theta} N)
= \frac{S}{\sigma}$$

where $N(k, \theta; \mathbf{x}, t) = F/\sigma$ is **action** density (energy over intrinsic
frequency — conserved in the presence of currents, where energy isn't).

Read the terms left to right and you have the whole model:

| Term | Physics | WW3 switch/flag |
|---|---|---|
| $\partial N/\partial t$ | evolution in time | — |
| $\nabla_x \cdot (\dot x N)$ | waves travel at the group velocity | `RUN%FLCX`, `RUN%FLCY` |
| $\partial(\dot k N)/\partial k$ | shoaling, current-induced frequency shift | `RUN%FLCK` |
| $\partial(\dot\theta N)/\partial\theta$ | refraction by depth and current | `RUN%FLCTH` |
| $S/\sigma$ | **all the physics you can't derive** | `RUN%FLSOU` |

The left side is clean conservative transport in a 4D phase space (x, y, k, θ). It's
numerics, and it's solvable. The right side is where the model earns its keep and where
all the uncertainty lives.

The first half of this course (00–08) teaches you to *run* the thing that solves this
equation. The second half (09–13) teaches you to make it run *faster without changing the
answer*, and it follows the ladder of the UFRJ/DEL project proposal that this repository
serves: compile options → run configuration → modern Fortran → C++/Kokkos kernels for the
GPU, each rung gated by parity against the reference run before the next one is allowed
to start. The proposal itself is in [`../pubs/proposal/pt/`](../pubs/proposal/pt/)
(Portuguese, eight short sections) and its mind maps in
[`../pubs/proposal/mapas-mentais.pt.md`](../pubs/proposal/mapas-mentais.pt.md); the
lessons are the English, hands-on version of the same ladder, with the code in
[`../kokkos/`](../kokkos/).

## The source terms

$$S = S_{in} + S_{nl} + S_{ds} + S_{bot} + S_{db} + S_{ice} + \dots$$

- $S_{in}$ — **wind input**. Momentum from the atmosphere into the waves. The
  parameterisation you pick (`ST2`, `ST4`, `ST6`) mostly means picking a version of this.
- $S_{nl}$ — **nonlinear wave-wave interaction**. Four-wave resonant quartets shuffle
  energy between frequencies. This is the term that makes the spectrum evolve towards its
  characteristic shape rather than just growing where the wind pushes. The exact
  computation is ~1000× the cost of everything else combined, so everybody uses the
  Discrete Interaction Approximation (`NL1`, Hasselmann 1985) and everybody knows it's the
  weakest link.
- $S_{ds}$ — **dissipation**, mostly whitecapping. Historically the tuning term: whatever
  was needed to make $S_{in} + S_{nl}$ balance out to observed spectra.
- $S_{bot}$, $S_{db}$ — bottom friction, depth-induced breaking. Shallow water only.
- $S_{ice}$ — ice attenuation and scattering (`IC1`–`IC5`, `IS1`/`IS2`).

**The crucial thing to internalise:** $S_{in}$ and $S_{ds}$ are individually large and
individually uncertain, and they nearly cancel. The net is a small difference of big
numbers. That's why source-term packages come as matched sets — you cannot mix the `ST4`
input with the `ST6` dissipation and expect anything sensible.

## Phase-averaged, not phase-resolving

WW3 does **not** know where any individual wave crest is. It tracks statistics. If you want
a surface you can render, or wave-by-wave runup on a beach, or slamming loads on a hull,
you want a different class of model entirely (SWASH, XBeach, FUNWAVE, OpenFOAM — see
`docs/AWESOME-WW3_202609.md`).

What WW3 gives you is: the spectrum, and everything derivable from it. $H_s$, peak period,
mean direction, directional spread, Stokes drift, radiation stresses, partitioned swell
systems.

## The five dimensions

Every field WW3 carries lives on **latitude × longitude × frequency × direction × time**.
A modest regional run — 81 × 81 points, 32 frequencies, 36 directions — is 7.5 million
numbers *per timestep*, before any output. This is why WW3 is expensive, and it is directly
relevant to the GPU question in lessons 11–13: you cannot casually shuttle that array
across PCIe every step, so a port has to decide where the spectrum *lives*.

## The program pipeline

WW3 isn't one binary. It's a family, communicating through files:

```
  ww3_grid   bathymetry + spectrum + timesteps  ──►  mod_def.ww3   [THE model definition]
  ww3_strt   initial conditions                 ──►  restart.ww3
  ww3_prnc   netCDF forcing → binary            ──►  wind.ww3, current.ww3, ice.ww3
  ww3_bounc  spectra from a parent run          ──►  nest.ww3
     │
     ▼
  ww3_shel   THE MODEL (single grid)            ──►  out_grd.ww3, out_pnt.ww3, log.ww3
  ww3_multi  THE MODEL (mosaic of grids)              restart.ww3
     │
     ▼
  ww3_ounf   gridded fields    → netCDF         ──►  ww3.*.nc
  ww3_ounp   point spectra     → netCDF         ──►  ww3.*_spec.nc
  ww3_outf / ww3_outp          → ASCII          (legacy, avoid)
  ww3_trnc   track output      → netCDF
  ww3_gint   grid interpolation
  ww3_systrk wave system tracking
  ww3_uprstr restart update from Hs analysis
```

**`mod_def.ww3` is the centre of gravity.** It's an opaque binary produced by `ww3_grid`
containing the complete model definition — grid, spectral discretisation, timesteps,
physics configuration. Every other program reads it. Change `ww3_grid.nml`, and every
downstream artifact is stale until you rerun `ww3_grid`. A large share of all confusing WW3
behaviour reduces to a stale `mod_def`.

## Two input file formats

WW3 accepts **legacy `.inp`** (fixed-format, positional, comment lines start with `$`) and
**modern `.nml`** (Fortran namelists, order-independent, self-documenting). Both work in
v7. Everything in this repo uses `.nml`, because:

- the annotated templates in `$WW3/model/nml/` document every single parameter inline;
- you can omit anything you want defaulted;
- the small Fortran generators in `examples/` write them, and so does every third-party
  wrapper you might meet (lesson 08) — nobody targets `.inp` any more.

If you follow an older tutorial and get `error reading input file`, check whether it's
handing an `.inp` to a program expecting `.nml` or vice versa. That's a real and common
failure mode.

## Where to go next

→ [`01-build.md`](01-build.md) — get it compiled.
