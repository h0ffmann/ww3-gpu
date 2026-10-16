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

The profile names 87 routines, 33 of them ported, so this table has 54 rows, not 100. Rows 2 to 54 took less than one gprof sample (10 ms): their order is gprof's, not a cost order, and a longer case or `perf` is needed to rank them. A routine this case never calls (another switch, another grid) is not in the profile and not here.

| # | routine | WW3 file | self s | self % | cumulative % |
|---|---|---|---|---|---|
| 1 | `init` | `—` | 0.86 | 0.65 | 100.0 |
| 2 | `w3servmd:itrace` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 3 | `w3timemd:dsec21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 4 | `w3servmd:print_memcheck` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 5 | `w3gsrumd:w3fdwt_r8` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 6 | `w3timemd:stme21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 7 | `w3servmd:wwtime` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 8 | `w3timemd:tick21` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 9 | `w3gdatmd:w3setg` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 10 | `w3adatmd:w3seta` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 11 | `w3odatmd:w3seto` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 12 | `w3gsrumd:get_fdw3.constprop.0` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 13 | `w3gsrumd:w3nnsc` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 14 | `w3wdatmd:w3setw` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 15 | `w3idatmd:w3seti` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 16 | `w3gdatmd:w3gntx` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 17 | `w3gsrumd:gsu_create` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 18 | `w3gsrumd:w3cgdm_r4` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 19 | `w3gsrumd:w3gsuc_ptr_r8` | `model/src/w3gsrumd.F90` | < 0.01 | 0.00 | 100.0 |
| 20 | `w3timemd:d2j` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 21 | `w3timemd:t2d` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 22 | `w3timemd:tdiff` | `model/src/w3timemd.F90` | < 0.01 | 0.00 | 100.0 |
| 23 | `main` | `—` | < 0.01 | 0.00 | 100.0 |
| 24 | `w3adatmd:w3dmnl` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 25 | `w3adatmd:w3naux` | `model/src/w3adatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 26 | `w3fldsmd:w3fldh` | `model/src/w3fldsmd.F90` | < 0.01 | 0.00 | 100.0 |
| 27 | `w3gdatmd:w3dimx` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 28 | `w3gdatmd:w3nmod` | `model/src/w3gdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 29 | `w3idatmd:w3dimi` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 30 | `w3idatmd:w3ninp` | `model/src/w3idatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 31 | `w3initmd:w3init` | `model/src/w3initmd.F90` | < 0.01 | 0.00 | 100.0 |
| 32 | `w3iogomd:w3fldtoij` | `model/src/w3iogomd.F90` | < 0.01 | 0.00 | 100.0 |
| 33 | `w3iorsmd:w3iors` | `model/src/w3iorsmd.F90` | < 0.01 | 0.00 | 100.0 |
| 34 | `w3nmlshelmd:read_domain_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 35 | `w3nmlshelmd:read_homogeneous_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 36 | `w3nmlshelmd:read_input_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 37 | `w3nmlshelmd:read_output_date_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 38 | `w3nmlshelmd:read_output_path_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 39 | `w3nmlshelmd:read_output_type_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 40 | `w3nmlshelmd:report_domain_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 41 | `w3nmlshelmd:report_homogeneous_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 42 | `w3nmlshelmd:report_input_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 43 | `w3nmlshelmd:report_output_date_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 44 | `w3nmlshelmd:report_output_path_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 45 | `w3nmlshelmd:report_output_type_nml` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 46 | `w3nmlshelmd:w3nmlshel` | `model/src/w3nmlshelmd.F90` | < 0.01 | 0.00 | 100.0 |
| 47 | `w3odatmd:w3dmo5` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 48 | `w3odatmd:w3nout` | `model/src/w3odatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 49 | `w3servmd:str_to_upper` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 50 | `w3servmd:strsplit` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 51 | `w3servmd:wwdate` | `model/src/w3servmd.F90` | < 0.01 | 0.00 | 100.0 |
| 52 | `w3wdatmd:w3dimw` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 53 | `w3wdatmd:w3ndat` | `model/src/w3wdatmd.F90` | < 0.01 | 0.00 | 100.0 |
| 54 | `finalise.0.constprop.0` | `—` | < 0.01 | 0.00 | 100.0 |

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
| `w3src0md:w3spr0` | ported | `srce_st0_spr0.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src1md:w3spr1` | ported | `srce_st1_spr1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src1md:w3sin1` | ported | `srce_st1_sin1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src1md:w3sds1` | ported | `srce_st1_sds1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src2md:w3spr2` | ported | `srce_st2_spr2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src2md:w3sin2` | ported | `srce_st2_sin2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src2md:w3sds2` | ported | `srce_st2_sds2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src2md:inptab` | ported | `srce_st2_inptab.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src2md:w3beta` | ported | `srce_st2_inptab.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src3md:w3spr3` | ported | `srce_st3_spr3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src3md:w3sin3` | ported | `srce_st3_sin3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src3md:w3sds3` | ported | `srce_st3_sds3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src3md:insin3` | ported | `srce_st3_insin3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src3md:tabu_stress` | ported | `srce_st3_tabu_stress.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src3md:tabu_tauhf` | ported | `srce_st3_tabu_tauhf.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src3md:calc_ustar` | ported | `srce_st3_calc_ustar.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:w3spr6` | ported | `srce_st6_spr6.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:w3sds6` | ported | `srce_st6_sds6.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:lfactor` | ported | `srce_st6_lfactor.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:tau_wave_atmos` | ported | `srce_st6_tau_wave_atmos.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:irange` | ported | `srce_st6_util.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:rrange` | ported | `srce_st6_util.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:tauwinds` | ported | `srce_st6_util.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3swldmd:w3swl6` | ported | `srce_swl6.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3swldmd:w3swl4` | ported | `srce_swl4.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3swldmd:irange` | ported | `srce_swl_util.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3flx1md:w3flx1` | ported | `srce_flx1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3flx2md:w3flx2` | ported | `srce_flx2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3flx3md:w3flx3` | ported | `srce_flx3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3flx4md:w3flx4` | ported | `srce_flx4.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3flx5md:w3flx5` | ported | `srce_flx5.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3src6md:w3sin6` | ported | `srce_st6_sin6.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs the fork branch pinned and WW3's regtests |
| `w3fld1md:infld` | ported | `fld1_infld.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3fld1md:sig2wn` | ported | `fld1_sig2wn.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3fld1md:wnd2sat` | ported | `fld1_wnd2sat.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3fld2md:wnd2sat` | ported | `fld2_wnd2sat.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gig1md:df1f2theta` | ported | `gig1_df1f2theta.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:asymsmattimvec` | ported | `gke_asymsmattimvec.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:qfunc` | ported | `gke_qfunc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:ufunc` | ported | `gke_ufunc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:vmfunc` | ported | `gke_vmfunc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:vpfunc` | ported | `gke_vpfunc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3iogomd:vmin_d` | ported | `iogo_vmin_d.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3iogomd:vplus_d` | ported | `iogo_vplus_d.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:amux` | ported | `profs_amux.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:amuxd` | ported | `profs_amuxd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:amuxe` | ported | `profs_amuxe.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:amuxj` | ported | `profs_amuxj.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:amuxms` | ported | `profs_amuxms.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:atmux` | ported | `profs_atmux.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:atmuxr` | ported | `profs_atmuxr.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:bisinit` | ported | `profs_bisinit.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:brkdn` | ported | `profs_brkdn.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:daxpy` | ported | `profs_daxpy.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:ddot` | ported | `profs_ddot.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:dlassq` | ported | `profs_dlassq.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:dnrm2` | ported | `profs_dnrm2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:givens` | ported | `profs_givens.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:ilu0` | ported | `profs_ilu0.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:implu` | ported | `profs_implu.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:ldsol` | ported | `profs_ldsol.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:ldsolc` | ported | `profs_ldsolc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:ldsoll` | ported | `profs_ldsoll.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:lsol` | ported | `profs_lsol.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:lsolc` | ported | `profs_lsolc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:lusol` | ported | `profs_lusol.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:lutsol` | ported | `profs_lutsol.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:qsplit` | ported | `profs_qsplit.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:runrc` | ported | `profs_runrc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:setdepth` | ported | `profs_setdepth.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:stopbis` | ported | `profs_stopbis.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:tidycg` | ported | `profs_tidycg.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:udsol` | ported | `profs_udsol.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:udsolc` | ported | `profs_udsolc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:uppdir` | ported | `profs_uppdir.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:usol` | ported | `profs_usol.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:usolc` | ported | `profs_usolc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:vbrmv` | ported | `profs_vbrmv.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcgtcrfr` | ported | `psmc_smcgtcrfr.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smckuno2` | ported | `psmc_smckuno2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcxuno2r` | ported | `psmc_smcxuno2r.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcyuno2r` | ported | `psmc_smcyuno2r.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sbt8md:ccosh` | ported | `bt8_ccosh.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sbt8md:csinh` | ported | `bt8_csinh.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sbt9md:calc_nd` | ported | `bt9_calc_nd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:cginic3_cheng` | ported | `ic3_cginic3_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:delta` | ported | `ic3_delta.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:f_zhao_cheng` | ported | `ic3_f_zhao_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:f_zhao_v1` | ported | `ic3_f_zhao_v1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:func0_zhao` | ported | `ic3_func0_zhao.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:ic3table_cheng` | ported | `ic3_ic3table_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:smooth_k` | ported | `ic3_smooth_k.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:cmplx_cosh` | ported | `ic5_cmplx_cosh.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:cmplx_sinh` | ported | `ic5_cmplx_sinh.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:cmplx_tanh2` | ported | `ic5_cmplx_tanh2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sis2md:findroots_nr` | ported | `is2_w3rpwnice.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sis2md:funcd_fderiv` | ported | `is2_funcd_fderiv.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sis2md:funcd_fval` | ported | `is2_funcd_fval.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sis2md:w3rpwnice` | ported | `is2_w3rpwnice.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3smcomd:calc_interp` | ported | `smco_calc_interp.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3smcomd:mapsta_smc` | ported | `smco_mapsta_smc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3smcomd:w3s2xy_smc` | ported | `smco_w3s2xy_smc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3smcomd:w3s2xy_smcnn` | ported | `smco_w3s2xy_smcnn.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3smcomd:w3s2xy_smcnn_int` | ported | `smco_w3s2xy_smcnn_int.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3smcomd:w3s2xy_smcrg` | ported | `smco_w3s2xy_smcrg.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3smcomd:w3s2xy_smcrg_dir` | ported | `smco_w3s2xy_smcrg_dir.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl1md:gauleg` | ported | `snl1_gauleg.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl4md:cgfnc` | ported | `snl4_cgfnc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl4md:wkfnc` | ported | `snl4_wkfnc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl5md:hasnan` | ported | `snl5_hasnan.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snlsmd:expand` | ported | `snls_expand.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3srcemd:sign_vsd_patankar_ww3` | ported | `srce_sign_vsd_patankar_ww3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3srcemd:sign_vsd_semi_implicit_ww3` | ported | `srce_sign_vsd_semi_implicit_ww3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:astr` | ported | `tide_astr.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:caldatt` | ported | `tide_caldatt.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:dpythag` | ported | `tide_dpythag.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:dsvbksb` | ported | `tide_dsvbksb.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:tide_predict` | ported | `tide_tide_predict.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:tide_predict_only` | ported | `tide_tide_predict_only.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:tide_set_indices` | ported | `tide_tide_set_indices.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3uostmd:compute_reduction_psi` | ported | `uost_compute_reduction_psi.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3uostmd:uost_setgrid` | ported | `uost_uost_setgrid.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3uostmd:uost_sourceterm_compute` | ported | `uost_uost_sourceterm_compute.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3uostmd:uost_sourceterm_setgrid` | ported | `uost_uost_sourceterm_setgrid.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3uostmd:uost_srctrmcompute` | ported | `uost_uost_srctrmcompute.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_wave_setup_apply_fct` | ported | `wavset_fd_wave_setup_apply_fct.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3partmd:fifo_add` | ported | `part_fld.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3partmd:fifo_first` | ported | `part_fld.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3partmd:fifo_empty` | ported | `part_fld.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:differentiate_xydir` | ported | `wavset_differentiate_xydir.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_wave_setup_scalar_prod` | ported | `wavset_fd_wave_setup_scalar_prod.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:trig_compute_diff` | ported | `wavset_trig_compute_diff.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl3md:minlam` | ported | `snl3_tables.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:compute_active_node` | ported | `wavset_compute_active_node.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:juldayt` | ported | `tide_juldayt.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:vuf` | ported | `tide_vuf.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:trig_wave_setup_apply_fct` | ported | `wavset_trig_wave_setup_apply_fct.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl3md:maxlam` | ported | `snl3_tables.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl3md:expnd2` | ported | `snl3_gmd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:tide_find_indices_prediction` | ported | `tide_tide_find_indices_prediction.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_compute_diff` | ported | `wavset_fd_compute_diff.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sis2md:w3fsd_dave` | ported | `is2_w3fsd_dave.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_set_meanvalue_to_zero` | ported | `wavset_fd_set_meanvalue_to_zero.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3parall:init_get_jsea_isproc` | ported | `parall_init_get_jsea_isproc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl3md:expand` | ported | `snl3_gmd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_wave_setup_apply_precond` | ported | `wavset_fd_wave_setup_apply_precond.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:coocsrind` | ported | `gke_coocsrind.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3updtmd:w3uic1` | ported | `updt_w3uic1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_wave_setup_compute_system` | ported | `wavset_fd_wave_setup_compute_system.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3parall:init_get_isea` | ported | `parall_init_get_isea.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:differentiate_xydir_native` | ported | `wavset_differentiate_xydir_native.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl5md:insnl5` | ported | `snl5_insnl5.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3uostmd:uost_sourceterm_compute_ld` | ported | `uost_uost_sourceterm_compute_ld.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3parall:set_up_nseal_nsealm` | ported | `parall_set_up_nseal_nsealm.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcxuno2` | ported | `psmc_smcxuno2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_wave_setup_solve_poisson_neumann_dir` | ported | `wavset_fd_wave_setup_solve_poisson_neumann_dir.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcyuno3r` | ported | `psmc_smcyuno3r.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:trig_wave_setup_apply_precond` | ported | `wavset_trig_wave_setup_apply_precond.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcdcxy` | ported | `psmc_smcdcxy.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcyuno2` | ported | `psmc_smcyuno2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:ic3precalc_cheng` | ported | `ic3_ic3precalc_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcxuno3r` | ported | `psmc_smcxuno3r.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3updtmd:w3uic5` | ported | `updt_w3uic5.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3uostmd:uost_sourceterm_compute_se` | ported | `uost_uost_sourceterm_compute_se.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3parall:get_jsea_ibelong` | ported | `parall_get_jsea_ibelong.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3srcemd:calc_fpi` | ported | `srce_calc_fpi.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:wn_cmplx_hf` | ported | `ic3_wn_cmplx_hf.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:w3smcell` | ported | `psmc_w3smcell.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:bsdet` | ported | `ic3_bsdet.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:tfunc` | ported | `gke_tfunc.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3profsmd:mgsro` | ported | `profs_mgsro.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:w3smcgmp` | ported | `psmc_w3smcgmp.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl1md:couple` | ported | `snl1_couple.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3fld1md:wnd2z0m` | ported | `fld1_wnd2z0m.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:differentiate_xydir_mapsta` | ported | `wavset_differentiate_xydir_mapsta.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sbt9md:ng` | ported | `bt9_ng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:drfun_quad_cheng` | ported | `ic3_drfun_quad_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:prepkgrid` | ported | `gke_prepkgrid.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:trig_compute_lh_stress` | ported | `wavset_trig_compute_lh_stress.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:svd` | ported | `tide_svd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcdhxy` | ported | `psmc_smcdhxy.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcxuno3` | ported | `psmc_smcxuno3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:setvuf_fast` | ported | `tide_setvuf_fast.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3iogomd:w3flgrdupdt` | ported | `iogo_w3flgrdupdt.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:biinterpwt` | ported | `gke_biinterpwt.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:w3ic5wncg` | ported | `ic5_w3ic5wncg.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3gkemd:findquartetconfig` | ported | `gke_findquartetconfig.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:fd_compute_lh_stress` | ported | `wavset_fd_compute_lh_stress.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:setvuf` | ported | `tide_setvuf.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3tidemd:tide_find_indices_analysis` | ported | `tide_tide_find_indices_analysis.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcyuno3` | ported | `psmc_smcyuno3.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3parall:prop_refraction_pr1` | ported | `parall_prop_refraction_pr1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:func1_zhao` | ported | `ic3_func1_zhao.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl1md:f1f1f1` | ported | `snl1_f1f1f1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3psmcmd:smcaverg` | ported | `psmc_smcaverg.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:nr_corr` | ported | `ic5_nr_corr.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3snl5md:calc_wbtv2` | ported | `snl5_calc_wbtv2.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:balancing_matrix` | ported | `ic5_balancing_matrix.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3wavset:preparation_fd_scheme` | ported | `wavset_preparation_fd_scheme.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3iogomd:w3flgrdflag` | ported | `iogo_w3flgrdflag.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3updtmd:w3urho` | ported | `updt_w3urho.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:cmplx_root_muller_v1` | ported | `ic3_cmplx_root_muller_v1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:polyroots` | ported | `ic5_polyroots.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3pro3md:w3cflxy` | ported | `prop_cflxy.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:wn_cmplx_v1` | ported | `ic3_wn_cmplx_v1.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:drfun_dble_cheng` | ported | `ic3_drfun_dble_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic5md:nr_root` | ported | `ic5_nr_root.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3iogomd:skewness` | ported | `iogo_skewness.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:cmplx_root_muller_cheng` | ported | `ic3_cmplx_root_muller_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3sic3md:w3ic3wncg_cheng` | ported | `ic3_w3ic3wncg_cheng.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run); its checkpoint (ADR-0011) needs a shim and its caller's hunk |
| `w3adatmd:w3dima` | ported | `adat_w3dima.hpp`, `adat_wadat.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3INIT/W3IOGO wiring exist |
| `w3gdatmd:w3dims` | ported | `gdat_w3dims.hpp`, `gdat_sgrd.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); its checkpoint (ADR-0011) needs a shim and the W3IOGR wiring |
| `w3iogrmd:w3iogr` | ported | `iogr_w3iogr.hpp`, `iogr_state.hpp`, `iogr_unformatted.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); its checkpoint (ADR-0011) needs the hooks filled, a shim and the W3IOGR hunk |
| `w3wavemd:w3wave` | ported | `wave_w3wave.hpp`, `wave_w3wave_state.hpp`, `wave_w3wave_hooks.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE checkpoint (ADR-0011) runs it once a shim fills the hook table and the Views |
| `w3iogomd:w3outg` | ported | `iogo_w3outg.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); its checkpoint (ADR-0011) needs a shim, the W3WAVE wiring and the CALC_U3STOKES/CALC_WBT hooks |
| `w3updtmd:w3ulev` | ported | `updt_w3ulev.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); the W3WAVE support checkpoint (ADR-0011) runs it once a shim and the W3WAVE wiring exist |
| `pdlib_field_vec:do_output_exchanges` | ported | `pdlib_do_output_exchanges.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); its checkpoint (ADR-0011) needs a shim filling OutputExchangeHooks (MPI, GET_ARRAY_SIZE, W3XDMA/W3XETA/W3SETA) and the W3WAVE wiring |
| `pdlib_field_vec:get_array_size` | ported | `pdlib_get_array_size.hpp` | no: translated only, as the owner asked on 2026-10-16 (no test run in this round); its checkpoint (ADR-0011) needs a shim and DO_OUTPUT_EXCHANGES wired |
