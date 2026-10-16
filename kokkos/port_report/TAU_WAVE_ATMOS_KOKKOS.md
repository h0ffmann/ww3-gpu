# TAU_WAVE_ATMOS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

`TAU_WAVE_ATMOS(S, CINV, SIG, DSII, TAUNWX, TAUNWY)`, a private subroutine of the ST6 source-term
module `W3SRC6MD`: the stress carried from the waves back to the atmosphere by the negative part
of the wind input (adverse winds),

  tau_nw(x,y) = g rho_w integral from sigma_1 to 2 pi 10 Hz of |S_in^-(sigma)| (cos, sin) / c(sigma) dsigma,

with the resolved spectrum extended to 10 Hz by S_in prop. f^-2 and 1/c = sigma/g
(`w3src6md.F90:1178-1185`, `:1281-1315`). `W3SIN6` calls it only when `SIN6A0 > 0`
(`:522-531`), after `LFACTOR` (`:506`); the `SIN6A0` default is `SINA0 = 0.09`
(`w3gridmd.F90:1759`, `:1858`).

## What was ported

| | |
|---|---|
| WW3 routine | `TAU_WAVE_ATMOS` of `W3SRC6MD`, `model/src/w3src6md.F90:1157-1325` at NOAA-EMC/WW3 `03adda91`; authors S. Zieger, Q. Liu. Compiled with `ST6`: 61 switch files (`git grep -lw ST6 -- '*switch*'`; `ww3_ts1`, `ww3_tp2.15`, `mww3_test_05`, `model/bin/switch_NRL3`, `switch_UoM_*`, `switch_swin`); no NCEP or NWS operational switch. Its only `#ifdef` is `W3_S` (trace output); `FLX4` and `FLX5` give the same machine code for it |
| Kokkos code | [`../src/ww_kokkos/srce_st6_tau_wave_atmos.hpp`](../src/ww_kokkos/srce_st6_tau_wave_atmos.hpp), `ww::srce::st6::tau_wave_atmos(...)`, `KOKKOS_INLINE_FUNCTION`, returns 0, or 3 where the Fortran calls `EXTCDE(3)` (`:1274-1277`); `tau_wave_atmos_nk10hz(sig1, xfr, nk)` gives `NK10Hz` (`:1245-1246`) to size the work arrays. Calls `tauwinds` of [`srce_st6_util.hpp`](../src/ww_kokkos/srce_st6_util.hpp) |
| Structural changes | the 4 `ALLOCATE`s per call (`:1265-1268`) become caller work arrays (`UCINV10Hz` is never used and is dropped); the copies `ECOS2`, `ESIN2` (`:1270-1271`) are read in place from `ECOS(1)`, `ESIN(1)`; the module `SAVE` arrays `IK10Hz`, `SIG10Hz`, `DSII10Hz` and `MK10Hz` (`:103-107`), shared with `LFACTOR`, become tables built on the host and passed `const`; the per-call rewrite of `DSII10Hz` (`:1294-1297`) goes to a fifth work array (below) |
| C shim | none yet; it is called from the `W3SIN6` port |
| Plan | issue #42, branch `goal/42-port-st-flx-swl` |

### The 10 Hz tables the caller builds

Built once on the host, before the first `LFACTOR` or `TAU_WAVE_ATMOS` call of a grid. `W3SIN6`
always calls `LFACTOR` first in the same call on the same thread (`:506`, `:530`; the arrays are
`THREADPRIVATE` under `OMPG`, `:106-108`), so in WW3 `LFACTOR`'s blocks build them (`:951-969`, `:995-1001`,
`:1010-1013`) and `TAU_WAVE_ATMOS`'s own (`:1244-1263`, `:1284-1290`, `:1303-1306`) never run. Both
sets compute the same expressions and the object evaluates them the same way (v) (GIMPLE of
`lfactor` and `tau_wave_atmos`, 2026-10-16), so either is the specification:

1. `NK10Hz = MAX(NK, CEILING(ALOG(10./(SIG(1)/TPI))/ALOG(XFR))+1)` = `tau_wave_atmos_nk10hz(SIG(1), XFR, NK)`;
   glibc `logf` twice, a division by `TPI` (`0x40c90fdb`, not a multiplication by `TPIINV`),
   `INT` by `cvttss2si`, then `i+1` if `REAL(i) >= q` else `i+2`. `MK10Hz = NK10Hz`.
2. `IK10Hz = RRANGE(1,NK10Hz,1)`: `ik10hz[i] = REAL(i+1)`, `rrange()` of `srce_st6_util.hpp` (not
   inlined, a call in the object). `SIZE(IK10Hz) /= NK10Hz` is `EXTCDE(2)` (`:1251-1254`), unreachable
   for `NK10Hz >= 1`; the builder returns 2 there.
3. If `NK < NK10Hz`:
   - `SIG10Hz(i) = SIG(1) * XFR**(IK10Hz(i)-1.0)`: `(ik-1.0f)` then a real power. The loop is
     vectorised: for `NK10Hz >= 4`, elements `0 .. 4*floor(NK10Hz/4)-1` go four at a time
     through libmvec `_ZGVbN4vv_powf(XFR, ik-1)`, the remaining `NK10Hz mod 4` through scalar
     `powf`; for `NK10Hz < 4` all are scalar. This is `ww::st4::mvec_powf`
     ([`st4_libmvec.hpp`](../src/ww_kokkos/st4_libmvec.hpp)) with `x = XFR`, `y = ik-1`, then
     `SIG(1) * r` per element. libmvec and libm `powf` differ by up to a few ULP, so the grouping is
     part of the table.
   - `DSII10Hz(i) = (SIG10Hz(i)*0.5) * (XFR - 1.0/XFR)`; then
     `DSII10Hz(1) = (SIG10Hz(1)*0.5) * (XFR-1.0)` and
     `DSII10Hz(NK10Hz) = ((SIG10Hz(NK10Hz)*0.5) * (XFR-1.0)) / XFR` (a division, last).
4. Else (`NK >= NK10Hz`, so `NK10Hz = NK`): `SIG10Hz = SIG`, `DSII10Hz = DSII`, the caller's
   `SIG(1:NK)`, `DSII(1:NK)` of the first call (`W3SIN6`'s locals, `:413`, set at `:458-459` from
   `SIG2(IKN)`, `DSII2(IKN)`).

## Arithmetic

Reference: the `-O3` object of `w3src6md.F90` (WW3's CMake Release flags, gfortran 16.2,
`switch_lab_shrd` with `ST6 FLX4` and with `ST6 FLX5`, the shared builds `build_st6f4`,
`build_st6f5`), recompiled in scratch and checked identical in its disassembly to each CMake
object; the two objects have the same code. Compared with a throwaway C++ unit compiled with the
`openmp-release` flags (warning-free), GIMPLE and assembly, 2026-10-16.

- (v) `NK10Hz`: the object's `CEILING(x)+1` is `cvttss2si`, `cvtsi2ss`, `ucomiss`, `adc $1`, i.e.
  `i+1` when `REAL(i) >= q` and `i+2` otherwise (NaN included); the C++ uses `ww::st4::fint`
  (`cvttss2si` semantics, `INT_MIN` out of range) and the same test. Both call glibc `logf` on
  `10./(SIG(1)/TPI)` and on `XFR`, then divide. `TPI` is `0x40c90fdb` in both objects.
- (v) `SUM(ABS(MIN(0.,S))*RESHAPE(ECOS2,(/NTH,NK/)),1) * DTH` (`:1282-1283`, `:1308-1309`): the
  object vectorises the `NTH` loop by 4 but adds lane 0, 1, 2, 3 into the scalar sum one at a
  time (`BIT_FIELD_REF`, `addss` chain) from `0.0`, so index order; then `DTH*sum`. The C++ loop
  is scalar in the same order. `RESHAPE` is a `_gfortran_reshape_r4` copy, so element
  `(ith,ik)` is `ECOS(ith+(ik-1)*NTH)`.
- (v) `MIN(0.,S)` is `minps`/`minss` with `S` in the destination in the vector body and the scalar
  tail alike: `S < 0 ? S : 0`, so a NaN or `-0.` gives `+0`; then `andps` (`ABS`). The C++
  `x < 0 ? x : 0` compiles to `vcmpnltss`/`vandnps`, the same selection, then `vandps`.
- (v) `CINV10Hz(NK+1:) = SIG10Hz*0.101978` (`:1293`): one multiplication by `0x3dd0d9d7` in both.
- (v) The `DSII10Hz` rewrite (`:1294-1297`) is `(SIG10Hz*0.5)*c` with `c = XFR - 1.0/XFR`
  computed once, then the two end bins as in the table build; the C++ writes it to `dsii10hz_w`
  with the same operations. Because the bits equal the table's, not writing the shared table
  back changes nothing for later `LFACTOR` calls.
- (v) Extrapolation (`:1300-1301`): `r = SIG10Hz(NK)/SIG10Hz(k)`, `r*r` (the `**2` is one
  multiplication), times `SDENSX10Hz(NK)` loaded before the loop; same in the C++.
- (v) The two `TAUWINDS` calls are inlined 8 times after versioning (stride of `DSII10Hz`, branch);
  each is the in-order sum from `0.0` of `(cinv*sdens)*dsii`, times the folded `9.806e+3`, as
  `tauwinds()` computes it (see [`TAUWINDS_KOKKOS.md`](TAUWINDS_KOKKOS.md)). In the `NK >= NK10Hz`
  branch it reads the table `DSII10Hz`, not the argument `DSII`, as the Fortran does.
- (v) No FMA in either object; no libmvec call in the kernel (the one `_ZGVbN4vv_powf` of
  `tau_wave_atmos` in the object is in its table build; `lfactor` has its own); `logf` twice per call.
- ⚠ CUDA compiled (`sm_89`, `--fmad=false`, pinned `#cuda` shell), never run. The SASS has 125
  `FFMA`, all in the kernel; not traced, presumably CUDA's IEEE division and `logf` sequences.
  `logf` on the device is CUDA's, not glibc's, so `NK10Hz` could differ from the host's at a
  boundary of `CEILING` ⚠; the table build is host-only.
- (v) Fresh review 2026-10-16: no defect in the header. Recompiled `w3src6md.F90` from the
  `build_st6f4` and `build_st6f5` flags (disassembly identical to each CMake object, and the two
  identical to each other) and the throwaway unit (openmp-release flags, warning-free); compared
  GIMPLE and assembly: `NK10Hz` (`divss` by `TPI`, two `logf`, `cvttss2si`, `ucomiss`, `adc $1`,
  `MAX` with `NK`; the C++ `fint` path gives the same integer, NaN and out of range included);
  the 16 `MIN(0.,S)` sites of `:1282-1283`, `:1308-1309` (`minps`/`minss` with `S` in the
  destination and `+0` as source, so NaN and `-0.` give `+0`; the C++ `vcmpnltss`/`vandnps`
  selects the same); the 8 inlined `TAUWINDS` copies (vector(4) bodies and vector(2) epilogues,
  each `(cinv*sdens)*dsii` with lanes added in index order from `0.0`, then `*9806`; the C++
  `tauwinds` vectorises by 8 with the same in-order lane chain); the `DSII10Hz` rewrite, `CINV10Hz`
  and the `F**(-2)` extrapolation operation by operation; no FMA in either object. The table
  description was checked against the GIMPLE of `lfactor` (`:996-1000`) and `tau_wave_atmos`
  (`:1285-1289`): same expressions, same libmvec/scalar `powf` split (vector path when
  `NK10Hz >= 4`, `4*floor(NK10Hz/4)` elements). Fixed in this report only: the `W3SIN6` line cite
  of `SIG`, `DSII` (was `:116`), the `THREADPRIVATE` condition, and which routine's `powf` call is
  meant.

## Found in WW3 itself

- ⚠ `MK10Hz`, `IK10Hz`, `SIG10Hz`, `DSII10Hz` are module `SAVE` arrays, one set per thread, not
  per grid (`:103-107`). Under `ww3_multi` with ST6 grids of different `NK10Hz` the second grid
  stops in `EXTCDE(3)`; with equal `NK10Hz` but a different `SIG(1)` or `XFR`, it uses the first
  grid's 10 Hz frequencies without warning. Not checked against `mww3_test_05`'s grids.
- `:106` reads `#ifdef W3_OMPG || W3_OMPH`; `#ifdef` tests only `W3_OMPG` (gfortran warns about
  the extra tokens). Harmless at `03adda91`: every switch with `OMPH` also has `OMPG` (v)
  (`git grep -lw OMPH -- '*switch*'`, 2026-10-16).
- The first-call `DSII10Hz` block (`:1286-1289`) is overwritten unconditionally three lines
  later (`:1294-1297`); same bits, so harmless. `UCINV10Hz` is allocated and never used. The
  `601`/`602` messages name `IKN` and `TAU_WAV_ATMOS`. None changes a result.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
