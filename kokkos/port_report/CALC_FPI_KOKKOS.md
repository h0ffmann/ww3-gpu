# CALC_FPI_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Equivalent peak frequency from the wind input source term (Tolman and Chalikov 1996). With
`S` the 1-D wind input, `SIN1A(k) = sum over directions of MAX(0, S)`, weighted by
`DDEN(k) / (CG(k) * SIG(k)**3)`; `M0` and `M1` are its sums with weights 1 and `1/SIG`, plus a
tail term with `FTE` and `FTTR`; `FPI = M0/M1`, or `XFR*SIG(NK)` when `M1 < 1E-20`
(`w3srcemd.F90:2521-2543`).

## What was ported

| | |
|---|---|
| WW3 routine | `CALC_FPI`, `model/src/w3srcemd.F90:2428-2545` at NOAA-EMC/WW3 `03adda91`, 118 lines; author J. Meixner. Its only `#ifdef` block is `W3_S` (trace) |
| Callers | `W3SRCE`, `w3srcemd.F90:2279`, under `#if defined(W3_FLD1) \|\| defined(W3_FLD2)`. Switches that compile the call: `regtests/ww3_ts3/input/switch_ST4_PR1_FLD1`, `_FLD1_MPI`, `_FLD2`, `_FLD2_MPI`, `switch_ST6_PR1_FLD1`, `_FLD1_MPI`, `_FLD2`, `_FLD2_MPI`; `regtests/ww3_ufs1.1/input/switch_MPI`, `_MPI_OMPH`, `ww3_ufs1.1/input_unstr/switch_PDLIB`, `_PDLIB_TRNK`, `ww3_ufs1.2/input/switch_MPI_OMPH`, `ww3_ufs1.3/input/switch_MPI_OMPH` (FLD2); `model/bin/switch_multi_esmf` (FLD2). No `switch_NCEP_*` or `switch_NWS_*` has FLD1 or FLD2. Every switch compiles the routine itself (it is in `w3srcemd`) |
| Kokkos code | [`../src/ww_kokkos/srce_calc_fpi.hpp`](../src/ww_kokkos/srce_calc_fpi.hpp), `ww::srce::calc_fpi(a, cg, fpi, s, nk, nth, xfr, sig, dden, fte, fttr, sin1a)`, arrays 0-based, `sig` and `dden` from `SIG(1)` and `DDEN(1)`; the automatic array `SIN1A(NK)` (a `malloc` in the object) is the caller-provided work array `sin1a` |
| C shim | none |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: `w3srcemd.F90.o` of WW3's CMake Release build under `switch_lab_shrd` (gfortran
16.2.0, pinned shell); the module recompiled with `-fdump-tree-optimized-lineno` gives an object
identical to the CMake one (v). The disassembly of `__w3srcemd_MOD_calc_fpi` is identical in the
`build_lab`, `build_mixa` (FLD2) and `build_fld1` (`switch_ST4_PR1_FLD1`) objects (v). The C++
unit compiled with the `openmp-release` flags, warning-free (v), 2026-10-16.

- (v) No libm call; `SIG(IK)**3` is `(SIG*SIG)*SIG` in both objects, then multiplied by `CG`.
- (v) `MAX(0., S(IS))` (`:2524`) is `maxss` with `S(IS)` as destination (`S > 0 ? S : 0`, so
  NaN and `-0.` give `+0.`); the C++ ternary compiles to `vmaxss` with `S` as first source.
- (v) Reductions: the direction sum (`:2523-2525`) is scalar in both. The `M0`/`M1` loop
  (`:2530-2534`) is vectorised in both objects (gfortran 4-wide plus a 2-wide epilogue, g++ 8-wide
  and 4-wide) with the lanes added to the accumulators one by one in index order, so the sums keep
  the Fortran's order. Operand roles of every multiply, divide and add in the loops match
  (accumulator first; `DDEN` first in `DDEN*SIN1A`; `SIG**3` first in the product with `CG`).
- (v) `1E-20` is the same single-precision bits (`0x1e3ce508`) in both objects; `M1 < 1E-20`
  is `comiss`/`jbe` in both, so a NaN `M1` takes the `M0/M1` branch.
- ⚠ Two tail products have their operands in the other order: `SIN1A(NK)*FTE` (`:2537`;
  gfortran keeps `SIN1A` as destination, g++ puts `FTE` first) and `XFR*SIG(NK)` (`:2540`;
  gfortran `SIG` first, g++ `XFR` first). g++ canonicalises the commuted source spelling, so the
  order cannot be steered from C++. The results differ only in the NaN payload when both
  operands are NaN, which needs a NaN grid constant `FTE` or `XFR`.
- (v) No arithmetic `#ifdef` variant: the body does not depend on the switch.
- ⚠ `SIN1A(NK)` and `DDEN(NK)` are read with `NK < 1` (index 0, outside the arrays) in WW3
  itself; the C++ reproduces the read at `[-1]`. WW3 grids always have `NK >= 1`.
- ⚠ Not compiled for CUDA.
- (v) Fresh review 2026-10-16: no arithmetic defect. Recompiled `w3srcemd` from the `build_lab`,
  `build_mixa` and `build_fld1` flags (each identical to its CMake object; `CALC_FPI` identical in all
  three) and compared it with the `openmp-release` C++ object instruction by instruction: `maxss`
  operand roles, `(SIG*SIG)*SIG`, `DDEN*SIN1A`, lane-by-lane `M0`/`M1` accumulation in index order,
  tail adds (product first), `1E-20` bits and the NaN branch all match; only the two NaN-payload
  operand swaps above differ. Fixed: Fortran line cites (off by three) in the header and this
  report, and the switch list, which missed `ww3_ufs1.1` `switch_MPI_OMPH`, `switch_PDLIB` and
  `switch_PDLIB_TRNK` (FLD2).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. Only an FLD1 or FLD2 build calls it (`ww3_ts3`, `ww3_ufs1.*`).
