# Batch 3: the next 85 routines left in Fortran, and the shims of the source-term packages

Status: **claimed**, 2026-10-16, branch `goal/42-port-batch-3-shims`. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
the port planner. Translation and wiring only, bit for bit, nothing run: no regtest, no regtest
written, no Kokkos test written (the owner, 2026-10-16). The port has no tests of its own
([ADR-0010](ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)); its checkpoints are
[ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md)'s.

## Why these

The owner asked for the next batch of untranslated Fortran, to raise the number of routines
translated, and in the same pull request the shims of the source-term packages translated in PR
#157 (`ST0`, `ST1`, `ST2`, `ST3`, `ST6`, `FLX1`-`FLX5`, `SWL`), which were written then and held
back.

Of the 210 routines of the compute modules of `WW3-noaa/model/src` at `03adda91` still in
Fortran after PR #159, these are the 85 smallest that do no MPI call and whose purpose is not file
I/O (their only I/O is a diagnostic `WRITE`), smallest first, up to 172 lines. Seven of them
(`EXPAND`, `EXPND2`, `MINLAM`, `MAXLAM` of `NL3`; `FIFO_ADD`, `FIFO_FIRST`, `FIFO_EMPTY` of
partitioning) already have C++ inside an existing header: their agent checks that code against the
object and records it. Left out: the readers, writers and printers (`w3arrymd`, `w3iopomd`,
`w3tidemd` readers), the `w3parall` timers and exchanges, the routines that call MPI, and
`INIT_RANDOM_SEED` of `IC5` (it reads the clock).

## Method

As in [`docs/log/2026-10-16-easy-batch-kokkos-translation.md`](log/2026-10-16-easy-batch-kokkos-translation.md),
with two more WW3 CMake Release builds in the pinned shell: `nl3` (`switch_lab_shrd` with `NL3`)
and `setup` (`ww3_tp2.6`'s `switch_PDLIB` with `SETUP`, for `w3wavset`). One agent per routine
translates against its module's object, one fresh agent per routine reviews it once.

The shims: the ones for `ST0`, `ST1`, `ST2`, `ST3`, `FLX` and `SWL` were written and compiled with
PR #157; `ST6` gets one now. Their Fortran halves (the `W3KOKKOSMD` entries and the `W3SRCE`,
`W3INIT` hunks, each behind its switch and a run-time flag that defaults to the Fortran) go on a
branch of the `h0ffmann/WW3` fork, and the `WW3/` pin moves to it. Compiled, not run.

## The batch

| Routine | Lines at `03adda91` | Size | Reference build |
|---|---|---|---|
| `w3partmd:fifo_add` | `w3partmd.F90:1103-1113` | 11 | `lab` |
| `w3partmd:fifo_first` | `w3partmd.F90:1142-1152` | 11 | `lab` |
| `w3partmd:fifo_empty` | `w3partmd.F90:1122-1133` | 12 | `lab` |
| `w3tidemd:juldayt` | `w3tidemd.F90:1257-1281` | 25 | `mixa` |
| `w3tidemd:vuf` | `w3tidemd.F90:1706-1746` | 41 | `mixa` |
| `w3snl3md:maxlam` | `w3snl3md.F90:1593-1645` | 53 | `nl3` |
| `w3snl3md:minlam` | `w3snl3md.F90:1525-1578` | 54 | `nl3` |
| `w3snl3md:expnd2` | `w3snl3md.F90:717-773` | 57 | `nl3` |
| `w3snl3md:expand` | `w3snl3md.F90:644-703` | 60 | `nl3` |
| `w3wavset:differentiate_xydir` | `w3wavset.F90:363-435` | 73 | `setup` |
| `w3wavset:fd_wave_setup_scalar_prod` | `w3wavset.F90:2655-2729` | 75 | `setup` |
| `w3wavset:fd_set_meanvalue_to_zero` | `w3wavset.F90:2863-2943` | 81 | `setup` |
| `w3wavset:fd_wave_setup_apply_fct` | `w3wavset.F90:1945-2026` | 82 | `setup` |
| `w3wavset:trig_compute_diff` | `w3wavset.F90:598-685` | 88 | `setup` |
| `w3wavset:trig_wave_setup_apply_fct` | `w3wavset.F90:1020-1107` | 88 | `setup` |
| `w3sis2md:w3fsd_dave` | `w3sis2md.F90:1317-1405` | 89 | `mixa` |
| `w3wavset:fd_compute_diff` | `w3wavset.F90:2433-2525` | 93 | `setup` |
| `w3tidemd:tide_find_indices_prediction` | `w3tidemd.F90:244-339` | 96 | `mixa` |
| `w3wavset:compute_active_node` | `w3wavset.F90:1493-1588` | 96 | `setup` |
| `w3parall:init_get_jsea_isproc` | `w3parall.F90:1152-1252` | 101 | `lab, fld1 (PR1), setup (PDLIB)` |
| `w3wavset:fd_wave_setup_apply_precond` | `w3wavset.F90:2039-2139` | 101 | `setup` |
| `w3wavset:fd_wave_setup_compute_system` | `w3wavset.F90:2539-2642` | 104 | `setup` |
| `w3updtmd:w3uic1` | `w3updtmd.F90:1555-1659` | 105 | `lab` |
| `w3gkemd:coocsrind` | `w3gkemd.F90:874-979` | 106 | `nl5` |
| `w3uostmd:uost_sourceterm_compute_ld` | `w3uostmd.F90:793-898` | 106 | `mixa` |
| `w3wavset:differentiate_xydir_native` | `w3wavset.F90:105-212` | 108 | `setup` |
| `w3parall:init_get_isea` | `w3parall.F90:1396-1504` | 109 | `lab, fld1 (PR1), setup (PDLIB)` |
| `w3snl5md:insnl5` | `w3snl5md.F90:433-541` | 109 | `nl5` |
| `w3parall:set_up_nseal_nsealm` | `w3parall.F90:1030-1139` | 110 | `lab, fld1 (PR1), setup (PDLIB)` |
| `w3psmcmd:smcxuno2` | `w3psmcmd.F90:1297-1406` | 110 | `smc` |
| `w3psmcmd:smcyuno3r` | `w3psmcmd.F90:2211-2320` | 110 | `smc` |
| `w3psmcmd:smcyuno2` | `w3psmcmd.F90:1424-1534` | 111 | `smc` |
| `w3wavset:fd_wave_setup_solve_poisson_neumann_dir` | `w3wavset.F90:2742-2852` | 111 | `setup` |
| `w3psmcmd:smcdcxy` | `w3psmcmd.F90:3048-3159` | 112 | `smc` |
| `w3sic3md:ic3precalc_cheng` | `w3sic3md.F90:2047-2160` | 114 | `mixa` |
| `w3wavset:trig_wave_setup_apply_precond` | `w3wavset.F90:892-1005` | 114 | `setup` |
| `w3psmcmd:smcxuno3r` | `w3psmcmd.F90:2080-2194` | 115 | `smc` |
| `w3uostmd:uost_sourceterm_compute_se` | `w3uostmd.F90:920-1034` | 115 | `mixa` |
| `w3updtmd:w3uic5` | `w3updtmd.F90:1670-1786` | 117 | `lab` |
| `w3gkemd:tfunc` | `w3gkemd.F90:400-517` | 118 | `nl5` |
| `w3srcemd:calc_fpi` | `w3srcemd.F90:2428-2545` | 118 | `lab` |
| `w3sic3md:wn_cmplx_hf` | `w3sic3md.F90:984-1102` | 119 | `mixa` |
| `w3parall:get_jsea_ibelong` | `w3parall.F90:1265-1384` | 120 | `lab, fld1 (PR1), setup (PDLIB)` |
| `w3psmcmd:w3smcell` | `w3psmcmd.F90:3665-3784` | 120 | `smc` |
| `w3sic3md:bsdet` | `w3sic3md.F90:1611-1730` | 120 | `mixa` |
| `w3profsmd:mgsro` | `w3profsmd.F90:2701-2822` | 122 | `lab` |
| `w3sic3md:drfun_quad_cheng` | `w3sic3md.F90:2967-3088` | 122 | `mixa` |
| `w3psmcmd:w3smcgmp` | `w3psmcmd.F90:3808-3930` | 123 | `smc` |
| `w3snl1md:couple` | `w3snl1md.F90:1185-1307` | 123 | `lab` |
| `w3fld1md:wnd2z0m` | `w3fld1md.F90:1251-1375` | 125 | `mixa, fld1` |
| `w3wavset:differentiate_xydir_mapsta` | `w3wavset.F90:225-350` | 126 | `setup` |
| `w3sbt9md:ng` | `w3sbt9md.F90:429-557` | 129 | `mixb` |
| `w3tidemd:svd` | `w3tidemd.F90:1313-1443` | 131 | `mixa` |
| `w3gkemd:prepkgrid` | `w3gkemd.F90:1066-1197` | 132 | `nl5` |
| `w3psmcmd:smcdhxy` | `w3psmcmd.F90:2903-3036` | 134 | `smc` |
| `w3gkemd:biinterpwt` | `w3gkemd.F90:1483-1618` | 136 | `nl5` |
| `w3psmcmd:smcxuno3` | `w3psmcmd.F90:1764-1900` | 137 | `smc` |
| `w3tidemd:setvuf_fast` | `w3tidemd.F90:531-667` | 137 | `mixa` |
| `w3wavset:trig_compute_lh_stress` | `w3wavset.F90:448-584` | 137 | `setup` |
| `w3gkemd:findquartetconfig` | `w3gkemd.F90:732-869` | 138 | `nl5` |
| `w3tidemd:setvuf` | `w3tidemd.F90:1893-2030` | 138 | `mixa` |
| `w3wavset:fd_compute_lh_stress` | `w3wavset.F90:2281-2418` | 138 | `setup` |
| `w3iogomd:w3flgrdupdt` | `w3iogomd.F90:179-318` | 140 | `lab` |
| `w3sic5md:w3ic5wncg` | `w3sic5md.F90:493-633` | 141 | `mixb` |
| `w3sic3md:wn_cmplx_v1` | `w3sic3md.F90:839-981` | 143 | `mixa` |
| `w3sic3md:func1_zhao` | `w3sic3md.F90:1465-1608` | 144 | `mixa` |
| `w3psmcmd:smcyuno3` | `w3psmcmd.F90:1919-2063` | 145 | `smc` |
| `w3tidemd:tide_find_indices_analysis` | `w3tidemd.F90:343-488` | 146 | `mixa` |
| `w3parall:prop_refraction_pr1` | `w3parall.F90:285-431` | 147 | `lab, fld1 (PR1), setup (PDLIB)` |
| `w3sic5md:nr_corr` | `w3sic5md.F90:1602-1750` | 149 | `mixb` |
| `w3sic3md:drfun_dble_cheng` | `w3sic3md.F90:2812-2962` | 151 | `mixa` |
| `w3psmcmd:smcaverg` | `w3psmcmd.F90:2549-2700` | 152 | `smc` |
| `w3sic5md:polyroots` | `w3sic5md.F90:1435-1586` | 152 | `mixb` |
| `w3snl1md:f1f1f1` | `w3snl1md.F90:1349-1500` | 152 | `lab` |
| `w3snl5md:calc_wbtv2` | `w3snl5md.F90:560-711` | 152 | `nl5` |
| `w3sic5md:balancing_matrix` | `w3sic5md.F90:956-1109` | 154 | `mixb` |
| `w3wavset:preparation_fd_scheme` | `w3wavset.F90:1777-1932` | 156 | `setup` |
| `w3pro3md:w3cflxy` | `w3pro3md.F90:1980-2136` | 157 | `lab` |
| `w3sic3md:w3ic3wncg_cheng` | `w3sic3md.F90:1800-1959` | 160 | `mixa` |
| `w3sic5md:nr_root` | `w3sic5md.F90:1765-1924` | 160 | `mixb` |
| `w3iogomd:w3flgrdflag` | `w3iogomd.F90:589-749` | 161 | `lab` |
| `w3updtmd:w3urho` | `w3updtmd.F90:2626-2791` | 166 | `lab` |
| `w3sic3md:cmplx_root_muller_cheng` | `w3sic3md.F90:2540-2710` | 171 | `mixa` |
| `w3sic3md:cmplx_root_muller_v1` | `w3sic3md.F90:1106-1276` | 171 | `mixa` |
| `w3iogomd:skewness` | `w3iogomd.F90:4883-5054` | 172 | `lab` |
