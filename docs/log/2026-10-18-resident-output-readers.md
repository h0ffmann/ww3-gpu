# 2026-10-18: the boundary and track outputs read only their columns of the resident `VA`

Goal `abbb0456-6043`, unit U8 of the production single-node plan
([`docs/compound/plans/2026-10-18-002-production-single-node-plan.md`](../compound/plans/2026-10-18-002-production-single-node-plan.md)).
Fork branch `ww3-gpu/u8-out-readers` of h0ffmann/WW3: `31a09522`, `791e8c34` on `80644ba9`.
The change is [`PATCH.md` section 23](../../kokkos/src/fortran_iface/PATCH.md#23-the-boundary-and-track-outputs-read-only-their-columns-ww3-gpuu8-out-readers).

## What was tried

The output block of `W3WAVE` copied the whole device `VA` to the host before each of `W3IOTR`,
`W3IOBC ('WRITE')`, `W3IOSF` and the OASIS coupling (`w3wavemd.F90:2934-2975` at `80644ba9`).
Read in the Fortran at the pin:

- `W3IOBC ('WRITE')` reads the columns `ISBPO(1:NBO2(NFBPO))` (`w3iobcmd.F90:660-668`): now a
  column read.
- `W3IOTR` reads `MAPFS(IY,IX)` for the points `MASK1` marks with `MAPSTA > 0`
  (`w3iotrmd.F90:728-819`), known only after it reads the track file: now a column read inside
  `W3IOTR`, before its section 4.
- `W3IOSF` partitions every sea point with `MAPSTA >= 0` (`w3iosfmd.F90:243-265`): the full read
  stays. Partitioning on the device belongs to the `W3CPRT` unit.
- The OASIS coupling reads no `VA` (v, `grep` of `w3agcmmd.F90`, `w3ogcmmd.F90`,
  `w3igcmmd.F90`, `w3oacpmd.F90`): its sync is removed; not compiled here ⚠.

The new column reads are under `WW_KOKKOS_IOPE=1`, the opt-in flag of the point output's column
read.

## Commands

The superproject at `96ef800` plus this change, `WW3/` at the fork commit named, on an Intel Core
i9-14900 shared with other sessions, `OMP_NUM_THREADS=4`, never `--gpu`:

```bash
# the matrix entry with track output, before (WW3 at 80644ba9) and after (791e8c34)
nix develop ./picasso/labs/pratico#ww3 --command cmake -S regtests -B build/m-after \
  -DWW3GPU_SELECTION=serial -DWW3GPU_OMP_THREADS=4 -DWW3GPU_DATA=<ww3-regtest-data> \
  -DWW3GPU_KOKKOS=resident -DWW3GPU_SWITCH_WORDS='PR3;UQ;SHRD;!PDLIB;!MGP;!MGG' \
  -DWW3GPU_LAB_SWITCH=ON -DWW3GPU_STRICT=ON
WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_COUNT=1 \
  nix develop ./picasso/labs/pratico#ww3 --command ctest --test-dir build/m-after -R tp2.2
# the entry with boundary output, through run_entry.cmake (see "Not resolved"), with a copy of
# build/m-after/ww3gpu_config.cmake whose WW3GPU_KOKKOS is SRCE,OUTG,IOPE,UPDV
WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_COUNT=1 nix develop ./picasso/labs/pratico#ww3 --command \
  cmake -DCONFIG=build/m-after/ww3gpu_config_tr1.cmake -DN=901 -DTEST=ww3_tr1 \
  '-DOPTS=-i input_std -w work_std' -P regtests/cmake/run_entry.cmake
# pair to pair with --ww3
WW_KOKKOS_VA_COUNT=1 bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident
WW_KOKKOS_VA_COUNT=1 bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident
```

## What it showed

| Entry | test.comp after | `VA` before | `VA` after |
|---|---|---|---|
| `ww3_tp2.2 -s PR3_UQ`, resident, track every 4 h | 0/24 different, `track_o.ww3` and `ww3.202206_trck.nc` identical (v) | D2H 4 (60.07 MB), host reads 8, partial reads 3 | D2H 4 (60.07 MB), host reads 4, partial reads 7 (3 track copies, 5 columns, 4.4 kB) |
| `ww3_tr1 -i input_std`, `SRCE,OUTG,IOPE,UPDV`, boundary every 1 h | 0/23 different, `nest1.ww3` to `nest3.ww3` identical (v) | D2H 7 (0.57 MB), host reads 8, writes 6, partial reads 7 | D2H 7 (0.57 MB), host reads 1, writes 6, partial reads 14 (7 boundary copies of 8 columns, 70 kB) |
| `smoke`, `full`, resident | byte-identical to the Fortran and to `--ww3`, `nccmp-tol` PASS (v) | D2H 37, 1 (section 17) | D2H 37, 1 |

With `WW_KOKKOS_VA_POISON=1` the stale host `VA` is NaN after every device write, so a column a
reader needs and the list missed would show in its output file; none did. The whole-spectrum
copies per run did not drop in either entry: the next full sync of the same step copies `VA`
anyway (in `ww3_tp2.2` the exit read of `W3WAVE` at the wind times, which are the track times; in
`ww3_tr1` the host write before the `PR1` Fortran propagation at every step). What changed is
what each output reader copies: the columns it reads, not the whole spectrum.

## Found on the way

- No KOKKOS switch without `PR3` compiled: `W3KOKKOS_MAP3` and `W3KOKKOS_MAPT` named
  `W3ADATMD` variables declared only under `W3_PR3` (`w3adatmd.F90:520-525`, `:648-653`). Fixed in
  `31a09522`; `ww3_tr1` (`SHRD RTD PR1`) builds with it (v).
- `W3KOKKOS_IOTR_READ` writes `VA` while `W3IOTR` holds it as `INTENT(IN)`; the `-O3` dump loads
  nothing of it before the call (v, `PATCH.md` section 23), which is the compiler's code, not a
  guarantee of the standard ⚠.

## Not resolved

- The matrix driver drops the empty options of a first-kept entry: `ww3gpu_select_entries`
  (`regtests/cmake/ww3gpu_common.cmake:337-343`) appends `""` to an empty list, which adds nothing,
  so `E_OPTS` is one shorter than `E_TESTS` and the configure stops ("list index: 3 out of range")
  for the `SHRD;RTD;!PDLIB` selection, whose first entry is `ww3_tr1` with no options. Not fixed:
  `regtests/` changes need a person's review ⚠.
- No runnable entry writes boundary output with `PR3`: `ww3_tp2.17` is `MPI`, and `ww3_tp2.6` and
  `ww3_tp2.7` write it once, at the start, before the device holds newer bits ⚠.
- `W3IOSF` keeps its full read until partitioning runs on the device.
