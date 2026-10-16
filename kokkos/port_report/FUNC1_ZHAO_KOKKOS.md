# FUNC1_ZHAO_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

A helper of the IC3 sea-ice source term, "V1" path: the residual of the Zhao and Shen dispersion
relation for waves under a viscoelastic ice layer of thickness `HICE`, density `DICE`, effective
shear modulus `ES` and viscosity `NU`, at a complex wavenumber k, angular frequency `SIGMA` and
depth `DEPTH` (`w3sic3md.F90:1465-1608`). With the complex viscosity
`VE = CMPLX(NU, ES/DICE/SIGMA)` and `ALPHA = SQRT(k**2 - i SIGMA/VE)`, it returns the determinant
of a 4 x 4 complex matrix (`BSDET`, `:1599`) when `ES > 1E7`, and a closed form otherwise
(`:1601-1605`). It is the `JUDGE /= 0` branch of `F_ZHAO_V1` (`:1366`), whose root
`CMPLX_ROOT_MULLER_V1` finds; the chain is reached from `W3IC3WNCG_V1`, called by `W3SIC3`
and `W3WAVE` when `IC3CHENG` is false. The routine's own remark (`:1526-1527`) says the Cheng
update does not use it.

## What was ported

| | |
|---|---|
| WW3 routine | `FUNC1_ZHAO`, `model/src/w3sic3md.F90:1465-1608` at NOAA-EMC/WW3 `03adda91`; compiled under switch `IC3`, by 46 regtest switch files (`ww3_tic1.1`, `ww3_tic1.2`, `ww3_tic1.3`, `ww3_ts1`; `git grep -lw IC3 -- '*switch*'` lists them and `model/src/cmake/switches.json`), by no NOAA operational switch (`model/bin/switch_NCEP_*`, `switch_NWS_*`); exercised only by regtest namelists with `IC3CHENG = F`. Its only caller is `F_ZHAO_V1` (`:1366`). No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/ic3_func1_zhao.hpp`](../src/ww_kokkos/ic3_func1_zhao.hpp), `ww::ic3::func1_zhao(const Kokkos::complex<double>& wn, double sigma, double es, double nu, double dice, double hice, double depth) -> Kokkos::complex<double>`, and the functor `ww::ic3::Func1Zhao`, the `func1` callable that `f_zhao_v1` (`ic3_f_zhao_v1.hpp`) takes. It calls `bsdet` (`ic3_bsdet.hpp`) and reuses the `cexp`, `csqrt`, `cpow` wrappers of `ic3_drfun_quad_cheng.hpp`. The Fortran's `AA(4,4)` and `BSDET`'s automatic `MAT(4,4)` are fixed-size local arrays (no allocation) |
| C shim | none yet; its caller `F_ZHAO_V1` is translated but not wired |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3sic3md.F90` (WW3's CMake Release flags, gfortran 16.2,
`build_mixa`: `switch_lab_shrd` plus `TIDE UOST FLD2 IG1 IC3 IS2 BT8`), recompiled and checked
identical to the CMake object; its GIMPLE (`-fdump-tree-optimized-lineno`) and disassembly with
relocations read against those of a throwaway C++ unit compiled with the `openmp-release` flags
and `-ffp-contract=off`, 2026-10-16. The C++ unit compiles warning-free.

- (v) gfortran expands every complex product and quotient inline (Fortran rules, no `__muldc3`
  or `__divdc3`). The header writes them through three helpers in Fortran operand order:
  `mul` `(ar br - ai bi, ar bi + ai br)`, `div` (Smith's algorithm in GCC's form: branch on
  `|br| < |bi|`, ratio, denominator, two numerators, two divisions), and `add`/`sub`/`neg`
  component-wise. A REAL operand is passed promoted, `(x, 0.0)`, as gfortran promotes it: the
  products with `0.0` stay in both GIMPLEs; products with `1.0` and the subtraction of `0.0`
  fold in both; `2.*CMPLX(0.,1.)` folds to `(0.0, 2.0)` in both; the divisions by `2.` become
  `* 0.5` and those by `DBLE(DWAT)` stay `/ 1000.0` in both.
- (v) Every `z**p` with a REAL literal `p` (`WN**2.`, `WN**3.`, `WN**6.`, `ALPHA**2.`, `VE**2.`,
  `VE**4.`, `N**2.`, `N**4.`) is a call to glibc `cpow(z, (p, 0.0))` in the Fortran object, eight
  calls in all; `SQRT` is one `csqrt`, `EXP` six `cexp` (the four of `SK`, `CK`, `SA`, `CA`, `EXP(+x)`
  before `EXP(-x)`, and the two of `TH`, `EXP(-x)` first). The C++ object of `func1_zhao` has
  the same eight `cpow`, one `csqrt` and six `cexp` relocations, on the same arguments.
  `SIGMA**2.` and `SIGMA**2` fold to `SIGMA*SIGMA` in both.
- (v) `VE = CMPLX(NU, ES/DICE/SIGMA)` (`:1556`) has no `KIND=`, so both parts are rounded to
  `REAL` (float) and widened back: one narrowing per part on every path (the Fortran object
  duplicates the pair over three paths, `cvtpd2ps` or two `cvtsd2ss`; the C++ object has two
  `vcvtsd2ss`). The header writes
  `static_cast<double>(static_cast<float>(...))`. This loses the double precision of `NU` and
  `ES/DICE/SIGMA` in WW3 itself; the port reproduces it.
- (v) `THH` (`:1568-1569`, `:1572`) is never read; the Fortran object computes no `THH` (its four
  `cexp` are removed as dead), and the header does not compute it.
- (v) `M` (`:1575-1576`): `DBLE(DWAT)/DICE*SIGMA**2` is the real `(SIGMA*SIGMA)*(1000/DICE)` in
  both, then `(x, 0.0)/TH` by Smith; with `TH = 1.0` both objects fold the quotient's ratio to
  `0.0` and its denominator to `1.0` along the same path (`0.0 + x`, `0.0 - 0.0*x`).
- (v) `AA` (`:1579-1597`): every element read against the stores of both objects (column major,
  `AA(I,J)` at `aa[(I-1) + (J-1)*4]`); `-N*SIGMA*SK + M*CK` is `M*CK - N*SIGMA*SK` in both GIMPLEs
  (exact). The Fortran calls `BSDET` out of line; the C++ inlines `bsdet` with `n = 4`, which
  keeps its operations (`ic3_bsdet.hpp`, [BSDET_KOKKOS.md](BSDET_KOKKOS.md)).
- (v) The closed form (`:1601-1605`): `TH*DICE/DBLE(DWAT)` is `(TH*(DICE, 0.0)) / (1000, 0)`, not
  `TH * (DICE/1000)`; `DBLE(GRAV)**2.` folds to its exact square `0x1.80a16a139p+6`
  (`static_assert`); numerator, denominator and the final Smith quotient read operation by
  operation in both GIMPLEs.
- (v) `DBLE(GRAV)` is folded to `0x1.39cacp+3` (`GRAV = 9.806` as `REAL`, `constants.F90:62`)
  with a `static_assert`; `DWAT = 1000.` (`constants.F90:63`) is exact.
- (v) The tests `REAL(WN*DEPTH) <= 4.` and `ES > 1.E7` go to the `ELSE` branch for a NaN in both
  objects. No FMA in the C++ object (`-ffp-contract=off`).
- Some additions and multiplications have their operands commuted between the two GIMPLEs (for
  example `EXP(-x) + EXP(+x)`, `ALPHA**2 + WN**2`, the imaginary part of `M*SK`) and both
  compilers SLP-vectorise different pairs lane by lane. Each is the same IEEE operation on the same
  operands, so the bits are the same; only which NaN payload survives when both operands are NaN
  could differ ⚠ (not checked).
- No reduction, no MIN/MAX, no `INT`/`NINT`, no libmvec call.
- ⚠ On the device the `cexp`, `csqrt`, `cpow` wrappers use Kokkos's algorithms, not glibc's, and
  `bsdet`'s `cabs` is `Kokkos::abs`: not bit-checked. CUDA not compiled.
- (v) Fresh review 2026-10-16: no defect in the arithmetic. The Fortran object was recompiled and
  checked identical to `build_mixa`'s; both units were compiled again without SLP and loop
  vectorisation (`-fno-tree-slp-vectorize -fno-tree-vectorize`, which do not reorder an IEEE
  operation) and their optimised GIMPLE rebuilt as expression DAGs from the inputs, with each
  Smith branch and the jump-threaded `TH`/`M` paths tied to their conditions. The result and all
  32 parts of `AA` are equal operation by operation, constants and `cpow`/`cexp`/`csqrt`
  arguments included, once the operands of `+` and `*` are sorted; no subtraction, division,
  negation or association differs (a swapped subtraction planted in the C++ dump is caught). The
  `-O3` objects call the same eight `cpow`, six `cexp`, one `csqrt` (the C++ also four `cabs`, from
  the inlined `bsdet`); no FMA. `BSDET` itself is checked in [BSDET_KOKKOS.md](BSDET_KOKKOS.md).
  Fixed in this report: the switch-file count (46, not 47) and the conversion count.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
