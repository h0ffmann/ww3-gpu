# W3SNLSMD_EXPAND_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`EXPAND` of the nonlinear high-frequency smoother `W3SNLS` (switch `NLS`): it copies the action
spectrum `A(NTH,NFR)` into `SPEC(0:NTH+1,0:NFR+2)` with a zero row at `IFR = 0`, two tail
frequencies scaled by `FACHFA`, and one periodic direction on each side, and writes
`PSPC = SPEC / XWN` per frequency. It is a routine contained in `W3SNLS` and reads `A`, `FACHFA`,
`XWN`, `NTH` and `NFR` by host association. The report is named with its module because `W3SNL3`
has its own `EXPAND` (`w3snl3md.F90:644-703`, in `snl3_gmd.hpp`).

## What was ported

| | |
|---|---|
| WW3 routine | `EXPAND` inside `W3SNLS`, `model/src/w3snlsmd.F90:597-651` at NOAA-EMC/WW3 `03adda91`; called once, `CALL EXPAND ( UP, UN )` (`:276`), so `PSPC` is `UP` and `SPEC` is `UN`. `W3SNLS` is called from `W3SRCE` under `W3_NLS` (`w3srcemd.F90:1854-1855`). No `#ifdef` inside `EXPAND` |
| Switches that compile it | `NLS`: no regtest switch and no NOAA operational switch (`switch_NCEP_*`, `switch_NWS_*`); only `model/bin/switch_UoM_nl3s` (`git grep -lw NLS -- '*switch*'`). No regtest can exercise it |
| Kokkos code | [`../src/ww_kokkos/snls_expand.hpp`](../src/ww_kokkos/snls_expand.hpp), `ww::snls::expand`; `KOKKOS_INLINE_FUNCTION`, no allocation, host-associated state passed as arguments |
| C shim | none yet; it would be called by a port of `W3SNLS` |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: `w3snlsmd.F90` recompiled with the flags of WW3's CMake Release build under
`switch_lab_shrd` with `NL4 IC5 BT9 NLS` (gfortran 16.2.0, pinned shell); the object is identical
to the CMake object (`objdump -d` compared). `EXPAND` is inlined into `W3SNLS` in that object.
Compared with a throwaway C++ unit built with the `openmp-release` flags (g++ 16.2.0,
`-ffp-contract=off`), 2026-10-16.

- (v) The only floating-point operations are `SPEC*FACHFA` (`:637-638`, `mulps`/`mulss`) and
  `SPEC/XWN(IFR)` (`:644`, `divps`/`divss`, a true division, no reciprocal); the rest is
  `memset` (`:634`), `memcpy` (`:636`) and copies (`:640-641`). The C++ object has `vmulss` and
  `vdivss` and no FMA. gfortran vectorises the three loops and g++ does not; every operation is
  elementwise, so the lanes give the same bits.
- (v) The GIMPLE multiplies as `FACHFA * SPEC` (operands swapped from the source); the C++ keeps
  the source order. IEEE multiplication is commutative in value; only the NaN payload when both
  operands are NaN could differ, and `FACHFA` is a finite grid constant.
- (v) No reduction, no MIN/MAX, no libm or libmvec call, no integer conversion.
- (v) `:640` then `:641` are kept in order; their order cannot matter, since `:640` writes row
  `NTH+1` from row 1 and `:641` writes row 0 from row `NTH`, for every `NTH >= 1`.
- ⚠ `PSPC(:,0)` (`UP(NSPL:NSPL+NTH+1)`) is never written by `EXPAND`, and it is `INTENT(OUT)`.
  `W3SNLS` reads `UP(ISPX-NTHX)` at `IFR = IFRMIN` (`:414`, `:416`); `IFRMIN` stays 1 when no
  `FILTFP(IFR) < 1.E-10` before `IFR = 1` (`:349`, `:355-369`), and then those reads are of undefined memory. The
  port reproduces the Fortran: it leaves `pspc` column 0 as the caller had it.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: `w3snlsmd.F90` recompiled from `build_mixb`'s flags, object
  identical to the CMake object; the inlined `EXPAND` (`:634-645`) has only `mulps`/`mulss` by
  `FACHFA` and `divps`/`divss` by `XWN(IFR)`, `memset`/`memcpy` and copies; the C++ unit has two
  `vmulss` and one `vdivss`, no FMA, no reciprocal, no call. Layout checked against
  `NSPL = -NTHX`, `NSPH = NFRX*NTHX-1` (`:784-785`). No bit defect; a wrong comment and bullet on
  `NTH = 1` overlap at `:640-641` were corrected.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch has `NLS`, so no WW3 regtest can exercise it.
