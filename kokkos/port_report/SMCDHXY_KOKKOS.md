# SMCDHXY_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Water-depth gradients on a Spherical Multiple-Cell (SMC) grid and the refraction limiter.
`HCel` is `DW` raised to `DMIN` in shallow cells; `SMCGradn` gives its cell gradients
`DHDX`, `DHDY`, each limited to magnitude 0.1 and copied to every point of the full `NY` x `NX`
grid the cell covers (`DDDX`, `DDDY`), then rotated to map east in the Arctic part (`ARCTC`,
cells `n > NGLO`). The directional limiter is
`DHLMT(i,n) = MIN(Refran, 0.75*MIN(θ, |π-θ|))/DTH` with
`θ = ACOS(-(DHDX*ECOS(i) + DHDY*ESIN(i))/(1.0001*|∇H|))`, and 0 where `1.0001*|∇H| <= 1.0E-5`.
`W3WAVE` calls it when the depth field changes on an SMC grid (`w3wavemd.F90:1486`, under
`W3_SMC`, `FLDDIR`).

## What was ported

| | |
|---|---|
| WW3 routine | `SMCDHXY`, `model/src/w3psmcmd.F90:2903-3036` at NOAA-EMC/WW3 `03adda91` (134 lines); compiled by every switch with `SMC`: `model/bin/switch_SMCMlt`, `switch_UKMO_gbl`, `switch_UKMO_uk`, `regtests/ww3_tp2.10`, `ww3_tp2.16`, `mww3_test_09`, `ww3_tp2.14/input_oasacm6` (`git grep -lw SMC -- '*switch*'`); `ww3_tp2.10` and `ww3_tp2.16` `switch_MPI_OMPH` carry `OMPG` |
| Kokkos code | [`../src/ww_kokkos/psmc_smcdhxy.hpp`](../src/ww_kokkos/psmc_smcdhxy.hpp): `smcdhxy_grad_point` (one iteration of `DO n`, `:2952-2973`), `smcdhxy_dhlmt_point` (one iteration of `DO n`, `:2988-3017`) and the host driver `smcdhxy` |
| Not ported | `SMCGradn` (`:2341-2539`), called once (`:2942`): the driver takes it as a caller-provided callable until it is ported. `HCel`, `GrHx`, `GrHy` (automatic arrays, `:2915`) are caller-provided work arrays |
| `#ifdef` | `W3_OMPG` makes the three `DO` loops parallel; the arithmetic of the outlined bodies is the same (below). `W3_T` only counts refraction points in `L` (reset after `SMCGradn` has read it) and writes them: output, not translated |
| C shim | none yet |
| Plan | [`BATCH3_SHIMS_KOKKOS_PLAN_202610.md`](../../docs/BATCH3_SHIMS_KOKKOS_PLAN_202610.md), issue #42 |

## Arithmetic

Reference: the `-O3` object of `w3psmcmd.F90` built with the flags of WW3's CMake Release build
under `switch_SMCMlt` (gfortran 16.2.0, pinned shell), identical to the CMake object, and the same
file with `-DW3_OMPG -fopenmp`; compared with a throwaway C++ unit compiled with the
openmp-release flags (g++ 16.2.0, warning-free), 2026-10-16.

- (v) Constants: the same six folded `REAL` values in both GIMPLE dumps: 0.1
  (`1.00000001490116119384765625e-1`), `DERA` (`ww::prop::kDera`), 1.0001
  (`1.00010001659393310546875`), 1.0E-5, `PI` (`3.1415927410125732421875`, `ww::kPi`), 0.75.
- (v) Shallow cells: `DW(k) < DMIN` then `HCel(k) = DMIN`; a NaN depth fails the test and stays.
- (v) Limiter: `ABS(x) > 0.1` then `SIGN(0.1, x)` as `copysignf`, done with `andps`/`orps` in
  both objects; a NaN fails the test and is kept.
- (v) Order: the full grid receives the limited gradient before the Arctic rotation (`:2963-2964`
  before `:2968`), the reverse of `SMCDCXY`.
- (v) Rotation: one `sincosf` call in both objects, `DHDX*cos - DHDY*sin` and
  `DHDX*sin + cos*DHDY` with the same first operand of the `subss` and of the `addss`.
- ⚠ Rotation products: the Fortran object multiplies `DHDX*cos`, `DHDY*sin`, `DHDX*sin` with the
  gradient as first source, the C++ object with `cos` or `sin` first (`cos*DHDY` matches); writing
  the C++ operands in either order, or through named temporaries, gives the same C++ object. The
  bits differ only when both factors are NaN, which needs a non-finite `ANGARC(n-NGLO)*DERA`.
- (v) Refraction limiter: `sqrtss(DHDX*DHDX + DHDY*DHDY) * 1.0001`; then per direction
  `(DHDX*ECOS(i) + DHDY*ESIN(i)) / CNST4`, negated after the division (`xorps`), scalar `acosf`
  (libm, not libmvec, in both); `MIN(CNST6, ABS(PI-CNST6))` is `minss` with `|π-θ|` as first
  operand, `MIN(Refran, 0.75*m)` is `minss` with `0.75*m` as first operand, in both objects, then
  `divss` by `DTH`. No FMA in either; the Fortran object vectorises nothing in this routine and the C++ refraction loop stays scalar.
- (v) Indices: `IJKCel(r,n)/MRFct` is a truncating `idiv` in both, `MAX(1, ...)` on the sizes.
- (v) `W3_OMPG`: the outlined bodies `smcdhxy._omp_fn.0/1/2` have the same compare, limiter,
  `sincosf`, `acosf` and `minss` sequences. The parallel grid loop writes disjoint blocks only
  when no two cells cover the same grid point, which an SMC cell list satisfies; the C++ driver
  runs the loops serially, the point functions can run per `n` in a parallel loop under the same
  condition.
- (v) The block fills, copies and `= 0.0` clears copy values only (the C++ clears `DHLMT(:,n)`
  with `memset`, the Fortran with a loop); their order does not change bits.
- ⚠ Bit parity of the whole routine also needs `SMCGradn`, which is not ported.
- ⚠ CUDA not compiled for this header; `acosf` on a GPU is not glibc's.
- ⚠ `DDDX`, `DDDY` are taken as `(NY,NX)`; under `PDLIB` WW3 allocates them `(1,NSEAL)`
  (`w3adatmd.F90:1384`), a layout the header does not serve.
- (v) Fresh review 2026-10-16: no arithmetic defect. The module recompiled from
  `ref2/build_smc`'s flags is identical to the CMake object; the C++ unit compiled warning-free;
  the six `.rodata` constants, every float instruction, the `minss` operand order, the `sincosf`,
  `acosf` and `sqrtss` sites and the NaN branches were read against the serial object and the
  `W3_OMPG` outlined bodies. The review corrected the rotation bullet above (the Fortran
  products have the gradient first) and added the two ⚠ above; the header is unchanged.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
