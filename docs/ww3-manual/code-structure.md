# Programs, data flow and modules

Summary of the WW3 7.14 manual, chapters 4 and 6 (6.2 is in [switches.md](switches.md), 6.5 in
[grids.md](grids.md)). Evidence and provenance: [README.md](README.md). At the pin:
`manual/run/*.tex`, `manual/sys/files_w3.tex`, `files_wm.tex`, `fig_w3wave.tex`.

The manual names every source file `<name>.ftn`. At the pin the same modules are
`model/src/<name>.F90` (v, `git ls-tree 03adda91 model/src/`). The names below use no extension.

## The model is a subroutine (4.1, 4.2)

The wave model is `W3WAVE`, called by a program: the single-grid shell `ww3_shel`, the
multi-grid shell `ww3_multi`, or a coupled model (OASIS, appendix E; ESMF/NUOPC, appendix F).
It does not write to standard output; it keeps `log.ww3`.

- `W3INIT` (in `w3initmd`): one grid's initialisation. Unit numbers, time management, reads
  `mod_def.ww3` (through `W3IOGR`, which also initialises the interpolation tables and the
  physics parameterizations), reads `restart.ww3`, prepares output, sets up the persistent MPI
  communication (`W3MPII`, `W3MPIO`, `W3MPIP`).
- `W3WAVE` (in `w3wavemd`): can be called any number of times. It advances one grid to a target
  time: interpolates wind and current, updates ice and water level, propagates, applies the
  source terms, and writes output at the requested times. Its internal step is set by the
  interval asked for and by the output times.
- `WMINIT`, `WMWAVE`, `WMFINL` (`wminitmd`, `wmwavemd`, `wmfinlmd`): the same three roles for a
  mosaic of grids; `ww3_multi` initialises MPI and calls the three.
- `W3WDAS` (`w3wdasmd`): an empty data assimilation interface, called by `ww3_shel` after the
  model reaches the target time and before output.

## Files and programs (4.4)

| Program | Does | Reads | Writes |
|---|---|---|---|
| `ww3_grid` | grid preprocessor: spectral grid, time steps, physics namelists, depths, masks, obstructions | `ww3_grid.nml` or `.inp`, depth, mask and obstruction files | `mod_def.ww3` |
| `ww3_strt` | initial conditions | `mod_def.ww3` | `restart.ww3` |
| `ww3_bound`, `ww3_bounc` | boundary conditions from spectra (text, NetCDF) | spectra files | `nest.ww3` |
| `ww3_prep`, `ww3_prnc` | input fields onto the model grid (text, NetCDF) | wind, current, level, ice fields | `wind.ww3`, `current.ww3`, `level.ww3`, `ice.ww3` |
| `ww3_prtide` | tidal prediction from constituents | | `current.ww3`, `level.ww3` |
| `ww3_shel` | the single-grid model | `mod_def.ww3`, `restart.ww3`, `nest.ww3`, the input fields, `track_i.ww3` | `log.ww3`, `out_grd.ww3`, `out_pnt.ww3`, `track_o.ww3`, `partition.ww3`, `restartN.ww3`, `nestN.ww3` |
| `ww3_multi` | the multi-grid model | the same per grid, with the grid's name as extension | the same per grid, and `log.mww3` |
| `ww3_gspl` | splits a grid into sub-grids for `ww3_multi` | | |
| `ww3_gint` | interpolates gridded output between grids | | |
| `ww3_outf`, `ww3_ounf`, `gx_outf`, `ww3_grib` | gridded output: text, NetCDF, GrADS, GRIB2 | `out_grd.ww3` | |
| `ww3_outp`, `ww3_ounp`, `gx_outp` | point output: spectra, tables, source terms | `out_pnt.ww3` | |
| `ww3_trck`, `ww3_trnc` | track output | `track_o.ww3` | |
| `ww3_systrk` | tracking of wave systems | `partition.ww3` | |
| `ww3_uprstr` | restart file update, for data assimilation | `restart.ww3` | |

- Each `.ww3` file is written and read by one routine: `mod_def` `W3IOGR`, `out_grd` `W3IOGO`,
  `out_pnt` `W3IOPO`, `track_o` `W3IOTR`, `restart` `W3IORS`, `nest` `W3IOBC`, `partition`
  `W3IOSF`. They are unformatted and carry a version string.
- **`mod_def.ww3` holds every compile-independent choice**: grid, spectral grid, time steps,
  and the namelist values of the physics. A changed namelist value needs `ww3_grid` again, not
  a rebuild.
- Configuration is a namelist file (`.nml`, templates in `model/nml/`) or the legacy `.inp`; if
  both are present the namelist wins. This repository's examples use `.nml`.
- A cold start without `restart.ww3`: a calm sea if linear input or seeding is on, else a
  fetch-limited spectrum from the first wind field.
- The action table in `log.ww3` has one line per step with output or input: columns for input
  (boundary, wind, level, current, ice, assimilation data) and output (gridded, point, track,
  restart, boundary, partition, coupling).
- `ww3_outp` can print the source terms at an output point: it calls the source term routines
  itself, so a ported routine is also reached from the post-processors ⚠ (the manual says so
  for `W3SNL4`, 2.3.5; the others were not checked).

## Inside `W3WAVE` (figure 6.3)

The manual's call tree, without service, data-structure and MPI routines:

| Phase | Routines |
|---|---|
| input | `W3UCUR`, `W3UWND`, `W3UINI`, `W3IOBC`, `W3UBPT`, `W3UICE`, `W3ULEV`, `W3UTRN`, `W3DZXY`, `W3MAPn`, `W3NMIN`, `W3CFLXY`, `W3CFLUG` |
| propagation | `W3KTPn`; `W3GATH`, then `W3XYPn` or `W3XYPUG` or `W3PSMC`, then `W3SCAT`; these use `W3QCKn` and `W3UNOn` |
| source terms | `W3SRCE`, which calls `W3SPRn`, `W3FLXn`, `W3SLNn`, `W3SINn`, `W3SNLn`, `W3SDSn`, `W3SWLn`, `W3SBTn`, `W3SICn` and the rest |
| output | `W3CPRT`, `W3OUTG`, `W3IOGO`, `W3IOPE`, `W3IOPO`, `W3IOTR`, `W3IORS`, `W3IOBC`, `W3IOSF` |

The order in time is in [numerics.md](numerics.md), with the line numbers at the pin.

## Module list (6.3)

| Group | Modules (routines) |
|---|---|
| data structures | `w3gdatmd`, `w3wdatmd`, `w3adatmd`, `w3odatmd`, `w3idatmd` ([grids.md](grids.md)) |
| input update | `w3updtmd` (`W3UCUR`, `W3UWND`, `W3UINI`, `W3UBPT`, `W3UICE`, `W3ULEV`, `W3UTRN`, `W3DDXY`, `W3DCXY`) |
| propagation | `w3pro1md` (`W3MAP1`, `W3XYP1`, `W3KTP1`), `w3pro2md`, `w3pro3md` (and `W3MAPT`), `w3uqckmd` (`W3QCK1/2/3`), the UNO routines (`w3uno2md` at the pin), `w3psmcmd` (SMC), `w3profsmd` (`W3XYPUG`, `W3CFLUG`, the N, PSI, FCT and implicit schemes) |
| source terms | `w3srcemd` (`W3SRCE`); `w3flx1md` to `w3flx3md` (stress); `w3sln1md`; `w3src0md` to `w3src6md` (`W3SPRn`, `W3SINn`, `W3SDSn`); `w3swldmd` (`W3SWL4`, `W3SWL6`); `w3snl1md` to `w3snl3md`, `w3snlsmd`; `w3sbt1md`, `w3sbt4md`, `w3sbt8md`, `w3sbt9md`; `w3sdb1md`; `w3str1md`; `w3sbs1md`; `w3sic1md` to `w3sic3md`, `w3sis2md`; `w3ref1md` |
| I/O | `w3iogrmd`, `w3iogomd` (`W3OUTG` computes the gridded fields), `w3iopomd`, `w3iotrmd`, `w3iorsmd`, `w3iobcmd`, `w3iosfmd` |
| namelists | `w3nmlgridmd`, `w3nmlshelmd`, `w3nmlmultimd`, `w3nmlprncmd`, `w3nmlounfmd`, `w3nmlounpmd`, `w3nmltrncmd`, `w3nmlbouncmd` |
| service | `constants` (physical constants), `w3arrymd`, `w3bullmd`, `w3cspcmd` (spectral grid conversion), `w3dispmd` (dispersion relation and its tables), `w3gsrumd` (regridding), `w3partmd`, `w3servmd`, `w3timemd`, `w3triamd` |
| multi-grid | `wminitmd`, `wmwavemd`, `wmfinlmd`, `wmmdatmd`, `wmgridmd` (grid relations), `wmupdtmd` (input), `wminiomd` (internal communication), `wmiopomd` (point output), `wmunitmd` (unit numbers), `wmscrpmd` (SCRIP remapping); `wmesmfmd` at the pin ⚠ (not in 6.3.2) |

Parameters worth knowing (6.6.1): `constants` holds `GRAV`, `DWAT`, `DAIR`, `KAPPA`, `RADIUS`,
`TPI` and their inverses; `w3dispmd` holds the tables for wavenumber and group velocity from
frequency and depth (`EWN1`, `ECG1`); the offset $\epsilon$ of (3.61) is a parameter in
`w3srcemd`; `w3src3md` holds the dimensions of the stress tables.

Modules at the pin that the manual's list does not have are in [README.md](README.md), "Where
the manual is stale".

## Building and testing (chapter 5)

Chapter 5 describes `bin/w3_setup`, `w3_make`, the `switch` file, the `w3adc` preprocessor and
`make_makefile.sh`. At the pin the build is CMake, the switch file is passed to it, and this
repository drives it through `scripts/ww3-gpu.sh` and `just rt` ([AGENTS.md](../../AGENTS.md),
"Setup and commands"); treat chapter 5 as history, except its list of switches. The regression
tests live in `regtests/` (5.11); the `matrix.base` list this repository runs them from is not in
the manual, see [WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md).
