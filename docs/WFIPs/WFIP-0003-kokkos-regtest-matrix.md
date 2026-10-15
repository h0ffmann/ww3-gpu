# WFIP-0003: The Kokkos DIA through WW3's own regtest harness, on the h0ffmann/WW3 fork

| | |
|---|---|
| **Status** | Draft |
| **Author** | M. Hoffmann (asked 2026-10-14 in the project thread: "how to use Kokkos code + ww3 regtest tooling, in a automated way, using the WW3 fork") |
| **Created** | 2026-10-14 |
| **Deliverable** | D4, D5 |
| **Related** | [`kokkos/port_report/W3SNL1_KOKKOS.md`](../../kokkos/port_report/W3SNL1_KOKKOS.md) ("How it was regtested"); [`scripts/hoff.sh`](../../scripts/hoff.sh) (the L2 replay it generalises); [`kokkos/tests/L2_replay.sh`](../../kokkos/tests/L2_replay.sh) (the `work_a`/`work_b` pairing); [`docs/AGENTS_KOKKOS_202609.md`](../AGENTS_KOKKOS_202609.md) §1.5 item 4 and §3.3; [ADR-0002](../ADRs/ADR-0002-port-order-wall-time.md); [ADR-0010](../ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md) (since 2026-10-14 this matrix and `hoff.sh` are the port's only tests) |
| **Effort** | M: one shell script and one recipe; no kernel, no Fortran, no WW3 edit; most of the work is the first full run and reading its failures |
| **Gain** | `proposal` (D4: WW3's own comparison, `test.comp`, beside the lab's comparator; D5: the Kokkos DIA validated against the Fortran on 100 WW3 configurations instead of 1); `lab/dev-loop` (every later port reuses the matrix) |
| **Effort vs Gain** | do next: L2 rests on one regtest today, and the next port (`W3SDS4`, row 1 of `kokkos/PORT_RANKING.md`, #118) would inherit the same thin check |
| **Depends on** | the `hoff/kokkos-snl1` branch of h0ffmann/WW3 at the pin; the owner's workstation for the full run (CPU only); nothing from other WFIPs |
| **Blocked by** | none |
| **Risk** | `run_cmake_test` rebuilds WW3 for every entry, so 100 entries take hours, and a failure at that scale is hard to read; the selection is the lever (§11) |

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | M. Hoffmann, with an agent |
| **Tasks** | none needed: one script, one recipe and one run; §7 is the task list |
| **Tests** | `scripts/hoff_matrix.sh --self-test` (the selection, the staging and the verdict logic on fixtures) |
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
outputs `(v)` (h1 log: "1 judged, 25 unlisted"). `hoff.sh` copies `ww3_ts1`'s input but does not
use WW3's harness `(v)` (`scripts/hoff.sh:224`).

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

Before: `just smoke-hoff`, one case, one verdict. After:

```text
$ just hoff-matrix --list | head -2
ww3_ta1  -s ST4 -i input_UPD0F_O ...
ww3_ta1  -s ST4 -i input_UPD0F_U ...
$ just hoff-matrix
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
`kokkos/tools/nccmp-tol/tolerances.txt` `(v)` (`scripts/hoff.sh:268-280`). `ww3_ts2` and
`ww3_tr1` have no `ww3_ounf` input `(v)` (`ls regtests/<test>/input*/ww3_ounf*`), so for them only
`test.comp` judges, on native output.

**Pick:** WW3's harness for the build and the runs, both comparators for the verdict.

## 5. Design

`scripts/hoff_matrix.sh` (shell, `set -euo pipefail`, shellcheck clean, `--self-test`,
`--dry-run`, `--list`, `--only <test>[:<switch>]`), behind `just hoff-matrix`, entering
`nix-config/labs/pratico#ww3` itself as `hoff.sh` does. CPU only: Kokkos on OpenMP. It refuses
`--gpu` (exit 64): GPU runs stay the owner's, supervised (`AGENTS.md`).

1. **Select.** Read `$WW3/regtests/bin/matrix.base` at the pin; keep an entry when its switch
   file contains `NL1`, has none of `MPI`, `OMPG`, `OMPH`, and the line has no `-p` or `-m`. Keep
   the entry's whole option list: entries that share a test, switch and input directory differ by
   `-g`, `-o` or `-w` (`ww3_ts1` has four on `input_10ms`), and each is its own case. `--list`
   prints them, one per line.
2. **Kokkos package.** Build and install `kokkos/` with the `openmp-release` preset into
   `kokkos/build/openmp-release/install`, the step `hoff.sh` already does (`hoff.sh:183-191`).
3. **Stage.** Copy `WW3/regtests/<test>/` to `build/hoff-matrix/regtests/<test>/`, and write
   beside the original switch a `switch_<s>_KOKKOS` with the token `KOKKOS` appended. Inputs WW3
   keeps off git (`ww3_tp2.15`'s `wind.nc`, `density.nc`, `momentum.nc`) are copied in as
   `$WW3/model/bin/ww3_from_ftp.sh` copies them, from the tarball it names, fetched once into
   `build/ww3-data/<version>/` by `scripts/hoff_matrix.sh --fetch-data`. `WW3/` is
   never written: the run ends with `git -C WW3 status --porcelain` and fails if it is not empty.
4. **Run.** From `build/hoff-matrix/regtests/`, per entry, with `path_build_root` shared, the
   entry's own options kept and only `-s` and `-w` replaced (`$WW3/model` is the source argument,
   as `matrix.base` passes `ww3='../model'`, `:270`):
   - `CMAKE_OPTIONS="-DWW_KOKKOS=ON -DCMAKE_PREFIX_PATH=<prefix>" WW_KOKKOS_SNL1=0 run_cmake_test <options> -s <s>_KOKKOS -w work_F_<n> $WW3/model <test>`;
   - the same with `WW_KOKKOS_SNL1=1 ... -w work_K_<n>`; its log must say `Kokkos DIA (W3SNL1) enabled.`;
   - `-o netcdf` is added when the test has a `ww3_ounf` input.
5. **Compare.** `test.comp <test> work_F_<n> work_K_<n>` (byte verdict, the count of differing files) and,
   when `ww3.*.nc` exists, `nccmp-tol work_F/... work_K/...` (tolerance verdict and the variables
   judged). An entry that does not build or does not run is recorded as such and the matrix goes
   on.
6. **Record.** `build/hoff-matrix/summary.tsv`, one row per entry; the committed copy is
   `docs/data/hoff_matrix_<date>.tsv` with a header naming the date, the commit of each repository,
   the machine and `just hoff-matrix`. Exit 0 when no tolerance verdict fails, 1 otherwise.

Deterministic: the selection at a given pin; the runs, up to the OpenMP backend's thread order
(the build keeps `-ffp-contract=off`; see §6).

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

- [x] The selection matched the grep in the Appendix (100 entries at `WW3@6d1bcdfe`): `just hoff-matrix --list | wc -l`
- [x] The script passed its self-test and shellcheck: `bash scripts/hoff_matrix.sh --self-test && shellcheck scripts/hoff_matrix.sh`
- [x] A dry run printed every command and wrote nothing under `WW3/`: `just hoff-matrix --dry-run && git -C WW3 status --porcelain`
- [x] One entry ran end to end on the CPU with both verdicts: `just hoff-matrix --only ww3_ts1:ST4`
- [x] The full matrix ran on the owner's workstation and its table was committed with its header: `just hoff-matrix` → `docs/data/hoff_matrix_<date>.tsv`
- [ ] A dated log entry recorded the wall time, the failures and what they mean: `docs/log/<date>-hoff-matrix.md`
- [x] The W3SNL1 port report and the `regtested` cell of `kokkos/ports.tsv` cited the table: `python3 scripts/port_score.py --check`

## 8. Risks, limitations, and honest caveats

- **Time.** One `run_cmake_test` builds WW3; 100 entries with two runs each is hours of CPU ⚠
  (not measured). `--only` keeps iteration cheap; the shared `path_build_root` saves the second
  build of each entry, not the first.
- **Serial only.** Phase 1 calls the shim inside `!$OMP CRITICAL` and has never run under MPI
  ⚠. The 658 other entries, MPI and OpenMP among them, are out of scope here.
- **Two regtests have no gridded output** (`ww3_ts2`, `ww3_tr1`): only the byte verdict, which is
  expected to say "differ"; a tolerance check on point output (`ww3_ounp`) is a later step.
- **What `nccmp-tol` judges** is what each test's `ww3_ounf` writes; fields beyond `hs` appear only
  where the test asks for them.
- **A non-DIA failure** (a switch that does not build with `KOKKOS`, a test broken upstream) shows
  as a failed entry, not as a kernel defect; the log entry sorts them.

## 9. Alternatives considered

- **Do nothing**: L2 stays one regtest, and the next port starts from the same thin evidence.
- **Extend `hoff.sh` with more cases by hand**: copies each test's inputs into the lab's own
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
  critical section. The owner decides.
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

### Not checked

- Whether every one of the 100 switches builds with `KOKKOS` appended ⚠.
- The wall time of one entry and of the matrix ⚠.
- Whether `-o netcdf` is accepted by each test's post-processing as written ⚠.
