# DDOT_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The LINPACK dot product of two REAL(8) vectors, unrolled by 5 (J. Dongarra, 1978), that the
SPARSKIT iterative solvers inside `W3PROFSMD` use. It is `PRIVATE` (`w3profsmd.F90:61-65`). It is
reached in a run only through `BCGSTAB` (`:2135`, `:2140`, `:2201`, `:2261`, `:2267`, `:2307`) and
`STOPBIS` (`:2524`, `:2530`, called from `BCGSTAB` at `:2292`), which `RUNRC` calls from
`W3XYPFSNIMP` (`:1239`), the implicit N scheme of unstructured grids that `W3XYPUG` selects with
`FSNIMP` (`:257-258`; namelist `UNST IMPFSN = T`). The calls in `MGSRO` (`:2742-2801`) and
`PGMRES` (`:4450`) are dead: nothing calls those two routines.

## What was ported

| | |
|---|---|
| WW3 routine | `DDOT`, `model/src/w3profsmd.F90:4684-4710` at NOAA-EMC/WW3 `03adda91`; `w3profsmd.F90` is built with switch `PR1`, `PR2` or `PR3`; no `#ifdef` inside the routine. The one regtest that sets `IMPFSN = T` is `ww3_ts4` with `-i input_ug` (`regtests/ww3_ts4/input_ug/ww3_grid.inp:44`, `namelists_Mediterranean.nml:1`; switch `PR3 ... ST0 NL0`, serial and MPI entries of `matrix.base`) |
| Kokkos code | [`../src/ww_kokkos/profs_ddot.hpp`](../src/ww_kokkos/profs_ddot.hpp), `ww::profs::ddot(n, dx, dy)`, `dx` and `dy` 0-based for `DX(1:N)`, `DY(1:N)` (assumed size, unit stride) |
| C shim | none yet; it would come with `BCGSTAB` and `STOPBIS` |
| Plan | issue #42, batch of the easiest routines left in Fortran |

## Arithmetic

Reference: the `-O3` object of `w3profsmd.F90` as WW3's CMake Release build compiles it
(gfortran 16.2.0, `switch_lab_shrd`, recompiled and checked identical to the CMake object),
its `-fdump-tree-optimized-lineno` GIMPLE and disassembly, against a throwaway C++ unit with the
`openmp-release` flags and `-ffp-contract=off`, 2026-10-16.

- (v) The remainder loop (`:4699-4701`): gfortran multiplies two elements at a time (`mulpd`)
  and adds the products to `DDOT` one by one, low lane then high lane (`addsd`, `unpckhpd`,
  `addsd`), an in-order reduction. g++ with `-march=x86-64-v3` multiplies four at a time
  (`vmulpd`) and also extracts and adds the lanes in order. A product is the same IEEE product
  either way, so the sequence of rounded adds is the same: `((0 + p1) + p2) + ...`.
- (v) The unrolled loop (`:4705-4708`): five `mulsd` and five `addsd` per pass in both objects,
  left-associative from `DDOT`, the same order as the source. The first add is written
  `p + DDOT` in the Fortran object; IEEE addition is commutative, so the bits are the same.
- (v) No FMA (`grep -c vfmadd` gives 0 on the C++ object), no libm call, no reassociation.
- `n <= 0` returns 0 (`:4695`); `MOD(N,5)` is `n % 5` because `n > 0` there.
- ⚠ Operand order of the remainder adds: gfortran writes every one as `DDOT + p`
  (`addsd %xmm1,%xmm0`, products as `dx*dy` in the pair, `dy*dx` in the odd tail); g++ writes
  some as `p + DDOT` (`m = 4`: the first two adds; `m = 2, 3`: the first add). The sum is
  the same for every input that is not NaN; when both operands are NaN with different payloads,
  x86 returns the first operand's payload, so the NaN's bits can differ. It cannot be pinned from
  the source, since g++ orders commutative operands itself. Not checked by any run.
- CUDA not compiled (optional this batch).
- (v) Fresh review 2026-10-16: no defect for non-NaN input. `w3profsmd.F90` recompiled from the
  `build_lab` flags (identical to the CMake object) and the C++ unit with the `openmp-release`
  flags, both read instruction by instruction: same products, same left-to-right add sequence,
  no FMA, no libm; the only difference is the NaN-payload operand order above, added here as ⚠.
  Callers, `PRIVATE` list and the `ww3_ts4 -i input_ug` regtest read again at `03adda91`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
