# Two shapes for one rule {#sec:shapes}

|                         | ww3-gpu (C++/Kokkos)                                                  | chaceon (Rust)                                   |
|:---------------------|:---------------------------------------|:-------------------------------------|
| Shape                   | Inside WW3: the Fortran calls C++ through a `bind(C)` shim, one flag for each routine | Replaces WW3: no Fortran in a chaceon process    |
| Reference               | NOAA-EMC/WW3 `03adda91`, through a fork that adds 5 687 lines and removes none | MetOcean WW3 7.14, `92e2cf66`                    |
| CPU parity against      | the Fortran path of the same executable                               | a reference that is compiled separately          |
| Device targets          | Kokkos 5.2.2, CUDA backend (`sm_89`); OpenMP on the CPU               | rust-gpu (SPIR-V, Vulkan) and Rust-CUDA (PTX)    |
| Spectrum resident on the device | yes, on the OpenMP backend                                    | yes, in SPIR-V on CPU Vulkan (lavapipe)          |
| Size of the port        | 62 257 lines of C++, 355 files (2026-10-14)                           | 29 229 lines of Rust port, 5 936 lines of tools  |
| Formal methods          | SMT queries, exhaustive sweeps                                        | one Lean 4 proof about `f32` division            |
| Licence                 | MIT, LGPL-3.0 kernels                                                 | MPL-2.0                                          |

: The two ports. Sources: chaceon `docs/WW3_GPU_PARITY_202611.md` (2026-11-09) and `README.md`
at `0c098fe` [@chaceon]; ww3-gpu `kokkos/PORT_STATUS.md` at `58167d7` [@ww3gpu]. {#tbl:shapes}

The Kokkos route keeps every WW3 program and replaces one routine at a time. Thus one regtest
can isolate the effect of one routine. The Rust route must rebuild the programs, the file formats
and the time loop before it can compare a whole run. After that, no Fortran remains that can
share state with the port.

The CPU results of the whole ww3-gpu runtime, with the resident spectrum, and their limits are
in its NOAA progress record [@noaarecord]. The DIA port is also published alone as
ww3-dia-gpu-v2 [@diagpuv2]. That repository compares the port with an earlier CUDA kernel of the
same routine.
