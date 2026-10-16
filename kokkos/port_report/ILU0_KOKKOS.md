# ILU0_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes the incomplete LU factorisation with no fill-in, ILU(0), of a sparse matrix given in
compressed sparse row form (`a`, `ja`, `ia`), writing the factors in modified sparse row form
(`alu`, `jlu`, `ju`) with the diagonal of `U` stored inverted, the form `lusol` and `lutsol`
read. A SPARSKIT routine kept in `w3profsmd.F90` to build the preconditioner of the implicit N
scheme on unstructured grids.

## What was ported

| | |
|---|---|
| WW3 routine | `ilu0`, `model/src/w3profsmd.F90:4241-4300` at NOAA-EMC/WW3 `03adda91`, 60 lines; no `#ifdef` inside it |
| Callers | `W3XYPFSNIMP` (`w3profsmd.F90:1220`), with `ASPAR` (`REAL*8`), `JAA`, `IAA`, `AU`, `FLJAU`, `FLJU` zeroed just before (`:1214-1218`) and `IWKSP(20*NX)` as `iw`; it ignores `IERROR`. `W3XYPFSNIMP` is selected by `FSNIMP` in `W3XYPUG` (`:257`), called from `W3WAVE` (`w3wavemd.F90:1970-1976`). `pgmres` (`:4332`) carries its own inline copy of the same loop and does not call `ilu0` |
| Switches | `w3profsmd.F90` is compiled with `PR1`, `PR2` or `PR3` (`model/src/cmake/switches.json:117-125`), so by nearly every switch file; no regtest namelist sets `FSNIMP = T` (`git -C WW3-noaa grep -il "FSNIMP *= *T" -- regtests` is empty), so no regtest exercises it |
| Kokkos code | [`../src/ww_kokkos/profs_ilu0.hpp`](../src/ww_kokkos/profs_ilu0.hpp), `ww::profs::ilu0`, `KOKKOS_INLINE_FUNCTION`, `double` as `REAL*8`, arrays 0-based holding the Fortran 1-based indices, `ierr` by reference |
| C shim | none yet |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` under `switch_lab_shrd` (WW3's CMake Release flags,
gfortran 16.2.0), recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake
object, against a throwaway C++ unit with the openmp-release flags and `-ffp-contract=off`,
2026-10-16.

- (v) Same optimised GIMPLE in both: `iw = 0` as one `memset` of `4*n` bytes guarded by `n > 0`;
  `tl = alu(j)*alu(jrow)` with `alu(j)` as the first operand; `alu(jw) - alu(jj)*tl` (the operands
  of the product swapped in the C++, a commutative IEEE multiply, same bits); `ABS_EXPR` of
  `alu(ii)` compared `<` against `1.17549435082228750796873653722224567781866555677208752151e-38`;
  `1.0e+0 / alu(ii)`. The double-precision instruction sequence of the two objects is the same
  (`mulsd`, `mulsd subsd`, `andpd comisd`, `divsd`; SSE in the Fortran, VEX in the C++).
- (v) `tiny(1.)` at `:4285` is the `REAL(4)` tiny, `FLT_MIN` = 2^-126, promoted to double; the
  C++ writes it as `0x1p-126`, and the GIMPLE constant above is that value. `ilut` (`:4222`) uses
  `tiny(1.d0)`, so the two SPARSKIT routines test a zero pivot with different thresholds.
- (v) DO bounds read once on entry (`ia(ii)`, `ia(ii+1)-1`, `ju(jrow)`, `jlu(jrow+1)-1`), as the
  Fortran DO does; the C++ reads them into locals before each loop.
- (v) No FMA, no libm or libmvec call; nothing vectorised (`-fopt-info-vec-optimized` reports
  nothing at `:4241-4300`; the C++ unit's two reports are in Kokkos's `stl_tree.h`).
- ⚠ A row with no diagonal entry leaves `ju(ii)` and `alu(ii)` as the caller left them. The only
  caller zeroes both (`:1214-1216`), so `jm = -1`, no elimination runs and the zero pivot returns
  `ierr = ii`; the caller then ignores `IERROR` and goes on with a partial factorisation. The C++
  reproduces the path; it does not check.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3profsmd.F90` from `build_lab`'s
  `flags.make` (disassembly identical to the CMake object) and a throwaway C++ unit; compared the
  optimised GIMPLE block by block (loads, stores, the hoisted `a(j)` load, DO bounds read once,
  `memset` guard, `tl` product, `alu(jw) - tl*alu(jj)`, the `FLT_MIN` threshold, `1.0/alu(ii)`) and
  the instruction mix (2 `mulsd`, 1 `subsd`, 1 `comisd`, 1 `divsd`, no FMA, no libm call in both);
  no `#ifdef` inside `:4241-4300`; callers and cited lines checked at `03adda91`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. No regtest sets `FSNIMP`, so the WW3 regtests cannot exercise it.
