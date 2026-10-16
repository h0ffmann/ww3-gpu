# TRIG_COMPUTE_LH_STRESS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The forcing of the wave setup (`w3wavset`, switch `SETUP`) on the unstructured (PDLIB) grid:
the radiation stresses SXX, SXY, SYY divided by DWAT*GRAV, their x and y derivatives through
`DIFFERENTIATE_XYDIR`, and F_X = -dSxx/dx - dSxy/dy, F_Y = -dSyy/dy - dSxy/dx, with the depth
DW copied to each node. Its only caller is `TRIG_WAVE_SETUP_COMPUTATION` (`w3wavset.F90:1702`).

## What was ported

| | |
|---|---|
| WW3 routine | `TRIG_COMPUTE_LH_STRESS`, `model/src/w3wavset.F90:448-584` at NOAA-EMC/WW3 `03adda91`. `w3wavset.F90` is built only with switch `SETUP`; no regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds none) |
| Kokkos code | [`../src/ww_kokkos/wavset_trig_compute_lh_stress.hpp`](../src/ww_kokkos/wavset_trig_compute_lh_stress.hpp), `ww::wavset::trig_compute_lh_stress`, an `inline` host function template |
| Callee | `ww::wavset::differentiate_xydir` ([`DIFFERENTIATE_XYDIR_KOKKOS.md`](DIFFERENTIATE_XYDIR_KOKKOS.md)), called three times (`:561`, `:566`, `:573`) with the same callable that binds `DIFFERENTIATE_XYDIR_MAPSTA`, its module state, its `WEI` work array and its MPI exchange |
| Structural changes | the seven automatic arrays `SXX_p`, `SXY_p`, `SYY_p`, `U_X1`, `U_Y1`, `U_X2`, `U_Y2` (`:526-528`, malloc'd per call in the object) are caller-provided work arrays of `npa` floats |
| Variants | `W3_S` (`STRACE`) and `W3_DEBUGSTP` (`:553-582`, `WRITE`/`FLUSH` only) are output only and not translated. `SXXmethod` is a constant 1 (`:530`) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90`, WW3's CMake Release flags, gfortran 16.2,
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (DIST MPI PDLIB SCOTCH), 2026-10-16. The
recompiled object is identical to the CMake object (v). The C++ unit binds the callable to an
external function and is compiled with the openmp-release flags, warning-free.

- (v) `REAL(rkind)` is `float` (`PDLIB/yowdatapool.F90:48`).
- (v) `DWAT*GRAV` (`constants.F90:62-63`) is folded by gfortran to `9.806e+3` (GIMPLE), one
  `divss` per stress, no reciprocal multiply; the C++ writes the folded value `0x1.327p+13f` with a
  `static_assert` that it equals `1000.0f * 9.806f`, and its GIMPLE shows the same `/ 9.806e+3`.
- (v) `DBLE(eSXX)` stored into a `REAL(rkind)` array (`:547-549`): the float to double to float
  round trip is dropped by gfortran (no `cvtss2sd` in the object); the C++ stores the float.
- (v) `F_X = -U_X1 - U_Y2`, `F_Y = -U_Y1 - U_X2` (`:571`, `:578`): both objects negate the first
  operand (sign `xorps`) and subtract the second from it, element by element, vectorised in both.
  No reduction. The C++ is versioned for aliasing (twice the `divss`/`subps` sites); each path does
  the same per-element operations.
- (v) Call order of the three derivatives and of the two element loops is the Fortran's.
- ⚠ CUDA not compiled: the function is host-only, since its callee does an MPI exchange.
- ⚠ `ISEA = MAPFS(1,IX)` is used without a check (`:541`): a node with `MAPFS(1,IX) = 0` reads
  `DW(0)` (`DW` is allocated `0:NSEA`, `w3adatmd.F90:1004`). The C++ reproduces it (`dw[0]`).
- ⚠ `IP = JSEA` (`:539`) assumes the local node and local sea point numbering coincide; the nodes
  `NSEAL < IP <= npa` keep `DWNX = 0` and zero stresses. Reproduced as written.
- (v) Fresh review 2026-10-16: no defect. Recompiled object (identical to the CMake object)
  against a fresh C++ unit: `9806.0` is `0x46193800` in `.rodata.cst4` of both, one `divss` per
  stress, the three derivative calls with the same arguments in the same order (`SXY_p` to
  `U_X2`/`U_Y2`), `F_X`, `F_Y` as negate then subtract, vectorised element-wise in both, no FMA.
  Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest switch compiles `w3wavset.F90`.
