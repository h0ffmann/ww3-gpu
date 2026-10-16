# W3SREF_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16). Its checkpoint (ADR-0011) needs a shim and the `W3SRCE`
hunk first.

Reflection of wave energy by shorelines, unresolved islands and icebergs (switch `REF1`), a
source term `W3SRCE` calls per sea point (`w3srcemd.F90:2311-2313`, `A = SPEC`, `DT = DTG`,
`S = VREF`). NOAA's `switch_NWS_rwps` selects it.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SREF`, `model/src/w3ref1md.F90:111-670` at NOAA-EMC/WW3 `03adda91`, as `switches/switch_lab_shrd` with `REF0` replaced by `REF1` compiles it; the `IG1` pass is not compiled there and not ported |
| Kokkos code | [`../src/ww_kokkos/srce_ref1.hpp`](../src/ww_kokkos/srce_ref1.hpp), the device function `w3sref_point` |
| C shim | none yet |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), batch A, issue #42 |

## Arithmetic

The reference is the `-O3` object of `w3ref1md.F90` (WW3's CMake Release flags, gfortran 16.2,
the switch above), rebuilt by the translator and by the reviewer, both byte-identical to the CMake
build's, read as GIMPLE and assembly, 2026-10-16.

- (v) Every operation in the Fortran's order: the `EB` sum `(EB*DDEN)/CG`, the power trees
  (`SLOPE**5`, `MAX(FMEANA,0.001)**4` as `(m*m)*(m*m)`), the shoreline updates `(S+T1)+T2` in the
  three `CASE` arms of the structured and unstructured branches, `:614`, `:623`, `:631-634`, the
  subgrid loop; the only differences are commuted single products or sums.
- (v) The 16 folded constants with the object's bits; `powi` is libgcc 16.2's `__powisf2`
  (disassembled); `log10f`, `expf`, `powf` as the Fortran calls them (`powf` three calls in the
  C++ for two, same arguments); `SI` in `REAL(8)`; `NINT` as `lroundf`, 0 for NaN.
- (v) All 23 `minss`/`maxss` sites with the object's operand order; no FMA in either object.
- (v) All three `GTYPE` branches (rectilinear and SMC, curvilinear, unstructured); the direction
  wrapping by `MOD`/`ABS`; `IK = NK+1` after the loop, so `SIG(NK+1)` is read at `:614,:623`.
- (v) Not passed because the compiled code never reads them: `WN`, `EMEAN`, `FMEAN`, `DEPTH`,
  `CX1`, `CY1`, `JSEA`, `FMEAN2`.
- ⚠ The `sig` argument is `SIG(0:NK+1)` from `SIG(0)`. A shim that passes the `SIG(1:NK)` section
  the DIA uses would read `SIG(NK+1)` one slot off.
- ⚠ Not compiled here: the `PDLIB` arm (`PDLIB_SI`, `IOBPD_LOC`) and `IG1`. The shim must keep the
  Fortran under either switch, and those regtest entries (`ww3_tp1.7`, `ww3_ts4`,
  `ww3_tp2.17`) do not count toward this port's coverage.
- ⚠ CUDA (compiled for `sm_89` with `--fmad=false`, never run): 339 fused multiply-adds in the
  SASS, taken to be CUDA's `powf`, `log10f`, `expf` and division, not traced; whether those
  return glibc's bits is unknown.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a translator and a fresh reviewer
each compared the C++ object (`openmp-release` flags) with the Fortran object; the reviewer found
no arithmetic defect.
