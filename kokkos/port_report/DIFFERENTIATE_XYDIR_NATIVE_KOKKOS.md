# DIFFERENTIATE_XYDIR_NATIVE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The x and y derivatives `DVDX`, `DVDY` of a nodal field `VAR` on the unstructured (PDLIB)
triangular grid of the wave setup module (`w3wavset`, switch `SETUP`): per triangle, the
gradient of the linear interpolant (`DOT_PRODUCT` of the three nodal values with the shape
function gradients `PDLIB_IEN`) is added to each of its three nodes, weighted by `2*PDLIB_TRIA`,
and each node's sum is divided by its total weight. The result goes through two MPI halo
exchanges. Nothing in WW3 calls it: the only call is commented out in `DIFFERENTIATE_XYDIR`
(`w3wavset.F90:434`), which calls `DIFFERENTIATE_XYDIR_MAPSTA` instead. It is translated for
completeness and cannot be exercised by any regtest.

## What was ported

| | |
|---|---|
| WW3 routine | `DIFFERENTIATE_XYDIR_NATIVE`, `model/src/w3wavset.F90:105-212` at NOAA-EMC/WW3 `03adda91`. `w3wavset.F90` is built only with switch `SETUP` (`model/src/cmake/switches.json`); no regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `switches.json`). Its only `#ifdef` blocks are `W3_S` (`STRACE`), output only |
| Kokkos code | [`../src/ww_kokkos/wavset_differentiate_xydir_native.hpp`](../src/ww_kokkos/wavset_differentiate_xydir_native.hpp), `ww::wavset::differentiate_xydir_native`, an `inline` host function template |
| Structural changes | the automatic array `WEI(npa)` (`:179`, a `malloc` in the object) is a caller-provided work array; `PDLIB_exchange1Dreal` (`:210-211`), an MPI exchange, is a callable `(float*)` the caller binds; module state `npa`, `NE`, `INE`, `PDLIB_IEN`, `PDLIB_TRIA` is passed as arguments. `MAPSTA` is USEd (`:157`) and never read |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90`, WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (DIST MPI PDLIB SCOTCH), recompiled and found
identical to the CMake object (v), read against a throwaway C++ unit with the openmp-release
flags (warning-free), 2026-10-16.

- (v) `REAL(rkind)` is `float`: `rkind = 4` (`PDLIB/yowdatapool.F90:48`).
- (v) `2.*PDLIB_TRIA(IE)` (`:193`): `addss x,x` in the Fortran, `vaddss x,x,x` in the C++, exact.
- (v) The vector-subscript updates `WEI(NI) = WEI(NI) + ...`, `DVDX(NI) = ...`, `DVDY(NI) = ...`
  (`:193`, `:202-203`) are done in place, node 1, 2, 3 in order, with no temporary, in the GIMPLE
  and in both objects; each node is reloaded after the previous store, so a triangle with a
  repeated node adds twice in both.
- (v) `DOT_PRODUCT` (`:200-201`) is inlined by gfortran as `(p1 + 0.0) + p2 + p3`, the `+ 0.0`
  kept (`val = _278 + 0.0` in the GIMPLE, `addss` with a zero register in the object); the C++
  keeps it (`vaddss` with a zeroed register), so a `-0.0` product becomes `+0.0` in both.
- (v) The final loop `DVDX(IX)/eW`, `DVDY(IX)/eW` (`:205-208`) is vectorised in both (4-wide and
  2-wide in the Fortran, 8-wide and 4-wide plus scalar tail in the C++); a packed division is the
  same IEEE operation per lane, so the bits are the same.
- (v) No FMA in either object, no libm or libmvec call, no MIN/MAX, no conversion.
- ⚠ Operand order of the commutative operations differs at some sites: the Fortran computes
  `PDLIB_IEN*VAR` and `acc + p`, and `WEI + tw`, `DVDX + dvdxie` for nodes 1 and 2 but
  `tw + WEI`, `dvdxie + DVDX` for node 3; the C++ has `VAR*PDLIB_IEN`, `p + acc`, `tw + WEI` and
  `dvdxie + DVDX` at every node, because g++ puts the memory operand second. The results are the
  same bits except for which NaN payload propagates when both operands are NaN. The Fortran's own
  order is a register allocation choice that source order does not steer.
- ⚠ A node in no triangle keeps `WEI = 0`, and `0/0` gives NaN in `DVDX`, `DVDY` (`:206-208`);
  reproduced, not guarded.
- ⚠ CUDA not compiled: the function is host-only, since it takes the MPI exchange.
- (v) Fresh review 2026-10-16: no defect. The object of `w3wavset.F90` recompiled from the
  `build_setup` flags (identical to the CMake object) read against a new C++ unit: the same
  in-place node 1, 2, 3 updates of `WEI`, `DVDX`, `DVDY` with a reload after each store, the
  `+ 0.0` that starts each `DOT_PRODUCT`, `2.0*PDLIB_TRIA`, and an element-wise division loop,
  no FMA, no libm call. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `w3wavset.F90`, and nothing in WW3 calls
this routine.
