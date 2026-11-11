# W3CPRT on the resident VA: partitioning without copying the spectrum back

2026-10-18, agent G of the production single-node goal (unit U9 of
[`docs/plans/`](../plans/), branch `goal/abbb0456-6043-u9-cprt`; fork branch
`ww3-gpu/u9-cprt` of h0ffmann/WW3, on `80644ba9`).

## Question

At every output time with partitioning on (gridded partition fields, or the partition file),
`W3WAVE` read the whole `VA` back from the device for `W3CPRT` (`w3wavemd.F90:2688-2693` at
`80644ba9`), and `W3KOKKOS_OUTG` fell back to the Fortran `W3OUTG` when a partition field was
requested. Can `W3CPRT` run on the resident `VA`, with only its table coming back, bit for bit?

## What was done

- `W3PART` and everything it calls were translated on 2026-10-16 and never wired
  ([`W3PART_KOKKOS.md`](../../kokkos/port_report/W3PART_KOKKOS.md)). `W3CPRT` itself is 236 lines:
  per sea point a conversion of `N` to `E = N 2 pi sigma / c_g`, one `W3PART` call, and the
  storage of `XP(1:15,0:NP)` one point after the other. Points are independent, so a launch over
  points with each point's serial code is the translation; the table is placed by an integer
  prefix sum. Kernel [`iosf_w3cprt.hpp`](../../kokkos/src/ww_kokkos/iosf_w3cprt.hpp), shim
  [`cprt_shim.cpp`](../../kokkos/src/fortran_iface/cprt_shim.cpp), fork unit
  `w3kokkos_cprtmd.F90`, details in
  [`PATCH.md` section 27](../../kokkos/src/fortran_iface/PATCH.md).
- The first run with `CPRT` on still showed 13 device-to-host copies of `VA` in 12 hours: the
  host read before `W3IOSF` (`w3wavemd.F90:2963-2966`). `W3IOSF` reads no `VA` (its `USE` lists,
  `w3iosfmd.F90:429-453`, v), so that read is skipped under `KOKKOS_CPRT`.
- `W3OUTG`'s section 6 (the partition fields from the table) is now its own entry point,
  `ww_outg_part`, so `W3KOKKOS_OUTG` no longer falls back for `FLPART`.
- Static parity: the `-O3` GIMPLE of `w3iosfmd.F90` and of the shim
  ([`W3CPRT_KOKKOS.md`](../../kokkos/port_report/W3CPRT_KOKKOS.md)).
- CPU only, never `--gpu`. i9-14900, shared with other agents' jobs, `OMP_NUM_THREADS=4`,
  `JOBS=4`, gfortran/g++ 16.2.0, Kokkos 5.2.2 OpenMP backend.

Runs. `bench_small` (`kokkos/tests/fixtures/bench_small`: 120 x 80, 9 520 sea points,
`NK = NTH = 24`, 12 hours) writes no output, so two throwaway variants of its `ww3_shel.nml` (not
committed, restored after each run) turned partitioning on, both with hourly field output and the
hourly partition file (`DATE%PARTITION`, `TYPE%PARTITION` over the whole grid, unformatted):

- `p6`: fields `HS T02 DIR` (which `W3OUTG` on Kokkos computes) and the partition file;
- `pf`: the same plus all 17 partition fields (`PHS PTP PLP PDIR PSPR PWS PDP PQP PPE PGW PSW
  PTM10 PT01 PT02 PEP TWS PNR`).

Each ran as `WW3GPU_SWITCH=switch_lab_<sw> WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_COUNT=1 JOBS=4
OMP_NUM_THREADS=4 bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos <parts>`, "before" with
`SRCE,XYP3,KTP3,OUTG,IOPE,UPDV` and "after" with `CPRT` added, and `partition.ww3` was compared
with `cmp` by hand (the script does not list it). WW3's regtest `ww3_tpt1.1` (one multi-modal
spectrum on 18 sea points, one grid per partitioning method `PTM = 1..5`, not in `matrix.base`)
ran through the matrix driver's `regtests/cmake/run_entry.cmake` with the configuration of
`WW3GPU_KOKKOS=CPRT WW3GPU_SWITCH_WORDS=SHRD just ww3-gpu-matrix-list serial`, options `-N -g
PTM<m>`, `WW_KOKKOS_VA_POISON=1`.

## Result

| Run | `VA` device-to-host copies, 12 h, 13 output times | Files |
|---|---|---|
| `p6`, `switch_lab_shrd`, before | 13 (275.71 MB) | every model file and `partition.ww3` byte-identical to the Fortran of the same executable and to `--ww3` (v) |
| `p6`, `switch_lab_shrd`, after | 1 (21.21 MB, `W3WAVE`'s read at its return, which U1 removes on its own branch) | the same (v) |
| `pf`, `switch_lab_shrd`, before (with `ww_outg_part`) | 13 | the same (v) |
| `pf`, `switch_lab_shrd`, after | 1 | the same (v) |
| `pf`, `switch_lab_ompg`, after | 1 | the same (v) |

- (v) In every run `VA host copies poisoned` was 97: the host `VA` was filled with NaN after each
  kernel write, and no output changed, so no host reader read a stale `VA`.
- (v) After, per output time, what a device would move instead of the 21.21 MB of `VA`
  (`WW_KOKKOS_CPRT_COUNT=1` and `WW_KOKKOS_OUTG_COUNT=1`, projected on the host backend, totals
  divided by 13 calls): the table back, 1 135 640 B (`ICPRT` 76 168 B and the used `DTPRT`
  columns); the inputs in, 2 212 324 B (`CG` and `WN` are 1.98 MB of it); with the 17 partition
  fields requested, those fields back, 3 387 072 B. 0 fallbacks to the Fortran.
- (v) `ww3_tpt1.1`, `PTM = 1` to `5`: `test.comp` found 0 of 22 files different and `nccmp-tol`
  passed on each; each Kokkos run made 2 `W3CPRT` Kokkos calls on 18 sea points, 0 fallbacks.
  The staged switch had `PR3 UQ` in place of the test's `PR0` ⚠: the fork's `KOKKOS` build does
  not compile with `PR0` (`w3kokkosmd.F90:3183` at `80644ba9` uses `W3ADATMD`'s PR3 arrays), so
  neither `ww3_tpt1.1` as written nor `ww3_tnc1` (the one serial `matrix.base` entry with partition
  fields, `PR0`) can be built with `KOKKOS`; both runs of each pair used the one executable. Two of
  the five entries first failed to link (`DSO missing from command line`, `libgfortran`) in a
  build directory left by an interrupted run; removing `build_903` and `build_905` fixed it ⚠.
- (v) A WW3 defect found on the way: the formatted partition file (`TYPE%PARTITION%FORMAT = T`)
  stops `ww3_shel` with a gfortran format error in the Fortran itself. Format 942 is
  `(I3,3F8.2,2F9.2,F7.2)` (`w3iosfmd.F90:739`) and `:706` writes `IP` and 15 values of
  `DTPRT(:,IP+IOFF)`; the format reverts to `I3` for a real (backtrace under `gdb`, Fortran run of
  the same executable). The runs above use `FORMAT = F`. Not changed: WW3 is read-only.
- ⚠ Nothing ran on a GPU, and the CUDA build of the new code was not compiled. The wall times the
  script prints are one run each on a shared machine and are not quoted.

## What it changes

`W3CPRT` and `W3OUTG`'s partition fields no longer copy the spectrum back: with `CPRT` on, an
output time with partitioning moves only the table and the fields written. Open: `CPRT` is not
in the `resident` preset (section 25 of PATCH.md), the fallbacks are not reported to
`WW_KOKKOS_STRICT` (section 24; this fork branch predates it), the regtest matrix has no
`KOKKOS`-buildable entry with partitioning, and the formatted partition file is broken in WW3
at the pin.
