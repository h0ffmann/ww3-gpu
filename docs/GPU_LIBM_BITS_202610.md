# Matching glibc's libm on a GPU, and the host's FMA: which functions, which bits, which CPU

2026-10-17. Issue [#42](https://github.com/h0ffmann/ww3-gpu/issues/42). Since
[ADR-0014](ADRs/ADR-0014-gpu-runs-judged-by-tolerance.md) (accepted 2026-10-17) a GPU run is
judged by `nccmp-tol`'s tolerances, because the port's device code cannot give glibc's bits at
known sites: libdevice's `expf`, `logf`, `powf`, `sincosf` and `tanhf`, and ST4's five libmvec
loops. The ADR is to be revisited when "a device math library with glibc's bits, or a
reimplementation of the five libmvec loops and the libdevice calls with glibc's algorithms,
exists". This page measures how far that is: which libm functions the `W3SRCE` unit calls per
sea point, which of them glibc rounds correctly, and a device transcription of the four it does
not, checked on the host. It also answers the owner's question of 2026-10-17 on how the host
CPU's FMA enters the Fortran's bits, here and on the H100 node.

Nothing here ran on a GPU (`AGENTS.md`, "Running on the GPU"). Every device statement is a
compile-time fact read from the SASS, or reasoning marked ⚠.

## Short answer

- Per sea point, the `W3SRCE` unit calls eleven libm entry points: `expf`, `logf`, `powf`,
  `sincosf`, `tanhf`, `atan2f`, `log10f`, libmvec's `_ZGVbN4v_expf` and `_ZGVbN4vv_powf`, and
  double `exp` and `tanh` ([table](#what-the-fortran-calls-per-sea-point)).
- Four of them are correctly rounded in glibc 2.44 (`tanhf`, `atan2f`, `log10f`, double `tanh`):
  any correctly rounded device implementation gives the same bits. `tanhf` and `log10f` were
  checked on all 2^32 inputs, `atan2f` on 2^30 pairs (v).
- Four are not (`expf`, `logf`, `powf`, `sincosf`). `kokkos/tools/libm_bits/glibc_mathf.hpp`
  transcribes them so that host and device code compute glibc's bits: 0 differences on all
  2^32 inputs of `expf`, `logf` and `sincosf` and on 7.9 × 10^10 pairs of `powf` (v, host). The
  device build has the same fused and unfused operations as glibc's (v, SASS); that it gives the
  same bits on a device is ⚠ until `scripts/libm_bits.sh --device` runs under the owner.
- glibc picks a build of `expf`, `logf`, `powf`, `sincosf`, `exp` and `tanh` per CPU: an fma build
  when the CPU has FMA and AVX2, an sse2 build otherwise. The two differ on 2 inputs of `expf`,
  34 outputs of `sincosf`, 15 of 7.9 × 10^10 pairs of `powf` and none of `logf` (v). WW3's own
  output did not change between them on `ww3_ts1` and `bench_small` (v).
- libmvec is the open item: its `expf` and `powf` differ from the scalar ones on 2.21 % and
  5.14 % of inputs (v), it has no device version here, and its SSE4 `powf` uses `rcpps`, an
  instruction whose bits the CPU vendor chooses ⚠.

## What the Fortran calls per sea point

Read from the objects of WW3's own build (`build/ww3-gpu-openmp`, the lab switch, gfortran 16.2.0
`-O3`, glibc 2.44 at `/nix/store/h4wfwic1...-glibc-2.44-25`): `objdump -dr` of each module's
`.F90.o`, every call to a libm symbol, grouped by the routine it sits in (v, 2026-10-17). The
set-up routines (`INSIN4`, `TABU_STRESS`, `TABU_TAUHF*`, `INSNL1`, `INSNLGQM`, `GAULEG`) also call
`cosf`, `sinf`, `acosf`, `asinf`, `cos`, `acos`, `log`, `pow` and libmvec's `_ZGVbN2v_cos`; they run
once on the host and their tables are copied, so they do not enter the device question.

| Entry point | Called per point by | glibc 2.44 | fma and sse2 builds | Device |
|---|---|---|---|---|
| `expf` | `W3SDS4`, `W3SIN4`, `W3SNL1`, `W3SLN1`, `CALC_USTAR` | not correctly rounded (ARM algorithm) | differ on 2 of 2^32 inputs | `ww::libm::expf` |
| `logf` | `W3SDS4`, `W3SIN4`, `W3SRCE`, `CALC_USTAR` | not correctly rounded | identical on all 2^32 | `ww::libm::logf` |
| `powf` | `W3SDS4`, `W3SIN4`, `W3SPR4`, `CALC_USTAR` | not correctly rounded | differ on 15 of 7.9 × 10^10 pairs | `ww::libm::powf` |
| `sincosf` | `W3SDS4`, `W3SIN4`, `W3SRCE`, `W3SLN1` | not correctly rounded | differ on 34 of 2 × 2^32 outputs | `ww::libm::sincosf` |
| `tanhf` | `W3SDS4`, `W3SIN4`, `W3SRCE`, `CALC_USTAR` | correctly rounded (CORE-MATH, 2.41) | one build | any correctly rounded `tanhf` |
| `atan2f` | `W3SDS4`, `W3SIN4`, `W3SPR4` | correctly rounded (CORE-MATH, 2.41) | one build | any correctly rounded `atan2f` |
| `log10f` | `W3SIN4` | correctly rounded (CORE-MATH, 2.41) | one build | any correctly rounded `log10f` |
| `_ZGVbN4v_expf` | `W3SDS4` (`w3src4md.F90:2509`) | libmvec, SSE4 build | not ifunc-fma; sse4 or sse2 by SSE4.1 ⚠ | none yet |
| `_ZGVbN4vv_powf` | `W3SDS4`, `W3SPR4` (`:316`, `:317`, `:2315`, `:2500`) | libmvec, SSE4 build, uses `rcpps` | as above | none yet |
| `exp` (double) | `W3SDB1` | not correctly rounded (ARM algorithm) | differ (fingerprint below) | not written |
| `tanh` (double) | `W3SDB1` | correctly rounded (CORE-MATH, 2.44) | fma and sse2 builds, same fingerprint | any correctly rounded `tanh` |

The libmvec line numbers are those of `kokkos/src/ww_kokkos/st4_libmvec.hpp`, from its GIMPLE
inventory. Correct rounding is from glibc 2.44's `NEWS` (v, the release tarball, sha256
`37f600f2...`, read 2026-10-17) and, for `tanhf`, `log10f` and `atan2f`, from the check below. The
fma/sse2 rows come from `ww <fn> plain` below. The CUDA math library's `tanhf` and others are
not correctly rounded ⚠ (general knowledge), so "any correctly rounded" means a transcription
of CORE-MATH, not CUDA's own function.

## The transcription

`glibc_mathf.hpp` takes the algorithm and the tables of `expf`, `logf`, `powf` and `sincosf` from
ARM optimized-routines at `f67e7daa` ("MIT OR Apache-2.0 WITH LLVM-exception"), the code glibc
imports. The configuration is glibc's x86-64 one: `TOINT_INTRINSICS` 0, `POWF_SCALE_BITS` 0,
the error paths' return values of `math_errf.c`. One template parameter chooses the build:

- **fma** (`Fma = true`): a call to `::fma` at every site where GCC fused when it compiled
  glibc with `-mfma`, read from the disassembly of `__expf_fma`, `__logf_fma`, `__powf_fma` and
  `__sincosf_fma` in glibc 2.44's `libm.so.6`. GCC's choices are not all visible in the C:
  in `expf` it duplicates `InvLn2N*xd` into both of its uses, `kd = fma(InvLn2N, xd, SHIFT)` and
  `r = fma(InvLn2N, xd, -kd)`; in `sincosf` it pairs the sine and the cosine in one SIMD register,
  each lane the scalar arithmetic.
- **plain** (`Fma = false`): every product rounded, then added, as the sse2 build does.

`::fma` is correctly rounded on the host and on a CUDA device, so the explicit fusions are
deterministic. Implicit contraction is not: the file is compiled with `-ffp-contract=off` and
`--fmad=false`, the flags `ww_kokkos` already uses.

### Results on the host

Machine: Intel Core i9-14900 (flags `fma avx2 sse4_1`), the pinned `ww3` shell (g++ 16.2.0,
glibc 2.44-25), 4 threads under `nice -n 19`. Command: `scripts/libm_bits.sh all 4`
(2026-10-17). "Differing": outputs whose bits differ, NaN outputs excluded and counted apart.

| Check | Inputs | Differing | What it shows |
|---|---|---|---|
| `probe` | — | — | glibc gave this process the **fma** build of `expf` and `sincosf`; `logf`'s two builds never differ (`ww logf plain`), and for `powf(x, 2.5)` the probe found no difference from x = 0.5 upward |
| `ww expf fma` | 4 294 967 296 (all) | **0** | the transcription is glibc's `expf` |
| `ww logf fma` | 4 294 967 296 | **0** | |
| `ww sincosf fma` | 4 294 967 296 (sine and cosine) | **0** | |
| `ww powf fma` | 79 456 894 976 | **0** | every x with 10 fixed y, every y with 8 fixed x, 2^31 random pairs |
| `ww expf plain` | 4 294 967 296 | 2 | the sse2 build against the fma build glibc gives here, e.g. x = `0x4202422f` |
| `ww logf plain` | 4 294 967 296 | 0 | |
| `ww sincosf plain` | 4 294 967 296 | 34 | e.g. cos at x = `0xc18a3adb` |
| `ww powf plain` | 79 456 894 976 | 15 | |
| `cr tanhf` | 4 294 967 296 | **0** | correctly rounded on every input (0 unresolved) |
| `cr log10f` | 4 294 967 296 | **0** | 0 unresolved; 2 139 095 040 NaN outputs whose payload is not judged |
| `cr atan2f` | 1 073 741 824 random pairs | **0** | 1 pair unresolved, 16 356 NaN outputs |
| `mvec expf` | 4 294 967 296 | 94 794 290 (2.21 %) | libmvec's `expf` is not the scalar `expf` |
| `mvec powf` | 1 073 741 824 random pairs | 55 228 991 (5.14 %) | libmvec's `powf` is not the scalar `powf` |

That the plain transcription is glibc's sse2 build was checked at the probe's distinguishing
inputs: with `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA` the probe answers sse2 for `expf` and
`sincosf` (v). The plain build was not compared exhaustively under that tunable ⚠.

The correct-rounding check takes glibc's double function, rounds it to float, and when the
double lies within two double ulps of a float midpoint recomputes in `__float128`
(libquadmath); "unresolved" counts the inputs still too close to call there.

### What a device compiles

`scripts/libm_bits.sh --nvcc` builds `device_check.cu` for sm_89 (the RTX 4090) and sm_90 (the
H100) with nvcc in the pinned `cuda` shell and runs nothing. One kernel per function and build,
compiled alone for sm_90 and read with `cuobjdump -sass` (v, 2026-10-17):

| Function | DFMA | DMUL | DADD | glibc's `__<fn>_fma`, x86 |
|---|---|---|---|---|
| `expf<true>` | 5 | 2 | 1 | 5 `vfmadd`/`vfmsub`, 2 `vmulsd`, 1 `vsubsd` |
| `logf<true>` | 5 | 1 | 1 | 5 fused, 1 `vmulsd`, 1 `vaddsd` |
| `powf<true>` | 9 | 5 | 4 | 9 fused, 5 `vmulsd`, 4 `vaddsd`/`vsubsd` |
| `sincosf<true>` | 22 | 19 | 0 | 7 fused per polynomial (inlined at 3 sites) and 1 in the reduction |
| any `<false>` | 0 | | | the sse2 build fuses nothing |

IEEE 754 double addition, multiplication and fused multiply-add are correctly rounded on both
sides, so the same operations on the same operands give the same bits. Whether they do on the
device, with its denormals, its conversions and the table reads from constant memory, is what
the supervised run answers ⚠: `scripts/libm_bits.sh --device` asks for a typed "yes", compares
every input of each function with the host's glibc, and prints CUDA's own functions beside the
transcription for comparison.

## The host's FMA, and the H100 node

**How the host CPU enters the Fortran's bits.** On x86-64, glibc chooses a build of `expf`,
`logf`, `powf`, `sinf`, `cosf`, `sincosf` and `tanh` once, at load time: the fma build if the CPU
reports FMA and AVX2, the sse2 build otherwise (v, glibc 2.44
`sysdeps/x86_64/fpu/multiarch/ifunc-fma.h`); double `exp` has its own selector
(`ifunc-avx-fma4.h`). WW3's Fortran gets whichever its CPU qualifies for, so "the Fortran's
bits" are the bits of that CPU's build. The i9-14900 gets the fma build (v, probe). libmvec has
a second, independent choice: its SSE4 build when the CPU has SSE4.1 (glibc 2.44
`ifunc-mathvec-sse4_1.h`), otherwise a wrapper that calls the scalar function per lane ⚠ (read,
not run).

**Does it change WW3's output?** Not on the two cases tried. Pure NOAA WW3 (`--ww3`, `03adda91`,
`switches/switch_lab_ompg`, the binary of `build/ww3/ww3/bin`), `OMP_NUM_THREADS=4`, run twice per
case with `GLIBC_TUNABLES` unset and set to `glibc.cpu.hwcaps=-AVX2,-FMA` (v, 2026-10-17):

| Case | `out_grd.ww3` fma vs sse2 | `out_pnt.ww3` fma vs sse2 | fma vs the run of 2026-10-15 |
|---|---|---|---|
| `ww3_ts1` (smoke) | identical | identical | identical |
| `bench_small` (full, 12 h) | identical | — | identical |

The few inputs where the builds differ were not met on these cases, or a difference rounded
away. That WW3's regtests are insensitive to the choice in general is not shown ⚠: a case that
evaluates `sincosf` or `exp` at one of those inputs changes.

**What follows for the H100 node** (issue #143; its CPU is not known here ⚠):

1. **The bit-for-bit rule does not cross machines.** It compares the Kokkos routine with the
   Fortran in the same executable on the same machine (`AGENTS.md`). On the node, the Fortran
   run is the reference, whatever build its CPU gets. A comparison of a node result with an
   i9-14900 record is a comparison of machines, not a test of the port.
2. **Use the pinned toolchain there.** The Nix shell brings glibc 2.44-25 with it. The system's
   glibc (2.43 on this machine's Ubuntu, `ldd --version`) is another library, with other
   CORE-MATH imports and possibly other bits.
3. **Run `scripts/libm_bits.sh probe` and `fingerprint` on the node first.** Equal hashes mean
   the node's host libm gives WW3 the same bits as here. The i9-14900's hashes:

   | Function | glibc fma build (default here) | with `-AVX2,-FMA` |
   |---|---|---|
   | `expf` | `a2c5f964fe2982f1` | `a2c5f964fe2982f1` |
   | `logf` | `480619614697393b` | `480619614697393b` |
   | `sincosf` | `e1d3fc956475d9d5` | `6ed400afc87448e5` |
   | `tanhf` | `4c6e4767b7543051` | `4c6e4767b7543051` |
   | `log10f` | `bffa8942ff519a41` | `bffa8942ff519a41` |
   | `powf` | `34c11ffdaf05b366` | `34c11ffdaf05b366` |
   | `atan2f` | `3d38accc84457a9d` | `3d38accc84457a9d` |
   | `_ZGVbN4v_expf` | `c819cfbda4fd167e` | `c819cfbda4fd167e` |
   | `_ZGVbN4vv_powf` | `274a16ed29f173c1` | `274a16ed29f173c1` |
   | `exp` | `d0624f4ec1450f25` | `f00781d58261b86a` |
   | `tanh` | `a7fe6277e0c1b08f` | `a7fe6277e0c1b08f` |

   The hashes sample (every 16th float, 2^26 seeded pairs; `libm_bits.cpp`), so `expf`'s two
   differing inputs are not in its row. Command: `scripts/libm_bits.sh fingerprint`, and the
   same with `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-FMA`.
4. **The device must follow the host's choice.** A device kernel that calls
   `ww::libm::expf<true>` matches a host that got the fma build. On a host without FMA and AVX2
   (none expected on an H100 server ⚠) it would need `<false>`. The choice belongs at run
   time, read once from the CPU the way glibc reads it, not in a compile flag.
5. **An AMD host may change libmvec's `powf`.** glibc's SSE4 `_ZGVbN4vv_powf` rounds an
   `rcpps` reciprocal to a 10-bit integer and uses it to choose a table entry
   (`svml_s_powf4_core_sse4.S:117-135`). Intel and AMD specify `rcpps` only to a relative error,
   not its bits ⚠, so near a rounding boundary the two vendors could pick different entries. If
   the node's `_ZGVbN4vv_powf` hash differs from the row above, that is the likely cause, and
   `W3SDS4` and `W3SPR4` then have another reference on that node.
6. **An Arm host (Grace) is another case.** glibc's aarch64 `expf` has no fma/sse2 pair, and
   gfortran there fuses by default where WW3's flags allow it ⚠ (not checked). The Fortran
   reference on such a node is re-established there, as rule 1 says.

The device side has no such choice: CUDA's `--fmad=false` stops nvcc from fusing, and
`glibc_mathf.hpp` fuses exactly where glibc's fma build does.

## libmvec, the open item

gfortran `-O3` turns five loops of `W3SDS4` and `W3SPR4` into calls of `_ZGVbN4v_expf` and
`_ZGVbN4vv_powf` four lanes at a time, and their bits differ from the scalar functions on 2.21 %
and 5.14 % of inputs (table above). The port reproduces them on the host by calling libmvec
(`st4_libmvec.hpp`); on a device that header falls back to scalar `Kokkos::exp` and
`Kokkos::pow`, and says so. Ways forward, each a decision for the owner:

- Transcribe glibc's `svml_s_expf4_core_sse4.S` (212 lines, packed single `mulps`/`addps`/`subps`
  only) and `svml_s_powf4_core_sse4.S` (374 lines, double arithmetic and two `rcpps`) the way
  `glibc_mathf.hpp` transcribes the scalar ones, and check them with `libm_bits` against
  libmvec. `expf4` looks direct ⚠. `powf4` needs `rcpps`'s bits for the CPU the Fortran runs
  on: a table of Intel's results is a fact about one microarchitecture ⚠.
- Build WW3 so that gfortran calls the scalar functions there (for example without loop
  vectorisation of those loops). That changes the Fortran's bits against every record so far
  and against WW3's own build: a change of the reference, not of the port.

## What is not shown

- No device run: that the device's bits equal the host's is ⚠ until `--device` runs.
- Double `exp` (`W3SDB1`) is not transcribed; it differs between glibc's builds (fingerprint).
- `tanhf`, `atan2f`, `log10f` and double `tanh` have no device implementation here; CORE-MATH's
  code in glibc carries the MIT licence and is the candidate ⚠.
- The NaN outputs are not judged: their payloads differ between x86 and a CUDA device in
  general, and the regtests compare fields, not payloads (`kokkos/port_report/`, the open
  NaN-payload notes).
- No kernel of `kokkos/src` calls `glibc_mathf.hpp` yet. Wiring it in changes what the CUDA
  build computes and is its own pull request, after the WFIP-0005 work in progress has merged.
  With libmvec still open, it would bring the device closer to the Fortran's bits but not to
  them, so ADR-0014's verdict by tolerance stands until both are done.
