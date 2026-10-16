# DIFFERENTIATE_XYDIR_MAPSTA_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The x and y derivatives `DVDX`, `DVDY` of a nodal field `VAR` on the unstructured (PDLIB)
triangular grid of the wave setup module (`w3wavset`, switch `SETUP`). Only triangles whose three
nodes are active (`MAPSTA(1,iplg(node)) > 0`) contribute: per such triangle, the gradient of the
linear interpolant (`DOT_PRODUCT` of the three nodal values with the shape function gradients
`PDLIB_IEN`) is added to each of its three nodes, weighted by `2*PDLIB_TRIA`, and each node's sum
is divided by its total weight. A node with no weight or not active gets 0, and a node with
`MAPSTA < 0` is set to 0 again. The result goes through two MPI halo exchanges. Its only caller is
`DIFFERENTIATE_XYDIR` (`w3wavset.F90:433`), itself called by `TRIG_COMPUTE_LH_STRESS`
(`:561`, `:566`, `:573`) on the radiation stresses SXX, SXY and SYY.

## What was ported

| | |
|---|---|
| WW3 routine | `DIFFERENTIATE_XYDIR_MAPSTA`, `model/src/w3wavset.F90:225-350` at NOAA-EMC/WW3 `03adda91`. `w3wavset.F90` is built only with switch `SETUP` (`model/src/cmake/switches.json:806-807`); no regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `switches.json`). Its only `#ifdef` blocks are `W3_S` (`STRACE`), output only |
| Kokkos code | [`../src/ww_kokkos/wavset_differentiate_xydir_mapsta.hpp`](../src/ww_kokkos/wavset_differentiate_xydir_mapsta.hpp), `ww::wavset::differentiate_xydir_mapsta`, an `inline` host function template |
| Structural changes | the automatic array `WEI(npa)` (`:299`, a `malloc` and `free` in the object) is a caller-provided work array; `PDLIB_exchange1Dreal` (`:348-349`), an MPI exchange, is a callable `(float*)` the caller binds; module state `npa`, `NE`, `INE`, `iplg`, `PDLIB_IEN`, `PDLIB_TRIA` and `MAPSTA` is passed as arguments, `MAPSTA` (allocated `MAPSTA(MY,MX)`, `w3gdatmd.F90:1733`) as a pointer to `MAPSTA(1,1)` with its leading dimension |
| Caller | [`DIFFERENTIATE_XYDIR_KOKKOS.md`](DIFFERENTIATE_XYDIR_KOKKOS.md) keeps taking this routine as a callable `(var, dvdx, dvdy)`: a direct call inlines this body into the driver, whose Fortran object is a single tail call |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90`, WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (DIST MPI PDLIB SCOTCH), recompiled and found
identical to the CMake object (v), read against a throwaway C++ unit with the openmp-release
flags (warning-free), 2026-10-16.

- (v) `REAL(rkind)` is `float`: `rkind = 4` (`PDLIB/yowdatapool.F90:48`).
- (v) The three `MAPSTA(1,IX) .gt. 0` tests (`:316`) are short-circuited in order in the GIMPLE
  and in both objects; `IX2` and `IX3` are loaded only when the previous test passed. No
  floating-point operation depends on the order.
- (v) `2.*PDLIB_TRIA(IE)` (`:317`): `addss x,x` in the Fortran, `vaddss x,x,x` in the C++, exact.
- (v) The vector-subscript updates `WEI(NI) = WEI(NI) + ...`, `DVDX(NI) = ...`, `DVDY(NI) = ...`
  (`:317`, `:326-327`) are done in place, node 1, 2, 3 in order, with no temporary, in the GIMPLE
  and in both objects, so a triangle with a repeated node adds twice in both.
- (v) `DOT_PRODUCT` (`:324-325`) is inlined by gfortran as `(p1 + 0.0) + p2 + p3`
  (`val.252_371 = _370 + 0.0` in the GIMPLE); the C++ keeps the `+ 0.0` (`vaddss` with a zeroed
  register), so a `-0.0` product becomes `+0.0` in both.
- (v) The division loop (`:330-339`) is scalar in both: `eW > 0.0` (`comiss`/`ja`, false for NaN)
  then `MAPSTA > 0`, two `divss` by `eW`, else two stores of `0.0`. The zeroing loop (`:340-346`)
  tests `MAPSTA < 0` with a sign test in both. No loop of the routine is vectorised in either
  object (`-fopt-info-vec-optimized` reports none).
- (v) No FMA in either object, no libm or libmvec call, no MIN/MAX, no conversion.
- ⚠ Operand order of the commutative operations differs at some sites, as in
  [`DIFFERENTIATE_XYDIR_NATIVE_KOKKOS.md`](DIFFERENTIATE_XYDIR_NATIVE_KOKKOS.md): the Fortran has
  `PDLIB_IEN*VAR`, and `WEI + tw`, `DVDX + dvdxie` for nodes 1 and 2 but `tw + WEI`,
  `dvdxie + DVDX` for node 3; the C++ has `VAR*PDLIB_IEN`, `tw + WEI` and `dvdxie + DVDX` at every
  node. The results are the same bits except for which NaN payload propagates when both operands
  are NaN.
- ⚠ `IX1`..`IX3` and `IX` index `MAPSTA(1,IX)` with the global node number `iplg(.)`; whether
  `MAPSTA`'s second dimension covers every global node under PDLIB is WW3's setup, not checked
  here.
- ⚠ CUDA not compiled: the function is host-only, since it takes the MPI exchange.
- (v) Fresh review 2026-10-16: no defect. The Fortran object recompiled from `ref3/build_setup`'s
  flags (identical to the CMake object) read against a fresh C++ unit: same in-place vector-subscript
  updates, `DOT_PRODUCT` with its `+ 0.0`, `eW > 0` then `MAPSTA` test, two `divss`, no
  vectorised loop and no FMA in either; `MAPSTA(1,IX)` with leading dimension `NY` and the 1-based
  `INE`/`iplg` indexing checked. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `w3wavset.F90`.
