# SVD_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Least-squares solution of the tidal harmonic fit by singular value decomposition: copies the
design matrix `Q` into `U`, decomposes it with `DSVDCMP`, zeroes the singular values below
`TOLER*max(w)`, solves with `DSVBKSB`, and returns the residuals in the last column of `Q`, their
sum of squares `SSQ`, the largest residual `RES` and the covariance `COV = V diag(1/w^2) V^T`
(switch `TIDE`). Its only caller is `flex_tidana_webpage` (`w3tidemd.F90:2308`), called by the
forcing preprocessor `ww3_prnc` (`ww3_prnc.F90:1675`). It never runs inside `ww3_shel` or
`ww3_multi`.

## What was ported

| | |
|---|---|
| WW3 routine | `SVD`, `model/src/w3tidemd.F90:1313-1443` at NOAA-EMC/WW3 `03adda91` (J. Cherniawsky, 1997-98); `w3tidemd` is compiled by every switch, `SVD` is reached only through `ww3_prnc` with `TIDE` (`ww3_tp2.18/input/switch_TIDE`, `switch_TIDE_MPI`; `model/bin/switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASACM`, `switch_OASICM`, `switch_OASOCM`, `switch_ite_pdlib`; `git grep -lw TIDE -- '*switch*'`), no NCEP or NWS operational switch. No `#ifdef` in the routine |
| Kokkos code | [`../src/ww_kokkos/tide_svd.hpp`](../src/ww_kokkos/tide_svd.hpp), `ww::tide::svd`; `Q(MM,N2)` (REAL), `U(MM,N2)`, `V(N2,N2)`, `COV(N2,N2)` column-major; `WTI(NWT=302)` stays a local array, as in the Fortran (not `SAVE`); calls `ww::tide::dsvbksb` ([`DSVBKSB_KOKKOS.md`](DSVBKSB_KOKKOS.md)) |
| Not translated | `DSVDCMP` (`:1023-1254`): taken as a caller-provided callable `dsvdcmp(a, m, n, mp, np, w, v)` |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3tidemd.F90` (WW3's CMake Release flags, gfortran 16.2) in
`build_mixa` (`switch_lab_shrd` plus `TIDE`) and `build_lab` (`switch_lab_shrd`), recompiled and
found identical to both CMake objects. Compared with the GIMPLE and object of a C++ unit compiled
with the openmp-release flags (`DSVDCMP` an external function behind the callable), 2026-10-16.

- (v) `WMAX` and `THRESH` are REAL: both objects compare `w(j)` with the widened `wmax`
  (`comisd`), store `(float)w(j)`, form `thresh = toler*wmax` in single precision (`mulss`) and
  compare `w(j)` with the widened `thresh`.
- (v) `WTI(J) = 1/(W(J)*W(J))` where `W(J) > 1.D-10`: one `mulsd`, one `divsd` in both, the
  constant `1.00000000000000003643e-10` in both GIMPLE dumps.
- (v) Residual loop (`:1419-1429`): `sum` adds `p(j)*dble(q(i,j))` in `j` order, scalar in both,
  `sum` the first operand of `addsd` in both. `RES = MAX(RES, RESI)` is `maxsd` with the widened
  `res` as first operand in both (`res > resi ? res : resi`), rounded to REAL. gfortran squares
  the difference `b(i)-sum`, not `resi`, for `RESI**2`; the port does the same (`d*d`). `SSQ`
  is widened, added in double, rounded to REAL on every `i`, in both.
- (v) Covariance (`:1432-1441`): `sum` adds `v(i,k)*v(j,k)*wti(k)` in `k` order. gfortran
  multiplies two `k` per `mulpd` and adds the lanes to `sum` one after the other; g++ does it
  four lanes at a time, also lane by lane in order. A lane operation is the scalar operation, so
  finite values give the same bits. No FMA (no `vfmadd` in the C++ object), no libm call.
- ⚠ Operand order where a double multiply or add commutes: `v(j,k)*v(i,k)` against
  `v(i,k)*v(j,k)`, `wmax*toler` against `toler*wmax`, and `sum+term` against `term+sum` in some
  vector lanes and scalar tails of the covariance sum. Only two NaN operands of distinct payloads
  can give a different NaN; no finite or single-NaN input differs.
- ⚠ With `IC == 2` the Fortran reads `U`, `W` and `V`, declared `INTENT(OUT)`, as the output of
  an earlier call; the port reads the caller's arrays the same way.
- ⚠ `N > 302` overruns `WTI` in the Fortran and in the port; the caller passes
  `N = NMAX <= NMAXP1 - 1 = 139` (`:93`, `:2308`).
- ⚠ WW3 defect: the caller passes `ICODE` (`:2308`), a local of `flex_tidana_webpage` (`:2132`)
  that is never assigned there, so `IC` is undefined; with `IC == 2` by chance the decomposition
  is skipped and uninitialised `U`, `W`, `V` are used. The port reproduces whatever `ic` it is
  given.
- ⚠ `SIG` is unused, in the Fortran and in the port.
- ⚠ CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect. The module was recompiled from `build_mixa`'s flags
  (identical to its CMake object) and the assembly of `SVD` read against the C++ function's
  (where g++ inlines `dsvbksb`, reviewed with its own header): the `wmax` and `thresh` compares
  and roundings, `WTI`, the residual loop (`sum` first operand, `b-sum`, `(b-sum)**2`, `maxsd`
  with the widened `res` as first source in both, `SSQ` and `RES` rounded each `i`) and the
  covariance sum added in `k` order lane by lane in both. The only differences are the
  commutative operand orders listed above (NaN payload only). Nothing was changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
