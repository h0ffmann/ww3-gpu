# Splitting WW3's physics between OpenMP and CUDA on the H100 node

2026-10-16. Asked by the owner during PR #152: once the Fortran is translated, can Kokkos run some
of WW3's physics on the host CPUs with OpenMP and the rest on the H100 with CUDA, and is that one
more reason to translate first, regtest at checkpoints, and tune for each backend last? Issue
[#42](https://github.com/h0ffmann/ww3-gpu/issues/42); the plan it extends is
[`KOKKOS_H100_PLAN_202609.md`](KOKKOS_H100_PLAN_202609.md).

## Short answer

Yes, in one executable, and nothing in Kokkos waits for the last routine: each kernel names the
execution space it runs on. An H100 node can do both, because the OpenMP side runs on the node's
host CPUs and the CUDA side on the GPU. What decides whether a split pays is the link between the
two memories and this repository's bit-for-bit rule, and neither can be judged until a whole time
step is in C++ and regtested. So the split is a tuning step after translation and checkpoints, not
a design choice to make now.

## What Kokkos allows

- (v) One build may enable "one device backend (e.g., CUDA, HIP, SYCL) and one host parallel
  backend" (Kokkos configuration guide, kokkos.org, read 2026-10-16). The pinned toolchain's CUDA
  Kokkos is built with Serial, OpenMP and CUDA together (`picasso/labs/pratico/flake.nix:97-99` at
  picasso `be76132`).
- (v) A kernel runs where its policy says: `Kokkos::RangePolicy<Kokkos::OpenMP>` on the host,
  `Kokkos::RangePolicy<Kokkos::Cuda>` on the GPU. These are template arguments in C++, not
  compiler directives. The port already picks a backend per call: the DIA runs on `Kokkos::Serial`
  inside a nested OpenMP region (`kokkos/src/ww_kokkos/snl1_config.hpp:20-23`).
- (v) That same line shows the obstacle: in a CUDA build the Views live in `CudaSpace`, which a
  host backend cannot read. Every table of the port today is allocated in
  `Kokkos::DefaultExecutionSpace::memory_space` (`snl1_config.hpp:31`, `st4_arrays.hpp:37`,
  `snl3_tables.hpp:34`), which is `CudaSpace` in a CUDA build. A split needs the kernels templated
  on their memory space, and that is a change to every shim, not to the translated arithmetic.
- (v) Running both at once is possible with execution-space instances; `partition_space` gives
  each CUDA instance its own stream, while on host backends it "may return copies of the original
  instance", so OpenMP work does not overlap with itself that way (Kokkos API,
  `partition_space`, read 2026-10-16). It is still in `Kokkos::Experimental`.

## Can the H100 do both

The H100 is the GPU; OpenMP runs on the CPUs of the node it sits in. Which H100 the lab has is
not recorded in this repository ⚠: the variant (PCIe or SXM) and the host CPU should be written
into this file when known, because they set the numbers below. From NVIDIA's H100 datasheet (a
copy hosted by Cisco, not the primary page ⚠, read 2026-10-16):

| | H100 PCIe | H100 SXM |
|---|---|---|
| GPU memory bandwidth | about 2 TB/s | about 3.35 TB/s |
| FP32 (no tensor cores) | 51 TFLOPS | 67 TFLOPS |
| Link to the host CPUs | PCIe Gen5, 128 GB/s (both directions together) | PCIe Gen5, 128 GB/s |

A GH200 (Grace Hopper) is a different product: the Grace CPU and the Hopper GPU share a coherent
memory over NVLink-C2C at 900 GB/s, which NVIDIA calls 7 times PCIe Gen5 (NVIDIA GH200 page, read
2026-10-16). On a GH200 a CPU/GPU split costs far less; on an H100 card in an x86 server every byte
that changes side crosses PCIe.

## What a split by physics costs

Splitting by physics inside a time step (say source terms on the GPU, propagation on the CPU)
moves the whole spectrum across the link twice per step: NSEA × NSPEC × 4 bytes each way.
Illustration only, not a measurement ⚠: with NSPEC = 32 × 36 = 1 152 (the operational grid's
`NK` and `NTH` as recorded in [`W3SDS4_TRITON_202610.md`](W3SDS4_TRITON_202610.md):34) and an
assumed NSEA = 100 000, one copy is 461 MB. At an assumed 64 GB/s in one direction that is about
7 ms per copy, 14 ms per step, while reading the same array once from the H100's memory takes
0.14 to 0.23 ms. The two copies of one step cost as much as sixty to a hundred
passes over the spectrum on the GPU. The plan's record of comparable ports says the same from measurement: FESOM2's first GPU
run was 3.8 times slower than its CPU node until its halo data stayed on the device
([`KOKKOS_H100_PLAN_202609.md`](KOKKOS_H100_PLAN_202609.md):201).

Splitting by sea points costs less: part of the grid runs on the GPU and part on the CPUs at the
same time, each keeping its own spectrum, exchanging only at the boundary, as WW3 already does
between MPI ranks. Balancing the two sides is then the work.

## What the bit-for-bit rule allows

The rule is the Fortran's bits, inside the same WW3 executable (`AGENTS.md`, "Bit for bit with
the Fortran"). It already marks the GPU as ⚠: CUDA's `expf` and `logf` are not glibc's. This batch
adds routines that are bit for bit on the CPU only because they reproduce gfortran's use of
glibc's libmvec, which no GPU has: five ST4 loops, the SCALE loop of `W3SNL3` and the four
logarithms of `INSNL3` (`kokkos/port_report/W3SNL3_KOKKOS.md`, `INSNL3_KOKKOS.md`). Under a split,
which points or terms land on the GPU would change the output's bits. Whether the GPU side is held
to a tolerance (an L2 verdict) instead is the owner's decision; the rule says such a port stops and
asks.

## Is this evidence for translate, then regtest, then tune

Mostly yes, with one correction.

- **Yes: the split needs the whole step in C++.** While a Fortran routine sits between two ported
  kernels, the data goes back to the host for it on every call (phase 1 copies per call). The
  device-resident step that every successful port needed
  ([`KOKKOS_H100_PLAN_202609.md`](KOKKOS_H100_PLAN_202609.md):214) and any choice of where each
  kernel runs both wait for the last routine of the step.
- **Yes: the choice needs measured times, and a time needs parity.** Which kernel belongs on which
  backend is decided by its time on each, and no time enters a table without a passed regtest
  replay on the same commit (`AGENTS.md`, "No timing without parity").
- **Yes: translation is backend-neutral.** A Kokkos kernel compiles for OpenMP and CUDA from one
  source, so translating does not commit to a placement. The placement is the execution space,
  the memory space, the layout and the team sizes, which the tuning step sets per kernel.
- **The correction: regtests do not wait for the whole port.** ADR-0011 rejected "port in units
  and run no regtest until the whole port ends" because defects pile up and a failure maps to
  thousands of lines ([ADR-0011](ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md)). The
  order is translate a unit, regtest it at its checkpoint, and tune once the step is in C++ and
  regtested. Tuning that changes a reduction's order or the arithmetic is a physics change: its
  own PR, asked about first, with L2 evidence.

## Next, when the step is in C++ and regtested

1. Record the lab's H100 variant and host CPU here.
2. Template the shims on the memory space, keeping today's single-space path as the default.
3. Time every kernel on OpenMP and on CUDA (`--ww3-gpu --gpu` only with the owner supervising).
4. Try the split by sea points first; a split by physics only for a kernel whose GPU gain exceeds
   two spectrum copies per step.

Sources read 2026-10-16:
[Kokkos configuration guide](https://kokkos.org/kokkos-core-wiki/get-started/configuration-guide.html),
[Kokkos `partition_space`](https://kokkos.org/kokkos-core-wiki/API/core/spaces/partition_space.html),
[NVIDIA H100 datasheet (Cisco copy)](https://bxin.cisco.com/c/dam/en/us/products/collateral/servers-unified-computing/ucs-c-series-rack-servers/nvidia-h100-80-gpu.pdf),
[NVIDIA GH200 Grace Hopper Superchip](https://www.nvidia.com/en-us/data-center/grace-hopper-superchip).
