# PDLIB_W3XYPUG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The per-component driver of the explicit unstructured-grid schemes: it turns `VA` into the
action `AC`, builds the advection velocities, calls the selected scheme
([N](PDLIB_W3XYPFSN2_KOKKOS.md), [PSI](PDLIB_W3XYPFSPSI2_KOKKOS.md) or
[FCT](PDLIB_W3XYPFSFCT2_KOKKOS.md)) and stores the result back into `VA`, under `PDLIB`.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_W3XYPUG`, `model/src/w3profsmd_pdlib.F90:626-866` at NOAA-EMC/WW3 `03adda91`; its `REF1` object is the plain one plus padding, so `kRef1` only reaches the schemes |
| Kokkos code | [`../src/ww_kokkos/pdlib_w3xypug.hpp`](../src/ww_kokkos/pdlib_w3xypug.hpp), `pdlib_w3xypug<kRef1>`; returns `kXypugStopFsnimp` where the Fortran stops for `FSNIMP` (`:837`), with `VA` untouched |
| C shim | none yet; `boundary_times` (the two `DSEC21`, only with `FLBPI`) is a host callable, and `allreduce_min` and `exchange` pass through to the scheme. nvcc refuses local lambdas as callables, so the shim passes named functors |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` objects of `w3profsmd_pdlib.F90` for `switch_PDLIB` and with `REF1`
(gfortran 16.2), identical to the CMake builds', read by the translator and a fresh reviewer,
2026-10-16.

- (v) The first true flag selects the scheme, `FSN`, `FSPSI`, `FSFCT`, as at `:830-838`; with none set
  no scheme runs and `VA` is still written back, as in the Fortran.
- (v) `AC = (VA/CG)*CLATS`; `VLCFLX = (CG*CCOS)/CLATS`, `VLCFLY = CSIN*CG`; the current terms
  `(FACX*CX)/CLATS` added; `C = REAL(IOBDP_LOC)*VLCFL`; all `REAL`, no FMA.
- (v) The store `VA = MAX(0., CG/CLATS*AC)` (`:853-858`) is `divss`, `mulss`, then `maxss` with +0 as
  the source, so NaN and −0 store +0, in both objects.
- (v) `MAPSTA_HACK` is `.FALSE.` (`:113`) and never set; its branches are translated and its restore
  is serial. `VGX`, `VGY` are read only under `W3_MGP`, in no `PDLIB` switch. `JSEA = IP` is the
  Fortran's own assumption (`:762`), copied.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`; `max.f32` for the store), never run; `max.f32` on −0 taken
  from the PTX ISA, not re-read.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
