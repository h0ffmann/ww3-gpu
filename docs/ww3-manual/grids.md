# Grids, storage and parallel layout

Summary of the WW3 7.14 manual, sections 3.4.3 to 3.4.5, 3.4.8, 3.4.9, 3.14, 6.4 and 6.5.
Evidence and provenance: [README.md](README.md). At the pin: `manual/num/space_*.tex`,
`nest.tex`, `manual/sys/store*.tex`, `optim.tex`.

## Grid types (3.4)

The type is a string read by `ww3_grid` and stored in `mod_def.ww3`; the coordinate system
(spherical or Cartesian) and the closure are read there too. The old compile switches `LLG` and
`XYG` are gone.

| Type | String | Propagation | Notes |
|---|---|---|---|
| regular | `RECT` | `PR1`, `UNO`, `UQ` | constant $\Delta x$, $\Delta y$; longitude closure is "simple" (same latitude index across the seam) |
| curvilinear | `CURV` | the same schemes | a Jacobian converts to a straightened space inside the propagation routine only, per call and per spectral component; unity on regular grids. Tripole closure `TRPL` works with `PR1` only |
| triangles | `UNST` | contour residual distribution schemes from the Wind Wave Model II: explicit N (default, first order, diffusive), PSI, FCT, and an implicit N scheme | chosen in the `UNST` namelist; nodes stored as a vector with `NY=1`. Explicit: the usual fractional steps, sub-steps from the largest CFL number. Implicit: one linear system per $\Delta t_g$ with the source terms linearised ($\epsilon=1$), the other time steps have no effect, and the result depends on the time step when CFL exceeds 1. No GSE correction; the N scheme's own diffusion compensates |
| Spherical Multiple-Cell | `SMC` switch | UNO2, or UNO3 with the `PSMC` namelist | latitude-longitude cells merged towards the pole and refined by quartering; relaxes the CFL limit at high latitude; refraction by a rotation scheme, limited by `RFMAXD` (36° by default); `B4B` makes its OpenMP sums reproducible |

Also: a rotated-pole grid (`RTD`, pole set in namelist `ROTD`), treated inside the model as a
standard latitude-longitude grid (3.4.9); and a continuously moving grid (`MGP`, `MGW`, `MGG`)
for idealised hurricane tests in deep water on a Cartesian grid (3.4.8).

Shoreline treatment on triangles uses the `IOBPD` array, updated whenever `MAPSTA` changes.

## Parallel propagation on triangles (3.4.4)

Two approaches: the "card deck" of the structured grids (below), or domain decomposition with
ParMETIS through the Parallel Decomposition Library (PDLIB, switch `PDLIB`, needs MPI). PDLIB
provides the exchange routines.

## Storage of one grid (6.5.1)

- Spatial counters `ix` (longitude or x, `NX`) and `iy` (`NY`). Input arrays are
  `array(nx,ny)`, read row by row; inside the model they are `array(ny,nx)` and are usually
  addressed as one-dimensional, with `ixy = iy + (ix-1)*my` (`my` the first dimension).
- The outer rows and columns are land, excluded or active boundary points unless the grid is
  closed, so the smallest grid is 3 by 3.
- Spectral counters `ith` (direction, `NTH`) and `ik` (wavenumber, `NK`); a spectrum is also
  addressed as one-dimensional (`ith + (ik-1)*nth` ⚠, the formula is not in the manual).
- **Only sea points store a spectrum**: `a(ith,ik,isea)`, `isea = 1..NSEA`. Sea points include
  active boundary points and ice-covered sea. This array is most of the model's memory.
- The maps between the two: `mapsf(isea,1) = ix`, `mapsf(isea,2) = iy`, `mapsf(isea,3) = ixy`;
  `mapfs(iy,ix) = isea`, 0 on land.

**Status maps.** `mapsta(iy,ix)`: 0 excluded, 1 sea, 2 active boundary; a negative value (-1,
-2) is a sea or boundary point switched off for now. `mapst2` says why, bit by bit: 1 ice, 2
dried out, 3 land in a moving grid or inferred in nesting, 4 masked in two-way nesting; for
`mapsta = 0` it separates land (0) from otherwise excluded (1). They are stored together as
`maptmp = mapsta + 8*mapst2`.

At the pin the array is `VA(:,:)`, two-dimensional (v, `model/src/w3wdatmd.F90:183` at NOAA-EMC/WW3
`03adda91`): the spectral index is already collapsed, and the manual's `a(ith,ik,isea)` is the
three-dimensional view of it. It is the array the resident preset keeps on the device
([AGENTS.md](../../AGENTS.md), "Resident spectrum").

## Multiple grids in one program (6.5.3)

Each data module declares a derived type, an allocatable array of it with one element per grid,
and module-level pointers with the old variable names:

```fortran
TYPE(GRID), TARGET, ALLOCATABLE :: GRIDS(:)
INTEGER, POINTER :: NX, NY, NSEA
REAL, POINTER    :: ZB(:)
...
NX => GRIDS(IMOD)%NX        ! in W3SETG
```

A `W3SETx` call re-points every alias to grid `IMOD`; a `W3DIMx` call allocates and ends with
the matching set call. So `NX`, `NK`, `VA` and the rest are pointers whose target changes when
`ww3_multi` moves to another grid. A kernel that caches a pointer or a size across grids reads
the wrong grid.

| Module | Holds | Set and allocate |
|---|---|---|
| `w3gdatmd` | spatial and spectral grids, every physical and numerical parameter | `W3NMOD`, `W3DIMX`, `W3DIMS`, `W3SETG` |
| `w3wdatmd` | the wave data: spectra and the fields needed for a hot start (such as $u_*$) | `W3NDAT`, `W3DIMW`, `W3SETW` |
| `w3adatmd` | auxiliary fields and work parameters | `W3NAUX`, `W3DIMA`, `W3SETA` |
| `w3odatmd` | output data | `W3NOUT`, `W3SETO` |
| `w3idatmd` | input fields | `W3NINP`, `W3DIMI`, `W3SETI` |
| `wmmdatmd` | multi-grid data | `WMNDAT`, `WMSETM` |

The variables are documented in these files, no longer in the manual (6.6).

## Shared memory (6.4)

OpenMP directives are mainly on the loop over sea points that calls `W3SRCE` and on the
propagation routines. Switches: `OMPG` for general loop directives, `OMPH` for those used only
in the hybrid MPI-OpenMP build. Work arrays are automatic so that the code is thread safe.
Interpolation tables (the dispersion relation, the wind-wave interaction) are part of the
optimisation, set up at initialisation. The vector optimisation dates from 1997.

## Distributed memory, the "card deck" (6.5.2)

- Each spectrum lives on one process. Sea points are dealt with a constant stride:
  `isea = iaproc + (jsea-1)*naproc`, `jsea = 1 + (isea-1)/naproc`,
  `iaproc = 1 + mod(isea-1,naproc)`. `jsea` is the local sea point counter.
- `naproc` processes compute; processes up to `ntproc` can be reserved for output.
- Source terms and intra-spectral propagation need no communication.
- Spatial propagation needs a **data transpose**: for one spectral component `(ith,ik)`, the
  values of all sea points are gathered on one process into a full field (`W3GATH`), propagated,
  and scattered back (`W3SCAT`). Components are assigned to processes so that the number of
  propagation sub-steps is balanced. Communication is non-blocking and persistent, with
  `MPIBUF` buffers.
- Input fields, maps and output fields are kept whole on every process; only `a(ith,ik,jsea)`
  is distributed. Input files are read by every process.
- The cost: much more data moves per step than with a halo exchange, in return for automatic
  load balance of the dynamic source term integration and independence from the stencil.
- Output: `IOSTYP` 0 (each process writes the restart file, direct access), 1 (each file from
  its assigned process), 2 (one dedicated output process), 3 (one per output type).

For this repository: the transpose is why a resident spectrum on a device meets MPI at steps 3
and at output, and nowhere else in the time step
([ADR-0020](../ADRs/ADR-0020-mpi-in-scope-exchange-on-the-resident-spectrum.md)).

## Nesting (3.14, appendix B)

- **One-way** (`ww3_shel`): a run reads boundary spectra from `nest.ww3` and can write up to
  nine `nestN.ww3` files. Boundary data are energy spectra $F(\sigma,\theta)$, interpolated in
  space and time each $\Delta t_g$. Between active boundary points and sea points a first-order
  scheme is used.
- **Two-way** (`ww3_multi`): any number of grids, each with a rank (higher rank, finer). Lower
  to higher rank: boundary data, as above. Higher to lower: after the finer grid has caught up
  in time, the coarse point takes the area-weighted average of the fine points covering it
  (3.94), except where it feeds the finer grid's boundary. Equal rank with overlap: both advance
  one step, then each keeps its own interior and a weighted average where interiors overlap.
- Grids of a mosaic may have different spectral grids; spectra are converted on the fly.
  Each grid keeps its own time steps (appendix A.2).
