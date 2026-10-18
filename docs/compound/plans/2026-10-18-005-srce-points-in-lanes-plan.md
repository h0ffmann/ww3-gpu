---
title: Sea points in the vector lanes of the batched W3SRCE - Plan
type: perf
date: 2026-10-18
artifact_contract: ce-unified-plan/v1
product_contract_source: "owner, session of 2026-10-18 (goal c3e7eecf-3917): item 1 of the opportunity ranking, in PR #197"
execution: code
---

# Sea points in the vector lanes of the batched `W3SRCE` - Plan

## Goal Capsule

- **Objective:** the per-point code of the resident `W3SRCE` batch runs `W` sea points at once,
  one point per vector lane, so its ordered sums and its scalar statements become vector
  statements, with every lane's arithmetic the Fortran's, in the Fortran's order.
- **Target:** the batched `W3SRCE` kernel, 12.45 s of the 14.36 s `full` run at 32 threads in the
  last committed profile (v, `docs/log/2026-10-18-resident-profile.md`, thread-scaling table, at
  `db2c27c`, before #193). Ranks 2 to 4 of that profile are the parts that run on one thread of
  the team inside `Kokkos::single` (`kokkos/src/ww_kokkos/srce_kernel.hpp:177`, `:268`, `:278`,
  `:397`, read 2026-10-18): `W3SDS4` 3.10 s, `W3SIN4` 1.41 s, the integration 1.12 s.
- **Authority:** the owner asked for it on 2026-10-18, in PR #197. It changes loop nesting, which
  AGENTS.md ("Porting") allows only with the owner's agreement; it changes no reduction order, no
  limiter and no integration order, so the "ask first, own PR" rule for those does not apply,
  and the owner chose to carry it in #197.
- **Stop conditions:** any output byte of `smoke` or `full` different from the Fortran of the same
  executable; any unit whose per-lane arithmetic would need a reordered sum, a fused
  multiply-add or a vector libm call whose bits are not shown equal to the scalar call's. Each
  stops the unit; none is solved by a tolerance.
- **Execution profile:** large. Unit S0 decides whether the rest is worth building.

## What is already true (read 2026-10-18 at `91fb315`, `WW3@50a10730`)

- (v) One team per sea point: `Kokkos::TeamPolicy<ExecSpace, Kokkos::Schedule<Kokkos::Dynamic>>
  policy(n, Kokkos::AUTO)` with one league entry per point (`kokkos/src/fortran_iface/srce_shim.cpp:430-433`,
  `:460`).
- (v) The DIA is the only team-parallel term: `snl1::snl1_team` (`srce_kernel.hpp`, section 2 of
  the loop). With #193's halo layout its two sections are contiguous loads over the 24 directions
  and g++ vectorised them with 32-byte vectors before the halo (profile log, "Inside the DIA").
  So the DIA gains little from points in lanes now: it is already a vector loop per point.
- (v) Everything else runs inside `Kokkos::single`: the initialisation, `W3SLN1`, `W3SIN4`,
  `W3SDS4`, `W3SDB1`, `W3SBT1` and the integration. Inside them, g++ left the hot sums scalar:
  `W3SIN4`'s stress loop "couldn't vectorize loop" (`st4_sin4.hpp:242`), `W3SDS4`'s
  mean-square-slope sums (profile log, "Inside `W3SDS4`").
- (v) Those loops call scalar libm per spectral bin. On the host `dm::powf`, `dm::expf` and the
  rest resolve to `Kokkos::pow`, `Kokkos::exp`, that is glibc's `libm.so.6`
  (`kokkos/src/ww_kokkos/libm_device.hpp:69-124`). `W3SIN4` has 4 `powf` and 4 `expf` sites,
  `W3SDS4` 7 `powf`, `W3SPR4` 4 `powf` (`grep -c dm::powf`).
- (v) Five ST4 loops follow gfortran's libmvec grouping, four directions per call from the first
  element (`st4_libmvec.hpp:1-10`). Their bits depend on that grouping, so a lane cannot hold a
  point inside them; they stay a loop over the lanes, each lane calling today's grouped code.
- (v) `W3SRCE`'s integration loop runs a different number of steps per point (`st.nsteps`,
  `st.done`, `srce_kernel.hpp:267-398`), so lanes finish at different times.

## The parity argument

A lane holds one sea point, its arrays stored point-minor in the team's scratch
(`x[is * W + lane]`). Every statement is executed for all `W` lanes, and lane `l` computes
exactly the statement the scalar code computes for point `l`:

1. **Same operations, same order, per lane.** A vector add of lane `l` is the IEEE add of the
   scalar code's two operands for that point; no operation combines two lanes. A sum over the
   spectrum stays a loop over `is` in the scalar order, with one accumulator per lane.
2. **No contraction.** The library keeps `-ffp-contract=off`, so a vector multiply and add are
   not fused, as the scalar ones are not.
3. **Branches become masks.** `if (c) x = a; else x = b;` becomes, per lane, the value the taken
   branch computes; the untaken branch's value is computed and discarded, never stored. A branch
   whose untaken side could trap or write memory (a division, an out-of-range index) keeps a
   guard so the discarded value is never read from outside the array ⚠: each such site is listed
   in its unit.
4. **Libm stays scalar per lane** unless a vector routine is shown to give the scalar call's bits
   on every input: today that is `expf`, `logf`, `sincosf`, `tanhf`, `log10f`, `coshf`, `sinhf`
   on all 2^32 inputs, and not yet `powf` or `atan2f` (v, `docs/GPU_LIBM_BITS_202610.md`, as
   read by the analysis of 2026-10-18). Using the vector copies on the host is the owner's
   decision (decision 2 of that analysis) and is not part of this plan's first units.
5. **Points that finish early** keep their state: a lane whose point has `done` set is masked off
   for every later statement of the loop, so its values are those the scalar loop left.

The check is the one AGENTS.md asks for: `smoke` and `full` byte-identical to the Fortran of the
same executable at 1 and 32 threads, then the resident regtest matrix of the U3 log in the
validation phase. Moving data into and out of the point-minor scratch is a copy, so it cannot
change a bit.

## Units

| Unit | What | Done when |
|---|---|---|
| S0 | One-thread profile of today's batch, libm separated from the rest | the shares below are measured; the go/no-go of S0 is applied |
| L1 | The lane layer: teams of `W` points (`WW_KOKKOS_SRCE_LANES`, 1 to 8, 1 reproduces today, default 8 since L2), each section run for the team's points in turn with today's scalar routines | **Done** 2026-10-18: `full` and `smoke` byte-identical with 1 and 8 (`kokkos/port_report/W3SRCE_KOKKOS.md`, "Points per team"). The point-minor scratch moves to L2, the first unit that reads it |
| L2 | `W3SIN4` on lanes (its stress loop, `st4_sin4.hpp:198-249`) | **Done** 2026-10-18: byte-identical on `smoke` and `full`; one-thread `W3SRCE` kernel 105.17 s to 99.77 s, 5.1 % ([log](../../log/2026-10-18-srce-lanes-sin4.md)) |
| L3 | `W3SDS4` on lanes, the libmvec sites left as lane loops | **Built** 2026-10-18: byte-identical on `smoke` and `full` with 1 and 8 points per team; timing and regtests in the [log](../../log/2026-10-18-srce-lanes-sds4.md) |
| L4 | The integration and the initialisation (`srce_kernel.hpp:177-400`) | the same |
| L5 | `W3SPR4`, `W3SLN1`, `W3SDB1`, `W3SBT1` | the same |
| L6 | The DIA reads its input from the point-minor layout, or the layer transposes around it, whichever the timing favours | the same |
| L7 | Median of three on an idle machine, `--repeat 3`, the record updated | `docs/NOAA_PROGRESS_RECORD_202610.md` section 6 has the row |

Every unit runs the gates of AGENTS.md for `kokkos/`: `just kokkos-test serial-debug` and
`openmp-release`, then `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident` and the
same on `full`. On a GPU the layer is a different question (lanes are threads there); `W` = 1
keeps the CUDA build as it is, and no device run is part of this plan.

## S0: where one thread of the batch spends its time

Run 2026-10-18 20:16 to 20:19 UTC, at `5cd0140` (`WW3@50a10730`, `switch_lab_ompg` plus
`KOKKOS`, the build of the `--repeat 3` timing), `full`, `OMP_NUM_THREADS=1`,
`WW_KOKKOS_PRESET=resident`, gdb stack sampling every 0.1 s (the method of the profile log, "How
to repeat"), load 9.0 to 10.1 from another session's jobs. The run's `out_grd.ww3` is
byte-identical to the timing run's Kokkos output (v, `cmp`). 1 602 samples, 1 439 inside
`ww_srce_batch`, each charged to its innermost frame (no `-g`, so a frame is a function the
compiler kept out of line):

| Innermost frame | Samples | Share of the batch | Lanes help? |
|---|---|---|---|
| `ww::snl1::snl1_team_halo` | 305 | 21.2 % | little: already a vector loop over directions |
| `ww::snl1::snl1_team` (the DIA outside the halo routine) | 256 | 17.8 % | little, as above ⚠ (not split further) |
| `ww::st4::w3sds4_point` | 357 | 24.8 % | yes |
| `ww::st4::w3sin4_point` | 186 | 12.9 % | yes |
| `ww::srce::w3srce_team` (initialisation, integration, `W3SLN1`, `W3SDB1`, `W3SBT1` inlined) | 142 | 9.9 % | yes |
| `ww::st4::w3spr4_point` | 50 | 3.5 % | yes |
| `libmvec` (`_ZGVbN4vv_powf_sse4`) | 70 | 4.9 % | no: the grouped libmvec sites stay |
| `libm` (`__powf_fma` 28, `atan2f` 17, `__expf_fma` 7, others 5) | 57 | 4.0 % | no, unless vector copies are approved |
| other (`memset`, `powf@plt`) | 11 | 0.8 % | |

- (v) The scalar per-point code that lanes can vectorise is 51.1 % of one thread's batch
  (`W3SDS4`, `W3SIN4`, `w3srce_team`, `W3SPR4`). libm and libmvec together are 8.9 %.
- (v) The DIA is 39.0 %, already vectorised over the 24 directions per point.
- ⚠ Projection, not a measurement: if lanes made the 51.1 % four times faster (eight lanes, with
  masks and the per-lane libm left scalar), one thread's batch would take about 62 % of today's
  time. The batch was 87 % of the 32-thread wall time in the committed profile.
- **Go.** The share lanes can reach is large and libm does not bound it. L1 and L2 next; L2's
  kernel timing replaces the projection.

The commands, `profile.sh` and `reduce.py`, are written out in
[`docs/log/2026-10-18-srce-lanes-s0-profile.md`](../../log/2026-10-18-srce-lanes-s0-profile.md),
"How to repeat"; they run no build and change no file of the repository.

## Risks

- **The gain is bounded by libm.** If most of the per-point time is in scalar `powf` and `expf`,
  lanes without vector libm remove only the rest. S0 measures it.
- **Masks cost work.** Lanes whose point took the other branch compute a discarded value; on the
  stress loop of `W3SIN4` the two branches are short, on the integration loop a finished point
  wastes its lane until the slowest point of the team finishes ⚠.
- **Scratch per team grows `W` times.** 18 arrays of `NSPEC` reals per point
  (`srce_work_reals`, `srce_kernel.hpp:102-106`): 18 x 576 x 4 B x 8 = 332 kB per team at
  `W` = 8, beyond a 48 kB L1 data cache and inside a 2 MB L2 ⚠ (sizes for the i9-14900's
  performance cores, not read from this machine).
- **Load balance.** Teams of 8 points give 1 190 teams for the 9 520 sea points of `full`, still
  far more than 32 threads.
