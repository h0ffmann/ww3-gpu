# gprof self time inside the source terms, `regtests/ww3_ts1`

⚠ Transcribed from issue #45 (measured 2026-10-07; not yet reproduced by a committed command).
Task T1 of #45 replaces this file with the output of `kokkos/tools/profile/gprof_table.sh` and the
exact invocation.

Setup as reported: `ww3_shel` at `WW3@761cf79d`, switch `switches/switch_lab_shrd` without NC4
(ST4, NL1, PR3, UQ, LN1, BT1, DB1, MLIM, no MPI), `-O3 -pg`, gfortran 16.2; `ww3_ts1` with
`ww3_grid_ST4_T475.nml`, 3×3 grid, NK=36, NTH=24, 10 model days, 14 401 calls of `W3SRCE`,
32 s wall. Self time in seconds; W3SRCE is its own self time, without the routines it calls; "others" is W3SLN1 + W3SDB1 + W3SBT1 + CALC_USTAR:

| routine | ST4 default (SDSCUM=-0.40344) | SDSCUM=0 |
|---|---|---|
| W3SDS4 | 2.01 | 0.51 |
| W3SNL1 | 0.42 | 0.45 |
| W3SIN4 | 0.30 | 0.27 |
| W3SPR4 | 0.14 | 0.03 |
| W3SRCE | 0.09 | 0.07 |
| others | 0.03 | 0 |
| total | 2.98 | 1.33 |
