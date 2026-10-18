# Where the Kokkos resident run of `full` spends its wall time on the CPU

2026-10-18, an agent for the owner. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42),
[WFIP-0005](../WFIPs/WFIP-0005-device-resident-step.md) (the resident preset). Raw numbers:
[`docs/data/ww3gpu_resident_perf_2026-10-18.tsv`](../data/ww3gpu_resident_perf_2026-10-18.tsv).

## Question

On the owner's i9-14900, `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident` ran
`ww3_shel` in 15.5 s with the resident preset (`SRCE`, `XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV` on
Kokkos' OpenMP backend), 23.4 s with the Fortran of the same executable and 24.8 s as pure WW3
(`--ww3`), one run each on a loaded machine ⚠ (reported by the owner's session, not repeated
here). The committed gprof profile (`docs/data/ww3_full_gprof.tsv`) is of the Fortran. Where do
the seconds of the Kokkos run go now, and what is the next bottleneck?

## What was done

- **Machine**: Intel Core i9-14900, 24 cores and 32 hardware threads (`lscpu`, v); that these are
  8 performance cores with two threads each and 16 efficiency cores is Intel's description of the
  part, not read on the machine ⚠. Shared with a regtest matrix of another session: the 1-minute
  load average before each run is in the raw table, 3.5 to 35, so **every number below is one
  run on a shared machine** ⚠.
- **Build**: branch `goal/b8e23abb-5182-resident-profile` from `db2c27c`, `WW3/` at `80644ba9`,
  `switches/switch_lab_ompg` plus `KOKKOS`, built by
  `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident --build-only`. The Kokkos package
  was then reconfigured with `-DCMAKE_CXX_FLAGS="-O3 -march=x86-64-v3 -g"` (the preset's flags plus
  `-g`, so gdb names inlined frames) and `ww3_shel` relinked. GCC's `-g` is designed not to change
  generated code; that this binary's code equals the preset's was not compared ⚠. Every run below
  wrote an `out_grd.ww3` that `cmp` finds identical to the Fortran run's in
  `build/runs/ww3-gpu-full-openmp/fortran/` (v).
- **Runs**: the case directory `build/runs/ww3-gpu-full-openmp/kokkos/` (made by `ww3_grid`) copied
  to a scratch directory, and `ww3_shel` run there with the environment `ww3-gpu.sh` gives the
  resident preset:
  `OMP_PROC_BIND=false WW_KOKKOS_ALL=0 WW_KOKKOS_SRCE=1 WW_KOKKOS_XYP3=1 WW_KOKKOS_KTP3=1 WW_KOKKOS_OUTG=1 WW_KOKKOS_IOPE=1 WW_KOKKOS_UPDV=1 ww3_shel`,
  plus `WW_KOKKOS_TIMERS=1` for the per-part times (the shim's fenced regions,
  `kokkos/src/fortran_iface/timers.hpp`) and `OMP_NUM_THREADS` at 32, 16, 8 and 1. The Fortran of
  the same executable ran the same way with the six part flags at 0.
- **Sampling**: `perf` refuses every event here (`perf_event_paranoid` is 4; `perf stat -e
  task-clock true` fails with "Disallow CPU event access"), and `sudo` needs a password. Stacks were
  sampled with gdb instead (gdb stack sampling, script at the end): `ww3_shel` runs as gdb's child,
  gets `SIGUSR2` after every 0.1 s (run `pmp-32`) or 0.05 s (run `pmp-32b`) of run time, and gdb
  prints the stack of all 32 threads; 367 samples in all. The main thread's stack says which
  region the wall time is in; every thread's innermost frame of this repository's kernels (or
  `libgomp`, idle) says which term the thread time is in. A helper frame (Kokkos accessors, `libm`,
  `libmvec`, `st4_libmvec.hpp`) is charged to its caller.
- **Vectorisation**: the kernel's translation unit `srce_shim.cpp` compiled again with the build's
  flags and `-fopt-info-vec-all`, to see which hot loops g++ vectorised.

## Result

### The ranked costs, 32 threads

Seconds are the shim's timers in run `k-t32b`, the quietest (load 3.5 before it; wall 14.36 s,
against the owner's 15.5 s). The split of the `W3SRCE` kernel among its terms is the share of
thread samples in the two sampled runs (321 samples with the main thread in the kernel, 10 272
thread samples) applied to that run's kernel time, so those rows are derived, not timed ⚠. The
two sampled runs agree on every term's share within 1.6 points (DIA 52.2 % and 50.6 %).

| # | Cost | Seconds | Share of wall | Where |
|---|---|---|---|---|
| 1 | `W3SNL1` (DIA) inside the batched `W3SRCE` | 6.37 | 44.4 % | `kokkos/src/ww_kokkos/snl1_team.hpp:228-290`, from `WW3/model/src/w3snl1md.F90:378-440` |
| 2 | `W3SDS4` inside `W3SRCE` | 3.10 | 21.6 % | `kokkos/src/ww_kokkos/st4_sds4.hpp`, from `WW3/model/src/w3src4md.F90:2043-2659` |
| 3 | `W3SIN4` inside `W3SRCE` | 1.41 | 9.8 % | `kokkos/src/ww_kokkos/st4_sin4.hpp`, from `WW3/model/src/w3src4md.F90:423-1003` |
| 4 | `W3SRCE`'s own integration (time step, update, fluxes) | 1.12 | 7.8 % | `kokkos/src/ww_kokkos/srce_kernel.hpp` from `:266`, from `WW3/model/src/w3srcemd.F90:1210-2042` |
| 5 | `W3XYP3` (spatial propagation, batched) | 1.09 | 7.6 % | `kokkos/src/fortran_iface/prop3_shim.cpp:531-552`, from `WW3/model/src/w3pro3md.F90:641-1477` |
| 6 | `W3KTP3` (spectral propagation) | 0.39 | 2.7 % | `kokkos/src/ww_kokkos/prop_ktp3.hpp`, from `WW3/model/src/w3pro3md.F90:1519-1961` |
| 7 | `W3SPR4` inside `W3SRCE` | 0.30 | 2.1 % | `kokkos/src/ww_kokkos/st4_spr4.hpp`, from `WW3/model/src/w3src4md.F90:142-384` |
| 8 | everything outside Kokkos: start, `W3INIT`, `W3WAVE`'s Fortran, `W3IOGO`, exit | 0.22 | 1.5 % | wall minus the `.call` regions |
| 9 | `W3OUTG` (13 output times) | 0.19 | 1.3 % | `kokkos/src/ww_kokkos/iogo_w3outg.hpp` |
| 10 | threads idle inside the `W3SRCE` launch | 0.14 | 1.0 % | dynamic schedule, one point per steal (`srce_shim.cpp:430-441`) |
| 11 | copies of `VA` (one H2D, one D2H; on OpenMP they are host copies) | 0.002 | 0.0 % | `va.h2d`, `va.d2h` |

The five kinds of cost the question named:

- **Kokkos kernels**: 14.12 s of 14.36 s (98.3 %): `srce.batch.kernel` 12.45 s, `xyp3.kernel`
  1.09 s, `ktp3.kernel` 0.39 s, `outg.kernel` 0.19 s (v, `k-t32b` timers).
- **Host-device copies and syncs**: 0.002 s (`va.h2d` 1 call, `va.d2h` 1 call); the batch inputs
  of `W3SRCE` are not copied on the host backend (`srce.batch.h2d`, 25 calls, under 0.0001 s)
  (v). The traffic line's "device syncs 110" are calls that found `VA` current and copied
  nothing (H2D 1, D2H 1 in the same line) (v).
- **Fortran left in `W3WAVE`, I/O and setup**: 0.22 s together (wall minus the `.call` regions);
  the main thread was outside every Kokkos part in 4 of 367 samples (v). `W3IOGO`, `W3INIT`'s
  reading of `mod_def.ww3` (27 MB) and the driver loops are each below what this sampler resolves.
- **OpenMP idle time**: inside `W3SRCE`, 1.1 % of thread samples are in `libgomp`; inside
  `W3XYP3` 57 % (27 samples), `W3KTP3` 44 % (9 samples) and `W3OUTG` 50 % (6 samples) ⚠ (few
  samples). In `W3XYP3` the busy threads per sample range from 0 to 32, so its 576 components
  finish unevenly. A thread that the operating system set aside for another process still shows
  its own stack, so the other session's load is not in "idle": it is spread over the busy rows.
- **One thread against the Fortran**: at `OMP_NUM_THREADS=1` the Kokkos run took 174.78 s and the
  Fortran of the same executable 174.27 s (one run each ⚠). The per-thread cost of the ported code
  is the Fortran's; what the resident preset gains at 32 threads comes from how the work is
  spread over threads (one Kokkos launch per step over every sea point, the propagation over
  every component), not from faster per-point code.

Inside the DIA, the two data-parallel sections share it evenly: section 3 (the interactions,
`snl1_team.hpp:228-268`, `w3snl1md.F90:378-414`) 46 % of its samples, section 4 (source and
diagonal, `:270-290`, `w3snl1md.F90:416-440`) 50 %, sections 1 and 2 4 %. g++ vectorised both
with 32-byte vectors ("loop vectorized using 32 byte vectors" at `snl1_team.hpp:230` and `:271`):
the host SIMD path (`WW_SNL1_HOST_SIMD`, `snl1_team.hpp:53`) is on, and most of its loads go
through the index tables `IP`, `IM`, `IC`, so they are gathers.

Inside `W3SDS4`, 52 % of the samples are the saturation integral `BTH(IS) =
DOT_PRODUCT(SATWEIGHTS(:,ITH), A(IS0+SATINDICES(:,ITH)))*FACSAT` (`w3src4md.F90:2281-2285`,
`st4_sds4.hpp:170-177`), 13 % the `powf` of the libmvec site at `:2315` (`st4_sds4.hpp:210`) and
13 % the mean-square-slope sums (`:2228-2254`, `st4_sds4.hpp:128-147`). g++ did not vectorise the
integral: the inner loop is an ordered sum, and the outer loop over `ITH` has "unsupported control
flow". Its length is `2*SDSNTH+1` with `SDSNTH = MTH/2-1` (`w3gdatmd.F90:2099`), 23 for
`NTH = 24`, while only `2*MIN(NINT(SSDSDTH/(DTH*RADE)),NTH/2-1)+1` = 11 weights are set
(`w3src4md.F90:1135-1146`, `SDSDTH = 80` by default, `w3gridmd.F90:2166`): 12 of the 23 products
multiply a zero weight. The Fortran does the same work.

Inside `W3SIN4`, the samples sit in the loop over the spectrum that builds `DSTAB` and the
wave-supported stress (`st4_sin4.hpp:198-249`, `w3src4md.F90:785-795` as the port cites it),
whose sums g++ leaves scalar (`st4_sin4.hpp:242`, "couldn't vectorize loop").

### Thread scaling, `OMP_NUM_THREADS` 32, 16, 8, 1

One run each, `WW_KOKKOS_TIMERS=1` on the Kokkos runs, shared machine ⚠. At 32 threads one run
with the timers took 21.46 s and one without 22.59 s, so the fences do not show above the noise
of this machine ⚠. The load column gives the load before the Kokkos run, then before the
Fortran run:

| Threads | Load (1 min) | Kokkos wall (s) | `W3SRCE` kernel (s) | `W3XYP3` (s) | `W3KTP3` (s) | Outside Kokkos (s) | Fortran, same executable (s) | Fortran / Kokkos |
|---|---|---|---|---|---|---|---|---|
| 32 (quiet) | 3.5 / 11.8 | 14.36 | 12.45 | 1.09 | 0.39 | 0.22 | 26.31 | 1.83 |
| 32 | 17.6 / 25.9 | 21.21 | 18.48 | 1.60 | 0.57 | 0.25 | 33.02 | 1.56 |
| 16 | 29.4 / 27.1 | 29.21 | 26.44 | 1.46 | 0.69 | 0.26 | 36.46 | 1.25 |
| 8 | 21.4 / 28.3 | 47.35 | 42.58 | 2.53 | 1.32 | 0.23 | 56.13 | 1.19 |
| 1 | 22.8 / 8.7 | 174.78 | 156.51 | 11.04 | 5.96 | 0.19 | 174.27 | 1.00 |

From 1 to 32 threads the `W3SRCE` kernel speeds up 12.6 times on the quiet run and 8.5 times on
the loaded one; with the load this uneven, the scaling between 8, 16 and 32 threads is not
separable from the other session's ⚠. The "quiet" Fortran run's load of 11.8 is partly the
Kokkos run that had just ended, which the 1-minute average still counts.

## What it changes

The next bottleneck is the source-term kernel, 87 % of the wall, and inside it the DIA and
`W3SDS4`, 66 % of the wall together. Propagation, output, copies and the Fortran left in
`W3WAVE` are 13 % together, with nothing above 1.1 s. Since one thread of the port costs what one
thread of the Fortran costs, the next gain has to come from the per-point code. Three proposals,
none implemented, each to be argued bit for bit before it is written (AGENTS.md, "Bit for bit"):

1. **DIA, points in the vector lanes** (rank 1, 6.4 s). Sections 3 and 4 are already vectorised
   over the spectral bins, which turns most loads into gathers through `IP`/`IM`/`IC`, and the
   one-thread run shows no gain over the Fortran (built with `-O3` and no `-march`,
   `WW3/model/src/CMakeLists.txt:134-135`). A team of eight points, one per AVX2
   lane, would run the same statement for eight spectra with contiguous loads (spectra stored
   point-minor in the team's scratch), each lane doing its own point's arithmetic in the
   Fortran's order. No sum crosses a lane, so the bits cannot change; the parity argument is the
   per-lane GIMPLE. Gain not measured ⚠: the profile does not separate the gathers' cost from the
   arithmetic.
2. **`W3SDS4` saturation integral** (rank 2, about 1.6 s of the 3.1 s). (a) Interchange the loops
   of `st4_sds4.hpp:170-177` so that `J` is outer and `ITH` inner, with 24 accumulators: each
   `BTH(IS)` still sums its products in the order `J = 1, 2, ..., 23`, so the bits stay, and the
   inner loop becomes vectorisable over `ITH`. This changes loop nesting, which phase 1 allows
   only with the owner's agreement. (b) Skipping the 12 zero-weight terms gives the same sum when
   every `A` is finite and non-negative, and differs when one is infinite or NaN (`0*Inf` is
   NaN): a change of what the routine computes for such input, so it is the owner's decision and
   its own pull request.
3. **`W3SIN4`'s stress sums** (rank 3, 1.4 s). The loop over the spectrum near
   `w3src4md.F90:785-795` accumulates the stress with branches on the sign of `DSTAB`; reordering
   the sums is not allowed. The bit-neutral route is the one of proposal 1: points in the lanes,
   which makes every per-point sum a lane-local sum in the Fortran's order. If proposal 1 is
   built as a batched-points layer for the whole `W3SRCE` team body, `W3SDS4`, `W3SIN4` and the
   integration (ranks 1 to 4, 12.0 s) all inherit it. One trap: the five libmvec sites of ST4
   (`st4_libmvec.hpp`) must keep the Fortran's groups of four directions per call, so a lane
   cannot hold a point there; those loops would stay as they are.

Not ranked, recorded so nobody repeats it: `perf` is not usable on this workstation without
`sudo` (the owner can run `sudo sysctl kernel.perf_event_paranoid=1` for a session), and
`W3XYP3`'s idle share needs more samples before it ranks. A timing for a table needs the median
of three runs on an idle machine after a passed regtest replay (`bench/README.md`); these numbers
are a diagnostic profile, not a speed figure.

## How to repeat

`pmp.gdb`, given to
`gdb -q -batch -nx -iex "set auto-load safe-path /nix/store" -x pmp.gdb --args <ww3_shel>` in the
case directory, with the preset's environment:

```text
set pagination off
set confirm off
set print thread-events off
set print inferior-events off
set print frame-arguments none
set width 0
handle SIGUSR2 stop print nopass
handle SIGPIPE nostop noprint pass
run > ww3_shel.out 2>&1
while 1
  echo ==SAMPLE==\n
  thread apply all bt 60
  continue
end
```

Beside it, a loop finds gdb's child, waits until it runs (its state in `/proc/<pid>/stat` is not
`t`), sleeps 0.1 s (or 0.05 s) and sends `kill -USR2 <pid>`, until the child exits. The reduction
charges each sample to the region of the main thread (thread 1): the first of `ww_srce_batch`,
`xyp3`, `ktp3`, `outg`, `w3iogo`, `w3init`, `w3wave` that appears in any of its frames, in that
order. It charges each thread to the first frame whose file starts with `snl1_`, `st4_sds4`,
`st4_sin4`, `st4_spr4`, `srce_kernel`, `prop_xyp3`, `prop_qck`, `prop_ktp3` or `iogo_w3outg`,
skipping helper frames, or to "idle" when its innermost frame is in `libgomp`.
