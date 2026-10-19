# A bit-identical Kokkos runtime for WAVEWATCH III: CPU results and five speed-ups

18 October 2026, updated the same day after #191 merged (`5cd0140`). Status: **WIP.** This record is for wave-modelling physicists and WW3
developers. It gives the CPU results of an optional C++/Kokkos runtime for WAVEWATCH III® (WW3),
their limits, and the result of five speed-ups that do not change one output bit. One of them is
a change to the Fortran of `W3SDS4` that any ST4 build can use.

Code references are `file:line`. The Fortran is pure NOAA WW3 at commit `03adda91` or the fork
at commit `50a10730`, whose `w3src4md.F90`, `w3snl1md.F90` and `w3pro3md.F90` are identical to
`03adda91`'s (v, `git -C WW3 diff --quiet 03adda91 50a10730 -- <file>`, 2026-10-18). The C++
paths are those of the port (`kokkos/...`).

**What changed with #191.** The fork pin moved from `80644ba9` to `50a10730`. The resident preset
has a seventh part, `CPRT` (the spectral partitioning). `ww3_multi`, OpenMP builds and the
partition output are now shown bit-identical (section 4). The spectrum is no longer copied back
to the host in steady state (section 2). A fallback to the Fortran can now stop the run (strict
mode). The first version's speed figures were measured before these changes; section 3 says what
that means.

## Status at a glance

| Question | Answer | Where |
|---|---|---|
| Does the default build change WW3? | No: the patch removes no line, every line it adds to a WW3 source file is inside a `W3_KOKKOS` preprocessor block, and with `KOKKOS` built and no flag set the Fortran runs | section 2 |
| Bit-identical to the Fortran on the CPU? | Yes, on every regtest entry run with the preset (sections 4 and 8 list what was not run) | section 4 |
| Faster on the CPU? | 1.65 times less wall time than NOAA WW3 on `full`, 32 threads, median of ten alternated runs on the merged code; regtest pending ⚠ (22 entries passed, not the whole matrix) | section 3 |
| GFS-Wave (`DIST MPI`, Intel compiler)? | No. The runtime is single node by design; MPI keeps the Fortran ([ADR-0017](ADRs/ADR-0017-kokkos-port-is-single-node-mpi-stays-fortran.md)) | section 8 |
| GPU? | The CUDA build compiles; no device run yet | section 8 |
| Ready for operations or for upstream? | Not yet. Thirteen gaps, ranked, with their next step | [`PRODUCTION_READINESS_202610.md`](PRODUCTION_READINESS_202610.md) |

## How to read the marks

| Mark | Meaning |
|---|---|
| **Checked (v)** | Compared with a run output, a data file or the source code. The source is given. |
| **Assumption ⚠** | Not checked. A projection or an inference, not a measurement. |
| **Position** | The opinion of the author, given as a recommendation. |
| **WIP** | Work that started and is not complete. |

## 1. Summary

- **Checked (v).** The runtime is optional. A run-time flag selects each part. With no flag, the
  executable runs the original Fortran.
- **Checked (v).** On the CPU, the "resident" configuration gives output files byte-identical to
  the Fortran of the same executable. This is true on the test cases `smoke` and `full`, at 1 to
  32 threads, and on 22 entries of the WW3 regtest matrix, `ww3_multi` and OpenMP builds included
  (section 4).
- **Checked (v).** On one 32-thread node with a `SHRD OMPG` build, on the merged code
  (`5cd0140`, fork `50a10730`), the median of ten alternated runs of `ww3_shel` on the `full` case
  was 26.18 s for NOAA WW3 and 15.84 s for the Kokkos runtime: 1.65 times less wall time, every
  output byte-identical to the first run's (section 3). The machine was idle when the runs started
  (load 2.3). Regtest pending ⚠: WW3's regtests have passed on this commit for 22 entries
  (section 4), not for the whole matrix.
- **Checked (v).** At one thread the Kokkos code and the Fortran take the same time. The gain
  comes from the distribution of the work over threads.
- **Checked (v).** The gain applies to shared-memory builds only. Under `DIST MPI`, the build of
  GFS-Wave, the runtime keeps the Fortran path by design.
- **Checked (v).** Three of the five speed-ups are built. They keep every output bit. The DIA
  kernel alone is 1.93 times faster at one thread (section 6).
- **Checked (v).** A loop interchange in the Fortran `W3SDS4` keeps every output bit of WW3
  (section 7). It applies to every ST4 build, `DIST MPI` included.

## 2. What the runtime is

WW3 integrates the action balance equation

$$\frac{\partial N}{\partial t} + \nabla_x \cdot (\dot{x} N) + \frac{\partial}{\partial k}(\dot{k} N) + \frac{\partial}{\partial \theta}(\dot{\theta} N) = \frac{S}{\sigma}, \qquad S = S_{in} + S_{nl} + S_{ds} + \dots$$

with fractional steps: spatial propagation (`W3XYP3`, the third-order UQ scheme, switch
`PR3 UQ`), intra-spectral propagation (`W3KTP3`), and the source terms per sea point
(`W3SRCE`). The runtime keeps these steps and their arithmetic. It moves them to C++/Kokkos.

- **Checked (v).** The fork adds the switch `KOKKOS`, a branch at each call site and
  `bind(C)` interfaces. At `50a10730` it changes 9 files and adds 7 modules under `model/src/`:
  5 687 lines added, none removed (`git -C WW3 diff --stat 03adda91 50a10730 -- model`). Each
  added line in a WW3 source file sits inside an `#if` on `W3_KOKKOS` (a script over the
  `git diff -U0` hunks, 2026-10-18). Each part has a flag `WW_KOKKOS_<PART>`. Unset or `0` runs the Fortran
  routine, which stays compiled in the same executable.
- **Checked (v).** The "resident" preset sets seven parts: `SRCE` (all of `W3SRCE` for every sea
  point in one batched launch: `W3SIN4`, `W3SDS4`, `W3SNL1`, `W3SPR4` and the integration),
  `XYP3`, `KTP3`, `OUTG`, `IOPE`, `UPDV` and `CPRT` (`kokkos/src/fortran_iface/presets.txt`). One
  variable selects it: `WW_KOKKOS_PRESET=resident`, read by `W3INIT`. A part flag set by hand
  still wins.
- **Checked (v).** The spectrum `VA` stays in Kokkos memory between time steps and between
  `W3WAVE` calls. A Fortran routine that reads or writes it moves only the columns it touches.
  On `full`, `WW_KOKKOS_VA_COUNT=1` counts one whole-spectrum copy to the device (when `VA` is
  bound) and none back (`docs/log/2026-10-18-ww3-multi-resident-va.md`).
- **Checked (v).** With `WW_KOKKOS_STRICT=1`, a part that would fall back to the Fortran stops the
  run with its reason. Without it, the run prints one line at exit with the number of calls left
  on the Fortran. The regtest driver's Kokkos runs use strict mode.
- **Checked (v).** `WW_KOKKOS_VA_POISON=1` fills a stale host copy of `VA` with NaN, so a missed
  sync changes the output instead of passing silently. Every regtest run of section 4 used it.
- **Checked (v).** The C++ is compiled with contraction off (`-ffp-contract=off`), so no fused
  multiply-add changes a rounding.

## 3. CPU results on the `full` case

The `full` case: a 120 x 80 rectilinear grid, 9 520 sea points, `NK = 24`, `NTH = 24` (576
spectral components), 12 hours, ST4 physics with the DIA, `PR3 UQ`, switch `SHRD OMPG`. Machine:
Intel Core i9-14900, 24 cores, 32 hardware threads. Compilers: gfortran and g++ 16.2.0. Kokkos
5.2.2, OpenMP backend.

Command: `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident`. The script builds the
fork, runs it twice (all flags at 0, then the preset), then builds and runs pure NOAA WW3.

| Run | Load (1 min) | NOAA WW3 (s) | Fortran, same executable (s) | Kokkos resident (s) | Ratio to NOAA WW3 |
|---|---|---|---|---|---|
| 17 Oct, fork `769c44f0` | 35.9 to 46.5 | 43.09 | 41.92 | 17.98 | 2.40 |
| 18 Oct, morning | not recorded | 24.81 | 23.38 | 15.51 | 1.60 |
| 18 Oct, quietest | 3.5 / 11.8 | not run | 26.31 | 14.36 | 1.83 to the same executable |
| 18 Oct, final code | not recorded | 32.06 | 30.86 | 18.36 | 1.75 |
| 18 Oct, after #191, one run (#191's notes) | shared | not run | 31.8 | 46.4 | 0.69 to the same executable |
| 18 Oct, after #191, median of 3 | 1.8 at start, 18.9 to 24.9 during | 29.83 | 26.54 (run 1 only) | 19.99 | 1.49 |
| **18 Oct, after #191, median of 10** | **2.3 at start** | **26.18** | 27.09 (run 1 only) | **15.84** | **1.65** |

- **Checked (v).** In every run, the Kokkos output is byte-identical (`cmp`) to the Fortran of the
  same executable. Against NOAA WW3, `out_grd.ww3` and `mod_def.ww3` are byte-identical.
  `ww3.field.nc` differs only in the global attribute `WAVEWATCH_III_switches`, which ends with
  `KOKKOS` in the fork. `nccmp-tol` finds `max|d| = 0` on `hs`.
- **Assumption ⚠.** The same NOAA build took 24.8 s in one run and 43.1 s in another. The load of
  other jobs on the machine explains this spread. A number for a table needs the median of three
  runs on an idle machine.
- **Checked (v), shared machine ⚠.** The first four rows were measured before #191. After it, one
  loaded run had the Kokkos runtime slower than the Fortran (46.4 s against 31.8 s). The median
  of three on the merged code then gave `--ww3` 29.83, 32.97, 29.21 s and Kokkos 20.95, 19.99,
  14.28 s, every output byte-identical to run 1's. Command: `bash scripts/ww3-gpu.sh --ww3-gpu
  full --cpu --kokkos resident --repeat 3` at `5cd0140`, 2026-10-18 19:55 to 20:00 UTC. The
  load rose because other jobs started during the runs; the spread of the Kokkos runs (14.3 to
  21.0 s) is that load. So the slow run was load, not a regression, as far as a shared machine
  can show ⚠.
- **Checked (v).** The median of ten, the figure this record quotes: `bash scripts/ww3-gpu.sh
  --ww3-gpu full --cpu --kokkos resident --repeat 10`, 2026-10-18 21:21 to 21:32 UTC, code tree
  identical to `5cd0140` (`git diff --stat 5cd0140 -- kokkos scripts regtests switches` empty),
  after the other session's jobs had stopped. Runs, s: `--ww3` 26.02, 26.08, 24.98, 24.55, 25.76,
  26.59, 26.28, 26.47, 26.76, 26.76; Kokkos 16.58, 15.04, 14.74, 14.86, 15.71, 16.01, 16.38,
  15.75, 15.92, 16.02. Every output byte-identical to run 1's.
- **Assumption ⚠.** The load printed during the timed runs (24.5 before, 27.0 after) is the runs'
  own 32 threads, which the 1-minute average counts; no other job was running when they started,
  and the spread of each set is under 13 %. That other jobs did not start during the runs is
  inferred from that spread, not recorded.

### Thread scaling (18 October, one run each, shared machine ⚠)

| Threads | Kokkos resident (s) | Fortran, same executable (s) | Fortran / Kokkos |
|---|---|---|---|
| 1 | 174.78 | 174.27 | 1.00 |
| 8 | 47.35 | 56.13 | 1.19 |
| 16 | 29.21 | 36.46 | 1.25 |
| 32 | 14.36 | 26.31 | 1.83 |

**Checked (v).** The `out_grd.ww3` of every row is identical to the Fortran's. The bits do not
depend on the number of threads.

## 4. Bit parity: what is shown and what is not

Every run below: fork `50a10730`, the source-term switches of the test build (`BT1 DB1 LN1
MLIM`, because no `ST4 NL1` switch file of the regtests has this set), strict mode, the host copy
of `VA` poisoned. `test.comp` compares every output file of the Kokkos run with the Fortran run of
the same executable; "0/23" means 0 of 23 files differ. Source:
[`docs/log/2026-10-18-u3-regtests-v2.md`](log/2026-10-18-u3-regtests-v2.md) and its summary
tables in `docs/data/ww3gpu_matrix_*_pin-50a10730_2026-10-18.tsv`.

| Entries | Program, build | Parts | `test.comp` |
|---|---|---|---|
| `ww3_tp2.15` (4 inputs), `ww3_ts2` `ST4_PR3_UQ` | `ww3_shel`, serial | resident | 0/23, 0/23, 0/23, 0/25, 0/22 |
| `ww3_ts3`, 5 propagation schemes | `ww3_shel`, `OMPG` | `SRCE,OUTG,IOPE,UPDV,CPRT` | 0/20 each |
| `mww3_test_05`, 5 propagation schemes | `ww3_multi`, `OMPG` | `SRCE,OUTG,IOPE,UPDV,CPRT` | 0/27 each |
| `mww3_test_05` `ST1`, `ST4_PR3_UQ_OMP` | `ww3_multi`, `OMPG` | `SRCE,OUTG,IOPE,UPDV` | 0/27 each |
| `mww3_test_01` to `_04` | `ww3_multi`, serial | resident | 0/22, 0/23, 0/34, 0/25 |
| `ww3_tnc1` (partition fields) | `ww3_shel`, serial, `PR0` | `SRCE,OUTG,IOPE,UPDV,CPRT` | 0/30 |

- **Checked (v).** `nccmp-tol` passes on every entry it judged. No entry needed a second Fortran
  run to excuse a file, because no file differed.
- **Checked (v).** `XYP3` and `KTP3` refuse a moving grid, so `ww3_ts3` and `mww3_test_05` ran
  without them. `mww3_test_01` to `_04` turn the source terms off in their grid inputs, so they
  test the exchanges between grids, the propagation and the outputs on the resident spectrum, not
  `W3SRCE`.
- **Checked (v).** Four entries fail, and stay failed by the owner's decision: `ww3_tp2.6`
  (`ST4`, `ST4_ASCII`) and `ww3_tnc1` with the full preset stop at start-up, because `XYP3` does
  not cover an unstructured grid or `PR0`; `mww3_test_07` fails in the Fortran run too (its
  `ww3_grid` namelist read against the test switch). None is marked expected-to-fail.
- **Checked (v).** The DIA (`W3SNL1`) alone passes the `NL1` entries of the regtest matrix.
- **Not shown.** Parity with the Intel compiler; parity on a GPU (judged by tolerance); restart
  reproduction beyond one `SHRD` case (`scripts/restart_pair.sh` ran on `full` at `WW3/` `50a10730` and `fac74069`, OpenMP backend, 12 hours, every comparison identical; [log](log/2026-10-18-resident-restart-pair.md)); `ww3_multi` under MPI.

## 5. Where the CPU time goes

The quietest run of section 3, 32 threads, from the runtime's timers and gdb stack samples. The
split of the `W3SRCE` kernel into terms comes from sample shares, so those rows are derived ⚠.

| Rank | Part | s | Share of wall |
|---|---|---|---|
| 1 | `W3SNL1` (DIA) | 6.37 | 44.4 % |
| 2 | `W3SDS4` | 3.10 | 21.6 % |
| 3 | `W3SIN4` | 1.41 | 9.8 % |
| 4 | `W3SRCE` integration | 1.12 | 7.8 % |
| 5 | `W3XYP3` | 1.09 | 7.6 % |
| 6 | `W3KTP3` | 0.39 | 2.7 % |
| | Fortran outside Kokkos, I/O, set-up | 0.22 | 1.5 % |
| | `VA` copies | 0.002 | 0.0 % |

**Why the Fortran is slower at 32 threads.**

- **Checked (v).** In NOAA WW3 with `SHRD OMPG`, the loop over the spectral components for
  spatial propagation is serial (`w3wavemd.F90:1944`, `DO ISPEC=1, NSPEC`). `W3GATH` and `W3SCAT`
  have no OpenMP directive (`w3wavemd.F90:3024-3644`). Each call of `W3XYP3` opens its own
  parallel regions over the sea points (`w3pro3md.F90:978`, `:1077`, `:1106`, `:1126`).
- **Checked (v).** The Kokkos `W3XYP3` propagates all components in one launch, in parallel over
  the components.
- **Assumption ⚠.** At one thread, the Kokkos parts other than `W3SRCE` take 18.3 s. If the
  Fortran spends the same time there and its `W3SRCE` loop scales like the Kokkos one, the rest
  of the Fortran takes about 14 s at 32 threads: a speed-up of about 1.3 from 1 to 32 threads.
  A timing per region of the Fortran at 32 threads can confirm or reject this.
- **Position.** If this is correct, a parallel loop over the components in the Fortran `SHRD`
  path gives part of the gain to OpenMP builds of WW3 without Kokkos. Each component is
  independent, so the bits do not change.

## 6. Six speed-ups that keep every bit

Each item must make the resident mode faster, keep every output bit, and give WW3 a part that it
can use alone. Four items are built. Two items needed no change.

| Item | Target | Status | Bits | Result |
|---|---|---|---|---|
| 6.1 Thread placement | all launches | no change | Checked (v) | not measured ⚠ |
| 6.2 `-fno-math-errno` | source terms | built | Checked (v) | not separated ⚠ |
| 6.3 `W3XYP3` load balance | rank 5 | no change | Checked (v) | already in place |
| 6.4 `W3SDS4` saturation integral | rank 2 | built | Checked (v) | loop 1.7 to 1.9 times faster |
| 6.5 DIA halo layout | rank 1 | built | Checked (v) | kernel 1.93 times faster, 1 thread |
| 6.6 `W3XYPUG` four switches | unstructured grids | built | Checked (v), regtest pending ⚠ | kernel 1.21 times faster at 32 threads, 1.03 at 1 thread; whole `ww3_shel` 1.21 times at 32 threads (CPU re-run, 3 rounds); `FUSE_UT` and `SKIP_H2D` help, `LAYOUT` and `CFL_PAR` inside the spread |

**Checked (v), for all items.** With the final code, `full` and `smoke` give model files
byte-identical to the Fortran of the same executable, and `nccmp-tol` passes against NOAA WW3
with `max|d| = 0`. About 40 runs of `full` during the work, at 1 and 32 threads, all gave an
`out_grd.ww3` identical to the Fortran's.

### 6.1 Thread placement

- **Checked (v).** The run script passes `OMP_PROC_BIND` and `OMP_PLACES` from the environment to
  both runs, so no code change is necessary.
- **Checked (v).** The output is byte-identical at 1, 8, 16 and 32 threads. Placement changes
  which thread runs a point, not the arithmetic.
- **WIP.** Compare `OMP_PLACES=cores OMP_PROC_BIND=spread` with the default on an idle machine.
  On a shared machine, the measurement cannot rank them.

### 6.2 `-fno-math-errno`

- **Change.** The C++ library uses gfortran's default. Before, g++ added a branch to glibc's
  `sqrtf` for a negative argument, only to set `errno`.
- **Bits. Checked (v).** IEEE 754 square root is correctly rounded, so `sqrtss` and glibc's
  `sqrtf` give the same value. All runs stayed byte-identical.
- **Speed. Assumption ⚠.** Not separated from the other changes.

### 6.3 `W3XYP3` load balance

- **Checked (v).** The list of components is in ascending `ISPEC`. So the lowest frequencies,
  which need the most CFL (Courant-Friedrichs-Lewy) sub-steps, start first. This is already the
  best order for a dynamic schedule.
- **Assumption ⚠.** The Kokkos chunk for 576 items on 32 threads is one item. This was read from
  the Kokkos rule, not printed. No bit-neutral change was left.

### 6.4 The saturation integral of `W3SDS4`

The original statement (`w3src4md.F90:2281-2285`) sums 23 products for each direction:
`BTH(IS) = DOT_PRODUCT(SATWEIGHTS(:,ITH), A(IS0+SATINDICES(:,ITH))) * FACSAT`. One sum is a chain
of 23 dependent additions.

- **Change.** The loop over the weight index `J` is outside and the loop over `ITH` is inside. The
  C++ reads transposed copies of `SATWEIGHTS` and `SATINDICES`, with the direction index
  fastest, so the inner loop reads contiguous memory.
- **Bits. Checked (v).** Each `BTH(IS)` still starts from 0, adds its 23 products in the order
  `J = 1, ..., 23`, and is then multiplied by `FACSAT`.
- **Speed. Checked (v).** In an isolated test of this loop (best of 7 x 20 000 calls, `NK = 24`,
  `NTH = 24`), one sea point took 5.5 to 5.7 µs with the original order, 5.1 µs with the
  interchange on the original tables, and 3.0 to 3.3 µs with the transposed tables. The
  transposed tables give most of the gain.
- **Excluded.** 12 of the 23 weights are zero for `SSDSDTH = 80`. To skip them changes the result
  when one `A` is infinite or NaN (`0 x Inf` is NaN), so this is not a bit-neutral change.

### 6.5 The DIA with a direction halo

Sections 3 and 4 of `W3SNL1` read the spectrum at the quadruplet bins through the index tables
`IP`, `IM` and `IC` (`w3snl1md.F90:378-440`). With the bins vectorised, each read is a gather.

- **Checked (v).** Each table entry is `IT(ITH) + (IF(IFR)-1)*NTH`: a direction shift with a
  wrap, and a map of the frequency row (`w3snl1md.F90:667-727`).
- **Change.** The kernel copies the arrays into rows with a halo of the wrapped neighbour
  directions on both sides. A read is then the same row and a fixed shift for all directions of
  a frequency, so the loads are contiguous. The set-up compares every table entry with the row
  and the shift. If one entry is different, the kernel keeps the gathers.
- **Bits. Checked (v).** The values read and the arithmetic are those of the gather form. Only
  the location of the copies changes.
- **Speed. Checked (v).** The DIA kernel alone (1 000 sea points, `NK = 25`, `NTH = 24`, median of
  7 alternated runs, machine load 8.4 to 8.9):

| Threads | Before (ms per call) | After (ms per call) | Ratio |
|---|---|---|---|
| 1 | 12.31 | 6.39 | 1.93 |
| 32 | 2.50 | 2.11 | 1.19 |

- **Assumption ⚠.** Inside the model, with the halo switched on and off in one executable, the
  `W3SRCE` kernel took 7 % to 15 % less time with the halo (32 threads, load 31 to 57). The whole
  `ww3_shel` medians, before and after all changes, were 17.67 s and 17.09 s (load 8 to 38).
  That difference is inside the spread of the runs, so the end-to-end gain is not measured yet.

### 6.6 The unstructured-grid propagation kernel (`W3XYPUG`, `W3XYPFSN2`)

Four changes to the batched kernel of issue #196, each with its own switch
(`WW_KOKKOS_XYPUG_FUSE_UT`, `_SKIP_H2D`, `_LAYOUT`, `_CFL_PAR`, default on).

- **Change.** The first sub-step's `UTILDE` is computed in the per-triangle pass
  (`w3profsmd.F90:671` with `:632-634`). The per-call arrays are uploaded only when they differ from
  a host copy. `KELEM` is stored in the node-list order, and `IOBPD` and `CG` are transposed copies.
  The per-node divisions of the CFL step (`:647`) are made in parallel.
- **Bits. Checked (v), regtest pending ⚠.** Each change moves the place or the time of an
  operation, not its operands or its order; the sums per node keep ascending `IE`, then `K`, and the
  minimum of `:648` stays one chain in ascending `IP`. `ww3_tp2.6` `ST4` and `ST4_ASCII`: 0 of 36
  and 0 of 38 files different, `nccmp-tol` PASS, with the switches on and with them off. The rest of
  the regtests is for the validation phase.
- **Speed. Checked (v), one entry, regtest pending ⚠.** `xypug.kernel`, median of three, i9-14900: 7.97 s with all
  off and 6.57 s with all on at 32 threads; 37.43 s and 36.50 s at 1 thread. `UTILDE` alone gives
  6.79 s at 32 threads. The other three are inside the spread on this CPU; the uploads fall from
  0.43 s to 0.06 s. Not measured on a GPU or for the whole `ww3_shel` run ⚠.
  Log: `docs/log/2026-10-19-xypug-speedups.md`.
- **Speed on the CPU, per switch. Checked (v), 3 rounds, one entry, regtest pending ⚠.** Re-timed the
  same day with the whole `ww3_shel` time (`Elapsed time`), median of three interleaved rounds, a
  difference counted only above the larger min-max spread. At 32 threads all off 9.78 s, all on
  8.05 s (kernel 7.87 s and 6.51 s); at 8 threads 10.46 s and 8.46 s; at 1 thread 38.26 s and
  37.64 s. `FUSE_UT` helps (real at 32, 8 and 1 threads, about 1 s of `ww3_shel` at 32 and 8
  threads). `SKIP_H2D` helps the whole run at 32 threads (minus 0.43 s alone, plus 0.84 s when
  removed from all-on) and is inside the spread in the kernel timer alone; at 8 threads it is at
  the edge ⚠. `LAYOUT` and `CFL_PAR` are inside the spread alone and as leave-one-out: no gain on
  the CPU, no loss. Their target is the GPU, not measured ⚠. Raw rows:
  `docs/data/xypug_speedups_cpu_rerun_2026-10-19.tsv`.

## 7. The Fortran change for WW3: `W3SDS4`

**Checked (v).** This change was built and run, not only proposed. It replaces lines 2281-2285 of
`w3src4md.F90` and adds one integer declaration (`INTEGER :: JSAT`):

```fortran
          ! partial integration: JSAT outer, ITH inner. Each BTH(IS) adds its
          ! products in the order of DOT_PRODUCT, from 0, then times FACSAT.
          DO ITH=1,NTH
            BTH(IS0+ITH)=0.
          END DO
          DO JSAT=1,SIZE(SATWEIGHTS,1)
            DO ITH=1,NTH
              BTH(IS0+ITH)=BTH(IS0+ITH)                                  &
                   +SATWEIGHTS(JSAT,ITH)*A(IS0+SATINDICES(JSAT,ITH))
            END DO
          END DO
          DO ITH=1,NTH
            BTH(IS0+ITH)=BTH(IS0+ITH)*FACSAT
          END DO
```

- **Static check. Checked (v).** In gfortran's optimised intermediate code at `-O3` with the build's
  flags (`-fdump-tree-optimized`), the original sets `val = 0.0`, adds each `weight*A` in index
  order, and returns `facsat*val`. The new loop sets `BTH = 0.0`, adds the same products in the
  same order, and multiplies by `facsat`. IEEE 754 addition and multiplication are commutative,
  so each result has the same bits.
- **Run check. Checked (v).** The original and the changed WW3 were built from commit `03adda91`
  with the same switch and flags. On `full`, at 32 threads and twice at one thread, the changed
  build wrote an `out_grd.ww3` byte-identical to the original's.
- **Control. Checked (v).** A build with `SRHS = SRHS * 1.0001` in `W3SDS4` changes `out_grd.ww3`
  (first difference at byte 267 482). So the comparison detects a change in `W3SDS4`. The
  control line must come before the whitecap-output test of `W3SDS4`: the usual path returns
  there, and a line before the last `RETURN` of the routine does not run.
- **Speed. Assumption ⚠.** The CPU time at one thread was 296.4 s and 280.8 s for the original
  and 295.8 s and 196.2 s for the changed build. The machine load explains more of this spread
  than the change does. In C++, most of the gain needed the transposed tables (section 6.4).
- **Position.** Offer the interchange together with transposed copies of `SATWEIGHTS` and
  `SATINDICES`, built once in `INSIN4`. Measure it in an MPI build of GFS-Wave's grid before a
  speed is claimed.

## 8. Limits

- **Checked (v).** GFS-Wave uses `DIST MPI`. Under MPI the resident parts `XYP3`, `OUTG`, `IOPE`
  and `UPDV` refuse to run by design, and the runtime is single node. Do not read the results of
  section 3 as a speed-up of GFS-Wave.
- **Checked (v).** The Kokkos `W3SRCE` translates one source-term set (`ST4 NL1` with
  `BT1 DB1 LN1 MLIM`). Other sets run the Fortran.
- **Assumption ⚠.** Bit parity is shown for gfortran and g++ on x86-64 only.
- **WIP.** On a GPU, the device must give glibc's bits for `expf`, `logf` and `powf`. Device
  copies of glibc's functions exist; no device run has compared them yet. The CUDA build of the
  preset compiles and links (`--build-only`, `docs/log/2026-10-18-cuda-build-only.md`); it has
  not run on a device.
- **Checked (v).** `XYP3` covers rectilinear and curvilinear grids only; an unstructured grid
  stops the preset at start-up. The unstructured (`PDLIB`) path is translated and not wired.
- **Assumption ⚠.** The ESMF cap (`wmesmfmd.F90`) reads the resident `VA` before its exports.
  It was not compiled here: no ESMF library is installed.
- **Checked (v).** Most of the C++ is LGPL-3.0-or-later, like the WW3 source it translates (249
  of 347 files under `kokkos/src`). Where the library would live upstream is open
  ([ADR-0018](ADRs/ADR-0018-where-ww-kokkos-lives-for-upstream.md), Proposed).

## 9. Before a speed is final

1. Record the median of ten of section 3 in `bench/results/` once WW3's regtests have passed on
   `5cd0140` (ADR-0016); 22 entries have, the rest of the matrix has not.
2. Time the Fortran per region at 32 threads, to confirm or reject the assumption of section 5.
3. Compare the thread placements of section 6.1 on the idle machine.
4. Time the Fortran change of section 7, with and without transposed tables, on an idle machine
   and in an MPI build.

## 10. How to repeat

| What | Command |
|---|---|
| Parity and wall time, CPU | `bash scripts/ww3-gpu.sh --ww3-gpu full --cpu --kokkos resident` |
| Median of three, alternated with NOAA WW3 | add `--repeat 3` |
| Stop on any fallback; catch a missed sync | `WW_KOKKOS_STRICT=1 WW_KOKKOS_VA_POISON=1` in the environment |
| The regtest entries of section 4 | the commands of `docs/log/2026-10-18-u3-regtests-v2.md`, "What was done" |
| Restart reproduction, `SHRD` | `scripts/restart_pair.sh` (ran at `WW3/` `50a10730` and `fac74069`, [log](log/2026-10-18-resident-restart-pair.md)) |
| The same on the point case | `bash scripts/ww3-gpu.sh --ww3-gpu smoke --cpu --kokkos resident` |
| Per-part timers | add `WW_KOKKOS_TIMERS=1` to the Kokkos run, or `--timers` |
| DIA kernel alone | `OMP_PROC_BIND=false OMP_NUM_THREADS=<t> kokkos/build/openmp-release/tests/ww_bench_snl1` |
| Tests of the port's tools | `just kokkos-test serial-debug`, `just kokkos-test openmp-release` |
