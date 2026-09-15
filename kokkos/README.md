# `kokkos/` — the C++/Kokkos half of the lab

One CMake tree, one backend per configure preset. It holds the portable kernel
library (`ww_kokkos`), the lesson-11 intro programs, the GoogleTest suites and the
C++ tools. The first ported source term, `W3SNL1`/`INSNL1` (the DIA nonlinear
interactions), lives in `src/ww_kokkos/snl1_*`.

## Toolchain

Everything is built inside the pinned `nix-config/labs/pratico` shells — they are
what put Kokkos, GoogleTest, CMake, Ninja and NetCDF on the search path:

| shell | Kokkos backends | used by |
|---|---|---|
| `nix develop ./nix-config/labs/pratico#ww3` | Serial + OpenMP | `serial-debug`, `openmp-release`, CI |
| `nix develop ./nix-config/labs/pratico#cuda` | CUDA (ADA89) + Serial | `cuda-release`, this host only |

Pins: Kokkos 5.2.0, GoogleTest 1.18.0, CMake 4.4.2, gfortran 15.3.

## Layout

```
CMakeLists.txt        top level: options, find_package, the ww_kokkos target
CMakePresets.json     serial-debug, openmp-release, cuda-release
cmake/                CompilerWarnings.cmake -- the one warning policy
src/ww_kokkos/        real.hpp (float32 + helpers), spectrum_fixtures.hpp (JONSWAP, cos^2),
                      snl1_{config,tables,dia} (the DIA port), fixture_io (fixture reader)
src/fortran_iface/    ww_kokkos_c.hpp + snl1_shim.cpp (the C ABI, built into ww_kokkos),
                      w3kokkosmd.F90 (the Fortran module), PATCH.md (the WW3 caller)
intro/                01..06, one Kokkos concept each, each a CTest case
tests/                kokkos_env.hpp (runtime lifetime), L1_* GoogleTest suites, bench_snl1.cpp
tests/fixtures/       the verbatim Fortran reference, the shared sea state, the committed
                      binary fixtures and shim_driver.F90 (the shim_roundtrip CTest case)
PORT_STATUS.md        the port ledger: phase, parity and ms/call per routine
tools/nccmp-tol/      per-field NetCDF comparator                (Task 5)
tools/bench_case/     benchmark-case generator                   (Task 6)
tools/fetch_analyse/  fetch-growth analyser                      (Task 6)
```

## Building

From the repository root:

```bash
just kokkos-test serial-debug     # configure + build + ctest
just kokkos-test openmp-release
just kokkos-cuda-test             # cuda-release, inside the #cuda shell
just kokkos-clean                 # rm -rf kokkos/build
just snl1-fixtures                # regenerate the committed W3SNL1 fixture
```

Or directly, from this directory inside a pratico shell:

```bash
cmake --preset openmp-release && cmake --build --preset openmp-release && ctest --preset openmp-release
```

Builds land in `kokkos/build/<preset>/` (git-ignored).

## Presets

| preset | build type | notable |
|---|---|---|
| `serial-debug` | Debug | AddressSanitizer + UndefinedBehaviorSanitizer, `WW_DETERMINISTIC=ON` |
| `openmp-release` | Release | `-O3 -march=x86-64-v3` |
| `cuda-release` | Release | `CMAKE_CXX_COMPILER=nvcc_wrapper`, `CMAKE_CUDA_ARCHITECTURES=89` |

Two things the names do *not* promise:

- **The backend comes from the shell, not from the preset.** OpenMP is the default
  execution space in `#ww3`, so `serial-debug` is a *build type*, not a Serial-only
  build. Code that must run on Serial says `Kokkos::Serial` explicitly — see
  `intro/03_reduce_and_scan.cpp` and the last case in `tests/L1_test_intro.cpp`.
- **`Kokkos_ENABLE_DEBUG_BOUNDS_CHECK` cannot be set here.** It is an option of the
  Kokkos *build*, so it is fixed by the pinned Kokkos derivation in nix-config; a
  consumer project cannot turn it on. `serial-debug`'s safety net is the sanitizers.

`CMakePresets.json` uses presets schema version 10, which needs CMake ≥ 4.1.

## Options

| option | default | meaning |
|---|---|---|
| `WW_ENABLE_FORTRAN` | `ON` | build the Fortran interface module, reference and drivers (incl. `intro_06`) |
| `WW_DETERMINISTIC` | `OFF` | pin reductions to a fixed summation order for bit-reproducible validation runs |
| `WW_WW3_BUILD_DIR` | *(empty)* | a configured WW3 build (`libww3.a` + `mod/`); empty skips the WW3-linked targets |

## Intro programs

Each is a self-checking `main`: it prints one line and exits non-zero if the
concept it demonstrates does not hold, so CI runs the lesson material too.

| program | concept |
|---|---|
| `01_views` | `View` allocation, label, extents, `deep_copy` to a host mirror |
| `02_parallel_for` | `MDRangePolicy<Rank<2>>` + `parallel_reduce`, checked against a closed-form `m0` |
| `03_reduce_and_scan` | `Kokkos::MaxLoc` (value + location) and `parallel_scan` |
| `04_layouts_and_mirrors` | `LayoutLeft` vs `LayoutRight` strides, mirrors, unmanaged views |
| `05_team_scratch` | `TeamPolicy` + `team_scratch(0)`: the DIA kernel's extended spectrum |
| `06_interop_bindc` | `extern "C"` + Fortran `bind(C)`, borrowing a Fortran array |

## Tests

`tests/kokkos_env.hpp` starts the Kokkos runtime once per test binary from a
`::testing::Environment` (two OpenMP threads). `ww_add_test(<name>)` in
`tests/CMakeLists.txt` builds `<name>.cpp`, hands it `WW_FIXTURE_DIR` and registers
it with CTest.

Naming: `L1_*` are unit tests against analytic or captured-Fortran fixtures; `L2_*`
replay a whole WW3 regtest.

| suite | what it pins down |
|---|---|
| `L1_test_intro` | the lesson material's invariants |
| `L1_test_snl1_tables` | `make_tables` against `INSNL1`: all 32 address tables exactly, the weights and `AF11` to 1e-6 relative |
| `L1_test_snl1_dia` | `snl1` against `W3SNL1` to 1e-5 relative; plus zero-in/zero-out, the cubic scaling of `Snl`, and launch-to-launch bit-reproducibility |
| `L1_test_snl1_shim` | the same parity through the raw C ABI, plus the error paths: call before init, negative `npts`, null pointers, buffer growth, idempotent init |
| `shim_roundtrip` | a Fortran program (`tests/fixtures/shim_driver.F90`) calling `W3KOKKOSMD` → shim → kernel and comparing with `W3SNL1_REF` in the same process |

Kernels live in free functions rather than in `TEST()` bodies: nvcc rejects an
extended lambda inside a private member function, and a `TEST()` body is one.

## The W3SNL1 port

`src/ww_kokkos/snl1_dia.cpp` and `snl1_tables.cpp` are translations, not
reimplementations: same expressions, same order, same float32 arithmetic as
`WW3/model/src/w3snl1md.F90` (WW3 7.14). They carry `SPDX-License-Identifier:
LGPL-3.0-or-later`, like their source; the tooling around them is MIT.

Two consequences worth knowing before touching them:

- **Floating-point contraction is off** for `ww_kokkos` (`-ffp-contract=off`, and
  `--fmad=false` on the CUDA backend). With GCC's default `-ffp-contract=fast`,
  `-O3 -march=x86-64-v3` fuses `AWG1*UE(..) + AWG2*UE(..)` into an FMA -- one
  rounding where the Fortran does two -- and `openmp-release` drifted 1.1e-5
  relative from the fixture while `serial-debug` was bit-identical. With
  contraction off all three presets reproduce the Fortran **bit for bit**.
- **`x**n` is not `std::pow`.** gfortran lowers a real raised to an integer to a
  binary-exponent chain of multiplications; `powf` does something else, to within
  a ULP. `snl1_tables.cpp` has a `powi()` that reproduces the chain, and that is
  what makes the parity exact rather than merely close.

The DIA has no reduction, so `WW_DETERMINISTIC` changes nothing here: every output
element is written by one thread from inputs no thread modifies.

### The `ww3_lib` cross-check -- untested

`tests/fixtures/gen_snl1_ww3lib.F90` and `just l1-crosscheck` exist to prove that
`snl1_ref.F90` really is a verbatim copy, by running the genuine `W3SNL1` out of a
configured WW3 build and `cmp`-ing the two fixture files. **Neither has ever been
run.** The WW3 build available on this host (`~/src/WW3/build`) was configured with
switch `NL0`, so `w3snl1md` is not compiled into its `libww3.a` and `w3snl1md.mod`
does not exist; the CMake guard detects exactly that and skips the target with a
warning. Running it needs a WW3 built with an `NL1` switch, e.g.
`just build switches/switch_lab_shrd`. Until then the committed fixture, generated
from the standalone reference, is the sole source of truth.

## The bind(C) boundary

`src/fortran_iface/` is how WAVEWATCH III reaches the kernels. Three files and one
rule each:

| file | rule |
|---|---|
| `ww_kokkos_c.hpp` | the C ABI: `float` (WW3's default `REAL`), Fortran-ordered, caller-owned arrays, and no function that can throw |
| `snl1_shim.cpp` | ownership (finalize only a Kokkos *we* started), lifetime (a `push_finalize_hook` drops the Views before `Kokkos::finalize`), and errors (every entry point is a `try`/`catch` that records a code) |
| `w3kokkosmd.F90` | `MODULE W3KOKKOSMD`: one `ISO_C_BINDING` interface block per C declaration, plus `LOGICAL :: KOKKOS_SNL1` |

The C++ half is compiled into `ww_kokkos` unconditionally, so the shim is testable
with no Fortran compiler in the loop; only `w3kokkosmd.F90` needs
`WW_ENABLE_FORTRAN`, and it is built as its own target (`ww_kokkos_f`) because it
is meant to be *copied into* `WW3/model/src` — which is what
`src/fortran_iface/PATCH.md` describes, hunk by hunk, with real line numbers.
Nothing in this repository modifies `WW3/`.

Two switches, both read at `ww_kokkos_init()`:

| variable | effect |
|---|---|
| `WW_KOKKOS_SNL1=1` | `ww_snl1_enabled()` returns 1, so `W3KOKKOS_SETUP` sets `KOKKOS_SNL1` and the patched `W3SRCE` calls the port instead of `W3SNL1` |
| `WW_KOKKOS_DEVICE_ID` | which GPU to use (default 0). It is an environment variable and not `MPI_Comm_rank()` on purpose: phase 1 links no MPI, so `ww_kokkos_init(comm_f)` accepts the communicator and ignores it |

`ww_snl1` is `void` because a Fortran `CALL` cannot read a return value; its status
is `ww_snl1_last_error()`, and a caller that ignores it turns a failed launch into
a plausible-looking wrong forecast.

Three limits of phase 1, all of them consequences of the shim holding one
file-static, unlocked context. They are the caller's problem, so `PATCH.md`
handles each one explicitly:

| limit | what goes wrong without it |
|---|---|
| one spectral grid per process | `ww_snl1_init` replaces the tables rather than adding a grid, so a `ww3_multi` run reads past the end of a later grid's spectra |
| one caller at a time | `W3SRCE` runs inside an `!$OMP PARALLEL` region under `W3_OMPG`/`W3_OMP0`; two threads in `ww_snl1` race over the device buffers |
| `sig` starts at bin 1 | the C dummy is assumed-size and `W3GDATMD` allocates `SIG(0:MK+1)`, so passing the bare name builds every quadruplet one frequency bin low |

## Timing

`tests/bench_snl1.cpp` builds `ww_bench_snl1` (not a CTest case): 1 000 sea points,
20 calls, timed both end-to-end through `ww_snl1()` and with everything already on
the device. The numbers, the machine they were measured on and what they do *not*
mean are in [`PORT_STATUS.md`](PORT_STATUS.md).

SPDX-License-Identifier: MIT
