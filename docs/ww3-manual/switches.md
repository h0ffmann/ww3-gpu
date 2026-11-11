# Compile-time switches

Summary of the WW3 7.14 manual, sections 5.9 and 6.2. Evidence and provenance:
[README.md](README.md). At the pin: `manual/impl/switch.tex`, `manual/sys/w3adc.tex`.

## How a switch reaches the code (6.2)

The manual describes source lines of the form `!/SWT`, turned into code by the preprocessor
`w3adc` when the switch `SWT` is in the `switch` file. That is the mechanism up to 2021. At the
pin a switch `SWT` is the C preprocessor macro `W3_SWT`, and the lines are
`#ifdef W3_SWT ... #endif` (v, `model/src/w3srcemd.F90:569`, `#ifdef W3_ST4`, at NOAA-EMC/WW3
`03adda91`). The switch names and their meaning are unchanged; the `switch` file is still one
line of names, in any order.

A switch decides what is compiled. A namelist value in `ww3_grid` decides a parameter inside the
compiled option and is stored in `mod_def.ww3`. Since version 4 the grid type and the coordinate
system are run-time choices, not switches.

## Mandatory groups: exactly one of each (5.9.1)

| Group | Switches |
|---|---|
| memory model | `SHRD` shared, `DIST` distributed |
| message passing | `SHRD` none, `MPI` (only with `DIST`) |
| propagation and GSE method | `PR0` none, `PR1` first order, `PR2` higher order with diffusion, `PR3` higher order with averaging |
| propagation scheme | `PR0`, `PR1`, `UNO` second order, `UQ` third order |
| stress | `FLX0` computed in the source term, `FLX1` equation (2.62), `FLX2` Tolman and Chalikov, `FLX3` the same with a cap, `FLX4` equation (2.149), `FLX5` from an atmospheric stress given as input |
| linear input | `LN0` none, `SEED` seeding (3.70), `LN1` |
| input and dissipation | `ST0` none, `ST1`, `ST2`, `ST3`, `ST4`, `ST6`; optional additions `STAB2` (with `ST2`), `STAB3` (with `ST3`, `ST4`), `STAB0` (no effect) |
| nonlinear interactions | `NL0` none, `NL1`, `NL2`, `NL3`, `NL4` (and `NL5`, described in 2.3.6 but missing from this list) |
| bottom friction | `BT0` none, `BT1`, `BT4`, `BT8`, `BT9` |
| ice damping | `IC0` none as a source term, `IC1`, `IC2`, `IC3`, `IC4` (and `IC5`, in 2.4.5) |
| ice scattering | `IS0` none, `IS1`, `IS2` |
| reflection | `REF0` none, `REF1` |
| depth-induced breaking | `DB0` none, `DB1` |
| triads | `TR0` none, `TR1` |
| bottom scattering | `BS0` none, `BS1` |
| wind interpolation in time | `WNT0` none, `WNT1` linear, `WNT2` approximately quadratic |
| wind interpolation in space | `WNX0` vector, `WNX1` approximately linear speed, `WNX2` approximately quadratic speed |
| current interpolation | `CRT0`, `CRT1`, `CRT2`; `CRX0`, `CRX1`, `CRX2` |
| GRIB | `NOGRB`, `NCEP2` |

The physics behind each is in [source-terms.md](source-terms.md) and
[numerics.md](numerics.md).

## Optional switches (5.9.2)

| Kind | Switches |
|---|---|
| OpenMP | `OMPG` general loop directives (pure OpenMP: `SHRD OMPG`), `OMPH` the directives of the hybrid build (hybrid: `DIST MPI OMPG OMPH`) |
| reproducibility | `B4B`: forces bit-for-bit reproducible OpenMP sums, by scaling values to integers before a reduction; affects only code compiled with `SMC` |
| triangles in parallel | `PDLIB` domain decomposition, needs ParMETIS and MPI |
| grids | `SMC`, `RTD` rotated pole, `MGP` `MGW` `MGG` moving grid |
| physics additions | `MLIM` Miche limiter (3.71), `NLS` nonlinear filter, `UOST`, `IG1`, `RWND` wind relative to the current, `REFRX` refraction from phase speed gradients, `TIDE`, `FLD1` `FLD2`, `XW0` `XW1` and `DSS0` (variants of the `PR2` diffusion) |
| coupling | `COU`, `OASIS` with `OASACM` `OASOCM` `OASICM`, `SCRIP` `SCRIPNC` remapping, `WRST` |
| time | `SEC1` global time steps below 1 s |
| operations | `NCO` NCEP operational file names and unit numbers |
| output detail | `O0` to `O16` (extra printed output of the pre- and post-processors, `O7b` and `O10` to `O13` for `ww3_multi`) |
| test output | `S` subroutine tracing (`STRACE`), `T` and `Tn` test output per routine, `MPIT`, `MPRF` profiling of `ww3_multi`, `REFT`, `TIDET`, `NNT` |
| tracking | `TRKNC` NetCDF in `ww3_systrk` |

For this repository's bit-for-bit rule: `B4B` exists because OpenMP reductions reorder sums, and
the manual limits its effect to `SMC` code. Whether other OpenMP sums of WW3 reproduce is
measured in [CONTRIBUTING.md](../../CONTRIBUTING.md), "Is WW3 deterministic enough for bit for
bit".

## Reading a switch file

An example from the manual's preprocessor input (figure 6.1), a shared-memory build with the
Tolman and Chalikov physics:

```text
NOGRB SHRD PR3 UQ FLX2 LN1 ST2 STAB2 NL1 BT1 DB1 MLIM TR0 BS0 WNX1 WNT1 CRX1 CRT1 O0 O1 O2 O3 O4 O5 O6 O7 O11 O14
```

It carries no `IC`, `IS` or `REF` name: the figure predates those groups. The switch files this
repository builds are in `switches/` (the lab one is `switch_lab_shrd`, `ST4`), and WW3's own
are in `model/bin/switch_*` and in each regtest's `input*/switch*`.

The manual says there is no single default configuration since version 3.14; centres publish
their own switch files.
