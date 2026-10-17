# W3SPR6_KOKKOS

**Status:** ported, phase 1 (translated). Wired: WW3 at the `WW3/` pin (`357f575c`) calls it at `w3srcemd.F90:1290, :2275` behind `WW_KOKKOS_SPR6` (default off); not regtested: compiled, never run inside WW3 (the owner, 2026-10-16).

Mean wave parameters for the ST6 source terms (switch `ST6`): energy, mean frequency, mean
wavenumber, maximum action density and a peak frequency weighted by `F**4` (Young 1999, p. 239).
`W3SRCE` calls it per sea point (`w3srcemd.F90:1129`, `:1887`); the point post-processors
`ww3_outp`, `ww3_ounp` and `gx_outp` call it too.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SPR6`, `model/src/w3src6md.F90:129-264` at NOAA-EMC/WW3 `03adda91`; compiled by every switch with `ST6` (`git grep -lw ST6 -- '*switch*'`): `ww3_ts1` (`input`, `input_capcha`, `input_fld`, `input_nl5_*`), `ww3_ts2`, `ww3_ts3`, `ww3_tp2.15`, `mww3_test_05`, and `model/bin/switch_NRL3`, `switch_swin`, `switch_UoM_*`; by no `switch_NCEP_*` or `switch_NWS_*`. Its only `#ifdef` block is `W3_S` (subroutine tracing), not ported |
| Kokkos code | [`../src/ww_kokkos/srce_st6_spr6.hpp`](../src/ww_kokkos/srce_st6_spr6.hpp), `ww::srce::st6::w3spr6_point`; the automatic array `EB(NK)` (`:217`) becomes caller work `eb` |
| C shim | none yet; it passes `SIG(1:NK)`, `DDEN`, `DSII`, `DTH`, `FTE`, `FTF`, `FTWN` of the current grid |
| Plan | issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2) from the
reference builds `build_st6f4` (`ST6 FLX4`) and `build_st6f5` (`ST6 FLX5`), each recompiled with
`-fdump-tree-optimized-lineno` and checked identical to the CMake object; the disassembly of
`__w3src6md_MOD_w3spr6` is the same in both builds. Compared with the `-O3` GIMPLE and assembly of a
throwaway C++ unit (`openmp-release` flags, `-ffp-contract=off`), operation by operation,
2026-10-16.

- (v) Every `SUM` (`SUM(A,1)` at `:229` and `:255`, `SUM(EB)`, `SUM(EB/SIG)`, `SUM(EB/SQRT(WN))`,
  both sums of `:256`) adds its terms one by one from `0.0` in index order. gfortran vectorised the
  loops at `:229`, `:233-235`, `:255-256`, but as in-order reductions: each vector is split into
  lanes added in sequence (`BIT_FIELD_REF` chains in the GIMPLE). The C++ object does the same.
- (v) `SUM(A,1)` is computed twice, at `:229` and again at `:255`, as in the Fortran; no reuse.
- (v) Operation order per term, read from the GIMPLE: `(S*DDEN)/CG`, `EB/SIG`, `EB/sqrtf(WN)`,
  `((S*SIG)/CG)*DTH`, `(SIG*EB**4)*DSII`, `EB**4*DSII`; `EB**4` is `(EB*EB)*(EB*EB)` (two
  `powmult`); `(EMEAN/MAX(...))**2` is one product of the quotient; `FMEAN = (TPIINV*EMEAN)/MAX`;
  `FP = (num/MAX(1E-10,den))*TPIINV`.
- (v) `MAX(1.0E-7,X)` at `:245`, `:246` and `MAX(1E-10,X)` at `:256` are `maxss` with `X` the
  destination: `X > c ? X : c` (`spr6_detail::maxss`), so a NaN `X` gives the constant.
- (v) `MAXVAL(A)` (`:230`) as gfortran expands it: from `-Inf`, a first loop skips elements until
  `limit <= a` (`comiss`/`jb`), then `limit = a > limit ? a : limit` (`maxss`); all NaN gives NaN
  `0x7fc00000`, an empty `A` gives `-HUGE` `0xff7fffff` (`spr6_detail::maxval_a`).
- (v) No libm or libmvec call in either object: `SQRT` is `sqrtss`/`sqrtps` (Fortran) and
  `sqrtss` plus the `sqrtf` errno call for a negative argument (C++), the same bits. No FMA.
- (v) Constants in `.rodata.cst4` of both objects: `TPIINV` `0x3e22f983` (folded, a hex literal
  with a `static_assert` against `srce_params.hpp`), `1.0E-7` `0x33d6bf95`, `1E-10` `0x2edbe6ff`,
  `HSMIN` `0.05` `0x3d4ccccd`, `4.0`.
- ⚠ `EBAND = EB(NK)/DDEN(NK)` (`:239`) reads `EB(0)` when `NK <= 0` in the Fortran (undefined), and
  the C++ reads `eb[-1]`; no grid has `NK <= 0`.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, the `#cuda` shell), never run. The `FFMA` in the SASS
  belong to nvcc's IEEE division and square-root sequences (`MUFU.RCP`/`MUFU.RSQ` with Newton
  steps), not checked one by one. The three `MAX` sites and `MAXVAL` may compile to `FMNMX`, which
  differs from `maxss` only for a tie of `+0` and `-0` in `MAXVAL`; action densities are not
  negative zero in practice, not checked.
- (v) Fresh review 2026-10-16: no defect. `w3src6md.F90` recompiled from the `build_st6f4` and
  `build_st6f5` flags, both identical to the CMake objects, and `__w3src6md_MOD_w3spr6` the same in
  both; its GIMPLE and assembly read against a fresh `-O3` C++ unit: every sum in order from `+0.0`
  (lane-by-lane `BIT_FIELD_REF` chains on both sides), the product order of `:229`, `:255`, `:256`
  (commuted operands only), `EB**4` as two `powmult`, the `maxss` operand of `:245`, `:246`, `:256`
  (`X` kept unless NaN or not greater) and of `MAXVAL` (`a > limit ? a : limit` after a
  `limit <= a` entry loop), the `.rodata` constants, no FMA, no libm or libmvec call. The only
  `#ifdef` in `:129-264` is `W3_S`. Nothing changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
