# WFIP-0003: The Kokkos DIA through WW3's own regtest harness, on the h0ffmann/WW3 fork

| | |
|---|---|
| **Status** | Postponed (2026-10-15, ADR-0011) |
| **Author** | M. Hoffmann (asked 2026-10-14 in the project thread: "how to use Kokkos code + ww3 regtest tooling, in a automated way, using the WW3 fork") |
| **Created** | 2026-10-14 |
| **Deliverable** | D4, D5 |
| **Related** | [`kokkos/port_report/W3SNL1_KOKKOS.md`](../../kokkos/port_report/W3SNL1_KOKKOS.md) ("How it was regtested"); [`scripts/ww3-gpu.sh`](../../scripts/ww3-gpu.sh) (the L2 replay it generalises); [`kokkos/tests/L2_replay.sh`](../../kokkos/tests/L2_replay.sh) (the `work_a`/`work_b` pairing); [`docs/AGENTS_KOKKOS_202609.md`](../AGENTS_KOKKOS_202609.md) §1.5 item 4 and §3.3; [ADR-0002](../ADRs/ADR-0002-port-order-wall-time.md); [ADR-0010](../ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md) (since 2026-10-14 this matrix and `ww3-gpu.sh` are the port's only tests) |
| **Effort** | M: one shell script and one recipe; no kernel, no Fortran, no WW3 edit; most of the work is the first full run and reading its failures |
| **Gain** | `proposal` (D4: WW3's own comparison, `test.comp`, beside the lab's comparator; D5: the Kokkos DIA validated against the Fortran on 100 WW3 configurations instead of 1); `lab/dev-loop` (every later port reuses the matrix) |
| **Effort vs Gain** | do next: L2 rests on one regtest today, and the next port (`W3SDS4`, row 1 of `kokkos/PORT_RANKING.md`, #118) would inherit the same thin check |
| **Depends on** | the `ww3-gpu/kokkos-st4` branch of h0ffmann/WW3 at the pin; the owner's workstation for the full run (CPU only); nothing from other WFIPs |
| **Blocked by** | none |
| **Risk** | `run_cmake_test` rebuilds WW3 for every entry, so 100 entries take hours, and a failure at that scale is hard to read; the selection is the lever (§11) |

> **Postponed on 2026-10-15.** [ADR-0011](../ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md) runs the matrix only at checkpoints; its DoD is ticked for W3SNL1, and its next change, the ST4 selection of [`ST4_KOKKOS_PLAN_202610.md`](../ST4_KOKKOS_PLAN_202610.md) §10.3, is made for the ST4 checkpoint.

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | M. Hoffmann, with an agent |
| **Tasks** | none needed: one script, one recipe and one run; §7 is the task list |
| **Tests** | `just ww3-gpu-matrix-selftest` (the selection, the argument lists and the verdict logic on fixtures); until 2026-10-15, `scripts/hoff_matrix.sh --self-test` |
| **Spec-kit** | none |
| **Issues** | not filed: Draft |

## 1. Summary

Run the Kokkos DIA (`W3SNL1`) through WW3's own regression harness, `regtests/bin/run_cmake_test`,
on every serial `NL1` entry of WW3's regtest matrix, on the CPU, with WW3 from the h0ffmann/WW3
fork and nothing edited in it. Each entry runs with the Fortran DIA and with the Kokkos DIA from
one executable, is compared byte for byte by WW3's `test.comp` and within tolerance by the lab's
`nccmp-tol`, and lands in one table. This turns the L2 evidence from one regtest into the cases
NOAA itself runs.

## 2. Motivation

The Kokkos DIA's L2 evidence is two cases: `ww3_ts1` (37 values of `hs`) and the lab's own
`bench_small` `(v)` (`W3SNL1_KOKKOS.md`, "How it was regtested"; CI health h1 log, 2026-10-15).
`nccmp-tol` judged one variable of 26 in each, since `fp`, `dir`, `dp` and `t0m1` are not in those
outputs `(v)` (h1 log: "1 judged, 25 unlisted"). `ww3-gpu.sh` copies `ww3_ts1`'s input but does not
use WW3's harness `(v)` (`scripts/ww3-gpu.sh:224`).

WW3's matrix (`regtests/bin/matrix.base`) has 758 entries; 100 of them run a switch with `NL1` and
without `MPI`, `OMPG` or `OMPH`, in 10 regtests `(v)` (Appendix, the selection command, at
`WW3@6d1bcdfe`):

| regtest | entries | regtest | entries |
|---|---|---|---|
| `ww3_ts3` | 29 | `ww3_tc1` | 3 |
| `ww3_ts2` | 25 | `ww3_tp2.6` | 3 |
| `ww3_ts1` | 15 | `ww3_tr1` | 3 |
| `ww3_ta1` | 14 | `ww3_tnc1` | 1 |
| `ww3_tp2.15` | 6 | `ww3_tp2.7` | 1 |

Their spectral grids reach NK=40 NTH=36 (`ww3_tp2.15`) and NK=32 NTH=36 (`ww3_tc1`) `(v)`
(`SPECTRUM%NK`, `%NTH` in each `ww3_grid*.nml`), beyond `ww3_ts1`'s NK=36 NTH=24; and they cover
`ST0` to `ST6`, not only `ST4`. The kernel's level-1 scratch fallback, which CTest never takes
(`kokkos/PORT_RANKING.md`, "Regtested, per port", #118), is likely exercised by them ⚠.

## 3. What changes for the reader

Before: `just smoke-ww3-gpu`, one case, one verdict. After:

```text
$ just ww3-gpu-matrix-list | head -2
  Test   #5: 001:ww3_ta1:ST4
  Test   #6: 002:ww3_ta1:ST4
$ just ww3-gpu-matrix
regtest      switch       built  ran   test.comp(F,K)   nccmp-tol(F,K)   judged
ww3_ts1      ST4          ok     ok    23 differ        PASS             hs
ww3_ts2      ST1_PR2_UQ   ok     ok    4 differ         n/a: no ounf     -
...
100 entries: 97 ran, 3 failed to build; nccmp-tol 61 PASS, 0 FAIL, 36 n/a
```

The numbers above are the shape, not a result ⚠. The real table is committed as
`docs/data/hoff_matrix_<date>.tsv` with the command and the commit, and the W3SNL1 port report's
regtest table cites it.

## 4. Sources and dependencies reviewed

### WW3's harness on the fork (`WW3/` at `6d1bcdfe`, branch `hoff/kokkos-snl1`)

- `run_cmake_test` passes the environment variable `CMAKE_OPTIONS` straight to `cmake`
  `(v)` (`regtests/bin/run_cmake_test:447,481,522`), so a Kokkos build needs no edit:
  `CMAKE_OPTIONS="-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH=<prefix>"`.
- It resolves the test directory as `$(pwd)/<test>` `(v)` (`run_cmake_test:239`) and the source
  as the argument's parent `(v)` (`:400-402`), so a staged copy of `regtests/<test>/` outside
  `WW3/` runs against the submodule's source.
- `path_build_root` is taken from the environment when set `(v)` (`:412`), so the Fortran and
  Kokkos runs of one entry can share one build.
- The fork's `model/src/CMakeLists.txt:24-36` adds `WW_KOKKOS` and refuses it without the
  `KOKKOS` token in the switch `(v)`.
- `WW_KOKKOS_SNL1=0|1` picks the DIA at run time, read by `WW_KOKKOS_INIT` `(v)`
  (`model/src/w3kokkosmd.F90:109,158`); the second run logs `Kokkos DIA (W3SNL1) enabled.` (`model/src/w3initmd.F90:768`).
- `test.comp <test> <dirA> <dirB>` compares output files with `cmp` `(v)` (`regtests/bin/test.comp:80`):
  WW3's bit-for-bit criterion.

### The lab's comparator

`nccmp-tol` compares the `ww3.*.nc` that `ww3_ounf` writes, field by field, against
`kokkos/tools/nccmp-tol/tolerances.txt` `(v)` (`scripts/ww3-gpu.sh:268-280`). `ww3_ts2` and
`ww3_tr1` have no `ww3_ounf` input `(v)` (`ls regtests/<test>/input*/ww3_ounf*`), so for them only
`test.comp` judges, on native output.

**Pick:** WW3's harness for the build and the runs, both comparators for the verdict.

## 5. Design

`scripts/hoff_matrix.sh` (shell, `set -euo pipefail`, shellcheck clean, `--self-test`,
`--dry-run`, `--list`, `--only <test>[:<switch>]`), behind `just ww3-gpu-matrix`, entering
`picasso/labs/pratico#ww3` itself as `ww3-gpu.sh` does. CPU only: Kokkos on OpenMP. It refuses
`--gpu` (exit 64): GPU runs stay the owner's, supervised (`AGENTS.md`).

1. **Select.** Read `$WW3/regtests/bin/matrix.base` at the pin; keep an entry when its switch
   file contains `NL1`, has none of `MPI`, `OMPG`, `OMPH`, and the line has no `-p` or `-m`. Keep
   the entry's whole option list: entries that share a test, switch and input directory differ by
   `-g`, `-o` or `-w` (`ww3_ts1` has four on `input_10ms`), and each is its own case. `--list`
   prints them, one per line.
2. **Kokkos package.** Build and install `kokkos/` with the `openmp-release` preset into
   `kokkos/build/openmp-release/install`, the step `ww3-gpu.sh` already does (`ww3-gpu.sh:183-191`).
3. **Stage.** Copy `WW3/regtests/<test>/` to `build/ww3-gpu-matrix/regtests/<test>/`, and write
   beside the original switch a `switch_<s>_KOKKOS` with the token `KOKKOS` appended. Inputs WW3
   keeps off git (`ww3_tp2.15`'s `wind.nc`, `density.nc`, `momentum.nc`) are copied in as
   `$WW3/model/bin/ww3_from_ftp.sh` copies them, from the tarball it names: pratico's
   `ww3-regtest-data` package, a fixed-output derivation with the tarball's SHA-256
   (`just ww3-gpu-matrix-fetch`, update of 2026-10-15). `WW3/` is
   never written: the run ends with `git -C WW3 status --porcelain` and fails if it is not empty.
4. **Run.** From `build/ww3-gpu-matrix/regtests/`, per entry, with `path_build_root` shared, the
   entry's own options kept and only `-s` and `-w` replaced (`$WW3/model` is the source argument,
   as `matrix.base` passes `ww3='../model'`, `:270`):
   - `CMAKE_OPTIONS="-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH=<prefix>" WW_KOKKOS_SNL1=0 run_cmake_test <options> -s <s>_KOKKOS -w work_F_<n> $WW3/model <test>`;
   - the same with `WW_KOKKOS_SNL1=1 ... -w work_K_<n>`; its log must say `Kokkos DIA (W3SNL1) enabled.`;
   - `-o netcdf` is added when the test has a `ww3_ounf` input.
5. **Compare.** `test.comp <test> work_F_<n> work_K_<n>` (byte verdict, the count of differing files) and,
   when `ww3.*.nc` exists, `nccmp-tol work_F/... work_K/...` (tolerance verdict and the variables
   judged). An entry that does not build or does not run is recorded as such and the matrix goes
   on.
6. **Record.** `build/ww3-gpu-matrix/summary.tsv`, one row per entry; the committed copy is
   `docs/data/hoff_matrix_<date>.tsv` with a header naming the date, the commit of each repository,
   the machine and `just ww3-gpu-matrix`. Exit 0 when no tolerance verdict fails, 1 otherwise.

Deterministic: the selection at a given pin; the runs, up to the OpenMP backend's thread order
(the build keeps `-ffp-contract=off`; see §6).

*Update, 2026-10-15:* at the owner's request the driver moved from `scripts/hoff_matrix.sh` to a
CMake/CTest project, [`regtests/CMakeLists.txt`](../../regtests/CMakeLists.txt) with the scripts in
`regtests/cmake/`, and `scripts/hoff_matrix.sh` was removed. Configure reads `matrix.base` with the
selection of step 1 and registers one CTest test per entry, `NNN:<test>:<switch>`, labelled `entry`
and `serial` or `omp`, around the fixtures `setup:kokkos` and `setup:stage` (steps 2 and 3) and
`record:summary` (step 6, `<build>/summary.tsv`); `selection:count` fails when a WW3 pin changes the
number of entries (100 serial, 25 `omp` at `WW3@6d1bcdfe`). Each entry test is steps 4 and 5 for one
entry. The configure options are `WW3GPU_SELECTION` (`serial`, the default, or `omp`), `WW3GPU_WW3`,
`WW3GPU_DATA` (WW3's ftp data), `WW3GPU_STRICT` (default `ON`: a file `test.comp` finds different
fails the entry, the answer to the first question of §11 now that the DIA is byte-identical) and
`WW3GPU_OMP_THREADS` (`OMP_NUM_THREADS` of each run and `matrix.base`'s `$np`; the recipe sets it to
`nproc --all` divided by the jobs). The `omp` selection exports `OMP_MAX_ACTIVE_LEVELS=2` for both
runs (`regtests/cmake/run_entry.cmake`). The recipes are `just ww3-gpu-matrix [jobs] [serial|omp]
[ctest args]` (`jobs=N` and `selection=S` are accepted too; the record is
`build/ww3-gpu-matrix-<selection>/summary.tsv`), `just ww3-gpu-matrix-list [serial|omp]`,
`just ww3-gpu-matrix-fetch` (the old `--fetch-data`) and `just ww3-gpu-matrix-selftest` (the old
`--self-test`); `--only`, `--dry-run` and `--omp` became `ctest -R`, `ctest -N` and the `omp`
selection, as in `just ww3-gpu-matrix 8 serial -R ww3_ts1` (v) (`justfile`, read 2026-10-15). CTest is only the
runner: every test it runs is one of WW3's regtests, with the Fortran DIA and with the Kokkos DIA,
so this is not a test of the port's own and ADR-0010 holds. The tables of 2026-10-14 and 2026-10-15
were written by the shell script; no table from the CTest driver is committed yet ⚠.

## 6. Parity and physics impact

None on the model: no kernel, Fortran or tolerance changes. The tolerances stay those of
`tolerances.txt`; this WFIP adds cases, not slack. `test.comp` is expected to report differences,
since L2 is not byte-identical even on `ww3_ts1` (`W3SNL1_KOKKOS.md`); it is recorded, not fatal.
A tolerance failure on any entry is reported and stops a status change in `kokkos/ports.tsv`; it
never widens a tolerance.

*Update, 2026-10-15:* the first full run did find a defect, not slack: the `AF11` table of the
Kokkos `INSNL1` used gfortran's `-O0` chain for `x**11`; with the `-O3` chain the matrix is
byte-identical on all 100 entries (`docs/data/hoff_matrix_2026-10-15.tsv`,
`docs/log/2026-10-14-snl1-bitwise-af11.md`).

## 7. Verification plan and definition of done

- [x] The selection matched the grep in the Appendix (100 entries at `WW3@6d1bcdfe`): `just ww3-gpu-matrix --list | wc -l`
- [x] The script passed its self-test and shellcheck: `bash scripts/hoff_matrix.sh --self-test && shellcheck scripts/hoff_matrix.sh`
- [x] A dry run printed every command and wrote nothing under `WW3/`: `just ww3-gpu-matrix --dry-run && git -C WW3 status --porcelain`
- [x] One entry ran end to end on the CPU with both verdicts: `just ww3-gpu-matrix --only ww3_ts1:ST4`
- [x] The full matrix ran on the owner's workstation and its table was committed with its header: `just ww3-gpu-matrix` → `docs/data/hoff_matrix_<date>.tsv`
- [x] A dated log entry recorded the wall time, the failures and what they mean: [`docs/log/2026-10-14-snl1-bitwise-af11.md`](../log/2026-10-14-snl1-bitwise-af11.md) (serial, with the failure entries it links) and [`docs/log/2026-10-15-hoff-matrix-omp.md`](../log/2026-10-15-hoff-matrix-omp.md) (OpenMP)
- [x] The W3SNL1 port report and the `regtested` cell of `kokkos/ports.tsv` cited the table: `python3 scripts/port_score.py --check`

*Update, 2026-10-15:* the commands of the boxes above are those of `scripts/hoff_matrix.sh`, which
ticked them; the script is gone (§5). Their equivalents now are `just ww3-gpu-matrix-list`,
`just ww3-gpu-matrix-selftest`, `just ww3-gpu-matrix 8 serial -N` for the dry run,
`just ww3-gpu-matrix 8 serial -R ww3_ts1` for one test, and `just ww3-gpu-matrix` for the whole
selection; none of them is ticked again here ⚠ (not run on this driver by this update).

## 8. Risks, limitations, and honest caveats

- **Time.** One `run_cmake_test` builds WW3; 100 entries with two runs each is hours of CPU ⚠
  (not measured). `--only` keeps iteration cheap; the shared `path_build_root` saves the second
  build of each entry, not the first.
- **Serial only.** Phase 1 calls the shim inside `!$OMP CRITICAL` and has never run under MPI
  ⚠. The 658 other entries, MPI and OpenMP among them, are out of scope here.

  *Update, 2026-10-15:* the owner opened the shared-memory OpenMP entries, CPU only (§11).
  `scripts/hoff_matrix.sh --omp` keeps an entry when its switch has `NL1` and `OMPG` or `OMPH`,
  no `MPI`, and the line has no `-p` and no `-m`: 25 entries, all `ww3_ts3` with `OMPG`, `ST1` to
  `ST6` × `PR1`/`PR2`/`PR3` × `UQ`/`UNO` `(v)` (`just ww3-gpu-matrix --list --omp`, `WW3@6d1bcdfe`).
  Their 25 `mww3_test_05` twins (`-m grdset_a`) are left out: `ww3_multi` runs three grids in one
  process and the port refuses `IMOD > 1` `(v)` (`model/src/w3initmd.F90:754-759`). Each entry keeps
  `-f -O -n $np`; `matrix.base` leaves `$np` to the site's matrix script, so `hoff_matrix.sh` puts
  `OMP_NUM_THREADS` there, and `run_cmake_test` exports `OMP_NUM_THREADS=<n>` for the run `(v)`
  (`regtests/bin/run_cmake_test:1515-1520`). The first try aborted every Kokkos run at the first
  DIA call, `Kokkos::OpenMP: Nested parallelism requires the maximum active levels to be larger
  than 1`, because `W3SRCE` runs inside `W3WAVE`'s `!$OMP PARALLEL` region
  (`model/src/w3wavemd.F90:2240-2243`, the shim at `w3srcemd.F90:1275`) `(v)` (entries 1 to 4,
  `ran=fortran-only`). With `OMP_MAX_ACTIVE_LEVELS=2` exported for both runs, all 25 entries ran
  and are byte-identical, `test.comp` 0 of 20 files and `nccmp-tol` PASS on `hs` `(v)`
  (`OMP_MAX_ACTIVE_LEVELS=2 just ww3-gpu-matrix --omp --jobs 4`, 19 min on an i9-14900 at 8 threads
  per entry; `docs/data/hoff_matrix_omp_2026-10-15.tsv`). The `!$OMP CRITICAL` still serialises the
  calls, so this shows the answer does not change under OpenMP, not that the DIA runs in parallel.
  Whether a nested Kokkos team inside the critical section costs or gains time is not measured ⚠.
  The 161 `NL1` entries with `MPI` are still out of scope and were not run.
- **Two regtests have no gridded output** (`ww3_ts2`, `ww3_tr1`): only the byte verdict, which is
  expected to say "differ"; a tolerance check on point output (`ww3_ounp`) is a later step.
- **What `nccmp-tol` judges** is what each test's `ww3_ounf` writes; fields beyond `hs` appear only
  where the test asks for them.
- **A non-DIA failure** (a switch that does not build with `KOKKOS`, a test broken upstream) shows
  as a failed entry, not as a kernel defect; the log entry sorts them.

## 9. Alternatives considered

- **Do nothing**: L2 stays one regtest, and the next port starts from the same thin evidence.
- **Extend `ww3-gpu.sh` with more cases by hand**: copies each test's inputs into the lab's own
  driver, drifting from WW3's harness; the matrix and `test.comp` are what NOAA runs.
- **Write the `KOKKOS` switches into `WW3/regtests/` on the fork branch**: works, but edits the
  read-only tree for test plumbing; staging leaves the fork as it is.
- **An `L2_test_snl1_replay` GoogleTest** (`W3SNL1_KOKKOS.md`, "GoogleTest, the WW4 way"): it
  would wrap one case in CTest, and ADR-0010 (2026-10-14) rules out a test of the port's own; the
  regtests run by this script are the test.
- **Run it in CI**: the runner has no time budget for hours of builds; h1 stays the CI check.

## 11. Open questions

- Should `test.comp` "differ" ever fail the matrix? **Default:** no; it is recorded, and only
  `nccmp-tol` decides, until L2 is byte-identical. The owner decides.
- Add fields to each staged `ww3_ounf` input so `nccmp-tol` judges more than `hs`? **Default:** no;
  the test's own output only, so the comparison stays WW3's case as written; the owner decides
  after the first table.
- Include the MPI and OpenMP entries? **Default:** no, serial only, until phase 1 leaves the
  critical section. The owner decides. *Update, 2026-10-15:* the owner decided yes for the
  shared-memory OpenMP entries, CPU only; MPI stays out. `--omp` runs them (§8); they need
  `OMP_MAX_ACTIVE_LEVELS=2`, and whether the shim should set nesting itself instead of the caller's
  environment is a question for the port, not for this script.
- A weekly run on a schedule? **Default:** no; by hand on the owner's workstation.

## Appendix

### Checked live

- `WW3/regtests/bin/matrix.base` at `6d1bcdfe`, 2026-10-14: 758 `echo "$rtst ..."` lines; 100 kept by
  the selection of §5.1 (`grep -o 'echo "\$rtst [^"]*"' bin/matrix.base`, each switch file checked
  for `NL1` and for `MPI`/`OMPG`/`OMPH`, lines with `-p` or `-m` dropped).
- `run_cmake_test:239,400-402,412,447,481,522`, `test.comp:80`, `model/src/CMakeLists.txt:24-36`,
  `model/src/w3kokkosmd.F90:109,158`: read 2026-10-14 at `6d1bcdfe`.
- `SPECTRUM%NK`/`%NTH` of each candidate's `ww3_grid*.nml`, and the presence of `ww3_ounf*`, read
  2026-10-14.
- CI health h1 on `main` at `6b7003b`, 2026-10-15: `hs ... 9.879e-08 PASS`, "1 judged, 25 unlisted".

- `matrix.base` at `6d1bcdfe`, 2026-10-15: of the `NL1` entries, 100 serial, 50 with `OMPG` and no
  `MPI` (25 `ww3_ts3`, 25 `mww3_test_05` with `-m`), 161 with `MPI`; `run_cmake_test:86,154,157,1515-1520`
  (`-O`, `-n`, the `OMP_NUM_THREADS` export), `w3initmd.F90:754-759`, `w3wavemd.F90:2240-2243`,
  `w3srcemd.F90:1270-1278` read the same day.

### Not checked

- Whether every one of the 100 switches builds with `KOKKOS` appended ⚠.
- The wall time of one entry and of the matrix ⚠.
- Whether `-o netcdf` is accepted by each test's post-processing as written ⚠.
