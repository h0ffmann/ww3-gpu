# GPU sandbox

Aimed at your RTX 4090. **Read
[`../course/09-benchmark-profile-compile-run.md`](../course/09-benchmark-profile-compile-run.md) first**: it
explains why these are toy kernels rather than a WW3 port, and what the realistic
expectation is.

## Setup

Install the [NVIDIA HPC SDK](https://developer.nvidia.com/hpc-sdk) (free) and put its
compilers on `PATH`:

```bash
export PATH=/opt/nvidia/hpc_sdk/Linux_x86_64/<version>/compilers/bin:$PATH
nvaccelinfo        # confirms driver + device, prints your compute capability
```

You want it to report **compute capability 8.9** for a 4090.

```bash
make info      # what you actually have
make           # build everything for cc89
make run       # build and run, with kernel-launch tracing on
make cpu       # same directives, CPU threads -- the correctness reference
```

## The programs

| | What it teaches |
|---|---|
| `00_hello_acc.f90` | Does the toolchain work at all. `!$acc parallel loop`, `!$acc data`, a reduction. If `NVCOMPILER_ACC_NOTIFY=1` prints nothing, you built a CPU binary. |
| `01_dispersion.f90` | A kernel that's actually WW3-shaped: Newton-solve the dispersion relation at every (point, frequency). Embarrassingly parallel, tiny per-thread state, arithmetic-heavy. **This is what a good GPU kernel looks like**: contrast with `W3SRCEMD`. |
| `02_do_concurrent.f90` | The same thing in standard ISO Fortran with no directives. One source, three targets (`-stdpar=gpu`, `-stdpar=multicore`, plain gfortran). |
| `03_precision.f90` | Measures FP64 vs FP32 on your card. Expect roughly 60× on a 4090 (nominal 1:64). On an A100 it's about 2×. That gap is the whole argument for data-centre cards. |
| `../bench/` | **Where the actual CPU-vs-GPU measurements live**, including a WW3-shaped kernel and a concurrent CPU+GPU split sweep. Come here to learn the tools, go there to get numbers. |
| `build_netcdf_nvfortran.sh` | Rebuild HDF5 + NetCDF with nvfortran so WW3 can be built with it. Necessary because Fortran `.mod` files are compiler-specific. |

## The three flags that matter

```bash
-Minfo=accel                  # which loops offloaded, and why the others didn't
NVCOMPILER_ACC_NOTIFY=1       # print a line per kernel launch at runtime
NVCOMPILER_ACC_TIME=1         # per-kernel timing summary at exit
```

`-Minfo=accel` is the one to internalise. It says things like *"Accelerator restriction:
call to procedure with no acc routine"* or *"Loop not vectorized: data dependency"*.
That is the compiler telling you exactly what to fix. Most OpenACC work is reading this
output and responding to it.

## The correctness trick

`make cpu` builds `01_dispersion` with `-acc=multicore`: **the same directives, running on
CPU threads instead of the GPU.** Run both and compare.

- Numbers agree → your parallelisation is sound.
- Numbers differ → you have a data-movement bug (something not copied in, something not
  copied back, a race), not a physics bug.

This bisects the two failure modes that otherwise look identical, and it costs one
recompile.

## Suggested progression

1. `make info`, `make run`. Confirm kernels actually launch.
2. Run `03_precision`. Write down your FP64 ratio. Now you know your hardware.
3. In `01_dispersion`, move the `!$acc data` region *inside* the loop over frequencies so
   the arrays get copied every iteration. Measure. **That's the PCIe bottleneck that
   limited the published WW3 port**, reproduced in twenty lines. Understanding it viscerally
   is worth more than reading about it.
4. Vary `niter` from 2 to 64. Where does the kernel stop being bandwidth-bound and start
   being compute-bound? That crossover point is the thing to know about any kernel.
5. Rewrite `01_dispersion` in CUDA Fortran (`-cuda`, `attributes(global)`) and compare
   against OpenACC. Usually within 10–20%, which is the argument for directives.
6. Now go read `W3SRCEMD` in the WW3 source and ask yourself, honestly, how you would
   offload it. That question is the real exercise.
