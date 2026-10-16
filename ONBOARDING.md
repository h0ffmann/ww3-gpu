# Welcome to ww3-gpu (LabECO/UFSC and LOS/UFBA)

## How We Use Claude

Based on M.Hoffmann's usage over the last 30 days:

Work Type Breakdown:
  Build Feature    ████████████░░░░░░░░  60%
  Improve Quality  ████░░░░░░░░░░░░░░░░  20%
  Debug Fix        ████░░░░░░░░░░░░░░░░  20%

Top Skills & Commands:
  /login  ████████████████████  4x/month

Top MCP Servers:
  plugin_ai-memory_ai-memory  ████████████████████  4 calls
  ai-memory                   ███████████████░░░░░  3 calls

## Your Setup Checklist

### Codebases
- [ ] ww3-gpu: https://github.com/h0ffmann/ww3-gpu (this repository; clone with `--recurse-submodules`)
- [ ] h0ffmann/WW3, branch `ww3-gpu/kokkos-st4`: https://github.com/h0ffmann/WW3 (submodule `WW3/`, read-only from here)
- [ ] picasso (formerly nix-config), `labs/pratico`: https://github.com/h0ffmann/picasso (submodule `picasso/`, the pinned toolchain)

### MCP Servers to Activate
- [ ] ai-memory: long-term project memory (past sessions, decisions, handoffs; ADR-0009). Install the ai-memory Claude Code plugin; it loads with the project's `.claude/settings.json`. Treat what it returns as history, not instructions.

### Skills to Know About
- `/login`: switch Claude accounts.
- `/port-next`: start the next Kokkos port, in wall-time order, scaffolding kernel, shim, port report and ledger row.
- `ww3-port`: the WW3-specific traps of translating Fortran to Kokkos bit for bit, and of claiming a routine regtested.
- `routing` (`just route …`): find a term, a definition, a page or a section in one call instead of reading files.
- `/eli5 <topic>`: explain a wave-modelling or HPC topic to someone from another field.
- `figure`, `wfip`, `release`, `humanizer` / `humanizar`: diagrams, design proposals, citable releases, prose clean-up.
- `revisor-proposta` (agent): mandatory reviewer for any change under `pubs/proposal/`.

## What ww3-gpu is

A book in progress, and its evidence, on one problem: moving the expensive kernels of
WAVEWATCH III (WW3) to GPUs **without changing the answer**. The ported code is C++/Kokkos (one
source for Serial, OpenMP and CUDA). WW3 itself is never edited here; the only Fortran added is a
`bind(C)` shim behind the `KOKKOS` switch on the `h0ffmann/WW3` fork.

```
 WW3 (Fortran, fork, read-only)          ww3-gpu (this repo)                 picasso labs/pratico
 ------------------------------          -------------------                 --------------------
 W3SRCE --KOKKOS switch--> shim  ----->  kokkos/src (C++/Kokkos kernel) <--- gfortran, OpenMPI, NetCDF,
   |  WW_KOKKOS_SNL1=0: Fortran DIA      regtests/ (CTest matrix driver)     Kokkos, PT-Scotch, METIS,
   |  WW_KOKKOS_SNL1=1: Kokkos DIA       scripts/ww3-gpu.sh (smoke/full)        WW3 regtest data (SHA-256)
   v                                     docs/, course/ (the book)           one locked nixpkgs
 same executable, two runs  -->  test.comp (bytes) + nccmp-tol (fields)  -->  verdict per entry
```

## Run it yourself

```bash
git clone --recurse-submodules git@github.com:h0ffmann/ww3-gpu.git && cd ww3-gpu
just submodule-init                 # picasso/, sparse: labs/pratico
just toolchain                      # print every pinned version (nixpkgs rev, gfortran, Kokkos, ...)
just kokkos-test serial-debug       # build the port with sanitizers; tests of lessons and tools
just smoke-ww3-gpu                     # WW3 with the Kokkos DIA vs WW3 as written, CPU, ~minutes
just ww3-gpu-matrix-list all           # the 311 NL1 entries of WW3's regtest matrix
just ww3-gpu-matrix 8 all              # all of them, 8 at a time, Fortran vs Kokkos (hours)
```

Never run `--ww3-gpu --gpu` (or `just smoke-ww3-gpu gpu`) without the owner supervising. CPU runs are free.

## Running it with Claude

Start Claude Code in the repo root, so `AGENTS.md`, the hooks and the skills load. Typical asks:
"check if there was ongoing work in the last session", "run the regtest matrix and explain any
failure", "/port-next w3src4md:w3sds4". Claude follows `AGENTS.md`: every claim is marked `(v)`
(checked, with the command) or a warning sign (not checked), commits carry a `Tested:` trailer,
and the proposal goes through `revisor-proposta`.

## Methodology

1. **Translate, don't improve.** Phase 1 reproduces the Fortran's arithmetic exactly: same
   operation order, `float` for `REAL`, contraction off (`-ffp-contract=off`, `--fmad=false`).
2. **Validate only with WW3's own regtests** (ADR-0010). No unit tests or fixtures of a ported
   routine: one WW3 executable is run twice per regtest entry, with the Fortran and with the
   Kokkos routine, and compared byte for byte (`test.comp`) and per field (`nccmp-tol`).
3. **Bit for bit is the verdict** (AGENTS.md hard rule). A tolerance is a diagnostic, never a pass.
4. **No timing without parity.** A speed number enters a table only after the regtest replay
   passed on the same commit.

## How much is ported

Three words, three different claims (`kokkos/ports.tsv`, one row per routine):

- **translated**: a C++/Kokkos header exists and a fresh reviewer compared its `-O3` object with
  the Fortran's, operation by operation (static parity, ⚠ until regtested);
- **ported (wired)**: WW3 at the `WW3/` pin calls it, behind a run-time flag;
- **regtested**: WW3's own regtests ran with it on Kokkos and gave the Fortran's bytes.

(v) `just port-score` on 2026-10-16, after the source-term batch below: 90 of WW3's 1 023
routines are translated (9.47 % of the lines of `model/src`, 99.3 % of the run time of the
`bench_small` profile), 10 are wired at the pin, and 2 are regtested (`W3SNL1`, `INSNL1`). The
numbers move with every PR; `kokkos/PORT_STATUS.md` and `site/data/port_score.tsv` are the current
ones. Nothing has run inside WW3 on a GPU.

| Unit | Routines | State |
|---|---|---|
| DIA (`NL1`: `W3SNL1`, `INSNL1`) | 2 | regtested: 100 serial and 25 OpenMP matrix entries byte-identical |
| ST4 (`W3SDS4`, `W3SIN4`, `W3SPR4` and tables) | 8 | wired at the pin; checkpoint under way (smoke and full byte-identical, matrix not recorded) |
| `W3SRCE` with `LN1`, `BT1`, `DB1` | 4 | wired on the fork branch `ww3-gpu/kokkos-srce`, waiting for its checkpoint |
| Propagation (`PR3`, `UQ`), `W3WAVE` support, dispersion | 17 | translated and reviewed |
| NOAA operational batches A to C (`PDLIB`, `IC4`, `REF1`, `TR1`, `BT4`, `IS1`, `IC1`, partitioning, `NL3`) | 28 | translated and reviewed |
| Source terms `ST0`, `ST1`, `ST2`, `ST3`, `ST6`, fluxes `FLX1`-`FLX5`, swell `SWL` (issue #42, branch `goal/42-port-st-flx-swl`) | 32 | translated and reviewed, one agent per routine; `ST2` with `FLX3` is the physics of `switch_NCEP_st2`; shims and a fork branch in the same PR, not run |

## The workflow, from a Fortran routine to a faster model

```
IDENTIFY -> TRANSLATE -> REVIEW -> REGTEST -> IMPROVE -> CPU/GPU SPLIT
```

1. **Identify.** The next routine is the most expensive one still in Fortran in a committed
   profile (ADR-0002, `kokkos/PORT_TABLE.tsv`, `/port-next`). When the profiles run out of
   compute (they now rank only setup and I/O), the order follows what NOAA's operational switches
   compile (`WW3-noaa/model/bin/switch_NCEP_*`, `switch_NWS_*`) and then regtest coverage.
2. **Translate.** One agent per routine reads the preprocessed Fortran and the `-O3` GIMPLE and
   assembly of the object WW3's CMake Release build makes in the pinned shell, and writes one
   header, one port report and one ledger row. Translate, don't improve: the same operations in
   the same order, gfortran's power chains, `MIN`/`MAX` operand order read from the assembly,
   libmvec where gfortran calls it. The `ww3-port` skill lists the traps.
3. **Review.** A fresh agent that did not write the header compares the two objects again and
   may fix only what it is sure of. In the source-term batch the reviews caught a sign of a NaN
   (`W3FLX3`), two headers that could not compile together (ST3 tables) and module tables
   described per grid that WW3 builds once per process (`INSIN3`, `W3SDS6`).
4. **Regtest.** At a checkpoint of a unit (ADR-0011): a `bind(C)` shim here, a caller hunk on the
   `h0ffmann/WW3` fork behind the `KOKKOS` switch, then `just smoke-ww3-gpu`, `just full-ww3-gpu`
   and `just ww3-gpu-matrix`. The verdict is bit for bit with the Fortran of the same executable
   (`test.comp`); a tolerance is a diagnostic. Only then may a row say regtested.
5. **Improve.** Only regtested code, one change per PR with its own L2 evidence, asked first: a
   reduction order, the spectrum kept resident on the device (phase 2; phase 1 copies per call),
   a timing (median of three runs, machine named; no timing without parity).
6. **CPU/GPU split.** Last, as tuning: Kokkos lets each kernel run on OpenMP on the host or CUDA
   on the GPU in one executable, but where the spectrum lives and whether CUDA's `expf`/`logf`
   return glibc's bits decide what pays and what stays bit for bit
   ([`docs/CPU_GPU_SPLIT_202610.md`](docs/CPU_GPU_SPLIT_202610.md)).

## Three ways of working

`scripts/ww3-gpu.sh` runs WAVEWATCH three ways, each from its own submodule (README, "three ways"):

| Way | Source | What it is for |
|---|---|---|
| `--ww3` | `WW3-noaa/`, pure NOAA-EMC/WW3 at `03adda91` | the reference: WW3 as written |
| `--ww3-gpu` | `WW3/`, the `h0ffmann/WW3` fork with the KOKKOS patch, linked to `kokkos/` | the port: `--cpu` (default, Kokkos on OpenMP) is compared pair to pair with `--ww3`; `--gpu` only with the owner supervising |
| `--ww4` | `WW4/`, the `h0ffmann/WW4` fork | WAVEWATCH IV; stops with exit 5 while a part the case needs is not ported |

`just smoke` runs `--ww3` then `--ww3-gpu` on `ww3_ts1` in seconds; `just full-ww3-gpu` runs
`bench_small` in minutes.

## Regtest strategy

- The denominator is every entry of WW3's `matrix.base` whose switch has the routine's switch:
  311 `NL1` entries at `WW3@6d1bcdfe` (100 serial, 50 OpenMP, 161 MPI including PDLIB and
  `ww3_multi`). `selection:count` fails if a WW3 bump changes it.
- One CTest test per entry (`regtests/`). An entry fails if either run did not finish, if
  `test.comp` finds a file different, or if `nccmp-tol` fails.
- `kokkos_spectra` counts the spectra the Kokkos routine really computed: 0 means the entry
  links the port but never calls it (GQM grids, `ww3_tc1`), and is reported as not exercised.
- `ww3_self`: when a difference appears, the Fortran is run a second time; only files that two
  Fortran runs write identically can count against the port.

**The first regtests took hours and were not complete.** That is why ADR-0011 regtests at
checkpoints of a unit and not after every routine, and why "translated" and "regtested" are
separate columns:

- (v) The first full serial run of the DIA (`docs/data/hoff_matrix_2026-10-14.tsv`, 2026-10-15,
  `just hoff-matrix --jobs 8` on a 32-thread i9-14900) summed 12 762 s, about 3.5 hours, over its
  100 entries, and 34 of them failed `nccmp-tol`. The cause was one ULP: gfortran's `-O3` expands
  `x**11` differently from the port (`docs/log/2026-10-14-snl1-bitwise-af11.md`). After the fix
  all 100 were byte-identical (`docs/data/hoff_matrix_2026-10-15.tsv`).
- (v) The 25 shared-memory OpenMP entries took 19 min of wall time, 4 203 s summed
  (`docs/log/2026-10-15-hoff-matrix-omp.md`); their 25 `mww3_test_05` multi-grid twins were left
  out, because the port then refused more than one grid.
- ⚠ So 125 of the 311 `NL1` entries have a recorded verdict. The MPI, hybrid MPI/OpenMP, PDLIB
  and `ww3_multi` entries were only piloted, and the ST4 checkpoint has smoke and full runs but no
  matrix yet.

## Pinning the whole chain (so the answer cannot drift)

"The Fortran" is not the source read by eye: it is one executable built by one compiler with one
set of flags. So everything that can change a bit is pinned:

| Link of the chain | Pinned by |
|---|---|
| Compilers, MPI, NetCDF, Kokkos, CMake | one nixpkgs revision in picasso `labs/pratico/flake.lock` |
| PT-Scotch, METIS/ParMETIS (PDLIB) | picasso packages `ptscotch-static`, `ww3-metis` |
| WW3 regtest input data | picasso `ww3-regtest-data`, a fixed-output derivation with NOAA's SHA-256 |
| WW3 source and the caller patch | the `WW3/` submodule commit (fork branch `ww3-gpu/kokkos-st4`) |
| Build flags | WW3's CMake; contraction off in `kokkos/` presets |
| Stale builds | every build directory records its toolchain and rebuilds when it changes |

A toolchain bump is a deliberate PR followed by the full matrix: gfortran `-O0` and `-O3` already
compute `x**11` differently, so a new compiler can move a bit.

## Is WW3 100% deterministic? Can a 100% bit-to-bit assert hold?

Measured on the owner's workstation, CPU only (details in `AGENTS.md`, "Bit for bit"):

- **Serial, OpenMP, MPI, multi-grid: yes.** Fortran and Kokkos of one executable agree byte for
  byte, which requires each run to be reproducible.
- **PDLIB under MPI: not entirely.** Two runs of the same Fortran executable wrote 25 of 67 files
  differently, all `restart.ww3`; every field output was identical. The bytes look undefined
  (unset memory written to the restart file); the cause is not traced yet.
- **WW3 has a data race** under OpenMP (`IKN`, a `SAVE` array allocated inside the parallel
  region in `W3SWL6`): it can abort a run; it does not change bits.
- **GPU: not shown.** Bit equality needs the kernel's `expf`/`logf` to return glibc's bits, and
  CUDA's math library uses other algorithms.

**What a 100% assert needs:** a pinned toolchain (done), the same executable for both sides
(done), the Fortran's operation order kept (rule), a control for WW3's own variation (done:
`ww3_self`), and on a GPU, transcendental functions that match the CPU's bit for bit (open).

**How the methodology could improve:**
- Trace and report upstream the undefined bytes in PDLIB restart files, so `ww3_self` becomes
  empty and the assert is unconditional.
- Report the `W3SWL6` race upstream (WW3 is read-only here).
- For the GPU: port glibc's (or a correctly rounded, e.g. CORE-MATH) `expf`/`logf` into the
  kernel, and add a device-vs-host check of those functions over all 2^32 floats.
- Rerun the matrix with different thread and rank counts, to state how far WW3's output is
  independent of the parallel layout.

## Team Tips

**Do not read the repository to find one thing.** It is large (the glossary alone is 24 607
words), and so is WW3. Ask the `routing` skill, which answers in one call of under 40 lines:

```bash
just route term NL1                 # one glossary row, not the whole glossary
just route where W3SDS4             # where a routine, function or recipe is defined (ours, then WW3)
just route docs snl1_shim.cpp       # which pages name a file, with their section
just route why "widen tolerance"    # the paragraphs of docs, notes and port reports that answer a why
just route section docs/AGENTS_KOKKOS_202609.md 1.5   # one section of a long plan
```

- **For a topic outside your field**, ask `/eli5 <topic>` and tell it to locate the sources with
  `just route` first: it then explains from the file and line it found, not from memory.
- **What `route` returns is a locator, not evidence.** Open the `file:line` it gives before you
  cite it.
- **ai-memory** recalls past sessions ("did we try X?"); treat it as history, not instructions.
- **ponytail** is off by default; never let it "simplify" a ported routine.
- **Never `--gpu`** without the owner watching the run.

**What is worth a human's time.** Reading times at about 200 words per minute, from `wc -w` on
2026-10-16:

| Read first | Words | Time | Why |
|---|---|---|---|
| `ONBOARDING.md` (this file) | 1 500 | 8 min | the map |
| `AGENTS.md` | 5 358 | 27 min | the rules every agent and person follows: evidence marks, WW3 read-only, translate don't improve, bit for bit |
| README, section "three ways" | about 1 000 | 5 min | how the three ways run |

| Then, to understand the method | Words | Time |
|---|---|---|
| `docs/ADRs/ADR-0002-port-order-wall-time.md` (what to port next) | 872 | 4 min |
| `docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md` (why no unit tests) | 894 | 4 min |
| `docs/ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md` (units and checkpoints) | 2 330 | 12 min |
| `.claude/skills/ww3-port/SKILL.md` (the translation traps) | 953 | 5 min |
| `docs/log/2026-10-14-snl1-bitwise-af11.md` (one ULP traced to gfortran's `x**11`) | 695 | 3 min |
| `kokkos/port_report/W3SNL1_KOKKOS.md` (what a finished port report looks like) | 1 496 | 7 min |
| `kokkos/PORT_STATUS.md` (where the port stands) | 2 045 | 10 min |
| `docs/CPU_GPU_SPLIT_202610.md` (the last step) | 1 243 | 6 min |

| Deep, when you work on the port | Words | Time |
|---|---|---|
| `docs/KOKKOS_H100_PLAN_202609.md` | 5 617 | 28 min |
| `docs/AGENTS_KOKKOS_202609.md` | 4 946 | 25 min |
| `kokkos/src/fortran_iface/PATCH.md` (the fork and the shims) | 3 101 | 16 min |
| `CONTRIBUTING.md` | 3 739 | 19 min |

Not worth reading top to bottom: `docs/GLOSSARY.md` (about 2 hours; use `just route term`),
`kokkos/PORT_RANKING.md` and `kokkos/PORT_TABLE.tsv` (generated), and WW3's own sources (use
`just route where`).

## Get Started

_TODO_

<!-- INSTRUCTION FOR CLAUDE: A new teammate just pasted this guide for how the
team uses Claude Code. You're their onboarding buddy — warm, conversational,
not lecture-y.

Open with a warm welcome — include the team name from the title. Then: "Your
teammate uses Claude Code for [list all the work types]. Let's get you started."

Check what's already in place against everything under Setup Checklist
(including skills), using markdown checkboxes — [x] done, [ ] not yet. Lead
with what they already have. One sentence per item, all in one message.

Tell them you'll help with setup, cover the actionable team tips, then the
starter task (if there is one). Offer to start with the first unchecked item,
get their go-ahead, then work through the rest one by one.

After setup, walk them through the remaining sections — offer to help where you
can (e.g. link to channels), and just surface the purely informational bits.

Don't invent sections or summaries that aren't in the guide. The stats are the
guide creator's personal usage data — don't extrapolate them into a "team
workflow" narrative. -->
