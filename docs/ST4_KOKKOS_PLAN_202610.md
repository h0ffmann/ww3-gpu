# ST4 on Kokkos: an 8-hour port plan (phase 1, translation only)

Status: **Tasks 0-10 done**, 2026-10-15, under [ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md) ([log](log/2026-10-15-st4-kokkos-translation.md)); the checkpoint of §12 has not run. It answers the owner's
request of 2026-10-15: port all of ST4 to Kokkos in one disciplined session, with subagents, and
without the regtests. Issue [#45](https://github.com/h0ffmann/ww3-gpu/issues/45) is the issue it
answers; [ADR-0002](ADRs/ADR-0002-port-order-wall-time.md) sets its order,
[ADR-0010](ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md) sets what "tested" means, and
[ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md) makes ST4 one unit with its
regtests at a checkpoint after this plan.

> **For agentic workers:** execute this plan task by task. Run Tasks 1, 3, 4, 5, 6 and 8 in
> subagents as section 6 says, and the other tasks in the main session. Steps use checkbox (`- [ ]`) syntax for tracking. Read `AGENTS.md`, the
> `ww3-port` skill and `.claude/commands/port-next.md` before Task 0.

**Goal.** Translate the ST4 source-term package of WAVEWATCH III (`model/src/w3src4md.F90`) to
C++/Kokkos, phase 1 (copy in, kernel, copy out), wire it into `W3SRCE` behind the `KOKKOS`
switch and one run-time flag per routine, and make it compile on Serial, OpenMP and CUDA.

**Architecture.** Each routine becomes one per-point device function
(`KOKKOS_INLINE_FUNCTION`), so the later `W3SRCE` team kernel can call it
([`KOKKOS_H100_PLAN_202609.md`](KOKKOS_H100_PLAN_202609.md):240). A thin phase-1 launcher and a
`bind(C)` shim call that function once per point, in the same way as `W3SNL1`. The ST4 tables come
from the Fortran's `W3GDATMD` at init, so they have the same bits by construction.

**Tech stack.** C++20, Kokkos (Serial, OpenMP, CUDA presets in `kokkos/CMakePresets.json`),
gfortran and WW3's own CMake build from the pinned `picasso/labs/pratico` shell.

**Spec.** This file, issue #45, [`AGENTS_KOKKOS_202609.md`](AGENTS_KOKKOS_202609.md) §1 and §3,
[`kokkos/src/fortran_iface/PATCH.md`](../kokkos/src/fortran_iface/PATCH.md) and the template port
[`kokkos/port_report/W3SNL1_KOKKOS.md`](../kokkos/port_report/W3SNL1_KOKKOS.md).

## 1. Scope

In scope, in the order of the committed profile (`kokkos/PORT_RANKING.md`, gprof on
`bench_small`, `WW3@6d1bcdfe`, 2026-10-14) (v):

| Rank | Routine | Lines at `WW3@bb08623a` | Self time | Share |
|---|---|---|---|---|
| 1 | `W3SDS4` | `w3src4md.F90:2043-2659` (617) | 30.52 s | 23.23 % |
| 2 | `W3SIN4` | `w3src4md.F90:423-1003` (581) | 16.63 s | 12.66 % |
| 4 | `W3SPR4` | `w3src4md.F90:142-384` (243) | 4.10 s | 3.12 % |
| (below 10 ms) | `CALC_USTAR` | `w3src4md.F90:1849-2011` (163) | 0 | 0 |
| (below 10 ms) | `INSIN4`, `TABU_STRESS`, `TABU_TAUHF`, `TABU_TAUHF2` | `w3src4md.F90:1014-1831` (778) | 0 | 0 |

Rank 3 is `W3SRCE` (7.36 s, 5.60 %). It is not part of ST4, and it is the unit of a later phase.
ST4 is 39.0 % of the profiled run time; with `W3SNL1` (48.4 %, ported), the ported share would be
87.4 % ⚠ (sum of the gprof rows, not a measurement of the ported code).

Out of scope:

- **Regtests** (the owner, 2026-10-15). No `just smoke`, `just smoke-ww3-gpu`,
  `just full-ww3-gpu` or `just ww3-gpu-matrix` run is part of this plan. A later plan does them
  (section 12).
- **Timings.** "No timing without parity": without a passed regtest replay, no number enters a
  table or a port report.
- **GPU runs.** CUDA is compiled, never run (`--build-only` only). `--ww3-gpu --gpu` needs the
  owner at the terminal.
- **The `WW3/` pin.** The fork branch is pushed, but this plan does not move the submodule pin.
  A pin bump needs `just smoke` (`AGENTS.md`, gate table), which is a regtest.
- **The `W3SRCE` team kernel**, device residency (phase 2) and any optimisation.
- **`W3_FLX5`**: `W3SPR4` then calls `W3FLX5` in another module. The Kokkos path refuses this
  switch at init (Task 2). Debug output under `W3_T`, `W3_T0`, `W3_T1` and `W3_S` is not
  ported: a kernel never prints.

What this scope means under the hard rules:

1. No ST4 routine moves past status `ported` in `kokkos/ports.tsv`. None is L2. None is done
   (§1.5 needs the L2 replay and the matrix).
2. Each new `ports.tsv` row has `regtested` = `no: waiting for the checkpoint of ST4
   (ADR-0011)`.
3. Each run-time flag defaults to `0`, so the Fortran runs unless a person turns a flag on. An
   unvalidated port must never be the default.
4. "Bit for bit with the Fortran" is the design target of every step. This plan argues it
   statically, by the GIMPLE comparison of ADR-0011 (Task 6, Step 2). Every claim of parity in
   the port reports stays ⚠ until the checkpoint runs.

## 2. Global constraints

Every task includes these. Each line restates a hard rule from `AGENTS.md` or
`AGENTS_KOKKOS_202609.md`.

- Kokkos code is written only under `organizationType` = `claude_team` (`~/.claude.json`).
- `WW3/` and `$WW3` are read-only. Fortran changes go to the `h0ffmann/WW3` fork, on a new branch,
  behind `W3_KOKKOS`, and leave the Fortran routine selectable.
- Translate, do not improve. Keep every loop order, every reduction order, every quirk listed in
  section 4.6, and every redundant computation (for example the loop-invariant `COS`/`SIN` at
  `w3src4md.F90:2567`).
- Ask the owner before you change a reduction order, a limiter or an integration order. Stop the
  task and write the question; do not work around it.
- Contraction stays off: `-ffp-contract=off` on the host, `--fmad=false` on the device.
- `float` is WW3's `REAL`. `double` only where WW3 uses `REAL(8)`.
- No allocation in a kernel. No host memory, `std::` container or `std::function` in a kernel.
- Every View names its memory space. No Unified Memory.
- No L1 test, GoogleTest or fixture of a ported routine (ADR-0010).
- Never widen a tolerance. Never mark an entry as expected to fail.
- No `--ww3-gpu --gpu` run. No piped "yes", no faked terminal, no `WW3GPU_IN_SHELL`.
- Commits carry `Tested:` in the final block and never a `Cost:` trailer.
- Prose in this repository is plain English. No em dash in prose. Every new abbreviation goes into
  `docs/GLOSSARY.md` with its first use.
- Use the current command names: `--ww3-gpu`, `scripts/ww3-gpu.sh`, `just smoke-ww3-gpu`,
  `just full-ww3-gpu`, `just ww3-gpu-matrix`. Older logs say `hoff`; do not copy those names.

## 3. The schedule

Clock times count from the start of the session. Tasks 3, 4 and 5 run in parallel, each in its
own subagent. They touch different files. They depend only on Task 2.

| Time | Task | Who | Output |
|---|---|---|---|
| 0:00-0:30 | 0. Preflight | main session | branch, fork clone, go/no-go |
| 0:30-1:00 | 1. Reference arithmetic | subagent A (read-only) | `-O3` dump inventory |
| 1:00-1:45 | 2. Common layer | main session | parameters, tables, C ABI, CMake |
| 1:45-3:45 | 3. `W3SDS4` | subagent B | `st4_sds4.*` |
| 1:45-3:30 | 4. `W3SIN4` | subagent C | `st4_sin4.*` |
| 1:45-2:45 | 5. `W3SPR4` and `CALC_USTAR` | subagent D | `st4_spr4.*` |
| 3:45-4:45 | 6. Translation review | subagents E, F, G (read-only) | one review per routine |
| 4:45-6:00 | 7. Fortran wiring in the fork | main session | fork branch `ww3-gpu/kokkos-st4` |
| 6:00-6:45 | 8. `INSIN4` and the `TABU_*` tables | subagent H | `st4_tables_build.*`, time-boxed |
| 6:45-7:20 | 9. Build gates | main session | the four builds of section 7, Task 9 |
| 7:20-8:00 | 10. Ledger, reports, log, PR | main session | PR against `main` |

Buffer: Task 8 is the only task you can drop. If the session is late at 6:00, skip Task 8, keep
the tables from the Fortran, and write the reason in the log entry.

## 4. Design

### 4.1 One device function per routine

Each routine is one function that takes one point and plain pointers into Views:

```cpp
// kokkos/src/ww_kokkos/st4_sds4.hpp
namespace ww::st4 {
struct Sds4Work;   // preallocated per-point work arrays, sized at init (no allocation in a kernel)
KOKKOS_INLINE_FUNCTION
void w3sds4_point(const Params& p, const TablesView& t, Sds4Work& w,
                  const Real* a, const Real* k, const Real* cg,
                  Real depth, Real dair, Real ustar, Real usdir, Real dlwmean,
                  Real* srhs, Real* ddiag, Real* brlambda, Real* whitecap);
}
```

The phase-1 launcher runs it in a `Kokkos::parallel_for` with a range of one point (the shim's
`npts` argument stays, as in `ww_snl1`). The work arrays live in the per-grid context, which the
shim allocates at init. `W3SIN4`'s `ALLOCATE(W(JTOT))` at `w3src4md.F90:863` becomes a fixed
`Real w[50]`: `JTOT` = 50 is a literal there (v). That changes the storage, not the arithmetic.

### 4.2 Parameters and tables

- `Params` holds the scalars of `SRCP` that ST4 reads (section 4.5 of the research, the
  `SIN4` and `SDS4` namelists): `ZZWND`, `AALPHA`, `BBETA`, `SSINTHP`, `ZZ0MAX`, `ZZALP`,
  `ZZ0RAT`, `TTAUWSHELTER`, `SSWELLF(1:7)`, `SINTAILPAR(1:5)`, `CAPCHNK(1:5)`, `SSDSC(1:21)`,
  `SSDSBR`, `SSDSBT`, `SSDSP`, `SSDSDTH`, `SSDSCOS`, `SSDSBM(0:4)`, `SSDSBRF1`, `SSDSBCK`,
  `SSDSABK`, `SSDSPBK`, `SSDSBINT`, `SSDSHCK`, `WWNMEANP`, `WWNMEANPTAIL`, `SSTXFTF`,
  `SSTXFTFTAIL`, `SSTXFTWN`, `AAIRCMIN`, `AAIRGB`, and the grid arrays `SIG`, `DSIP`, `DDEN`,
  `DDEN2`, `SIG2`, `ESIN`, `ECOS`, `TH`, `DTH`, `XFR`, `FTE`, `FTF`.
- `SRCP` is a derived type. §3.2 forbids derived types across the boundary, so the Fortran side
  packs the scalars into `REAL(C_FLOAT) RPAR(NRPAR)` and `INTEGER(C_INT) IPAR(NIPAR)`. The index of
  every entry is a named constant in both `st4_params.hpp` and `w3kokkosmd.F90`, and the
  argument table in the port report lists them.
- The tables (`TAUHFT`, `TAUHFT2`, `TAUT`, `DELU`, `DELTAUW`, `DELUST`, `DELALP`, `DELTAIL`,
  `SATINDICES`, `SATWEIGHTS`, `IKTAB`, `DCKI`, `QBI`, `CUMULW`, `DIKCUMUL`, `FWTABLE`, `DELAB`,
  `ABMIN`) are copied once from the Fortran after `W3IOGR` reads `mod_def.ww3`
  (`w3iogrmd.F90:1590-1612`). Size: `DCKI` and `QBI` are 2000 x 1300 reals each (10.4 MB each),
  `CUMULW` is `NSPEC`^2 (v, `w3gdatmd.F90:637-638`).
- `TAUHFT2` exists only when `TTAUWSHELTER > 0`; `TAUT` and `TAUHFT` only when
  `SINTAILPAR(1) > 0.5`. The init copies a table only under the Fortran's own condition
  (`ww3-port` skill, §2).

### 4.3 Run-time selection

| Variable | Default | Selects |
|---|---|---|
| `WW_KOKKOS_SDS4` | 0 | `W3SDS4` on Kokkos |
| `WW_KOKKOS_SIN4` | 0 | `W3SIN4` on Kokkos |
| `WW_KOKKOS_SPR4` | 0 | `W3SPR4` and `CALC_USTAR` on Kokkos |
| `WW_KOKKOS_ST4_COUNT` | unset | print the spectra each routine computed, at finalize |
| `WW_KOKKOS_ST4_TABLES` | 0 | build the tables in C++ (Task 8) instead of copying them |

Each flag is read once, in `ww_kokkos_init`, as `WW_KOKKOS_SNL1` is (`snl1_shim.cpp:204`). One
flag per routine lets the ST4 checkpoint test one routine at a time.

### 4.4 The argument contract

- **Copy in every `OUT` array, not only the `IN` arrays.** `W3SDS4` returns early
  (`w3src4md.F90:2581`) and leaves `WHITECAP` unassigned unless `FLOGRD(5,7)` or
  `FLOGRD(5,8)` is set, and it never sets `WHITECAP(3)`. The caller's values must come back
  unchanged. A copy out of uninitialised device memory would change the bits.
- `LLWS` is a default `LOGICAL`. The caller hunk converts it to `INTEGER(C_INT)` with `MERGE`
  before and after the call. No `LOGICAL` crosses the boundary.
- `SIG` is a `POINTER` with bounds `0:NK+1`: pass `SIG(1:NK)`. `CG1` and `WN1` are `(NK)`;
  `WN2` is `(NSPEC)` (v, the calls at `w3srcemd.F90:1250` and `:1333`).
- `IX`, `IY` and `BRLAMBDA` (in `W3SIN4`) are unused by the routine (v). The shim does not take
  them. The argument table says so.

### 4.5 Arithmetic

The reference is the object code that WW3's CMake build produces (`-O3`,
`model/src/CMakeLists.txt:87-89`, the GNU block), not the source read by eye (`docs/log/2026-10-14-snl1-bitwise-af11.md`).

| Fortran (`REAL`) | C++ (`float`) | Note |
|---|---|---|
| `EXP`, `LOG`, `ALOG`, `ALOG10`, `SQRT`, `TANH`, `COS`, `SIN`, `ATAN2` | `std::exp`, `std::log`, `std::log10`, `std::sqrt`, `std::tanh`, `std::cos`, `std::sin`, `std::atan2` on `float` | glibc `expf`, `logf` and the others on the host. On CUDA they are not glibc ⚠ (section 9). |
| `x**r`, `r` a `REAL` variable | `std::pow(float, float)` | `COSWIND**SSINTHP` at `:756` is a `powf` call, not a square ⚠ until the dump confirms. |
| `x**n`, `n` a literal integer | the multiplication tree in the `-O3` dump | `x**11` was `x5*x6` at `-O3` and `__powisf2` at `-O0`. |
| `x**n`, `n` an integer variable | `powi()` from `snl1_tables.cpp:34` | never `std::pow` |
| `SUM`, `DOT_PRODUCT`, `MAXVAL` | a plain left-to-right loop | ⚠: Task 1 confirms in the dump that gfortran keeps the order. |
| `PI**2.` | the constant the dump shows | gfortran may fold it at compile time. |

Integer powers of `KAPPA`, `GRAV` and similar constants may also be folded. Task 1 lists every
one.

### 4.6 Quirks the port keeps

From the map of `w3src4md.F90` at `WW3@bb08623a` (v, read 2026-10-15). Each item is a behaviour of
the Fortran. The port reproduces it. Each item gets one comment line in the kernel that cites
the line.

1. `W3SIN4`: the tail loop at `:852` uses `COSU` and `SINU` from the last `IK = NK` pass
   (`:719-720`), not the `USDIRP` recomputed at `:838`.
2. `W3SIN4`: the `LLWS` override at `:769` uses `USTAR`, not `USTP`.
3. `W3SIN4`: the stress loop over `IK` (`:713-797`) is a sequential scan. Each `IK` depends on the
   stress accumulated over the earlier ones. Only the `ITH` work inside one `IK` could run in
   parallel, and phase 1 does not do that.
4. `W3SIN4`: the analytic tail (`:885-930`) is a recurrence over `J = 1..50`.
5. `W3SDS4`: `DIKCUMUL` is a module variable that the routine writes on every call (`:2218`).
   The kernel computes it as a local with the same expression. It does not write it back.
6. `W3SDS4`: `CASE(2)` at `:2473` computes `-PB2*A` through `(1-SSDSC(1))*PB2*A+SSDSC(1)*PB`.
   Keep the expression as written.
7. `W3SDS4`: `CASE(3)` at `:2485` divides by `USTAR**2`. At `USTAR = 0` this gives the same
   infinity in C++ (no `-ffast-math`). Keep it.
8. `W3SDS4`: the `DOT_PRODUCT` at `:2283` has length `2*(MTH/2-1)+1` for every `SDSNTH`. The
   extra terms have weight 0 and index 1. Keep the full length: `0*x` is not zero when `x` is
   infinite, and the length sets the order.
9. `W3SDS4`: the default cumulative branch (`SDSCUM = -0.40344`, `w3gridmd.F90:2157`, so
   `SSDSC(3) < 0`) is the O(NK^2 NTH^2) loop at `:2554-2563`. Keep it. The choice of branch is
   physics, and only LabECO can change it (issue #45, task T2).
10. `W3SDS4`: `WHITECAP(1)` is a recurrence (`:2615`). Keep the `IK` order.
11. `W3SPR4`: `ELCS` and `ELSN` accumulate across `IK`, then `ITH`, into one scalar (`:300-301`).

### 4.7 Switches and threads

- Physics switches are compile-time constants in C++ (§1.1). `W3_STAB3` and `W3_IG1` are passed
  in `IPAR` and are supported. `W3_FLX5` makes `ww_st4_init` return an error code, and the
  Fortran stops with `EXTCDE`. The failure is loud, not a silent fallback.
- `W3SRCE` runs inside `!$OMP PARALLEL` under `W3_OMPG` and `W3_OMP0`
  (`w3wavemd.F90:2240-2243`, `:1551`). Each ST4 call goes inside
  `!$OMP CRITICAL (WW_KOKKOS_ST4)`, and the shim uses the Serial fallback when
  `omp_get_level() > 0` (`snl1_shim.cpp:146-151`). This makes the calls serial. That is the
  accepted cost of phase 1.
- The `IENT` counter under `W3_S` is not ported.

## 5. Files

In this repository:

| Path | Status | Responsibility |
|---|---|---|
| `kokkos/src/ww_kokkos/st4_params.hpp` | create | `Params`, the `RPAR`/`IPAR` index constants |
| `kokkos/src/ww_kokkos/st4_tables.hpp`, `.cpp` | create | device Views of the tables; copy from the Fortran arrays |
| `kokkos/src/ww_kokkos/st4_sds4.hpp`, `.cpp` | create | `w3sds4_point`, its work arrays, the phase-1 launcher |
| `kokkos/src/ww_kokkos/st4_sin4.hpp`, `.cpp` | create | `w3sin4_point`, the phase-1 launcher |
| `kokkos/src/ww_kokkos/st4_spr4.hpp`, `.cpp` | create | `w3spr4_point`, `calc_ustar`, the phase-1 launcher |
| `kokkos/src/ww_kokkos/st4_tables_build.hpp`, `.cpp` | create (Task 8) | host translation of `INSIN4` and `TABU_*` |
| `kokkos/src/fortran_iface/st4_shim.cpp` | create | `ww_st4_init`, `ww_sds4`, `ww_sin4`, `ww_spr4`, the per-grid context |
| `kokkos/src/fortran_iface/ww_kokkos_c.hpp` | modify | the four C entry points and their error codes |
| `kokkos/src/ww_kokkos/CMakeLists.txt` | modify | the new sources |
| `kokkos/src/fortran_iface/CMakeLists.txt` | modify | `st4_shim.cpp`; the branch name in the cache string |
| `kokkos/CMakeLists.txt` | modify | `project(... VERSION 0.2.0)` |
| `kokkos/port_report/{W3SDS4,W3SIN4,W3SPR4,CALC_USTAR,INSIN4}_KOKKOS.md` | create | what was ported, argument table, "not regtested" |
| `kokkos/ports.tsv` | modify | five rows, status `ported` |
| `kokkos/PORT_STATUS.md`, `kokkos/PORT_RANKING.md`, `site/data/port_score.tsv` | regenerate | `scripts/port_score.py --write` |
| `docs/GLOSSARY.md` | modify | new names (`WW_KOKKOS_SDS4`, `RPAR`, and so on) |
| `docs/log/2026-10-15-st4-kokkos-translation.md` | create | what was done, what was skipped, what was not checked |

In the fork `h0ffmann/WW3`, branch `ww3-gpu/kokkos-st4` from `hoff/kokkos-snl1` at `bb08623a`,
in a separate clone (never inside `WW3/`):

| Path | Change |
|---|---|
| `model/src/w3kokkosmd.F90` | interfaces of `WW_ST4_INIT`, `WW_SDS4`, `WW_SIN4`, `WW_SPR4`; the `RPAR`/`IPAR` packing; flags `KOKKOS_SDS4`, `KOKKOS_SIN4`, `KOKKOS_SPR4` |
| `model/src/w3initmd.F90` | after `W3IOGR`, under `W3_KOKKOS` and `W3_ST4`, call `WW_ST4_INIT(IMOD, ...)` |
| `model/src/w3srcemd.F90` | the eight call sites: `W3SPR4` at `:1091`, `:1130`, `:1908`, `:2040`; `W3SIN4` at `:1111`, `:1250`, `:2037`; `W3SDS4` at `:1333` |
| `model/src/CMakeLists.txt`, `model/src/cmake/switches.json` | section 10.2 |

## 6. Subagents

| Agent | Type | Tasks | Writes |
|---|---|---|---|
| A | `Explore` (read-only) | 1 | nothing; returns the inventory |
| B, C, D | `general-purpose`, same `claude_team` session | 3, 4, 5 | only its own `st4_<routine>.*` files |
| E, F, G | fresh `general-purpose`, told to report only | 6 | nothing |
| H | `general-purpose` | 8 | only `st4_tables_build.*` |

Rules for every subagent prompt:

- Give the exact routine, its `file:line` at `WW3@bb08623a`, the interface of section 4.1, the
  quirks of section 4.6 for that routine, and Task 1's inventory (§1.6: "every task names the
  routine, the file, the commit, the interface and the phase").
- Tell it: do not edit `WW3/`, do not run a regtest, do not run on a GPU, do not write a test.
- Tell it to stop and report a question instead of changing an order of operations.
- Do not let a subagent commit. The main session commits, in rank order: `W3SDS4`, then
  `W3SIN4`, then `W3SPR4` and `CALC_USTAR`, then the tables.

## 7. Tasks

### Task 0: Preflight (0:00-0:30)

**Files:** none.

- [ ] **Step 1: Check the account.**
  Run: `python3 -c "import json,os;print(json.load(open(os.path.expanduser('~/.claude.json')))['oauthAccount']['organizationType'])"`
  Expected: `claude_team`. Any other value: stop.
- [ ] **Step 2: Check the tree and the pin.**
  Run: `git status --short && git -C WW3 rev-parse --short HEAD`
  Expected: a clean tree and `bb08623a`.
- [ ] **Step 3: Branch from `main`.**
  Run: `git fetch origin && git switch -c claude/45-port-st4 origin/main && git branch --unset-upstream`
- [ ] **Step 4: Get the owner's go for the fork branch.** `port-next` puts the caller patch on the
  fork only when the owner asks. Write the question. Do not start Task 7 without the answer.
- [ ] **Step 5: Clone the fork outside this repository.**
  Run: `git clone --branch hoff/kokkos-snl1 git@github.com:h0ffmann/WW3.git ~/src/WW3-kokkos-st4 && git -C ~/src/WW3-kokkos-st4 switch -c ww3-gpu/kokkos-st4`
- [ ] **Step 6: Check that the profile rank still holds.**
  Run: `python3 scripts/port_score.py --check && sed -n 15,25p kokkos/PORT_RANKING.md`
  Expected: `w3sds4` is row 1 and `w3sin4` is row 2.

### Task 1: Reference arithmetic (0:30-1:00, subagent A)

**Files:** none in the repository. Output in the scratchpad.

- [ ] **Step 1: Build WW3's `w3src4md` the way the regtests build it.** Configure WW3's CMake in
  the scratchpad with `switch_lab_shrd`, `CMAKE_BUILD_TYPE=Release`, and add
  `-fdump-tree-optimized` to the Fortran flags of that one target. Read the real flags from the
  generated `flags.make`.
- [ ] **Step 2: For each of `W3SDS4`, `W3SIN4`, `W3SPR4` and `CALC_USTAR`, list from the dump:**
  every `__builtin_powf` and `__powisf2` call; every integer power that became a multiplication
  tree, with the tree; every constant folded at compile time (`PI**2.`, `KAPPA**2`, `GRAV**2`);
  the loop order of each `SUM`, `DOT_PRODUCT` and `MAXVAL`; any FMA (expected: none).
- [ ] **Step 3: Return the inventory as a table**, `source line | Fortran | what -O3 computes`.
  The main session copies it into each port report, section "Arithmetic".

### Task 2: Common layer (1:00-1:45)

**Files:**
- Create: `kokkos/src/ww_kokkos/st4_params.hpp`, `kokkos/src/ww_kokkos/st4_tables.hpp`,
  `kokkos/src/ww_kokkos/st4_tables.cpp`, `kokkos/src/fortran_iface/st4_shim.cpp`
- Modify: `kokkos/src/fortran_iface/ww_kokkos_c.hpp`, both `CMakeLists.txt` of section 10.1,
  `kokkos/CMakeLists.txt`

**Interfaces produced** (Tasks 3 to 8 and Task 7 use these names):

```cpp
// ww_kokkos_c.hpp
extern "C" {
int ww_st4_init(int imod, int nk, int nth, const float* sig, const float* rpar, int nrpar,
                const int* ipar, int nipar, const float* const* tables, const int* table_sizes,
                int ntables);
int ww_sds4(int igrid, int npts, const float* a, const float* k, const float* cg,
            float depth, float dair, float ustar, float usdir, float dlwmean,
            float* srhs, float* ddiag, float* brlambda, float* whitecap);
int ww_sin4(int igrid, int npts, const float* a, const float* cg, const float* k,
            float u, float ustar, float drat, float as, float usdir, float z0, float cd,
            float* tauwx, float* tauwy, float* tauwnx, float* tauwny,
            float* s, float* d, int* llws);
int ww_spr4(int igrid, int npts, const float* a, const float* cg, const float* wn,
            float u, float udir, float tauwx, float tauwy, const int* llws,
            float* ustar, float* usdir, float* emean, float* fmean, float* fmean1,
            float* wnmean, float* amax, float* cd, float* z0, float* charn,
            float* fmeanws, float* dlwmean);
}
```

The argument order of `ww_sin4` follows the Fortran call at `w3srcemd.F90:1250`, so the caller
hunk is easy to read against the original. The error codes extend the ones at
`ww_kokkos_c.hpp:43-47`; add `5` = unsupported switch (`W3_FLX5`) and `6` = table size mismatch.

- [ ] **Step 1: Write `st4_params.hpp`** with `struct Params` and one `constexpr int` per `RPAR`
  and `IPAR` index. Give each index the Fortran name in a comment.
- [ ] **Step 2: Write `st4_tables.*`.** One `Kokkos::View<float*, MemSpace>` or
  `Kokkos::View<int*, MemSpace>` per table, `LayoutLeft`, filled by one `deep_copy` from a host
  mirror. Check every size against `table_sizes` and return error `6` on a mismatch.
- [ ] **Step 3: Write `st4_shim.cpp`**: a per-grid context (`std::map<int, St4Context>`, as
  `contexts()` in `snl1_shim.cpp:83`), the flags of section 4.3, the Serial fallback, the counters
  for `WW_KOKKOS_ST4_COUNT`. The three entry points return error `1` until Tasks 3 to 5 fill them.
- [ ] **Step 4: Add the sources to CMake** (section 10.1) and build.
  Run: `just kokkos-build serial-debug`
  Expected: the build succeeds with no new warning.
- [ ] **Step 5: Commit.**
  `git add kokkos/ && git commit` with the subject `kokkos: ST4 common layer, parameters, tables
  and C ABI` and `Tested: just kokkos-build serial-debug (passed)`.

### Task 3: `W3SDS4` (1:45-3:45, subagent B)

**Files:** create `kokkos/src/ww_kokkos/st4_sds4.hpp`, `kokkos/src/ww_kokkos/st4_sds4.cpp`; modify
`ww_sds4` in `st4_shim.cpp` (the main session does this merge).

**Interfaces:** consumes `Params`, `TablesView`; produces `w3sds4_point` (section 4.1) and
`launch_sds4(St4Context&, int npts, ...)`.

- [ ] **Step 1: Write the heritage header**: routine, `w3src4md.F90:2043-2659` at
  `WW3@bb08623a`, the authors from the Fortran header, the phase, and "translated, not regtested".
- [ ] **Step 2: Translate section by section, in source order.** Keep the Fortran's section
  numbers as comments. Order: mean slope and `MSSSUM` prefix sum (`:2237-2246`); `SELECT CASE`
  `(1)` saturation (`:2259-2290`); `(2)` (`:2340-2460`); `(3)` (`:2473-2512`); the cumulative
  term, both branches (`:2539-2563`); the turbulence term; the whitecap block (`:2581-2646`).
- [ ] **Step 3: Apply the arithmetic of Task 1** to every power, every transcendental call and
  every reduction. Cite the dump line in a comment where the C++ differs from the source text.
- [ ] **Step 4: Keep quirks 5 to 10** of section 4.6, each with its one-line comment.
- [ ] **Step 5: Build.** Run: `just kokkos-build serial-debug`. Expected: success, no warning.
- [ ] **Step 6: Report** to the main session: the file list, every place where you were unsure,
  and every question about an order of operations. Do not commit.

### Task 4: `W3SIN4` (1:45-3:30, subagent C)

**Files:** create `kokkos/src/ww_kokkos/st4_sin4.hpp`, `kokkos/src/ww_kokkos/st4_sin4.cpp`.

**Interfaces:** consumes `Params`, `TablesView` (`TAUHFT`, `TAUHFT2`, `FWTABLE`); produces
`w3sin4_point` and `launch_sin4`.

- [ ] **Step 1: Write the heritage header** (`w3src4md.F90:423-1003`).
- [ ] **Step 2: Translate in source order**: swell dissipation set-up (`:606-674`); the main
  `IK` scan with the `ITH` loop inside (`:713-797`), sequential; the `W3_STAB3` double pass
  (`:683-697`, `:806-813`) under a compile-time `if constexpr`; the high-frequency tail
  (`:835-930`) with the fixed `Real w[50]`; the stress outputs (`:939-972`).
- [ ] **Step 3: Apply Task 1's arithmetic.** `COSWIND**SSINTHP` at `:756` stays `std::pow`
  unless the dump shows otherwise.
- [ ] **Step 4: Keep quirks 1 to 4** of section 4.6.
- [ ] **Step 5: Build** (`just kokkos-build serial-debug`) **and report.** Do not commit.

### Task 5: `W3SPR4` and `CALC_USTAR` (1:45-2:45, subagent D)

**Files:** create `kokkos/src/ww_kokkos/st4_spr4.hpp`, `kokkos/src/ww_kokkos/st4_spr4.cpp`.

**Interfaces:** consumes `Params`, `TablesView` (`TAUT`, `DELU`, `DELTAUW`); produces
`w3spr4_point`, `calc_ustar` (a `KOKKOS_INLINE_FUNCTION`, called by `w3spr4_point` as at
`:361`) and `launch_spr4`.

- [ ] **Step 1: Write the heritage headers** (`:142-384`, `:1849-2011`).
- [ ] **Step 2: Translate `CALC_USTAR`**: the table branch when `SINTAILPAR(1) > 0.5`
  (`:1938-1948`), else the 10-step Newton loop (`:1969-1983`). Keep 10 steps; do not add a
  convergence test.
- [ ] **Step 3: Translate `W3SPR4`.** Keep the sums of `:299-332` in their order (quirk 11).
- [ ] **Step 4: Build and report.** Do not commit.

### Task 6: Translation review (3:45-4:45, subagents E, F, G)

This is a code review, not a test. It reads the C++ and the Fortran side by side. It runs
nothing.

- [ ] **Step 1: Start one fresh reviewer per routine.** Give it the Fortran range, the C++ file,
  section 4 of this plan and Task 1's inventory. Ask for a report of every line where the C++
  does a different operation, a different order, a different type or a different power
  expansion, with both `file:line`.
- [ ] **Step 2: Static parity (ADR-0011, ADR-0001's GIMPLE diff).** For each routine, compare
  the optimised GIMPLE of the Fortran (Task 1's dump) with that of the C++ kernel, built with the
  `openmp-release` flags plus `-fdump-tree-optimized` on its source file. Match, in order: every
  `float` operation, every call (`expf`, `logf`, `powf`, `__powisf2`, `tanhf`, `atan2f`), every
  folded constant; and check that no `.FMA` appears. Write the result into the port report,
  section "Static parity", ⚠ until the checkpoint. A mismatch is a defect: fix it, or ask.
- [ ] **Step 3: The main session fixes each confirmed finding.** A finding that needs a change of
  order goes to the owner as a question (section 9), not into the code.
- [ ] **Step 4: Merge the three launchers into `st4_shim.cpp` and build.**
  Run: `just kokkos-build serial-debug && just kokkos-build openmp-release`
- [ ] **Step 5: Commit in rank order**, one commit per routine: `W3SDS4`, then `W3SIN4`, then
  `W3SPR4` with `CALC_USTAR`. Each has `Tested: just kokkos-build serial-debug and
  openmp-release (passed); GIMPLE comparison (<result>); not regtested, waiting for the ST4
  checkpoint (ADR-0011)`.

### Task 7: Fortran wiring in the fork (4:45-6:00)

Only with the owner's go from Task 0, Step 4. Work in `~/src/WW3-kokkos-st4`.

- [ ] **Step 1: `w3kokkosmd.F90`.** Add the interfaces of Task 2, the flags, and a subroutine
  `W3KOKKOS_ST4_SETUP(IMOD)` that packs `RPAR`, `IPAR` and the table pointers from `W3GDATMD`
  and calls `WW_ST4_INIT`.
- [ ] **Step 2: `w3initmd.F90`.** After the `W3IOGR` call and next to the `W3SNL1` init, under
  `#ifdef W3_ST4`, call `W3KOKKOS_ST4_SETUP(IMOD)` and write `Kokkos ST4 (W3SDS4 W3SIN4 W3SPR4)
  enabled: <flags>.` to the log.
- [ ] **Step 3: `w3srcemd.F90`.** Wrap each of the eight calls in the pattern of the `W3SNL1` hunk
  (`w3srcemd.F90:1270-1290`). For `W3SDS4` at `:1333`:

```fortran
#ifdef W3_KOKKOS
      IF ( KOKKOS_SDS4 ) THEN
        !$OMP CRITICAL (WW_KOKKOS_ST4)
        CALL WW_SDS4 ( IGRID, 1, SPEC, WN1, CG1, DEPTH, DAIR, USTAR, USTDIR,   &
                       DLWMEAN, VSDS, VDDS, BRLAMBDA, WHITECAP )
        IERR_K = WW_ST4_LAST_ERROR()
        !$OMP END CRITICAL (WW_KOKKOS_ST4)
        IF ( IERR_K /= 0 ) THEN
          WRITE (NDSE,'(A,I0)') '*** WAVEWATCH III ERROR IN W3SRCE : '//       &
               'KOKKOS W3SDS4 FAILED, CODE ', IERR_K
          CALL EXTCDE ( 1 )
        END IF
      ELSE
        CALL W3SDS4 ( SPEC, WN1, CG1, USTAR, USTDIR, DEPTH, DAIR, VSDS,       &
             VDDS, IX, IY, BRLAMBDA, WHITECAP, DLWMEAN )
      END IF
#else
      CALL W3SDS4 ( SPEC, WN1, CG1, USTAR, USTDIR, DEPTH, DAIR, VSDS,         &
           VDDS, IX, IY, BRLAMBDA, WHITECAP, DLWMEAN )
#endif
```

  Keep every `IF (SINTAILPAR(4) ...)` guard outside the new `IF`. Under `W3_FLX5` keep the
  Fortran call only.
- [ ] **Step 4: Build WW3 with the KOKKOS switch, without running it.** Install the Kokkos library
  (`cmake --install kokkos/build/openmp-release --prefix <scratch>/ww_kokkos`), then configure the
  fork with `switch_lab_shrd` plus `KOKKOS`, `-DWW_KOKKOS=ON` and `ww_kokkos_DIR`, and build.
  Expected: `ww3_shel` and `ww3_grid` link. Do not run them: a run is a regtest.
- [ ] **Step 5: Commit in the fork and push the branch.** Do not move the `WW3/` pin here.

### Task 8: `INSIN4` and the `TABU_*` tables (6:00-6:45, subagent H, time-boxed)

Drop this task if the session is late. The tables from the Fortran (section 4.2) stay the
default either way.

- [ ] **Step 1: Translate `INSIN4`, `TABU_STRESS`, `TABU_TAUHF` and `TABU_TAUHF2` as host code**
  (they run once, at init, so they are not kernels). Write into host mirrors.
- [ ] **Step 2: Do not port the cache file** `ST4TABUHF2.bin` (`:1710-1821`, unit 993). With
  `WW_KOKKOS_ST4_TABLES=1` the C++ always computes the table. The port report says so.
- [ ] **Step 3: Keep the `INSIN4` quirks**: `ECOS(1+|ITH2-ITH|)` as the cosine at `:1269`. Do not
  reproduce the leak of `:1209`; it changes memory, not values. The port report says so.
- [ ] **Step 4: Wire it behind `WW_KOKKOS_ST4_TABLES`** in `ww_st4_init`, build and commit.

### Task 9: Build gates (6:45-7:20)

These are builds, not regtests. `kokkos-test` runs the tests of the intro programs and tools; the
port has none (ADR-0010).

- [ ] **Step 1:** `just kokkos-test serial-debug` (sanitizers and bounds checks). Expected: pass.
- [ ] **Step 2:** `just kokkos-test openmp-release`. Expected: pass.
- [ ] **Step 3: CUDA, compile only.** Run:
  `nix develop picasso/labs/pratico#cuda --command cmake -S kokkos --preset cuda-release && nix develop picasso/labs/pratico#cuda --command cmake --build kokkos/build/cuda-release`.
  Do not run `just kokkos-cuda-test` (it runs `ctest` on the GPU).
- [ ] **Step 4: The fork build of Task 7, Step 4**, again on the final commit.
- [ ] **Step 5:** `python3 .claude/hooks/check_agent_frontmatter.py` only if a skill changed;
  `bash -n` and `shellcheck` only if a script changed.

### Task 10: Ledger, reports, log, PR (7:20-8:00)

- [ ] **Step 1: Five rows in `kokkos/ports.tsv`**, for example
  `w3src4md:w3sds4	model/src/w3src4md.F90:2043-2659	617	ported	translated 2026-10-15, static parity by GIMPLE comparison	no: waiting for the checkpoint of ST4 (ADR-0011)	st4_sds4.cpp,st4_shim.cpp`.
  Check the column order against the header first.
- [ ] **Step 2: One port report per row** with: what was ported; the argument table; the
  arithmetic table from Task 1; the static parity of Task 6; the quirks kept; "Benchmarks: none, no timing without parity";
  "How it was regtested: not yet". Every parity statement carries ⚠.
- [ ] **Step 3: Regenerate the global numbers.**
  Run: `python3 scripts/port_score.py --write && python3 scripts/port_score.py --check`
- [ ] **Step 4: `docs/GLOSSARY.md`**: `WW_KOKKOS_SDS4`, `WW_KOKKOS_SIN4`, `WW_KOKKOS_SPR4`,
  `WW_KOKKOS_ST4_TABLES`, `RPAR`, `IPAR`, and any other new name.
- [ ] **Step 5: The log entry** `docs/log/2026-10-15-st4-kokkos-translation.md`: what was done,
  every review finding and its fix, every open question, what was skipped and why.
- [ ] **Step 6: Gates for the documents.** `git add` the new files first (vale skips untracked
  files), then run `just vale` and `just wfip check` if a WFIP changed.
- [ ] **Step 7: Commit and open the PR** with `just pr`. The PR says in its first paragraph that
  the port is not regtested and that every flag defaults to the Fortran.

## 8. Review focus

The five failure modes most likely to bite, with the place that catches each:

1. **An `OUT` array copied back from device memory that the Fortran never wrote**
   (`WHITECAP`, early return at `:2581`). Caught by the copy-in rule of section 4.4 and by
   reviewer E.
2. **An integer power expanded differently from gfortran `-O3`.** This is the AF11 failure again.
   Caught by Task 1, the review and the GIMPLE comparison of Task 6; the checkpoint is the proof.
3. **A table copied under a different condition from the Fortran's** (`TAUHFT2` only when
   `TTAUWSHELTER > 0`). Caught by Task 2, Step 2, which checks sizes, and by the init log line.
4. **The `IK` scan of `W3SIN4` made parallel.** Caught by quirk 3 and reviewer F.
5. **A switch the port does not support compiled in silently** (`W3_FLX5`). Caught by error code
   `5` at init.

## 9. Stop and ask the owner

Stop the task and write the question when:

- a translation needs a different order of a sum, a limiter or the integration order;
- Task 1 finds an FMA or a reordered reduction in the Fortran's `-O3` code;
- a table size from the Fortran does not match the size the C++ expects;
- the account is not `claude_team`;
- the owner has not given the go for the fork branch at 4:45 (then skip Task 7, and the PR
  holds only the C++).

On CUDA, `expf`, `logf`, `powf` and `tanhf` are not glibc's ⚠. This plan only compiles for CUDA,
so the question is open, not blocking. `AGENTS.md` says a GPU port that cannot meet the
bit-for-bit rule stops and asks the owner.

## 10. CMake changes after the implementation

The owner asked for this list on 2026-10-15. Section 10.1 is done inside the plan. Sections 10.2
and 10.3 are needed before the ST4 checkpoint can run; 10.3 belongs to that checkpoint.

### 10.1 `kokkos/` (done in Tasks 2 to 8)

- `kokkos/src/ww_kokkos/CMakeLists.txt`: add `st4_params.hpp`, `st4_tables.{hpp,cpp}`,
  `st4_sds4.{hpp,cpp}`, `st4_sin4.{hpp,cpp}`, `st4_spr4.{hpp,cpp}` and, after Task 8,
  `st4_tables_build.{hpp,cpp}` to `target_sources(ww_kokkos ...)`. The
  `-ffp-contract=off` and `--fmad=false` lines already apply to the whole target; check that no
  new source is moved to a separate target that would lose them.
- `kokkos/src/fortran_iface/CMakeLists.txt`: add `st4_shim.cpp`. Change the description of the
  `WW_W3KOKKOSMD` cache string and the `FATAL_ERROR` text from `hoff/kokkos-snl1` to the branch
  that the pin will point at (`ww3-gpu/kokkos-st4`), in the same PR that moves the pin.
- `kokkos/CMakeLists.txt`: `project(ww3_gpu_kokkos VERSION 0.2.0 ...)`. The C ABI grows, and the
  package is `SameMinorVersion`, so a WW3 build that asks for 0.2 refuses an old 0.1 install at
  configure time instead of at link time.
- `install(DIRECTORY src/ ... "*.hpp")` already installs the new headers. No change.

### 10.2 The fork, `h0ffmann/WW3` branch `ww3-gpu/kokkos-st4`

- `model/src/cmake/switches.json`: the `KOKKOS` switch has `"requires": ["NL1"]`. After this port
  it carries `NL1` and `ST4` code. Remove the `requires` entry, and guard each shim call with its
  own physics switch (`W3_NL1` around the `W3SNL1` hunk, `W3_ST4` around the ST4 hunks), which the
  caller hunks already do. Then `KOKKOS` builds with `NL1`, `ST4` or both. Without this change, 5
  serial and 3 MPI ST4 entries of `matrix.base` (`NL0`, `NL2`, `NL3`, `NL4`) cannot build with
  `KOKKOS` (v, count of `matrix.base` at `WW3@bb08623a`, 2026-10-15).
- `model/src/CMakeLists.txt`: change `find_package(ww_kokkos REQUIRED)` to
  `find_package(ww_kokkos 0.2 REQUIRED)`, so a library without the ST4 entry points fails at
  configure time.
- `model/src/CMakeLists.txt`: add an error when `KOKKOS` and `FLX5` are both in the switch list
  and `ST4` is too: "the Kokkos ST4 path does not support FLX5". This moves error code `5` from run
  time to configure time.

### 10.3 `regtests/` (the matrix driver; part of the ST4 checkpoint, not run here)

- `regtests/cmake/ww3gpu_common.cmake:88`: the selection word `NL1` is hard-coded. Make it a cache
  variable `WW3GPU_PHYSICS` (`NL1`, `ST4`, or `NL1;ST4`), and keep `NL1` as the default so the
  current records do not change. Change `ww3gpu_sel_desc` (`:113-121`) to print the word.
- `regtests/CMakeLists.txt:28-31`: the expected counts are for `NL1` (100, 50, 161, 311). Add
  the `ST4` counts: 47 serial, 10 OpenMP, 80 MPI, 137 all (v, the same count). With the
  `requires` entry of 10.2 still in place, the counts are those of `ST4` and `NL1`: 42, 10, 77,
  129 (v).
- `regtests/cmake/run_entry.cmake:101-105`: set `WW_KOKKOS_SDS4`, `WW_KOKKOS_SIN4` and
  `WW_KOKKOS_SPR4` from a cache variable `WW3GPU_ROUTINES`, for the K run, and `0` for the F and
  G runs. One routine at a time is the first use.
- `regtests/cmake/run_entry.cmake:162` and `:170`: grep the ST4 log line of Task 7, Step 2, and
  the `WW_KOKKOS_ST4_COUNT` lines, next to the `W3SNL1` ones. Add a column per routine to the
  record.
- `regtests/cmake/ww3gpu_common.cmake:187-209`: the verdict fails an entry with no DIA log line.
  Make it ask for the log line of each selected routine, and report `kokkos_spectra` = 0 per
  routine as "not exercised".
- `regtests/cmake/summary.cmake`: name the routines and the physics word in the record header.
- Gates for 10.3: `just ww3-gpu-matrix-selftest` and `just ww3-gpu-matrix-list`.
- Not CMake, but in the same change: `scripts/ww3-gpu.sh:35-37` and `:293-324` set and grep only
  `WW_KOKKOS_SNL1`. They need the same flags and log line.

### 10.4 Other repositories and the Docker image

- **picasso** (`picasso/`, `labs/pratico`): no change. The ST4 port uses the same Kokkos
  5.2.2, gfortran and CMake as the `W3SNL1` port, and it adds no package (v, the
  dependencies of section 5, 2026-10-15). A picasso PR is needed only if Task 9 finds a missing
  tool.
- **`docker/Dockerfile`**: no change in this plan. It copies `kokkos/` and builds the
  `openmp-release` preset (`docker/Dockerfile:43-53`), so the new sources build with it. When the
  ST4 checkpoint moves the `WW3/` pin, the same PR changes the branch label at
  `docker/Dockerfile:98` (`hoff/kokkos-snl1`) and the comments that say "Kokkos DIA"
  (`:3`, `:10`), and `scripts/docker.sh` passes the new `WW_KOKKOS_*` flags into the container.
  `env/Dockerfile` builds upstream WW3 without `KOKKOS` (`env/Dockerfile:26-34`) and does not
  change.

## 11. What this plan does not decide

- Which `SDSCUM` branch the operational run uses. The port keeps the default; LabECO decides
  (issue #45, task T2).
- Whether the Triton arms of #45 follow ADR-0010 ⚠ (open question for the owner).
- The `W3SRCE` team kernel. ADR-0002 keeps `W3SRCE` after `W3SDS4` and `W3SIN4`.

## 12. Next: the ST4 checkpoint

The port is not usable until the checkpoint of ADR-0011 does, in this order: the CMake changes of 10.2 and
10.3; the pin bump with `just smoke`; `just smoke-ww3-gpu` and `just full-ww3-gpu` with one
routine flag at a time, then all three; `just ww3-gpu-matrix <jobs> all` with
`WW3GPU_PHYSICS=ST4` and `WW3GPU_STRICT=ON`; status L2 in `ports.tsv` only after that; then the
timings, median of three runs, with the machine named. For the budget: one serial `NL1` matrix
took 22 min 37 s at `-j8` on the i9-14900, and its 42 `ST4` entries took 4197 s summed (v,
`docs/data/hoff_matrix_2026-10-15.tsv`).
