# INPTAB_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) runs it inside the Kokkos `W3KOKKOS_ST2_SETUP`, behind `WW_KOKKOS_SPR2, SIN2 or SDS2` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

The table of the Chalikov and Belevich (1993) wind-wave interaction parameter that the
Tolman and Chalikov (1996) input term `W3SIN2` (switch `ST2`) interpolates bilinearly in the
non-dimensional apparent frequency and the drag coefficient (`BETATB`, `DSIGA`, `DDRAG`), built
once per process: a host function.

## What was ported

| | |
|---|---|
| WW3 routine | `INPTAB`, `model/src/w3src2md.F90:847-1228` at NOAA-EMC/WW3 `03adda91` (its internal function `W3BETA` :1081-1224 has its own report, [`W3BETA_KOKKOS.md`](W3BETA_KOKKOS.md)); the table and its sizes `NRSIGA`, `NRDRAG`, `SIGAMX`, `DRAGMX` at :93-99. Called by `W3IOGR` once per process, on the first mod_def read (`FLINP` is `SAVE`, `w3iogrmd.F90:355,1493`). `ST2` is in 83 regtest switches (`mww3_test_05`, `ww3_ta1`, `ww3_ts1`, `ww3_ts2`, `ww3_ts3`; `git grep -lw ST2 -- '*switch*'`) and in `model/bin/switch_NCEP_st2` |
| Kokkos code | [`../src/ww_kokkos/srce_st2_inptab.hpp`](../src/ww_kokkos/srce_st2_inptab.hpp), `ww::srce::st2::inptab(betatb, dsiga, ddrag)`; `betatb` is caller-provided, `kBetatbSize` = 802 x 21 floats in the Fortran's column-major order, `BETATB(isiga, idrag)` at `(isiga + 400) + 802*(idrag - 1)`, so `W3SIN2` (`srce_st2_sin2.hpp`) reads it unchanged |
| Dropped | the `W3_T` write of `DSIGA`, `DDRAG` (:960-962), the `W3_T0` printed table (:977-994) and the `W3_T1` interpolation-error estimate (:996-1029): output only, and the error estimate writes nothing back. `W3_S` is tracing |
| C shim | none yet; it calls `inptab` once per process and hands the table and the two scalars to every `W3SIN2` call |
| Plan | issue #42 (source terms, fluxes and water levels of the NOAA switches) |

## Arithmetic

Reference: the `-O3` object of `w3src2md.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd` with `ST2 STAB2 FLX3`, the physics of `switch_NCEP_st2`),
recompiled with `-fdump-tree-optimized-lineno` and checked identical to the CMake object
(`objdump -d`), against a throwaway C++ unit compiled with the `openmp-release` flags
(g++ 16.2.0, `-O3 -march=x86-64-v3 -ffp-contract=off`), 2026-10-16.

- (v) `DSIGA = 40/400` and `DDRAG = 0.01/20` folded by both compilers to the same floats
  (0x1.99999ap-4, 4.99999965541e-4).
- (v) The two GIMPLE dumps match operation by operation: `W3BETA` inlined in both; both unswitch
  the `OMA < -1` branch (constant over `IDRAG`) and vectorise it over `IDRAG` with `A1` and `A2`
  folded per `IDRAG` to the same 21 constant pairs (all floating-point constants of the two
  functions identical, apart from one in a Fortran debug statement); the other four branches
  scalar, the same operations in the same order. The vector width differs (4 lanes in the Fortran
  object, 8 + 4 in the C++), which changes no bit: the loop is element by element, with no
  reduction.
- (v) No FMA in either object (no `vfmadd` / `vfmsub`), no libm or libmvec call, no `MIN`, `MAX`,
  `INT` or reduction.
- (v) No `#ifdef` changes the arithmetic: the routine's only blocks are `W3_S`, `W3_T`, `W3_T0`,
  `W3_T1`; `STAB2` and the `FLX` choice do not reach it.
- ⚠ CUDA compiled (`nix develop picasso/labs/pratico#cuda`, `nvcc_wrapper`, `--fmad=false`,
  `sm_89`): the functions are host-only, so no device code was produced; never run.
- (v) Fresh review 2026-10-16: no defect. `w3src2md.F90` recompiled from `build_st2`'s `flags.make` (gfortran 16.2.0) and checked identical to the CMake object; its `inptab` GIMPLE compared operation by operation with a fresh C++ unit of `inptab` (g++ 16.2.0, `openmp-release` flags): same folded `DSIGA`/`DDRAG`, same 21 folded `A1`/`A2` pairs and `1.E-4` in the unswitched `OMA < -1` branch, same operand order on the four scalar branches, no FMA in either object. The layout `kBetatbN1` = 802, `(isiga + 400) + 802*(idrag - 1)` matches `BETATB(-400:401, 21)` (:99) and the indices `W3SIN2` reads, `IOMA` in [-400, 400] and `ICL` in [1, 20] plus one (:469-474); `srce_st2_sin2.hpp` restates `kNrsiga` and `kBetatbN1` with the same values instead of including this header. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
