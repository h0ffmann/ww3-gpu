# IC3TABLE_CHENG_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A set-up routine of the IC3 sea-ice source term (Cheng's viscoelastic ice layer): it fills the
tables `IC3WN_R`, `IC3WN_I`, `IC3CG(0:NK+1, 0:300)` with the real and imaginary wavenumber and the
group velocity of the ice-covered dispersion relation in deep water (`DPT = 999 m`), one column per
ice thickness `h_i = JITK * 0.01 m`, `JITK = 1..300`. Column 0 is open water: `k_r = sigma^2 / g`,
`k_i = 0`. Each column is computed by `IC3PRECALC_CHENG`. Called once, by `W3WAVE`
(`w3wavemd.F90:997-1000`), when `IC3PARS(9) = 1` (Cheng's method) and `CALLEDIC3TABLE = 0`, with the
fixed rheology `IC3PARS(14:16)`.

## What was ported

| | |
|---|---|
| WW3 routine | `IC3TABLE_CHENG`, `model/src/w3sic3md.F90:1962-2044` at NOAA-EMC/WW3 `03adda91`, with the parameters `IC3_DITK = 0.01`, `IC3_MAXITK = 3.` (`:89`); compiled under switch `IC3`, by 46 regtest switch files (`git grep -lw IC3 -- '*switch*'`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`). Exercised only where Cheng's method is selected: `ww3_tic1.1` (`input_IC3NL`, `*_CHENG`), `ww3_tic1.2/input_IC3_CHENG`, `ww3_tic1.3/input_IC3_CHENG`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_ic3table_cheng.hpp`](../src/ww_kokkos/ic3_ic3table_cheng.hpp), `ww::ic3::ic3table_cheng`, an `inline` host function (a table built once) |
| Callee | `IC3PRECALC_CHENG` (`:2047-2160`) has no header yet: the port takes it as a host callable `(wn_r, wn_i, cg, ice1, ice2, ice3, ice4, dpt)` with each array `0:NK+1` and the scalars by value (all `INTENT(IN)` in the Fortran, `:2116`). It allocates `SIGMA`, so it runs on the host |
| C shim | none yet; its caller is not ported |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly read
against those of a throwaway C++ unit compiled with the `openmp-release` flags and
`-ffp-contract=off`, the callee an external function, 2026-10-16.

- (v) `ITKNUM = CEILING(IC3_MAXITK/IC3_DITK)` (`:2034`) is folded to 300 in the GIMPLE
  (`# DEBUG itknum => 300`); the header writes the constant `kIc3Itknum = 300`. It equals the
  second extent of the tables (`0:300`, `w3adatmd.F90:1316-1330`).
- (v) `IC3WN_R(:,0) = SIG**2/GRAV` (`:2035`): `SIG*SIG` (`mulps`/`mulss`, no `powf`) then a true
  division by `9.806f` (`0x411ce560`; `divps`/`divss`, no reciprocal) in both objects. The Fortran
  goes through a temporary, because `SIG` and `IC3WN_R` are both pointers; a copy, no arithmetic.
  Both vectorise it element-wise, so the bits do not depend on the width.
- (v) `ICE1 = JITK*IC3_DITK` (`:2038`): `cvtsi2ss` of `JITK` then `mulss` by `0.01f`
  (`0x3c23d70a`) in both objects.
- (v) `DPT = 999.` is `999.0f`, read by the callee as `DBLE(DPT) = 999.0` exactly.
- (v) Each column `IC3WN_R(:,JITK)` of a pointer array goes through `_gfortran_internal_pack` /
  `_internal_unpack` in the Fortran object; the port passes the contiguous column. A copy, no
  arithmetic.
- (v) No libm or libmvec call, no reduction, no `MIN`/`MAX` in the routine's own code; the C++ unit
  compiles warning-free.
- ⚠ gfortran inlines `IC3PRECALC_CHENG` into this routine in the object (the calls left are
  `WAVNU1`, `WN_PRECALC_CHENG`, `SMOOTH_K`, `CGINIC3_CHENG`); the arithmetic of that callee is not
  covered here and belongs to its own port.
- ⚠ `IC3CG` is allocated only when `FL_ALL` (`w3adatmd.F90:1325-1331`), while `:2039-2040` writes
  `IC3CG(:,JITK)` unconditionally. The port takes `ic3cg` as given; a caller with a partial
  initialisation would write through an unallocated pointer in the Fortran.
- ⚠ `IC3CG(:,0)` is never written. The Fortran's own comment (`:2007-2010`, "UNRESOLVED BUG")
  says the table is not rebuilt when the rheology changes. Both reproduced as written.
- ⚠ CUDA not compiled: a host routine.
- (v) Fresh review 2026-10-16: no defect. `w3sic3md.F90` recompiled from `build_mixa`'s
  `flags.make` (disassembly identical to the CMake object) and the C++ unit recompiled with the
  `openmp-release` flags; read side by side: `SIG*SIG / 9.806f` element-wise in the vector bodies
  and scalar tails of both (no `powf`, no reciprocal), `IC3WN_I(:,0)` zeroed (`memset` in C++,
  `+0.0` stores in Fortran), `(float)JITK * 0x3c23d70a` for `JITK = 1..300`, `DPT = 999.0f`; the
  callee's dummies (`:2115-2116`) match the callable's order. The Fortran sizes `:2035` from
  `SIG`'s extent and the port from `nk + 2`; equal while `SIG` is `0:NK+1`. ⚠ `IC3CHENG` defaults
  to `.TRUE.` (`w3gridmd.F90:2929`), so `ww3_ts1/input_icesct_IC3`, which does not set it, also
  selects Cheng's method; with the default rheology `IC3VISC = -2.0` (`w3gridmd.F90:2934`) `W3WAVE`
  stops at `EXTCDE(2)` (`w3wavemd.F90:991-996`) before this routine, read from the source, not run.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
