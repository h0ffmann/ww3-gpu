# PDLIB_W3XYPFSN2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim, the MPI callables
and the `PDLIB_W3XYPUG` hunk first.

Explicit advection of one spectral component on the unstructured grid by the N scheme
(fluctuation splitting), with its CFL sub-stepping, called through `PDLIB_W3XYPUG` under `PDLIB`.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_W3XYPFSN2`, `model/src/w3profsmd_pdlib.F90:868-1200` at NOAA-EMC/WW3 `03adda91`, as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it |
| Kokkos code | [`../src/ww_kokkos/pdlib_fsn2.hpp`](../src/ww_kokkos/pdlib_fsn2.hpp) (stages and the driver `w3xypfsn2`) and the shared mesh struct [`../src/ww_kokkos/pdlib_params.hpp`](../src/ww_kokkos/pdlib_params.hpp) |
| C shim | none yet; the driver takes the MPI `ALLREDUCE(MIN)` (`:1063`, only with `LCALC`) and `PDLIB_exchange1DREAL` (`:1188`, after each sub-step) as host callables, outside every kernel |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3profsmd_pdlib.F90` (WW3's CMake Release flags,
gfortran 16.2, `switch_PDLIB`), rebuilt by the translator identical to the CMake build's, read as
GIMPLE and assembly by the translator and by a fresh reviewer, 2026-10-16.

- (v) Sums into nodes: the Fortran adds `KKSUM` (`:1051`) and `ST` (`:1112`) one element corner
  at a time onto a zeroed array; each node's elements come in increasing `IE`
  (`yowpdlibmain.F90:1478-1485,1507-1514`), so the C++, one thread per node walking `IE_CELL` from
  +0, makes the same additions in the same order, with no atomics. `UTILDE` keeps the `+ 0` of
  `SUM` and `DOT_PRODUCT`.
- (v) Two loops stay serial in the Fortran's order: the `LCALC` loop (`DTMAXEXP` carries from node
  to node into `CFLXYMAX`) and the boundary loop (two boundary points on one node keep the last
  store).
- (v) Precision as the object has it: arrays `REAL` (`rkind = 4`, `yowdatapool.F90:48`, MPI type
  `MPI_REAL4`), double only for `FL111..FL312` (`:1033-1038`), `DTMAXEXP`, `CFLXYMAX`, the `MOD`,
  `DTSI` and the `:1124` update; `:1025` and `:1065` narrowed to `divss` by `-O3`, as in the C++.
- (v) Every `MIN`/`MAX` operand order from the assembly. At `:1060` gfortran emits two loop copies
  with opposite order, chosen by `IOBDP_LOC`'s stride; it points at a whole array
  (`w3profsmd_pdlib.F90:600,608`), so the contiguous copy runs, and the header follows it.
- (v) Constants with the object's bits (`-THR` `0x80800000`, the `DBLE(10.E-10)` floor), `NINT` as
  `lroundf`, `MOD` as the inline x87 `fprem`; no FMA in either object; no warnings with
  `-Wconversion`.
- (v) A mesh with a repeated corner in an element cannot reach the solver (its area is exactly 0
  and `PDLIB_INIT` stops, `yowpdlibmain.F90:1399-1405`); `check_cell_adjacency` refuses one.
- (v) CUDA (`sm_89`, `--fmad=false`, compiled, never run): no `fma` in the PTX, `div.rn`
  divisions. `min.f32(-0,+0)` is −0 where `minss` in this order gives +0 (PTX ISA §9.7.3.11-12);
  the only such tie is at `:1025`, whose sign the following sums erase.
- ⚠ The SASS was not read; `NINT`/`MOD` on NaN or Inf not compared; the shim's `cell_off` and
  `check_cell_adjacency` do not exist yet.
- (v) `W3_REF1` (`:923, :959, :1002, :1125`), added 2026-10-16 as `template <bool kRef1 = false>` with a trailing
  `refpars3` argument (`REFPARS(3)` of the grid): at `:1125`, `U(IP) = AC(IP)` when
  `REFPARS(3) < 0.5` (false for NaN) and `IOBPD_LOC(ITH,IP) = IOBPA_LOC(IP) = 0`, reading the
  old `AC`, after the `U` update and before `AC = REAL(U)`; checked against an object built with
  `ww3_tp2.17/input_deep/switch_PDLIB`, and by a second fresh review. `IOBPDR` is filled but never
  read, so it is not translated. With `kRef1 = false` the kernels are the reviewed ones, token for
  token (the diff re-indents them inside `if constexpr`).
- ⚠ That the shim passes `REFPARS(3)` of the right grid is not checked (no shim yet).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. The explicit scheme does not run on `ww3_tp2.17` grid c, the
profiled case (implicit), so its share of NOAA's run time is not measured ⚠.
