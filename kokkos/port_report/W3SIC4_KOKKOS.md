# W3SIC4_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim and the `W3SRCE`
hunk first.

Damping of waves by sea ice with empirical, frequency-dependent attenuation (switch `IC4`), a
source term `W3SRCE` calls per sea point (`w3srcemd.F90:1381` and `:2164`, with `SPEC` and
`CG1`). NOAA's `switch_NWS_rwps` selects it.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIC4`, `model/src/w3sic4md.F90:121-803` at NOAA-EMC/WW3 `03adda91`, every method of `IC4METHOD` (1 to 10 and the default), as `switches/switch_lab_shrd` with `IC0` replaced by `IC4` compiles it |
| Kokkos code | [`../src/ww_kokkos/srce_ic4.hpp`](../src/ww_kokkos/srce_ic4.hpp), the device function `w3sic4_point`; it returns 201 where the Fortran stops in `EXTCDE(201)` (methods 5 and 6), and the shim must stop the run |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3sic4md.F90` (WW3's CMake Release flags, gfortran 16.2,
the switch above), rebuilt by the translator and by the reviewer identical to the CMake build's,
read as GIMPLE and assembly, 2026-10-16.

- (v) gfortran vectorises four loops with glibc's libmvec: `_ZGVbN4v_expf` at `:544` and `:585`
  over `SIG(0:NK+1)` from element 0, `_ZGVbN4vv_powf` at `:660` and `:707` over `IK = 1..NK`, in
  groups of four then a scalar tail of up to three. The C++ calls the same variants on the same
  groups through `st4_libmvec.hpp` (the reason the function takes `SIG(0)` and `SIG(NK+1)`).
- (v) Every method's operations in the Fortran's order: method 1 `-(c1*TPI/SIG) - c2`, the
  polynomials of methods 2 and 3, method 4's `(EB*DDEN)/CG` and `EMEAN` sum, method 9's
  `CHF*powf(HICE,MPOW)` once before the loop, method 10's left-to-right sum with `x**3 = x*x**2`
  and scalar `powf(10,K)`, `D1D = -((CG*2)*WN_I)`, `FREQ` a true division by `TPI`.
- (v) All 10 `MIN`/`MAX` operand orders from the assembly, NaN and −0 included (methods 3 and 9
  keep a NaN `HICE`, 7 and 8 turn it into 0); every literal with the same value; no FMA in the
  C++ object.
- (v) `INFLAGS2(-7..-3)` and `ICECOEF1..7` (`ICECOEF6/7` as `MUDD`/`MUDT`) map as the Fortran reads
  them (`:511-527`).
- ⚠ On a device the libmvec loops become scalar calls, and CUDA's `expf`, `powf`, `sqrtf` are not
  glibc's; the CUDA build (compiled for `sm_89` with `--fmad=false`, never run) has 454 fused
  multiply-adds in its SASS, taken to be CUDA's math and division, not traced.
- ⚠ The shim's packing of `IC4METHOD` and `ICECOEF`, and its stop on 201, do not exist yet.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a translator and a fresh reviewer
each compared the C++ object (`openmp-release` flags) with the Fortran object; the reviewer found
no defect.
