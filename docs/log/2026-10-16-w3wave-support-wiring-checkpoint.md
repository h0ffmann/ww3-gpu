# W3WAVE's support routines wired, a master Kokkos flag, and the pin-move checkpoint

2026-10-16, an agent for the owner, issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
[`docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md).

## Question

Does WW3 at a new fork commit, with `W3GATH`, `W3SCAT`, `W3UWND`, `W3UTRN`, `W3DZXY`, `W3MAP3`,
`W3MAPT` and `DISTAB` behind their own flags and every wired part behind one master flag, give the
Fortran's bits with every flag off and with every flag on?

## What was done

The shim `kokkos/src/fortran_iface/wave_support_shim.cpp`, the resolver `part_flags.cpp`, and
h0ffmann/WW3 `357f575c` (`ww3-gpu/kokkos-wave-support`, one commit on `8e433f99`), described in
[`PATCH.md`](../../kokkos/src/fortran_iface/PATCH.md), sections 7 and 8. On an Intel Core
i9-14900 (32 threads), CPU only, in the pinned `ww3` shell (gfortran/g++ 16.2.0, Kokkos 5.2.2
OpenMP): `just smoke-ww3-gpu`; `scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos X` for X =
`none`, `all`, `SNL1,SDS4,SIN4,SPR4,ST4_TABLES`, `UWND,UTRN,MAP3,MAPT,DISTAB`, `DISTAB`; and
`--ww3-gpu full --cpu --kokkos X` for X = `all` and `GATH,SCAT,UWND,UTRN,DZXY,MAP3,MAPT,DISTAB`;
each output compared with `cmp` against the Fortran run of the same executable and against
`--ww3`.

## Result

- Every run: `out_grd.ww3` (and `out_pnt.ww3`, `restart.ww3` in the smoke) and the `ww3_ounf`
  NetCDF file byte-identical to the Fortran of the same executable; the binary files
  byte-identical to `--ww3`; the NetCDF file against `--ww3` differs only in the
  `WAVEWATCH_III_switches` attribute, which names `KOKKOS` (`ncdump` diff) (v).
- Calls with `WW_KOKKOS_ALL=1`, full case: `W3SRCE` 230 100 spectra, `W3GATH` and `W3SCAT`
  13 824 each, `W3UWND` 25, `W3UTRN`, `W3DZXY`, `W3MAP3`, `W3MAPT`, `DISTAB` 1 each; under the
  master the per-routine ST4 and DIA counters are 0, because the Kokkos `W3SRCE` calls them inside
  itself. The ST4 and DIA flags alone in the smoke: `W3SNL1` 2 339, `W3SDS4` 2 339, `W3SIN4`
  6 838, `W3SPR4` 6 660 spectra, ST4 tables "0 values differ" (v).
- The Kokkos DISTAB tables equal the Fortran's: 0 of the 246 `EWN1`, `ECG1` values, `DSIE` and
  `N1MAX` (v, the shim's comparison).
- Negative for coverage: both cases have `FLAGTR = 0` (`ww3_grid.out`), so `W3UTRN` ran only its
  initialisation; the smoke is one point, so `W3GATH`, `W3SCAT`, `W3DZXY` made 0 calls there ⚠.

## What it changes

The WW3/ pin moves to `357f575c`; `kokkos/ports.tsv` says `wired` yes for the nine rows of the
unit and `regtested` yes for all but `W3UTRN`. Next: `just ww3-gpu-matrix` and the `OMPG`
entries with the master flag on, a case with `FLAGTR > 0` for `W3UTRN`, and the `wired` column
of the `W3SRCE` and ST0 to ST6 rows, which the pin already carries ⚠.
