# C++/Kokkos course and code — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn ww3-gpu into the lab for the UFRJ/DEL proposal: a compiled, tested C++/Kokkos port of WW3's `W3SNL1` kernel with its validation tooling, a Python-free code base, a 16-lesson course, and the toolchain pinned in `h0ffmann/nix-config` `labs/pratico`.

**Architecture:** One CMake tree in `kokkos/` (C, C++, Fortran) consumes Kokkos, GoogleTest and netcdf from the `nix-config/labs/pratico` `#ww3` devShell. The DIA kernel is translated from `WW3/model/src/w3snl1md.F90` (7.14) without algorithmic change, exposed to Fortran through `bind(C)`, and validated by GoogleTest against fixtures produced by a verbatim Fortran reference. A per-field NetCDF comparator gates every optimisation step. Lessons reference only files that exist.

**Tech Stack:** C++20, Kokkos 5.2 (Serial/OpenMP; CUDA preset for the owner's RTX 4090), GoogleTest 1.17, CMake ≥ 3.25 with presets, gfortran 14 / OpenMPI / netcdf-c / netcdf-fortran from pratico, shell (bash, shellcheck-clean), pandoc book pipeline unchanged.

**Spec:** `docs/superpowers/specs/2026-09-15-cpp-kokkos-course-design.md`

## Global Constraints

- Lab code languages: C++, Fortran, C, shell. No Python except `scripts/book_prep.py` and `scripts/translate_md.py`.
- C++ line count target ≥ 2 500 (`git ls-files '*.cpp' '*.hpp' | xargs cat | wc -l`).
- Every build and test runs inside `nix develop <repo>/nix-config/labs/pratico#ww3 --command ...` (justfile variable `pratico`). Do not `apt install` anything.
- Kokkos rules from `docs/AGENTS_KOKKOS_202609.md` §1.2: explicit memory spaces, `LayoutLeft` at the Fortran boundary, no host containers or allocation inside kernels, `parallel_reduce` not atomics, `float` state, `Kokkos::fence()` before host reads, kernel labels named after the WW3 routine (`"srce.snl1.dia"`).
- Phase-1 rule: translate, do not improve. Any deviation from the Fortran arithmetic order is a bug.
- File headers on every new C++/Fortran file: one comment block with purpose, WW3 heritage (module, routine, version 7.14, file path) when translating, and `SPDX-License-Identifier: LGPL-3.0-or-later` (WW3-derived code) or `MIT` (new tooling, matching the repo `LICENSE`).
- Commits: conventional prefix (`feat:`, `docs:`, `build:`, `ci:`, `test:`, `chore:`), body explains why, trailer `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>` (or the executing model's own attribution line as given by the harness).
- Course voice: English, direct, tables, `(v)` for verified facts and `⚠` for unverified ones, as in the existing lessons.
- Work on branch `feat/cpp-kokkos-course` in `/home/hoffmann/code/ww-lab` (remote `h0ffmann/ww3-gpu`). nix-config work happens in a fresh clone under the scratchpad directory on branch `feat/pratico-kokkos`.

---

## Task map and order

| Task | Deliverable | Depends on |
|---|---|---|
| 1 | nix-config `labs/pratico`: Kokkos (OpenMP), GoogleTest, gdb, valgrind; `#cuda` shell; `kokkos-smoke` check | — |
| 2 | `kokkos/` CMake skeleton, presets, intro programs, first GTest, `just kokkos-*`, CI job | 1 (pin the submodule to the PR branch commit) |
| 3 | `W3SNL1`/`INSNL1` port: tables, kernel, Fortran reference, fixtures, L1 tests | 2 |
| 4 | `bind(C)` shim, `w3kokkosmd.F90`, `PATCH.md`, `PORT_STATUS.md` | 3 |
| 5 | `nccmp-tol` comparator, `L2_replay.sh`, `tools/profile/` | 2 |
| 6 | Python removal: bench/examples/exercises in C++/Fortran/shell; justfile; README; CI | 2, 5 |
| 7 | Lessons 00–08 revised, 14–15 renumbered and updated | 3, 5, 6 (file names) |
| 8 | Lessons 09–13 written, `course/README.md`, book builds | 3, 4, 5, 6 |
| 9 | Final verification, ww3-gpu PR, nix-config PR referencing it | all |

Tasks 3 and 5 can run in parallel after 2. Task 6 can start after 2 (its bench/examples parts) and finish after 5 (comparator in `example01`). Tasks 7 and 8 need the final file names from 3–6; run them last, in parallel with each other.

---

### Task 1: nix-config — extend `labs/pratico`

**Files (in a fresh clone of `git@github.com:h0ffmann/nix-config.git`, branch `feat/pratico-kokkos`):**
- Modify: `labs/pratico/flake.nix`
- Modify: `labs/pratico/justfile`
- Modify: `labs/pratico/README.md`
- Modify: `labs/pratico/lab.json`
- Modify: `README.md` (root; the `## prático` block)
- Create: `labs/pratico/smoke/kokkos/CMakeLists.txt`, `labs/pratico/smoke/kokkos/smoke.cpp`, `labs/pratico/smoke/kokkos/smoke_test.cpp`

**Interfaces:**
- Produces: devShells `ww3` (Kokkos Serial+OpenMP, GTest, cmake, ninja, gdb, valgrind on PATH; `Kokkos_DIR`/`GTest_DIR` discoverable by `find_package`), `cuda` (x86_64-linux only; Kokkos with CUDA backend, `nvcc` on PATH, `CUDACXX` set), `pratico` (unchanged plus the same additions). Check `checks.<system>.kokkos-smoke`. Just recipes `kokkos-smoke`, `cuda`.
- Consumed by: every later task through `nix develop .../labs/pratico#ww3`.

- [ ] **Step 1: Clone and branch**

```bash
S=/tmp/claude-1000/-home-hoffmann-code-ww-lab/b245d1e4-6882-4bf7-b297-422ea8c9f8ba/scratchpad
git clone -q git@github.com:h0ffmann/nix-config.git "$S/nix-config" && cd "$S/nix-config" && git checkout -b feat/pratico-kokkos
```

- [ ] **Step 2: Add the Kokkos package variants to `labs/pratico/flake.nix`**

Inside the `let` block, after `ww3Toolchain`, add (nixpkgs' `kokkos` is Serial-only with tests on; the overrides below are the whole pin):

```nix
      # Kokkos from nixpkgs is Serial-only with its test suite on. ww3-gpu's kernels run
      # Serial + OpenMP on any host and CUDA on the owner's RTX 4090 / an H100, so pin
      # the same source with the backends turned on and the (slow) upstream tests off.
      kokkosHost = pkgs: pkgs.kokkos.overrideAttrs (o: {
        pname = "kokkos-openmp";
        cmakeFlags = [
          "-DKokkos_ENABLE_SERIAL=ON"
          "-DKokkos_ENABLE_OPENMP=ON"
          "-DKokkos_ENABLE_TESTS=OFF"
          "-DKokkos_ENABLE_DEPRECATED_CODE_4=OFF"
          "-DCMAKE_CXX_STANDARD=20"
        ];
        doCheck = false;
      });
      kokkosCuda = pkgs: (pkgs.kokkos.override { stdenv = pkgs.cudaPackages.backendStdenv; }).overrideAttrs (o: {
        pname = "kokkos-cuda";
        nativeBuildInputs = o.nativeBuildInputs ++ [ pkgs.cudaPackages.cuda_nvcc ];
        buildInputs = (o.buildInputs or []) ++ [ pkgs.cudaPackages.cuda_cudart pkgs.cudaPackages.cuda_cccl ];
        cmakeFlags = [
          "-DKokkos_ENABLE_SERIAL=ON"
          "-DKokkos_ENABLE_OPENMP=ON"
          "-DKokkos_ENABLE_CUDA=ON"
          "-DKokkos_ENABLE_CUDA_LAMBDA=ON"
          "-DKokkos_ARCH_ADA89=ON"    # RTX 4090
          "-DKokkos_ARCH_HOPPER90=ON" # H100
          "-DKokkos_ENABLE_TESTS=OFF"
          "-DCMAKE_CXX_STANDARD=20"
        ];
        doCheck = false;
      });
      kokkosTooling = pkgs: with pkgs; [ gtest gdb valgrind ];
```

Add `] ++ kokkosTooling pkgs ++ [ (kokkosHost pkgs) ]` to the `ww3` and `pratico` shell package lists (keep `cmake`, `ninja` already there). Extend `pkgsFor`'s predicate:

```nix
        config.allowUnfreePredicate = pkg: builtins.elem (lib.getName pkg) [
          "parmetis" "cuda_nvcc" "cuda_cudart" "cuda_cccl" "cuda_nvrtc" "libcublas" "cudatoolkit" "cuda-merged"
        ];
        config.cudaSupport = false; # only the #cuda shell pulls CUDA packages in
```

Add the `cuda` devShell (Linux x86_64 only) inside the `rec { ... }`:

```nix
          cuda = pkgs.mkShell ({
            name = "ww3-cuda";
            packages = ww3Toolchain pkgs ++ kokkosTooling pkgs ++ [
              (pkgs.python3.withPackages sciPython)
              (kokkosCuda pkgs)
              pkgs.cudaPackages.cuda_nvcc
              pkgs.cudaPackages.cuda_cudart
            ];
            CUDACXX = "${pkgs.cudaPackages.cuda_nvcc}/bin/nvcc";
            shellHook = ww3Banner + ''
              echo "kokkos: CUDA backend (Ada 8.9, Hopper 9.0) — binary cache: see labs/cuda setup-cuda-cache"
            '';
          } // ww3Env pkgs);
```

Guard it with `lib.optionalAttrs (system == "x86_64-linux") { cuda = ...; }` merged into the `rec` set (convert the `rec { ... }` to `let shells = rec {...}; in shells // lib.optionalAttrs ...`).

- [ ] **Step 3: Write the smoke project**

`labs/pratico/smoke/kokkos/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(pratico_kokkos_smoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Kokkos REQUIRED)
find_package(GTest REQUIRED)
add_executable(smoke smoke.cpp)
target_link_libraries(smoke PRIVATE Kokkos::kokkos)
add_executable(smoke_test smoke_test.cpp)
target_link_libraries(smoke_test PRIVATE Kokkos::kokkos GTest::gtest_main)
enable_testing()
add_test(NAME smoke_test COMMAND smoke_test)
```

`smoke.cpp`:

```cpp
// pratico smoke: Kokkos links, initialises, and runs one parallel_reduce on the default host backend.
#include <Kokkos_Core.hpp>
#include <cstdio>
int main(int argc, char** argv) {
  Kokkos::initialize(argc, argv);
  {
    const int n = 1 << 20;
    double sum = 0.0;
    Kokkos::parallel_reduce("smoke.sum", n, KOKKOS_LAMBDA(const int i, double& acc) { acc += 1.0 / (1.0 + i); }, sum);
    std::printf("kokkos %s backend=%s sum=%.6f\n", KOKKOS_VERSION_STRING, Kokkos::DefaultExecutionSpace::name(), sum);
  }
  Kokkos::finalize();
  return 0;
}
```

`smoke_test.cpp`:

```cpp
#include <Kokkos_Core.hpp>
#include <gtest/gtest.h>
class KokkosEnv : public ::testing::Environment {
 public:
  void SetUp() override { Kokkos::initialize(); }
  void TearDown() override { Kokkos::finalize(); }
};
static ::testing::Environment* const kokkos_env = ::testing::AddGlobalTestEnvironment(new KokkosEnv);
TEST(Smoke, ReduceMatchesClosedForm) {
  const int n = 1000;
  long long s = 0;
  Kokkos::parallel_reduce("smoke.arith", n, KOKKOS_LAMBDA(const int i, long long& acc) { acc += i; }, s);
  EXPECT_EQ(s, static_cast<long long>(n) * (n - 1) / 2);
}
```

- [ ] **Step 4: Add the flake check**

In `checks = forAll (pkgs: { ... })` add:

```nix
        kokkos-smoke = pkgs.stdenv.mkDerivation {
          name = "pratico-kokkos-smoke";
          src = ./smoke/kokkos;
          nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
          buildInputs = [ (kokkosHost pkgs) pkgs.gtest ];
          cmakeFlags = [ "-GNinja" ];
          doCheck = true;
          checkPhase = "ctest --output-on-failure && ./smoke | tee smoke.txt";
          installPhase = ''mkdir -p "$out"; cp smoke.txt "$out/"'';
        };
```

- [ ] **Step 5: justfile, README, lab.json**

Append to `labs/pratico/justfile`:

```make
# Kokkos + GoogleTest smoke project, configured/built/run in the sandbox (what CI runs).
kokkos-smoke:
    nix build --no-link --print-out-paths "{{justfile_directory()}}#checks.x86_64-linux.kokkos-smoke"

# Toolchain shell with Kokkos built for CUDA (RTX 4090 / H100). x86_64-linux only.
cuda:
    nix develop "{{justfile_directory()}}#cuda"
```

Extend the existing `toolchain` recipe to also print the Kokkos, GoogleTest and CMake versions, by running inside the `#ww3` shell:

```bash
cmake --version | head -1
cmake -P <(printf 'find_package(Kokkos REQUIRED)\nfind_package(GTest REQUIRED)\nmessage(STATUS "kokkos ${Kokkos_VERSION} gtest ${GTest_VERSION}")\n')
```

README `labs/pratico/README.md`: add a "Kokkos, GoogleTest, CMake" section listing the pins (Kokkos 5.2.2 Serial+OpenMP in `#ww3`, +CUDA in `#cuda`, GoogleTest 1.17, CMake 4.3), the `just kokkos-smoke` and `just cuda` recipes, and one sentence: "Consumer: [ww3-gpu](https://github.com/h0ffmann/ww3-gpu) builds its `kokkos/` tree in `#ww3` and `#cuda`." Root `README.md` `## prático` block: add `just kokkos-smoke` and `just cuda` lines. `lab.json` headline: add `"kokkos"`, `"gtest"`, `"cmake"`.

- [ ] **Step 6: Verify**

```bash
cd labs/pratico && nix flake check -L 2>&1 | tail -20      # toolchain + kokkos-smoke pass
nix develop .#ww3 --command bash -c 'cmake -P <(printf "find_package(Kokkos REQUIRED)\nfind_package(GTest REQUIRED)\nmessage(STATUS \"kokkos \${Kokkos_VERSION} gtest \${GTest_VERSION}\")\n")'
nix develop .#cuda --command nvcc --version | tail -2       # on the 4090 host only; skip elsewhere
```

Expected: check passes; the message prints `kokkos 5.2.2 gtest 1.17.0`.

- [ ] **Step 7: Commit and push the branch (no PR yet — Task 9 opens it referencing ww3-gpu)**

```bash
git add -A && git commit -m "feat(pratico): Kokkos (Serial/OpenMP, CUDA shell), GoogleTest, gdb, valgrind; kokkos-smoke check" && git push -u origin feat/pratico-kokkos
git rev-parse HEAD   # record: <NIXCONFIG_SHA>
```

- [ ] **Step 8: Pin ww3-gpu's submodule to that commit**

```bash
cd /home/hoffmann/code/ww-lab && git -C nix-config fetch --depth 1 origin feat/pratico-kokkos && git -C nix-config checkout -q FETCH_HEAD && git add nix-config && git commit -m "build: pin nix-config to labs/pratico with Kokkos and GoogleTest (feat/pratico-kokkos)"
```

---

### Task 2: `kokkos/` skeleton, intro programs, CI

**Files:**
- Create: `kokkos/CMakeLists.txt`, `kokkos/CMakePresets.json`, `kokkos/README.md`
- Create: `kokkos/cmake/CompilerWarnings.cmake`
- Create: `kokkos/intro/01_views.cpp`, `02_parallel_for.cpp`, `03_reduce_and_scan.cpp`, `04_layouts_and_mirrors.cpp`, `05_team_scratch.cpp`, `06_interop_bindc.cpp` + `06_interop_driver.F90`
- Create: `kokkos/tests/CMakeLists.txt`, `kokkos/tests/kokkos_env.hpp`, `kokkos/tests/L1_test_intro.cpp`
- Modify: `justfile` (new recipes), `.github/workflows/ci.yml` (new job `kokkos`)

**Interfaces:**
- Produces: CMake targets `ww_kokkos` (static lib, empty for now; Task 3 adds sources), `ww_intro_*` executables, `L1_test_intro`; presets `serial-debug`, `openmp-release`, `cuda-release`; just recipes `kokkos-configure preset`, `kokkos-build preset`, `kokkos-test preset`, `kokkos-clean`; CMake option `WW_ENABLE_FORTRAN` (default ON) and `WW_WW3_BUILD_DIR` (path to a WW3 build, default empty → WW3-linked targets skipped).
- Consumed by: Tasks 3–6.

- [ ] **Step 1: Top-level CMake**

`kokkos/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(ww3_gpu_kokkos VERSION 0.1.0 LANGUAGES C CXX Fortran)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(WW_ENABLE_FORTRAN "Build the Fortran interface module, reference and drivers" ON)
option(WW_DETERMINISTIC "Force serial reductions for bit-reproducible validation builds" OFF)
set(WW_WW3_BUILD_DIR "" CACHE PATH "A configured WW3 build directory (libww3.a + mod/); enables the ww3_lib cross-check target")
include(cmake/CompilerWarnings.cmake)
find_package(Kokkos REQUIRED)
find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NETCDF REQUIRED IMPORTED_TARGET netcdf)
if(WW_ENABLE_FORTRAN)
  pkg_check_modules(NETCDFF REQUIRED IMPORTED_TARGET netcdf-fortran)
endif()
add_library(ww_kokkos STATIC)
target_include_directories(ww_kokkos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ww_kokkos PUBLIC Kokkos::kokkos)
ww_apply_warnings(ww_kokkos)
if(WW_DETERMINISTIC)
  target_compile_definitions(ww_kokkos PUBLIC WW_DETERMINISTIC=1)
endif()
add_subdirectory(src/ww_kokkos)
if(WW_ENABLE_FORTRAN)
  add_subdirectory(src/fortran_iface)
endif()
add_subdirectory(intro)
add_subdirectory(tools/nccmp-tol)
enable_testing()
add_subdirectory(tests)
```

Create empty `src/ww_kokkos/CMakeLists.txt` (`target_sources(ww_kokkos PRIVATE real.hpp)` with a header-only `real.hpp` defining `namespace ww { using Real = float; }` and `KOKKOS_INLINE_FUNCTION` helpers `sqr`, `cube`) and `src/fortran_iface/CMakeLists.txt` (empty until Task 4; must not error). `tools/nccmp-tol/CMakeLists.txt` created empty here and filled in Task 5.

`cmake/CompilerWarnings.cmake`:

```cmake
function(ww_apply_warnings tgt)
  target_compile_options(${tgt} PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter>
    $<$<COMPILE_LANG_AND_ID:CXX,NVIDIA>:-Wall>
    $<$<COMPILE_LANGUAGE:Fortran>:-Wall -Wextra -fimplicit-none>)
endfunction()
```

- [ ] **Step 2: Presets**

`kokkos/CMakePresets.json` with `configurePresets` `serial-debug` (`binaryDir: build/serial-debug`, `CMAKE_BUILD_TYPE=Debug`, `CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer`, `CMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`, `Kokkos_ENABLE_DEBUG_BOUNDS_CHECK=ON`, `WW_DETERMINISTIC=ON`), `openmp-release` (`Release`, `-O3 -march=x86-64-v3`), `cuda-release` (`Release`, `CMAKE_CXX_COMPILER=nvcc_wrapper` is NOT needed with Kokkos 5 + CMake: set `CMAKE_CUDA_ARCHITECTURES=89;90`, `CMAKE_CXX_COMPILER=g++`, and rely on the CUDA-built Kokkos package exporting its compile options), each with `generator: Ninja`. `buildPresets` and `testPresets` of the same names; test presets set `output.outputOnFailure: true`. Note in the JSON `$comment` that the Kokkos backend is fixed by which pratico shell (`#ww3` vs `#cuda`) provides `Kokkos_DIR`.

- [ ] **Step 3: Intro programs (one concept each, ≤ 80 lines, heavy comments; each prints one line on success)**

- `01_views.cpp`: `View<float**>` allocation, extents, `deep_copy` to a host mirror, printing `view.label()`, `span_is_contiguous()`.
- `02_parallel_for.cpp`: fill a spectrum `E(ith, ik)` with a JONSWAP × cos² shape using `MDRangePolicy<Rank<2>>`; compute Hs = 4√(ΣE·dθ·dσ) with `parallel_reduce`; assert it matches the closed-form target within 2 %. The JONSWAP helper lives in `kokkos/src/ww_kokkos/spectrum_fixtures.hpp` (header-only, `KOKKOS_INLINE_FUNCTION Real jonswap(Real sigma, Real u10, Real fetch)`, `cos2_spread`), reused by tests.
- `03_reduce_and_scan.cpp`: `parallel_reduce` with a custom reducer (max Hs and its index) and `parallel_scan` for cumulative fetch.
- `04_layouts_and_mirrors.cpp`: `LayoutLeft` vs `LayoutRight` strides shown numerically; `create_mirror_view`; why the Fortran boundary needs `LayoutLeft`; an unmanaged view over a `std::vector<float>` buffer.
- `05_team_scratch.cpp`: `TeamPolicy` with `team_scratch(0)` holding one extended spectrum per team; the shape used by the DIA kernel.
- `06_interop_bindc.cpp` + `06_interop_driver.F90`: `extern "C" void ww_intro_scale(int n, float* x, float s)` scaling a Fortran array through an unmanaged `View<float*, LayoutLeft, HostSpace>`; Fortran main calls it and checks. Built as `ww_intro_06` only when `WW_ENABLE_FORTRAN`.

`intro/CMakeLists.txt`: `foreach` over the six, `add_executable(ww_intro_NN ...)`, link `ww_kokkos`; register each as a CTest (`add_test(NAME intro_NN COMMAND ww_intro_NN)`) so the programs are exercised in CI.

- [ ] **Step 4: Test infrastructure**

`tests/kokkos_env.hpp` = the `KokkosEnv` class from Task 1 Step 3 (with `Kokkos::InitializationSettings().set_num_threads(2)` when OpenMP). `tests/CMakeLists.txt`: a `ww_add_test(name)` function creating `add_executable(${name} ${name}.cpp)`, linking `ww_kokkos GTest::gtest_main`, `add_test`. `L1_test_intro.cpp`: three tests — JONSWAP integral Hs vs closed form (`EXPECT_NEAR(hs, 4*sqrt(m0), 0.02*hs)`), `cos2_spread` integrates to 1 over 2π within 1e-3, unmanaged `LayoutLeft` view over a Fortran-ordered buffer indexes `(ith, ik)` as `ith + ik*nth`.

- [ ] **Step 5: just recipes and CI**

Append to `justfile` under a new `# Kokkos (kokkos/)` section:

```make
kokkos_dir := justfile_directory() + "/kokkos"

# Configure kokkos/ with a preset: serial-debug (default), openmp-release, cuda-release (needs `just cuda` shell).
kokkos-configure preset="serial-debug":
    nix develop "{{pratico}}#ww3" --command cmake -S "{{kokkos_dir}}" --preset {{preset}}

# Build a preset.
kokkos-build preset="serial-debug": (kokkos-configure preset)
    nix develop "{{pratico}}#ww3" --command cmake --build "{{kokkos_dir}}" --preset {{preset}}

# Build and run ctest for a preset.
kokkos-test preset="serial-debug": (kokkos-build preset)
    nix develop "{{pratico}}#ww3" --command ctest --test-dir "{{kokkos_dir}}/build/{{preset}}" --output-on-failure

# Remove kokkos/build.
kokkos-clean:
    rm -rf "{{kokkos_dir}}/build"
```

For `cuda-release` the recipe must use `#cuda`: add `kokkos-cuda-test:` that runs the same three commands with `nix develop "{{pratico}}#cuda"`.

`.github/workflows/ci.yml`: add job `kokkos` (runs-on ubuntu-latest): `actions/checkout@v4` with `submodules: true`, `DeterminateSystems/nix-installer-action@main`, `DeterminateSystems/magic-nix-cache-action@main`, then `nix develop ./nix-config/labs/pratico#ww3 --command bash -c 'cd kokkos && cmake --preset serial-debug && cmake --build --preset serial-debug && ctest --preset serial-debug'` and the same for `openmp-release`. Keep `links`. The `lint` job loses the Python steps in Task 6.

- [ ] **Step 6: Verify and commit**

```bash
just kokkos-test serial-debug && just kokkos-test openmp-release
```

Expected: all intro programs and `L1_test_intro` pass under both presets. Commit: `feat(kokkos): CMake tree, presets, intro programs, test harness, CI job`.

---

### Task 3: `W3SNL1`/`INSNL1` port with Fortran reference and L1 parity

**Files:**
- Create: `kokkos/src/ww_kokkos/snl1_config.hpp` (parameters struct), `snl1_tables.hpp/.cpp`, `snl1_dia.hpp/.cpp`, `spectrum_fixtures.hpp` (from Task 2 if not yet), `fixture_io.hpp/.cpp` (binary fixture reader)
- Create: `kokkos/tests/fixtures/snl1_ref.F90` (verbatim reference), `kokkos/tests/fixtures/gen_snl1_fixture.F90` (driver), `kokkos/tests/fixtures/gen_snl1_ww3lib.F90` (optional cross-check against `ww3_lib`), `kokkos/tests/fixtures/CMakeLists.txt`, committed fixtures `kokkos/tests/fixtures/snl1_nk25_nth24_d{deep,50m,10m}.bin`
- Create: `kokkos/tests/L1_test_snl1_tables.cpp`, `kokkos/tests/L1_test_snl1_dia.cpp`
- Modify: `kokkos/src/ww_kokkos/CMakeLists.txt`, `kokkos/tests/CMakeLists.txt`

**Interfaces (exact):**

```cpp
namespace ww::snl1 {
struct Config {            // everything INSNL1/W3SNL1 read from W3GDATMD/W3ADATMD, in WW3 names
  int nk, nth;             // NK, NTH
  Real xfr;                // XFR  frequency increment factor
  Real dth;                // DTH  = 2π/NTH
  Real lam;                // LAM  (LAMBDA, default 0.25)
  Real snlc1;              // NLPROP / g^4
  Real kdcon, kdmn;        // KDCONV=0.75, KDMIN=0.50
  Real snls1, snls2, snls3;// 5.5, 0.833, -1.25
  Real fachfe;             // XFR**(-FACHF)
  // sig(1..nk) is passed separately (View); TPIINV = 1/(2π) from constants.F90
};
struct Tables {            // outputs of INSNL1, all device Views, LayoutLeft, 1-based logic mapped to 0-based storage
  int nfr, nfrhgh, nfrchg, nspecx, nspecy, nspec;
  Real dal1, dal2, dal3;
  Real awg[8], swg[8];     // AWG1..8, SWG1..8
  Kokkos::View<int*>  ip[2][4], im[2][4];   // IP11..IP14, IP21..IP24 / IM11..IM14, IM21..IM24  (size nspecx)
  Kokkos::View<int*>  ic[8][2];             // IC11,IC12 ... IC81,IC82                          (size nspec)
  Kokkos::View<Real*> af11;                 // size nspecx
};
Tables make_tables(const Config& c, Kokkos::View<const Real*, Kokkos::HostSpace> sig);   // INSNL1, lines 483-779
// One kernel launch over npts points. a, s, d are (nspec, npts) LayoutLeft device views; cg is (nk, npts); kdmean is (npts).
void snl1(const Config& c, const Tables& t,
          Kokkos::View<const Real*> sig,                 // device copy, size nk
          Kokkos::View<const Real**, Kokkos::LayoutLeft> a,
          Kokkos::View<const Real**, Kokkos::LayoutLeft> cg,
          Kokkos::View<const Real*> kdmean,
          Kokkos::View<Real**, Kokkos::LayoutLeft> s,
          Kokkos::View<Real**, Kokkos::LayoutLeft> d);   // W3SNL1, lines 115-473
}
```

Index convention: Fortran `ISP = ITH + (IFR-1)*NTH` (1-based) ↔ C++ `isp = ith + ifr*nth` (0-based); the tables store 0-based indices; Fortran extended arrays `UE(1-NTH:NSPECY)` become scratch of size `nspecy + nth` with offset `nth` (so Fortran index `j` → scratch `j - 1 + nth`).

Fixture binary format (little-endian, written by Fortran `ACCESS='STREAM'`): header `int32 magic=0x534E4C31 ('SNL1'), int32 nk, nth, npts; float32 xfr, dth, lam, snlc1, kdcon, kdmn, snls1, snls2, snls3, fachfe; float32 sig(nk)`; then `int32 nfr, nfrhgh, nfrchg, nspecx, nspecy; float32 dal1..3, awg(8), swg(8); int32 ip11(nspecx) … im24(nspecx) [16 arrays in INSNL1 order]; int32 ic11, ic21, ic31, ic41, ic51, ic61, ic71, ic81, ic12, ic22, ic32, ic42, ic52, ic62, ic72, ic82 (nspec each); float32 af11(nspecx)`; then per point: `float32 kdmean, cg(nk), a(nspec), s(nspec), d(nspec)`. Indices are stored 1-based exactly as Fortran holds them; `fixture_io` subtracts 1.

- [ ] **Step 1: Write the Fortran reference `snl1_ref.F90`**

A module `SNL1_REF` with module variables named as in WW3 (`NK, NTH, NSPEC, XFR, DTH, LAM, SIG(:), FACHFE, KDCON, KDMN, SNLC1, SNLS1, SNLS2, SNLS3, TPIINV`, and every `W3ADATMD` table) and two subroutines `INSNL1_REF()` and `W3SNL1_REF(A, CG, KDMEAN, S, D)` whose bodies are the verbatim text of `WW3/model/src/w3snl1md.F90` lines 340–402 (W3SNL1 sections 1–4) and 590–779 (INSNL1 sections 1–9) with only these edits: `USE` lines removed, `W3DMNL` call replaced by `ALLOCATE` of the tables (`IP11(NSPECX)…`, `IC11(NSPEC)…`, `AF11(NSPECX)`), `#ifdef` test output removed. Header comment states the source lines and WW3 version. Also a subroutine `SETUP_REF(NK_IN, NTH_IN, XFR_IN, FREQ1)` that sets `SIG(IK) = TPI*FREQ1*XFR**(IK-1)`, `DTH = TPI/NTH`, `LAM=0.25`, `SNLC1 = 2.5E7/GRAV**4` (NLPROP for ST4 default, `w3gridmd.F90` line ~1889; note in the header which one), `KDCON=0.75, KDMN=0.50, SNLS1=5.5, SNLS2=0.833, SNLS3=-1.25`, `FACHFE = XFR**(-5.)` (FACHF=5 default (v) `w3gridmd.F90`: check the default `FACHF` value with `grep -n "FACHF *=" WW3/model/src/w3gridmd.F90` and record it).

- [ ] **Step 2: Write the driver `gen_snl1_fixture.F90`**

Program: `SETUP_REF(25, 24, 1.1, 0.04118)`, `INSNL1_REF`, then for three depths `(1000., 50., 10.)` build a JONSWAP(U10=10 m/s, fetch 100 km, γ=3.3) × cos² spectrum in action form `A(ISP) = E(σ,θ)/σ`, group velocity `CG(IK)` from the linear dispersion relation at that depth (solve `σ² = g k tanh(kd)` by Newton, 20 iterations), `KDMEAN` = mean kd over the spectrum weighted by energy, call `W3SNL1_REF`, write the record. Output path from `argv(1)`; three files or one file with `npts=3`: write **one file with npts = 3**. Deterministic: no random numbers.

- [ ] **Step 3: Build and generate fixtures**

`tests/fixtures/CMakeLists.txt`: `add_executable(gen_snl1_fixture snl1_ref.F90 gen_snl1_fixture.F90)`; custom target `snl1-fixtures` running it into `${CMAKE_CURRENT_SOURCE_DIR}/snl1_nk25_nth24.bin`. Run `just kokkos-build serial-debug && ./kokkos/build/serial-debug/tests/fixtures/gen_snl1_fixture kokkos/tests/fixtures/snl1_nk25_nth24.bin` and commit the file (expected size ≈ 3 × (1+25+3×600)×4 + tables ≈ 80 KB; acceptable).

- [ ] **Step 4: Failing tests first**

`L1_test_snl1_tables.cpp`: load the fixture with `ww::fixture::load("snl1_nk25_nth24.bin")` (path from `WW_FIXTURE_DIR` compile definition set in `tests/CMakeLists.txt`), build `Config` and `sig` from its header, call `make_tables`, then `EXPECT_EQ` every integer scalar and every element of the 32 index tables (after the fixture's `-1`), `EXPECT_NEAR` for `dal*`, `awg`, `swg` (1e-6 relative) and `af11` (1e-6 relative; values are large, compare relative).

`L1_test_snl1_dia.cpp`: tests `MatchesFortranReference` (for each of the 3 points, `s` and `d` within `1e-5` relative of the fixture, with an absolute floor `1e-30`), `ZeroSpectrumGivesZeroSource`, `ActionConservation` (Σ over bins of `s(isp)/con(isp)`-weighted … keep it simple and correct: the DIA conserves total *energy* only approximately on a discrete grid; instead test *scaling*: doubling `a` multiplies `s` by 8 within 1e-4, since Snl is cubic in the spectrum), `OpenMPMatchesSerial` (only meaningful under the OpenMP preset: run twice and compare bitwise; under Serial it is trivially true).

Run: `just kokkos-test serial-debug` → the two new tests fail to link (`make_tables` undefined).

- [ ] **Step 5: Implement `snl1_tables.cpp`**

Translate INSNL1 sections 1–9 into `make_tables`: compute scalars exactly as Fortran (`std::acos`, `std::asin`, `std::log`, `std::pow` on `float`, same expression order), fill host mirrors of the tables with the same loops, `deep_copy` to device, return. Comment each block with the Fortran section number.

- [ ] **Step 6: Implement `snl1_dia.cpp`**

`TeamPolicy<>(npts, Kokkos::AUTO)` with per-team scratch (level 0) sized for `UE(nspecy+nth)`, `SA1, SA2, DA1C, DA1P, DA1M, DA2C, DA2P, DA2M (nspecx+nth)`, `CON(nspec)` floats. Section 1 (`X, X2, CONS`) computed per team by one thread and broadcast via scratch scalar or recomputed by all (recompute; it is 3 flops). Section 2 with `TeamThreadRange` over `ifr` and `ThreadVectorRange` over `ith` for `UE`, `CON`; the `NFR+1..NFRHGH` extension loop is sequential in `ifr` (it reads `UE(ISP-NTH)` from the previous row) — do it as `Kokkos::single(PerTeam)` loop over `ifr` with a vector loop over `ith` and `team_barrier()` between rows. Section 3 `parallel_for(TeamThreadRange(team, nspecx))` — pure gather. Section 4 `TeamThreadRange(team, nspec)` writing `s`, `d`. `team_barrier()` between sections. Under `WW_DETERMINISTIC` nothing changes (no reductions in this kernel; note that in a comment). Kernel label `"srce.snl1.dia"`.

- [ ] **Step 7: Tests pass**

`just kokkos-test serial-debug && just kokkos-test openmp-release` → all pass. If `MatchesFortranReference` fails at ~1e-4, the usual culprits: `XFR**IFRP` with negative `IFRM` (use `std::pow(xfr, (float)ifrm)`), integer division in `IFR = 1 + (ISP-1)/NTH`, the `MAX(0, …)` clamps in `IF3..IF6`, and the 1-based/0-based offset of the extended arrays.

- [ ] **Step 8: Optional cross-check against `ww3_lib` (only if `WW_WW3_BUILD_DIR` is set)**

`gen_snl1_ww3lib.F90`: uses `W3GDATMD` (`W3NMOD(1,6,6)`, `W3DIMS(1,NK,NTH,6,6)`, `W3SETG(1,6,6)`, set `XFR, SIG, DTH, LAM, SNLC1, KDCON, KDMN, SNLS1..3, FACHFE` through the module pointers), `W3ADATMD` (`W3NAUX(6,6)`, `W3SETA(1,6,6)`), calls `INSNL1(1)` then `W3SNL1` on the same three spectra and writes the same record format to a second file. CMake: `if(WW_WW3_BUILD_DIR)` add the executable linking `${WW_WW3_BUILD_DIR}/src/libww3.a` with `target_include_directories(... ${WW_WW3_BUILD_DIR}/src/mod)` plus MPI/NetCDF as the WW3 switch requires; just recipe `l1-crosscheck ww3=ww3_src` that builds WW3 (`just build`), configures with `-DWW_WW3_BUILD_DIR`, runs both generators and `cmp`s the files. Document the expected result (bit-identical) in `PORT_STATUS.md` (Task 4). If linking `libww3.a` standalone fails because of unresolved switch-dependent symbols, keep the target but mark it `⚠ needs a static-link investigation` in `kokkos/README.md`; the standalone reference remains the committed fixture source.

- [ ] **Step 9: Commit**

`feat(kokkos): W3SNL1/INSNL1 DIA kernel on Kokkos with Fortran reference fixtures and L1 parity tests`.

---

### Task 4: `bind(C)` shim, Fortran interface module, patch note, port ledger

**Files:**
- Create: `kokkos/src/fortran_iface/ww_kokkos_c.hpp` (C API declarations), `snl1_shim.cpp`, `w3kokkosmd.F90`, `PATCH.md`, `CMakeLists.txt` (filled), `kokkos/tests/L1_test_shim.cpp`, `kokkos/tests/fixtures/shim_driver.F90` (+ CTest `shim_roundtrip`)
- Create: `kokkos/PORT_STATUS.md`

**Interfaces (exact C API):**

```c
// ww_kokkos_c.hpp — all extern "C", all arrays Fortran (column-major) order
int  ww_kokkos_init(int comm_f);                  // comm_f: MPI_Comm_c2f handle or -1; returns 0
void ww_kokkos_finalize(void);
int  ww_snl1_init(int nk, int nth, float xfr, float dth, float lam, float snlc1, float kdcon, float kdmn,
                  float snls1, float snls2, float snls3, float fachfe, const float* sig);   // builds Tables once
void ww_snl1(int npts, const float* a, const float* cg, const float* kdmean, float* s, float* d);
                  // a,s,d: (nspec, npts); cg: (nk, npts). Phase 1: copy-in, kernel, copy-out.
int  ww_snl1_enabled(void);                        // 1 if env WW_KOKKOS_SNL1=1 at ww_kokkos_init, else 0
```

Fortran module `W3KOKKOSMD` (file `w3kokkosmd.F90`): `INTERFACE` blocks for the five functions with `ISO_C_BINDING`, plus `LOGICAL :: KOKKOS_SNL1` set in `W3KOKKOS_SETUP()` from `ww_snl1_enabled()`.

- [ ] **Step 1: Failing test** — `L1_test_snl1_shim.cpp`: call `ww_snl1_init` with the fixture header, `ww_snl1` on the 3 points through raw pointers, compare with the fixture (1e-5 relative), and `shim_roundtrip` (Fortran program `shim_driver.F90` using `W3KOKKOSMD`, calling the same on the JONSWAP setup from `snl1_ref`, comparing against `W3SNL1_REF` and stopping with non-zero status on mismatch > 1e-5). Run → link failures.

- [ ] **Step 2: Implement** `snl1_shim.cpp`: a file-static `struct Ctx { Config cfg; Tables tables; View<Real*> sig_d; bool enabled; }`; `ww_kokkos_init` initialises Kokkos once (`Kokkos::is_initialized()` guard; device id `rank % ndevices` when `comm_f >= 0` — read `WW_KOKKOS_DEVICE_ID` env instead of MPI to keep MPI out of the shim in phase 1, documented), reads `WW_KOKKOS_SNL1`; `ww_snl1` wraps the pointers in unmanaged `LayoutLeft` host views, `deep_copy` into persistent device views (grown on demand), runs `ww::snl1::snl1`, `fence`, copies out.

- [ ] **Step 3: Tests pass** under both presets. Commit `feat(kokkos): bind(C) shim and W3KOKKOSMD interface for the DIA kernel`.

- [ ] **Step 4: `PATCH.md`** — the exact diff to apply on a WW3 fork branch: in `model/src/w3srcemd.F90`, where `W3SNL1` is called under `#ifdef W3_NL1` (find it with `grep -n "CALL W3SNL1" WW3/model/src/w3srcemd.F90` and quote the line numbers), wrap: `IF (KOKKOS_SNL1) THEN; CALL WW_SNL1(1, SPEC, CG(1:NK), KDMEAN, VSNL, VDNL); ELSE; CALL W3SNL1(...); END IF` behind `#ifdef W3_KOKKOS`; add `w3kokkosmd.F90` to `model/src/cmake/src_list.cmake` under a `KOKKOS` switch and `"KOKKOS"` to `switches.json`; `W3INIT` calls `WW_KOKKOS_INIT(-1)` (or the communicator) and `W3KOKKOS_SETUP`; link `ww_kokkos` in `model/src/CMakeLists.txt` when `-DWW_KOKKOS=ON`. Mark clearly: "not applied in this repo; `just src-pr`/fork branch work".

- [ ] **Step 5: `PORT_STATUS.md`** — table `Routine | WW3 file:lines | Phase | Shim | L1 parity | L2 replay | Serial ms | OpenMP ms | CUDA ms | Notes` with the `W3SNL1` row filled from a timing run: add `kokkos/tests/bench_snl1.cpp` (not a test; `ww_bench_snl1` executable timing 1 000 points × 20 calls, printing ms per call) and record Serial and OpenMP numbers from the owner's machine (or the CI runner, labelled). L2 column: "pending fork branch (PATCH.md)". Commit `docs(kokkos): PATCH.md for the WW3 caller and PORT_STATUS ledger`.

---

### Task 5: `nccmp-tol` comparator, L2 replay, profiling recipes

**Files:**
- Create: `kokkos/tools/nccmp-tol/CMakeLists.txt`, `nccmp_tol.cpp` (main), `compare.hpp/.cpp` (library: read variables, compute stats), `tolerances.hpp/.cpp` (parser), `tolerances.txt` (default), `README.md`
- Create: `kokkos/tests/L1_test_nccmp_tol.cpp` (uses the library + netcdf-c to write temp files), `kokkos/tests/L2_replay.sh`, `kokkos/tools/profile/gprof_table.sh`, `kokkos/tools/profile/perf_table.sh`, `kokkos/tools/profile/README.md`

**Interfaces:**

```cpp
namespace ww::nccmp {
struct Tolerance { std::string name; double abs; double rel; };
std::vector<Tolerance> parse_tolerances(std::istream&);      // "name abs rel", '#' comments, blank lines
struct Stats { std::string name; std::size_t n; double max_abs, rms, max_rel; bool judged, pass; };
std::vector<Stats> compare_files(const std::string& ref, const std::string& test, const std::vector<Tolerance>&);
                    // every numeric variable present in both files; NaN/_FillValue excluded; judged only if listed
std::string format_table(const std::vector<Stats>&);          // fixed-width text table
}
```
CLI: `nccmp-tol REF TEST [TOLERANCES]`, exit 0 iff every judged variable passes, 1 otherwise, 2 on I/O error. Default tolerances file content:

```
# variable  abs      rel     (a value passes if |d| <= abs OR |d|/max(|ref|,eps) <= rel)
hs          1e-4     1e-4
fp          1e-4     1e-4
dir         1e-2     1e-4
dp          1e-2     1e-4
t0m1        1e-4     1e-4
```

- [ ] **Step 1: Failing tests** — `L1_test_nccmp_tol.cpp`: `ParsesTolerances` (3 rows, comment skipped), `IdenticalFilesPass` (write `hs(time=2,y=3,x=4)` twice via netcdf-c, expect all pass), `PerturbationBeyondRelFails` (multiply one value by 1.01 → `hs` fails, exit-style bool false), `FillValuesIgnored`, `UnlistedVariableReportedNotJudged`. Run → link failure.
- [ ] **Step 2: Implement** with netcdf-c (`nc_open`, `nc_inq_nvars`, `nc_inq_var`, `nc_get_var_double`, `_FillValue` via `nc_inq_var_fill`), template over numeric types by reading as double. CMake target `nccmp_tol_lib` (static) + `nccmp-tol` executable, link `PkgConfig::NETCDF`.
- [ ] **Step 3: Tests pass; commit** `feat(kokkos): nccmp-tol per-field NetCDF comparator with tolerance file`.
- [ ] **Step 4: `L2_replay.sh`** — `L2_replay.sh <ww3-dir> <regtest> [switch]`: builds nothing; requires `just rt <regtest>` done; copies `<ww3-dir>/regtests/<regtest>/work_lab` to `work_a` and `work_b`, runs `ww3_shel` in each with `WW_KOKKOS_SNL1=0` and `=1`, runs `ww3_ounf`, then `nccmp-tol work_a/ww3.nc work_b/ww3.nc` with the default tolerances and appends the table to `kokkos/PORT_STATUS.md` under an "L2 replays" heading. shellcheck-clean. Just recipe `l2 test="ww3_ts1" ww3=ww3_src`. Document that without the PATCH applied both runs are identical and the script proves the harness, not the kernel.
- [ ] **Step 5: Profiling recipes** — `gprof_table.sh <ww3-dir> <regtest>`: rebuilds WW3 with `-pg` via `just build ... Debug` env `FFLAGS=-pg` (document the CMake variable used by WW3's build: check `WW3/model/CMakeLists.txt` for `CMAKE_Fortran_FLAGS` handling), runs the regtest, `gprof -b` → awk into `routine | self % | cumulative %` for the top 25, grouped by phase using a name map (`w3srce*→source terms`, `w3pro*|w3uqck*→propagation`, `w3gath|w3scat|mpi_→communication`, `w3io*→I/O`). `perf_table.sh`: same table from `perf record -g` + `perf report --stdio` when `perf` is available (it is not in pratico; say so). Just recipe `profile test="ww3_tp1.1" ww3=ww3_src`. Commit `feat(tools): L2 replay harness and profiling tables`.

---

### Task 6: Remove Python from the lab code

**Files:**
- Delete: `bench/make_bench_case.py`, `examples/01-fetch-limited-growth/{make_inputs.py,analyse.py}`, `examples/02-regional-real-forcing/{get_era5.py,make_bathy.py}`, `exercises/ex0*.py`, `exercises/solutions/ww3lab.py`
- Create: `bench/make_bench_case.cpp`, `bench/CMakeLists.txt` (or add to `kokkos/tools`; choose: `kokkos/tools/bench_case/` and keep `bench/` as scripts + Fortran), `examples/01-fetch-limited-growth/make_inputs.F90`, `examples/01-fetch-limited-growth/analyse.cpp` (→ `kokkos/tools/fetch_analyse/`), `examples/02-regional-real-forcing/get_gfs.sh`, `examples/02-regional-real-forcing/make_bathy.F90`, `exercises/README.md` (rewritten), `exercises/ex09_bench.md … ex13_port.md`, `exercises/solutions/{ex09_matrix.sh, ex10_profile.sh, ex11_refactor.F90, ex11_refactor_test.F90, ex12_reduce.cpp, ex13_compare.sh}`
- Modify: `bench/README.md`, `bench/run_all.sh`, `bench/Makefile` (drop the Python call), `examples/*/README.md`, `examples/*/run.sh`, `justfile`, `README.md`, `.github/workflows/ci.yml` (lint job), `.gitignore`

**Interfaces:**
- `ww_bench_case --size small|medium|large --nx N --ny N --nk N --nth N --hours H -o DIR` writes `depth.inp`, `mask.inp`, `namelists.nml`, `ww3_grid.nml`, `ww3_shel.nml`, `ww3_ounf.nml`, `case.json` — byte-identical output to the Python version for `--size small` (keep the old script's output as a fixture under `kokkos/tests/fixtures/bench_small/` and test equality).
- `ww_fetch_analyse ww3.nc [u10]` prints the same table as `analyse.py` (no plot).
- `make_inputs` (Fortran, no args) writes `depth.inp`, `mask.inp` for example 01; `make_bathy gebco.nc` (Fortran, netcdf-fortran, nearest-neighbour sampling; document the downgrade from bilinear) writes `bathy.inp`, `mask.inp`.
- `get_gfs.sh YYYYMMDD HH` downloads GFS 0.25° 10 m winds for the example box from NOMADS (`filter_gfs_0p25.pl` with `leftlon=-52&rightlon=-44&toplat=-24&bottomlat=-32`, `var_UGRD=on&var_VGRD=on&lev_10_m_above_ground=on`), 0–192 h every 3 h, concatenates with `grib_copy`/`cdo mergetime`, converts with `grib_to_netcdf` → `gfs_winds.nc`; `ww3_prnc_wind.nml` updated to the GFS variable names (`10u`/`10v` become `u10`/`v10` after `grib_to_netcdf`; verify with `ncdump -h` and adjust `FORCING%FIELD`/`FILE%VAR`).

- [ ] **Step 1: bench case generator** — before deleting, run the Python once inside `nix develop <pratico>#ww3` (python is there) to produce the fixture: `python3 bench/make_bench_case.py --size small -o kokkos/tests/fixtures/bench_small`. Write `L1_test_bench_case.cpp` that runs the generator library function `ww::bench::write_case(dir, params)` into a temp dir and compares each file byte-for-byte with the fixture. Implement `bench_case.hpp/.cpp` + `main.cpp` in `kokkos/tools/bench_case/` (CLI parsing by hand, no deps); same CFL formulas, same string formatting (`%.1f`, JSON by hand). Test passes; delete the `.py`; update `bench/run_all.sh`, `bench/Makefile`, `bench/README.md`, `justfile` `bench` recipe (`just kokkos-build openmp-release` first, then `kokkos/build/openmp-release/tools/bench_case/ww_bench_case`).
- [ ] **Step 2: example 01** — `make_inputs.F90` (61×5, 250 m, mask `0` then `1`s) and `ww_fetch_analyse` (`kokkos/tools/fetch_analyse/`, netcdf-c, reads `hs(time,y,x)`, x-name detection `x`/`longitude`, centre row, Kahma & Calkoen 1992 and Pierson–Moskowitz as in the Python, same table). `run.sh`: compile `make_inputs.F90` with `gfortran` into `./make_inputs` if missing, run it, then the WW3 programs, then `ww_fetch_analyse`. README updated. Delete the two `.py`.
- [ ] **Step 3: example 02** — `get_gfs.sh`, `make_bathy.F90`, README (GEBCO instructions kept; ERA5 note removed), `run.sh` updated, `ww3_prnc_wind.nml` variable names. Delete the two `.py`.
- [ ] **Step 4: exercises** — new README ("Exercises for lessons 09–13") and five exercise sheets with solutions: ex09 compile-option matrix (`solutions/ex09_matrix.sh` builds `switch_lab_shrd` with `-O2` and `-O3 -march=native` and 1/2/4 OpenMP threads, runs `ww3_tp1.1`, tabulates wall-clock and `nccmp-tol` verdicts); ex10 profile table (`gprof_table.sh`); ex11 refactor a WW3-style routine (`ex11_refactor.F90` holds a 40-line "automatic array + implicit interface" routine and its modern twin; `ex11_refactor_test.F90` compares them on random input to 1e-6, built with gfortran); ex12 Kokkos `parallel_reduce` computing Hs from a spectrum view (`ex12_reduce.cpp`, built against `ww_kokkos` via a small `CMakeLists.txt` in `exercises/solutions/`); ex13 run `L2_replay.sh` on `ww3_ts1` and read the table (`ex13_compare.sh`). Delete old `.py` files and `solutions/README.md` content about pyww3.
- [ ] **Step 5: CI and repo docs** — `ci.yml` lint job: remove `setup-python` and the Python steps; "Benchmark case generator runs" becomes a `ctest -R bench_case` inside the `kokkos` job. `README.md`: layout table (add `kokkos/`, drop `exercises` Python mention, `just` sections), stance sentence ("Lab code is C++, Fortran and shell; Python only in the publishing pipeline"), proposal link. `.gitignore`: `kokkos/build/`, `exercises/solutions/build/`.
- [ ] **Step 6: Verify** — `git ls-files '*.py'` prints exactly `scripts/book_prep.py` and `scripts/translate_md.py`; `just kokkos-test serial-debug` passes; `bash -n` and `shellcheck` on every `.sh`. Commit in three parts: `refactor(bench): C++ benchmark case generator replaces Python`, `refactor(examples): Fortran/C++/shell inputs and analysis replace Python`, `refactor(exercises): exercises for lessons 09-13 in shell/Fortran/C++`.

---

### Task 7: Lessons 00–08 revised; 14 and 15 renumbered and updated

**Files:**
- Modify: `course/00-orientation.md`, `01-build.md`, `03-grids.md`, `04-forcing.md`, `06-output.md`, `07-physics-choices.md`, `08-python.md`
- Rename: `course/10-ww4-and-the-future.md` → `course/14-ww4-and-the-future.md` (git mv), `course/11-swan.md` → `course/15-swan.md`; `course/09-gpu-and-performance.md` is **deleted** (its content is redistributed to 09/10/11 in Task 8; keep the RTX 4090 Q&A paragraph and the Ikuyajolu summary in the new 09 and 11)
- Modify: `course/14-ww4-and-the-future.md`

- [ ] **Step 1: 00** — add the paragraph after "The one equation" table: the second half of the course (09–13) follows the proposal's ladder: compile options → run configuration → modern Fortran → C++/Kokkos kernels, each gated by parity; link `pubs/proposal/pt/` and `pubs/proposal/mapas-mentais.pt.md`.
- [ ] **Step 2: 01** — add "Building inside the pinned toolchain" (`just ww3`, `just build`, what pratico pins, `just toolchain`), and "Switch files are compile options" with the `switch_lab_shrd` contents explained line by line and a pointer to lesson 09.
- [ ] **Step 3: 03, 04, 06** — replace every Python snippet: 03 uses `make_inputs.F90` / `make_bathy.F90`; 04 replaces the ERA5 section by GFS via `get_gfs.sh` + ecCodes (keep a one-line note that ERA5 needs the CDS Python client, so this repo does not use it); 06 replaces the `wavespectra` section with `ncdump`/`ncks` reading, the `ww_fetch_analyse` table, and "compare two runs with `nccmp-tol`".
- [ ] **Step 4: 07** — add a closing section "Why `W3SNL1` is the first kernel to port": per-point, table-driven, no I/O, dominant share, deterministic gather; link lesson 12.
- [ ] **Step 5: 08** — rewrite as "The Python ecosystem, and why this repo does not depend on it" (≤ 60 lines): the tool table stays, pyww3's design summary stays as prose, remove all code, add "what we use instead" table (namelists by hand or Fortran generators, `ww_bench_case` for sweeps, netcdf-c tools, `nccmp-tol`). No exercises reference.
- [ ] **Step 6: 14 (WW4)** — update the status block with the 2026-09-15 facts (repo created 2025-11-17; 36 commits; no releases/tags; last push 2026-08-19; Phase II done, NCEP Office Note 528; `src/ww4_core` init/wave/finalize + utils + `ww4_standalone`; tests four levels, GoogleTest, L1/L2 rewritten to full coverage 2026-07-22 in PR #50, L3/L4 absent; PR #66 CMake-only build open since 2026-08-21; issue #43 CPU–GPU architecture, Kokkos proposed; 35 open design issues), replace the phase table's "⚠ past the Phase IV date" note with what actually happened, and add "first public release: expected January 2027 per the proposal's advisor communication ⚠ no cited NOAA source; ON 525 said summer 2027". Add a short section "What WW4's test levels mean for a WW3 port" linking lesson 12.
- [ ] **Step 7: 15 (SWAN)** — renumber only; fix internal links.
- [ ] **Step 8: Verify** `nix develop . --command python3 scripts/book_prep.py course /tmp/bp` exits 0 (no dangling lesson links). Commit `docs(course): lessons 00-08 revised for the C++/Fortran lab; WW4 lesson updated to Sept 2026; renumbering`.

---

### Task 8: Lessons 09–13 and the course index

**Files:**
- Create: `course/09-benchmark-profile-compile-run.md`, `course/10-modern-fortran-refactoring.md`, `course/11-kokkos-and-modern-cpp.md`, `course/12-porting-a-kernel-w3snl1.md`, `course/13-bulk-porting-with-agents.md`
- Modify: `course/README.md`

Each lesson 100–200 lines, same voice as 00–07, every code path real. Required content:

- [ ] **Step 1: 09** — "Measure first": the reference run (`just rt`), `bench/` (`ww_bench_case`, `bench_ww3_cpu.sh`, `run_all.sh`), the metric (wall-clock per forecast hour), `gprof_table.sh` output shape and the phase buckets, WW3's matrix as a bit-for-bit gate (`matrix.base`, `matrix.comp`, `rstrt_b4b`/`nth_b4b`/`npl_b4b` variants (v)), the compile-option matrix (compiler flags, `SHRD`/`DIST`/`OMPG`/`OMPH` switches, `NC4`), MPI×OpenMP layout on one node, run-configuration knobs one at a time (`DTMAX/DTXY/DTKTH/DTMIN`, output frequency and field list, restart), and "when reordering breaks b4b: use `nccmp-tol`". Include the RTX 4090 Q&A from old lesson 09 as a boxed aside ("the GPU question, short version"), pointing to 11–13.
- [ ] **Step 2: 10** — the Fortran step: what a profile-top WW3 routine looks like (quote `W3SNL1`'s local arrays `UE(1-NTH:NSPECY)` etc. (v)), explicit interfaces, `contiguous`, `intent`, `pure`, `do concurrent` (with `gpu/02_do_concurrent.f90` as the example and the `LOCAL()` caveat), killing automatic arrays, the runtime switch pattern (same binary, two paths), per-routine parity test (`exercises/solutions/ex11_refactor_test.F90`), and directives (`gpu/00_hello_acc.f90`, Ikuyajolu's 1.3× and why: host↔device traffic) as the contrast that motivates Kokkos.
- [ ] **Step 3: 11** — Kokkos for Fortran people: execution and memory spaces, `View` (rank, layout, `LayoutLeft` = Fortran order), mirrors and `deep_copy`, `parallel_for`/`reduce`/`scan`, `MDRangePolicy`, `TeamPolicy` and scratch, `KOKKOS_LAMBDA` capture rules ("no host memory in kernels"), fences, `float` vs `double`, RAII/`const`/no raw `new`, `bind(C)` interop with an unmanaged view (`kokkos/intro/06`), building with the presets (`just kokkos-test`), CUDA on the 4090 (`just kokkos-cuda-test`, `Kokkos_ARCH_ADA89`). Each concept points to `kokkos/intro/0N_*.cpp`.
- [ ] **Step 4: 12** — the port walk-through: Fortran source map (INSNL1 sections 1–9, W3SNL1 sections 1–4, with line numbers (v)), the `Config`/`Tables` structs, index conventions (1-based ↔ 0-based, extended arrays), kernel structure (team per point, scratch, four sections and barriers), the shim and `W3KOKKOSMD`, fixtures (`snl1_ref.F90` verbatim, generator, binary format), L1 tests and their tolerances (why 1e-5 relative for float32), the optional `ww3_lib` cross-check, `PATCH.md` (what changes in the WW3 caller, and why the fork carries it), `L2_replay.sh`, the timing table in `PORT_STATUS.md`, and the definition of done (spec §4 / AGENTS_KOKKOS §1.5).
- [ ] **Step 5: 13** — bulk porting with agents: the FESOM2 recipe (Koldunov et al. 2026: two stages, literal translation, validation ladder, weeks not years), what a port PR contains, the ranked port list and phases from `docs/AGENTS_KOKKOS_202609.md` §2–3 (summarised, linked), the data-residency ladder, the operation decision (gain measured on the operational case + parity report → LabECO decides), prompt patterns and anti-patterns for agents, the WW4 relationship (do not compete: build artefacts in WW4's L1/L2 shape), and the proposal's mind maps 2, 3, 5, 6 reproduced as Mermaid in English.
- [ ] **Step 6: `course/README.md`** — the 16-row table with "You'll be able to" outcomes; update the reading-order paragraph (09–13 are the ladder, do them with `kokkos/` open; 14 and 15 stand alone).
- [ ] **Step 7: Verify** — `just book` builds (`--fail-if-warnings`); `python3 scripts/book_prep.py course /tmp/bp` exit 0; `grep -rn "\.py" course/` shows only lesson 08's tool table and the two publishing scripts if mentioned. Commit `docs(course): lessons 09-13 — benchmark, Fortran, Kokkos, the W3SNL1 port, bulk porting; index`.

---

### Task 9: Final verification and pull requests

- [ ] **Step 1: Run the definition of done (spec §10)**

```bash
just kokkos-test serial-debug && just kokkos-test openmp-release
git ls-files '*.py'                                   # exactly the two publishing scripts
git ls-files '*.cpp' '*.hpp' | xargs cat | wc -l      # ≥ 2500
just book && nix develop . --command python3 -m unittest discover -s tests
for f in $(git ls-files '*.sh'); do bash -n "$f"; done; nix develop nix-config/labs/lint --command shellcheck $(git ls-files '*.sh') || true
```

- [ ] **Step 2: ww3-gpu PR** — `git push -u origin feat/cpp-kokkos-course`, `gh pr create` titled "feat: C++/Kokkos lab — W3SNL1 port, nccmp-tol, Python-free code, 16-lesson course" with a body listing the spec, the nine tasks, the verification output, and "Depends on h0ffmann/nix-config#<n> (submodule pinned to its branch commit; re-pin to main after merge with `just submodule-commit`)". Record the PR URL.
- [ ] **Step 3: nix-config PR** — from the scratchpad clone: `gh pr create -R h0ffmann/nix-config --head feat/pratico-kokkos` titled "feat(pratico): Kokkos, GoogleTest, CUDA shell for ww3-gpu", body: what is pinned and why, the smoke check, and "Consumer: https://github.com/h0ffmann/ww3-gpu (PR <url from Step 2>) builds `kokkos/` in `#ww3`/`#cuda`; ww3-gpu pins this branch's commit until merge." Both PR bodies end with the attribution line required by the harness.
- [ ] **Step 4: Report** — final message: both PR URLs, verification numbers (tests, line counts, pages), what is deferred (L2 with the kernel on → fork branch; CUDA preset validated only on the 4090 host; `ww3_lib` cross-check status).
