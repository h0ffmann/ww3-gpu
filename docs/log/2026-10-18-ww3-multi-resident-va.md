# `ww3_multi` and the resident `VA`: the defect reproduced, column syncs at the exchanges, no read at `W3WAVE`'s return

2026-10-18, an agent for M. Hoffmann (goal `abbb0456-6043`); unit U1 + U7 of
[the production single-node plan](../compound/plans/2026-10-18-002-production-single-node-plan.md),
gaps 2 and 13 of [`PRODUCTION_READINESS_202610.md`](../PRODUCTION_READINESS_202610.md),
[WFIP-0005](../WFIPs/WFIP-0005-device-resident-step.md). The fork change is
[`PATCH.md` section 26](../../kokkos/src/fortran_iface/PATCH.md#26-ww3_multi-and-the-resident-va-column-syncs-no-read-at-w3waves-return-ww3-gpuu1-multi-va).

## Question

Does `ww3_multi` change a grid's `VA` on the host between two `W3WAVE` calls behind the resident
device copy, as gap 2 read from the code? If so, can the syncs sit where `ww3_multi` reads and
writes `VA`, moving only those columns, with the whole-array read at `W3WAVE`'s return removed,
and still give the Fortran's bytes?

## What was done

On the owner's Intel Core i9-14900 (32 hardware threads, shared with three other sessions; at
most 8 threads used: 2 matrix jobs of 4), in the pinned `ww3` shell (gfortran and g++ 16.2.0),
Kokkos OpenMP backend, never `--gpu`.

1. Before, `WW3/` at `80644ba9`: the `ww3_multi` entries that are `SHRD` with `PR3 UQ`, with the
   lab switch's source terms, the resident preset and the VA diagnostics in the environment:

   ```bash
   WW_KOKKOS_VA_CHECK=1 WW_KOKKOS_VA_COUNT=1 WW3GPU_OMP_THREADS=4 WW3GPU_LAB_SWITCH=ON \
     WW3GPU_KOKKOS=resident WW3GPU_SWITCH_WORDS='PR3,UQ,SHRD,!PDLIB,!MGP,!MGG' \
     just ww3-gpu-matrix 2 serial -R '^0(33|34|38|43|61):'
   ```

   then the same without `WW_KOKKOS_VA_CHECK`, and the moving-grid entries of `mww3_test_05`,
   where `XYP3` and `KTP3` refuse (`w3kokkos_prop3md.F90:123-133`): `WW3GPU_KOKKOS=SRCE,OUTG,IOPE,UPDV
   WW3GPU_SWITCH_WORDS='PR3,UQ,SHRD,MGP,!PDLIB' just ww3-gpu-matrix 2 omp -R '^00(2|8):'`, with
   and without the check. The task's subset `SRCE,KTP3,OUTG,IOPE,UPDV` stops in `W3INIT` there,
   `KTP3` refusing the moving grid (v, `WW_KOKKOS_KTP3=1, BUT ITS KOKKOS TRANSLATION DOES NOT COVER
   THIS SWITCH FILE`), so `KTP3` was dropped from it. The before runs used `kokkos/` with the new
   entry point already added; `80644ba9` does not call it.
2. The change, on the fork branch `ww3-gpu/u1-multi-va` (`3bbd1557` to `f330dc53`), and the shim's
   `ww_va_host_wrote_cols` here (`PATCH.md` section 26). A first plan (B1) marked the host newer at
   `W3WAVE`'s return; the owner rejected it the same day, since on a GPU it adds a whole copy of
   `VA` per grid per call, and asked that the read at the return go too.
3. After, `WW3/` at `8f6ba104`, whose tree equals `f330dc53` (`diff --stat` empty): the same
   entries with `WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_CHECK=1` and plain, all with
   `WW_KOKKOS_VA_COUNT=1` (the script `run_set.sh` of the session, which only sets these
   variables around the two commands above).
4. `ww3_shel`, before and after: `WW_KOKKOS_VA_COUNT=1 OMP_NUM_THREADS=8 JOBS=8 bash
   scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident`, and after also `smoke`, and both with
   `WW_KOKKOS_VA_POISON=1 WW_KOKKOS_VA_CHECK=1`.
5. Without `KOKKOS`: `gfortran -E -P` of `w3wavemd.F90`, `wminiomd.F90`, `wmwavemd.F90` of the
   fork and of `WW3-noaa/` with every word of `switches/switch_lab_shrd`, then of
   `mww3_test_08`'s `switch_ST4_PR3_UQ_MPI`, blank lines dropped. With `KOKKOS` and that MPI
   switch: `cmake -S <fork> -B build/mpi_check/b -DSWITCH=<switch plus KOKKOS> -DWW_KOKKOS=ON
   -DCMAKE_PREFIX_PATH=kokkos/build/openmp-release/install` and `cmake --build build/mpi_check/b
   -j 8 --target ww3_multi`.
6. `just kokkos-test serial-debug`; `bash scripts/ww3-gpu.sh --self-test`; `shellcheck
   scripts/ww3-gpu.sh`.

The summaries are in
[`ww3gpu_matrix_multi_va_2026-10-18.tsv`](../data/ww3gpu_matrix_multi_va_2026-10-18.tsv).

## Result

- (v) Gap 2 is real. Before, with `WW_KOKKOS_VA_CHECK=1`, `mww3_test_02`, `_03` and `_04`
  (`grdset_a`) stop: "2 columns of the host VA of grid 1 changed since the last sync, and no
  W3KOKKOS_VA_HOST_WRITE declared it" at a kernel's sync (02), 110 columns of grid 1 at a host read
  (03), 240 columns of grid 2 at a kernel's sync (04). Without the check they run and differ:
  `test.comp` 5/23, 10/34 and 4/25 files different, `ww3_self` 0 each (a Fortran/Fortran run
  gives the same bits), `nccmp-tol` FAIL; in 02 `out_grd.coarse`, `out_pnt.coarse`, the
  spectra, track and field NetCDF. `mww3_test_01` (one grid) passes, 0/22.
- (v) On the moving grid of `mww3_test_05` (`SRCE,OUTG,IOPE,UPDV`), the check stops both entries
  (138 columns of grid 2, at a host write), and without it both pass, 0/27: the host write before
  the Fortran propagation declares the host newer before any kernel reads `d_va`, so the defect is
  hidden there, as gap 2 said it might be.
- (v) After: all six entries give `test.comp` 0 files different, plain, with the check, and with
  the host copy poisoned, every part shown as `=K` in the parts line.
- (v) Whole-`VA` copies device to host per run, before → after (`WW_KOKKOS_VA_COUNT=1`, plain runs):
  `mww3_test_01` 24 → 12, `_02` 384 → 8, `_03` 216 → 216, `_04` 864 → 0, `_05` 156 → 156; none
  went up. In their place the exchanges moved columns: in `_04`, 384 372 columns read
  (737.99 MB) and 71 388 stored (137.07 MB) over the run, against 3 002.99 MB of whole copies
  before. The 216 of `_03` are the `W3OUTG` fallback inside `W3WAVE` (0 Kokkos `W3OUTG` calls in
  that run), and the 156 of `_05` the Fortran propagation's host write: both inside `W3WAVE`.
- (v) `ww3_shel` full, after, plain and poisoned: every model file byte-identical to the Fortran's,
  every file but the NetCDF to `--ww3`'s, `nccmp-tol` PASS; whole-`VA` D2H 1 → 0 (21.21 MB less
  per run), H2D 1 → 1 (the bind). Smoke: the same verdicts, D2H 37 → 37 (the output readers of
  `PATCH.md` section 23). The run ends with the device copy newer; `scripts/ww3-gpu.sh` failed on
  that line, as mutant A2 of the mutation audit had made it, and now prints it.
- ⚠ Wall time of full, one run each on a shared machine: 40.86 s before, 43.99 s and 32.35 s after
  (the second run poisoned). The extra column copies of `ww3_multi` were not timed; no conclusion.
- (v) Without `KOKKOS`, the three files preprocess to `WW3-noaa`'s text with both switches (the
  include path in one comment aside). With `KOKKOS` and the MPI switch, `ww3_multi` builds; it
  was not run (ADR-0017). `just kokkos-test serial-debug`: 11/11; the self-test and shellcheck
  pass.
- (v) `mww3_test_07` was not compared: with the lab switch its `ww3_grid` stops in the Fortran
  run too, reading its `SDS4` namelist.

**`mww3_test_08`** (`ww3_multi`, `MPI`, `SRCE,KTP3`), read and not run (ADR-0017). Its
`ST4_PR3_UQ_MPI` entry uses `input/ww3_multi_grdset_a.inp`: grids `outer` rank 1 and `inner`
rank 2 in one group, so the inner grid feeds the outer one (`WMIOHS` reads the inner `VA`,
`WMIOHG` writes the outer `VA`) and the outer grid sends boundary spectra to the inner one
(`WMIOBS`). Under `DIST MPI` the batched `W3SRCE` and `W3KTP3` run on the device copy and
`XYP3` does not (v, `docs/log/2026-10-18-resident-mpi-lab-switch.md`). At `80644ba9`: `W3WAVE`
of the outer grid returns with "both equal", `WMIOHG` writes its overlap columns on the host, and
in the next step `W3KTP3` on the device reads `d_va` without them; the host write before the
Fortran `W3XYP3` (`w3wavemd.F90:2037`) then copies `d_va` back over the host and the nesting
update is lost. The inner grid's boundary, taken from those outer columns by `WMIOBS`, differs.
This is the defect fixed here, and it matches the six differing files there, all of the inner
grid ⚠: why the outer grid's own outputs did not differ (its overlap points are masked by
`FLGHG1`/`FLGHG2`) was not traced, and the entry was not run with the fix.

## What it changes

- Gap 2's blocker for `ww3_multi` under `SHRD` is closed on these entries; the fork branch
  `ww3-gpu/u1-multi-va` waits for the integrator's pin bump. `ww_kokkos` is 0.10.0.
- `mww3_test_08` is the regtest that would show the same fix under MPI, when the owner runs MPI.
- Left inside `W3WAVE`, for the output-reader units: the `W3OUTG` fallback's whole read per step
  (`mww3_test_03`), and the Fortran propagation's host write per step when `XYP3` refuses.
- The change to `scripts/ww3-gpu.sh` turns a gate's failure into a printed line; a person reviews
  it before it merges (`AGENTS.md`, "Never cheat a regtest tool").
