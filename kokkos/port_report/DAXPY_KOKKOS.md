# DAXPY_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The LINPACK update `y := y + a*x` on two REAL(8) vectors, unrolled by 4 (J. Dongarra, 1978),
inside `W3PROFSMD`. It is `PRIVATE` (`w3profsmd.F90:61-65`). Its only callers are in `PGMRES`
(`:4452`, `:4554`, both under `if (lblas)`, a local flag initialised `.false.` at `:4327`, not a switch), and nothing in `model/src` calls `PGMRES`
(`git -C WW3-noaa grep -nwi pgmres -- model/src` finds only its definition and the `PRIVATE`
list), so no WW3 run can reach it. It is translated for completeness.

## What was ported

| | |
|---|---|
| WW3 routine | `DAXPY`, `model/src/w3profsmd.F90:4712-4761` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3`; no `#ifdef` inside the routine, so there is one variant |
| Kokkos code | [`../src/ww_kokkos/profs_daxpy.hpp`](../src/ww_kokkos/profs_daxpy.hpp), `ww::profs::daxpy(n, da, dx, incx, dy, incy)`, `dx` and `dy` 0-based for the assumed-size `DX(1)`, `DY(1)`; both the strided path (`:4724-4738`) and the unit-stride unrolled path (`:4746-4759`) |
| C shim | none; no live caller |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) The object holds two bodies: the generic `__w3profsmd_MOD_daxpy` and a clone
  `daxpy.constprop.0.isra.0` with `INCX = INCY = 1` folded in (the `PGMRES` calls). Both compute
  every element as `round(round(DA*DX(i)) + DY(i))`; the C++ unit does the same (`vmulsd` then
  `vaddsd`, or `vmulpd`/`vaddpd` on two lanes). The GIMPLE writes the add as `p + dy` in some
  sites and `dy + p` in others; IEEE addition is commutative, so the bits are the same.
- (v) gfortran vectorises the clean-up loop (`:4748`) two lanes wide; g++ does too, behind a
  runtime alias check. No update reads another element's result, so vector and scalar give the
  same bits.
- (v) `ABS(DA) .LT. TINY(1.D0)` is `ABS_EXPR < 2.2250738585072014e-308` in both GIMPLE dumps;
  the C++ writes `0x1p-1022`.
- (v) No FMA (`grep -c vfmadd` gives 0 on the C++ object), no libm call, no reduction.
- (v) `MOD(N,4)` is `n % 4` because `n > 0` there (both objects compute `n & 3`). The strided
  start index `(-N+1)*INCX + 1` and the increments are 32-bit signed, as in the Fortran.
- ⚠ `PGMRES` at `:4554` calls `daxpy(n, t, vv(1,j), 1, vv, 1)`, which for `j = 1` passes the
  same array as `DX` and `DY` (non-conforming aliasing; gfortran marks both `restrict`). Every
  update is element-wise, so the result is still defined in practice. The non-BLAS branch beside
  it (`:4556`) updates `vv(:,j)` with `vv(:,1)`, the reverse of the BLAS call. Both are dead code.
- CUDA not compiled (optional this batch).
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3profsmd.F90` from `build_lab`'s flags (identical to the CMake object) and compared the GIMPLE and disassembly of `__w3profsmd_MOD_daxpy` and its `constprop.isra` clone with a C++ unit (generic and unit-stride calls): same `mul` then `add` per element in every path (strided, 2-lane clean-up, scalar tail, unrolled body), same `ABS < DBL_MIN` test, no FMA in either object, no libm call; the `#ifdef`s of the file lie outside `:4712-4761`. Fixed only the report: `lblas` is a local runtime flag, not a switch, and the `MOD` bullet carries its (v).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest can exercise it: its only caller, `PGMRES`, is never
called.
