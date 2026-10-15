# ADR-0011: The port moves in large units, with discipline; WW3's regtests run at checkpoints, not after each routine

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-15 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | The size of a port step and when WW3's regtests run during it: `/port-next`, the `ww3-port` skill, the gate table of `AGENTS.md` for `kokkos/`, and the plans that schedule a port. Not what validates a port (ADR-0010, unchanged), not the bit-for-bit rule, not "no timing without parity", not the port order (ADR-0002, unchanged), not the GPU rule |
| **Related** | [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md) (WW3's regtests are the port's only tests; this ADR sets when they run); [ADR-0002](ADR-0002-port-order-wall-time.md) (port order); [`docs/ST4_KOKKOS_PLAN_202610.md`](../ST4_KOKKOS_PLAN_202610.md) (the first unit under this ADR); [WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md) (the regtest matrix); issue [#45](https://github.com/h0ffmann/ww3-gpu/issues/45) |

## Context

The owner, 2026-10-15, verbatim: "larger portions will be ported like entire ST4, due to slowness
of regtests, regtests will be execute only after big moves in the codebase, or when work is
finished. If we translate small part by part with regtest validation upfront, this project can't
be completed in 6 months. So it's better to scale, porting WITH DISCIPLINE, doing a deep review of
existing tooling for port (docker, picasso, i9, rtx4090)." And later the same day: "consider this
incoming PR as self-improving, so policies of porting may be adjusted to fit more efficient
porting os ST4, keeping bit-parity without regtests".

### What one routine cost

`W3SNL1` with `INSNL1` is the only port so far. Its history, from `git log` and `docs/log/` (v,
read 2026-10-15):

| Date | Event |
|---|---|
| 2026-09-15 | Kokkos tree, the DIA kernel, L1 fixtures; L1 bit-identical on three presets; kernel benchmark on the i9-14900 and the RTX 4090 |
| 2026-10-08 | Port order by wall time (ADR-0002) |
| 2026-10-14 | WW3 caller patch on the fork (`6d1bcdfe`); health h1 in CI; ADR-0010 removes the L1 tests |
| 2026-10-14 to 10-15 | First serial matrix: 5 of 100 entries fail |
| 2026-10-15 | The AF11 defect found and fixed: all 100 serial entries byte-identical ([log](../log/2026-10-14-snl1-bitwise-af11.md)) |
| 2026-10-15 | OpenMP matrix, 25 of 25 byte-identical after the nesting fix ([log](../log/2026-10-15-hoff-matrix-omp.md)) |
| 2026-10-15 | One shim context per grid, for `ww3_multi` (fork `bb08623a`) |

That is 30 calendar days for 2 of 1023 routines (`kokkos/ports.tsv`, `ww3_routines`). The two
routines hold 48.4 % of the profiled run time (`kokkos/PORT_RANKING.md`, gprof on `bench_small`,
`WW3@6d1bcdfe`) (v). Sixteen other routines take more than 10 ms in that profile (v). At the pace
of `W3SNL1`, one routine per 30 days, those 16 routines take about 480 days, and the 8 routines
of ST4 alone about 240 days ⚠ (a crude extrapolation, not a measurement). The project has 6
months.

### What a regtest costs

Measured on the i9-14900 (32 hardware threads), CPU only, `nix-config/labs/pratico`:

| Run | Wall time | Source |
|---|---|---|
| `just smoke-ww3-gpu` (`ww3_ts1`), `ww3_shel` alone | 2.67 s Fortran, 2.81 s Kokkos | `kokkos/port_report/W3SNL1_KOKKOS.md` (v) |
| Health h1 in CI (`ww3_ts1`, with the `--ww3` pair), end to end | 276 s to 520 s, 15 runs on 2026-10-15 | `gh run list` (v) |
| `just full-ww3-gpu` (`bench_small`) | 152.7 s Fortran, 217.9 s Kokkos | the same port report (v); machine not stated ⚠ |
| Serial matrix, 100 `NL1` entries, `-j8` | 22 min 37 s; 6790 entry-seconds | `docs/data/hoff_matrix_2026-10-15.tsv` (v) |
| The same before the warm build directories | 12 762 entry-seconds | `docs/data/hoff_matrix_2026-10-14.tsv` (v) |
| OpenMP matrix, 25 entries, `-j4` | 19 min 19 s; 4203 entry-seconds | `docs/data/hoff_matrix_omp_2026-10-15.tsv` (v) |
| The whole matrix, 311 entries (100 serial, 50 OpenMP, 161 MPI) | about 1.5 h to 2.1 h warm, 3 h to 4 h after a toolchain change | extrapolated from the rows above and a partial local MPI run ⚠ |

Most of an entry is the WW3 build: every entry builds WW3 in its own `build_N`
(`regtests/cmake/run_entry.cmake:36`), 35 s to 42 s cold ⚠ (local timestamps), and entries
with the same switch share nothing. A regtest after every routine and every fix costs hours per
step, and a large unit has many steps.

### What the regtests found, and what that means for this decision

All three defects of the `W3SNL1` port were found by WW3's regtests inside WW3, at `-O3`: the
AF11 power expansion (the L1 fixtures, built at `-g`, could not see it), the OpenMP nesting
abort (OMPG entries) and the single-grid shim (`ww3_multi` entries) (v, the logs above). A later
regtest still finds these defects. It finds them later, and in a larger diff. The decision
below accepts that cost and adds the controls that keep it small.

## Decision

**A port step is a unit: a physics package or a group of routines that WW3 calls together, such
as all of ST4 (`w3src4md.F90`). Inside a unit, the routines are translated with discipline and
without regtests. WW3's regtests run at checkpoints: after a big move in the codebase, and when
the unit is finished.** ADR-0010 is unchanged: the regtests remain the only tests of the port,
and nothing is "ported", L2 or timed without them.

### Checkpoints

A checkpoint runs `just smoke`, `just full-ww3-gpu` and `just ww3-gpu-matrix <jobs> all` for the
switch of the unit, on the CPU. A checkpoint is due at each of these events, and before the next
one starts:

1. a port unit is finished (all its routines translated and wired);
2. the `WW3/` pin moves, or the fork branch changes the caller hunks;
3. the toolchain changes: a picasso (`nix-config`) bump, a new gfortran, Kokkos or CUDA;
4. the memory contract changes, such as phase 2 (device-resident spectrum);
5. before any status in `kokkos/ports.tsv` moves to L2, before any timing enters a table, before
   a release, and before a GPU run under the owner's supervision.

Between checkpoints, a change to Kokkos code needs only the build gates of `AGENTS.md`
(`just kokkos-test serial-debug`, `just kokkos-test openmp-release`, the CUDA preset compiled).
CI keeps health h1 on every pull request, unchanged.

### The discipline that replaces the per-routine regtest

1. **One run-time flag per routine**, all off by default (`WW_KOKKOS_SNL1`, `WW_KOKKOS_SDS4`, and
   so on). A unit is never the default path before its checkpoint passes, and a failing
   checkpoint is bisected by turning on one flag at a time.
2. **The `-O3` object code is read before the translation.** A first task lists every power,
   folded constant and reduction order of the unit from gfortran's `-fdump-tree-optimized`, with
   WW3's own Release flags. This is the AF11 lesson, applied before the code exists.
3. **A line-by-line translation review** by a fresh reviewer for each routine, against the
   Fortran at the pin, before the routine is committed. It reads; it runs nothing.
4. **Bit parity is argued statically between checkpoints**, by the route
   [ADR-0001](ADR-0001-proof-language.md) accepted: a comparison of the optimised GIMPLE
   (`-fdump-tree-optimized`) of the Fortran routine, built with WW3's Release flags, and of the
   C++ kernel, built with the `openmp-release` flags. For each routine it shows the same `float`
   operations in the same order, the same libm calls (`expf`, `logf`, `powf`, `__powisf2`), the
   same constants after folding, and no FMA. A difference is a defect of the port, fixed before
   the commit, or a question to the owner. This is evidence read from the compilers, not a test
   of the port and not a fixture (ADR-0010). A helper of one `float` input (an integer power
   tree, a table lookup) may also get ADR-0001's exhaustive sweep over all 2^32 inputs, kept under
   `proof/` beside `proof/snl1_cons`, never in CTest. It covers the host build only: CUDA's device code and
   libm are outside it. The port report records it as "static parity, ⚠ until the checkpoint";
   the checkpoint remains the verdict.
5. **One commit per routine, in profile order** (ADR-0002), so a checkpoint failure maps to a
   small set of commits.
6. **Every quirk of the Fortran is listed in the plan and kept**, with one comment line in the
   kernel that cites the source line.
7. **The ledger says the truth between checkpoints**: status `ported`, `regtested` = `no:
   waiting for the checkpoint of <unit>`, every parity claim ⚠, no number in a table.
8. **A checkpoint failure stops new translation** until the failure is explained, in a
   `docs/log/` entry.

## Tooling review (2026-10-15)

The owner asked for a deep review of the port tooling before the first unit. Every fact is (v)
unless marked; "[local]" marks a build artefact on the workstation, not a record.

| Tool | What it gives a large unit | What it lacks |
|---|---|---|
| **picasso** (`nix-config/labs/pratico`, `be76132`) | one nixpkgs lock (`6510408d`, 2026-10-10): gfortran 16.2.0, Open MPI 5.0.11, netCDF-C 4.10.1, Kokkos 5.2.2 (Serial and OpenMP; a CUDA build for `ADA89`), PT-Scotch 7.0.12, NOAA's regtest data 7.14.2 with its SHA-256 (`docs/TOOLCHAIN.md`) | the CUDA toolkit comes from nixpkgs, not pinned by version (nvcc 12.9.86 today); its README still says GCC 15 and Kokkos 5.2.0 ⚠; the toolchain moved from gfortran 15.3.0 and Kokkos 5.2.0 to 16.2.0 and 5.2.2 on 2026-10-15, after the committed byte-identical matrices, so those verdicts are not yet repeated on the current toolchain |
| **Docker** (`docker/Dockerfile`, `just dbuild`, `just dsmoke*`, `just dfull*`) | an Ubuntu 26.04 image of 234 MB [local] with Kokkos 5.2.2 (Serial, OpenMP), WW3 with and without `KOKKOS`, `ww3_ts1` and `bench_small`; Trivy in CI (245 s to 268 s) | no MPI, no CUDA, no `regtests/` and no regtest data, so no matrix; Ubuntu's gfortran (15.2 ⚠) is not the reference toolchain, so a Docker verdict is not a bit-for-bit verdict (one `ww3_ts1` run gave the same bytes as the Nix build [local], which does not generalise ⚠) |
| **i9-14900** (24 cores, 32 threads, 184 GiB [local]) | every matrix record; the matrix reaches 3.6x to 5.0x concurrency | the RAM is not recorded in any `bench/results/` record; one machine, so the matrix is not run in parallel elsewhere |
| **RTX 4090** (24 GiB, `sm_89`) | the 2026-09-15 kernel benchmark of `W3SNL1` | driver 595.84 in the record, 615.71.09 today [local]; no CUDA run of the port inside WW3 exists; `expf` and `logf` bit equality with glibc is unproven (`AGENTS.md`, "Bit for bit"); GPU runs need the owner |
| **Regtest matrix** (`regtests/`, CTest) | one test per `matrix.base` entry, F and K runs in one executable, `test.comp` and `nccmp-tol`, `ww3_self` excuses | selection hard-coded to `NL1` (`regtests/cmake/ww3gpu_common.cmake:88`); toggle and log line only for `W3SNL1` (`run_entry.cmake:99-106`, `:162`, `:170`); no ST4 selection (137 of 758 entries have `ST4`; 8 of them have no `NL1` and cannot build with `KOKKOS` today); no build shared between entries with the same switch; no committed record of `all` |
| **`/port-next`**, `ww3-port`, `port_score.py`, `ports.tsv` | the account check, the order check, the scaffold, the traps of the `W3SNL1` port | one routine per call; `port_score.py --check` needs one port report per row; no unit column; the kernel benchmark covers `W3SNL1` only |
| **CI** | health h1 (`ww3_ts1`, ST4 and NL1, `--ww3-gpu --cpu`), the Kokkos presets with sanitizers, the lint gates | no GPU, no matrix, no MPI |

Work that follows from the review, each item for its own issue and PR:

1. The matrix selects by any physics word and toggles several flags (the list is in
   [`ST4_KOKKOS_PLAN_202610.md`](../ST4_KOKKOS_PLAN_202610.md) §10.3).
2. The `KOKKOS` switch of the fork stops requiring `NL1` (§10.2 of the same plan).
3. A build cache shared by the matrix entries with the same switch, to cut the 35 s to 42 s per
   entry ⚠.
4. The serial and OpenMP matrices repeated on gfortran 16.2.0 and Kokkos 5.2.2, the current
   toolchain, before the first checkpoint of ST4, so that the reference itself is current.
5. The machine record gains RAM and the GPU driver; picasso's README states the versions its lock
   gives.
6. `port_score.py` and `ports.tsv` learn a unit, so one port report can cover a unit.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep a regtest after every routine and every fix | The rule of the `W3SNL1` port. Its pace does not finish the profile in 6 months (Context) |
| Regtest after each routine, smoke only (`ww3_ts1`) | One case, ST4 and NL1 only; it judged `hs` only before the matrix existed, and it missed the five matrix failures of 2026-10-14 |
| Bring back L1 tests to replace the per-routine regtest | ADR-0010 removed them; the L1 fixtures could not see the AF11 defect |
| Port in units and run no regtest until the whole port ends | Defects pile up across units; a failure would map to thousands of lines |

## Consequences

- A port unit goes from plan to checkpoint in days, not weeks. ST4 is planned as one 8-hour
  translation session ([`ST4_KOKKOS_PLAN_202610.md`](../ST4_KOKKOS_PLAN_202610.md)).
- A defect shows up later, in a larger diff. The per-routine flags, the commit order and the
  `-O3` inventory are what keep the search small. If a checkpoint failure takes longer to locate
  than the regtests it saved, the unit was too large; the next plan makes it smaller.
- Long stretches of code sit at status `ported`, unvalidated. The flags default to the Fortran, so
  no user of the fork runs unvalidated code unless they turn a flag on.
- `AGENTS.md` (the gate table and "Running on the GPU"), `CONTRIBUTING.md`, `/port-next` and the
  `ww3-port` skill are changed to this cadence in the same pull request.
- Bit for bit, "no timing without parity", ADR-0002's order and the GPU rule do not change.

## Revisit when

- A checkpoint failure takes more than one working day to locate.
- The matrix gets fast enough (a shared build cache, a second machine) that a run per routine
  costs less than 15 minutes.
- The first two units have passed their checkpoints: then record here how long each took, and how
  many defects each checkpoint found.
