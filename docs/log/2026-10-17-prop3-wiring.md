# W3XYP3 and W3KTP3 wired into WW3, batched, byte-identical on the CPU

2026-10-17, an agent (Lane C of issue #42's speed work), branch `goal/42-srce-prop3` (PR #173's
base `goal/42-srce-batch`), h0ffmann/WW3 `ww3-gpu/kokkos-prop3` `fcdfabef`.

## Question

Can the translated PR3 propagation of the lab switch (`W3XYP3` with `W3QCK3`, `W3KTP3` with
`W3QCK1` and `W3QCK2`) run inside WW3 with the Fortran's bits, and does running it in one call per
loop make WW3 faster than pure WW3 on the CPU?

## What was done

- The prop3 wiring unit ([PATCH.md section 11](../../kokkos/src/fortran_iface/PATCH.md#11-the-pr3-propagation-batched-on-ww3-gpukokkos-prop3)):
  `prop3_shim.cpp` (`ww_xyp3_batch`, `ww_ktp3_batch`), `w3kokkos_prop3md.F90` and three hunks in
  `w3wavemd.F90` on the fork. `W3XYP3` takes every spectral component of a step in one call, one
  work item per component; `W3KTP3` every sea point of an intra-spectral part, all its sub-steps
  in one work item. The flags `WW_KOKKOS_XYP3`, `WW_KOKKOS_KTP3` are opt-in (`kOptInOnly`).
- Runs on an Intel Core i9-14900 (32 threads, shared with other sessions, load average 40 to 50),
  Kokkos 5.2.2 OpenMP, gfortran/g++ 16.2.0, the pinned `ww3` shell, never `--gpu`:
  `bash scripts/ww3-gpu.sh --ww3-gpu full|smoke --cpu --kokkos XYP3,KTP3,SRCE [--timers]` with
  `switch_lab_ompg` and with `WW3GPU_SWITCH=switch_lab_shrd`; the full also with
  `WW_KOKKOS_PROP3_BATCH_MIB=64`; `--repeat 3` with `--kokkos SRCE`, `XYP3,KTP3,SRCE` and
  `XYP3,KTP3`; `--gpu --build-only`; `just kokkos-test serial-debug` and `openmp-release`.

## Result

- (v) Byte-identical on the first run, every run: `out_grd.ww3` and the NetCDF field equal (`cmp`)
  to the Fortran of the same executable, `out_grd.ww3` equal to `--ww3`, `nccmp-tol` PASS, on both
  switches and with the chunked buffers. Counters in the full: `W3XYP3` 13 824 spectra (24 calls),
  `W3KTP3` 441 792 point calls (48 calls). The smoke case (one point) calls neither.
- ⚠ Not exercised: `bench_small` has no current, no wavenumber shift (so `W3QCK2` never ran), no
  index closure, no boundary input.
- (v) Fenced breakdown (`--timers`): the time outside Kokkos fell from 19.35 s (`--kokkos SRCE`)
  to 2.89 s (`XYP3,KTP3,SRCE`); `xyp3.call` 1.80 s, `ktp3.call` 0.60 s.
- (v) Medians of three, `switch_lab_ompg`, a noisy machine ⚠: `--kokkos XYP3,KTP3` 27.32 s against
  `--ww3` 44.12 s (1.61x); `XYP3,KTP3,SRCE` 48.88 s against 41.40 s (0.85x); `SRCE` alone 66.36 s
  against 39.94 s (0.60x). The batched `W3SRCE` on the CPU is now the slow part.
- (v) `kokkos/PORT_STATUS.md`: the "ported" level went from 94.0 % to 99.3 % of the profile's run
  time (`python3 scripts/port_score.py --write`).

## What it changes

The rows of `W3XYP3`, `W3KTP3`, `W3QCK1`, `W3QCK2`, `W3QCK3` say `wired` yes; `regtested` stays no
until the regtest matrix runs them, with cases that have currents, a wavenumber shift, closure and
boundary input. The ABI version was not moved (`kokkos/CMakeLists.txt` 0.4.0, the fork asks for
0.4): an install from before this change fails at link, not at configure ⚠, left to the integrator
so this unit edits no shared file of the fork.
