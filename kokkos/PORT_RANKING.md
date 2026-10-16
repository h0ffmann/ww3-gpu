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

The profile names 87 routines, 25 of them ported, so this table has 62 rows, not 100. Rows 5 to 62 took less than one gprof sample (10 ms): their order is gprof's, not a cost order, and a longer case or `perf` is needed to rank them. A routine this case never calls (another switch, another grid) is not in the profile and not here.

| # | routine | WW3 file | self s | self % | cumulative % |
|---|---|---|---|---|---|
| 1 | `init` | `—` | 0.86 | 0.65 | 100.0 |
| 2 | `w3wavemd:w3wave` | `model/src/w3wavemd.F90` | 0.03 | 0.02 | 100.0 |
| 3 | `w3adatmd:w3dima` | `model/src/w3adatmd.F90` | 0.01 | 0.01 | 100.0 |
| 4 | `w3gdatmd:w3dims` | `model/src/w3gdatmd.F90` | 0.01 | 0.01 | 100.0 |
| 5 | `w3parall:init_get_isea` | `model/src/w3parall.F90` | < 0.01 | 0.00 | 100.0 |
| 6 | `w3servmd:itrace` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 7 | `w3timemd:dsec21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 8 | `w3servmd:print_memcheck` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 9 | `w3gsrumd:w3fdwt_r8` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 10 | `w3timemd:stme21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 11 | `w3servmd:wwtime` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 12 | `w3timemd:tick21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 13 | `w3gdatmd:w3setg` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 14 | `w3adatmd:w3seta` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 15 | `w3odatmd:w3seto` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 16 | `w3gsrumd:get_fdw3.constprop.0` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 17 | `w3gsrumd:w3nnsc` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 18 | `w3wdatmd:w3setw` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 19 | `w3idatmd:w3seti` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 20 | `w3gdatmd:w3gntx` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 21 | `w3gsrumd:gsu_create` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 22 | `w3gsrumd:w3cgdm_r4` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 23 | `w3gsrumd:w3gsuc_ptr_r8` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 24 | `w3iogrmd:w3iogr` | `model/src/w3iogrmd.F90` | < 0.01 | 0.00 | 100.0 |
| 25 | `w3parall:set_up_nseal_nsealm` | `model/src/w3parall.F90` | < 0.01 | 0.00 | 100.0 |
| 26 | `w3timemd:d2j` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 27 | `w3timemd:t2d` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 28 | `w3timemd:tdiff` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 29 | `main` | `—` | < 0.01 | 0.00 | 100.0 |
| 30 | `w3adatmd:w3dmnl` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 31 | `w3adatmd:w3naux` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 32 | `w3fldsmd:w3fldh` | `model/src/w3fldsmd.F90` | < 0.01 | 0.00 | 100.0 |
| 33 | `w3gdatmd:w3dimx` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 34 | `w3gdatmd:w3nmod` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 35 | `w3idatmd:w3dimi` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 36 | `w3idatmd:w3ninp` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 37 | `w3initmd:w3init` | `model/src/w3initmd.F90` | < 0.01 | 0.00 | 100.0 |
| 38 | `w3iogomd:w3fldtoij` | `model/src/w3iogomd.F90` | < 0.01 | 0.00 | 100.0 |
| 39 | `w3iogomd:w3flgrdflag` | `model/src/w3iogomd.F90` | < 0.01 | 0.00 | 100.0 |
| 40 | `w3iogomd:w3flgrdupdt` | `model/src/w3iogomd.F90` | < 0.01 | 0.00 | 100.0 |
| 41 | `w3iorsmd:w3iors` | `model/src/w3iorsmd.F90` | < 0.01 | 0.00 | 100.0 |
| 42 | `w3nmlshelmd:read_domain_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 43 | `w3nmlshelmd:read_homogeneous_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 44 | `w3nmlshelmd:read_input_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 45 | `w3nmlshelmd:read_output_date_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 46 | `w3nmlshelmd:read_output_path_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 47 | `w3nmlshelmd:read_output_type_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 48 | `w3nmlshelmd:report_domain_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 49 | `w3nmlshelmd:report_homogeneous_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 50 | `w3nmlshelmd:report_input_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 51 | `w3nmlshelmd:report_output_date_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 52 | `w3nmlshelmd:report_output_path_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 53 | `w3nmlshelmd:report_output_type_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 54 | `w3nmlshelmd:w3nmlshel` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 55 | `w3odatmd:w3dmo5` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 56 | `w3odatmd:w3nout` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 57 | `w3servmd:str_to_upper` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 58 | `w3servmd:strsplit` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 59 | `w3servmd:wwdate` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 60 | `w3wdatmd:w3dimw` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 61 | `w3wdatmd:w3ndat` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 62 | `finalise.0.constprop.0` | `—` | < 0.01 | 0.00 | 100.0 |

The cumulative column starts from the ported routines' share, so it reads as the share of the run
that would be in Kokkos once every row down to it is ported.

## Regtested, per port

The port has no tests of its own ([ADR-0010](../docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)):
a ported routine is tested only by WW3's regtests, run inside WW3 with the routine on Kokkos and
compared pair to pair with the same WW3 without it (`scripts/ww3-gpu.sh --ww3-gpu --cpu`, `just ww3-gpu-matrix`).
The record is the `regtested` column of `kokkos/ports.tsv`.

| routine | status | Kokkos files | regtested |
|---|---|---|---|
| `w3snl1md:w3snl1` | L2 | `snl1_dia.cpp`, `snl1_shim.cpp` | yes: L2 on the CPU, ww3_ts1 and bench_small, against the Fortran DIA and pair to pair against --ww3; ww3_ts1 in CI (health h1); WW3 regtest matrix of 100 entries, byte-identical on all 100 (test.comp), nccmp-tol 71 PASS, 29 n/a, 0 FAIL, after AF11 took gfortran's -O3 chain for x**11 (docs/data/hoff_matrix_2026-10-15.tsv); the 25 shared-memory OpenMP (OMPG) NL1 entries, byte-identical on all 25 (test.comp), nccmp-tol 25 PASS, with OMP_MAX_ACTIVE_LEVELS=2, without which Kokkos aborts inside W3WAVE's parallel region (docs/data/hoff_matrix_omp_2026-10-15.tsv) |
| `w3snl1md:insnl1` | L2 | `snl1_tables.cpp`, `snl1_shim.cpp` | yes: through the DIA's L2, which reads these tables in every --ww3-gpu run (health h1); byte-identical on the 100-entry matrix (docs/data/hoff_matrix_2026-10-15.tsv) |
| `w3src4md:w3sds4` | ported | `st4_sds4.hpp`, `st4_libmvec.hpp`, `st4_shim.cpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3src4md:w3sin4` | ported | `st4_sin4.hpp`, `st4_shim.cpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3src4md:w3spr4` | ported | `st4_spr4.hpp`, `st4_libmvec.hpp`, `st4_shim.cpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3src4md:calc_ustar` | ported | `st4_calc_ustar.hpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3src4md:insin4` | ported | `st4_insin4.cpp`, `st4_arrays.hpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3src4md:tabu_stress` | ported | `st4_tabu_stress.cpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3src4md:tabu_tauhf` | ported | `st4_tabu_tauhf.cpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3src4md:tabu_tauhf2` | ported | `st4_tabu_tauhf2.cpp` | no: the ST4 checkpoint (ADR-0011) is under way: smoke-ww3-gpu on ww3_ts1 and full-ww3-gpu on bench_small byte-identical to the Fortran with each flag and all together, 2026-10-15 (docs/log/2026-10-15-st4-checkpoint.md); the WW3 regtest matrix is not yet recorded |
| `w3srcemd:w3srce` | ported | `srce_kernel.hpp`, `srce_params.hpp`, `snl1_team.hpp`, `srce_shim.cpp`, `shim_state.hpp` | no: waiting for the checkpoint of W3SRCE (ADR-0011); built on serial-debug, openmp-release and cuda-release only |
| `w3sln1md:w3sln1` | ported | `srce_ln1.hpp` | no: waiting for the checkpoint of W3SRCE (ADR-0011); built on serial-debug, openmp-release and cuda-release only |
| `w3sbt1md:w3sbt1` | ported | `srce_bt1.hpp` | no: waiting for the checkpoint of W3SRCE (ADR-0011); built on serial-debug, openmp-release and cuda-release only |
| `w3sdb1md:w3sdb1` | ported | `srce_db1.hpp` | no: waiting for the checkpoint of W3SRCE (ADR-0011); built on serial-debug, openmp-release and cuda-release only |
| `w3pro3md:w3xyp3` | ported | `prop_xyp3.hpp`, `prop_params.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the propagation checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3uqckmd:w3qck3` | ported | `prop_qck3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the propagation checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3uqckmd:w3qck1` | ported | `prop_qck1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the propagation checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3pro3md:w3ktp3` | ported | `prop_ktp3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the propagation checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3dispmd:wavnu1` | ported | `prop_wavnu1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the propagation checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3uqckmd:w3qck2` | ported | `prop_qck2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the propagation checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3wavemd:w3gath` | ported | `wave_gath_scat.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3wavemd:w3scat` | ported | `wave_gath_scat.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3updtmd:w3uwnd` | ported | `updt_uwnd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3updtmd:w3dzxy` | ported | `updt_dzxy.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3dispmd:wavnu2` | ported | `disp_distab.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3IOGR wiring exist |
| `w3dispmd:distab` | ported | `disp_distab.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3IOGR wiring exist |
| `w3updtmd:w3utrn` | ported | `updt_utrn.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3pro3md:w3map3` | ported | `prop_map3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3pro3md:w3mapt` | ported | `prop_mapt.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `w3ref1md:w3sref` | ported | `srce_ref1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `w3sic4md:w3sic4` | ported | `srce_ic4.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `pdlib_w3profsmd:pdlib_w3xypfsn2` | ported | `pdlib_fsn2.hpp`, `pdlib_params.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim, the MPI callables and the PDLIB_W3XYPUG hunk |
| `pdlib_w3profsmd:apply_boundary_condition_va` | ported | `pdlib_boundary_va.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim, the MPI callables and the PDLIB_W3XYPUG hunk |
| `pdlib_w3profsmd:pdlib_w3xypfspsi2` | ported | `pdlib_fspsi2.hpp`, `pdlib_params.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim, the MPI callables and the PDLIB_W3XYPUG hunk |
| `w3parall:prop_refraction_pr3` | ported | `prop_parall.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the PDLIB implicit solver wired |
| `w3parall:prop_freq_shift_m2` | ported | `prop_parall.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the PDLIB implicit solver wired |
| `w3parall:prop_freq_shift` | ported | `prop_freq_shift.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the PDLIB implicit solver wired |
| `pdlib_w3profsmd:calcarray_jacobi_spectral_1` | ported | `pdlib_jacobi_spectral.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the PDLIB implicit solver wired |
| `pdlib_w3profsmd:calcarray_jacobi_vec` | ported | `pdlib_jacobi_vec.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the PDLIB implicit solver wired |
| `w3sis1md:w3sis1` | ported | `srce_is1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `w3sic1md:w3sic1` | ported | `srce_ic1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `w3dispmd:wavnu3` | ported | `disp_wavnu3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its PDLIB callers wired |
| `w3dispmd:wavnu_local` | ported | `disp_wavnu3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its PDLIB callers wired |
| `w3str1md:w3str1` | ported | `srce_tr1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `w3sbt4md:tabu_erf` | ported | `srce_bt4.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `w3sbt4md:insbt4` | ported | `srce_bt4.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `w3sbt4md:w3sbt4` | ported | `srce_bt4.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
| `pdlib_w3profsmd:pdlib_jacobi_gauss_seidel_block` | ported | `pdlib_jgs_block.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim with the MPI hooks and the U_JAC contract, and the W3WAVE hunk |
| `pdlib_w3profsmd:pdlib_explicit_block` | ported | `pdlib_explicit_block.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim with the MPI hooks |
| `pdlib_w3profsmd:apply_boundary_condition` | ported | `pdlib_boundary.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim with the MPI hooks |
| `pdlib_w3profsmd:pdlib_w3xypfsfct2` | ported | `pdlib_fsfct2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); no regtest selects EXPFSFCT |
| `pdlib_w3profsmd:pdlib_w3xypug` | ported | `pdlib_w3xypug.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3WAVE hunk |
| `w3partmd:ptnghb` | ported | `part_ptnghb.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3PART hunk |
| `w3partmd:pt_fld` | ported | `part_fld.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3PART hunk |
| `w3partmd:ptsort` | ported | `part_ptsort.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3PART hunk |
| `w3partmd:ptmean` | ported | `part_ptmean.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs W3PART and a shim |
| `w3snl3md:insnl3` | ported | `snl3_tables.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3IOGR hunk |
| `w3partmd:w3part` | ported | `part_w3part.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3IOSF hunk |
| `w3snl3md:w3snl3` | ported | `snl3_gmd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and the W3SRCE hunk |
