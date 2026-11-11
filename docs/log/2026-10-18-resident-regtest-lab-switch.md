# The resident Kokkos preset on WW3's regtest inputs: refused by every `matrix.base` switch, passed with the lab switch's source terms

2026-10-18, an agent for M. Hoffmann; the CPU validation plan
([`docs/plans/2026-10-18-001-cpu-resident-validation-plan.md`](../plans/2026-10-18-001-cpu-resident-validation-plan.md),
units U3, U4, U4a), [WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md),
[ADR-0016](../ADRs.md#adr-0016).

## Question

Does the configuration that `scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident` times
(parts `SRCE`, `XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV` on Kokkos OpenMP) give the Fortran's bytes on
WW3's own regtests?

## What was done

On the owner's i9-14900 (32 hardware threads), with `WW3/` at `769c44f0` and then at `80644ba9`:

1. U3, the timed case pair to pair: `bash scripts/ww3-gpu.sh --ww3-gpu smoke|full --cpu --kokkos
   resident`, at both pins.
2. U4, the plan's selection: `WW3GPU_KOKKOS=resident just ww3-gpu-matrix 4 serial`, then `omp`
   with `WW3GPU_OMP_THREADS=8` (words `ST4;NL1;PR3;!PDLIB`, 10 serial and 4 `omp` entries).
3. U4a on the failures, then a sample of 6 entries with each switch's source-term words replaced
   by `switches/switch_lab_shrd`'s (`WW3GPU_LAB_SWITCH=ON`): `WW3GPU_KOKKOS=resident
   WW3GPU_LAB_SWITCH=ON just ww3-gpu-matrix 4 serial -R '^0(01|03|07|10):'`, and `2 omp -R
   '^00[12]:'` with `WW3GPU_OMP_THREADS=8`. The summaries say `dcb85d2 (dirty)`: the driver change
   was not yet committed; `db2c27c` commits it unchanged.

## Result

- (v) U3 passes at both pins. On smoke and full, every model file of the Kokkos run is
  byte-identical to the Fortran run of the same executable, and every file but the NetCDF is
  byte-identical to `--ww3`'s. One run each on a loaded machine ⚠: full took 15.51 s with Kokkos,
  23.38 s with the Fortran and 24.81 s with `--ww3`. These numbers are not for a table.
- (v) U4 failed on all 14 entries without a run that could be compared, for two reasons in the
  fork:
  - `W3KOKKOS_SRCE_SETUP` used `W3GDATMD`'s `LN1`, `BT1` and `DB1` scalars without their switch
    guard, so 12 builds stopped in `w3kokkosmd.F90`. This is fixed on the fork at `80644ba9`
    (`kokkos/src/fortran_iface/PATCH.md`, section 22).
  - The Kokkos `W3SRCE` translates only the lab switch's source terms. Its gate needs `BT1 DB1
    LN1 MLIM` and none of 44 other words (`model/src/w3kokkosmd.F90:1751-1757` at `769c44f0`).
    No `ST4`+`NL1` switch file in `WW3/regtests` has that set: 47 lack `BT1 DB1 MLIM`, and
    `ww3_tp2.15`'s lacks `MLIM` (v, a word check of every `switch*` under `WW3/regtests`). Their
    Kokkos runs stop at `WW_SRCE_INIT` code 5.
- (v) With the lab switch's source terms, the sample gives:

| entry | verdict | evidence |
|---|---|---|
| `ww3_tp2.15` `default` | pass | `test.comp` 0/23 files different, `nccmp-tol` PASS, every part `=K`, `SRCE` 6 900 spectra |
| `ww3_ts2` `ST4_PR3_UQ` | pass | `test.comp` 0/22, every part `=K`, `SRCE` 4 632 spectra |
| `ww3_ts3` `ST4_PR3_UNO` | refused by design | `test.comp` 0/20, but `XYP3=F KTP3=F`: no `UQ` |
| `ww3_tp2.6` `ST4` | refused by design | `WW_KOKKOS_XYP3=1, BUT ITS KOKKOS TRANSLATION DOES NOT COVER THIS SWITCH FILE`: unstructured grid |
| `ww3_ts3` `ST4_PR3_UQ_OMP` | refused by design | the same message: `MGP MGG` (moving grid) |
| `mww3_test_05` `ST4_PR3_UQ_OMP` | refused by design | the same message: `MGP MGG` |

  Record: [`docs/data/ww3gpu_matrix_resident_lab_2026-10-18.tsv`](../data/ww3gpu_matrix_resident_lab_2026-10-18.tsv).
- (v) The Kokkos `W3XYP3` covers `PR3`+`UQ` on a rectilinear or curvilinear grid, with `SHRD` and
  without `PDLIB`, `MGP`, `MGG` or a tripole closure (`model/src/w3kokkos_prop3md.F90:112-134` at
  `80644ba9`). With those words the selection is 7 serial entries, 0 `omp` and 0 `mpi` (v,
  `WW3GPU_KOKKOS=resident just ww3-gpu-matrix-list serial|omp|mpi`). The resident defaults are now
  those words.

## What it means

- No difference between Fortran and Kokkos was found on any run that reached a comparison. Two
  regtest inputs pass bit for bit with the timed run's source terms.
- The timed configuration's OpenMP path (`OMPG`) is covered by U3 alone: no `matrix.base` entry
  has `PR3`, `UQ` and `OMPG` without a moving grid. Its `MPI` path is covered by nothing, because
  the Kokkos `W3XYP3` needs `SHRD` ⚠.
- "Regtested" for the resident preset means, as of this entry: WW3's regtest inputs built with the
  lab switch's source terms, serial, on a sample. It does not mean `matrix.base` as WW3 ships it.
