# W3PART_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The driver of WW3's spectral partitioning: one 2-D spectrum `E(f,theta)` in, the number of
partitions `NP` and their integrated parameters `XP(1:15,0:NP)` out. Under `PTMETH` 1 to 3 it
digitises the inverted spectrum into `IHMAX` levels, sorts them (`PTSORT`), floods the watersheds
(`PT_FLD`, Vincent and Soille 1991) and integrates each partition (`PTMEAN`); `PTMETH` 2 moves the
wind-forced bins of every partition into one new partition, 3 only sorts by Hs, 1 sorts by wind-sea
fraction, merges the wind seas when `FLCOMB` and sorts the rest by Hs. `PTMETH` 4 splits by the wave
age criterion `WSMULT*U*cos(theta-theta_u) > c`, 5 by a cut frequency `PTFCUT`
(`w3partmd.F90:197-209`). Callers: `W3IOSF` (`w3iosfmd.F90:278`), `ww3_outp`, `ww3_ounp`
(`ww3_ounp.F90:2067`).

## What was ported

| | |
|---|---|
| WW3 routine | `W3PART`, `model/src/w3partmd.F90:138-497` at NOAA-EMC/WW3 `03adda91`; its only `#ifdef` is `W3_S` (not ported). Reference switch `switch_NCEP_st4` (GFS-Wave) |
| Kokkos code | [`../src/ww_kokkos/part_w3part.hpp`](../src/ww_kokkos/part_w3part.hpp), `ww::part::w3part`; it calls `ptsort` ([`part_ptsort.hpp`](../src/ww_kokkos/part_ptsort.hpp)), `pt_fld` ([`part_fld.hpp`](../src/ww_kokkos/part_fld.hpp)), `ptmean` ([`part_ptmean.hpp`](../src/ww_kokkos/part_ptmean.hpp)) and, in its host form, `PtnghbTable::update` ([`part_ptnghb.hpp`](../src/ww_kokkos/part_ptnghb.hpp)); none is re-translated |
| C shim | none yet; see "What the shim must do" |
| Translated before | none (checked kokkos/ports.tsv, kokkos/src, remote branches, PRs, 2026-10-16) |
| Plan | [`NOAA_OPS_KOKKOS_PLAN_202610.md`](../../docs/NOAA_OPS_KOKKOS_PLAN_202610.md), issue #42 |

## What the shim must do

- Pass the `PtmeanGrid` and `PtmeanOpts` that `PTMEAN` needs (see
  [`PTMEAN_KOKKOS.md`](PTMEAN_KOKKOS.md)); `W3PART` reads from them `SIG(1:NK)`, `TH`, `WSMULT` and
  `PTMETH`. Pass `W3partOpts` with `IHMAX`, `WSCUT`, `PTFCUT`, `FLCOMB` of the current output set
  (`w3odatmd.F90:1836-1850`); `IHMAX >= 1` (`W3GRID` makes it `MAX(50, IHM)`, `w3gridmd.F90:3140`).
- Give `NEIGH` for the current `NK`, `NTH`. The host overload `w3part(PtnghbTable&, ...)` updates it
  as the Fortran's module state does; a parallel caller calls `update()` once before the loop and
  passes `table.neigh.data()` (copied to the device) to the device form. The Fortran calls
  `PTNGHB` at `:359`, after the early returns of `:281-344`; the table depends on `(NK, NTH)`
  only, so building it earlier changes no output.
- Give each spectrum its own scratch: `w3part_nreal(nk, nth, dimxp)` Reals and
  `w3part_nint(nk, nth, dimxp, ihmax)` ints, carved by `w3part_carve`. `PTMEAN`'s part is sized for
  `NPI = NSPEC + 1`, the largest `NP_MAX` (`PT_FLD`'s label count plus `PTMETH` 2's wind partition).
  `XP` holds `15*(DIMXP+1)` Reals.
- Act on the result. `nxpfull > 0` (return `kPtmeanXpFull`): write format 1000 (`:1533-1534`) with
  `DIMXP+1` on `NDSE`, once per count, when `IAPROC == NAPERR` (`:1416`); `NP` and `XP` are the
  Fortran's, which goes on after the message. `kPtmeanNoPeak`: call the Fortran `W3PART` for that
  spectrum and write nothing for `nxpfull` (an earlier `PTMEAN` call may have counted one; the
  Fortran call writes its own format 1000, so writing both would duplicate it); `W3PART` sets `NP`
  and all of `XP` again (`:266-267`) and has no other output. The Fortran reads out of bounds
  there (below) and its result is not reproducible.
- Under `PTMETH` 4 and 5 pass `DIMXP >= 2` (`:207-209`): `PTMEAN` adds a partition below `HSPMIN`
  without the `:1415` test, so with `DIMXP < 2` both the Fortran and the C++ write past `XP`.

## What WW3 does here at the pin

- (v) `PTMEAN`'s out-of-bounds read when no frequency has `SUMF > 0` (`w3partmd.F90:1436`,
  `:1489`, see [`PTMEAN_KOKKOS.md`](PTMEAN_KOKKOS.md)) is reachable from every `PTMEAN` call of
  `W3PART` (`:303`, `:333`, `:368`, `:397`, `:452`). The C++ stops at the first one.
- (v) `PTMETH` 4 and 5 skip the flat-spectrum test of `:344`: a zero spectrum still goes to
  `PTMEAN` with two partitions. Translated as written.
- (v) `MINVAL`, `MAXVAL` and `MAXLOC` skip NaN. A NaN bin gets level `MAX(1,MIN(IHMAX,NINT(NaN)))`
  = 1 (`lroundf` of NaN truncated to 0), the deepest level. In the Hs sorts (`:417`, `:485`) a
  NaN Hs is never picked while a number is left, and when only NaN and `-1.` entries remain the
  `-1.` (already copied) column wins, so a partition can be copied twice and a NaN one dropped.
  Translated as written.
- (v) `PMAP` is an uninitialised local (`:250`); `3.b` reads `PMAP(INDEX(IP))` for `INDEX(IP)` in
  `1..NP`, and under `PTMETH` 1 and 2 `PTMEAN` writes `PMAP(1..NPO)` for every `NPO >= 1`
  (`:1419-1423`), so no undefined value is read.

## Arithmetic

Reference: the `-O3` object of `w3partmd.F90` as WW3's CMake Release build compiles it under
`switch_NCEP_st4` (gfortran 16.2, the `pratico#ww3` shell; the same object the `PTMEAN` translator
built and checked identical to the CMake object, re-checked identical 2026-10-16), with its
`-fdump-tree-optimized-lineno` GIMPLE. C++: the `openmp-release` flags and `-ffp-contract=off`,
warnings clean. Scripts in the translator's scratch: `tu.sh`, `cuda.sh`. The translator's object
was built by the host's g++ 15.2; the reviewer rebuilt it with g++ 16.2 in the `pratico#ww3`
shell (2026-10-16), and the items below hold for that object.

- (v) `UPAR = (WSMULT*UABS)*MAX(COS(TH-DERA*UDIR),0.)` at `:286` and `:380`: the object computes
  `DERA*UDIR` and `WSMULT*UABS` once and keeps the cosine as `maxss` destination (`maxss %xmm6,%xmm0`
  after `cosf`: NaN gives 0). The C++ writes `max_r(cos, zero)`; g++ emits `vmaxss 0,cos,cos`, the
  same operand order. `C = SIG(IK)/WN(IK)`; the tests are `C >= UPAR` (`:289`) and `UPAR > C`
  (`:383`) in both objects.
- (v) `SIGCUT = TPI*PTFCUT` (`:314`, constant 0x40c90fdb in both) and `SIG(IK) <= SIGCUT`.
- (v) `MINVAL`/`MAXVAL` (`:342-343`) as gfortran inlines them: first non-NaN element seeds the
  limit (`<= Inf`, `>= -Inf`), then strict `<`/`>`; all NaN gives NaN. `MAXVAL` reuses
  `pt_fld_detail::maxval`. The flat test `ZMAX-ZMIN < 1.E-9` with the constant 0x3089705f.
- (v) Levels (`:346-349`): `Z = ZMAX-ZP` stored, `FACT = REAL(IHMAX-1)/(ZMAX-ZMIN)`, then
  `NINT(Z*FACT + 1.)`. gfortran calls `lroundf` and keeps the low 32 bits; the C++ uses
  `ww::st4::nint` (`roundf`, range test, 64-bit convert, truncate), equal for every float,
  NaN and out of range give 0. Then integer `MIN(IHMAX, .)`, `MAX(., 1)`. The C++ object
  converts only strictly between `0xdeffffff` and `0x5effffff`, `±(2^63-2^39)` (two `vcomiss`, NaN fails the first, then
  `vcvttss2si` to 64 bits), so g++ has no out-of-range conversion to exploit; outside it gives
  0, as `lroundf`'s `cvttss2si` gives `LONG_MIN` whose low 32 bits are 0, and every float of
  magnitude `>= 2^55` has zero low 32 bits anyway.
- (v) `MAXLOC` (`:417`, `:434`, `:462`, `:485`) as gfortran inlines it: first non-NaN element
  seeds position and limit, strict `>` keeps the first of equal maxima, all NaN gives 1 (the
  object's PHI). The copies, the `-1.` markers, the `NWS` count with `TP(6,IT) >= WSCUT`, the
  `IMO` relabelling of `3.b` and `NWS = MIN(1,NWS)` follow the Fortran's order.
- (v) libm calls matched by relocation: `cosf` 2 in both; `lroundf` 1 in the Fortran, `roundf` 1
  in the C++. No FMA in either object.
- (v) CUDA (CUDA 12.9 in the `pratico#cuda` shell, `sm_89`, `--fmad=false -lineinfo`, compiled
  only, never run; reviewer, 2026-10-16): the 1380 `FFMA` of the kernel with `PTMEAN` inlined are
  none of the port's own arithmetic. By the innermost source line of `nvdisasm -gi`: 523 in the
  libdevice bodies of `logf` (180), `atan2f` (120), `fmodf` (90), `sqrtf` (70) and `cosf` (63);
  22 in the out-of-line `rcp_rn`, `sqrt_rn` and `div_rn` slow paths; the other 835, on lines of
  `part_w3part.hpp`, `part_ptmean.hpp` and `prop_wavnu1.hpp`, each within 14 instructions of a
  `MUFU.RCP` or `FCHK`: the Newton steps of IEEE division and reciprocal (5 per quotient, so
  `:287`, `:348` and `:381` give 5 each). Products and sums of the source are separate `FMUL`
  and `FADD`. ⚠ Whether CUDA's `cosf`, `logf`, `atan2f` and `fmodf` return glibc's bits is not
  known.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
