# W3SPR3_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Mean wave parameters of the WAM4+ source terms (switch `ST3`): the spectral energy `EMEAN`, the
mean frequencies `FMEAN` (weight `SIG**(2*WWNMEANPTAIL)`), `FMEANS` (weight `SIG**(2*WWNMEANP)`)
and `FMEANWS` (wind sea only, from `LLWS`), the mean wavenumber `WNMEAN`, the largest action
density `AMAX`, then the drag coefficient `CD`, `USTAR`, `Z0` and `CHARN` through `CALC_USTAR`.
`W3SRCE` calls it per sea point (`w3srcemd.F90:1052`, `:1056`, `:1063`, `:1873`); the point
post-processors call it too (`ww3_outp.F90:1966`, `:2016`, `:2143`, `:2189`, `ww3_ounp.F90:2124`,
`:2174`, `:2304`, `:2352`, `gx_outp.F90:1072`, `:1121`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3SPR3`, `model/src/w3src3md.F90:134-347` at NOAA-EMC/WW3 `03adda91`; compiled by every `ST3` switch: 47 regtest switch files (`mww3_test_05` 20, `ww3_ts3` 20, `ww3_ts2` 5, `ww3_ts1`, `ww3_tnc1`; `git grep -lw ST3 -- '*switch*'`), by no `switch_NCEP_*`/`switch_NWS_*` in `model/bin`. Its only `#ifdef` blocks are `W3_S`/`W3_T` output; `STAB3` changes `W3SIN3` only (`:513-565`) |
| Kokkos code | [`../src/ww_kokkos/srce_st3_spr3.hpp`](../src/ww_kokkos/srce_st3_spr3.hpp), `ww::srce::st3::w3spr3_point`; the automatic arrays `EB`, `EB2` (`:238`, a `malloc` each per call in the object) and the three powers of section 2 are a caller-provided work array of `spr3_work_reals(p) = 5*NK` reals. It calls `calc_ustar` of [`srce_st3_calc_ustar.hpp`](../src/ww_kokkos/srce_st3_calc_ustar.hpp) ([`W3SRC3MD_CALC_USTAR_KOKKOS.md`](W3SRC3MD_CALC_USTAR_KOKKOS.md)) |
| Not ported | `ALFA(NK)` (`:273`): a local array computed and never read; the object still computes and stores it, with no effect on any output |
| C shim | none yet; it passes `SIG(1:NK)`, `DDEN(1:NK)`, `WWNMEANP`, `WWNMEANPTAIL`, `FTE`, `SSTXFTF`, `SSTXFTWN`, `SSTXFTFTAIL` of the current grid, `LLWS` as `LOGICAL(4)`, and `TAUT`, `DELTAUW`, `DELU`, `ZZWND`, `AALPHA` as `calc_ustar` takes them |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

## Arithmetic

Reference: the `-O3` objects of `w3src3md.F90` under the reference builds `build_st3` (ST3 STAB0
FLX0) and `build_st3s` (ST3 STAB3 FLX0), WW3's CMake Release flags, gfortran 16.2, recompiled in
the pinned shell and checked identical to the CMake objects; C++ from a throwaway unit with the
`openmp-release` flags (g++ 16.2, `-ffp-contract=off`, Kokkos 5.2.0), both read operation by
operation in the optimised GIMPLE and the disassembly, 2026-10-16.

- (v) The disassembly of `w3spr3` is the same under STAB0 and STAB3: one variant.
- (v) `CALC_USTAR` is not inlined into `W3SPR3` in the Fortran object (`call
  __w3src3md_MOD_calc_ustar`, `TAUW` passed through the stack); g++ inlines `calc_ustar`, whose
  arithmetic the C++ object keeps as its own port report records. `Z0 = 0.` (`:324`) is removed
  by `-O3`: `Z0` is `INTENT(OUT)` there and written on every path. The first `UNZ` (`:246`) is
  dead.
- (v) Section 1 (`:259-268`) is scalar in the object: `EB` and `EB2` summed in `ITH` order, `EB2`
  only where `LLWS(IS) /= 0`; `AMAX = MAX(AMAX, A)` is `maxss A,AMAX` (destination `AMAX`: a NaN
  `A` or a tie gives `A`); the C++ gives `vmaxss` with the same destination.
- (v) Section 2 (`:272-282`) is vectorised by four from `IK = 1` (`-fopt-info-vec`: `:272`,
  versioned for aliasing). The three powers call libmvec's `_ZGVbN4vv_powf` per group of four and
  scalar `powf` for the last `NK mod 4`; with `NK < 4` the whole loop is scalar. The C++ computes
  them with `ww::st4::mvec_powf` (`st4_libmvec.hpp`), whose object calls the same
  `_ZGVbN4vv_powf` per group of four from the first element and `powf` for the tail. `:281` reuses
  the power of `:277` (CSE in the object); the C++ reuses the same array. The six sums are added
  lane by lane in `IK` order in both objects, so the vectorised C++ loop and the scalar order give
  the same bits. `EB*DDEN/CG` is a product then a quotient in both.
- ⚠ The vectorised path of the Fortran also needs its runtime alias check to pass (the `malloc`ed
  `EB`, `EB2`, `ALFA` against `DDEN`); separate allocations never overlap, so the port always takes
  the libmvec grouping when `NK >= 4`. Argued from the object, not run.
- (v) Section 3 (`:287-294`): `EBAND = EB(NK)/DDEN(NK)`, each sum plus `EBAND*FT`, as written.
- (v) Section 4 (`:299-318`): `TPIINV` and `1.E-7` are folded (`0x3e22f983`, `0x33d6bf95`), hex
  literals with a `static_assert` of how they are built. `MAX(1.E-7, EMEAN)` and `MAX(1.E-7,
  WNMEAN)` are `maxss 1.E-7,x` (a NaN gives `1.E-7`), as the C++. In each `ELSE` branch `-O3`
  drops the `MAX(1.E-7, ·)` of the variable the `IF` just tested (`FMEAN`, `FMEANS`, and both
  `FMEANWS` and `EMEANWS` at `:317`): a NaN reaches `powf` unclamped. The C++ writes the object's
  form, not the source's. The exponents are `1.0/(WWNMEANPTAIL*2.)` and `1.0/WWNMEANP`; four
  scalar `powf` calls in both objects. `EMEANWS` (`:293`) is computed only when `FMEANWS >= 1.E-7`
  in the object; the C++ computes it before the test, with the same value.
- (v) Section 5 (`:322-328`): `TAUWX**2` is `x*x`, `sqrtss`; `USTAR = MAX(0.0001, USTAR)` (`:247`)
  and `UNZ = MAX(0.01, U)` (`:326`) are `maxss` with the variable as destination; `(USTAR/UNZ)**2`
  is one division squared. The C++ adds `TAUWY**2 + TAUWX**2` in the other operand order: a
  single correctly rounded addition, the same bits.
- (v) No FMA in either host object; no libm or libmvec call other than the `powf` and
  `_ZGVbN4vv_powf` above and `expf` in the inlined `calc_ustar`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, Kokkos 5.2.2 CUDA), never run. On a device there is no
  libmvec: `mvec_powf` falls back to scalar `pow` for every `IK`, and CUDA's `powf`, `expf` and
  `sqrtf` are not glibc's, so a device build cannot be bit for bit at these sites. The SASS holds
  `FFMA` inside the IEEE division, square root and `powf`/`expf` sequences; attributed by count,
  not read line by line.
- (v) Fresh review 2026-10-16: no defect. Recompiled `w3src3md.F90` under `build_st3` and `build_st3s` (identical to the CMake objects; `w3spr3` differs between them only in alignment padding) and the C++ unit, and compared GIMPLE and disassembly: the vectorised path of `:272` is taken when `NK-1 > 2` (unsigned) and the alias check of the three `malloc`ed arrays against `DDEN` passes, groups of four from `IK = 1` with no peeling, in-order lane sums, three `_ZGVbN4vv_powf` and a scalar `powf` tail in both objects (16 `powf` sites in the Fortran, counting the alias fallback and section 4; 7 in the C++); every `maxss` has the variable as destination in both; the `ELSE` branches of `:303`, `:309`, `:317` divide the unclamped value in the object, as the C++ does. `ALFA` (`:273`) has no observable effect: stored to a `malloc`ed block freed on return, no `-ffpe-trap` in the Release flags, no caller reads an exception flag; dropping it is the same kind of change as the dead `Z0 = 0.` and first `UNZ`, so it stays omitted. ⚠ Only under the Debug flags (`-ffpe-trap=invalid,zero,overflow`, `model/src/CMakeLists.txt:72`) could an overflow in `ALFA` abort a run, which the port would not reproduce. ⚠ Where a product is commuted (`DDEN*EB` in the Fortran's vector body), two NaN operands can give a different NaN payload; NaN inputs only.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
