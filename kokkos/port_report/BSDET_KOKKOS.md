# BSDET_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term, "V1" path: the determinant of an `N x N`
`DOUBLE COMPLEX` matrix by Gaussian elimination with full pivoting (largest `ABS` over the
remaining submatrix, row and column swaps each flipping the sign `F`), returning `0` when the pivot
`Q` satisfies `Q + 1.0 == 1.0` (`w3sic3md.F90:1683-1726`). `FUNC1_ZHAO` calls it on its 4 x 4
dispersion-relation matrix (`:1599`); `DRFUN_DBLE_CHENG` also calls it (`:2948`), but nothing in
`model/src` calls `DRFUN_DBLE_CHENG` (`git grep -n DRFUN_DBLE_CHENG` finds only its definition).
`FUNC1_ZHAO` is the `JUDGE /= 0` branch of `F_ZHAO_V1` (`:1366`), reached from `W3IC3WNCG_V1`
when `IC3CHENG` is false. The routine's own remark (`:1659-1660`) says the Cheng update does not
use it.

## What was ported

| | |
|---|---|
| WW3 routine | `BSDET`, `model/src/w3sic3md.F90:1611-1730` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'`, less `model/src/cmake/switches.json`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised only by namelists with `IC3CHENG = F`. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_bsdet.hpp`](../src/ww_kokkos/ic3_bsdet.hpp), `ww::ic3::bsdet(const Kokkos::complex<double>* aa, int n, Kokkos::complex<double>* mat) -> Kokkos::complex<double>`; `aa` column major, Fortran `AA(I,J)` at `aa[(I-1) + (J-1)*n]`; `mat` is a caller-provided work array of `n*n`, the Fortran's automatic `MAT(N,N)`, which the object allocates with `malloc` and frees on every call (a kernel may not allocate); `ww::ic3::bsdet_cabs` is its `ABS` of a complex |
| C shim | none yet; its caller `FUNC1_ZHAO` is not ported |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly with
relocations read against those of a throwaway C++ unit compiled with the `openmp-release` flags
and `-ffp-contract=off`, 2026-10-16. The C++ unit compiles warning-free.

- (v) `BSDET` is not inlined into its callers: the object keeps `__w3sic3md_MOD_bsdet` and the
  three call sites (`:1599` twice after inlining of `FUNC1_ZHAO`, `:2948`) call it with `N = 4`
  passed by reference, so the general-`n` function is what runs.
- (v) `ABS(MAT(I,J))` is a call to glibc `cabs` in the Fortran object (`R_X86_64_PLT32 cabs`);
  the host path calls `std::abs(std::complex<double>)`, which libstdc++ lowers to
  `__builtin_cabs`, and the C++ object has the same `cabs` relocation. The search keeps the
  first maximum in column-major order of the `I`, `J` loops with a strict `>`, as the Fortran.
- (v) `Q + 1.0 == 1.0` is an `addsd` then `ucomisd` against `1.0` in both objects; the early
  return gives `(0.0, 0.0)`.
- (v) Every complex product is gfortran's inline form (no `__muldc3`): `DET * MAT(K,K)` is
  `(det_r p_r - det_i p_i, det_i p_r + det_r p_i)`; `D * MAT(K,J)` and the final
  `F * DET * MAT(N,N)` (left to right) likewise; the header writes them component by component
  rather than through `Kokkos::complex` operators. Per site the Fortran and C++ objects have the
  same count of `mulsd`, `addsd`, `subsd` on the same operands (4 products, 1 subtraction,
  1 addition per product; 3 subtractions in the update `MAT(I,J) - D*MAT(K,J)`).
- (v) The quotient `D = MAT(I,K) / MAT(K,K)` is Smith's algorithm inlined (no `__divdc3`):
  branch on `|p_r| < |p_i|`, ratio, denominator, two numerators, two divisions, the same in both
  GIMPLEs; a NaN takes the `ELSE` branch in both. `Kokkos::complex`'s `operator/` would not give
  these bits.
- (v) `F = -F` negates both components (`xorpd` with the sign mask in both objects; g++ packs the
  two into one vector `xor`, the same bits). The copy `MAT = AA` and the row and column swaps are
  moves, vectorised as 16-byte moves in the Fortran object; no arithmetic.
- ⚠ g++ commutes some products and additions relative to the Fortran object, exact in IEEE
  arithmetic; only which NaN survives when both operands are NaN can differ (the first source
  operand's). Traced per instruction: the four products of `DET * MAT(K,K)` (`:1717`) have
  `MAT(K,K)` first in the C++ and `DET` first in the Fortran, and the `ELSE` branch's
  denominator of `D` (`:1719`) is `ratio*p_i + p_r` in the C++ against `p_r + p_i*ratio` in the
  Fortran. Swapping the source order does not move g++'s choice; left as is. Every other site
  (the update `:1721`, the final product `:1726`, the `THEN` branch) has the Fortran's first
  operands.
- No reduction, no MIN/MAX, no `INT`/`NINT`, no libm call other than `cabs`, no libmvec call, no
  FMA in either object.
- (v) Fresh review 2026-10-16: Fortran object recompiled and checked identical to `build_mixa`'s;
  GIMPLE and the disassembly of both (the C++ also with `-g`, same code, for line numbers) read
  site by site: `cabs` calls, the pivot search and `Q + 1.0 == 1.0`, Smith's quotient, every
  complex product, the sign flips. No defect outside NaN payloads; the two NaN-only sites above
  are recorded, not fixed. Header unchanged.
- ⚠ For `N <= 0` the Fortran reads `MAT(N,N)` outside its allocation (`:1726`, index
  `(N-1)*(N+1)` on a 1-byte `malloc`); every caller passes 4, and the C++ requires `n >= 1`.
- ⚠ On the device `bsdet_cabs` uses `Kokkos::abs`, not glibc's `cabs`; not bit-checked. CUDA not
  compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
