# Strict mode sees `W3OUTG`'s Fortran fallback, stops only where a run names its parts, and the missed-sync poison is on by default

2026-10-18, an agent for M. Hoffmann (goal `abbb0456-8241`), the follow-up of PR #191: findings
1, 2 and 3 of its code review (`ce-code-review` run `20261018-154456-d3a4d3cf`), approved by the
owner the same day. [`PATCH.md`](../../kokkos/src/fortran_iface/PATCH.md) section 31,
[`PRODUCTION_READINESS_202610.md`](../PRODUCTION_READINESS_202610.md) gap 6.

## Question

Three gaps in what a pass of `scripts/ww3-gpu.sh` or `just ww3-gpu-matrix` means after #191:

1. `W3KOKKOS_OUTG` runs the Fortran `W3OUTG` whole for a field its translation leaves to the
   Fortran, without telling `WW_KOKKOS_STRICT` (finding 3). Does routing it through
   `KOKKOS_FALLBACK` stop a strict run and count a non-strict one, with nothing else changed?
2. `WW_KOKKOS_VA_POISON=1`, the detector of a host reader that misses its sync, was set by no
   script (finding 2). Can every Kokkos run have it without changing a correct run's bytes?
3. Under `WW3GPU_KOKKOS=all` the matrix set strict mode too, so every entry whose switch a part
   does not cover stopped in `W3INIT` (finding 1). Does an `all` entry without `PR3` run again
   when strict mode is set only for a run that names its parts?

## What was done

The owner's Intel Core i9-14900 (32 hardware threads, shared with other sessions: load average
about 12 at 17:45), the pinned `ww3` shell (gfortran and g++ 16.2.0), Kokkos' OpenMP backend, at
most 8 threads or jobs, never `--gpu`.

- **Fork, `ww3-gpu/strict-outg`** (h0ffmann/WW3), one commit `fac74069` on `50a10730`: only
  `model/src/w3kokkos_outgmd.F90` changes (PATCH.md section 31). `WW3/` is pinned to it in this
  change.
- **`scripts/ww3-gpu.sh`**: `va_poison` (`${WW_KOKKOS_VA_POISON:-1}`) goes into the Kokkos run's
  flags in `run_model`; the Fortran run unsets `WW_KOKKOS_VA_POISON`; `--self-test` checks both.
- **`regtests/cmake/ww3gpu_common.cmake`, `ww3gpu_kokkos_env`**: the K run gets
  `WW_KOKKOS_VA_POISON=1` (or `0` when `run_entry.cmake` read a caller's `0` before the runs) and
  `WW_KOKKOS_STRICT=1` only when its flags do not hold `ALL`; F and G unset both.
  `selftest.cmake` has the new expectations.
- **Branch names**: `.gitmodules` and the comments and messages that named older fork branches now
  name `ww3-gpu/kokkos-pnt`, which holds the pin.

Commands, in the order run (the outputs are in the session's scratch directory, not committed):

```bash
gfortran -E -P <switch_lab_shrd words as -DW3_<word>> model/src/*.F90   # at 50a10730 and fac74069
bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident          # JOBS=8 OMP_NUM_THREADS=8
bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident
# a scratch copy of build/runs/ww3-gpu-full-openmp/kokkos with TYPE%FIELD%LIST = 'HS SKW',
# ww3_shel with the resident parts, WW_KOKKOS_VA_POISON=1, WW_KOKKOS_OUTG_COUNT=1, with and
# without WW_KOKKOS_STRICT=1
WW3GPU_KOKKOS=all just ww3-gpu-matrix-list serial
WW3GPU_KOKKOS=all WW3GPU_SWITCH_WORDS="PR0;SHRD" WW3GPU_OMP_THREADS=8 just ww3-gpu-matrix 1 serial -R ww3_tnc1
WW3GPU_KOKKOS=resident WW3GPU_LAB_SWITCH=ON WW3GPU_OMP_THREADS=2 just ww3-gpu-matrix 4 serial
```

## Result

| Check | Result |
|---|---|
| `gfortran -E -P`, no `KOKKOS`, 143 files | 142 identical apart from blank lines; the other is `w3kokkos_outgmd.F90`, compiled only under `WW_KOKKOS` (v) |
| `smoke`, resident | every model file byte-identical to the Fortran of the same executable, every file but the NetCDF to `--ww3`'s; POISON fills 2161; `calls left on the Fortran: SRCE=0 CPRT=0 OUTG=37` (v) |
| `full`, resident | byte-identical to the Fortran and (but the NetCDF) to `--ww3`; POISON fills 97; `SRCE=0 CPRT=0 OUTG=0`, 13 `W3OUTG` Kokkos calls; ww3_shel 34.46 s Kokkos, 60.41 s Fortran, one run each, regtest pending ⚠ (v) |
| `full` copy with `SKW`, `WW_KOKKOS_STRICT=1` | exit 1 at the first output time: `... PART OUTG WOULD RUN ON THE FORTRAN: A SKEWNESS FIELD IS REQUESTED (FLOLOC(8,7:9), SKEWNESS), ...` (v) |
| `full` copy with `SKW`, strict unset, domain cut to 2 h | exit 0, `End of program`, `SRCE=0 CPRT=0 OUTG=3` (v) |
| `WW3GPU_KOKKOS=all just ww3-gpu-matrix-list serial` | 363 entries; 38 with `PR0;SHRD` (v) |
| `all`, `ww3_tnc1` (entry 38, `PR0 SHRD`) | runs and passes: test.comp 0/30, 10 of 16 parts on Kokkos, `calls left on the Fortran: MAP3=1 MAPT=1 SRCE=1`; before this change strict mode would have stopped it in `W3INIT` (v, run; the stop itself not re-run) |
| resident matrix, 4 serial jobs, merged with `origin/main` at `0cfb595`, pin `fac74069`: 5 of 7 entries PASS (`ww3_tp2.15` x4, `test.comp` 0 of 23, 23, 23, 25 files different, `nccmp-tol` PASS; `ww3_ts2:ST4_PR3_UQ` 0 of 22, no `nccmp-tol` reference), 2 FAIL by design (`ww3_tp2.6` `ST4` and `ST4_ASCII`: unstructured mesh, `W3INIT` stops on `XYP3`, `ran=fortran-only`; the owner keeps them FAIL); every passing Kokkos run ends `calls left on the Fortran: SRCE=0 CPRT=0 OUTG=0`; 729.7 s wall, load average 12 to 25 on the shared machine; the run asked for no `WBT` or `SKW`, so strict mode's new `W3OUTG` stop did not fire in it (v, `build/ww3-gpu-matrix-serial-kokkos-resident-lab/summary.tsv`) |

Two things the runs show beyond the question:

- The `smoke` case asks for `WBT` and `SKW`, so its `OUTG` has always taken the Fortran branch;
  37 output times fell back, now counted. A strict run of `smoke` with the resident preset would
  stop (not run ⚠).
- The Fortran `SKEWNESS` is slow on `full`: with `SKW` requested, about four output times took 24
  minutes on the shared machine, where the Kokkos run of `HS` alone takes 34 s for all thirteen.
  That twelve-hour run was stopped and the counted run repeated over two hours. Not profiled ⚠.

## What it changes

- A pass of the matrix under the resident preset now means `W3OUTG` ran on Kokkos at every output
  time, and every Kokkos pass of either tool ran with the stale host `VA` poisoned.
- `all` is a coverage run again: what it cannot put on Kokkos is counted in the exit line, not a
  stop.
- Units 2 and 3 change a gate's invocation, and unit 1 adds a stop: a person reviews them before
  the merge (AGENTS.md, "Never cheat a regtest tool").
- The matrix command, run in the worktree of this branch: `WW3GPU_KOKKOS=resident
  WW3GPU_LAB_SWITCH=ON WW3GPU_OMP_THREADS=2 just ww3-gpu-matrix 4 serial` (exit 8 from the two
  `ww3_tp2.6` entries only). Regtest verdict of this commit: the entries above; the full ADR-0016
  validation phase is still the owner's.
- Next: a supervised GPU run reads the POISON count with the others; `smoke`'s `WBT` and `SKW`
  stay on the Fortran until those fields are translated.
