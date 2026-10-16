# W3NAUX_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the wiring of the programs that call it (`ww3_shel.F90:447` and the other `CALL W3NAUX`
sites) first.

- (v) Fresh review 2026-10-16: an independent WW3 CMake Release build of `ww3_lib` at the pin with
  `switch_lab_shrd` (`W3_NL1` and `W3_SHRD` defined, no `W3_S`, `W3_T`, `W3_MPI`), `w3adatmd.F90`
  recompiled with its `flags.make` and `-fdump-tree-optimized-lineno`, disassembly of
  `__w3adatmd_MOD_w3naux` identical to the CMake object; the check order, both `ISTAT` paths
  (5014, 5020) to `EXTCDE(99)`, `NADATA = NGRIDS` and the eleven stores of zero (`NLINIT` among
  them) read in that GIMPLE and found as the port has them; `NADATA` written only at `:792` and `WADATS`
  deallocated nowhere in `model/src`; every line citation re-read. Defects fixed: the loop was
  cited `:797-810` in the header (its `END DO` is `:811`), and the `AINIT2` comment cited only the
  declaration for "set by W3XDMA" (now `:2375`). The C++ was compiled with explicit
  instantiations on `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial` and its `-O3`
  GIMPLE read (below). No physics defect.

Allocates `WADATS(NGRIDS)`, the per-model array of `TYPE WADAT`, sets the module counter
`NADATA = NGRIDS`, and gives each model's scalar counters and flags their first value. It does no
floating-point arithmetic and allocates no array component: that is `W3DIMA`'s job, model by model.

## What was ported

| | |
|---|---|
| WW3 routine | `W3NAUX`, `model/src/w3adatmd.F90:705-831` at the `WW3/` pin `8e433f99` (byte-identical to NOAA-EMC/WW3 `03adda91`), as `switches/switch_lab_shrd` compiles it |
| Kokkos code | [`../src/ww_kokkos/adat_w3naux.hpp`](../src/ww_kokkos/adat_w3naux.hpp), a host-side function template over the execution space; it includes [`../src/ww_kokkos/adat_wadat.hpp`](../src/ww_kokkos/adat_wadat.hpp) and does not change it |
| Memory | no View is created; `std::vector<Wadat<ExecSpace>>` of default-constructed (unallocated) Views, which `W3DIMA` ([`W3DIMA_KOKKOS.md`](W3DIMA_KOKKOS.md)) then fills |
| Errors | the `EXTCDE` exit codes returned as `W3nauxStatus` (1, 99) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

## How it relates to W3DIMA's types

`adat_wadat.hpp` (written for `W3DIMA`) holds `Wadats` (`WADATS`, `NADATA`, `IADATA`) and, per
model, `Wadat` with the arrays and the three scalars `W3DIMA` reads or writes: `NSEALM`, `AINIT`,
`FL_ALL`. `W3NAUX` sets eight more scalars of `TYPE WADAT`. Those are `WadatScalars` in
`adat_w3naux.hpp`, a vector the caller keeps parallel to `Wadats::wadats` (`scalars[imod-1]` is
`WADATS(IMOD)`'s), so the `W3DIMA` files stay as reviewed. A later `W3SETA` port that needs
`ITIME`, `FLCOLD` and the rest reads them from there.

| `TYPE WADAT` component | Declared | Set by W3NAUX | Held in |
|---|---|---|---|
| `ITIME`, `IPASS`, `IDLAST` | `:566` | `0`, `:798-800` | `WadatScalars` |
| `NSEALM` | `:566` | `0`, `:801` | `Wadat::nsealm` |
| `ITSTEP` | `:566` | `0`, `:802` | `WadatScalars` |
| `FLCOLD`, `FLIWND` | `:568` | `.FALSE.`, `:803-804` | `WadatScalars` |
| `AINIT` | `:568` | `.FALSE.`, `:805` | `Wadat::ainit` |
| `AINIT2` | `:568` | `.FALSE.`, `:806` | `WadatScalars` |
| `FL_ALL` | `:568` | `.FALSE.`, `:807` | `Wadat::fl_all` |
| `NLINIT` (`W3_NL1` block) | `:546` | `.FALSE.`, `:809` | `WadatScalars` |

`LOGICAL` is `bool` here, as `Wadat::ainit` and `Wadat::fl_all` already are.

## Order of operations

As the Fortran, `w3adatmd.F90`:

1. `NGRIDS == -1` exits 1 (`:782-785`), after a `WRITE(NDSE,1001)` that is not ported.
2. `ALLOCATE(WADATS(NGRIDS), STAT=ISTAT)` (`:790`), `CHECK_ALLOC_STATUS` (`:791`,
   `w3macros.h:49-51`) exits 99 when `WADATS` is already allocated or `malloc` fails. `NGRIDS <= 0`
   gives a zero-size array and no error.
3. `NADATA = NGRIDS` (`:792`). `IADATA` is not touched.
4. `DO I = 1, NGRIDS` (`:797-811`): the eleven stores of the table above.

## Arithmetic and parity

The reference is the `-O3` object of `w3adatmd.F90`, compiled 2026-10-16 with gfortran 16.2 in
the pinned `ww3` shell, the Fortran flags of WW3's CMake Release build
(`ww3_lib.dir/flags.make`) and the `switch_lab_shrd` defines, plus
`-fdump-tree-optimized-lineno`; read as GIMPLE and assembly.

- (v) The object is WW3's: the disassembly of `__w3adatmd_MOD_w3naux` (122 lines) is identical to
  the one in the CMake build's own `w3adatmd.F90.o` made in the same scratch directory.
- (v) The `NGRIDS == -1` test is the first statement (GIMPLE `:782`), before the `ALLOCATE`; the
  port tests it first.
- (v) `ALLOCATE(WADATS(NGRIDS), STAT=ISTAT)`: GIMPLE `:790` tests `wadats.data != 0` and sets
  `ISTAT = 5014` without calling `malloc`; otherwise `malloc(MAX(NGRIDS*19120, 1))`, `NGRIDS <= 0`
  giving size 0, and `ISTAT = 5020` when it returns NULL, with `wadats.data` still NULL. Both
  values reach `EXTCDE(99)` (`:791`). The port returns 99 in both cases and leaves `reg` and
  `scalars` as on entry.
- (v) "Already allocated" is `reg.nadata != -1` in the port, because an empty `std::vector` cannot
  tell a zero-size allocation from none. That is exact for the module's own state: `WADATS` is
  allocated only at `:790` and deallocated nowhere in `model/src`, and `NADATA` is assigned only at
  `:792` (initial value `-1` at `:377`), reached only with `NGRIDS != -1` (searched in
  `WW3/model/src/*.F90` at the pin).
- (v) The `ALLOCATE` stores nothing into the elements (no store between `malloc` and `:792` other
  than the descriptor): `TYPE WADAT` has no default initialisation under this switch (its only
  one, `NRQSG1 = 0` at `:553`, is in the `W3_MPI` block `:550-555`). Its pointer components are undefined
  until `W3DIMA`; in the port they are default-constructed Views, a defined state no conforming
  caller can tell apart.
- (v) The loop body is integer and logical stores of zero, no arithmetic. gfortran merges them:
  one 16-byte store of `{0,0,0,0}` over `ITIME, IPASS, IDLAST, NSEALM` (adjacent at `:566`), then
  `ITSTEP`, `FLIWND`, one 16-byte store over `AINIT, AINIT2, FL_ALL, FLCOLD` (`:568`), and `NLINIT`
  (`:809`, so `W3_NL1` is compiled in). The port stores each field in source order; nothing reads
  them in between, so the order of the stores is not observable.
- (v) `NADATA` is stored from a reload of `NGRIDS` after the allocation (GIMPLE `pretmp_16`), the
  same value the test read; the port uses its argument.
- (v) The C++ `-O3` GIMPLE of `w3naux<Kokkos::Serial>` (g++ 16.2, fresh review): `ngrids == -1`
  tested first, then `reg.nadata != -1`, `reg.nadata = ngrids` after the moves, and in the loop the
  eleven stores of zero, one per field, the same fields as the Fortran's merged stores. `bool` is one
  byte, `LOGICAL(4)` four: a shim must convert, not copy the bytes.
- ⚠ An allocation failure is `malloc` returning NULL in the Fortran. In the port it is any
  exception from `std::vector::resize` (`std::bad_alloc`, `std::length_error`), caught and returned
  as 99. Not exercised.
- ⚠ The caller must keep `scalars` parallel to `reg.wadats`; nothing in the types enforces it.
  It is a faithful mirror of `WADATS(IMOD)%ITIME` and the rest only because `W3NAUX` is the one
  routine that sizes `WADATS` and it sizes both vectors together; `W3DIMA` resizes neither. The
  alternative, the eight fields in `Wadat`, would remove the invariant but reopen the reviewed
  `adat_wadat.hpp`; the shim, which will own `Wadats`, should own `scalars` beside it, or a later
  change folds them into `Wadat`.

## Not ported

- `W3_S` STRACE (`:774-777`) and the `W3_T` print (`:813-815`): not compiled under the lab switch.
- The `WRITE(NDSE,1001)` message (`:783`, format `:821-823`) and `EXTCDE`: returned as the code it
  would exit with. `NDSE` and `NDST`, used only by those, are not parameters.
- `ISTAT`, module-private (`:373`), used only for the check.

## Benchmarks

None. No timing without parity (`AGENTS.md`), and the routine runs once per program at set-up.

## How it was regtested

Not yet, and nothing was run. What was done, 2026-10-16 (v), in
`/tmp/claude-1000/naux-scratch/`:

1. WW3's CMake configure and `ww3_lib` build at the pin with `-DCMAKE_BUILD_TYPE=Release
   -DSWITCH=switch_lab_shrd`, then `w3adatmd.F90` recompiled with the flags of
   `ww3_lib.dir/flags.make` plus `-fdump-tree-optimized-lineno`, and both objects disassembled with
   `objdump -d --no-show-raw-insn` (`build.sh` there).
2. A throwaway translation unit that takes the address of
   `ww::adat::w3naux<Kokkos::DefaultExecutionSpace>`, compiled in the pinned `ww3` shell (g++ 16.2)
   with the `openmp-release` flags and `-Wshadow -Wconversion`, no warning:

```bash
g++ -DKOKKOS_DEPENDENCE -Ikokkos/src/ww_kokkos -Ikokkos/src -O3 -march=x86-64-v3 -DNDEBUG \
    -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-unused-parameter \
    -ffp-contract=off -fopenmp -c tu_w3naux.cpp
```

The same unit also compiled at `-O0` with `-Wsign-conversion` and
`-fsanitize=address,undefined`, again with no warning. It was never linked or run. No program of
the port was run.

The fresh review repeated both in `/tmp/claude-1000/naux-review/` with a unit holding explicit
instantiations of `w3naux` on `Kokkos::DefaultExecutionSpace` and `Kokkos::Serial`: `-O3` with
the flags above plus `-Wsign-conversion`, and `-O0 -g -fsanitize=address,undefined` with the same
warnings, no warning in either; compiled only.
