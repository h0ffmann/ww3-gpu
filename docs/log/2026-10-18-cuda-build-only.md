# The CUDA build of `ww_kokkos` and of the fork for the resident preset, build only: 51 nvcc warnings, none in a wired part

2026-10-18, an agent for M. Hoffmann; unit U5 of
[`2026-10-18-002-production-single-node-plan.md`](../compound/plans/2026-10-18-002-production-single-node-plan.md),
[ADR-0014](../ADRs/ADR-0014-gpu-runs-judged-by-tolerance.md). No device run.

## Question

Do `ww_kokkos` and the `h0ffmann/WW3` fork build on Kokkos' CUDA backend for the resident preset
(`--kokkos resident`, `SRCE,XYP3,KTP3,OUTG,IOPE,UPDV`), and does any warning of nvcc, the Kokkos
wrapper or gfortran touch the bits: contraction, a conversion, a math call, a host function
called from device code? The answer is the precondition of the owner's supervised run,
[`GPU_SUPERVISED_RUN_CHECKLIST_202610.md`](../GPU_SUPERVISED_RUN_CHECKLIST_202610.md).

## What was done

On branch `goal/abbb0456-6043-u5-gpu-ready` at `4f87fec` (from
`goal/abbb0456-6043-production-single-node`), `WW3/` at `80644ba9` (`ww3-gpu/kokkos-pnt`),
`WW3-noaa/` at `03adda91`, `picasso/` at `be76132`. Machine: Intel Core i9-14900, 32 threads,
shared with three other agents (at most 8 build jobs); NVIDIA GeForce RTX 4090, driver 615.71.09
(`nvidia-smi --query-gpu=name,driver_version`, a driver query, no kernel launched).

Toolchain, all from the `#cuda` shell of `picasso/labs/pratico` (v, the configure log): nvcc
12.9.86 (`cuda_12.9.r12.9/compiler.36037853_0`), Kokkos 5.2.2 (`kokkos-cuda-5.2.2`, devices
`OPENMP;SERIAL;CUDA`, `-arch=sm_89`), nvcc_wrapper's host compiler g++ 14.4.0, gfortran and gcc
16.2.0, CMake 4.4.3, glibc 2.44, `WW_KOKKOS_LIBM=ON`.

```bash
nix develop ./picasso/labs/pratico#cuda --command cmake -S kokkos --preset cuda-release
nix develop ./picasso/labs/pratico#cuda --command cmake --build kokkos/build/cuda-release -j8 -- -v
JOBS=8 scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --kokkos resident --build-only
```

`just kokkos-cuda-test` was not used: its last line runs `ctest` with CUDA as the default
execution space, a device run. `just kokkos-test cuda-release` was not used either: it builds in
the `#ww3` shell and also runs `ctest`. The `--build-only` command rebuilt `cuda-release` (the
script's toolchain stamp differs from a hand configure) and built WW3 with the switch
`switch_lab_ompg` plus `KOKKOS`, `-DCMAKE_CXX_COMPILER=nvcc_wrapper`, Fortran `-O3`. The logs
are in the worktree's `build/u5/` and `build/ww3-gpu-cuda/{configure,build}.log` (git-ignored).

## Result

Both builds passed (v): the 73 steps of `ww_kokkos` with its tools and intro programs, and the 21
WW3 programs (`ww3_shel`, `ww3_multi`, ...) in `build/ww3-gpu-cuda/ww3/bin`. The second build of
`cuda-release` printed the same 53 lines matching `warning` as the first (v, `grep -c`).

**Contraction and math flags** (v, the `-v` build log and `nvcc_wrapper -dryrun` on a test file):
every one of the 25 translation units of `libww_kokkos.a` is compiled with `-ffp-contract=off
--fmad=false`; nvcc_wrapper passes `-ffp-contract=off` to the host g++, ptxas gets `--fmad
false`, and cicc `-ftz=0 -prec_div=1 -prec_sqrt=1 -fmad=0`. No `--use_fast_math` anywhere.

**nvcc warnings, 51 in the Kokkos code, grouped:**

| File:line | Warning | Count | Can it touch the bits |
|---|---|---|---|
| `kokkos/src/ww_kokkos/pdlib_do_output_exchanges.hpp`:151, 153, 156, 159, 217-220, 247, 248, 265-267, 271, 272, 291 | `#20013-D` calling a `constexpr __host__` function (`operator()`) from a `__host__ __device__` function (`walk_columns`, or the packing lambda of `do_output_exchanges<Kokkos::Cuda>`); "the experimental flag `--expt-relaxed-constexpr` can be used to allow this" | 48 (16 sites, 3 each) | The lambdas `one`, `e3`, `swell`, `cols` of `walk_columns` are host-only to nvcc, and the packing kernel calls them on the device. It compiled; whether the device code calls them correctly is ⚠ (never run). No floating-point arithmetic in this routine (`kokkos/ports.tsv`, row `pdlib_field_vec:do_output_exchanges`), so a defect would misplace values, not round them. The routine is translated only: header-only, no shim, no `W3WAVE` hunk (v, `ports.tsv`), instantiated only by `wave_support.cpp:305` for the compile check. Not in the resident preset and not called by the fork. |
| `kokkos/src/ww_kokkos/uost_uost_sourceterm_compute.hpp`:54, 56 | `#1675-D` unrecognized GCC pragma (`#pragma GCC novector`) | 2 | nvcc's front end does not know the pragma, and passes it on: on a test file the host g++ 14.4.0 under nvcc_wrapper reports "splitting region at dont-vectorize loop" (v, `-Xcompiler -fopt-info-vec-all`). The pragma keeps the Fortran's operand order of `S_LD + S_SE` when both are NaN (the header's comment, lines 14-16); it is off in device code (`!defined(__CUDA_ARCH__)`). `UOST_SOURCETERM` is translated only, no shim (v, `ports.tsv`). |
| `kokkos/src/ww_kokkos/wavset_fd_compute_lh_stress.hpp`:45 | `#550-D` variable `syy_x` was set but never used | 1 | No: a dead store in a translation of `W3WAVSET`, translated only (v, `ports.tsv`). Not changed: translate, don't improve. |

No warning about a double-to-float conversion, an unsupported math function, a host function
called from a kernel of a wired part, or contraction, in either build (v, `grep -i warning`).

**CMake, one warning:** Kokkos' package "does not support CXX extensions. Forcing
`-DCMAKE_CXX_EXTENSIONS=Off`" at WW3's `find_package(ww_kokkos)`. No effect on the arithmetic ⚠
(it selects `-std=c++20` over `-std=gnu++20`).

**gfortran, 8 warnings, all in WW3's own files:** "Legacy Extension: Missing comma in FORMAT
string" at `w3dispmd.F90:913`, `w3iorsmd.F90:490`, `w3iogomd.F90:316`, `w3iogomd.F90:2734`,
`w3iopomd.F90:2415`, `ww3_gspl.F90:3029`, `ww3_ounp.F90:1344`, `ww3_prnc.F90:2430`. A FORMAT
string formats text output; it does not touch the model's arithmetic. None in the fork's
`w3kokkos*md.F90` modules (v, `grep`).

**One thing that is not a warning and may touch the bits:** the CUDA build's host C++ is
compiled by g++ 14.4.0 (nvcc_wrapper's host compiler), while the CPU build and every static
parity argument of `kokkos/ports.tsv` use g++ 16.2.0. A host branch of a kernel in the CUDA
build (a fallback that runs on the host, or the `Kokkos::Serial` path a shim takes) was never
compared with the Fortran under g++ 14 ⚠. On the device the question is the libm one of
[`GPU_LIBM_BITS_202610.md`](../GPU_LIBM_BITS_202610.md).

Nothing ran on the GPU: no `ctest`, no `ww3_shel`, no `libm_bits --device`.

## What it changes

- The checklist [`GPU_SUPERVISED_RUN_CHECKLIST_202610.md`](../GPU_SUPERVISED_RUN_CHECKLIST_202610.md)
  starts from this build.
- `just ww3-gpu-matrix-cuda` forwards `WW3GPU_ROUTINES` only, not `WW3GPU_KOKKOS` or
  `WW3GPU_LAB_SWITCH`, so it cannot run the resident-preset entries of the CPU matrix as they ran
  (v, `justfile`). Left for the integrator and a person's review: it changes how a regtest
  driver is called.
- The `#20013-D` sites matter once `DO_OUTPUT_EXCHANGES` is wired on a device; the wiring unit
  reads them first. Not fixed here.
