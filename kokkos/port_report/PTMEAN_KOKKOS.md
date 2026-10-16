# PTMEAN_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The integrated parameters of each spectral partition, the last step of `W3PART`'s watershed
partitioning: for the whole spectrum (column 0) and each partition, `XP(1:15)` holds Hs, peak
period, peak wavelength, mean direction, directional spread, wind-sea fraction, peak direction,
spectral width, the peak enhancement ratio, Goda's peakedness, a Gaussian frequency width, the
energy, mean and zero-crossing periods and the peak 1-D density (`w3partmd.F90:1492-1525`).
`W3PART` calls it once per spectrum under every `PTMETH` (`w3partmd.F90:303`, `:333`, `:368`,
`:397`, `:452`); `W3PART` itself is called by `W3IOSF` (`w3iosfmd.F90:278`), `ww3_outp`
(`ww3_outp.F90:1910`) and `ww3_ounp` (`ww3_ounp.F90:2067`).

## What was ported

| | |
|---|---|
| WW3 routine | `PTMEAN`, `model/src/w3partmd.F90:1175-1538` at NOAA-EMC/WW3 `03adda91`; its only `#ifdef` is `W3_S` (not ported). Reference switch `switch_NCEP_st4` (GFS-Wave) |
| Kokkos code | [`../src/ww_kokkos/part_ptmean.hpp`](../src/ww_kokkos/part_ptmean.hpp), `ww::part::ptmean`; it calls `ww::prop::wavnu1` ([`prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp)), not a new translation of `WAVNU1` |
| C shim | none yet; see "What the shim must do" |
| Translated before | none (checked kokkos/ports.tsv, kokkos/src, remote branches, PRs, 2026-10-16) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## What the shim must do

- Pass `W3GDATMD`'s `NK`, `NTH`, `DTH`, `XFR`, `FACHFE`, `FTE`, `SIG(0:NK+1)`, `DSIP(0:NK+1)`,
  `DSII`, `TH`, `ECOS`, `ESIN` of the current grid, `W3ODATMD`'s `HSPMIN`, `WSMULT`, `PTMETH` of the
  current output set (`w3odatmd.F90:1837`), and `UNDEF`, which is a variable, not a `PARAMETER`
  (`constants.F90:85`), so it is read at call time; and the `DISTAB` table for `wavnu1`.
- Give each spectrum its own scratch: `ptmean_nreal(nk, nth, npi)` Reals and `npi + 1` ints, carved
  by `ptmean_carve`. The Fortran's automatic arrays are `malloc`ed per call (`w3partmd.F90:1250-1261`).
- Act on the return code. `kPtmeanXpFull`: write format 1000 (`:1533-1534`) with `NPO+1` on `NDSE`
  when `IAPROC == NAPERR`, as `:1416` does, and return (the Fortran does not stop).
  `kPtmeanNoPeak`: call the Fortran `PTMEAN` for that spectrum (see the defect below).

## What WW3 does here at the pin

- (v) Defect: out-of-bounds reads when no frequency has `SUMF > 0`. `IFPMAX(IP)` starts at 0 and is
  set only when `SUMF(IK,IP) > EFPMAX(IP)` (`w3partmd.F90:1355-1358`). If it stays 0 and the
  partition passes `HS < HSPMIN` (`:1403`), the Fortran reads `SUMF(-1,IP)` (`:1436`, below the
  array for `IP = 0`), `SUMFX(0,IP)`, `SUMFY(0,IP)` and `DSII(0)` (`:1489-1490`, `DSII` is
  allocated `(MK)`, `w3gdatmd.F90:2069`). It cannot happen for `HSPMIN > 0` and a finite spectrum
  (then `M0 <= 0` and `HS = 0`). It can for `HSPMIN <= 0` or a NaN spectrum (`HS` is NaN, the
  compare is false); `ww3_grid` writes `HSPMIN = MAX(0.0001, HSPM)` (`w3gridmd.F90:3141`), so in
  WW3 only the NaN spectrum reaches it. The C++ returns `kPtmeanNoPeak` at that point, and only
  there, instead of reading out of bounds.
- ⚠ The bytes that out-of-bounds read returns were not traced: for `IP >= 1` the `-O3` object reads
  `SUMF(NK+1,IP-1)` and `SUMFX(NK,IP-1)` through the column-major layout, and for `IP = 0` and
  `DSII(0)` the 4 bytes before a `malloc`ed block. The result may be repeatable for one build,
  but the standard does not define it; the shim's call to the Fortran keeps its bits either way.
- (v) The `PTMETH` 4 and 5 branch (`:1407-1411`) increments `NPO` and writes `XP(:,NPO)` without
  the `DIMXP` check of `:1415`; it relies on `DIMXP = 2` and `NPI = 2` under those methods
  (`:117`). Translated as written.
- (v) When the total spectrum (`IP = 0`) is below `HSPMIN` under `PTMETH` 1 to 3, `NPO` is 0 at
  `IP = 1`, which then `CYCLE`s (`:1421`): partition 1 is not reported and `XP(:,0)` stays 0.
- ⚠ The "weighted least-squares" sums (`:1471-1487`) accumulate `SUMXY` as `XHAT*YHAT` and
  `SUMXYLOGY` as the running `SUMXY` times `YHAT`, not the sums the comment at `:1458-1463`
  describes. Not analysed; translated as written.

## Arithmetic

Reference: the `-O3` object of `w3partmd.F90` as WW3's CMake Release build compiles it under
`switch_NCEP_st4` (gfortran 16.2, the `pratico#ww3` shell), recompiled with
`-fdump-tree-optimized-lineno` and checked identical to the CMake object, 2026-10-16. Scripts
in the translator's scratch: `cfg.sh` (configure, build to `w3partmd.F90.o`), `dump.sh`, `tu.sh`
(g++ 16.2 with the `openmp-release` flags and `-ffp-contract=off`, warnings clean), `cuda.sh`.

- (v) Every sum in the Fortran's loop order: section 3.a per `IK` then `ITH`, both into column 0
  and column `IP`; section 3.b per `IP` then `IK`, then the tail with `FTEII`; the
  least-squares sums per `IK`. No reduction is reordered; g++ vectorises only `C(IK) = SIG/WN`,
  an element-wise division, as gfortran does (`:1299`).
- (v) Operation order of each expression compared statement by statement in the two GIMPLE
  dumps: `UPAR = (WSMULT*UABS)*MAX(COS(TH-DERA*UDIR),0)`, `FCDIR`, `FACT`, the moments with
  `(SUMF**2*DSII)*SIG` and `(SUMF*DSII)*SIG**2`, the tail factors, `M0..M2`, `QP`, the peak
  interpolation of `SIGP`, the parabolic fit, `ALP_PM`, and `SIGP**(-5)` as
  `1/((S*S*S)*(S*S))`, the object's power tree. No FMA in either object.
- (v) Folded constants with the object's bits: `DERA`, `TPI`, `TPIINV`, `RADE`; `TPIINV**2`
  (0x3ccf817a) and `TPIINV**3` (0x3b841a2e), both written as the object's bits (the float product
  chain gives the same bits, a `static_assert` says so); `0.3333/0.25` (0x3faaa64c), `0.2/0.25` (0x3f4ccccd), `2.865048E-1`, `1.E-15`; each
  appears in the `.rodata.cst4` of both objects with the same bits.
- (v) `SIGN(ABS(DENOM),DENOM)` is folded to `DENOM` by gfortran (`:1441`, `:1455`); `SIGN(ABS(DENOM),
  NUMER)` is `copysignf(DENOM,NUMER)` (`:1487`); the C++ writes both that way.
- (v) The nine `MAX`/`MIN` sites with the object's `maxss`/`minss` operand order: `MAX(M0,0.)` at
  `:1402` has the constant as destination (`maxss m0,zero`: a NaN `M0` gives NaN), written
  `max_r(zero, m0)`; g++ emits it as `vcmpnltss`+`vandps`, the same result for NaN and -0. The
  other eight have the expression as destination.
- (v) libm calls matched by relocation: `cosf` 1, `logf` 1, `atan2f` 2. Both compilers expand
  `MOD`/`fmodf` inline as an `fprem` loop (g++ keeps an `fmodf` call for a NaN result); both are
  exact. `SQRT` is `sqrtss` in both (g++ keeps a
  `sqrtf` call on the negative-argument path for `errno`; same value).
- (v) `WAVNU1` is an external call in the Fortran (`__w3dispmd_MOD_wavnu1`); the C++ inlines
  `ww::prop::wavnu1`, whose parity is argued in [`WAVNU1_KOKKOS.md`](WAVNU1_KOKKOS.md).
- ⚠ CUDA (compiled for `sm_89` with `--fmad=false`, never run): 287 fused multiply-adds in the SASS,
  taken to be CUDA's `cosf`, `logf`, `atan2f`, `fmodf` and division, not traced; whether those
  return glibc's bits is unknown.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
