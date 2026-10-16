# The easiest Fortran left, translated to Kokkos: one agent per routine, one review pass

Status: **translated and reviewed**, 2026-10-16, branch `goal/42-port-batch-easy` (the record is
[`docs/log/2026-10-16-easy-batch-kokkos-translation.md`](log/2026-10-16-easy-batch-kokkos-translation.md)). Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
the port planner. Translation only, bit for bit, nothing run: no regtest, no regtest written, no
Kokkos test written (the owner, 2026-10-16). The port has no tests of its own
([ADR-0010](ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)); its checkpoints are
[ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md)'s.

## Why these routines

Every routine a committed profile times in compute is translated (`kokkos/PORT_RANKING.md`), and
so is every source-term package NOAA's operational switches compile (PRs #152, #157). The owner
asked for the easiest of what is left first. Of the 303 untranslated routines of the compute
modules of `WW3-noaa/model/src` at `03adda91`, these are the 92 of at most 100 lines that do no
MPI and whose purpose is not file I/O, smallest first. Left out: the `UOST` file readers `LOAD_ALPHABETA` and `UOST_INITGRID`; `w3wavset` (its `SETUP` switch
needs `PDLIB` and no regtest compiles it), the output printers of `w3arrymd`, the `w3parall`
timers and exchanges, the `w3tidemd` readers and writers, and the helpers already inside a header
(`EXPAND`, `EXPND2`, `MINLAM`, `MAXLAM` of `NL3`; the `FIFO_*` of partitioning).

## Method

As in [`docs/log/2026-10-16-st-flx-swl-kokkos-translation.md`](log/2026-10-16-st-flx-swl-kokkos-translation.md):
six WW3 CMake Release builds in the pinned shell (gfortran 16.2.0), each from
`switches/switch_lab_shrd`: `lab` as is; `mixa` with `TIDE UOST FLD2 IG1`, `IC3 IS2 BT8`; `mixb`
with `NL4 IC5 BT9 NLS`; `nl5` with `NL5`; and the WW3 switch files `switch_SMCMlt` (`smc`) and
`ww3_ts3/input/switch_ST4_PR1_FLD1` (`fld1`). One agent per routine translates against its
module's object, and one fresh agent per routine reviews it once.

## The batch

| Routine | Lines at `03adda91` | Size | Reference build |
|---|---|---|---|
| `w3sbt8md:ccosh` | `w3sbt8md.F90:523-530` | 8 | `mixa` |
| `w3sbt8md:csinh` | `w3sbt8md.F90:504-511` | 8 | `mixa` |
| `w3tidemd:dpythag` | `w3tidemd.F90:976-991` | 16 | `mixa` |
| `w3sbt9md:calc_nd` | `w3sbt9md.F90:571-590` | 20 | `mixb` |
| `w3tidemd:dsvbksb` | `w3tidemd.F90:995-1019` | 25 | `mixa` |
| `w3profsmd:ddot` | `w3profsmd.F90:4684-4710` | 27 | `lab` |
| `w3profsmd:lusol` | `w3profsmd.F90:3673-3699` | 27 | `lab` |
| `w3tidemd:caldatt` | `w3tidemd.F90:1284-1310` | 27 | `mixa` |
| `w3snl5md:hasnan` | `w3snl5md.F90:884-914` | 31 | `nl5` |
| `w3profsmd:tidycg` | `w3profsmd.F90:2560-2591` | 32 | `lab` |
| `w3smcomd:w3s2xy_smcnn` | `w3smcomd.F90:711-743` | 33 | `smc` |
| `w3profsmd:brkdn` | `w3profsmd.F90:2594-2627` | 34 | `lab` |
| `w3tidemd:tide_set_indices` | `w3tidemd.F90:494-527` | 34 | `mixa` |
| `w3gkemd:vmfunc` | `w3gkemd.F90:319-354` | 36 | `nl5` |
| `w3profsmd:lutsol` | `w3profsmd.F90:3701-3736` | 36 | `lab` |
| `w3smcomd:w3s2xy_smcnn_int` | `w3smcomd.F90:762-798` | 37 | `smc` |
| `w3snl1md:gauleg` | `w3snl1md.F90:1310-1346` | 37 | `lab` |
| `w3gkemd:qfunc` | `w3gkemd.F90:234-271` | 38 | `nl5` |
| `w3gkemd:ufunc` | `w3gkemd.F90:358-396` | 39 | `nl5` |
| `w3sic3md:smooth_k` | `w3sic3md.F90:2497-2535` | 39 | `mixa` |
| `w3iogomd:vmin_d` | `w3iogomd.F90:4777-4816` | 40 | `lab` |
| `w3profsmd:givens` | `w3profsmd.F90:2461-2500` | 40 | `lab` |
| `w3gkemd:vpfunc` | `w3gkemd.F90:275-315` | 41 | `nl5` |
| `w3iogomd:vplus_d` | `w3iogomd.F90:4820-4860` | 41 | `lab` |
| `w3profsmd:amuxms` | `w3profsmd.F90:2903-2944` | 42 | `lab` |
| `w3profsmd:uppdir` | `w3profsmd.F90:2418-2459` | 42 | `lab` |
| `w3profsmd:ldsol` | `w3profsmd.F90:3302-3344` | 43 | `lab` |
| `w3profsmd:lsol` | `w3profsmd.F90:3258-3300` | 43 | `lab` |
| `w3profsmd:usol` | `w3profsmd.F90:3492-3534` | 43 | `lab` |
| `w3profsmd:dlassq` | `w3profsmd.F90:4638-4681` | 44 | `lab` |
| `w3profsmd:lsolc` | `w3profsmd.F90:3346-3389` | 44 | `lab` |
| `w3profsmd:udsol` | `w3profsmd.F90:3536-3579` | 44 | `lab` |
| `w3profsmd:usolc` | `w3profsmd.F90:3581-3624` | 44 | `lab` |
| `w3profsmd:implu` | `w3profsmd.F90:2372-2416` | 45 | `lab` |
| `w3profsmd:amux` | `w3profsmd.F90:2856-2901` | 46 | `lab` |
| `w3profsmd:udsolc` | `w3profsmd.F90:3626-3671` | 46 | `lab` |
| `w3uostmd:uost_srctrmcompute` | `w3uostmd.F90:298-343` | 46 | `mixa` |
| `w3profsmd:ldsolc` | `w3profsmd.F90:3391-3437` | 47 | `lab` |
| `w3tidemd:tide_predict_only` | `w3tidemd.F90:2571-2617` | 47 | `mixa` |
| `w3profsmd:amuxe` | `w3profsmd.F90:3046-3093` | 48 | `lab` |
| `w3smcomd:w3s2xy_smc` | `w3smcomd.F90:816-863` | 48 | `smc` |
| `w3uostmd:uost_setgrid` | `w3uostmd.F90:231-278` | 48 | `mixa` |
| `w3profsmd:atmux` | `w3profsmd.F90:2946-2994` | 49 | `lab` |
| `w3profsmd:atmuxr` | `w3profsmd.F90:2996-3044` | 49 | `lab` |
| `w3profsmd:vbrmv` | `w3profsmd.F90:3203-3251` | 49 | `lab` |
| `w3profsmd:daxpy` | `w3profsmd.F90:4712-4761` | 50 | `lab` |
| `w3smcomd:mapsta_smc` | `w3smcomd.F90:518-568` | 51 | `smc` |
| `w3profsmd:amuxd` | `w3profsmd.F90:3095-3146` | 52 | `lab` |
| `w3profsmd:ldsoll` | `w3profsmd.F90:3439-3490` | 52 | `lab` |
| `w3profsmd:amuxj` | `w3profsmd.F90:3148-3201` | 54 | `lab` |
| `w3profsmd:stopbis` | `w3profsmd.F90:2503-2557` | 55 | `lab` |
| `w3snlsmd:expand` | `w3snlsmd.F90:597-651` | 55 | `mixb` |
| `w3tidemd:tide_predict` | `w3tidemd.F90:2513-2568` | 56 | `mixa` |
| `w3sic3md:delta` | `w3sic3md.F90:1732-1790` | 59 | `mixa` |
| `w3profsmd:ilu0` | `w3profsmd.F90:4241-4300` | 60 | `lab` |
| `w3smcomd:w3s2xy_smcrg` | `w3smcomd.F90:359-418` | 60 | `smc` |
| `w3tidemd:astr` | `w3tidemd.F90:910-971` | 62 | `mixa` |
| `w3uostmd:uost_sourceterm_setgrid` | `w3uostmd.F90:606-667` | 62 | `mixa` |
| `w3profsmd:qsplit` | `w3profsmd.F90:3738-3800` | 63 | `lab` |
| `w3profsmd:dnrm2` | `w3profsmd.F90:4568-4635` | 68 | `lab` |
| `w3profsmd:bisinit` | `w3profsmd.F90:2630-2699` | 70 | `lab` |
| `w3psmcmd:smckuno2` | `w3psmcmd.F90:2821-2890` | 70 | `smc` |
| `w3smcomd:w3s2xy_smcrg_dir` | `w3smcomd.F90:438-510` | 73 | `smc` |
| `w3uostmd:uost_sourceterm_compute` | `w3uostmd.F90:1056-1128` | 73 | `mixa` |
| `w3fld1md:sig2wn` | `w3fld1md.F90:1174-1247` | 74 | `mixa, fld1` |
| `w3psmcmd:smcgtcrfr` | `w3psmcmd.F90:2717-2790` | 74 | `smc` |
| `w3sic3md:cginic3_cheng` | `w3sic3md.F90:2420-2493` | 74 | `mixa` |
| `w3snl4md:cgfnc` | `w3snl4md.F90:5396-5469` | 74 | `mixb` |
| `w3smcomd:calc_interp` | `w3smcomd.F90:622-696` | 75 | `smc` |
| `w3srcemd:sign_vsd_semi_implicit_ww3` | `w3srcemd.F90:2559-2633` | 75 | `lab` |
| `w3srcemd:sign_vsd_patankar_ww3` | `w3srcemd.F90:2647-2722` | 76 | `lab` |
| `w3profsmd:setdepth` | `w3profsmd.F90:1601-1678` | 78 | `lab` |
| `w3gkemd:asymsmattimvec` | `w3gkemd.F90:983-1061` | 79 | `nl5` |
| `w3gig1md:df1f2theta` | `w3gig1md.F90:60-142` | 83 | `mixa` |
| `w3sic3md:ic3table_cheng` | `w3sic3md.F90:1962-2044` | 83 | `mixa` |
| `w3uostmd:compute_reduction_psi` | `w3uostmd.F90:688-770` | 83 | `mixa` |
| `w3snl4md:wkfnc` | `w3snl4md.F90:5286-5369` | 84 | `mixb` |
| `w3profsmd:runrc` | `w3profsmd.F90:3801-3886` | 86 | `lab` |
| `w3psmcmd:smcyuno2r` | `w3psmcmd.F90:1661-1746` | 86 | `smc` |
| `w3sis2md:funcd_fval` | `w3sis2md.F90:1420-1506` | 87 | `mixa` |
| `w3sis2md:funcd_fderiv` | `w3sis2md.F90:1523-1611` | 89 | `mixa` |
| `w3sis2md:w3rpwnice` | `w3sis2md.F90:1098-1187` | 90 | `mixa` |
| `w3sic3md:f_zhao_v1` | `w3sic3md.F90:1279-1369` | 91 | `mixa` |
| `w3sic3md:func0_zhao` | `w3sic3md.F90:1372-1462` | 91 | `mixa` |
| `w3psmcmd:smcxuno2r` | `w3psmcmd.F90:1551-1644` | 94 | `smc` |
| `w3fld1md:infld` | `w3fld1md.F90:781-875` | 95 | `mixa, fld1` |
| `w3sic5md:cmplx_cosh` | `w3sic5md.F90:2050-2144` | 95 | `mixb` |
| `w3sic5md:cmplx_sinh` | `w3sic5md.F90:1940-2034` | 95 | `mixb` |
| `w3fld2md:wnd2sat` | `w3fld2md.F90:592-687` | 96 | `mixa` |
| `w3fld1md:wnd2sat` | `w3fld1md.F90:1377-1473` | 97 | `mixa, fld1` |
| `w3sic3md:f_zhao_cheng` | `w3sic3md.F90:2713-2809` | 97 | `mixa` |
| `w3sic5md:cmplx_tanh2` | `w3sic5md.F90:2160-2256` | 97 | `mixb` |
