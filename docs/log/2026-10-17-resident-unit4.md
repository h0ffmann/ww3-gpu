# The source-term inputs kept on the device (WFIP-0005 unit 4), and why unit 5 waits

2026-10-17, an agent for M. Hoffmann; issue #42, [WFIP-0005](../WFIPs/WFIP-0005-device-resident-step.md) units 4 and 5.

## Question

After unit 3b the spectrum `VA` crosses only at output times, but the batched `W3SRCE`
(`ww_srce_batch_va`) still copied its eight packed inputs to the device and five back at every
step. Can they stay on the device without listing every WW3 writer of `WN`, `CG`, the forcing and
the packed state, and without changing a bit on the CPU? And is unit 5 (`W3OUTG` on the device)
one unit of work?

## What was done

`kokkos/src/fortran_iface/srce_shim.cpp` only, on `goal/42-gpu-step-units45` from `main` `5eca265`;
the fork pin (`90631fad`) and the ABI (0.6) unchanged. On Serial and OpenMP the kernel reads and
writes the packed arrays in place; on a device each input is copied only when its bits differ from
a host shadow of its buffer (`memcmp`), the inout ones shadowed after the copy back
(`kokkos/src/fortran_iface/PATCH.md` §14). A diagnostic `WW_KOKKOS_SRCE_STAGED=1` runs the device's
copy-and-skip on the host. Runs: `bash scripts/ww3-gpu.sh --ww3-gpu full|smoke --cpu --kokkos
resident` and `smoke --kokkos all`, `WW3GPU_SWITCH` unset (`switch_lab_ompg`) and
`switch_lab_shrd`, plain, with `WW_KOKKOS_VA_RESIDENT=1` and with `WW_KOKKOS_SRCE_STAGED=1`, then
`cmp` of `out_grd.ww3`, the NetCDF, `out_pnt.ww3` and `restart.ww3` against the Fortran of the same
executable and against `--ww3`. Machine: Intel Core i9-14900, 32 threads, shared with other
sessions; gfortran/g++ 16.2.0, Kokkos 5.2.2 OpenMP. Never `--gpu` except `--build-only`.

## Result

- (v) All 15 runs byte-identical both ways (the NetCDF against `--ww3` differs only in
  `WAVEWATCH_III_switches`), `nccmp-tol` PASS.
- (v) Source-term inputs, `WW_KOKKOS_SRCE_COUNT=1`, full case: 3 129 360 B H2D at the first
  step and none after (192 of 192 later inputs unchanged), where every step copied 3 129 360 B;
  1 104 480 B per step still come back. Smoke: 41 092 B in 1 452 copies where 942 196 B crossed;
  `BPIN` changed on 1 441 of 2 161 steps and `BPSC` on 5, so the host does rewrite them and the
  comparison catches it. On the CPU these are projections for a device ⚠.
- `VA` traffic unchanged: H2D 1, D2H 13 on the full case.
- `--timers`, full, one fenced run each: `srce.batch.h2d`/`.d2h` 0.035/0.024 s before, 0.010/0.000 s
  after; on the CPU the gain is a host `memcpy`, a few hundredths of a second ⚠.
- Unit 5 was not started. `iogo_w3outg.hpp` is translated and never run; wiring it needs a shim
  for about 90 arrays, a Fortran fallback for `CALC_U3STOKES`, `CALC_WBT` and the partition
  fields, a read of the `switch_lab_ompg` object (the translation was checked against
  `switch_lab_shrd`'s, and `w3iogomd.F90:1730` is an `!$OMP PARALLEL DO` over the loop whose
  libmvec split it reproduces), an audit of the host readers of its fields (`W3IORS` writes some
  under `OARST`, `w3iorsmd.F90:1044-1060`), and the output-block read of `w3wavemd.F90:2650` split
  per consumer. The list is in row 5 of `WFIP-0005.tasks.md`.

## What it changes

Unit 4 is done on the CPU: the per-step H2D of the source terms is the forcing that changed. The
next device traffic to remove is the 1.10 MB per step of `BPSC`, `BWC`, `BTB`, `BTI`, `BSH` coming
back, which needs their host readers (the output and `W3SRCE`'s next pack) on the device, and the
`CFLTHMAX`/`CFLKMAX` copies of `W3KTP3` (PATCH.md §13). Unit 5 is its own lane.
