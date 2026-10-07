# Benchmark results

Copy this file, fill it in, commit it. Numbers without this metadata are not results.

## Hardware

| | |
|---|---|
| CPU | e.g. Intel Core i9-14900K, 8 P-cores (16 threads) + 16 E-cores |
| RAM | e.g. 64 GB DDR5-6000, dual channel |
| GPU | e.g. NVIDIA RTX 4090, 24 GB, compute capability 8.9 |
| Driver | `nvidia-smi --query-gpu=driver_version --format=csv` |
| PCIe | e.g. Gen4 x16 |
| OS | e.g. Ubuntu 24.04, kernel 6.8 |

## Software

| | |
|---|---|
| NVHPC | `nvfortran --version` |
| gfortran | `gfortran --version` |
| MPI | `mpirun --version` |
| WW3 commit | `git -C $WW3 rev-parse --short HEAD` |
| Switch file | e.g. `switches/switch_lab_mpi` |
| Precision | single (default) / double (`-r8`) |

## 1. Kernel proxy (`kernel_bench`)

Command: `./kernel_X 50000 8 20`

| Build | Resident data [s] | Copy every step [s] | Penalty | Checksum |
|---|---|---|---|---|
| `kernel_cpu` (gfortran, OpenMP, N threads) | | | | |
| `kernel_mc` (nvfortran, `-acc=multicore`) | | | | |
| `kernel_gpu` (nvfortran, `-acc -gpu=cc89`) | | | | |

Checksums must match across all three. If they don't, stop and debug before reporting.

**Implied host↔device bandwidth:** ____ GB/s (printed by the program)

## 2. Heterogeneous split (`hetero_split`)

| cpu_frac | wall [s] |
|---|---|
| 0.0 (all GPU) | |
| 0.1 | |
| … | |
| 1.0 (all CPU) | |

- Best split: ____ on CPU, at ____ s
- Predicted optimum from the two endpoints: ____
- Gain over the better single device: ____×
- **Verdict:** worth it / not worth it, and why

## 3. WW3 MPI scaling (`bench_ww3_cpu.sh`)

Case: `nx=___ ny=___ nk=___ nth=___`, ___ h, ___ global steps
(copy from `case_*/case.json`)

| Ranks | Pinning | Wall [s] | Speedup | Efficiency |
|---|---|---|---|---|
| 1 | — | | 1.00× | 100% |
| 2 | | | | |
| 4 | | | | |
| 8 | P-cores only | | | |
| 8 | unpinned | | | |
| 16 | | | | |
| 24 | all cores | | | |

- Knee of the curve at ____ ranks
- P-core-only vs all-core at the same rank count: ____
- Hyperthreading helped / hurt: ____

## Conclusions

1. Fastest way to actually run WW3 on this machine: ____
2. Would a GPU port be worth it here? ____
3. Anything surprising: ____

## Caveats

- Runs: ____ repetitions, reporting mean / median / best
- Clocks locked: yes / no
- Machine otherwise idle: yes / no
- ⚠ The kernel benchmark is a *proxy*. It is friendlier to a GPU than real
  `W3SRCEMD`, which has far higher register pressure. Any GPU advantage here is an
  optimistic upper bound on what a real WW3 port would achieve.
