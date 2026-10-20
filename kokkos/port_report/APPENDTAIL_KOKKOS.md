# APPENDTAIL_KOKKOS

**Status:** ported, phase 1 (translated); called by `W3FLD2` ([report](W3FLD2_KOKKOS.md)) inside the batched
`W3SRCE`. Not regtested by WW3's regtest matrix: postponed (ADR-0016). Goal be5bdbea-3817.

`APPENDTAIL` of `W3FLD1MD` attaches a constant-saturation tail to the extended 2-D spectrum: it rescales the bins
between `KA1` and `KA2` to a linear level `BT`, sets bins `KA2` to `KA3` to the level `SAT` with a `cos**2`
distribution about a direction that turns from the spectral peak to the wind, and the bins beyond `KA3` to the same
distribution about the wind.

## What was ported

| | |
|---|---|
| WW3 routine | `APPENDTAIL`, `model/src/w3fld1md.F90:878-1170` at NOAA-EMC/WW3 `03adda91` (293 lines) |
| Kokkos code | [`../src/ww_kokkos/srce_fld2.hpp`](../src/ww_kokkos/srce_fld2.hpp), `ww::fld2::appendtail` |
| Called from | `ww::fld2::w3fld2` only (`W3FLD1` has its own copy of the call and is not translated) |

## Arithmetic

Reference: the `-O3` GIMPLE of `w3fld1md.F90` (ww3_ufs1.2 switch, gfortran 16.2.0).

- (v) `WN2(K)**3.0` is `powf(x, 3.)`. The three divisions `/(WN2(K)**3.0)` sit in loops over the directions that gfortran
  vectorises for `NTH >= 4`: the first `4*(NTH/4)` directions use libmvec's `_ZGVbN4vv_powf` (all lanes get the same
  argument, so one lane gives the bits) and the rest scalar `powf`. The port does the same through
  `ww::st4::mvec_powf` (host: the SSE function; device: `ww::dm`'s copy of it).
- (v) The first sum (`BT(KA1)`) uses scalar `powf`, once per call. Every `SUM` adds in index order.
- (v) `COS(ANGDIF)**2.0` is `c*c`; `COS(ANGLE2-WNDDIR)` is a lone `cosf`, `SIN` and `COS` of `WNDDIR-ANGLE2` one `sincosf`;
  `ASIN` and `ACOS` are `asinf`, `acosf`.
- (v) `ANGLE2 = SUM(ANGLE1)/MAX(REAL(MAI),1.)` for `MAI > 1`; the `MAXAN`/`MINAN` tests are `a < b ? ...` as GIMPLE.
- (v) `asinf` and `acosf` go through `ww::dm`: on the host the glibc call, on a device `ww::libm::asinf`, `acosf`
  (CORE-MATH's code as glibc 2.44 carries it, `coremath_f.hpp`), equal to glibc's on all 2^32 floats on the host
  (`scripts/libm_bits.sh ww asinf fma 3`, then `acosf`, 2026-10-19). ⚠ No device run has compared them.
- ⚠ A NaN spectrum matches no direction in the `MAXANG` scan: the Fortran leaves `ANGLE1(1)` undefined, the port uses 0.

## Benchmarks

None.

## How it was regtested

As [W3FLD2](W3FLD2_KOKKOS.md): inside the batched `W3SRCE`, byte-identical to the Fortran in the CPU smoke and full
cases of 2026-10-19. WW3's regtest matrix has not run on it (ADR-0016).
