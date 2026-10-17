# Benchmarks: i9 vs RTX 4090

## Read this before running anything

**You cannot benchmark "WW3 on the GPU" because that binary does not exist.** WW3 has no
GPU code path upstream. Any benchmark claiming otherwise is either running a research
branch or measuring nothing.

So this directory measures three separate, honest things:

| What | Program | Measures |
|---|---|---|
| **The real WW3, CPU only** | `bench_ww3_cpu.sh` | Actual WW3 wall time versus MPI rank count on your i9. This is the number that should decide how you run the model. |
| **A WW3-shaped kernel, CPU vs GPU** | `kernel_bench.f90` | The closest honest proxy: same data layout, same loop structure, same near-cancelling source terms as `W3SRCEMD`. Two modes: data resident on the device, and copied every step. |
| **CPU and GPU at the same time** | `hetero_split.f90` | Sweeps the work split from all-GPU to all-CPU and finds your optimum. This is the direct answer to "can I use both together?" |

A measurement that a table quotes is kept as data in [`results/`](results/), one JSON record per
study: machine, toolchain, build commit, parity status, statistic, and each run's command and numbers.
The table on the page is generated between `<!-- results:<study>:start -->` and `:end -->` markers
by `python3 scripts/results.py table`; CI (`results.py check`) fails when a record lacks a command
or a passed parity check, or a quoted table no longer matches its record. Parity is a passed WW3
regtest replay on the same commit (`regtest: passed: <command>`,
[ADR-0010](../docs/ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md)); a record made before
2026-10-14 keeps the `l1: passed` of its day as history.

```bash
make run                                   # the kernel benchmarks
just bench-case --size medium -o bench/case_medium   # generate a WW3 case (from the repo root)
bash bench_ww3_cpu.sh $WW3/build case_medium
just bench                                 # or just do everything (builds the generator first)
```

### WW3 against ww3-gpu, end to end

`just perf-ww3-gpu [case] [runs] [parts]` (GPU, the owner's supervised run; `parts` is `gpu`, SRCE
alone, by default, or `resident`, SRCE with the PR3/UQ propagation so the spectrum stays on the
device between output times; `--cpu` through
`scripts/ww3-gpu.sh --ww3-gpu <case> --cpu --repeat N` for the OpenMP backend) times `ww3_shel`
of pure NOAA WW3 (`--ww3`) against ww3-gpu, both built with `switches/switch_lab_ompg`. It first
compares the Kokkos run with the Fortran of the same executable and with `--ww3` (`nccmp-tol`),
then alternates N runs of each, checks every output against run 1's, and prints the medians.
Only then does it write `build/runs/ww3-gpu-<case>-<backend>/perf.tsv`.
`python3 scripts/results.py record <perf.tsv>` turns it into a record here. It refuses fewer than
three runs and never overwrites a record, and the record says so when the tree had uncommitted
changes. The baseline is OpenMP on one CPU node; NOAA runs WW3 under MPI on many nodes, so the
ratio is not a speed-up over operations.

### The case generator

`ww_bench_case` is a C++ program in `kokkos/tools/bench_case/`, built with the rest of
the `kokkos/` tree (`just kokkos-build openmp-release` puts it at
`kokkos/build/openmp-release/tools/bench_case/ww_bench_case`; `just bench-case …` builds
and runs it in one go). It writes a complete run directory with no external data:

```
ww_bench_case [--size small|medium|large] [--nx N] [--ny N] [--nk N] [--nth N]
              [--hours H] [--dx-km X] [-o DIR]
```

| size | grid | spectrum | hours | rough serial time |
|---|---|---|---|---|
| `small` | 120 × 80 | 24 × 24 | 12 | ~10 s |
| `medium` | 300 × 200 | 32 × 36 | 24 | ~90 s |
| `large` | 700 × 450 | 32 × 36 | 48 | ~15 min |

The timesteps are derived from `--dx-km` through the CFL condition (`DTXY` rounded
*down* to tens of seconds, `DTMAX = 3 DTXY`, `DTKTH = DTMAX/2`), so a resized case stays
stable. An unstable run does a different amount of work, which would make the
benchmark lie. `case.json` records every derived number for the write-up.

Field output is off (`DATE%FIELD` stride `'0'` in `ww3_shel.nml`) because the case
times compute, not disk. To compare two runs field by field with
[`nccmp-tol`](../kokkos/tools/nccmp-tol/), set that stride to `'3600'`, rerun `ww3_shel`
and run `ww3_ounf` with the `ww3_ounf.nml` the generator also writes.

The generator replaced a Python script; its `--size small` output is pinned byte for
byte to that script's captured files by `kokkos/tests/L1_test_bench_case.cpp`, so
numbers taken before and after the rewrite are comparable.

### The forced case (`coast`)

`smoke` is one point and `full` (`bench_small`) is 500 m of water everywhere with a constant wind and no current, level, ice or boundary input, so
neither reaches the branches of W3WAVE that forcing switches on. `coast` does:
`bash scripts/ww3-gpu.sh --ww3-gpu coast --cpu --kokkos resident` (`just coast-ww3-gpu`). Its
namelists are in [`kokkos/tests/fixtures/coast/`](../kokkos/tests/fixtures/coast/); the depth,
the mask, the forcing and the boundary spectra are written into the run directory by
[`scripts/coast_case.sh`](../scripts/coast_case.sh) (awk writes CDL, `ncgen` turns it into
netCDF), and `ww3-gpu.sh` runs `ww3_prnc` three times and `ww3_bounc` once, with every Kokkos part
off, before `ww3_shel`, so both runs of a pair read the same forcing files.

| What | In the case | The WW3 path it opens |
|---|---|---|
| grid | 80 x 50 Cartesian cells of 5 km, 3 791 sea and 50 boundary points, NK = 24, NTH = 24, 12 hours, `DTMAX` 600 s, `DTXY` 150 s, `DTKTH` 300 s | |
| bathymetry | a shelf from 100 m to 8 m, a 1 m intertidal strip, an island of nine cells on a shoal | refraction on depth gradients |
| currents | hourly, `ww3_prnc` as is: a tidal along-shore jet (0.8 m/s), a cross-shelf tide, an eddy round the island | `FLCUR` (`W3UCUR`, `CRT1`/`CRX1`), the current terms of `W3KTP3`, and with `FLCK` on `W3QCK2` |
| water levels | hourly: a tide of 0.8 m offshore to 1.5 m at the coast | `W3ULEV`, the strip falling dry and wetting again, `FLMAP` and `W3MAP3` |
| ice | hourly concentration growing from the north-east corner (`IC0`: a cell at `CICEN`, 0.5 by default, `WW3/model/src/w3gridmd.F90:2750`, or more leaves the active sea points, `w3updtmd.F90:1956` at `90631fad`) | `W3UICE`, `MAPSTA` changing during the run |
| boundary | three JONSWAP spectra on the west edge, Hs 1.5 to 2.9 m, through `ww3_bounc` into `nest.ww3` | `W3IOBC`, `W3UBPT` and the boundary points of `W3XYP3` |
| wind | homogeneous, 10 m/s from 250° veering to 15 m/s from 220° | |

The output is every hourly field of the case (`HS FP DIR DP T0M1 CUR WLV ICE DPT`) and 5 point
spectra every 30 min (`out_pnt.ww3`). Whether `FLCK` reaches the answer was checked once, by hand,
on 2026-10-17: the same `--ww3` case with `RUN%FLCK = F` gave an `hs` up to 0.073 m different
(`nccmp-tol` FAIL on `hs`, `dir`, `dp`), so a port of `W3QCK2` that went wrong would show in the
output (v). None of the forcing is a physical study; it is a case that takes those branches.

---

## Can the i9 and the 4090 work on the same problem together?

**Technically yes, and `hetero_split.f90` does it.** The mechanism is straightforward:
launch the GPU kernel with OpenACC `async`, which returns control to the host immediately,
then run an OpenMP loop on the host over the other half of the data, then `!$acc wait` to
join. Both devices are busy simultaneously. `nvfortran -acc -mp` compiles it.

**Whether it's worth it is arithmetic, and the arithmetic is usually discouraging.**

If the CPU processes work at rate `R_cpu` and the GPU at `R_gpu`, the optimal split puts
fraction `f = R_cpu / (R_cpu + R_gpu)` on the CPU, and the best possible speedup over
using the faster device alone is:

```
(R_cpu + R_gpu) / max(R_cpu, R_gpu)
```

So:

| GPU is this much faster | Max gain from adding the CPU |
|---|---|
| 2× | +50% |
| 5× | +20% |
| 10× | +10% |
| 20× | +5% |

And that's the *ceiling*, before you pay for anything. What you actually pay:

1. **PCIe.** Keeping both devices' copies coherent means shipping the CPU's slice to the
   device and pulling the GPU's slice back, every step. `hetero_split.f90` charges you for
   this honestly with `update device` / `update self`. At ~25 GB/s effective and a
   230 MB spectrum array, that's ~18 ms per direction, often more than the compute.
2. **Synchronisation.** The join costs you whatever the slower half overruns by, and the
   split is static so it can't adapt to jitter.
3. **Complexity.** Two code paths, two sets of bugs, results that depend on the split.

**Where it genuinely does pay:** when the two devices are within a small factor of each
other, when the data is already resident on both, and when the kernel is compute-bound
rather than transfer-bound. Run `hetero_split` and find out which side of that line your
box falls on. If the best split comes back at 0.00 or 1.00, you have your answer.

### Three other senses of "use both together" that work better

**MPI ranks sharing one GPU.** This is what the [GMD 2023 WW3 port](https://gmd.copernicus.org/articles/16/1445/2023/)
actually did: several CPU MPI ranks each offloading to the same GPU. They found packing
3 or 4 ranks per GPU barely changed the ~1.3× result, because the bottleneck was transfer
bandwidth, not GPU occupancy. Still, this is the standard pattern in production HPC and
the one WW4 will likely use.

**Pipelining across subdomains.** In real WW3 the source-term step and the propagation
step are sequentially dependent within a timestep, so you can't split them the way
`hetero_split` splits points. What you *can* do is have the GPU work on subdomain N's
source terms while the CPU does subdomain N−1's propagation. Real, standard, and a
significant refactor.

**Different jobs entirely.** The best use of your hardware today: run WW3 on the i9 with
MPI across all cores, and use the 4090 for something it's actually good at: a parameter
sweep's post-processing, training an ML emulator on the output, a Celeris or DualSPHysics
run, or an FFT ocean surface renderer fed by `ww3_ounp` spectra. Zero contention, both
devices saturated, no code to write.

---

## What to expect

Rough priors so you can tell a broken measurement from a real one. These are predictions,
not measurements. ⚠ Nothing here was run on your hardware, or any hardware.

**The kernel, data resident on device:** the 4090 should beat a fully loaded i9
substantially. This kernel is friendlier to a GPU than real WW3 code, being
single-precision, low register pressure, uniform trip count. Treat it as an optimistic
upper bound.

**The kernel, copying every step:** expect the advantage to collapse, possibly to below
the CPU. That collapse is the entire story of the GMD paper reproduced in one program, and
it's the reason "just add OpenACC to `W3SRCEMD`" doesn't work.

**FP64:** don't run this in double precision expecting anything good. The 4090 runs FP64 at
roughly 1/64 of FP32. See `../gpu/03_precision.f90`.

**WW3 MPI scaling on the i9:** near-linear early, with efficiency falling off as
communication in the "shuffle" decomposition starts to dominate. Where the knee sits
depends on case size: if the curve is flat from rank 1, your case is too small.

---

## The i9 trap you will fall into

Modern Intel i9s have **P-cores and E-cores with very different throughput**, plus
hyperthreading. An MPI job runs at the pace of its slowest rank, so scattering ranks
across both core types means every rank waits for the E-cores.

```bash
lscpu -e                        # see your actual topology first
mpirun -np 8 --bind-to core --cpu-set 0-15 ./ww3_shel     # P-cores only
```

It is common for **8 pinned P-cores to beat 24 unpinned mixed cores** on a memory-bound
spectral model. Measure both. Also test 1 rank per *physical* core against 1 per *logical*
core. Hyperthreading rarely helps a bandwidth-limited workload.

This affects the `hetero_split` result too: if the CPU half is running on E-cores, you're
measuring the wrong CPU.

---

## Methodology notes

**Wall time, not CPU time.** Everything here uses `SYSTEM_CLOCK`. `CPU_TIME` sums across
OpenMP threads, so on 16 threads it reports roughly 16× the wall time and makes threading
look like a catastrophic slowdown. This is the single most common bug in homemade
benchmarks.

**Compute, not disk.** The generated WW3 case sets the field output stride to `'0'`.
Writing hourly fields on a 300×200 grid can easily dominate the measurement and turn a
compute benchmark into a filesystem benchmark.

**`ww3_grid` is excluded.** It's serial setup. Including it puts a fixed serial cost in
every measurement and flattens your scaling curve for no reason (Amdahl, self-inflicted).

**Correctness first.** `make kernel_mc` builds the same OpenACC directives for CPU threads.
If `kernel_mc` and `kernel_gpu` produce different checksums, you have a data-movement bug,
not a physics result. Never report a speedup you haven't checked this way.

**Run it more than once.** Thermals matter: a 4090 and an i9 both clock down under
sustained load, and the first run of a sweep is often the fastest for reasons that have
nothing to do with your code. Report medians.

**Fix the clocks if you want reproducibility.**
```bash
sudo nvidia-smi -pm 1
sudo nvidia-smi -lgc 2100         # lock GPU clocks; check your card's range first
sudo cpupower frequency-set -g performance
```

---

## Writing it up

If you publish numbers, report all of this or they mean nothing:

- CPU model, core counts (P/E separately), RAM speed and channels
- GPU model, driver version, `nvaccelinfo` output
- Compiler and exact flags, NVHPC version
- Case dimensions: `nx`, `ny`, `nk`, `nth`, timesteps, number of global steps
- Whether it's single or double precision
- Whether the transfer cost is included (it usually isn't, in other people's numbers)
- Thread/rank pinning
- Number of repetitions and whether you report mean, median or best

The most common way GPU speedups get inflated is comparing an optimised GPU kernel against
*one* CPU core. The GMD paper's 1.3× is against **42 cores**, which is why it's a credible
number and most blog-post speedups aren't.
