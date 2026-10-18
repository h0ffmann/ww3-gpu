# W3XYPFSN2_KOKKOS

**Status:** ported and wired, 2026-10-18 (issue #196), inside the batched `W3XYPUG` kernel. Not
regtested: postponed (ADR-0016) ⚠.

The explicit N-scheme of fluctuation-splitting advection on the triangles of an unstructured grid
without `PDLIB`, for one spectral component: flux Jacobians `KELEM` and Simpson weights `FLALL`
per triangle, the number of sub-steps `ITER(IK,ITH)` from the CFL condition when the flow field
changed (`LCALC`), then `ITER` explicit sub-steps with the boundary values of `FLBPI`. The
`PDLIB` sibling is [PDLIB_W3XYPFSN2](PDLIB_W3XYPFSN2_KOKKOS.md), a translation of
`w3profsmd_pdlib.F90`, a different file.

## What was ported

| | |
|---|---|
| WW3 routine | `W3XYPFSN2`, `model/src/w3profsmd.F90:470-727` at the fork pin (`w3profsmd.F90` is identical at `80644ba9` and `50a10730`), called by `W3XYPUG` (`:251-252`) when `FSN` is true |
| Kokkos code | [`../src/ww_kokkos/ug_xypug.hpp`](../src/ww_kokkos/ug_xypug.hpp), with `W3XYPUG`; shim [`xypug_shim.cpp`](../src/fortran_iface/xypug_shim.cpp) |
| Not covered | `W3_REF1` (`:696-700`) and `W3_PDLIB` (`:712-715`): the set-up refuses `REF1`, and the `PDLIB` path calls `PDLIB_W3XYPUG` instead |

## Arithmetic, benchmarks, regtest

See [W3XYPUG](W3XYPUG_KOKKOS.md), section "The batched kernel with W3XYPFSN2": the parallel
structure, the per-node gathers that keep the Fortran's addition order, the GIMPLE comparison and
the builds. No timing (no parity run yet, `AGENTS.md`). The regtest is `ww3_tp2.6` `ST4` and
`ST4_ASCII` with the resident preset, postponed (ADR-0016) ⚠.
