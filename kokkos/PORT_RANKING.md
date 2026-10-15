# PORT_RANKING

The WW3 routines still in Fortran, the most expensive first, and how much of each ported
routine's Kokkos code was regtested. Written by `scripts/port_score.py --write` from
`docs/data/ww3_full_gprof.tsv` and `kokkos/ports.tsv`; do not
edit. Port order is wall time ([ADR-0002](../docs/ADRs/ADR-0002-port-order-wall-time.md)):
the next port is row 1, and `/port-next` starts it.

## Still in Fortran, by self time

Profile: full (bench_small, 120 x 80, 9 520 sea points, 12 h), Release -pg, switches/switch_lab_shrd, 1 thread, ww3_shel 146 s under -pg, WW3@6d1bcdfe, 2026-10-14 (`just profile-case full`). Self time, so a routine's callees and
what the compiler inlined into it are in the caller's row; the shares are this case's and this
switch's, not every model's.

The profile names 87 routines, 2 of them ported, so this table has 85 rows, not 100. Rows 19 to 85 took less than one gprof sample (10 ms): their order is gprof's, not a cost order, and a longer case or `perf` is needed to rank them. A routine this case never calls (another switch, another grid) is not in the profile and not here.

| # | routine | WW3 file | self s | self % | cumulative % |
|---|---|---|---|---|---|
| 1 | `w3src4md:w3sds4` | `model/src/w3src4md.F90` | 30.52 | 23.23 | 71.6 |
| 2 | `w3src4md:w3sin4` | `model/src/w3src4md.F90` | 16.63 | 12.66 | 84.2 |
| 3 | `w3srcemd:w3srce` | `model/src/w3srcemd.F90` | 7.36 | 5.60 | 89.8 |
| 4 | `w3src4md:w3spr4` | `model/src/w3src4md.F90` | 4.10 | 3.12 | 93.0 |
| 5 | `w3pro3md:w3xyp3` | `model/src/w3pro3md.F90` | 3.92 | 2.98 | 95.9 |
| 6 | `w3uqckmd:w3qck3` | `model/src/w3uqckmd.F90` | 1.50 | 1.14 | 97.1 |
| 7 | `w3uqckmd:w3qck1` | `model/src/w3uqckmd.F90` | 1.19 | 0.91 | 98.0 |
| 8 | `init` | `—` | 0.86 | 0.65 | 98.6 |
| 9 | `w3sdb1md:w3sdb1` | `model/src/w3sdb1md.F90` | 0.51 | 0.39 | 99.0 |
| 10 | `w3pro3md:w3ktp3` | `model/src/w3pro3md.F90` | 0.40 | 0.30 | 99.3 |
| 11 | `w3sbt1md:w3sbt1` | `model/src/w3sbt1md.F90` | 0.28 | 0.21 | 99.6 |
| 12 | `w3wavemd:w3scat` | `model/src/w3wavemd.F90` | 0.21 | 0.16 | 99.7 |
| 13 | `w3sln1md:w3sln1` | `model/src/w3sln1md.F90` | 0.17 | 0.13 | 99.8 |
| 14 | `w3wavemd:w3gath` | `model/src/w3wavemd.F90` | 0.15 | 0.11 | 100.0 |
| 15 | `w3wavemd:w3wave` | `model/src/w3wavemd.F90` | 0.03 | 0.02 | 100.0 |
| 16 | `w3updtmd:w3uwnd` | `model/src/w3updtmd.F90` | 0.01 | 0.01 | 100.0 |
| 17 | `w3adatmd:w3dima` | `model/src/w3adatmd.F90` | 0.01 | 0.01 | 100.0 |
| 18 | `w3gdatmd:w3dims` | `model/src/w3gdatmd.F90` | 0.01 | 0.01 | 100.0 |
| 19 | `w3src4md:calc_ustar` | `model/src/w3src4md.F90` | < 0.01 | 0.00 | 100.0 |
| 20 | `w3parall:init_get_isea` | `model/src/w3parall.F90` | < 0.01 | 0.00 | 100.0 |
| 21 | `w3dispmd:wavnu1` | `model/src/w3dispmd.F90` | < 0.01 | 0.00 | 100.0 |
| 22 | `w3servmd:itrace` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 23 | `w3timemd:dsec21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 24 | `w3servmd:print_memcheck` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 25 | `w3dispmd:wavnu2` | `model/src/w3dispmd.F90` | < 0.01 | 0.00 | 100.0 |
| 26 | `w3gsrumd:w3fdwt_r8` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 27 | `w3timemd:stme21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 28 | `w3servmd:wwtime` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 29 | `w3timemd:tick21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 30 | `w3gdatmd:w3setg` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 31 | `w3adatmd:w3seta` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 32 | `w3odatmd:w3seto` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 33 | `w3gsrumd:get_fdw3.constprop.0` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 34 | `w3gsrumd:w3nnsc` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 35 | `w3wdatmd:w3setw` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 36 | `w3idatmd:w3seti` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 37 | `w3gdatmd:w3gntx` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 38 | `w3gsrumd:gsu_create` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 39 | `w3gsrumd:w3cgdm_r4` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 40 | `w3gsrumd:w3gsuc_ptr_r8` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 41 | `w3iogrmd:w3iogr` | `model/src/w3iogrmd.F90` | < 0.01 | 0.00 | 100.0 |
| 42 | `w3parall:set_up_nseal_nsealm` | `model/src/w3parall.F90` | < 0.01 | 0.00 | 100.0 |
| 43 | `w3timemd:d2j` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 44 | `w3timemd:t2d` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 45 | `w3timemd:tdiff` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 46 | `main` | `—` | < 0.01 | 0.00 | 100.0 |
| 47 | `w3adatmd:w3dmnl` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 48 | `w3adatmd:w3naux` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 49 | `w3dispmd:distab` | `model/src/w3dispmd.F90` | < 0.01 | 0.00 | 100.0 |
| 50 | `w3fldsmd:w3fldh` | `model/src/w3fldsmd.F90` | < 0.01 | 0.00 | 100.0 |
| 51 | `w3gdatmd:w3dimx` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 52 | `w3gdatmd:w3nmod` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 53 | `w3idatmd:w3dimi` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 54 | `w3idatmd:w3ninp` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 55 | `w3initmd:w3init` | `model/src/w3initmd.F90` | < 0.01 | 0.00 | 100.0 |
| 56 | `w3iogomd:w3fldtoij` | `model/src/w3iogomd.F90` | < 0.01 | 0.00 | 100.0 |
| 57 | `w3iogomd:w3flgrdflag` | `model/src/w3iogomd.F90` | < 0.01 | 0.00 | 100.0 |
| 58 | `w3iogomd:w3flgrdupdt` | `model/src/w3iogomd.F90` | < 0.01 | 0.00 | 100.0 |
| 59 | `w3iorsmd:w3iors` | `model/src/w3iorsmd.F90` | < 0.01 | 0.00 | 100.0 |
| 60 | `w3nmlshelmd:read_domain_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 61 | `w3nmlshelmd:read_homogeneous_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 62 | `w3nmlshelmd:read_input_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 63 | `w3nmlshelmd:read_output_date_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 64 | `w3nmlshelmd:read_output_path_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 65 | `w3nmlshelmd:read_output_type_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 66 | `w3nmlshelmd:report_domain_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 67 | `w3nmlshelmd:report_homogeneous_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 68 | `w3nmlshelmd:report_input_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 69 | `w3nmlshelmd:report_output_date_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 70 | `w3nmlshelmd:report_output_path_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 71 | `w3nmlshelmd:report_output_type_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 72 | `w3nmlshelmd:w3nmlshel` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 73 | `w3odatmd:w3dmo5` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 74 | `w3odatmd:w3nout` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 75 | `w3pro3md:w3map3` | `model/src/w3pro3md.F90` | < 0.01 | 0.00 | 100.0 |
| 76 | `w3pro3md:w3mapt` | `model/src/w3pro3md.F90` | < 0.01 | 0.00 | 100.0 |
| 77 | `w3servmd:str_to_upper` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 78 | `w3servmd:strsplit` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 79 | `w3servmd:wwdate` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 80 | `w3src4md:insin4` | `model/src/w3src4md.F90` | < 0.01 | 0.00 | 100.0 |
| 81 | `w3updtmd:w3dzxy` | `model/src/w3updtmd.F90` | < 0.01 | 0.00 | 100.0 |
| 82 | `w3updtmd:w3utrn` | `model/src/w3updtmd.F90` | < 0.01 | 0.00 | 100.0 |
| 83 | `w3wdatmd:w3dimw` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 84 | `w3wdatmd:w3ndat` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 85 | `finalise.0.constprop.0` | `—` | < 0.01 | 0.00 | 100.0 |

The cumulative column starts from the ported routines' share, so it reads as the share of the run
that would be in Kokkos once every row down to it is ported.

## Regtested, per port

The port has no tests of its own ([ADR-0010](../docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)):
a ported routine is tested only by WW3's regtests, run inside WW3 with the routine on Kokkos and
compared pair to pair with the same WW3 without it (`scripts/hoff.sh --hoff --cpu`, `just hoff-matrix`).
The record is the `regtested` column of `kokkos/ports.tsv`.

| routine | status | Kokkos files | regtested |
|---|---|---|---|
| `w3snl1md:w3snl1` | L2 | `snl1_dia.cpp`, `snl1_shim.cpp` | yes: L2 on the CPU, ww3_ts1 and bench_small, against the Fortran DIA and pair to pair against --ww3; ww3_ts1 in CI (health h1); WW3 regtest matrix of 100 entries, byte-identical on all 100 (test.comp), nccmp-tol 71 PASS, 29 n/a, 0 FAIL, after AF11 took gfortran's -O3 chain for x**11 (docs/data/hoff_matrix_2026-10-15.tsv) |
| `w3snl1md:insnl1` | L2 | `snl1_tables.cpp`, `snl1_shim.cpp` | yes: through the DIA's L2, which reads these tables in every --hoff run (health h1); byte-identical on the 100-entry matrix (docs/data/hoff_matrix_2026-10-15.tsv) |
