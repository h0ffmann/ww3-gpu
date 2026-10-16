# COUPLE_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The four-wave coupling coefficient of Webb (1978) for wavenumbers k1, k2, k3, k4, in double
precision. `INSNLGQM` calls it four times per quadrature node (`w3snl1md.F90:1972-1973`,
`:2057-2058`) to fill `TB_TPM` and `TB_TMP`, the tables of the Gaussian Quadrature Method (GQM)
form of `NL1`, which `W3IOGR` selects on reading a `mod_def` when `IQTPE <= 0`.

## What was ported

| | |
|---|---|
| WW3 routine | `COUPLE`, `model/src/w3snl1md.F90:1185-1307` at NOAA-EMC/WW3 `03adda91`, adapted from TOMAWAC (M. Benoit, E. Gagnaire-Renou). Compiled wherever `NL1` is (392 switch files under `git grep -lw NL1 -- '*switch*'`; NOAA operational `switch_NCEP_gwm`, `switch_NCEP_st2`, `switch_NCEP_st4`, `switch_NCEP_st4sbs`, `switch_NWS_rwps`); reached only through `INSNLGQM`, with `IQTYPE <= 0` in the `SNL1` namelist. No `#ifdef` inside the routine |
| Kokkos code | [`../src/ww_kokkos/snl1_couple.hpp`](../src/ww_kokkos/snl1_couple.hpp), `ww::snl1::couple`, `KOKKOS_INLINE_FUNCTION` |
| C shim | none yet |
| Plan | issue #42, batch 3 |

The Fortran writes three lines to unit `*` when a denominator `DENO13` or `DENO14` is below
`1.D-10` in magnitude, and then leaves that term out. The port does not print: it leaves the term
out the same way and sets a bit of its `diag` argument per message (`couple_detail::kNan13`,
`kInf13`, `kNan14`, `kInf14`), so the shim can write the messages. The value returned is the
Fortran's in every case.

## Arithmetic

Reference: the `-O3` object of `w3snl1md.F90` built with the flags of WW3's CMake Release build
under `switch_lab_shrd` (gfortran 16.2.0, pinned shell), identical by disassembly to the CMake
object; compared with a throwaway C++ unit under the `openmp-release` flags (compiles warning
free), 2026-10-16. In the Fortran object `COUPLE` is not inlined: `INSNLGQM` calls
`__w3snl1md_MOD_couple` four times.

- (v) `PI = ACOS(-1.)` (`:1242`) is a default-REAL `ACOS` and `GRAV` a REAL parameter
  (`constants.F90:62`): gfortran folds `COEF = PI*GRAV*GRAV/4.D0` (`:1243`) to
  75.5220290520783663623660686425864696502685546875 (`0x1.2e168ec8a8f43p+6`), from the float pi
  and the float 9.806 widened to double. The header writes it as that hex literal with a
  `static_assert` of how it is built; the C++ object's constant pool holds the same bits.
- (v) Every double operation of `:1246-1305` is the same in both optimised GIMPLE dumps, in the same
  association: the Webb `D` sum (`:1272-1278`) left to right, `0.25D0*(S13+S24)*Q13*Q13` as
  `((0.25*s)*q13)*q13`, `2.50D0*RK1*RK2*RK3*RK4` left to right, `COEF*DDD*DDD` as
  `(ddd*COEF)*ddd`, the divisor `((WK1*WK2)*WK3)*WK4`. Every parenthesis the Fortran marks
  (`PAREN_EXPR`) encloses a single operation that g++ also keeps. No FMA, no vectorisation in
  either object.
- (v) `SQRT`: 11 bare `sqrtsd` in both objects, no libm call in either (the C++ object has no
  call at all). `ABS` is an `andpd`; the `< ZERO` tests are not-less on an unordered compare in
  both, so a NaN denominator takes the branch that adds the term, as in the Fortran.
- ⚠ The operand order of the commutative additions and multiplications differs between the two
  assemblies at up to 16 operations (for example `RK1` is `xk1*xk1+yk1*yk1` in the Fortran object and
  `yk1*yk1+xk1*xk1` in the C++ one; `2.50D0*RK1` is `2.5*rk1` against `rk1*2.5`). Read
  instruction by instruction, every result is the same tree up to that commutation on all four
  branch paths. On x86 the order decides only which NaN payload survives when both operands are
  NaNs with different payloads; every NaN the hardware creates from finite or infinite inputs is
  the same default NaN, so it needs a NaN among the wavenumber components on input (whose payload
  can then meet the default NaN or another input NaN).
- ⚠ WW3: the messages say `W3SNL2` although the routine is in `W3SNL1MD` (`:1284-1300`), and
  "inifinte" is misspelt; the shim should copy the text as written.
- CUDA: not compiled this batch.
- (v) Fresh review 2026-10-16: no defect. The Fortran module was recompiled into the review's
  scratch directory (identical to the CMake object of `build_lab`) and both functions were
  evaluated symbolically from their `-O3` assembly along the four branch paths (term 1-3 and term
  1-4 each added or left out): the returned expression, the constants (`COEF` included) and the
  branch conditions are identical up to the commutative operand order listed above, which is NaN
  payload only. The NaN branches (unordered compare takes the add path) and the `diag` bits match
  the Fortran's WRITEs. Nothing was changed in the header.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
