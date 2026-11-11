# Checklist for the owner's supervised GPU run, October 2026

The steps, in order, for the first runs of the Kokkos port on Kokkos' CUDA backend, and what each
one must leave on record. Only the owner runs them, at a terminal, watching the run (AGENTS.md,
"Running on the GPU"); every command below asks for a typed "yes" and exits 7 without one. An
agent never runs them, never sets `WW3GPU_IN_SHELL` or `WW3GPU_GPU_CONFIRMED`, and never pipes
the answer.

A GPU run is judged by `nccmp-tol` with `kokkos/tools/nccmp-tol/tolerances.txt`, against the
Fortran of the same executable and against `--ww3-noaa`, and a tolerance is never widened to make it
pass ([ADR-0014](ADRs.md#adr-0014)). Why the bits can differ, site
by site, is [`GPU_LIBM_BITS_202610.md`](GPU_LIBM_BITS_202610.md). This page is unit U5 of
[`plans/2026-10-18-002-production-single-node-plan.md`](plans/2026-10-18-002-production-single-node-plan.md)
and the GPU half of gap 4 of [`PRODUCTION_READINESS_202610.md`](PRODUCTION_READINESS_202610.md).

Nothing on this page has run on a GPU. The build of step 0 was compiled on 2026-10-18 without a
run ([`log/2026-10-18-cuda-build-only.md`](log/2026-10-18-cuda-build-only.md)); every statement
about a device result is ⚠ until the step that answers it has run.

## 0. Before the first command

**The build.** Both commands compile only; neither touches the device.

```bash
git submodule update --init WW3 && just ww3-base export && just submodule-init
nix develop ./picasso/labs/pratico#cuda --command cmake -S kokkos --preset cuda-release
nix develop ./picasso/labs/pratico#cuda --command cmake --build kokkos/build/cuda-release -j8
scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --kokkos resident --build-only
```

Do not use `just kokkos-cuda-test` for this step: its last line runs `ctest` with CUDA as the
default execution space, which is a device run. On 2026-10-18 both builds passed with 51 nvcc
warnings, none in a part the fork calls, and 8 gfortran warnings in WW3's own files (v, the log
entry above). A new warning in the build of the run's commit is read before step 1.

**Write down, once, in the log entry of the run:**

| What | Command |
|---|---|
| ww3-gpu commit, and whether the tree is clean | `git rev-parse --short HEAD`, `git status --porcelain` |
| `WW3/` pin (fork), its NOAA base, `picasso/` pin | `git submodule status WW3 picasso`; `just ww3-base rev` |
| CPU model and hardware threads | `grep -m1 'model name' /proc/cpuinfo`, `nproc --all` |
| GPU model and driver version | `nvidia-smi --query-gpu=name,driver_version --format=csv,noheader` |
| the CUDA version the driver supports | the header of `nvidia-smi` |
| nvcc, Kokkos, the host compilers | `nvcc --version`, the `kokkos-cuda-<version>` store path in `kokkos/build/cuda-release/CMakeCache.txt`, `gfortran --version`, and the "CXX compiler identification" line of the configure (nvcc_wrapper's host g++, 14.4.0 on 2026-10-18, while the `#ww3` shell's is 16.2.0) |
| whether the GPU drives a display | `nvidia-smi --query-gpu=display_active --format=csv,noheader` |
| the machine's load before and after | `cut -d' ' -f1 /proc/loadavg` |

A GPU that drives a display has a watchdog that ends a long kernel with
`cudaErrorLaunchTimeout`; `WW_KOKKOS_SRCE_BATCH_MIB` (default 128 MiB on a device) keeps a
`W3SRCE` launch short for that reason, and its limit was never measured
(`kokkos/src/fortran_iface/PATCH.md`, the `WW_KOKKOS_SRCE_BATCH_MIB` paragraph). Record the value
used when it is set. A launch timeout is a stop, not a reason to change the case.

**Stop rules for every step.** A step that fails stops the checklist. The failure is recorded as
it is, with its output, in the log entry. A tolerance, a tolerance file, a comparison script and
the way it is called stay as they are; a change to any of them is its own pull request reviewed
by a person (AGENTS.md, "Never cheat a regtest tool").

## 1. The device math: `scripts/libm_bits.sh --device`

```bash
scripts/libm_bits.sh --device
```

It runs `device_check` on GPU 0: every transcribed function (`expf`, `logf`, `sincosf`, `powf`,
the libmvec copies, `tanhf`, `log10f`, `atan2f`, `coshf`, `sinhf`, `exp`, `tanh`), every input
of each against the host's glibc, with CUDA's own functions printed beside them. Exit
0: no difference; 1: a difference; 2: a CUDA error; 7: not confirmed.

Record:

- the full output, per function: inputs checked, inputs different, the first differing input;
- the exit code;
- in [`GPU_LIBM_BITS_202610.md`](GPU_LIBM_BITS_202610.md), a dated "Results on the device"
  section beside "Results on the host", and the "What is not shown" line about the device updated
  only by what the run showed.

A difference here does not stop steps 2 to 4 (ADR-0014 already judges them by tolerance), but it
says which `⚠` of the libm page stays.

## 2. The smoke case on the GPU

```bash
just smoke-ww3-gpu gpu
scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --kokkos resident
```

The first is `--kokkos gpu` (`SRCE` alone, ADR-0013), the default of `--gpu`; the second is the
resident preset (`SRCE,XYP3,KTP3,OUTG,IOPE,UPDV`). Both run `ww3_ts1` twice in one executable
(the Fortran, then Kokkos), then `--ww3-noaa`, pure NOAA WW3 at the fork's base on the CPU.

Record, for each of the two commands:

- the `nccmp-tol` table against the Fortran of the same executable, and the one against `--ww3-noaa`,
  every field, listed or not: `max|d|`, rms, largest relative difference, and the verdict of the
  judged ones (`hs` on the smoke: "1 judged, 25 unlisted", ADR-0014, "Consequences");
- which model files `cmp` found different, against each reference (on CUDA the script reports
  them and goes on);
- the parts line the Kokkos run printed, which shows the parts the preset asked for ran on
  Kokkos;
- the exit code.

The tables go into `docs/data/ww3gpu_cuda_smoke_<YYYY-MM-DD>.md`, the command and the verdict
into the log entry. A failed `nccmp-tol` stops the checklist.

## 3. The full case on the GPU

```bash
just full-ww3-gpu gpu
scripts/ww3-gpu.sh --ww3-gpu full --gpu --kokkos resident
```

`bench_small`: 120 x 80, 9 520 sea points, NK = NTH = 24, 12 hours. Record the same items as in
step 2, in `docs/data/ww3gpu_cuda_full_<YYYY-MM-DD>.md`; the full case judges `hs` of five
listed fields (ADR-0014). The wall times these runs print are one run each: a check that the
pieces work, never a number for a table.

**The timing, only after step 3 passed on the same commit:**

```bash
just perf-ww3-gpu full 3 resident
```

Three runs of each, alternated, every output compared with the first run's, and the medians; the
script writes `perf.tsv` only after its own `nccmp-tol` comparisons passed. On a quiet machine
(load average recorded before and after). Then `python3 scripts/results.py record <perf.tsv>`
writes the `bench/results/` record, whose `regtest` says "GPU: judged by tolerance, not bits"
(ADR-0014, point 4), and `just results check`. A figure quoted anywhere names the machine, the
command and the median of three, and says "regtest pending ⚠" until the matrix of step 4 has
passed on its commit (ADR-0016).

## 4. WW3's regtest matrix on the GPU: `just ww3-gpu-matrix-cuda`

On the entries the CPU regtests of this pull request passed:

> **Entries: filled by U3 of this PR.** The integrator writes here the CTest names
> (`NNN:<test>:<switch>`) of the entries that passed on the CPU with the resident preset and the
> lab switch, and the record they came from in `docs/data/`.

```bash
just ww3-gpu-matrix-cuda 2 serial -R '<the entries above, as one CTest regex>'
```

Two things about the recipe as it is on 2026-10-18 (v, `justfile`, the `ww3-gpu-matrix-cuda`
recipe, and `regtests/CMakeLists.txt`):

- It forwards `WW3GPU_ROUTINES` only. `WW3GPU_KOKKOS=resident` and `WW3GPU_LAB_SWITCH=ON`, which
  the CPU entries of U3 used, are not passed to its CMake configure, so the recipe runs each
  entry's own `matrix.base` switch with the routines of `WW3GPU_ROUTINES` (`SNL1` by default),
  not the resident preset. Running the U3 entries as U3 ran them needs those two variables
  forwarded, a change to how a regtest driver is called and so reviewed by a person first.
  Until then, record which configuration ran.
- `WW3GPU_STRICT` defaults to ON, so an entry fails when `test.comp` finds a file different. On
  CUDA that verdict is expected and is not the ADR-0014 verdict: read the `nccmp_tol` and
  `judged` columns of `summary.tsv` for that, and record both.

Record:

- `build/ww3-gpu-matrix-cuda-<selection>[...]/summary.tsv`, copied to
  `docs/data/ww3gpu_matrix_cuda_<selection>_<YYYY-MM-DD>.tsv` with its header lines (date,
  commits, machine, command, switch, Kokkos flags);
- per entry: `test_comp`, `nccmp_tol`, `judged`, `kokkos_spectra` (an entry with 0 did not
  exercise the port and is reported as such), `ww3_self`;
- the log entry: which entries passed `nccmp-tol`, which differ and in which field.

## After the run

- One dated entry in `docs/log/` with every record above, indexed in
  [`log/README.md`](log/README.md), and linked from the pull request that commits it.
- [`PRODUCTION_READINESS_202610.md`](PRODUCTION_READINESS_202610.md), gap 4: the "GPU" paragraph
  updated by what was measured.
- ADR-0014's "Revisit when" names the first supervised run: the owner reads its `nccmp-tol`
  tables, every field, and decides whether the ADR stands.
- `kokkos/ports.tsv` changes only where the record supports it, then
  `python3 scripts/port_score.py --write` and `--check`.
