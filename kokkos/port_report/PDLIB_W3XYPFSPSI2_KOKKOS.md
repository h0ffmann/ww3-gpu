# PDLIB_W3XYPFSPSI2_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim, the MPI callables
and the `PDLIB_W3XYPUG` hunk first.

Explicit advection of one spectral component on the unstructured grid by the PSI scheme
(fluctuation splitting with a limiter), with its CFL sub-stepping, the sibling of
[`PDLIB_W3XYPFSN2`](PDLIB_W3XYPFSN2_KOKKOS.md), under `PDLIB`.

## What was ported

| | |
|---|---|
| WW3 routine | `PDLIB_W3XYPFSPSI2`, `model/src/w3profsmd_pdlib.F90:1202-1502` at NOAA-EMC/WW3 `03adda91`, as `WW3/regtests/ww3_tp2.17/input/switch_PDLIB` compiles it |
| Kokkos code | [`../src/ww_kokkos/pdlib_fspsi2.hpp`](../src/ww_kokkos/pdlib_fspsi2.hpp), driver `pdlib_w3xypfspsi2`, on the shared [`pdlib_params.hpp`](../src/ww_kokkos/pdlib_params.hpp) |
| C shim | none yet; `MPI_ALLREDUCE(MIN)` (`:1386`, only with `LCALC`) and `PDLIB_exchange1DREAL` (`:1488`, after each sub-step) are host callables outside every kernel |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3profsmd_pdlib.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_PDLIB`), rebuilt by the translator and, separately, by a fresh reviewer, 2026-10-16.

- (v) The limiter (`:1410-1422`): `FT = (((u1*f1+0)+u2*f2)+u3*f3)*ONESIXTH` and
  `BET1 = (-θ)/(-FT)` in float, `BETAHAT` in double then narrowed, the clamp
  `minsd 1,bh` → `minsd (1-bhj),m` → `maxsd 0,m` with the same first operands in the C++, so NaN
  and ±0 resolve alike; `THETA = b*ft`.
- (v) `KKSUM` (`:1374`) and `ST` (`:1427`): per-corner adds in increasing `IE` from +0 in the
  object; the C++ gathers the same sequence per node, without atomics.
- (v) The `LCALC` loop and the boundary loop serial in the Fortran's order; the sub-step count read
  once (`:1403`); `DTSI = (DT/ITER)/SI` in double.
- (v) `:1383`: `IOBDP_LOC` is an `INTEGER*1` pointer to a whole allocated array
  (`w3gdatmd.F90:1121`, `w3profsmd_pdlib.F90:600,608`), so the contiguous copy of the loop runs,
  `maxsd cfl,(DT/E)`, and the C++ follows it.
- (v) Constants with the object's bits (`0x51ba43b7`, `0x501502f9`, `0x3e2aaaab`, ±THR, 0.001,
  `(double)1e-9f`); `MOD` as `fprem`, `NINT` as `lroundf`; no FMA in either object.
- ⚠ `CFLXYMAX` is written for `IP = 1..npa`, inside the array only while `NSEALM >= npa`.
- ⚠ CUDA compiled by the translator (`sm_89`, `--fmad=false`, no `fma` in the PTX; the SASS's
  FFMA/DFMA are the expansion of `div.rn`), never run; the reviewer did not examine CUDA.
- (v) `W3_REF1` (`:1255, :1292, :1327, :1438`), added 2026-10-16 as `template <bool kRef1 = false>` with a trailing
  `refpars3` argument (`REFPARS(3)` of the grid): at `:1438`, `U(IP) = AC(IP)` when
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
profiled case, so its share of NOAA's run time is not measured ⚠.
