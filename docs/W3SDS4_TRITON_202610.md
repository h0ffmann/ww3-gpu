# W3SDS4 and the Triton arms, in figures

Companion to issue [#45](https://github.com/h0ffmann/ww3-gpu/issues/45), which is the plan and
the source of truth: the execution bottleneck of WW3 (`W3SDS4`, ST4 dissipation), its port to
Triton on CPU and GPU, the benchmark against Fortran and Kokkos, and WeatherNext 3 as wind input.
These five figures summarise that issue as it stood on 2026-10-07; where they and the issue
disagree, the issue wins and the figure is a bug. `(v)` and `⚠` keep their usual meaning.

## Where the source-term time goes

```mermaid
%% figure: sds4-profile-ts1
%% title: Where does the source-term time go, and how much of it is W3SDS4?
%% data: docs/data/ww3_ts1_gprof_202610.md --label routine --value 'ST4 default (SDSCUM=-0.40344)' --value SDSCUM=0 --y-title 'self time (s)'
%%{init: {"themeVariables": {"xyChart": {"plotColorPalette": "#4c6a8c, #c0392b"}}}}%%
xychart-beta
    x-axis ["W3SDS4", "W3SNL1", "W3SIN4", "W3SPR4", "W3SRCE", "others"]
    y-axis "self time (s)" 0 --> 2.21
    bar [2.01, 0.42, 0.3, 0.14, 0.09, 0.03]
    line [0.51, 0.45, 0.27, 0.03, 0.07, 0]
```

<details open>
<summary>How to read this figure</summary>

**Takeaway.** In this test, the routine that dissipates wave energy by breaking (`W3SDS4`) takes two thirds of the source-term time, about five times the nonlinear interaction (`W3SNL1`); with its cumulative-breaking term switched off it costs a quarter as much.

**How to read.** One blue bar per routine: self time in seconds with ST4 at its default settings (`W3SRCE` is its own time, without the routines it calls; *others* is `W3SLN1`, `W3SDB1`, `W3SBT1` and `CALC_USTAR`). The red line is the same routines with `SDSCUM=0`, which removes the cumulative term; where the line sits far below the bar, that term is the cost.

**Not shown.** Propagation, MPI, gather/scatter and output: `ww3_ts1` is a single-point source-term test (3×3 grid, NK=36, NTH=24). The operational grid is NK=32, NTH=36, and the cumulative term grows with NTH², so the gap should widen there ⚠.

**Evidence.** `docs/data/ww3_ts1_gprof_202610.md`, the gprof table of #45 (`WW3@761cf79d`, gfortran 16.2, `-O3 -pg`), transcribed ⚠: task T1 replaces it with committed `kokkos/tools/profile/gprof_table.sh` output. The chart is generated from that file (`python3 scripts/figures.py chart …`, the `%% data:` line) and CI fails if the two drift.

</details>

## Why `SDSCUM` decides the cost

```mermaid
%% figure: sds4-sdscum-branches
%% title: Which branch of the cumulative-breaking term does W3SDS4 run, and what does each cost?
flowchart TD
    S["ST4 parameter SDSCUM<br/>default -0.40344 (w3gridmd.F90:2157),<br/>copied to SSDSC(3) at :2268"] --> Q{"sign of SSDSC(3)"}
    Q -- "negative (the default)" --> N["1 · anisotropic version<br/>w3src4md.F90:2554-2563<br/>for every (IK, ITH): DOT_PRODUCT over NTH<br/>inside a loop over IK2<br/>cost O(NK² NTH²) per point"]
    Q -- positive --> P["2 · isotropic version<br/>w3src4md.F90:2539-2547<br/>once per IK: SUM over NTH<br/>inside a loop over IK2<br/>cost O(NK² NTH) per point"]
    Q -- zero --> Z["3 · no cumulative term"]
    N --> MN["ww3_ts1: W3SDS4 2.01 s,<br/>67 % of source terms"]
    P --> MP["not measured yet"]
    Z --> MZ["ww3_ts1: W3SDS4 0.51 s,<br/>38 % of source terms"]
    MN & MP & MZ -.-> D{{"T2 · LabECO decides which branch<br/>the operational run uses;<br/>the port reproduces that one"}}
```

<details open>
<summary>How to read this figure</summary>

**Takeaway.** One number in the ST4 settings picks one of three versions of the same term, and the default picks the most expensive one, which WW3's own comment calls "the expensive and largely useless version".

**How to read.** Top to bottom. The diamond tests the sign of the parameter; each numbered box is the code that sign selects, with its line numbers and how its cost grows with the spectral grid (NK frequencies, NTH directions). The boxes below give the measured time where there is one. The dashed arrows lead to the decision, the hexagon, which only the LabECO research group can take, because all three versions change the physics.

**Not shown.** The rest of `W3SDS4` (saturation-based dissipation, wave–turbulence interaction), which every branch runs; and `DIKCUMUL`, the frequency offset below which the term is skipped.

**Evidence.** `WW3@761cf79d`: `model/src/w3gridmd.F90:2157` and `:2268`, `model/src/w3src4md.F90:2539-2547` and `:2554-2563` (v, read 2026-10-08). Timings: `docs/data/ww3_ts1_gprof_202610.md` ⚠ (transcribed from #45).

</details>

## The port, task by task

```mermaid
%% figure: triton-port-plan
%% title: In what order is W3SDS4 ported to Triton, and what gate does each step pass?
flowchart TD
    T0["T0 · toolchain: Triton (GPU) and a pinned triton-cpu<br/>just triton-smoke on both backends"] --> T1
    T1["T1 · committed profile<br/>gprof_table.sh on ww3_ts1,<br/>NK/NTH 36/24 and 32/36"] --> T2
    T2{{"T2 · LabECO: which SDSCUM branch?"}} --> T3
    T3["T3 · W3SDS4 fixture from real sea points<br/>+ a NumPy reference"] --> G3{"NumPy passes L1?"}
    G3 -- no --> T3
    G3 -- yes --> T4["T4 · Triton G: literal translation,<br/>no improvements"]
    T4 --> G4{"L1 with a<br/>justified tolerance?"}
    G4 -- no --> T4
    G4 -- yes --> T5["T5 · Triton C: same source on triton-cpu,<br/>every line that has to differ is logged"]
    T5 --> G5{"builds and<br/>passes L1?"}
    G5 -- yes --> T6["T6 · benchmark:<br/>kernel vs kernel, projected model gain"]
    G5 -- "no: recorded as a result" --> T8
    T6 --> T8["T8 · report: is Triton worth continuing?<br/>a negative answer counts"]
    T8 --> T7["T7 · next routine by wall time:<br/>W3SNL1, W3SIN4, then one W3SRCE kernel"]
```

<details open>
<summary>How to read this figure</summary>

**Takeaway.** The slowest routine is rewritten for a new GPU language in small, checked steps, and nothing is timed until its answers match the original within a stated tolerance.

**How to read.** Top to bottom, in the order of the tasks in #45 (T7 comes last because it starts the next routine). Rectangles are work, diamonds are pass/fail gates (a *no* sends the step back to be fixed), and the hexagon is a decision only the research group can take. On the CPU backend a failure to build or to pass is itself a result, so it goes straight to the report.

**Not shown.** The open technical risks listed in #45: transcendental functions that make bit-for-bit agreement on the GPU unlikely, FMA contraction (`enable_fp_fusion=False` ⚠), `tl.dot` changing the order of a reduction (ask before), and the lack of a Fortran shim until Triton's ahead-of-time compiler is used ⚠.

**Evidence.** Issue #45, sections *Tarefas* and *O que deve dar trabalho* (v, 2026-10-07); port order by wall time, `CONTRIBUTING.md` "Port order is wall time" (PR #46).

</details>

## What the benchmark compares

```mermaid
%% figure: triton-benchmark-arms
%% title: Which implementations of W3SDS4 are timed, and when does a timing count?
flowchart LR
    FX[("W3SDS4 fixture<br/>1 000 sea points,<br/>NK/NTH 36/24 and 32/36")]
    FX --> F["Fortran W3SDS4<br/>gfortran -O3, 1 thread"]
    FX --> K["Kokkos W3SDS4<br/>1 and 32 threads, RTX 4090<br/>(once task P1.6 exists)"]
    FX --> TC["Triton C<br/>triton-cpu at a pinned commit<br/>1 and 32 threads"]
    FX --> TG0["Triton G, fusion off<br/>RTX 4090"]
    FX --> TG1["Triton G, fusion on<br/>RTX 4090, speed ceiling"]
    F -. reference output .-> L1{"agrees with the Fortran<br/>within the L1 tolerance?"}
    K & TC & TG0 & TG1 --> L1
    L1 -- yes --> TAB["row in the timing table:<br/>kernel-only ms and end-to-end ms,<br/>3 warm-up calls, 20 timed, median of 3 runs"]
    L1 -- no --> OUT["left out of the table"]
```

<details open>
<summary>How to read this figure</summary>

**Takeaway.** Five versions of the same routine get the same input, and only those whose results agree with the original Fortran are allowed into the speed comparison.

**How to read.** Left to right. The cylinder is the shared input. Each rectangle in the middle is one implementation (an "arm") with its build and hardware. The Fortran arm also provides the reference output (dashed arrow) that the diamond checks every other arm against; *yes* puts its timing in the table, *no* keeps it out.

**Not shown.** The whole-model run (`just bench-case`, `bench/bench_ww3_cpu.sh`), where the gain is a projection until a kernel runs inside WW3; the H100 rows; and Triton G's JIT compile time, which is reported apart from the median.

**Evidence.** Issue #45, *Como medir* §1 (v, 2026-10-07); timing protocol as in `kokkos/tests/bench_snl1.cpp` (v); in `W3SNL1` on Kokkos CUDA the host↔device copy is 94 % of a call, which is why end-to-end is reported next to kernel-only (`kokkos/PORT_STATUS.md`, v).

</details>

## Where WeatherNext 3 enters

```mermaid
%% figure: weathernext-wind-chain
%% title: How does WeatherNext 3 enter the benchmark, and what are the waves compared with?
flowchart LR
    subgraph WIND[10 m wind, the forcing]
        GFS["GFS / ERA5<br/>(today's forcing)"]
        WN["WeatherNext 3<br/>64 members, 0.1°, 15 days<br/>wind only, no wave variables"]
    end
    GFS --> PRNC["ww3_prnc<br/>forcing preprocessor"]
    WN -- "each member" --> PRNC
    PRNC --> WW3["WW3 run<br/>Fortran, or with Triton / Kokkos kernels"]
    WW3 --> HS["Hs, period, direction<br/>from WW3"]
    AIFS["ECMWF AIFS Single Wave<br/>ML wave forecast, 0.25°"] --> HSML["Hs, period, direction<br/>from ML"]
    HS -. "verified against" .-> OBS[("buoys and altimeters<br/>same period, same set")]
    HSML -. "verified against" .-> OBS
```

<details open>
<summary>How to read this figure</summary>

**Takeaway.** WeatherNext 3 does not forecast waves: it supplies the wind that drives WW3, so the same wave model can be run with a different wind, and each of its 64 ensemble members becomes one WW3 run. The machine-learning wave forecast in the comparison is ECMWF's AIFS Single Wave.

**How to read.** Left to right. The group on the left holds the two wind sources; either one goes through the same preprocessor into the same WW3. The AIFS row is a separate wave forecast that needs no WW3. Dashed arrows are verification: both sets of wave fields are scored against the same observations over the same period.

**Not shown.** Data access (BigQuery, Earth Engine or GCS after a request form), licences and cost ⚠, and WeatherNext 3's own run time, which has not been published ⚠.

**Evidence.** Issue #45, *Como medir* §3 (v, 2026-10-07), which cites the WeatherNext model and access guides and the ECMWF AIFS Single Wave dataset page.

</details>
