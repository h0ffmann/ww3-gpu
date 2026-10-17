# WFIP-0006: The device computes glibc's libm bits: routing the port's per-point math through `ww::libm`

| | |
|---|---|
| **Status** | Partially implemented (units 1–4 of 5, #178, on `main` with #179; 2026-10-17) |
| **Author** | M. Hoffmann (asked 2026-10-17 in the project thread: "move the compute to GPU without changing the answer") |
| **Created** | 2026-10-17 |
| **Deliverable** | D5 |
| **Related** | issue #42 (the port planner); [`GPU_LIBM_BITS_202610.md`](../GPU_LIBM_BITS_202610.md) (the measurements this design rests on); `kokkos/tools/libm_bits/` (the transcriptions); [ADR-0014](../ADRs/ADR-0014-gpu-runs-judged-by-tolerance.md) (its "revisit when"); [ADR-0010](../ADRs/ADR-0010-kokkos-validated-by-ww3-regtests-only.md); [ADR-0011](../ADRs/ADR-0011-port-in-units-regtest-at-checkpoints.md); [WFIP-0005](WFIP-0005-device-resident-step.md) |
| **Effort** | M: one routing header in `kokkos/src`, one build step, the call sites of twelve kernel files re-pointed; no physics changes |
| **Gain** | `proposal` (D5: a GPU result that is the Fortran's, so its timing needs no tolerance caveat); `science` (whether a whole WW3 time step on a GPU can be bit for bit with glibc's x86 libm) |
| **Effort vs Gain** | do when WFIP-0005's units have merged: the call sites live in the files that work is changing, and one rebase is cheaper than two |
| **Depends on** | the transcriptions of `kokkos/tools/libm_bits` merged (the PR that adds `GPU_LIBM_BITS_202610.md`); the owner at the terminal for `scripts/libm_bits.sh --device` and every `--gpu` run |
| **Blocked by** | none (WFIP-0005 must merge first, as **Depends on** and **Effort vs Gain** say; the index's dependency graph does not yet pass `figures.py check`) |
| **Risk** | the supervised `--device` run shows a difference the host checks cannot, such as a device conversion or a denormal path, and the device cannot be bit for bit without a deeper rewrite |

### Readiness

| | |
|---|---|
| **Manually reviewed** | no |
| **Written by** | M. Hoffmann, with an agent |
| **Tasks** | none needed: five units in §5, each one PR-sized; a tasks file once Accepted |
| **Tests** | none of the port's own (ADR-0010): `scripts/libm_bits.sh` checks the math library against glibc; `just smoke-ww3-gpu gpu`, `just full-ww3-gpu gpu` and `just ww3-gpu-matrix-cuda` judge the port |
| **Spec-kit** | none |
| **Issues** | not filed: Draft |

## 1. Summary

The CUDA build of the port calls NVIDIA's libdevice for `expf`, `logf`, `powf`, `sin`/`cos`,
`tanhf`, `atan2f`, `log10f`, `exp` and `tanh`, and scalar functions where the Fortran's object
calls libmvec. That is why [ADR-0014](../ADRs/ADR-0014-gpu-runs-judged-by-tolerance.md) judges a GPU
run by tolerance. `kokkos/tools/libm_bits` now has host-and-device transcriptions of glibc 2.44's
functions that equal glibc on the host on every input checked. This WFIP routes every device call
of the `W3SRCE` unit through them, keeps the CPU backend calling glibc itself, and ends with the
supervised runs that decide whether ADR-0014's verdict by tolerance can be revisited.

## 2. Motivation

- ADR-0014 lists the reasons a GPU run cannot be bit for bit: "On CUDA, `logf`, `tanhf`,
  `sincosf`, `expf` and `powf` come from NVIDIA's libdevice" and ST4's five libmvec loops; it is
  to be revisited when "a device math library with glibc's bits [...] exists" (v, the ADR,
  read 2026-10-17).
- The device path calls libdevice today: `srce_kernel.hpp:57` (`KOKKOS_IF_ON_DEVICE((c =
  Kokkos::cos(x); s = Kokkos::sin(x);))`), `srce_ln1.hpp:53` likewise, and `st4_libmvec.hpp:43,58`
  fall back to `Kokkos::pow`/`Kokkos::exp` on a device (v, read 2026-10-17).
- The host build already has the Fortran's libm set: `nm -u` of
  `kokkos/build/openmp-release/.../srce_shim.cpp.o` lists `atan2f exp expf log10f logf powf
  sincosf tanh tanhf _ZGVbN4v_expf _ZGVbN4vv_powf` and no `sinf` or `cosf` (v, 2026-10-17). The
  `cuda-release` object of the same file lists none of them: its device code is in the fatbin,
  linked to libdevice (v, same command).
- The transcriptions equal glibc on the host: 0 differences on every input of `expf`, `logf`,
  `sincosf`, `tanhf`, `log10f`, libmvec's `expf`, and on 10^10-scale samples of `powf`,
  `atan2f` and `exp` (v, `GPU_LIBM_BITS_202610.md`, `scripts/libm_bits.sh all`).

## 3. What changes for the reader

| | Before | After |
|---|---|---|
| `--ww3-gpu --gpu` verdict | `nccmp-tol` with `tolerances.txt` (ADR-0014) | `test.comp` 0 files different, as on the CPU, if §7 passes |
| `bench/results/` CUDA record, `regtest` | "GPU: judged by tolerance, not bits" | "GPU: bit for bit (test.comp)" |
| Device libm | libdevice | `ww::libm`, glibc 2.44's algorithms and the host's tables |
| CPU backend | glibc | glibc, unchanged (§6) |

## 4. Sources and dependencies reviewed

### Device call sites (v, `grep` of `kokkos/src/ww_kokkos` at the worktree, equal to origin/main in `kokkos/src`, 2026-10-17)

The table is the first survey. Re-read at goal/42-gpu-step 4fbffbe on 2026-10-17, the kernels
`WW_KOKKOS_ALL` runs also call `exp` in `updt_utrn.hpp` (W3UTRN), `atan2` and `cos`+`sin` in
`updt_uwnd.hpp` (W3UWND), and `tanh`, `cosh`, `sinh` in `disp_distab.hpp` (DISTAB); the line
numbers below have moved. The implementation routes all of them (§5).

| File | Calls (`Kokkos::` unless noted) | Replacement |
|---|---|---|
| `srce_kernel.hpp` | `cos`+`sin` of one argument `:57,59-60`; `log` `:287,370`; `tanh` `:364,375` | `sincosf`, `logf`, `tanhf` |
| `st4_sds4.hpp` | `pow` `:131,290,351,365,379,390,471`; `atan2` `:144`; `tanh` `:197,283`; `log` `:270`; `cos`+`sin` `:364,448`; `exp` `:502` | `powf`, `atan2f`, `tanhf`, `logf`, `sincosf`, `expf` |
| `st4_sin4.hpp` | `pow` `:94,167,197,253`; `tanh` `:101,317`; `log10` `:128`; `cos`+`sin` `:156-157,170-171,359-360`; `atan2` `:169,254`; `log` `:179,286,302,310,312`; `exp` `:196,291,301,315` | `powf`, `tanhf`, `log10f`, `sincosf`, `atan2f`, `logf`, `expf` |
| `st4_spr4.hpp` | `pow` `:72,125,128,133`; `atan2` `:84` | `powf`, `atan2f` |
| `st4_calc_ustar.hpp` | `tanh` `:63`; `pow` `:85`; `log` `:88`; `exp` `:103,108` | `tanhf`, `powf`, `logf`, `expf` |
| `st4_libmvec.hpp` | device fallback `pow` `:43`, `exp` `:58` | `mvec_powf`, `mvec_expf` per lane |
| `srce_ln1.hpp` | `cos`+`sin` `:53,55-56`; `exp` `:86` | `sincosf`, `expf` |
| `srce_db1.hpp` | `tanh` (double) `:81`; `exp` (double) `:112,115` | `tanh` (double, §8), `exp` (double) |
| `snl1_team.hpp` | `exp` `:164` | `expf` |
| `prop_xyp3.hpp`, `prop_ktp3.hpp`, `prop_qck1..3.hpp` | none | none |

The set matches the Fortran's per-point entry points (`GPU_LIBM_BITS_202610.md`, first table).

**The sincos trap.** gfortran `-O3` turns a `COS` and a `SIN` of one argument into one `sincosf`
call (v, `objdump -dr` of `w3src4md.F90.o`, `w3srcemd.F90.o`, `w3sln1md.F90.o`: `sincosf`, no
`sinf`/`cosf` per point). glibc's `sincosf` and its `sinf`/`cosf` share the polynomial, but the
device must not call two separate functions where the Fortran called one unless they are bit-equal:
every `cos`+`sin` pair above becomes one `ww::libm::sincosf<Fma>` call. A lone `sin` or `cos` of an
argument whose partner is not computed does not occur on this path (v, the table) ⚠ (re-check
after WFIP-0005 merges).

### The transcriptions

`kokkos/tools/libm_bits/glibc_mathf.hpp` (ARM optimized-routines, MIT OR Apache-2.0),
`coremath_f.hpp` (CORE-MATH, MIT), and the generated `svml_tables.hpp` (glibc's libmvec tables
and the host's `rcpps`, not committed). Checks and counts: `GPU_LIBM_BITS_202610.md`. Pick: these,
because no other device library claims glibc's bits.

## 5. Design

**Unit 1, the header in the port.** Move `glibc_mathf.hpp` and `coremath_f.hpp` to
`kokkos/src/ww_kokkos/libm/` (the port includes from there; the tool keeps including the same
files by path). `KOKKOS_INLINE_FUNCTION` replaces `WW_LIBM_FN` where Kokkos is included.

**Unit 2, tables at configure time.** `kokkos/CMakeLists.txt` builds `gen_tables` as a host
executable, finds `__svml_spow_data` in the `libmvec.so.1` of the toolchain that builds WW3 (the
glibc next to `libc.so.6`, as `scripts/libm_bits.sh` does), runs it and adds the generated header
to `ww_kokkos`'s include path. A configure that cannot read the symbol stops; `-DWW_KOKKOS_LIBM=OFF`
keeps libdevice and the ADR-0014 verdict.

**Unit 3, the device calls.** Each call site of the table becomes a `ww::libm` call inside
`KOKKOS_IF_ON_DEVICE`, and the host branch keeps calling glibc:

```cpp
// st4_sds4.hpp:364, after
Real cd, sd;
KOKKOS_IF_ON_HOST((::sincosf(dirforcum, &sd, &cd);))
KOKKOS_IF_ON_DEVICE((ww::libm::sincosf_rt(dirforcum, &sd, &cd, p.libm_fma);))
```

`st4_libmvec.hpp`'s device loop calls `mvec_powf`/`mvec_expf` for the lanes the host sends to
libmvec and `powf`/`expf` for the remainder, so the grouping from the first element is the same.

**Unit 4, the run-time `Fma` choice.** glibc picks its fma build when the CPU has FMA and AVX2
(`ifunc-fma.h`). The shim's init reads `__builtin_cpu_supports("fma") &&
__builtin_cpu_supports("avx2")` once on the host and stores it in each kernel's parameter struct;
`*_rt` wrappers branch on it (one uniform branch per call, the same for every thread). The
`libm_bits probe` answer is printed in `log.ww3` beside the Kokkos parts line.

**Unit 5, the check of the routing.** A `grep` in `scripts/ww3-gpu.sh --self-test` fails if any
`Kokkos::(exp|log|log10|pow|tanh|atan2|sin|cos)` remains in the files of §4 outside
`KOKKOS_IF_ON_HOST`.

What runs where: units 1 to 5 build and run on the CPU; only §7's device boxes need the GPU.

### As built (2026-10-17, branch goal/42-gpu-libm-route, #178)

- **Unit 1** without the move: `kokkos/src/ww_kokkos/libm_device.hpp` (`ww::dm`) includes the
  headers from `kokkos/tools/libm_bits`, on the include path of a CUDA build only. Each wrapper's
  host branch is the Kokkos call the kernel made before; its device branch calls `ww::libm`.
  Overloads for the other argument type are deleted, so a promotion is a compile error.
- **Unit 2** as designed, at build time rather than configure time: CMake builds `gen_tables` and
  runs it into `ww_libm_tables.hpp`. `gen_tables` now finds the loaded `libmvec.so.1` and the
  symbol's file offset itself.
- **Unit 4** at build time: `gen_tables` decides glibc's fma or sse2 build by comparing glibc's
  `exp` with both copies, and writes `WW_LIBM_HOST_FMA`. It follows `GLIBC_TUNABLES` (v:
  `glibc.cpu.hwcaps=-AVX2,-FMA` gives sse2). The run host must be the build host, or have the
  same glibc build and `rcpps` ⚠ (not checked at run time).
- **Unit 3** in twelve files: `srce_kernel.hpp`, `srce_ln1.hpp`, `srce_db1.hpp`, `st4_sds4.hpp`,
  `st4_sin4.hpp`, `st4_spr4.hpp`, `st4_calc_ustar.hpp`, `st4_libmvec.hpp`, `snl1_team.hpp`,
  `updt_utrn.hpp`, `updt_uwnd.hpp`, `disp_distab.hpp`. DISTAB needed `coshf` and `sinhf`, now in
  `coremath_f.hpp` (CORE-MATH, correctly rounded; 0 of 2^32 inputs differ from glibc each, v).
- **Unit 5** not done: `scripts/ww3-gpu.sh` was being changed by the goal/42-gpu-step work, which
  merged as #179, so nothing blocks it now (v, `scripts/ww3-gpu.sh` at `68ae5e2` has no libm check).
- **Not routed:** W3OUTG (`iogo_w3outg.hpp`, opt-in, not under `WW_KOKKOS_ALL`) calls libmvec's
  `sinf`/`cosf`, scalar `sinhf`, `coshf`, `logf`, `atan2f` and `powf`; the other switches'
  kernels (ST1, ST2, ST3, ST6 with libmvec `tanhf`, FLX1-5, IC4, ...) are not on the lab
  switch's path. Each still calls libdevice on a device.

## 6. Parity and physics impact

- **CPU backend:** none. The host branches still call glibc, so the objects keep the libm set of §2
  and the CPU verdict stays `test.comp` against the Fortran; checked by `nm -u` before and after
  and by `just smoke-ww3-gpu` and `just full-ww3-gpu` byte-identical.
- **GPU backend:** the change is the point. The verdict moves from `nccmp-tol` to `test.comp` only
  after §7's device boxes pass; until then ADR-0014 stands. `tolerances.txt` is not touched.

## 7. Verification plan and definition of done

- [ ] The headers build in the port on all three presets: `just kokkos-test serial-debug`, `just kokkos-test openmp-release`, `bash scripts/ww3-gpu.sh --ww3-gpu smoke --gpu --build-only`
- [ ] The CPU backend's libm set and bits are unchanged: `nm -u` of `srce_shim.cpp.o` equal before and after, `just smoke-ww3-gpu` and `just full-ww3-gpu` byte-identical
- [ ] No libdevice transcendental remains on the device path: `bash scripts/ww3-gpu.sh --self-test`
- [ ] Every transcription equals the host's glibc on the device, every input of the unary functions: `scripts/libm_bits.sh --device` (the owner at the terminal)
- [ ] `ww3_ts1` on the GPU is byte-identical to the Fortran of the same executable: `just smoke-ww3-gpu gpu` (the owner), `test.comp` 0 files different
- [ ] `bench_small` likewise: `just full-ww3-gpu gpu` (the owner)
- [ ] The CUDA regtest matrix passes strict, `test.comp` 0 different on every entry that runs the port: `just ww3-gpu-matrix-cuda` (the owner)
- [ ] An ADR superseding ADR-0014's verdict for the routed parts is written with the records above, or ADR-0014's revisit records why not

## 8. Risks, limitations, and honest caveats

- **rcpps is the host CPU's.** libmvec's `powf` rounds `rcpps` to choose a table entry, and on the
  i9-14900 the rounded value differs from an exact reciprocal's for 661 269 of 8 388 608
  mantissas (v, `gen_tables`). The device copies the build host's `rcpps`; a run whose Fortran
  executes on another vendor's CPU needs the tables generated there ⚠.
- **Speed.** glibc's `expf`, `logf`, `powf` (ARM's algorithms) and every CORE-MATH function
  compute in double. An H100 runs FP64 at about half its FP32 rate; an RTX 4090 at about 1/64,
  so there the routed build is expected to be clearly slower than libdevice ⚠ (vendor
  datasheets, not measured). `-DWW_KOKKOS_LIBM=OFF` keeps libdevice; a GPU timing should be
  recorded both ways.
- **NaN payloads** differ between x86 and CUDA default NaNs; `test.comp` compares bytes, so a NaN
  in an output field would fail the entry even with every finite value equal ⚠ (no regtest output
  is known to hold one).
- **Tables in device code.** `glibc_mathf.hpp` keeps its tables in global memory (`__device__
  const`), since the threads of a warp read different entries and `__constant__` serves one
  address at a time; `coremath_f.hpp` keeps CORE-MATH's as `constexpr` locals, which nvcc may materialise per call in
  local memory. That is a speed question, not a parity one, and is measured after §7.
- **Register pressure.** CORE-MATH's `atan2f` slow path (double-double polynomial, 32 terms)
  inlined into `W3SIN4`'s kernel may spill; `--resource-usage` is read in unit 3 ⚠.
- **Device conversions and denormals** are the untested part of "same operations, same bits":
  `-ftz=false` is nvcc's default without `--use_fast_math` ⚠ (the presets' flags to be read in
  unit 1).

## 9. Alternatives considered

- **Do nothing, keep ADR-0014.** Every GPU number stays "judged by tolerance"; the proposal's
  claim of an unchanged answer holds only on the CPU.
- **Correctly rounded functions everywhere (CORE-MATH for `expf`, `logf`, `powf`, `sincosf`).**
  Gives one answer on any machine, but not the Fortran's: glibc's `expf` is not correctly
  rounded, so the CPU Fortran itself would differ. Rejected: the reference is glibc's bits.
- **Rebuild WW3 without libmvec.** Removes the `rcpps` question but changes the Fortran's bits
  against every record so far (`GPU_LIBM_BITS_202610.md`, "libmvec").

## 11. Open questions

- Should a GPU build fail at configure when `gen_tables` cannot run (no libmvec found)? **Default:**
  yes, unless `-DWW_KOKKOS_LIBM=OFF` is passed; the owner decides.
- Is a separate `sinf`/`cosf` ever needed on the device? **Default:** no; unit 5's check flags any
  lone call, and a lone call gets `sinf`/`cosf` transcribed from the same `sincosf.h` before it
  merges.
- Is `WW_KOKKOS_LIBM=ON` the default of a CUDA build? **Default:** yes, as the bit-for-bit rule
  asks; the owner decides once the H100 cost is measured.

## Appendix

### Checked live

- `grep -nE` of the nine `kokkos/src/ww_kokkos` files of §4, 2026-10-17: the call sites listed.
- `nm -u` of `kokkos/build/openmp-release` and `kokkos/build/cuda-release` objects (`srce_shim.cpp.o`,
  `st4_shim.cpp.o`), 2026-10-17: the libm sets quoted in §2.
- ADR-0014, read 2026-10-17: the reasons and the revisit condition quoted in §2.

### Not checked

- The CUDA presets' flags beyond `--fmad=false` (`-ftz`, `-prec-div`, `-prec-sqrt`) ⚠.
- Whether WFIP-0005's units add or remove call sites ⚠.
- Any run on a GPU.
