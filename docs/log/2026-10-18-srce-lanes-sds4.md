# `W3SDS4` with one sea point per vector lane: bit-identical, 19 % off the one-thread `W3SRCE` kernel after L2

2026-10-18, for M. Hoffmann (goal `5afd9379-4417`), unit L3 of
[the points-in-lanes plan](../compound/plans/2026-10-18-005-srce-points-in-lanes-plan.md),
after [L2](2026-10-18-srce-lanes-sin4.md) (PR #199).

## Question

Does `W3SDS4` (`w3src4md.F90:2043-2659` at `WW3@50a10730`, 24.8 % of the one-thread batch in
[S0](2026-10-18-srce-lanes-s0-profile.md)) give the Fortran's bits and take less time when eight
sea points run its loops at once, one per vector lane?

## What was done

- `w3sds4_point` (`kokkos/src/ww_kokkos/st4_sds4.hpp`) split into `sds4_prep` (sections 0 and 1,
  `:2203-2224`), `sds4_mss` (`:2228-2254`), `sds4_sat` (case 1, `:2262-2290`), `sds4_case1` (the
  rest of case 1, `:2291-2331`), `sds4_case2`, `sds4_case3`, `sds4_turb` (`:2533-2575`) and
  `sds4_whitecap` (`:2581-2650`). (v) Every line of the old body is in the new file, case for
  case, with the whitespace removed (a line-multiset comparison: only the `switch` scaffolding is
  not in a part, and it is in `w3sds4_point`); the case bodies are dedented. The arguments and the
  work arrays travel in `Sds4Args` and `Sds4Work`, the locals that cross sections in `Sds4Mid`.
- `w3sds4_lanes<8>` runs the parts for up to eight points. Three loops have a lane version:
  - `mss_lanes`: the three mean-square-slope sums, one accumulator per lane, ITH order; `powf` and
    `atan2f` per lane.
  - `sat_lanes`: the saturation integral, one accumulator per ITH and lane, added in J order from
    zero over all `2*SDSNTH+1` terms (the zero weights are multiplied, as the scalar code does);
    the sum of `A` and the `MAXVAL` per lane. Without the transposed tables each point goes
    through `sds4_sat`.
  - `turb_lanes`: the cheap isotropic sum and the default O(NK^2 NTH^2) branch (`:2554-2563`)
    with the branch on `BTH0` as a select; `COS/SIN(USDIR)` computed once per lane instead of once
    per ITH (the same call, same arguments, so the same bits).
  - The two libmvec sites of `sds4_case1` and `sds4_case3` and every other loop stay per point:
    each point makes today's grouped call.
- The vector types moved to `st4_vec8.hpp`, shared with `W3SIN4`.
- `w3srce_lanes` calls `w3sds4_lanes` once per step for the points still in the loop
  (`srce_kernel.hpp`, `sds4_lanes`); `step_c_main` is `sds4` then `step_c_after_sds4`, so the
  scalar path runs the same statements in the same order.

## Result

- (v) Bits, `WW_KOKKOS_STRICT=1 WW_KOKKOS_VA_POISON=1`, `WW_KOKKOS_SRCE_LANES=1` and `=8`:
  `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident` and the same on `full` give
  every model file byte-identical to the Fortran of the same executable (`smoke`: `out_grd.ww3`,
  `out_pnt.ww3`, `restart.ww3`, `ww3.196806.nc`; `full`: `out_grd.ww3`, `ww3.field.nc`), every
  file but the NetCDF byte-identical to `--ww3`'s, `nccmp-tol` PASS.
- (v) What `full` exercises, read from a temporary print (removed): case 1, `SSDSC(3)` = -0.40344
  (the cumulative branch of `:2554-2563`), `SSDSC(18)` = 400 (so section 1.b runs), `SDSDTH` = 80
  (the weighted branch), the transposed tables present. Not exercised by it: cases 2 and 3 (their
  parts are the old code called per point), `SSDSC(3) > 0`, `SDSDTH >= 180`, no transposed tables.
- (v) Those branches checked against the scalar path with a temporary hack (removed, not
  committed) that overwrote a parameter from an environment variable in both the scalar and the
  lane entry: `SSDSC(3)` = 0.4, `SDSDTH` = 200, case 2, case 3 and the sums of those. For each,
  `full`, `WW_KOKKOS_SRCE_LANES=1` and `=8`, `out_grd.ww3` byte-identical between the two (and
  different from the normal run, so the hack took effect). This compares the lane path with the
  scalar path of the same code, not with the Fortran, and these parameter values are not WW3's.
- (v) `just kokkos-test serial-debug` and `openmp-release`: 11/11 each, no warning.
- (v) `bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --kokkos resident --build-only`: compiles and
  links, no warning; nothing ran on a device. The lane code is host-only.
- (v) `-fopt-info-vec-all` on `srce_shim.cpp` (openmp-release flags, the pinned `g++` 16.2.0): the
  vector-type statements are vector instructions by construction; the plain loops inside the lane
  functions (gathers of `A` through eight pointers, the per-lane stores) are scalar, as written.
- (v) WW3's regtests, `just regtest-selected` at `958b7bc` plus this log's uncommitted edits
  (`WW3@50a10730`, resident, lab switch, strict, 6 threads per run): `ww3_tp2.15` default,
  `-g 5km`, `input_const`, `input_rho` and `ww3_ts2` `ST4_PR3_UQ` give `test.comp` 0/23, 0/23,
  0/23, 0/25, 0/22, `nccmp-tol` PASS, `smoke` byte-identical, 8/8 ctest tests passed.
- (v) Time: `srce.batch.kernel` (`WW_KOKKOS_TIMERS=1`), `full`, one thread pinned to a performance
  core (`taskset -c 2`), runs alternated, Intel Core i9-14900, 2026-10-18 22:52 to 23:02 UTC
  (the machine's clock), load read before each run:

| `WW_KOKKOS_SRCE_LANES` | Runs (s) | Median | Load before each run |
|---|---|---|---|
| 1 | 108.71, 104.99, 104.43 | 104.99 s | 7.85, 2.51, 1.44 |
| 8 | 83.39, 80.51, 80.55 | 80.55 s | 4.17, 2.06, 1.45 |

- (v) Every run's `out_grd.ww3` is byte-identical to the Fortran-matched output of the build.
- ⚠ 24.4 s of 105.0 s, 23.3 %, for L2 and L3 together; L2 alone was 5.1 % in its log (105.17 s to
  99.77 s), so L3's part is about 19 s (19 %) of the kernel, against 24.8 % of the batch that
  `W3SDS4` was in S0 ⚠ (the two logs ran on different builds and days; L2's lanes=8 figure was not
  repeated here). The first lanes=1 run began at load 7.85, and its 108.71 s is the slowest.
- ⚠ No 32-thread figure: not measured in this unit.

## Open questions for the owner

1. The host lane loops call the same glibc `libm` and `libmvec` as the scalar code, per lane. The
   transcribed device `libm` (`libm_device.hpp`) is not used on the host; whether the `powf`,
   `atan2f` and `sincosf` calls (libm and libmvec together were 8.9 % of the batch in S0) may use a
   vector `libm` is decision 2 of `docs/GPU_LIBM_BITS_202610.md`.
2. Zero weights of the saturation integral are multiplied, not skipped: 12 of the 23 products per
   direction in the default configuration multiply zero. Skipping them is not bit-neutral (0 x Inf
   is NaN) and would be its own PR with its own evidence.
3. The loop nesting: IK and ITH outside, the lane loop inside, one accumulator per lane and ITH,
   and the vector types (GCC vector extensions, not `std::simd`). Review asked.
4. `COS/SIN(USDIR)` is computed once per lane in `turb_lanes`, not per ITH as the Fortran text
   reads. It is a pure call on the same argument, so the bits are the same; say if "translate,
   don't improve" should keep the per-ITH call.

## How to repeat

`lanes_1t.sh` of [the L2 entry](2026-10-18-srce-lanes-sin4.md#how-to-repeat), with `repo` set to
the checkout, after `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident`.

## Rerun at the combined pin `fac74069`, 2026-10-18

Branch `goal/abbb0456-8241-strict-outg-poison` at `debb1b0`, `WW3/` at `fac74069` (strict mode
stops a run when `W3OUTG` falls back to the Fortran), Kokkos OpenMP backend, never `--gpu`. For
`WW_KOKKOS_SRCE_LANES` 1 and 8, `WW_KOKKOS_VA_POISON=1`, with and without `WW_KOKKOS_STRICT=1`:

```bash
WW_KOKKOS_SRCE_LANES=<1|8> WW_KOKKOS_VA_POISON=1 [WW_KOKKOS_STRICT=1] \
  bash scripts/ww3-gpu.sh --ww3-gpu <smoke|full> --cpu --kokkos resident
```

| Case | Strict | Lanes 1 and 8 |
|---|---|---|
| `smoke` | off | exit 0; every model file byte-identical to the Fortran of the same executable (`out_grd.ww3`, `out_pnt.ww3`, `restart.ww3`, `ww3.196806.nc`), every file but the NetCDF byte-identical to `--ww3`'s, `nccmp-tol` PASS |
| `smoke` | on | exit 1, the run stops in `W3OUTG` (below); no comparison was made |
| `full` | off | exit 0; `out_grd.ww3`, `ww3.field.nc` byte-identical to the Fortran's, every file but the NetCDF byte-identical to `--ww3`'s, `nccmp-tol` PASS |
| `full` | on | exit 0; the same as off |

(v) The stop, `smoke`, both lane counts:
`*** WAVEWATCH III ERROR IN W3KOKKOSMD : WW_KOKKOS_STRICT=1 AND PART OUTG WOULD RUN ON THE FORTRAN:
A SKEWNESS FIELD IS REQUESTED (FLOLOC(8,7:9), SKEWNESS), WHICH THE TRANSLATION LEAVES TO THE
FORTRAN W3OUTG`. This is the follow-up of `fac74069` doing what it was written to do: `smoke`
requests a skewness field, which `W3OUTG` leaves to the Fortran. The earlier byte-identical
`smoke` result under strict (above) was at `50a10730`, where strict did not stop on this. The bit
verdict for `smoke` at `fac74069` is the non-strict run; its fallback count printed at exit shows
no call left on the Fortran for `SRCE` and `CPRT` (`W3CPRT` fallbacks 0). `full` is unchanged by
the new pin. Also at the combined HEAD: `just kokkos-test serial-debug` and `openmp-release` exit
0; `just regtest-selected` passed; `bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --kokkos
resident --build-only` exit 0, nothing run on a device.
