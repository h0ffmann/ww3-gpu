# Risk runs for the GFS-Wave switch: step 1 done, steps 2 to 4 blocked

2026-10-19, Claude (agent), goal be5bdbea-3817, PR 206. Regtest pending ⚠ (ADR-0016): these are
diagnostics of this machine (Intel Core i9-14900, 32 threads), CPU only, never `--gpu`.

## Step 1: the lab switches after the gate change (risk 6)

`WW3GPU_SWITCH=<switch> WW_KOKKOS_STRICT=1 bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos SRCE`
at `c0021a3`, `WW3@5dbf6eba`, gfortran 16.2.0 (v, run 2026-10-19). The `full` case is
`kokkos/tests/fixtures/bench_small` (9 520 points, 12 h). The batched W3SRCE is the only part on Kokkos.

| Switch | Load before | Fortran vs Kokkos SRCE | `nccmp-tol` | Fallbacks | W3SRCE spectra |
|---|---|---|---|---|---|
| `switch_lab_ompg` (default) | 3.2 | every model file byte-identical (`out_grd.ww3`, `ww3.field.nc`); also to `--ww3` except the NetCDF | PASS (1 judged, 4 unlisted), both pairs | none (strict, run finished) | 230 100 in 25 batched calls |
| `switch_lab_gfs` | 24 (my previous job still ending) | same | PASS | none | 230 100 in 25 batched calls |

Wall times, one run each, not a benchmark: ompg 30.55 s Fortran, 23.61 s Kokkos SRCE, 31.82 s `--ww3`;
gfs 34.72 s, 25.30 s, 35.28 s. A first `full --cpu` with the default part (SNL1 only) was also
byte-identical and PASS, but took 196.45 s with Kokkos against 30.69 s for the Fortran at load 16 to 24;
that default-part figure is not explained ⚠.

## Finding: the matrix driver expands `$nth1` and `$npl` wrongly

`regtests/cmake/ww3gpu_common.cmake`, `ww3gpu_expand`, replaces `$nth` before `$nth1` and `$np`
before `$npl`, so `-n $npl -t $nth1` becomes `-n 8l -t 21` at 8 ranks and 2 threads (read in
`logs/338.F.log`: "run_cmake_test ... -n 8l -t 21"). Every ufs entry that uses `$npl`, `$npl1` or
`$nth1` therefore runs with 10 times the intended threads (`OMP_NUM_THREADS=21`, about 150 runnable
threads for 8 ranks). That explains the ~108 s per half hour and the 12 GB per rank seen in
`2026-10-19-gfs-wave-switch.md`. The driver was not changed (a human reviews changes to the
invocation); a fix is to expand `$npl1`, `$npl`, `$nth1` before `$np`/`$nth`.

## Step 2 to 4: not done

- `scripts/profile_case.sh` has a new `gfs12` case (shellcheck clean, `bash -n` clean, never run ⚠).
- A shortened entry 338 (stop 03:00, 8 ranks, 2 threads, `-FS ww3-gpu` to keep the cut deck) was
  started. It could not finish: my earlier killed launch left a second `run_entry.cmake` chain
  (PID 1119405) that moved on to the Kokkos run at 21 threads and oversubscribed the node, and the
  permission system refused my request to stop it. No `test.comp`, `nccmp-tol` or fallback count
  exists for ufs1.2, no `ww3_gfs12_gprof.tsv`, no ufs1.1 timing.
- `ctest -FS` takes the fixture name (`ww3-gpu`), not a test regex; `-FS '^setup:'` re-stages and
  overwrites an edited deck.
