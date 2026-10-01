# 11 — Kokkos and modern C++ for Fortran people

Kokkos is a C++ library that lets one kernel run on a CPU thread pool or a GPU without
rewriting it: the backend is a build choice. Every concept below is a Fortran concept
with a new name, and every one has a runnable program in `kokkos/intro/` that checks
itself and exits non-zero if the concept did not hold (v, `kokkos/intro/CMakeLists.txt`).
Read each program next to its section. Build them first:

```bash
just kokkos-test serial-debug     # configure + build + ctest, inside the pratico #ww3 shell
```

| Fortran | Kokkos | Program |
|---|---|---|
| `REAL, ALLOCATABLE :: A(:,:)` | `Kokkos::View<Real**, DeviceSpace> a("label", n, m)` | `01_views.cpp` |
| column-major order | `Kokkos::LayoutLeft` | `04_layouts_and_mirrors.cpp` |
| `DO` loop over independent iterations | `parallel_for` with a `RangePolicy` / `MDRangePolicy` | `02_parallel_for.cpp` |
| `SUM`, `MAXLOC`, prefix sum | `parallel_reduce` with a reducer, `parallel_scan` | `03_reduce_and_scan.cpp` |
| automatic array per point | `TeamPolicy` + `team_scratch(0)` | `05_team_scratch.cpp` |
| `BIND(C)` interface block | `extern "C"` + unmanaged `View` | `06_interop_bindc.cpp` / `06_interop_driver.F90` |

## Execution and memory spaces

A kernel runs in an **execution space** (Serial, OpenMP, CUDA) and touches memory in a
**memory space** (`HostSpace`, `CudaSpace`). The lab spells the memory space out on every
`View` (v, every intro program):

```cpp
using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
```

That alias is `HostSpace` in the `#ww3` shell and `CudaSpace` in `#cuda`, so the same
source compiles everywhere, but the declaration now says whether host code may
dereference the array. Leaving it implicit is how host code ends up reading device memory
(v, comment in `01_views.cpp`). The backend comes from the *shell*, not the preset:
OpenMP is the default execution space in `#ww3`, so `serial-debug` is a build type, and
code that must run serially names `Kokkos::Serial` explicitly (v, `kokkos/README.md`).

## `View`

`01_views.cpp` (v): `Kokkos::View<ww::Real**, DeviceSpace> spectrum("intro.spectrum", nth, nk)`.
The string is a label that shows up in profilers and bounds-check aborts, so spell it.
`rank()`, `extent(i)` and `span_is_contiguous()` are carried by the View, so no separate
`nth, nk` arguments travel with the array. A fresh View is zero-initialised unless you ask
for `Kokkos::WithoutInitializing`. Views are reference-counted handles: copying one copies
a pointer, and the allocation dies with the last handle, which must happen *before*
`Kokkos::finalize()`, hence the inner scope in every intro `main` (v). `ww::Real` is
`float` (v, `kokkos/src/ww_kokkos/real.hpp`).

## Layout: `LayoutLeft` is Fortran order

Layout is part of the type and decides which index is contiguous. `04_layouts_and_mirrors.cpp`
asserts it (v): `LayoutLeft` has `stride(0) == 1`, `stride(1) == nth`; `LayoutRight` the
reverse. Kokkos defaults to `LayoutRight` on host spaces and `LayoutLeft` on CUDA because
each coalesces on its backend. You override it in exactly one place: the Fortran
boundary. A WW3 array `A(NTH,NK)` wrapped as anything but `LayoutLeft` still "works":
every index is silently transposed. `deep_copy` between different layouts is a transpose,
correct and expensive, which is the argument for choosing once at the boundary rather than
converting inside a time step (v).

## Mirrors and `deep_copy`

You may not dereference a device View from host code. `create_mirror_view(v)` gives a
host View with the same layout; `deep_copy(host, v)` fills it. On a host backend the
mirror *is* the original and the copy is a no-op, so the pattern costs nothing when it is
not needed (v, `01_views.cpp`). `create_mirror_view_and_copy(Kokkos::HostSpace(), v)` is
the one-liner the tests use (v, `03_reduce_and_scan.cpp`).

## `parallel_for`, `parallel_reduce`, `parallel_scan`

`02_parallel_for.cpp` builds a JONSWAP spectrum over a WW3-shaped grid with
`MDRangePolicy<Rank<2>>({0,0},{nth,nk})` and integrates it back to `Hs` with a
`parallel_reduce`, checked against the closed-form `m0` to within 2 % (v). Two habits in
that file: the reduction accumulator is `double` even though the state is `float`, because
768 terms over five decades lose digits otherwise; and the check against something
computed independently *is* the point: a kernel you cannot compare is one you cannot port.

`03_reduce_and_scan.cpp` (v): `Kokkos::MaxLoc<Real,int>` returns the largest `Hs` *and*
where; the reducer, not the lambda, owns the identity and the join, so it is right on every
backend. `parallel_scan` calls the functor more than once and only the `is_final` pass may
write. A `WW_DETERMINISTIC` build pins the scan to `Kokkos::Serial` after asking
`SpaceAccessibility` whether Serial can even reach the device memory (v).

## `TeamPolicy` and scratch

`05_team_scratch.cpp` is the shape of the DIA kernel (v): `TeamPolicy(npts, Kokkos::AUTO)`
makes one **team** per sea point; `policy.set_scratch_size(0, PerTeam(bytes))` with
`bytes = ScratchSpectrum::shmem_size(nth, nkx)` reserves level-0 scratch: shared memory
on a GPU, a slice of a thread-local arena on a CPU; inside the lambda,
`ScratchSpectrum ue(team.team_scratch(0), nth, nkx)` is an *unmanaged* view onto it (scratch is
a bump allocator; nothing frees). `TeamThreadRange(team, n)` parallelises across the team,
`team.team_barrier()` separates writing scratch from reading it (not optional), and
`Kokkos::single(PerTeam(team), …)` lets one thread write the team's result. The extended
spectrum with its parametric tail lives there because it is per point, re-read many times,
and must never be a global allocation inside a time loop (v). The port's scratch for
`NK=25, NTH=24` is about 28 KB, well inside `TeamPolicy::scratch_size_max(0)`, which for the Kokkos 5.2 CUDA backend is the device's opt-in shared-memory limit minus ~24.6 KB: about 75 KB on the RTX 4090, not the classic 48 KB (v, `snl1_dia.cpp`, `Kokkos_Cuda_Parallel_Team.hpp`).

## `KOKKOS_LAMBDA` and what a kernel may touch

`KOKKOS_LAMBDA` captures by value. A View is a handle, so capturing it is cheap and
correct; `snl1_dia.cpp` copies its `Tables` struct into a local before the lambda for
exactly that reason (v). The rule ("no host memory in kernels",
`docs/AGENTS_KOKKOS_202609.md` §1.2): only Views, scalars and `KOKKOS_INLINE_FUNCTION`
helpers such as `ww::jonswap` (v, `spectrum_fixtures.hpp`): no `std::vector`, no
`std::function`, no `this`, no allocation, no exceptions. One nvcc quirk the lab hit:
an extended lambda cannot live inside a private member function, and a GoogleTest
`TEST()` body is one, so kernels sit in free functions (v, `kokkos/README.md`).

## Fences

`Kokkos::fence()` before the host reads a result, before a mirror dies, before a timer.
`06_interop_bindc.cpp` fences because the Fortran caller reads the array the instant the
call returns (v); `snl1_tables.cpp` fences because its host mirrors go out of scope at the
end of `make_tables` (v). Do not sprinkle them: each one destroys overlap.

## `float` versus `double`

WW3's spectrum is default `REAL`; the port keeps `float` state on purpose, halving bytes
per quadruplet (v, `real.hpp`). Every kernel literal is written `static_cast<Real>(…)` and
the build turns on `-Wconversion` so a stray `double` is a warning, not a silent 2× in
bandwidth (v). Accumulate in `double`. And know that `x**n` in gfortran is a chain of
multiplications, not `powf`: `snl1_tables.cpp` has a `powi()` that reproduces the chain,
and that is the difference between bit parity and "close" (v, `kokkos/README.md`).

## Modern C++ hygiene, briefly

RAII: `Kokkos::ScopeGuard guard(argc, argv)` initialises and finalises the runtime with
the scope (v, every intro `main`; `06` is a library and exposes `init`/`finalize` entry
points instead). No raw `new`/`delete`, no C arrays in kernels, `const` by
default, C++20 (v, `kokkos/CMakeLists.txt`). One warning policy for the whole tree,
`kokkos/cmake/CompilerWarnings.cmake`, applied to every target through
`ww_apply_warnings` (v). The `serial-debug` preset adds AddressSanitizer and
UndefinedBehaviorSanitizer (v, `CMakePresets.json`). Exceptions are allowed outside
kernels only; the shim in lesson 12 turns them into status codes before Fortran sees them.

## `bind(C)` interop

`06_interop_bindc.cpp` and `06_interop_driver.F90` are the smallest honest boundary (v).
C++ side: `extern "C"` (no mangling, no overloads, no references), an unmanaged
`LayoutLeft` `HostSpace` view over the caller's pointer (Kokkos neither allocates nor
frees, Fortran stays the owner), and separate `init`/`finalize` entry points because
`Kokkos::initialize()` may run once per process. Fortran side:

```fortran
     subroutine ww_intro_scale(n, x, s) bind(c, name='ww_intro_scale')
       import :: c_int, c_float
       integer(c_int), value :: n
       real(c_float), intent(inout) :: x(*)
       real(c_float), value :: s
     end subroutine ww_intro_scale
```

`VALUE` for scalars C takes by value, `iso_c_binding` kinds everywhere, an assumed-size
array so the address of the first element crosses and column-major order is preserved.
The real shim (`kokkos/src/fortran_iface/`) is this with two-dimensional arrays, where
the layout choice actually bites.

## Building: presets and the CUDA backend

| Preset | What it is (v, `CMakePresets.json`) |
|---|---|
| `serial-debug` | Debug, sanitizers, `WW_DETERMINISTIC=ON` |
| `openmp-release` | Release, `-O3 -march=x86-64-v3` |
| `cuda-release` | Release, `CMAKE_CXX_COMPILER=nvcc_wrapper`, `CMAKE_CUDA_ARCHITECTURES=89` |

```bash
just kokkos-test openmp-release
just kokkos-cuda-test             # configure, build, ctest inside the #cuda shell
```

The pinned CUDA Kokkos is built for **one architecture only, `Kokkos_ARCH_ADA89`**: the
RTX 4090. An H100 needs a Kokkos rebuild with `HOPPER90` in `nix-config`, not a change to
the presets (v, `CMakePresets.json` comment). Kokkos 5.2.0 is the pin (v, `kokkos/README.md`).
`Kokkos_ENABLE_DEBUG_BOUNDS_CHECK` is an option of the Kokkos build itself, so the lab
cannot switch it on from here; the sanitizers are the net (v).

Exercise: `exercises/solutions/ex12_reduce.cpp`, a `parallel_reduce` computing `Hs`
from a spectrum View, built against `ww_kokkos`.

→ [`12-porting-a-kernel-w3snl1.md`](12-porting-a-kernel-w3snl1.md)
