# Operating coding agents on a phased WW3 → modern C++ / Kokkos port

**Status:** working draft, 2026-09-13
**Audience:** humans steering Claude Code (or similar agents) plus the agents themselves
**Goal of the port:** cut wall-clock time of a large ensemble wave forecast (target: ~1000 members) while keeping WAVEWATCH III® (WW3) as the operational driver until each piece is proven.
**Verification:** routine names, source files, regtest names and the WW3 version string below were checked against the `WW3/` submodule (WW3 7.14, `develop`) `(v)`. Published speed-up figures and the WW4 repository description are quoted from the sources in the last section and were not reproduced here `⚠`.

> This file is written to be dropped into a repo as `AGENTS.md` / `CLAUDE.md` (Sections 1 and 3 are the operative rules) or kept as a planning doc (Sections 2 and 4). The tables are deliberately opinionated; "typical share of runtime" numbers are starting priors, **not** substitutes for profiling your configuration.

---

## 0. Context and the one decision that matters most

**Where things stand (Sept 2026)**

| | WW3 (NOAA-EMC/WW3) | WW4 (NOAA-EMC/WW4) |
|---|---|---|
| Language | Fortran 90/2003 (+ preprocessor switches) | Modern C++20/23, Python tooling |
| What exists | Full model: grids, propagation, source terms, I/O, coupling | Driver skeleton: `w4core_init/wave/finalize`, YAML config, calendar/time management, logging, GTest L1/L2 tests. The time loop's "Propagate Solution" is still a placeholder. No source terms, no propagation, no MPI, no GPU. |
| Parallel model | MPI (spectral "card deck" decomposition + gather/scatter), OpenMP hybrid, PDLIB domain decomposition for unstructured | `std::execution::par_unseq` / `std::jthread` mandated by AGENTS.md; nothing performance-critical implemented yet |
| Agent guidance | none (human best-practices guide) | `AGENTS.md` persona "Aldgisl": RAII, no raw ownership, const/constexpr, Concepts, Doxygen with `@author/@date`, GTest, CMake ≥3.20, sanitizers, copyright + GenAI disclosure header |

**Implication:** essentially *every* numerical kernel is "not present in WW4". So the priority list (Section 2) is really "which WW3 loops buy the most forecast-time reduction per engineering hour on a Kokkos target".

**The key architectural decision.** A 1000-member ensemble is a *throughput* problem, not a *latency* problem. Two levers exist:

1. **Per-member speed:** port hot loops to Kokkos, run on GPU. Gains: 2–10× on the ported part, bounded by Amdahl and by host↔device traffic (the E3SM OpenACC port of `W3SRCEMD` got only ~1.3–1.4× per Summit node precisely because too much data crossed PCIe each step).
2. **Ensemble batching:** make the member index an explicit Kokkos dimension so one GPU processes N members per kernel launch, sharing grid/propagation metadata and amortising launch and I/O overhead. WW3 cannot pull this lever at all, and it is the one that lets small per-member grids (typical for ensembles) fill a GPU.

Design every ported kernel from day one with a leading/trailing member dimension (see §1.4), even if phase 1 runs with `nMember = 1`.

---

## 1. Agent guidance: modern, safe C++ + Kokkos for a Fortran port

### 1.1 What to keep from WW4's `AGENTS.md`, and what to override

Keep (they cost nothing and agents comply well):

- RAII everywhere; no `new`/`delete`, no C arrays, no C casts, no `#define` constants.
- `const` / `constexpr` by default; C++20 Concepts for templates.
- Doxygen on every class/struct/function with `@brief @details @param @return @pre @post`, `@author`, `@date` (YYYY-MM-DD).
- File header: WW4-style identification, copyright (current year only for new files), the NWS GenAI disclosure sentence, and **code heritage**: name the original WW3 module/routine and authors when translating.
- CMake ≥ 3.20, GTest, `-Wall -Wextra -Wpedantic`, sanitizers in Debug.
- "Compute-only" rule: kernels never print or write files; inject loggers.

Override (these conflict with Kokkos or with GPU execution):

| WW4 rule | Replacement for this port | Why |
|---|---|---|
| Prefer `std::transform/reduce` with `std::execution::par_unseq` | Use `Kokkos::parallel_for / parallel_reduce / parallel_scan` with an explicit execution space | std::execution has no portable GPU backend; Kokkos gives Serial/OpenMP/CUDA/HIP/SYCL from one source |
| Pass arrays as `std::span` | `std::span` **only** in host-side API; inside kernels use `Kokkos::View` (or `Kokkos::mdspan`) | `std::span`/`std::vector` are not device-accessible |
| `std::jthread` for concurrency | Kokkos execution spaces + explicit `fence()`; MPI for distributed | Threads inside kernels are undefined behaviour |
| `-Werror` unconditionally | `-Werror` on host compilers; warnings-as-errors *selectively* under `nvcc`/`hipcc` (they emit noisy, non-actionable warnings) | Avoid the agent "fixing" warnings by weakening code |
| Exceptions for error handling | Exceptions allowed **outside** kernels only; inside kernels use `KOKKOS_ASSERT` / return flags reduced to host | No exceptions on device |
| Virtual dispatch for pluggable physics | Compile-time selection (`if constexpr`, template tags, `std::variant` resolved on host) for source-term packages (ST4/ST6, Snl variants) | Virtual calls in device code are fragile and slow |

### 1.2 Non-negotiable Kokkos rules for agents

1. **Memory spaces are explicit.** Every `View` names its memory space (`Kokkos::DefaultExecutionSpace::memory_space` or `Kokkos::HostSpace`). Never rely on Unified/Managed memory to hide transfers.
2. **Layout follows the data owner.** Arrays that cross the Fortran boundary are `Kokkos::LayoutLeft` (column-major = Fortran order). Device-native arrays default to the backend's preferred layout. Never `deep_copy` between different layouts without a mirror in between (it is a silent transpose).
3. **No host memory in kernels.** Only `View`s captured by value, scalars, and `KOKKOS_INLINE_FUNCTION` helpers. No `std::` containers, no `this` from a non-trivially-copyable class, no `std::function`.
4. **No allocation inside kernels.** Per-thread scratch goes through `TeamPolicy` scratch memory (`team.team_scratch(0)`) or pre-allocated `View`s. This is the single most common bug when translating WW3, whose routines declare large local arrays (`W3SRCE` alone has dozens).
5. **Fence discipline.** `Kokkos::fence()` before reading device results on host, before MPI on device buffers, and before timers. Do not sprinkle fences "to be safe": it destroys overlap.
6. **Reductions use `parallel_reduce`, not atomics**, unless the access pattern is genuinely scatter (e.g. DIA quadruplet contributions can be done atomic-free by restructuring as gather).
7. **Precision is a template parameter** (`using Real = float;` default). WW3 spectra are REAL(4). Keep `float` for state, use `double` only where WW3 does (`REAL(8)` variables, integral parameters) and document each choice.
8. **Bounds checks in Debug**: configure with `-DKokkos_ENABLE_DEBUG_BOUNDS_CHECK=ON`; CI must run the full test suite once on Serial+bounds-check.
9. **Deterministic order for validation builds.** Provide a `WW_DETERMINISTIC` CMake option that forces serial reductions so bit-reproducibility against WW3 can be checked on CPU.
10. **One `Kokkos::initialize/finalize` per process**, owned by whichever program owns `main` (Fortran in phase 1–3, C++ later). Never initialise inside a library call.

### 1.3 Build and toolchain

```
# Canonical layout
CMakeLists.txt
cmake/presets/{serial,openmp,cuda,hip}.json   # CMakePresets, one per backend
external/kokkos                                 # git submodule pinned to a 4.x tag, or find_package(Kokkos)
src/ww_kokkos/                                  # C++ kernels (namespace ww::kokkos)
src/fortran_iface/                              # bind(C) shims, Fortran side
tests/L1_*                                      # kernel unit tests (synthetic spectra)
tests/L2_*                                      # Fortran<->C++ round-trip and regtest replays
tools/profile/                                  # Kokkos Tools connectors, scripts
PORT_STATUS.md                                  # ledger: routine, phase, owner, parity status
```

Rules:

- Language standard: C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`). Kokkos 4.x accepts C++17+; C++20 lets us use Concepts and `std::span` on the host side.
- `find_package(Kokkos REQUIRED)` and `target_link_libraries(... Kokkos::kokkos)`. Never hand-write nvcc flags; let `nvcc_wrapper`/CMake do it.
- Mixed-language: `project(... LANGUAGES C CXX Fortran)`; Fortran calls C++ through `bind(C)` shims only (see §3.2). No C++ symbol name mangling assumptions, no `-fno-underscoring`.
- Each preset must build and pass `ctest` before a PR is opened. Minimum CI matrix: `serial-debug-boundscheck`, `openmp-release`, plus `cuda-release` or `hip-release` when a GPU runner exists.
- Sanitizers (`-fsanitize=address,undefined`) on Serial/OpenMP builds; `compute-sanitizer` (CUDA) or `rocgdb`/`omniperf` (HIP) on device builds.
- Profiling: build with `-DKokkos_ENABLE_LIBDL=ON` and use Kokkos Tools (`kp_kernel_timer`, `kp_nvtx_connector`/`kp_roctx_connector`). Every kernel gets a human-readable label (`parallel_for("srce.snl1.dia", ...)`) so the profile maps to the WW3 routine it replaces.

### 1.4 Data layout convention for ensemble-ready kernels

```cpp
// Spectral action density, all members, one rank's sea points.
// Index order chosen so the *innermost* (fastest) index is the one threads stride over.
using Real = float;
using SpecView  = Kokkos::View<Real****, Kokkos::LayoutLeft, DeviceMem>;
//                      [ith][ik][isea][imember]   Fortran-order: ith fastest
```

- With `LayoutLeft` and index order `(ith, ik, isea, imember)`, a single member's spectrum at one point is contiguous (matches WW3 `VA(NSPEC, NSEA)`), and the member index is outermost, so a phase-1 `nMember = 1` view is bit-identical in memory to the WW3 array. Zero-copy interop.
- For GPU-optimal *device-native* buffers (phase 4), the agent may introduce a second layout with `imember` innermost and use `Kokkos::Experimental::` remap or explicit pack/unpack kernels, but only after measurement, and only behind a type alias.
- Grid-only data (`DW`, `CG`, `WN`, mask, neighbour tables, propagation coefficients) is shared across members: one `View` for all members, never replicated.

### 1.5 Validation contract (what "done" means for one ported routine)

A PR that ports routine `X` is complete only when the agent has produced **all** of:

1. `src/ww_kokkos/X.hpp/.cpp` with Doxygen and heritage header naming the WW3 module.
2. A `bind(C)` shim `ww_X_c` plus the Fortran interface block, with argument table documented (name, WW3 name, type, intent, shape, units).
3. `tests/L1_test_X.cpp`: unit test on a synthetic spectrum (JONSWAP + cos² spreading is the standard fixture) with `EXPECT_NEAR` tolerances **stated and justified** (e.g. `1e-6` relative for float32 kernels on Serial; `1e-4` on GPU when reduction order differs).
4. `tests/L2_test_X`: a replay of the smallest WW3 regtest exercising `X` (`regtests/ww3_ts1`, `ww3_tp2.x`, or `ww3_tp1.x` as appropriate) comparing WW3 output vs WW3+Kokkos output.
5. A timing line in `PORT_STATUS.md`: WW3 baseline vs Kokkos on Serial, OpenMP and (if available) GPU for the same test.
6. A conservation/sanity property test where physics allows it (Snl: total action conserved to round-off; propagation: mass conserved on a periodic grid; Sds/Sin: sign of net source term at fully-developed state).

"Translate, don't improve" is the phase-1 rule: numerical algorithm and loop nesting may change only when required by rule 1.2-4 (no local arrays). Any algorithmic change (different limiter, different integration order) is a separate PR with its own L2 evidence.

### 1.6 How to task the agent (prompt patterns that work)

Give the agent, in every task:

- The **exact** Fortran routine(s) with file path and version/commit of WW3.
- The **regtest** that exercises it and the reference output file.
- The **interface contract** (which arrays cross the boundary, in which layout, which precision).
- The **phase** (1 = translate; 2 = device-resident; 3 = ensemble batched) so the agent knows which optimisations are off-limits.

Examples:

```
Port W3SNL1 (DIA) from model/src/w3snl1md.F90 @ WW3 7.14 to a Kokkos kernel
ww::kokkos::snl1_dia. Phase 1: translate only. Interface: A(NTH,NK) per point in,
S and D out, all float32, LayoutLeft. Precompute the interpolation weights (the
INSNL1 setup) once on host into a SnlDiaTables struct of Views. Write L1 test
against a JONSWAP fixture, L2 test replaying regtests/ww3_ts1 with ST4+NL1.
Report timings on serial and openmp presets. Do not touch the Fortran caller
except to add the bind(C) interface behind the WW_KOKKOS_SNL1 switch.
```

```
Profile regtests/ww3_ufs1.1 (1deg global structured grid, input/switch_MPI)
on 4 MPI ranks with -pg and with Kokkos kernel timer. Produce a table: routine,
inclusive %, exclusive %, calls. Do NOT change any code. Append the table to
PORT_STATUS.md.
```

Anti-patterns to forbid explicitly:

- "Optimise while translating" (changes numerics silently).
- Replacing WW3 preprocessor switches with runtime `if` inside kernels (kills performance and diverges from WW3 behaviour).
- Claiming GPU speed-ups without an attached timing table.
- Editing WW3 Fortran physics to "make interop easier": the Fortran side is frozen except for shims.
- Introducing Unified Memory to avoid thinking about transfers.

---

## 2. Priority list: what to port first (most expensive loops)

### 2.1 Where WW3 time goes (structured grid, ST4 + DIA, explicit UQ propagation)

Published profiling of WW3 6.07 on Summit (Ikuyajolu et al., GMD 2023) shows the source-term driver `W3SRCE` as the dominant cost, and their GPU port was throttled by host↔device traffic caused by `W3SRCEMD`'s many local arrays and WW3's global data modules. Typical operational-style breakdowns (the numbers below are priors, confirm with §1.6 profiling task):

| Bucket | WW3 modules | Typical share | Notes |
|---|---|---|---|
| Source terms | `w3srcemd` (`W3SRCE`), `w3snl1md`, `w3src4md`/`w3src6md`, `w3sbt*`, `w3sdb1md` | 40–65 % | Per-point, embarrassingly parallel across points and members |
| Spatial propagation | `w3pro2md`/`w3pro3md` (`W3XYP2/3`), `w3uqckmd` | 15–30 % | Per spectral component, stencil, memory-bound |
| Intra-spectral propagation | `W3KTP2/3` (refraction + frequency shift) | 5–10 % | Per point, over spectrum |
| Gather/scatter transposes | `W3GATH`/`W3SCAT` (`w3wavemd`), MPI | 5–25 % | Grows with rank count; disappears if state is device-resident with a single decomposition |
| Output & restart I/O | `w3iogomd`, `w3iorsmd`, `w3iopomd` | 5–20 % | For 1000 members this dominates *system* throughput, not per-member time |
| Forcing update/interp | `w3updtmd` | 2–5 % | Cheap, but the H2D copy it triggers each input step is not |

### 2.2 Ranked port list

Score = (runtime share) × (Kokkos suitability) × (ensemble-batching payoff) ÷ (engineering effort + validation risk).

| # | WW3 routine(s) | What it is | Kokkos mapping | Ensemble payoff | Effort | Phase |
|---|---|---|---|---|---|---|
| **1** | `W3SNL1` + `INSNL1` (`w3snl1md`) | Discrete Interaction Approximation for Snl. Usually the single most expensive kernel; 4 mirror-image quadruplets × NK×NTH interpolations per point | `TeamPolicy` over (isea, imember); team over spectral bins; DIA tables (`IP11..IM42`, weights) precomputed once as shared `View`s. Gather formulation avoids atomics | Very high: identical tables for all members | Medium | 1 |
| **2** | `W3SRCE` driver (`w3srcemd`) | Per-point integration: sums Sin+Snl+Sds+Sbt+Sdb, dynamic sub-time step, limiter, semi-implicit update, updates mean parameters. Contains the "too many locals" problem | One kernel per point (team) that calls inline device functions for each source term; scratch for `SPEC`, `VS`, `VD` etc. This is the routine that makes or breaks device residency | Very high | High (biggest routine; many `#ifdef` switches) | 1 (skeleton) → 2 (device-resident) |
| **3** | `W3SIN4`, `W3SDS4`, `W3SPR4` (`w3src4md`) — or ST6 equivalents | Wind input + dissipation (Ardhuin ST4). Loops over spectrum with a few reductions (mean parameters, breaking probability) | Inline device functions inside the `W3SRCE` team kernel; `team_reduce` for integrals | High | Medium | 1 |
| **4** | `W3XYP2` + `W3QCK1/2/3` (`w3pro2md`, `w3uqckmd`) | ULTIMATE QUICKEST 2-D advection in x/y, per spectral component, with GSE alleviation (`W3PRO3` averaging variant) | `MDRangePolicy<Rank<3>>` over (ix, iy, ispec) per member, or team per (ispec, imember) with vector loop over x. Halo exchange via MPI on device buffers | High: same `CG`, `CX/CY` shared; members differ only in `VA` | Medium-high (boundary handling, masks, curvilinear/SMC grids) | 2 |
| **5** | `W3KTP2/3` | Refraction and frequency-shift advection in (θ, k) space per point | Team per (isea, imember), 1-D UQ sweeps over θ then k | High | Medium | 2 |
| **6** | `W3GATH`/`W3SCAT` + `W3WAVE` loop restructure | Not a loop, but the transposition between spectral decomposition (propagation) and point decomposition (source terms) | Eliminate: keep `VA` device-resident with *one* decomposition (spatial), do propagation with halo exchange (Kokkos + MPI, GPU-aware) | Very high — this is what unlocks phase 4 batching | High (changes WW3's parallel skeleton) | 3 |
| **7** | `W3SBT1` / `W3SBT4`, `W3SDB1`, `W3STR1`, `W3SIC*` | Bottom friction, depth-limited breaking, triads, ice | Inline device functions; trivial once #2 exists | Medium | Low | 1–2 |
| **8** | `W3OUTG` + mean-parameter routines (`w3iogomd`) | Integral parameters (Hs, Tp, dir, partitions via `W3PART`) | `parallel_reduce` per point on device; write only reduced fields to host | High for 1000 members: avoid copying full spectra back | Medium (`W3PART` watershed is awkward on GPU; keep host for now) | 2 |
| **9** | `W3UWND`, `W3UCUR`, `W3ULEV`, `W3UICE` (`w3updtmd`) — wind/current/level/ice interpolation | Time interpolation of forcing to each sea point | Simple `parallel_for`; important only because it triggers H2D copies | Medium (forcing often per-member for ensembles → this *is* member-specific data) | Low | 2 |
| **10** | Restart & field I/O | Binary restart, netCDF fields | Not Kokkos work: async/parallel I/O, one file per member group, device→host staging overlapped with next step | Very high system-level | Medium | 3–4 |
| — | PDLIB / implicit unstructured (`w3profsmd_pdlib`) | Implicit solver, ParMETIS decomposition | Do **not** port early: linear solver + irregular data. Only if your ensemble runs unstructured | — | Very high | later |
| — | `W3SNL2` (WRT exact), `W3SNL3` (GMD), `W3SNL4` (TSA) | Alternative Snl | Only if operationally used; `W3SNL2` is prohibitively expensive anyway | — | High | later |

### 2.3 Ensemble-specific priorities (beyond per-member loops)

1. **Batch members into kernels** (member as a `View` dimension, §1.4) once #1–#3 are device-resident. Expected: small global grids (0.5°–1°, ~50–200k sea points) cannot fill a modern GPU with one member; 8–32 members per GPU is the sweet spot to test.
2. **Share everything that is member-invariant**: grid metadata, propagation coefficients, DIA tables, ST4 tabulations (`TAUHFT` tables), output masks. Load once per process.
3. **Per-member forcing** (wind perturbations) is the main member-specific input; pipeline its H2D copy with compute (Kokkos execution-space instances / CUDA streams).
4. **Output reduction on device**: compute Hs/Tp/Dir/partitions on device, DMA only the 2-D fields. Full spectral output stays optional and staged.
5. **Placement**: N members × M ranks per member is a scheduling problem. Provide a `ww_ensemble` launcher config (members per GPU, ranks per member, output file grouping) rather than baking it into the model.

### 2.4 Profiling recipe (phase 0, mandatory before any port PR)

1. Pick the regtest closest to the operational ensemble configuration (grid, ST package, Snl, propagation scheme, output list).
2. Build WW3 with `-pg -O2` (or run under `perf record -g`, Score-P, or VTune) on 1, 4 and 16 ranks; also run WW3's own `OMPH`/timer switches if enabled.
3. Produce the inclusive/exclusive table by routine and by the buckets of §2.1; record MPI time separately (`mpiP` or `perf` MPI events).
4. Record per-step time and total sea points × spectral bins → derive a "bin-updates per second" figure; this is the metric every Kokkos PR must report.
5. Commit results to `PORT_STATUS.md`. The ranking in §2.2 is re-sorted against these numbers.

---

## 3. Running the partial solution: Fortran + C++/Kokkos together

### 3.1 Three integration strategies

| Strategy | Who owns `main`, MPI, time loop | Use when | Risk |
|---|---|---|---|
| **A. Bottom-up kernel replacement (recommended for phases 1–3)** | WW3 Fortran (`ww3_shel`/`ww3_multi`) | You want forecast-time reductions *now* without waiting for WW4 to have physics | Interop glue; H2D traffic unless state becomes device-resident |
| **B. Top-down driver replacement** | C++ (`ww4_standalone`-style core) calls unported Fortran physics through `bind(C)` | WW4 core matures and you want to converge with it | Two build systems until convergence; WW4 licence/trademark constraints on naming |
| **C. Ensemble orchestrator** | External launcher (Python/Slurm) runs A or B per member group | Immediately, independent of A/B | None — but gives no per-member speed-up |

Do A now, C alongside, and plan to flip to B when ported kernels cover >70 % of runtime.

### 3.2 The interop contract (Strategy A)

**Fortran side** (only file WW3 physics is allowed to change: a new `w3kokkosmd.F90` behind a `KOKKOS` preprocessor switch):

```fortran
MODULE W3KOKKOSMD
  USE ISO_C_BINDING
  IMPLICIT NONE
  INTERFACE
    SUBROUTINE WW_KOKKOS_INIT(COMM_F) BIND(C, NAME='ww_kokkos_init')
      IMPORT :: C_INT
      INTEGER(C_INT), VALUE :: COMM_F        ! MPI_Comm_c2f handle, or -1 without MPI
    END SUBROUTINE
    SUBROUTINE WW_KOKKOS_FINALIZE() BIND(C, NAME='ww_kokkos_finalize')
    END SUBROUTINE
    SUBROUTINE WW_SRCE_BATCH(NSEA, NSPEC, NK, NTH, VA, U10, U10D, DW, CG, WN, DT, IERR) &
         BIND(C, NAME='ww_srce_batch')
      IMPORT :: C_INT, C_FLOAT
      INTEGER(C_INT), VALUE :: NSEA, NSPEC, NK, NTH
      REAL(C_FLOAT), INTENT(INOUT) :: VA(NSPEC, NSEA)   ! action density, WW3 layout
      REAL(C_FLOAT), INTENT(IN)    :: U10(NSEA), U10D(NSEA), DW(NSEA), CG(NK, NSEA), WN(NK, NSEA)
      REAL(C_FLOAT), VALUE         :: DT
      INTEGER(C_INT), INTENT(OUT)  :: IERR
    END SUBROUTINE
  END INTERFACE
END MODULE
```

**C++ side**:

```cpp
extern "C" void ww_srce_batch(int nsea, int nspec, int nk, int nth,
                              float* va, const float* u10, const float* u10d,
                              const float* dw, const float* cg, const float* wn,
                              float dt, int* ierr) {
  using Host = Kokkos::HostSpace;
  using Unmanaged = Kokkos::MemoryTraits<Kokkos::Unmanaged>;
  Kokkos::View<float**, Kokkos::LayoutLeft, Host, Unmanaged> va_h(va, nspec, nsea);
  // ... same for inputs
  auto& st = ww::kokkos::State::instance();      // device-resident copies, allocated once
  Kokkos::deep_copy(st.va, va_h);                 // phase 1: copy in every call
  ww::kokkos::srce_batch(st, dt);                 // launches kernels
  Kokkos::deep_copy(va_h, st.va);                 // phase 1: copy out every call
  *ierr = 0;
}
```

Rules that keep this safe:

- Fortran arrays are passed assumed-size with explicit dims; shapes are asserted on the C++ side (`KOKKOS_ASSERT` in Debug, error code in Release).
- Mind WW3's extents: `CG` and `WN` are allocated `(0:NK+1, 0:NSEA)` in `W3ADATMD` `(v)`, and `VA` is `(NSPEC, NSEA)` with `ISP = ITH + (IK-1)*NTH` (θ fastest) `(v)`. The shim above passes `CG(NK, NSEA)`; the caller must slice (`CG(1:NK, 1:NSEA)`, which copies) or the interface must declare the extended extent and index accordingly.
- `INTENT(IN)` arrays become `const float*`; `VALUE` scalars are passed by value. No derived types across the boundary in phase 1 (flatten).
- MPI: Fortran owns `MPI_Init`. Pass the communicator as `MPI_Comm_c2f` integer and convert with `MPI_Comm_f2c` in C++. C++ never calls `MPI_Init/Finalize`.
- Kokkos: `ww_kokkos_init` is called once from `W3INIT` after `MPI_INIT`, `ww_kokkos_finalize` before `MPI_FINALIZE`. Kokkos needs the local GPU chosen per rank (`--kokkos-device-id` or `Kokkos::InitializationSettings().set_device_id(rank % ngpus)`).
- OpenMP: if WW3 is built with OpenMP threads, Kokkos OpenMP backend shares the same thread pool; do not nest Kokkos calls inside WW3 `!$OMP PARALLEL` regions.
- Precision: `REAL` in WW3 = `C_FLOAT`. If a build uses `-r8`/`-fdefault-real-8`, the shim must be compiled with `Real = double`; make it a CMake option and assert sizes at init.

### 3.3 Runtime switch and A/B validation

Every replaced routine keeps the Fortran original callable. Selection is a namelist/env flag (`WW_KOKKOS_SRCE=0|1`) read once at init, not a per-call branch inside loops. This gives:

- Same binary, two paths → regtest comparison without rebuilds.
- A "shadow mode": run both paths and compare on a sampled subset of points every N steps (debug builds only). This catches divergence early on real forcing that synthetic tests miss.

Acceptance thresholds (store in `tests/tolerances.yaml`, not in code): per-field L∞ and RMS differences vs WW3, e.g. Hs 1e-4 m relative on CPU Serial, 1e-3 m on GPU; spectra 1e-5 relative. Anything larger needs a written justification in the PR.

### 3.4 Data residency ladder (how H2D traffic goes away step by step)

| Step | What lives on device | What crosses per time step | Expected effect |
|---|---|---|---|
| 1 | Nothing persistent; copy `VA` in/out per call | Full spectra, twice | Correctness only; may be *slower* than Fortran |
| 2 | `VA`, grid metadata, tables persistent; source terms on device | Forcing fields (small), output fields (small) | Source-term speed-up realised |
| 3 | + propagation on device with device halo buffers | Halos, forcing, output | Transposes gone; Fortran driver only sequences calls |
| 4 | + member dimension | Same, batched | Ensemble throughput gain; Fortran loop per member collapses to one call |

Step 2 is where the E3SM OpenACC effort got stuck (data structures in Fortran modules); do it in C++ ownership (a `State` singleton created by `ww_kokkos_init`) rather than mirroring Fortran modules.

### 3.5 Build integration

- One top-level CMake with `LANGUAGES C CXX Fortran`; WW3 is already CMake-based, so add `add_subdirectory(ww_kokkos)` and link `ww3_lib` against `ww_kokkos` when `-DWW_KOKKOS=ON`.
- Preprocessor switch `KOKKOS` added to WW3 `switch` files; guards the calls in `W3WAVE`/`W3SRCE` callers.
- Link order: Fortran main → C++ static lib → Kokkos → CUDA/HIP runtime. Use `target_link_libraries(ww3_shel PRIVATE ww_kokkos)`; CMake handles the mixed-language link line if `CMAKE_Fortran_COMPILER` and CXX are from compatible toolchains (gfortran+g++/nvcc, ifx+icpx, cray).
- Spack environment file pinned for reproducibility (kokkos, netcdf-fortran, mpi, compiler).

### 3.6 Phased roadmap

| Phase | Deliverable | Exit criterion |
|---|---|---|
| 0 | Profiling table; `PORT_STATUS.md`; CI with Serial+bounds-check | Ranking in §2.2 confirmed or re-sorted |
| 1 | `W3SNL1`, ST4 (`Sin`, `Sds`), `W3SRCE` skeleton on Kokkos; copy-in/out | L2 parity on 3 regtests; OpenMP ≥ Fortran speed |
| 2 | Device-resident state; `W3KTP2`; output integrals on device | ≥2× per-member speed-up on GPU for the source-term bucket, measured |
| 3 | `W3XYP2` + halo exchange on device; transposes removed | Whole time step device-resident; Fortran driver only sequences |
| 4 | Member dimension; ensemble launcher; async I/O | Members-per-GPU sweep documented; 1000-member wall-clock target met or gap quantified |
| 5 | Flip to C++ driver (Strategy B / WW4 core) | Fortran retained only for unported optional physics |

---

## 4. Comparison table: WW3 vs WW4 vs this port

| Aspect | WW3 | WW4 (develop, 2026) | This port (WW3 + Kokkos, phased) |
|---|---|---|---|
| Language | Fortran + `switch` preprocessor | C++20/23 + Python | C++20 kernels (Kokkos) called from Fortran, converging to C++ driver |
| Maturity | Operational worldwide | Driver/config/logging skeleton, physics not yet present | Incremental; each routine validated against WW3 |
| Build | CMake (recent), classic `w3_make` legacy | CMake ≥3.20, yaml-cpp, GTest | CMake mixed-language, Kokkos presets per backend |
| Config | Namelists / `ww3_*.inp` (+ `.nml`) | YAML | WW3 namelists + `KOKKOS` switch + `ww_ensemble` YAML for launcher |
| Parallelism | MPI spectral decomposition + gather/scatter; OpenMP; PDLIB for unstructured | `std::execution` / `std::jthread` (none implemented yet) | Kokkos (Serial/OpenMP/CUDA/HIP) + MPI owned by Fortran, GPU-aware halos in phase 3 |
| GPU | No (E3SM OpenACC fork of `W3SRCEMD`, ~1.3–1.4× per node) | No | Yes, target of the port |
| Ensemble support | None inside model; run N jobs | None yet | Member dimension in kernels, shared invariant data, batched output |
| Precision | REAL(4) state, some REAL(8) | Not defined yet | Template `Real`, float32 default, parity-tested |
| Testing | `regtests/` (integration, `matrix` scripts), no unit tests | GTest L1 (unit) / L2 (integration) | L1 per kernel (synthetic spectra) + L2 regtest replays + shadow mode |
| Docs | Manual + `guide.pdf` style rules | Doxygen mandatory, `@author/@date`, GenAI disclosure | Same as WW4 + heritage tags naming WW3 routines |
| Agent guidance | none | `AGENTS.md` (persona "Aldgisl", Jules/Copilot used) | This file; overrides WW4 rules that conflict with Kokkos (§1.1) |
| Memory model | Global module arrays (`W3GDATMD`, `W3ADATMD`, `W3WDATMD`) | RAII, no raw ownership | Device-resident `State` owned by C++, unmanaged host views over Fortran arrays at the boundary |
| I/O | Fortran binary + netCDF post-processing | Not yet | Unchanged in phases 1–3; async/grouped in phase 4 |
| Licence / IP | LGPL v3, WAVEWATCH III® trademark | Separate LICENSE/INTENT/TRADEMARK; WW4™ | Derived from WW3 → LGPL obligations apply; do **not** call it WW4 unless contributing to NOAA-EMC/WW4 under its terms |

---

## 5. Quick reference for agents (paste into system prompt)

- You are porting WW3 kernels to Kokkos in phases. Phase 1 = translate faithfully; no numerics changes.
- Kokkos rules: explicit memory spaces; `LayoutLeft` at the Fortran boundary; no host containers or allocation in kernels; scratch via `TeamPolicy`; `parallel_reduce` not atomics; float32 default; fence before host reads and MPI.
- Every kernel is labelled with the WW3 routine it replaces (`"srce.snl1.dia"`).
- A port PR contains: kernel + Doxygen heritage header, `bind(C)` shim + Fortran interface, L1 unit test with stated tolerances, L2 regtest replay, timing table in `PORT_STATUS.md`, property test where applicable.
- Never edit WW3 physics Fortran except to add shims behind the `KOKKOS` switch.
- Never report a speed-up without the measurement attached; never introduce Unified Memory to avoid data-movement design.
- Ask before: changing loop order that affects reduction order, changing precision, changing a limiter or integration scheme.

---

### Sources consulted

- NOAA-EMC/WW4 repository: `README.md`, `AGENTS.md` (Aldgisl protocol), `ARCHITECTURE.md` (develop branch, Sept 2026).
- NOAA-EMC/WW3 repository and *WAVEWATCH III development best practices* (Tolman ed., 2019).
- Ikuyajolu et al., "Porting the WAVEWATCH III (v6.07) wave action source terms to GPU", *Geosci. Model Dev.* 16, 2023 — profiling of `W3SRCE` and the data-transfer bottleneck of the OpenACC port.
- Kokkos Core 4.x programming guide (Views, layouts, TeamPolicy scratch, interop) and the Kokkos Fortran interop layer (FLCL) for the `bind(C)` patterns.
