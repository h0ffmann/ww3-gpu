# W3SNL1 on a GPU: a DIA kernel proposed upstream and this repository's port, compared

2026-10-15. What a GPU DIA kernel proposed upstream ("GPU parallelization of DIA (W3SNL1):
up to 16.8x speedup (simplified indexing, upper bound)", an issue of NOAA-EMC/WW3; its code is
`consigcody94/ww3-dia-gpu@38919d2`) translated, against what `kokkos/` translated, section by
section of the Fortran, and what each one's speed-up measures. Both target the same routine: `W3SNL1`, the discrete
interaction approximation (DIA) of the nonlinear four-wave source term
(`model/src/w3snl1md.F90:115-473` at `WW3@6d1bcdfe`), with `INSNL1` building its tables
(`:483-786`).

## Sources read

- The proposal, an issue of the NOAA-EMC/WW3 tracker with three comments, opened 2026-03-26 by
  `consigcody94`, open on 2026-10-15 (v) (found with
  `gh search issues --repo NOAA-EMC/WW3 "GPU parallelization of DIA"`, read 2026-10-15).
- Its code: `ww3_dia_benchmark.cu` (259 lines, BSD-3-Clause), in
  [consigcody94/ww3-dia-gpu](https://github.com/consigcody94/ww3-dia-gpu) at `38919d2`
  (2026-03-26), and the same file, byte for byte, in
  [consigcody94/noaa-gpu-kernels](https://github.com/consigcody94/noaa-gpu-kernels)
  `waves/ww3/` at `1c1141f` (2026-10-01), whose README lists the result as "Caveated: simplified
  indexing" (v) (`git clone --depth 1`, `diff`, 2026-10-15). It was read, not built or run: GPU
  runs on this machine are the owner's, and third-party code is data here.
- [h0ffmann/ww3-dia-gpu](https://github.com/h0ffmann/ww3-dia-gpu), the owner's fork of it, made
  2026-10-15: the same commit `38919d2`, no file different (v) (`git clone`, `diff -r`,
  2026-10-15). Everything said below of the proposed code holds for the fork. This repository's
  port, documented as a successor to it, is
  [h0ffmann/ww3-dia-gpu-v2](https://github.com/h0ffmann/ww3-dia-gpu-v2).
- This repository: `kokkos/src/ww_kokkos/snl1_dia.cpp` (the kernel, sections 1 to 4),
  `snl1_tables.cpp` (`INSNL1`), `kokkos/src/fortran_iface/snl1_shim.cpp` (the shim WW3 calls),
  and the timings of [`kokkos/PORT_STATUS.md`](../kokkos/PORT_STATUS.md).

## What each one computes

The Fortran routine, read at the pin, has four sections. The `ww3-dia-gpu` kernel is a model of one
part of section 3; the port is all four, called from inside WW3.

| `W3SNL1` at `WW3@6d1bcdfe` | `ww3-dia-gpu@38919d2` | This repository's port |
|---|---|---|
| 1. depth scaling: `CONS` from `KDMEAN`, `KDCON`, `KDMN`, `SNLS1..3` (`w3snl1md.F90:338-342`) | no: `CONS = 3.0e7`, a constant | yes, per point (`snl1_dia.cpp:111-118`) |
| 2. `UE = A / CONX` with `CONX = TPIINV/SIG*CG`, the high-frequency tail `UE*FACHFE` up to `NFRHGH`, zeroed padding (`:348-376`) | no: `UE` is random numbers in [0.01, 0.51) | yes (`:120-154`) |
| `INSNL1`: the 32 index arrays `IP1n..IM2n`, `IC11..IC82`, weights `AWG1..8`, `SWG1..8`, `AF11 = (SIG*TPIINV)**11` (`:483-786`) | no: four index arrays `(isp ± NTH [+1]) mod n`, eight made-up weights, random `AF11`; its author's own comment calls them "simplified" | yes, built from the grid (`snl1_tables.cpp`), `AF11` with gfortran's `-O3` power chain |
| 3.a energies at the interacting bins: four-point interpolation for each of four members, `EP1, EM1, EP2, EM2`, 16 indexed loads (`:382-391`) | two-point interpolation from four indices; `EP2`/`EM2` reuse `EP1`/`EM1`'s indices with other weights | yes, all 16 (`:157-176`) |
| 3.b `SA1`, `SA2` and the six diagonal terms `DA1C, DA1P, DA1M, DA2C, DA2P, DA2M` (`:394-414`), over `NSPECX` (the extended spectrum) | `SA1`, `SA2` only, over `NSPEC` | yes, all eight, over `NSPECX` (`:177-196`) |
| 4. the source `S` and the diagonal `D`: gathers through `IC11..IC82` with `AWG`/`SWG` (`:416-440`) | no | yes (`:198-221`) |
| called by WW3 (`W3SRCE`) | no: a stand-alone benchmark | yes, through the `KOKKOS` switch on h0ffmann/WW3 |

So the `ww3-dia-gpu` kernel returns `SA1`/`SA2`, an intermediate of section 3, from synthetic inputs;
WW3 never reads those arrays. The port returns `S` and `D`, which `W3SRCE` integrates. The
proposal's statement that the loop is "embarrassingly parallel" holds for section 3 alone; section
4 reads `SA*` and `DA*` of other bins, so a kernel that does both needs every bin's section 3
before any bin's section 4 (in the port, a `team_barrier`, `snl1_dia.cpp:196`).

## What "bit-identical" means in each

| | `ww3-dia-gpu@38919d2` | This repository |
|---|---|---|
| Compared | its parallel CUDA kernel against its sequential CUDA kernel: the same arithmetic on the same GPU, in two loop orders | the Kokkos DIA against the Fortran DIA of the same WW3 executable |
| Result | max relative error 0.000 (the README) | `test.comp`: no file different, on the CPU, on WW3's regtest entries (`just ww3-gpu-matrix`, [`kokkos/port_report/W3SNL1_KOKKOS.md`](../kokkos/port_report/W3SNL1_KOKKOS.md)); on CUDA, bit-identical on the three fixture points, 2026-09-15 |
| Floating-point contraction | `nvcc -O3` without `--fmad=false`: the default fuses `AWG*UE + AWG*UE` and `E00*(...)` into FMAs, one rounding where the Fortran does two ⚠ (from `nvcc`'s documented default; not compiled here) | off: `-ffp-contract=off` on the CPU, `--fmad=false` on CUDA (`kokkos/src/ww_kokkos/CMakeLists.txt:19-31`) |

The proposal's equality is between two programs that perform the same operations in the same order
per bin, so it could not have come out otherwise; it says nothing about agreement with the
Fortran, which the proposal does not test. Agreement with WW3 is what this repository's rule asks
for (`AGENTS.md`, "Bit for bit with the Fortran").

## Speed: what each number measures

| | `ww3-dia-gpu@38919d2` | This repository (`kokkos/PORT_STATUS.md`, 2026-09-15) |
|---|---|---|
| GPU | RTX 3060 (Ampere, compute capability 8.6), CUDA 13.2 | RTX 4090 (Ada, compute capability 8.9), Kokkos 5.2.0 |
| Problem | 10 000 points × 1 152 bins (NK=32, NTH=36), synthetic | 1 000 points × 600 bins (NK=25, NTH=24), the committed fixture tiled |
| Baseline of the ratio | the same GPU, one thread per point looping over the spectrum | the same kernel on one CPU core (Kokkos Serial; i9-14900) |
| Reported | 1.29 ms against 21.56 ms: **16.8x**; 8.95e9 "DIA evaluations"/s | kernel 0.047 ms, through the shim (copy in, kernel, copy out) 0.75 ms; **34x** end to end against one core (25.88 ms), 7.9x against 32 threads (5.95 ms) |
| Work per bin | 5 to 9 loads, 2 outputs | 16 loads in 3.a, 8 outputs in 3.b, 36 gathered loads in 4, outputs `S` and `D` |
| Data transfers | none timed: the data stay on the GPU | timed in the shim column: the shim's time beyond the kernel (copies, fence, launch) is 0.70 of its 0.75 ms |

Normalised by bins, the `ww3-dia-gpu` kernel runs 8.9e9 bins/s (11 520 000 bins in 1.29 ms) and the
port's kernel 1.3e10 bins/s (600 000 bins in 0.047 ms) (v) (arithmetic on the recorded numbers).
These two numbers are not comparable, and neither ratio is a speed-up of WW3:

- the GPUs differ (an RTX 4090 has about 3x the memory bandwidth of an RTX 3060 ⚠, vendor
  specifications, not measured here);
- the `ww3-dia-gpu` kernel does a fraction of the work per bin (no section 1, 2, the diagonal terms
  or section 4), on index arrays that make neighbouring threads read neighbouring memory; its
  author's own comment expects the real `INSNL1` tables to lower its speed;
- its baseline is another layout on the same GPU (10 000 threads, each walking 1 152 bins),
  not WW3 on a CPU, so 16.8x measures the change of layout on one GPU;
- the port's kernel column excludes the copies that phase 1 makes on every call
  (`snl1_shim.cpp`), and inside WW3 the shim is called once per sea point (`NPTS = 1`,
  `w3srcemd.F90:1277`), so WW3 does not see the kernel column. `AGENTS.md`: never read the CUDA
  kernel column as "the model will be this much faster".

## What the two approaches share, and where they differ

- **The parallel decomposition is the same**: one GPU thread per spectral bin, one block (a
  Kokkos team) per grid point. The `ww3-dia-gpu` kernel launches a 2-D grid of `(NSPEC/256, NGRID)` blocks; the
  port launches a `TeamPolicy(npts, AUTO)` and a `TeamThreadRange` over the bins. That mapping
  is not where either gains or loses.
- **The port keeps the Fortran's working arrays in team scratch** (shared memory on the GPU):
  `UE`, `SA1`, `SA2`, the six `DA*` and `CON`, 27.9 KiB per point at NK=25/NTH=24, falling back to
  level-1 scratch when a grid needs more than the device offers (`snl1_dia.cpp:77-94`). The
  `ww3-dia-gpu` kernel reads `UE` from global memory and keeps nothing between sections, because it
  has only one.
- **The port is wired into WW3 and regtested by WW3** (311 `NL1` entries of `matrix.base`,
  serial, OpenMP, MPI, PDLIB and `ww3_multi`); the `ww3-dia-gpu` kernel is not called by WW3, and its
  author's comment lists that integration as not done.
- **Neither has a measured speed-up of WW3 itself.** For the port that is phase 2: keep the
  spectrum resident on the GPU and call the kernel once for all sea points, removing the 0.70 ms
  of copies and synchronisation per call and the per-point calls (`docs/KOKKOS_H100_PLAN_202609.md`).

## What this repository can take from it

Little in the kernel: the thread mapping is the one the port already has. One thing in the
measurement: a benchmark of the kernel with data resident on the device, like the proposal's, at
WW3's real grid sizes and with the real `INSNL1` tables, is what phase 2 needs before it is
built, and `kokkos/tests/bench_snl1.cpp` already separates the kernel from the shim. Any such
number enters a table only with the GPU run supervised by the owner and the regtest replay
passed on the same commit (`AGENTS.md`, "Running on the GPU", "No timing without parity").
