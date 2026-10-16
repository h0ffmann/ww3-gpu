# W3DIMS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: the owner asked
for translation only on 2026-10-16, with no test run. Its checkpoint (ADR-0011) needs the shim
and the `W3IOGR` wiring first.

- (v) Fresh review 2026-10-16: every array of `switch_lab_shrd` (extents, lower bounds, list
  order, condition, initial value), the error codes and their order, the `W3SETG` side effects
  (`:2262-2280`, `:2498-2534`, `:2707-2712`) and every line citation, against the Fortran at
  `8e433f99` and against a regenerated `-O3` GIMPLE dump (identical to the translator's, `.text`
  identical to WW3's CMake object); the header recompiled with `-Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -ffp-contract=off`, `w3dims` explicitly instantiated on
  `Kokkos::DefaultExecutionSpace` (OpenMP) and `Kokkos::Serial`: no warning, no FMA; a
  throwaway host run (not kept, ADR-0010) gave codes 1, 2, 3, 4 in order, the extents, the fill
  bits and 99 with the earlier arrays kept on a failed `CUMULW`. No defect in the code; in the
  report the callers and the `W3IOGR` duplicate were corrected and the object-identity ⚠
  checked for `.text`.

Sets the spectral dimensions `MK`, `MTH` of model `IMOD`, allocates the spectral arrays of
`SGRDS(IMOD)` and, under `ST4`, the dissipation tables of `MPARS(IMOD)%SRCPS`, fills them, marks
the grid initialised and selects it (`W3SETG`). Called by `W3IOGR` when it reads
`mod_def.ww3` (`w3iogrmd.F90:1168`) and by `W3GRID` of `ww3_grid` (`w3gridmd.F90:1270`); the
calls of `WMINIT` (`wminitmd.F90:2781`, `:5865`) are under `W3_MPI`, not compiled by the lab
switch.

## What was ported

| | |
|---|---|
| WW3 routine | `W3DIMS`, `model/src/w3gdatmd.F90:1929-2154` at h0ffmann/WW3 `8e433f99` (the `WW3/` pin), as `switches/switch_lab_shrd` compiles it (the `W3_ST4` block `:2088-2106` in) |
| Kokkos code | [`../src/ww_kokkos/gdat_w3dims.hpp`](../src/ww_kokkos/gdat_w3dims.hpp) (the routine, host-only) and [`../src/ww_kokkos/gdat_sgrd.hpp`](../src/ww_kokkos/gdat_sgrd.hpp) (one model's `SGRD`, the six `SRCP` arrays, the module counters) |
| C shim | none yet |
| Plan | [`W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md`](../../docs/W3WAVE_SUPPORT_KOKKOS_PLAN_202610.md), issue #42 |

`w3dims<ExecSpace>(exec, imod, mk, mth, gm, sg, sp)` returns 0, or the code the Fortran passes
to `EXTCDE`, which stops the program; the shim writes the message and stops. The Views live in
`ExecSpace::memory_space`, `LayoutLeft`, so `A(i,j)` maps to `a(i-lb1, j-lb2)` in the same
column-major order. They are allocated and filled from the host, outside any kernel
(`Kokkos::view_alloc(WithoutInitializing)`, then `deep_copy(exec, view, value)` and one fence).

| Fortran | C++ | Line |
|---|---|---|
| `IF (NGRIDS .EQ. -1)`, `EXTCDE(1)` | `return 1` | `w3gdatmd.F90:2029-2032` |
| `IMOD.LT.-NAUXGR .OR. IMOD.GT.NGRIDS`, `EXTCDE(2)` | `return 2` | `:2034-2037` |
| `MK.LT.3 .OR. MTH.LT.4`, `EXTCDE(3)` | `return 3` | `:2039-2042` |
| `SGRDS(IMOD)%SINIT`, `EXTCDE(4)` | `return 4` | `:2044-2047` |
| `MSPEC = MK*MTH` (`MK2` is a `SAVE` local nothing reads) | `const int mspec` | `:2049-2050` |
| `ALLOCATE(...)`, `CHECK_ALLOC_STATUS` | Views in list order; a failed allocation returns 99 | `:2058-2073`, `:2089-2093`, `:2098-2102`; `w3macros.h:49-51` |
| fills | `deep_copy` per array, in the Fortran's order | `:2074-2087`, `:2094-2096`, `:2103-2105` |
| `SDSNTH = MTH/2-1` | integer division | `:2097` |
| `SGRDS(IMOD)%SINIT = .TRUE.` | `sg.sinit = true` | `:2108` |
| `CALL W3SETG(IMOD,...)`: `IGRID = ISGRD = IPARS = IMOD` | `gm.igrid/isgrd/ipars = imod` | `:2116`, `:2278-2280` |
| `NK, NK2, NTH, NSPEC` through `W3SETG`'s pointers | `sg.nk`, `sg.nk2`, `sg.nth`, `sg.nspec` | `:2124-2127`, `:2498-2501` |

## Arrays

`MSPEC = MK*MTH`, `SDSNTH = MTH/2-1`; `NDTAB = 2000`, `NKHS = 2000`, `NKD = 1300`
(`w3gdatmd.F90:637-638`, `W3_ST4`). Declarations: `SGRD` `:795-804`, `SRCP` `:904-905`.

| Array | Type | Extents (Fortran bases) | Condition | Initial value | Allocated, filled |
|---|---|---|---|---|---|
| `MAPWN` | INTEGER | `(MSPEC+MTH)` | always | `0` (`0.` folded) | `w3gdatmd.F90:2058`, `:2074` |
| `MAPTH` | INTEGER | `(MSPEC+MTH)` | always | `0` (`0.` folded) | `:2059`, `:2075` |
| `TH` | REAL | `(MTH)` | always | `0.0f` | `:2060`, `:2076` |
| `ESIN` | REAL | `(MSPEC+MTH)` | always | `0.0f` | `:2061`, `:2077` |
| `ECOS` | REAL | `(MSPEC+MTH)` | always | `0.0f` | `:2062`, `:2078` |
| `ES2` | REAL | `(MSPEC+MTH)` | always | `0.0f` | `:2063`, `:2079` |
| `ESC` | REAL | `(MSPEC+MTH)` | always | `0.0f` | `:2064`, `:2080` |
| `EC2` | REAL | `(MSPEC+MTH)` | always | `0.0f` | `:2065`, `:2081` |
| `SIG` | REAL | `(0:MK+1)`, C++ index `k` direct, extent `MK+2` | always | `0.0f` | `:2066`, `:2082` |
| `SIG2` | REAL | `(MSPEC)` | always | `0.0f` | `:2067`, `:2083` |
| `DSIP` | REAL | `(0:MK+1)`, C++ index `k` direct, extent `MK+2` | always | `0.0f` | `:2068`, `:2084` |
| `DSII` | REAL | `(MK)` | always | `0.0f` | `:2069`, `:2085` |
| `DDEN` | REAL | `(MK)` | always | `0.0f` | `:2070`, `:2086` |
| `DDEN2` | REAL | `(MSPEC)` | always | `0.0f` | `:2071`, `:2087` |
| `SRCPS%IKTAB` | INTEGER | `(MK,NDTAB)` | `W3_ST4` | `0` (`0.` folded) | `:2089`, `:2094` |
| `SRCPS%DCKI` | REAL | `(NKHS,NKD)` | `W3_ST4` | `0.0f` | `:2090`, `:2095` |
| `SRCPS%QBI` | REAL | `(NKHS,NKD)` | `W3_ST4` | `0.0f` | `:2091`, `:2096` |
| `SRCPS%SATINDICES` | INTEGER | `(2*SDSNTH+1,MTH)` | `W3_ST4` | `1` (`1.` folded) | `:2098`, `:2103` |
| `SRCPS%SATWEIGHTS` | REAL | `(2*SDSNTH+1,MTH)` | `W3_ST4` | `0.0f` | `:2099`, `:2104` |
| `SRCPS%CUMULW` | REAL | `(MSPEC,MSPEC)` | `W3_ST4` | `0.0f` | `:2100`, `:2105` |

Every lower bound is 1 except `SIG` and `DSIP`. A `1`-based `A(i)` is `a(i-1)`; `A(i,j)` is
`a(i-1,j-1)`. The index maps are in the header of `gdat_sgrd.hpp`.

## Parity

The reference is the `-O3` object of `w3gdatmd.F90` built by gfortran 16.2 in the pinned `ww3`
shell with the flags of WW3's CMake Release build (`-O3 -fallow-argument-mismatch -g
-fno-second-underscore -ffree-line-length-none`) and the `switch_lab_shrd` defines, read as
GIMPLE (`-fdump-tree-optimized-lineno`), 2026-10-16. (v) Its `.text` section is byte-identical
(`objcopy -O binary --only-section=.text`, then `cmp`) to that of the `w3gdatmd.F90.o` WW3's own
CMake Release build wrote under `switch_lab_shrd` at the same pin, 2026-10-16; the whole objects
differ in size (153296 and 153304 bytes), in sections not compared ⚠ (debug information, which
records paths).

- (v) `W3DIMS` does no floating-point arithmetic: the optimised GIMPLE holds only integer
  extent arithmetic, `malloc`, descriptor stores and scalar stores of the constants `0.0`
  (REAL), `0` and `1` (INTEGER). The REAL literals assigned to INTEGER arrays fold to `0` and
  `1` at compile time. `RADE` is USEd (`:2000`) but only appears in a commented-out expression
  (`:2097`).
- (v) The checks run in the Fortran's order and return `EXTCDE`'s code (`:2029-2047`); the
  first failed `malloc` of an `ALLOCATE` list skips the rest of the list and calls
  `EXTCDE(99)` (GIMPLE: `istat = 5020`, then `extcde`). The C++ assigns each View as it goes and
  returns 99 at the first failure, so the arrays allocated before it stay allocated, as in the
  Fortran.
- (v) The fills give the same bits. Kokkos 5.2's `deep_copy` of a value whose bits are all zero
  is a memset of the View (`Kokkos_CopyViews.hpp`, `contiguous_fill_or_memset`), which writes
  `0x00000000`, the bits of the Fortran's `0.0` and `0`; `SATINDICES = 1` is a parallel fill of
  the integer 1. The order between fills does not change any value: each array is written once,
  with one constant.
- (v) `SDSNTH` is `MTH/2-1` in integer division; the GIMPLE builds the extent `2*SDSNTH+1` as
  `(MTH/2)*2-1`, the same integer. With `MTH >= 4`, `SDSNTH >= 1`, so no extent is zero.
- (v) Extents: the byte count of `CUMULW(MSPEC,MSPEC)` is 64-bit in gfortran (`MSPEC` widened to
  `kind=8` before the product) and in Kokkos (`size_t` extents). `MSPEC = MK*MTH` itself is a
  32-bit product in both and overflows only for `MK*MTH > 2^31-1` ⚠ (not reachable by any grid
  of the regtests, not checked).
- (v) `W3SETG` (`:2116`): its checks (`:2262-2270`) repeat the ones already passed; it sets
  `IGRID`, `ISGRD`, `IPARS` (`:2278-2280`), which `gm` mirrors; its pointer associations
  (`:2498-2534` for `SGRDS`, `:2707-2712` for these `SRCPS` arrays) are, in C++, the caller
  holding references to this model's `sg` and `sp`. `SINIT` is set before the call (`:2108`),
  as `W3SETG` associates the spectral arrays only when it is true (`:2518`).
- ⚠ `W3SETG` also re-associates every other `GRIDS(IMOD)` and `MPARS(IMOD)` pointer of the
  module. Those belong to the shim's per-model context and are not mirrored here.
- ⚠ An `ALLOCATE` of a pointer component that is still associated is legal in Fortran and leaks;
  the C++ would release the old View. `SINIT` false rules it out on every path through `W3NMOD`,
  but a caller that keeps the structs across a `W3DIMS` failure is not checked.
- ⚠ Only the host (OpenMP) build was compiled. On CUDA the same `deep_copy` would be a
  `cudaMemsetAsync` for the zeros and a device fill for the 1s, with no arithmetic either way;
  not compiled, not checked.

## What is not ported

- `W3_S` (`STRACE`, `:2002-2004`, `:2021-2024`) and the `TEST_W3GDATMD` writes (`:2051-2053`,
  `:2109-2111`, `:2117-2119`, `:2128-2130`): not compiled under the lab switch.
- The `WRITE (NDSE, 100x)` messages: the shim writes them from the returned code.
- The rest of `W3SETG`, and the arrays `SGRDS` and `MPARS` themselves (`W3NMOD`, `:1544-1547`):
  the caller keeps one `Sgrd` and one `Srcps` per model.
- Without `ST4` the `SRCPS` block is not compiled in the Fortran; the C++ always runs it, as the
  lab switch has `ST4`. Another switch would need a flag here.

The `W3IOGR` translation already shapes the same arrays on the host, as `std::vector`s of its
own `ww::iogr::IogrState` (`iogr_shape_w3dims`, `../src/ww_kokkos/iogr_state.hpp:276-306`); the
two should be merged at the shim.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet: no WW3 regtest ran, and the only execution was the reviewer's throwaway host run
(not kept, ADR-0010) named under Parity. What was done, 2026-10-16 (v): a throwaway translation unit in
the pinned `ww3` shell (Kokkos 5.2.2, OpenMP) with the `openmp-release` flags (`-O3
-march=x86-64-v3 -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wno-unused-parameter -ffp-contract=off -fopenmp`) explicitly instantiated
`w3dims<Kokkos::DefaultExecutionSpace>` and compiled with no warning and no FMA instruction in
the object. Not compiled for CUDA ⚠.
