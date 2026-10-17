# ADR-0012: Translate and wire now, with static parity and one review; the full WW3 regtest matrix of the translated units runs on the UFSC H100 node

| | |
|---|---|
| **Status** | Proposed |
| **Date** | 2026-10-16 |
| **Deciders** | Hoffmann |
| **Written by** | Hoffmann, with an agent |
| **Scope** | When WW3's regtests run while the port is translated and wired (goal 42, issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42)): which runs the i9-14900 keeps, which wait for the UFSC H100 node, what a row of `kokkos/ports.tsv` may say in between, and the run-time modes (`WW_KOKKOS_*`, `--kokkos`, `WW3GPU_ROUTINES`) that select what runs on Kokkos. Not what validates a port (ADR-0010, unchanged), not the bit-for-bit rule, not "no timing without parity", not the port order (ADR-0002), not the GPU rule |
| **Related** | [ADR-0011](ADR-0011-port-in-units-regtest-at-checkpoints.md) (checkpoints; this ADR narrows its checkpoint rule, see [Relation to ADR-0011](#relation-to-adr-0011)); [ADR-0010](ADR-0010-kokkos-validated-by-ww3-regtests-only.md); [ADR-0002](ADR-0002-port-order-wall-time.md); [ADR-0001](ADR-0001-proof-language.md) (static parity); [WFIP-0003](../WFIPs/WFIP-0003-kokkos-regtest-matrix.md) (the matrix); [`kokkos/src/fortran_iface/PATCH.md`](../../kokkos/src/fortran_iface/PATCH.md) §7 (the flags); [`H100_OPTIMIZATION_STRATEGY_202610.md`](../H100_OPTIMIZATION_STRATEGY_202610.md) (what follows on the H100) |

## Context

The owner, 2026-10-16: postpone the regtests, because they take hours on the i9 and are better
run on the UFSC H100 node; record how fast the translation now moves, the flags that turn every
part on (FULL) or a list of parts on (PARTIAL), and the default. Later the same day: no regtests
for now; the current work wires every translated routine into WW3 behind its `WW_KOKKOS_*` flag,
verified by building it, and the CPU checkpoint of the `W3WAVE` support unit stands as the last
regtest run.

### What the matrix costs on the i9

Measured on the owner's Intel Core i9-14900 (32 hardware threads), CPU only, the pinned
`picasso/labs/pratico#ww3` shell (v, the records read 2026-10-16):

| Run | Entries | Wall time | Entry-seconds | Record |
|---|---|---|---|---|
| Serial `NL1` matrix, `-j8` (`just hoff-matrix --jobs 8`, now `just ww3-gpu-matrix 8 serial`) | 100 | 22 min 37 s (02:58:30Z to 03:21:07Z) | 6 790 | `docs/data/hoff_matrix_2026-10-15.tsv` |
| OpenMP `NL1` matrix, `-j4`, 8 threads per entry (`OMP_MAX_ACTIVE_LEVELS=2 just hoff-matrix --omp --jobs 4`) | 25 | 19 min 19 s | 4 203 | `docs/data/hoff_matrix_omp_2026-10-15.tsv`, `docs/log/2026-10-15-hoff-matrix-omp.md` |
| `just full-ww3-gpu` (`bench_small`), `ww3_shel` alone | 1 case | 152.7 s Fortran, 217.9 s Kokkos | | `kokkos/port_report/W3SNL1_KOKKOS.md`; machine not stated there ⚠ |
| `just smoke-ww3-gpu` (`ww3_ts1`), `ww3_shel` alone | 1 case | 2.67 s Fortran, 2.81 s Kokkos | | the same port report |

The 19 minutes of the OpenMP matrix are in the OpenMP log and its record, not in the `justfile`
⚠ (no `justfile` line at `e6d0c6e` states a duration).

No run of the whole matrix is committed. What the selection would hold is countable without
running it: configuring the driver at the six-int worktree's pin (`WW3/` at `8e433f99`) with
every routine `WW3GPU_ALL_ROUTINES` lists gives 363 serial, 50 OpenMP and 344 MPI entries, 757 in
all, against 310 for `SNL1` alone and 136 for `SDS4` (v, 2026-10-16):

```bash
nix develop ./picasso/labs/pratico#ww3 --command cmake -S regtests -B <scratch>/all \
  -DWW3GPU_SELECTION=all \
  "-DWW3GPU_ROUTINES=SNL1;SDS4;SIN4;SPR4;SPR0;SPR1;SIN1;SDS1;SPR2;SIN2;SDS2;SPR3;SIN3;SDS3;SPR6;SIN6;SDS6;SWL6;FLX1;FLX2;FLX3;FLX4;FLX5"
# -- ww3-gpu-matrix: 757 all entries of .../matrix.base with FLX1;...;ST6; Kokkos SNL1;...
```

⚠ An estimate, not a measurement: at the measured 68 entry-seconds per serial entry and 168 per
OpenMP entry, and assuming an MPI entry costs as much as an OpenMP one (no MPI record exists),
the 757 entries are about 91 000 entry-seconds; at the concurrency the two records reached (5.0
at `-j8`, 3.6 at `-j4`) that is 5 h to 7 h of wall time, before any rerun of the Fortran that a
differing file triggers (`work_G_<n>`), and more after a toolchain change empties the build
directories (12 762 entry-seconds for the serial matrix cold, `hoff_matrix_2026-10-14.tsv`).
ADR-0011 extrapolated 1.5 h to 2.1 h for the 310 `NL1` entries warm. The command that measures
it is `WW3GPU_ROUTINES=<the list above, comma-separated> just ww3-gpu-matrix 8 all`, whose
`summary.tsv` records each entry's `wall_s`.

### How fast the translation moved

This is translation throughput: routines translated and reviewed per hour of wall-clock time,
counted from commit timestamps. It says nothing about how fast WW3 runs.

- **The session of 2026-10-16 evening** (`goal/42-port-six-expensive`, from `git log --format='%ad
  %s' --date=iso origin/main..goal/42-port-six-expensive`): the first claim commit `a11943c` at
  20:42:14 -03:00 and the last translation `e6d0c6e` at 21:50:03, 67.8 min. In that window the
  data rows of `kokkos/ports.tsv` went from 293 to 305, counted at each commit with
  `git show <commit>:kokkos/ports.tsv | grep -vc '^#\|^routine'`: 12 routines (`W3MPIO`,
  `W3SETG`, `W3IORS`, `W3IOBC`, `W3INIT`, `W3IOGO`, `W3IOPP`, `W3FLDTOIJ`, `W3FLDO`,
  `GSU_CREATE`, `W3IOPE`, `W3IOPO`), each in its own commit whose body records one fresh review
  (v, `git log -1 --format=%B <commit>`). That is 10.6 routines per hour, and 12 425 lines of
  Fortran by the `lines` column of `ports.tsv` (9 085 without `W3MPIO`, whose body compiles to a
  `ret` on the lab switch). The routines were translated by several agents at once; the rate is
  per hour of the session, not per agent.
- **Since ADR-0011's checkpoint rule** (the same row count over `git log --since=2026-10-14
  origin/main -- kokkos/ports.tsv` plus the branch): 2 rows at `5c06641` (2026-10-15 18:41) and
  305 at `e6d0c6e` (2026-10-16 21:50), 303 rows in 27 h 9 min of calendar time, nights included,
  about 11 per hour. The batch of PR #159 (92 routines in one commit, "the easiest left") weighs
  on that figure; the evening's 12 were the largest left by lines.
- **Before**: `W3SNL1` with `INSNL1`, 2 routines in 30 calendar days with a regtest after each
  step (ADR-0011, "What one routine cost").

### What has been regtested, and what has not

The last WW3 regtest run of the port is the pin-move checkpoint of the `W3WAVE` support unit,
2026-10-16, on the i9, CPU only (v, the `Tested:` record of the wiring change and
[`PATCH.md`](../../kokkos/src/fortran_iface/PATCH.md), "The checkpoint, 2026-10-16"):
`scripts/ww3-gpu.sh --ww3-gpu smoke --cpu` and `--ww3-gpu full --cpu` with `--kokkos none`,
`all`, by unit and by part, every run byte-identical to the Fortran of the same executable
(`cmp` of `out_grd.ww3`, `out_pnt.ww3`, `restart.ww3` and the NetCDF file) and to `--ww3`. It did
not run `just ww3-gpu-matrix`, the OpenMP entries or `test.comp`.

The only routines with a matrix record are `W3SNL1` and `INSNL1` (100 serial and 25 OpenMP
entries, byte-identical). ST4 has smoke and full only (`docs/log/2026-10-15-st4-checkpoint.md`).
Every routine translated since carries static parity (ADR-0011's `-O3` GIMPLE and object
comparison) and one fresh review, and no WW3 run.

## Decision

**A translated unit is accepted into the tree on static parity and one fresh review; it is wired
into WW3 behind its `WW_KOKKOS_*` flag, off by default, and verified by building WW3 with it. The
full WW3 regtest matrix of the translated units runs on the UFSC H100 node, on its host CPUs
(`--ww3-gpu --cpu`), not on the i9. The i9 keeps the CPU smoke and full runs (`just smoke-ww3-gpu`,
`just full-ww3-gpu`, pair to pair with `--ww3`) for when the owner calls a checkpoint: a finished
wiring batch or a move of the `WW3/` pin.** The owner's "no regtests for now" (2026-10-16)
applies until he calls one.

1. **Static parity and one review are the bar for a translation.** As ADR-0011 items 2 to 6: the
   `-O3` inventory before the code, a line-by-line review by a fresh reviewer against the Fortran
   at the pin, the GIMPLE and object comparison, one commit per routine. Nothing new is asked.
2. **Wired and not regtested is a legitimate state.** A row whose Kokkos code WW3 calls behind its
   flag has `wired` "yes: <where>" and `regtested` "no: <why>" in `kokkos/ports.tsv`, and the
   port score counts it at the *ported* level, never at *regtested* (`scripts/port_score.py`).
   Wiring is verified by building: WW3 with the `KOKKOS` switch configures, compiles without a
   new warning on a line of the hunks, and links; `just kokkos-test serial-debug` and
   `openmp-release` pass; the CUDA preset compiles.
3. **`WW_KOKKOS_ALL=1` on wired-but-not-regtested parts is for development, not for results.**
   Its output is not evidence of parity, does not enter a table, and is not quoted as "100 %
   Kokkos" without the words "of the wired parts, not regtested".
4. **The matrix is the verdict and nothing skips it.** No status moves to L2, no timing enters a
   table, no release claims a part, and no row's `regtested` becomes "yes" on the strength of the
   matrix without the matrix having run on the same commit with `WW3GPU_STRICT=ON`, every entry
   whose switch has the routine's switch. The bit-for-bit rule binds every one of those runs.
5. **On the H100 node the matrix runs on the CPU first.** The matrix driver has no GPU path, and
   a `--ww3-gpu --gpu` run needs the owner supervising it (`AGENTS.md`, "Running on the GPU"). The
   node serves the matrix for its host cores and because the i9 is the working machine, not for
   its GPU; how many cores it has is not recorded ⚠.

### The modes: FULL, PARTIAL and the default

What runs on Kokkos is chosen at run time; the build is the same (v, `PATCH.md` §7 at the wiring
change, read 2026-10-16).

| Mode | How it is set | What runs on Kokkos |
|---|---|---|
| Default | no `WW_KOKKOS_*` variable set | nothing: every part is the Fortran (0 % Kokkos), since 2026-10-14 |
| PARTIAL | `WW_KOKKOS_<PART>=1` for each part wanted (`WW_KOKKOS_SNL1`, `_SDS4`, `_SIN4`, `_SPR4`, `_ST4_TABLES`, `_SRCE`, `_GATH`, `_SCAT`, `_UWND`, `_UTRN`, `_DZXY`, `_MAP3`, `_MAPT`, `_DISTAB`, and the ST0 to ST6, FLX and SWL parts) | the listed parts |
| FULL | `WW_KOKKOS_ALL=1` | every wired part whose translation covers the switch file; a part that does not cover it stays Fortran |
| Explicit off | `WW_KOKKOS_<PART>=0` | that part is the Fortran, whatever `WW_KOKKOS_ALL` says |

One function, `ww_kokkos_flag` (`kokkos/src/fortran_iface/part_flags.cpp`), resolves every flag
for the shims and for `W3KOKKOS_SETUP`, so the Fortran and the C++ cannot disagree; `W3INIT`
writes one "Kokkos parts" line to `log.ww3` naming where each part ran. FULL means 100 % of the
wired parts: 87.6 % of the run time of the committed profile at the wiring change, with
propagation, I/O and set-up still Fortran (`python3 scripts/port_score.py --write` on that
branch, v 2026-10-16).

`scripts/ww3-gpu.sh --ww3-gpu <case> --kokkos all|none|<PART,...>` sets `WW_KOKKOS_ALL=1`, `=0`,
or `=0` plus the listed parts in the Kokkos run, gives the Fortran run `WW_KOKKOS_ALL=0`, and
prints the parts line and the per-part call counters. Without `--kokkos` it keeps
`WW3GPU_ROUTINES` (default `SNL1`).

The regtest matrix has a PARTIAL selection today: `WW3GPU_ROUTINES=<R>,<R>,... just
ww3-gpu-matrix <jobs> <serial|omp|mpi|all>` selects every `matrix.base` entry whose switch has the
physics word of one of the routines and sets `WW_KOKKOS_<R>=1` for them in the Kokkos run, every
flag 0 in the Fortran run (`regtests/CMakeLists.txt:19-22`, the routines of
`WW3GPU_ALL_ROUTINES`, `regtests/cmake/ww3gpu_common.cmake:52`). It knows the source-term
routines only: the `W3SRCE` and `W3WAVE` support parts are not in `WW3GPU_ALL_ROUTINES`. A FULL
matrix mode, the Kokkos run with `WW_KOKKOS_ALL=1`, is the planned next step for the H100 run and
is not implemented.

## Relation to ADR-0011

This ADR narrows ADR-0011's checkpoint rule; it does not supersede it, unless the owner says it
does. ADR-0011 runs `just smoke`, `just full-ww3-gpu` and `just ww3-gpu-matrix <jobs> all` at each
checkpoint. Under this ADR the matrix part of a checkpoint moves to the H100 node and waits for
it, and the i9 runs the smoke and the full when the owner calls the checkpoint. ADR-0011's
discipline (flags off by default, the `-O3` inventory, the fresh review, static parity, one
commit per routine, the ledger telling the truth) applies unchanged, and so does its item 5: L2,
a timing, a release and a GPU run each still wait for the matrix. ADR-0011's "Revisit when"
named a faster matrix; this ADR is the other way out, a bigger machine.

## Alternatives considered

| Option | Why not |
|---|---|
| Run the matrix on the i9 at every finished unit, as ADR-0011 says | 5 h to 7 h for the 757 entries of the translated source terms ⚠ (Context), each time a unit is finished, while translation moves at about ten routines an hour; the i9 is also the machine the work is done on |
| Run only the `NL1` or `ST4` subsets on the i9 | They cover the operational source terms, not the units translated since (ST0 to ST6, FLX, SWL, `PDLIB`, I/O); a subset is a PARTIAL verdict and would be read as more |
| Stop translating until a matrix runs | The pace of `W3SNL1` (2 routines in 30 days) does not finish in the project's six months (ADR-0011) |
| Count smoke and full as the regtest | One point (`ww3_ts1`) and one 2-D case (`bench_small`), one switch; they missed the five matrix failures of 2026-10-14 (ADR-0011) |

## Consequences

- Translation and wiring continue at the measured pace; the matrix is run once on a larger
  machine instead of many times on the i9.
- **A defect is found later and in a larger diff.** All three defects of the `W3SNL1` port were
  found by the matrix at `-O3`, not by smoke or full (ADR-0011). Between now and the H100 run,
  hundreds of routines will sit wired and unregtested. The per-part flags and the one commit per
  routine are what make the search possible; a matrix failure is bisected one flag at a time.
- **The H100 run is a bottleneck with an owner.** Until the node is available, with the pinned
  toolchain and NOAA's regtest data on it, no row moves to L2 and no number enters a table.
  ⚠ Which H100 node, its host CPU, its core count and whether Nix runs there are not recorded in
  this repository.
- **A matrix on another machine is a new reference.** The verdict is Fortran against Kokkos in
  the same executable, so it holds on any machine; a comparison with a record made on the i9 does
  not. The H100 run records its machine, as the i9 records do.
- **The `regtested` column must keep one meaning.** At the wiring change, rows that passed the
  smoke and the full only (`W3GATH`, `W3SCAT`, `W3UWND`, `W3DZXY`, `W3MAP3`, `W3MAPT`, `DISTAB`,
  `WAVNU2`) say `regtested` "yes"; ADR-0011's checkpoint includes the matrix. Whether "yes" needs
  the matrix is a question for the owner; this ADR proposes that it does, and that smoke-and-full
  rows say so in their text ("yes: smoke and full, matrix pending") until then ⚠.
- `AGENTS.md` (the gate table row for a kernel WW3 calls), `CONTRIBUTING.md`, the `ww3-port`
  skill and `/port-next` name this cadence once the owner accepts it.

## Revisit when

- The H100 node's matrix has run once: record here its wall time, its machine, and how many
  entries failed, per unit.
- A matrix failure takes more than one working day to locate.
- The matrix on the i9 drops under one hour for the translated units (a build shared between
  entries with the same switch, ADR-0011's tooling item 3).
