# W3DMNL_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with one review pass and no test run. Its checkpoint
(ADR-0011) needs the shim and the `INSNL1` wiring first.

- (v) Fresh review 2026-10-16: a WW3 CMake Release build of the pin with `switch_lab_shrd`, made
  for the review in `/tmp/claude-1000/dmnl-review/`; its `w3adatmd.F90.o` and a recompile with
  `-fdump-tree-optimized-lineno` give the same `__w3adatmd_MOD_w3dmnl` but for the `__FILE__`
  length. Read from that GIMPLE: the three checks in source order; 33 `__builtin_malloc` in
  `ALLOCATE` order, 17 sized from `NSPX` (IP, IM, `AF11`) and 16 from `NSP` (IC); `lbound = 1`;
  `elem_len = 4`; type code 3 (REAL) for `AF11`, 1 (INTEGER) for the rest; no store into the data;
  every NULL to one `ISTAT = 5020`, `EXTCDE(99)` block; `nlinit = 1`, then `w3seta`, then
  `*nspecx = NSPX`. Every `w3adatmd.F90`, `w3snl1md.F90` and `w3macros.h` line cited here and in
  the header was re-read at the pin. The header compiled without a warning with explicit
  instantiations on `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`, together with
  `adat_w3naux.hpp` in one unit (g++ 16.2, `-O3` and `-O0 -fsanitize=address,undefined`, both with
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -ffp-contract=off -fopenmp`),
  and on `Kokkos::Cuda` (Kokkos 5.2.2 `nvcc_wrapper`, CUDA 12.9, `-arch=sm_89 --fmad=false
  -Xcompiler -fopenmp`, compile only, in the `#cuda` shell). One defect fixed: the disassembly has
  703 instructions, not 704 (the count had included the label line). The code needed no change.

Allocates the address tables (`IPnn`, `IMnn`, `ICnn`) and the `f**11` scaling array `AF11` of
the Discrete Interaction Approximation (`W3_NL1`) for model `IMOD`, and stores their extent
`NSPECX`. `INSNL1` calls it once per model (`w3snl1md.F90:654`) with `NSP = NSPEC` and
`NSPX = NSPECX = NFRCHG*NTH` (`w3snl1md.F90:650`), then fills the tables itself. It does no
floating-point arithmetic and writes nothing into the arrays.

## What was ported

| | |
|---|---|
| WW3 routine | `W3DMNL`, `model/src/w3adatmd.F90:2426-2623` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`), as `switches/switch_lab_shrd` compiles it (`NL1` set) |
| Kokkos code | [`../src/ww_kokkos/adat_w3dmnl.hpp`](../src/ww_kokkos/adat_w3dmnl.hpp): the function template `ww::adat::w3dmnl`, the `#ifdef W3_NL1` components of `TYPE WADAT` (`w3adatmd.F90:531-547`) that it touches as `WadatNl1`, and their set `WadatsNl1` |
| Relation to W3DIMA | `Wadat` ([`../src/ww_kokkos/adat_wadat.hpp`](../src/ww_kokkos/adat_wadat.hpp)) leaves the `W3_NL1` block out; `WadatsNl1::models[imod-1]` is the NL1 part of `Wadats::wadats[imod-1]`, and `NADATA`, `IADATA` are `Wadats`' (one set of module counters). The extent helper and the `WithoutInitializing` allocator are reused from `adat_w3dima.hpp` (`detail::fext`, `detail::alloc`); no W3DIMA file was edited |
| Memory | every table a `Kokkos::View<..., Kokkos::LayoutLeft, ExecSpace::memory_space>`, created outside any kernel, `WithoutInitializing`; Fortran `X(i)` is `x(i-1)` |
| Errors | the `EXTCDE` exit codes returned as `W3dmnlStatus` (1, 2, 3, 99) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## Order of operations

As the Fortran, `w3adatmd.F90`:

1. `NGRIDS == -1` exits 1 (`:2514-2517`); `IMOD < 1` or `IMOD > NADATA` exits 2 (`:2519-2522`);
   `WADATS(IMOD)%NLINIT` exits 3 (`:2524-2529`).
2. One `ALLOCATE` statement of 33 arrays (`:2539-2572`), `CHECK_ALLOC_STATUS` (exit 99,
   `:2573`, `w3macros.h:49-51`).
3. `WADATS(IMOD)%NLINIT = .TRUE.` (`:2574`), stored into the model directly, before `W3SETA`.
4. `W3SETA(IMOD)` (`:2584`): `IADATA = IMOD` (`:2746`) and the aliases, `NSPECX` among them
   (`:2788`), now point at model `IMOD`.
5. `NSPECX = NSPX` (`:2593-2595`), through that alias, so into `WADATS(IMOD)%NSPECX`.

## Arrays

"undefined" means the Fortran allocates without assigning; the View is created
`WithoutInitializing` for the same reason. All lower bounds are 1.

| Arrays | Type | Fortran extents | Condition | Initial value | `w3adatmd.F90` |
|---|---|---|---|---|---|
| `IP11`, `IP12`, `IP13`, `IP14`, `IM11`, `IM12`, `IM13`, `IM14` | INTEGER | `(NSPX)` | `W3_NL1` | undefined | `:2539-2546` |
| `IP21`, `IP22`, `IP23`, `IP24`, `IM21`, `IM22`, `IM23`, `IM24` | INTEGER | `(NSPX)` | `W3_NL1` | undefined | `:2547-2554` |
| `IC11`, `IC12`, `IC21`, `IC22`, `IC31`, `IC32`, `IC41`, `IC42` | INTEGER | `(NSP)` | `W3_NL1` | undefined | `:2555-2562` |
| `IC51`, `IC52`, `IC61`, `IC62`, `IC71`, `IC72`, `IC81`, `IC82` | INTEGER | `(NSP)` | `W3_NL1` | undefined | `:2563-2570` |
| `AF11` | REAL | `(NSPX)` | `W3_NL1` | undefined | `:2571` |

| Scalar | Value | `w3adatmd.F90` |
|---|---|---|
| `NLINIT` of model `IMOD` | `.TRUE.` | `:2574` |
| `NSPECX` of model `IMOD` | `NSPX` | `:2594` |
| `IADATA` | `IMOD` | `:2584` → `:2746` |

`NSP` or `NSPX` below 1 gives a zero-size array, as `ALLOCATE` does.

## Arithmetic and parity

The reference is the `-O3` object of `w3adatmd.F90`, compiled 2026-10-16 in
`/tmp/claude-1000/dmnl-scratch/` with gfortran 16.2 in the pinned `ww3` shell
(`nix develop "path:picasso/labs/pratico#ww3"`), the Fortran flags of WW3's CMake Release build
(`-O3 -fallow-argument-mismatch -g -fno-second-underscore -ffree-line-length-none`), the `ENDIANNESS` and 36 `W3_*`
`switch_lab_shrd` defines of `ww3_lib.dir/flags.make`, the `.mod` files of an existing WW3 build,
plus `-fdump-tree-optimized-lineno`; read as GIMPLE and as `objdump -d`.

- (v) The object is WW3's: the disassembly of `__w3adatmd_MOD_w3dmnl` (703 instructions) is
  identical to the one in the CMake build of `03adda91`
  (`build/ww3/ww3/model/src/CMakeFiles/ww3_lib.dir/w3adatmd.F90.o` in the `ww3-gpu-noaa`
  worktree, gfortran 16.2) except one `push` immediate, `0xc` against `0x40`: the length of the
  `__FILE__` string `CHECK_ALLOC_STATUS` passes to `EXTCDE` (`w3adatmd.F90` here, an absolute
  path there).
- (v) The checks run in the source order: `ngrids == -1` (`:2514`), `imod <= 0` then
  `imod > nadata` (`:2519`), `wadats[imod].nlinit != 0` (`:2525`).
- (v) The 33 `__builtin_malloc` calls follow the `ALLOCATE` list, `IP11` to `IM24`, `IC11` to
  `IC82`, `AF11`; 17 take the size from `NSPX` and 16 from `NSP`, each `max(n,0)*4` bytes
  (`malloc(1)` when `n <= 0`); every descriptor gets `lbound = 1`, `ubound = n`, `elem_len = 4`;
  `AF11` has the REAL type code, the others INTEGER. No store into the data follows.
- (v) Any NULL return jumps to one block that sets `ISTAT = 5020` and calls `EXTCDE(99, ...)`
  (line `:2573`); the arrays after the failing one are not allocated. The port returns 99 from
  the first View allocation that throws.
- (v) `NLINIT = 1` is stored into `WADATS(IMOD)` before the `w3seta` call; after it the module
  pointer `nspecx` is loaded and `NSPX`, read once at entry, is stored through it. Since
  `W3SETA(IMOD)` just set that pointer (`:2788`), this is `WADATS(IMOD)%NSPECX`.
- (v) `W3SETA`'s own exits (`NADATA == -1`, `:2729`; `IMOD` out of range, `:2734`) cannot fire
  after `W3DMNL`'s check at `:2519` passed, read from the source.
- ⚠ An `ALLOCATE` failure is `malloc` returning NULL in the Fortran. In the port it is the
  exception Kokkos throws when a View allocation fails, caught and returned as 99. Read in the
  `#cuda` shell's Kokkos 5.2.2 (v): `impl/Kokkos_Error.hpp` declares `throw_bad_alloc` and
  `Kokkos::Experimental::BadAlloc : std::runtime_error`, and `Kokkos_CudaSpace.cpp.o` of
  `libkokkoscore.a` calls `throw_bad_alloc`. That every failed `cudaMalloc` reaches it, rather than
  an abort, was not checked, and no failing allocation was run.
- ⚠ `W3dmnlStatus::nl1_not_sized` (-1) is not a WW3 code. It guards the index into
  `WadatsNl1::models`, which the caller must size to `NADATA`; in the Fortran the NL1 components
  live inside `WADATS(IMOD)` and the case cannot arise.
- (v) `NLINIT` has one home, `WadatNl1::nlinit`, as the Fortran has one field,
  `WADATS(IMOD)%NLINIT`: `W3NAUX` (`adat_w3naux.hpp`) sizes `WadatsNl1` with `Wadats` and clears
  it for `:809`, and `W3DMNL` and `W3SETA` read it. The fresh review found it duplicated in
  `WadatScalars`; the duplicate was removed before commit.
- ⚠ A zero-extent Fortran array is `ALLOCATED`, a zero-extent View carries no such flag.
  Consumers test `nlinit`, as `W3SETA` does (`:3038`), not `View::is_allocated()`.
- ⚠ `WadatNl1::nspecx` starts at 0. In the Fortran `W3NAUX` leaves `NSPECX` unset, and `INSNL1`
  writes it through the alias (`w3snl1md.F90:650`) before `W3DMNL` stores `NSPX` again; a port of
  `INSNL1` must write the same field.
- ⚠ `ngrids` is the caller's `W3GDATMD` `NGRIDS`. The shim has to pass that module value, not
  `NADATA`.

## Not ported

- The `W3_S` STRACE (`:2493-2495`, `:2506-2509`) and the `W3_T` prints: not compiled under the lab switch.
- The build without `W3_NL1`, where `W3DMNL` is only the two checks and `W3SETA`.
- The `WRITE(NDSE)` messages and `EXTCDE`: returned as the code it would exit with.
- `W3SETA`'s pointer aliasing (`:2584`), except `IADATA` and the `NSPECX` target it decides.
  `NDSE` and `NDST` are not parameters of the port.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and the routine runs once per model at set-up.

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v): a throwaway translation unit
`/tmp/claude-1000/dmnl-scratch/tu_w3dmnl.cpp` that explicitly instantiates
`ww::adat::w3dmnl<Kokkos::DefaultExecutionSpace>` (OpenMP in the shell) compiled in the pinned
`ww3` shell (g++ 16.2), with no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
    -ffp-contract=off -fopenmp -c tu_w3dmnl.cpp
```

The same unit also compiled at `-O0 -g` with `-Wsign-conversion` and
`-fsanitize=address,undefined`, again with no warning. It was never linked or run. The only
programs run were the two compilers, `objdump` and `diff`.
