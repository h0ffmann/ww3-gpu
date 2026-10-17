# The ST4 regtest matrix of 2026-10-15, stopped at entry 91: 81 entries compared, 0 files different

2026-10-17, an agent for the owner. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
[ADR-0011](../ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md),
[WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md). It completes
[`2026-10-15-st4-checkpoint.md`](2026-10-15-st4-checkpoint.md), which ends with this matrix
"running when this entry was written".

## Question

What did the regtest matrix of the ST4 checkpoint find before it stopped, and does it let the
`W3SDS4`, `W3SIN4` and `W3SPR4` rows of `kokkos/ports.tsv` say `regtested` yes?

## What was done

- The matrix: `WW3GPU_ROUTINES=SDS4,SIN4,SPR4 just ww3-gpu-matrix 8 all`, `WW3GPU_STRICT=ON`,
  4 MPI ranks, 4 OpenMP threads, configured 2026-10-15 23:03 in
  `build/ww3-gpu-matrix-all-SDS4-SIN4-SPR4/` (its `CMakeCache.txt`), WW3 at the fork commit
  the checkpoint entry names, `c9a6a0c6`. Machine: the owner's i9-14900, CPU only.
- It was stopped on 2026-10-16 at 02:34 local time: four runs end with Open MPI's "Abort is in
  progress...hit ctrl-c again" (`logs/77.K.log`, `79.K.log`, `84.K.log`, `88.G.log`, all
  written at 02:34:14) (v). Nobody recorded the stop.
- Read on 2026-10-17: the 91 rows in `rows/<n>.tsv` and the logs of each row that did not compare.
  The rows were not copied into `docs/data/`: a partial matrix at an old pin is not a record a
  table may quote.

## Result

Of the 136 entries the selection had at that pin (136 after the `FLX5` exclusion; the
checkpoint entry's update), 91 have a row (v):

| Rows | `test.comp` | `nccmp-tol` | `kokkos_spectra` > 0 | What it means |
|---|---|---|---|---|
| 70 (24 serial, 10 OpenMP, 36 MPI) | 0 files different | PASS | yes | byte-identical, and the port ran |
| 7 (`ww3_ts2` five times, `ww3_tp2.18` twice) | 0 files different | n/a (no NetCDF field it judges) | yes | byte-identical, and the port ran |
| 4 (2 serial, 2 MPI) | 0 files different | PASS | no | identical, but the port did not run: the switch has `ST4` and the case does not call it |
| 1 (entry 8, `ww3_tp2.15 -i input_rho`) | n/a | n/a | — | neither run built: `ST4` with `FLX5`, refused at configure (the checkpoint entry's update) |
| 2 (entries 89, 90, `ww3_tp2.21`, `NO_PDLIB`) | n/a | n/a | — | `ww3_grid` stops in the Fortran run and in the Kokkos run alike: "Fortran runtime error: Missing comma between descriptors" at `WW3/model/src/ww3_grid.F90:72`; a failure of WW3's input under this toolchain, not of the port ⚠ (not checked with `--ww3`) |
| 7 (entries 77, 79, 82, 83, 84, 88 `ww3_tp2.17` PDLIB; 91 `ww3_tp2.21` PDLIB) | n/a | n/a | — | interrupted at 02:34 |

- (v) Every entry that compared, 81 of them, has `test.comp` 0 files different, and 77 of those
  ran the Kokkos ST4. No entry found a difference.
- Entries 92 to 136, 45 entries and mostly MPI, never started. The PDLIB entries are among
  them and among the interrupted ones, so no PDLIB entry ran the ST4 port here ⚠.

## What it changes

- `regtested` stays no for the ST4 rows. The matrix is partial, and the code it judged is no
  longer what `main` runs: since 2026-10-15 23:00 `st4_shim.cpp` changed in `daecd98`, `3023bb6`
  and `9d6ce2f`, and the `WW3/` pin has moved since `c9a6a0c6`
  (`git log --since=2026-10-15T23:00 -- WW3 kokkos/src/fortran_iface/st4_shim.cpp`). A pin move is itself a checkpoint (ADR-0011).
- What the result still says: at `c9a6a0c6`, 81 regtest entries saw no byte of difference from
  the ST4 port. A defect of the translation that shows in WW3's own regtests would most likely
  have shown here, and none did ⚠ (an argument, not a verdict).
- Next: the whole matrix again, at the current pin, after the parallel goal 42 work (WFIP-0005)
  has merged, so it neither competes for the CPU with that work's timings nor judges a shim that
  is still changing. The command is the one above. Its record goes to `docs/data/` and moves the
  three rows to `regtested` yes if every entry passes.
- `ww3_tp2.21`'s `ww3_grid` failure needs its own look: if `--ww3` stops there too, it is a
  WW3 input that gfortran 16.2.0 refuses, and that entry can never judge the port at this pin.
