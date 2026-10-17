# A mutation audit of the resident-step pieces: which deliberate breakages the CPU checks catch

2026-10-17, an agent for M. Hoffmann (lane S of goal 42); issue #42,
[WFIP-0005](../WFIPs/WFIP-0005-device-resident-step.md), branch `goal/42-gpu-step-mutate`.

## Question

Every piece added to the Kokkos port in this round was accepted on one kind of evidence: on the
CPU, the Kokkos run of `scripts/ww3-gpu.sh --ww3-gpu <case>` writes the same bytes as the Fortran
of the same executable and as pure WW3 (`--ww3`). Identical output proves only what reaches the
output files. The agent that moved `W3UICE`'s zeroing of `VA` to the device disabled its kernel on
purpose and the coast case stayed identical; only the host/device comparison of
`WW_KOKKOS_VA_RESIDENT=1` caught it (`kokkos/src/fortran_iface/PATCH.md` §18). So, for each piece:
would a deliberately broken version have failed? Where it would not, what check would catch it?

## What was done

For each piece, one or two minimal breakages (*mutants*), each applied to a scratch tree with
`git apply`, run, and reverted with `git apply -R`; none was committed (`git status` clean after
each batch, v). A mutant of the fork (`WW3/model/src/w3wavemd.F90`, `w3kokkos_outgmd.F90`,
`w3kokkos_pntmd.F90`) was applied to the submodule's working tree and reverted the same way; the
fork pin did not move. Files of other lanes (`srce_shim.cpp`, `prop3_shim.cpp`, `snl1_team.hpp`,
`prop_loops.hpp`) were mutated in scratch only, and no check was added to them.

Each mutant ran `bash scripts/ww3-gpu.sh --ww3-gpu <case> --cpu --kokkos <parts>` with
`WW3GPU_RUNS` set to a scratch directory, plain and with the diagnostic flags that apply (`WW_KOKKOS_` is left out of the flag names below); after the
script, `cmp` of every model file (`out_grd.ww3`, `out_pnt.ww3`, `restart*.ww3`, the NetCDF) against
the Fortran run and against `--ww3`. A mutant is **DETECTED** when the script fails or a model file
differs, **SURVIVED** when everything passes; *equivalent* marks a survivor that cannot change a bit
of what the case computes. `<parts>` is `resident` (`SRCE,XYP3,KTP3,OUTG,IOPE,UPDV`) unless the row
says otherwise. `libm_bits` (PR #178's host check) ran as `bash scripts/libm_bits.sh ww expf fma 8`.

- **Before the checks**: `goal/42-gpu-step` at `4fbffbe8`, fork `769c44f0`, 6 unmutated baselines
  and 40 mutant runs.
- **After the checks**: rebased onto `origin/main` `d8905a3b` (PRs #178 and #179 merged), the
  commits of this branch, then again onto `b38b5fc8` (#180: `ST4_TABLES` opt-in, `W3OUTG` on the
  device libm) for the final baselines and the P mutants; 35 unmutated runs (below) and 33
  mutant runs, plus two of `libm_bits`.

Machine: Intel Core i9-14900, 32 threads, shared with other sessions (load 11 to 16); gfortran and
g++ 16.2.0, Kokkos 5.2 on OpenMP, `switch_lab_ompg`. Never `--gpu`; no regtest matrix.

## Result

| Piece | Mutant | Run | Before the checks | After the checks |
|---|---|---|---|---|
| resident `VA` sync sites | A1: no `W3KOKKOS_VA_HOST_WRITE` before `W3ULEV` | coast; also `VA_RESIDENT=1` | DETECTED: `nccmp-tol` FAIL | DETECTED |
| | A2: no `W3KOKKOS_VA_HOST_READ` at `W3WAVE`'s return | coast; `VA_RESIDENT=1` | SURVIVED: every file identical; `resident_shim.cpp` printed "still newer on the device at finalize" and nobody read it | DETECTED: the script fails on that line |
| | A3: no host read before `W3IOPE` (part `IOPE` off) | coast, `SRCE,XYP3,KTP3,OUTG,UPDV`; `VA_RESIDENT=1` | SURVIVED the script (`nccmp-tol` PASS): `out_pnt.ww3` differed, and the script compared only the NetCDF | DETECTED: byte comparison of every model file |
| | A4: no host read before the second restart stream's `W3IORS` (`J = 8`) | smoke; coast; `VA_POISON=1` | SURVIVED: no case writes a second stream, so the site never runs | SURVIVED, not reached: with `DATE%RESTART2` in the coast case the run still passed, plain and poisoned, and kept only one restart file, so the change was dropped ⚠ (why the stream did not write was not traced) |
| | A7: no host read before the restart `W3IORS` (`J = 4`) | coast | not run (coast wrote no restart; the smoke's restart falls at the start of a `W3WAVE` call, as in A6) | DETECTED: coast now writes restarts at 05:30 and 11:30; `restart001.ww3`, `restart002.ww3` differ |
| | A5: no host write before `W3UICE` (part `UPDV` off) | coast, `SRCE,XYP3,KTP3,OUTG,IOPE`; `VA_RESIDENT=1` | SURVIVED: `W3ULEV`'s host write in the same step copies `d_va` back over the zeroing, and the points it zeroed stay under ice | DETECTED by `WW_KOKKOS_VA_CHECK=1`: "1 columns of the host VA of grid 1 changed since the last sync" |
| | A6: no host read before the Fortran `W3OUTG` (the fallback) | smoke; `VA_RESIDENT=1`; `VA_POISON=1` | SURVIVED | SURVIVED, equivalent here: the smoke's field outputs fall at the start of a `W3WAVE` call, after the read at the previous return ⚠ (inferred from A2 and A4, not traced) |
| `ww_srce_batch_va` | B1: no `device_written` after the launch | coast; `VA_RESIDENT=1` | SURVIVED: `W3XYP3`/`W3KTP3` mark the state before it | DETECTED with `--kokkos SRCE` (`nccmp-tol` FAIL) and with `VA_CHECK=1` there ("3639 columns of d_va ... differ ... while the state says both"); with `resident`, `VA_CHECK=1`, still SURVIVED: equivalent in that call order |
| | B2: the last point of the batch skipped | coast | DETECTED: `nccmp-tol` FAIL | DETECTED |
| source-term input shadows (unit 4) | C1: `memcmp` dropped, a valid shadow always skips | coast; `SRCE_STAGED=1` | plain SURVIVED (in place, no shadow); `SRCE_STAGED=1` DETECTED (`nccmp-tol` FAIL) | the same |
| | C2: never skip | coast, `SRCE_STAGED=1` | SURVIVED, equivalent (copies more, same bits) | DETECTED by `counts_resident.txt`: "inputs unchanged, not copied" 0, not 422 |
| `W3XYP3`/`W3KTP3` on `d_va` | D1: no `device_written` after a propagation launch | coast; `VA_RESIDENT=1` | SURVIVED: the source-term batch marks the state later in the step | DETECTED by `VA_CHECK=1` ("251 columns of d_va ... differ at a kernel's sync"); with `--kokkos XYP3,KTP3`, `nccmp-tol` FAIL |
| | D2: `W3XYP3` on the host `VA` instead of `d_va` | coast | DETECTED | DETECTED |
| prop3 `memcmp` input cache | E1: `memcmp` dropped, a valid shadow always skips | coast | SURVIVED: the cache is compiled for a device only (`if constexpr`), so the mutant does not exist on the CPU | SURVIVED: unprovable on the CPU without a staged diagnostic in `prop3_shim.cpp` (lane R's file, proposed below) |
| `W3OUTG` on `d_va` | F1: the `THP0` (`DP`) store dropped | coast | DETECTED: `nccmp-tol` FAIL | DETECTED |
| | F2: the `T02` store dropped | coast | SURVIVED: `T02` was not asked for | DETECTED: coast now asks for every field the Kokkos `W3OUTG` computes; `out_grd.ww3` differs |
| | F3: `FP0`'s copy-back key wrong | coast | SURVIVED: on a host backend nothing is copied back | DETECTED by `WW_KOKKOS_OUTG_STAGED=1`: `nccmp-tol` FAIL |
| | F4: always the Fortran fallback | coast | SURVIVED: the Fortran's bits, by construction | DETECTED by `counts_resident.txt`: "W3OUTG Kokkos calls" 0, not 13 |
| | F5: the fallback misses `WBT` | smoke | SURVIVED, equivalent here: the smoke also asks for `SKW`, which forces the fallback | not rerun; the shim refuses `WBT` itself (`WW_KOKKOS_ERR_UNSUPPORTED`) ⚠ (not run) |
| `IOPE` partial read | G1: the shim stores one column fewer | coast; `VA_RESIDENT=1` | SURVIVED the script: `out_pnt.ww3` differed, not compared | DETECTED: byte comparison |
| | G2: the Fortran list misses the last corner | coast; `VA_RESIDENT=1` | SURVIVED: every file identical (456 of 480 columns copied) | DETECTED by `WW_KOKKOS_VA_POISON=1`: `out_pnt.ww3` differs |
| `UPDV` | H1: the zeroing kernel never stores | coast; `VA_RESIDENT=1` | plain SURVIVED; `VA_RESIDENT=1` DETECTED ("1 columns of VA differ ... after the update"), as §18 found | `VA_CHECK=1` SURVIVED (the state is `DEVICE` at each ice update); `VA_RESIDENT=1` remains the check |
| `W3GATH`/`W3SCAT` in place, on Serial | I1: `W3SCAT` in place writes nothing | coast, `--kokkos all` | DETECTED: `nccmp-tol` FAIL | DETECTED |
| | I2: back on the OpenMP space | coast, `--kokkos all` | SURVIVED, equivalent: a copy in another execution space; only the timers show it | the same |
| `W3XYP3` team kernel | J1: no barrier after `TeamLoops::par` | coast, `XYP3_TEAM=4` | DETECTED: the run crashed | DETECTED |
| | J2: no barrier after `TeamLoops::one` | coast, `XYP3_TEAM=4` | DETECTED: `nccmp-tol` FAIL | DETECTED |
| `W3SNL1` `omp simd` loops | K1: the last bin of each range skipped | full | DETECTED: `nccmp-tol` FAIL | DETECTED |
| | K2: one four-term sum of section 3 regrouped (a rounding change only) | full | SURVIVED the script: `nccmp-tol` PASS within its tolerances while `out_grd.ww3` and the NetCDF differed | DETECTED: byte comparison |
| timer fence while unwinding (`d56b8d6b`) | L1: the fence back in the destructor | smoke; `just kokkos-test serial-debug` | SURVIVED: a host fence never throws | DETECTED by `L1_test_timers` (`L1_test_timers.cpp:53`, "a Region destroyed while an exception unwinds must not fence") |
| ST4 tables in C++ (`ST4_TABLES`, opt-in since #180) | P1: `ZZ00` of `TABU_STRESS` scaled by 1.0001 (`st4_tabu_stress.cpp`) | smoke `SRCE,ST4_TABLES`; coast `--kokkos all` | (not run) | `SRCE,ST4_TABLES`: DETECTED, "29965 values differ from the Fortran's" and `nccmp-tol` FAIL; `--kokkos all`: SURVIVED, the part is off there |
| | P2: `SATWEIGHTS` scaled by 1.0001 (`st4_insin4.cpp`) | the same | (not run) | `SRCE,ST4_TABLES`: DETECTED, "264 values differ" and the byte comparison (`nccmp-tol` PASSED it); `--kokkos all`: SURVIVED, the part is off there |
| `libm_bits` (#178) | M0: the last `fma` of the fma `expf` polynomial unfused | `libm_bits ww expf fma 8` | SURVIVED, equivalent: 0 of 2^32 inputs differ | — |
| | M1: `r = InvLn2N*x - kd` unfused in the fma build | `libm_bits ww expf fma 8` | DETECTED: 2 of 2^32 inputs differ (`x = c27c65d9`: `11fa2992`, glibc `11fa2993`), exit 1 | — |

The team-gather, `TeamLoops::par` and short-chunk mutants of lane R's device defaults
(`goal/42-gpu-fast` at `b0b8a49e`) are in that lane's report, caught under the forced setting;
they were not repeated here.

The checks added (each off by default except the first three, which are verdicts of the script;
none changes a bit of a correct run, v: every baseline below is byte-identical):

1. **Every model file, byte for byte** (`scripts/ww3-gpu.sh`, `cmp_files`): `out_grd.*`,
   `out_pnt.*`, `restart*.ww3` and every NetCDF against the Fortran of the same executable, the
   binary files against `--ww3`; on the CPU a difference fails the run, on a GPU it is reported.
   Before, the script judged one NetCDF with `nccmp-tol`, within its tolerances, and printed
   "byte-identical" only as a note (A3, G1, K2). A stricter verdict of a gate: it needs a human's
   review before it merges (AGENTS.md, "Never cheat a regtest tool").
2. **The device's `VA` newer at finalize fails the run** (the line `resident_shim.cpp` already
   printed; A2).
3. **The calls per part** (`kokkos/tests/fixtures/coast/counts_resident.txt`), checked for
   `--kokkos resident` on the coast case: a part that falls back to the Fortran, or a cache that
   never skips, writes the same bits (F4, C2).
4. **`WW_KOKKOS_VA_CHECK=1`** (`resident_shim.cpp`): at every sync call, before any copy, the
   host `VA` must still hold the bits of the last sync unless a host write was declared, and with
   the state `BOTH` `d_va` must too (A5, B1, D1). It adds no copy, so it does not hide a missing
   mark as `WW_KOKKOS_VA_RESIDENT=1` does (B1 passes under `VA_RESIDENT=1`).
5. **`WW_KOKKOS_VA_POISON=1`** (`resident_shim.cpp`): after every kernel write of `d_va` the stale
   host `VA` is filled with a quiet NaN, so a host read without its sync puts NaN into the output
   (G2).
6. **`WW_KOKKOS_OUTG_STAGED=1`** (`outg_shim.cpp`): the device's buffers and copy-back on a host
   backend (F3).
7. **The coast case asks for more** (`kokkos/tests/fixtures/coast/`): every field the Kokkos
   `W3OUTG` computes (41 names with the forcing fields, the 3-D and `P2S` fields with their `OUTS` flags, the space-time
   extremes on), no field that forces the fallback, and restarts of both streams at half hours,
   inside a `W3WAVE` call (F2, A4, A7). `--ww3` and `--ww3-gpu` still write the same bytes.
8. **`L1_test_timers`** (`kokkos/tests/`): a test of a tool, not of a ported routine (ADR-0010
   holds): Kokkos' fence callback counts the fences a `Region` asks for, with and without an
   exception in flight (L1).

Unmutated baselines after the rebase, every model file byte-identical to the Fortran's and the
binary ones to `--ww3`, `nccmp-tol` PASS both (v): coast `resident` plain and with each of
`VA_RESIDENT`, `VA_CHECK`, `VA_POISON`, `OUTG_STAGED`, `SRCE_STAGED`, `XYP3_TEAM=4`; coast
`--kokkos all`; coast `SRCE` and `XYP3,KTP3` with `VA_CHECK`; coast `SRCE,XYP3,KTP3,OUTG,UPDV`
with `VA_POISON`; coast `SRCE,XYP3,KTP3,OUTG,IOPE` with `VA_CHECK`; smoke `resident` plain, with
`VA_CHECK` and with `VA_POISON`; full `resident`; and, on the final fixture (restarts at 05:30 and
11:30), coast `resident` plain and with `VA_CHECK`, `VA_POISON`, `VA_RESIDENT`, `OUTG_STAGED` and
`SRCE_STAGED`. On `b38b5fc8`, once more: coast `resident` plain and with `VA_CHECK`, `VA_POISON`,
`OUTG_STAGED`, `VA_RESIDENT`; coast `--kokkos all`; smoke and full `resident`; smoke and coast
`SRCE,ST4_TABLES` ("0 values differ"). Diagnostic counters of those runs: `VA_CHECK` ran 352
checks on the coast, `VA_POISON` poisoned 288 times, `OUTG_STAGED` copied 572 fields back.

## What it changes

- Before this, "all byte-identical" in a record of this round meant one NetCDF judged with
  tolerances. Of the 28 mutants of the first pass, 18 passed the script; 13 of them are now
  caught on the CPU (A2, A3, A5, B1 and D1 in the call orders they break, C2, F2, F3, F4, G1, G2,
  K2, L1), and A7, new, is caught too. Three passed the script while a model file differed
  (A3, G1, K2): the evidence of a record was weaker than its words.
- **Not provable on the CPU, whatever check is added here**: the device copies themselves (the
  shadows' `deep_copy` on CUDA, `OUTG`'s copy-back as CUDA runs it, the partial read's gather on a
  device), the prop3 input cache (E1; a `WW_KOKKOS_PROP3_STAGED` like `SRCE_STAGED` in
  `prop3_shim.cpp` would make it testable, lane R's file), the device routing of
  `libm_device.hpp` (#178, and since #180 `W3OUTG`'s and `SKEWNESS`'s calls: on the CPU they are
  glibc itself, and `libm_bits` checks the transcription on the host, not the device's compiled
  code), the timer guard against a real CUDA error, and
  every CUDA bit (`expf`, `--fmad=false`). They need a supervised GPU run.
- **Not provable by these cases**: A6 and F5 (equivalent where the smoke puts its fallback fields)
  and H1 under `VA_CHECK` (the state is `DEVICE` at each ice update; `VA_RESIDENT=1` still catches
  it). The regtest matrix, which this lane did not run, is the place for paths none of the three
  cases takes.
- A diagnostic flag is evidence only when a run uses it: the checkpoints of the next units should
  run coast `resident` with `VA_CHECK=1`, `VA_POISON=1`, `OUTG_STAGED=1` and `SRCE_STAGED=1`
  besides `VA_RESIDENT=1`.
