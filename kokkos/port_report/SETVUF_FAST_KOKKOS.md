# SETVUF_FAST_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

Computes the nodal corrections of the `TIDE_MF` analysed tidal constituents at one time and one
latitude: the nodal factor `f`, the nodal phase `u` and the astronomical argument `V` (cycles),
from the mean longitudes `h, pp, s, p, enp` and the lunar time `tau` that `ASTR` and the caller
give. For a main constituent `K`, `V` is the fractional part (to an even integer) of
`II*tau + JJ*s + KK*h + LL*p + MM*enp + NN*pp + SEMI`, and `f`, `u` are the modulus and the
argument (over `2π`) of `1 + Σ_j r_j exp(2πi u_j)` over its satellites, with the latitude factor
of Foreman for the diurnal and semidiurnal satellites and `|XLAT|` held at 5° or more. A
shallow-water constituent takes `f = Π f_L2^|c|`, `V = Σ c V_L2`, `u = Σ c u_L2` over the main
constituents it is built from. The tidal current is then `f*A*cos(V+u-g)` in the callers.

## What was ported

| | |
|---|---|
| WW3 routine | `SETVUF_FAST`, `model/src/w3tidemd.F90:531-667` at NOAA-EMC/WW3 `03adda91` (137 lines). Called by `W3UCUR` and `W3ULEV` in `w3updtmd.F90` (`:389`, `:2290`, both under `#ifdef W3_TIDE`) and by `ww3_prtide.F90` (`:661`, `:749`). No `#ifdef` in the routine; `w3tidemd.F90` is in every build. Regtest switches with `TIDE`: `ww3_tp2.18` (`switch_TIDE`, `switch_TIDE_MPI`); also `switch_Ifremer1`, `switch_Ifremer2`, `switch_Ifremer2_pdlib`, `switch_OASACM`, `switch_OASICM`, `switch_OASOCM`, `switch_ite_pdlib` (`git grep -lw TIDE -- '*switch*'`); no `switch_NCEP_*`/`switch_NWS_*` |
| Kokkos code | [`../src/ww_kokkos/tide_setvuf_fast.hpp`](../src/ww_kokkos/tide_setvuf_fast.hpp), `ww::tide::setvuf_fast`, `KOKKOS_INLINE_FUNCTION`; the W3TIDEMD state it reads (`NTIDAL_CON`, `NTOTAL_CON`, `TIDE_MF`, `NJ`, `II`…`NN`, `SEMI`, `EE`, `PH`, `LDEL`, `MDEL`, `NDEL`, `IR`, `TIDE_INDEX2`, `TIDE_INDEXJ`, `TIDE_INDEXJK`, `COEF_CON`) is passed in; `FA`, `UA`, `VA` (170) stay locals as in the Fortran (3 × 680 bytes per call) |
| C shim | none yet. The index tables come from `TIDE_SET_INDICES` ([`tide_tide_set_indices.hpp`](../src/ww_kokkos/tide_tide_set_indices.hpp)) |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3tidemd.F90` (WW3's CMake Release flags, gfortran
16.2) from `build_mixa` (`TIDE` on) and `build_lab` (`TIDE` off), each recompiled and found
identical to the CMake object; the assembly of `setvuf_fast` is the same in both, so the routine
has one variant. Compared with a C++ unit built with the `openmp-release` flags (no warning),
GIMPLE and assembly, 2026-10-16.

- (v) libm: both objects call glibc's scalar `sinf` (SLAT, `:559`), one `sincosf` for `COS` and
  `SIN` of `UU*TWOPI` (`:629-630`, `__builtin_cexpif` in both GIMPLEs), `atan2f` (`:635`) and
  `powf` (`:653`); `SQRT` is an inline `sqrtss` in both. No libmvec call, nothing vectorised in
  either object, no FMA in the C++.
- (v) `VDBL` (`:609`) and `UUDBL` (`:626`) are summed in `REAL(8)` left to right, each integer
  converted with `cvtsi2sd`, `SEMI` and `PH` with `cvtss2sd`, then rounded once to `REAL`
  (`cvtsd2ss`): the same sequence of operations in both GIMPLEs.
- (v) `INT()` of `VDBL` and `UUDBL` is `cvttss2si` in the Fortran; the C++ `fint` gives the same
  value in range and `0x80000000` outside it or for a NaN (for `IV` then `(IV/2)*2` is
  `0x80000000` in both).
- (v) Constants: `PI` and `TWOPI` are the `REAL(KIND=4)` values `0x1.921fb6p+1` and
  `0x1.921fb6p+2` (`static_assert`), `0.36309` and `2.59808` the same floats in both GIMPLEs.
  `SLAT = sinf(PI*CXLAT/180.)`: multiply then divide in both.
- (v) `RR = EE*0.36309*(1.-5.*SLAT*SLAT)/SLAT` (`:622`): `((EE*0.36309)*(1-(5*SLAT)*SLAT))/SLAT`
  in both; the parenthesis is a `PAREN_EXPR` in the Fortran and nothing in the C++ reassociates
  across it.
- (v) `MAX(ABS(XLAT),5.)`: `maxss 5.,|XLAT|` in the Fortran, `vmaxss` with the same operand
  order in the C++: 5. for a NaN in both.
- (v) Sums `SUMC`, `SUMS` (`:629-630`) and the products over `J` (`:653-655`) keep the
  Fortran's serial order; only commutative operands of one `+` or `*` differ in the GIMPLE.
- (v) With no satellite (`J1 > JL`) gfortran folds `FA = SQRT(1.)` to `1.` and
  `UA = ATAN2(0.,1.)/TWOPI` to `0.`; the C++ folds the first and calls `atan2f(0.,1.)`, which is
  `+0.`, divided by `TWOPI`: the same bits.
- (v) The shallow-water `DO J` bounds (`:651`) are evaluated once, before the loop, in both.
- ⚠ When `NTIDAL_CON+1 > NTOTAL_CON` the Fortran returns at `:645` before `F`, `U`, `V` are
  written (`:659-663`): the caller keeps whatever its arrays held. The C++ returns at the same
  point. `VUF_SET_PARAMETERS` sets `NTIDAL_CON = 45`, `NTOTAL_CON = 146` (`:1462-1463`), so the
  path is not taken there; `OPNVUF` counts both from its input file (`:1822`, `:1848`), not checked.
- ⚠ `TIDE_INDEX2(L)` keeps its previous value (initially 0) when a name of the analysis matches
  no constituent (`TIDE_SET_INDICES`, `:500-504`), and `TIDE_INDEXJK(J)` stays 0 when a
  shallow-water component is not found: then `FA(0)`, `UA(0)`, `VA(0)` are read outside the
  arrays (`:653-655`, `:660-662`). Neither the Fortran nor the C++ checks; the C++ reads
  `fa[-1]`, undefined behaviour as in the Fortran.
- ⚠ `F`, `U`, `V` are `F(NTIL=44)`; `TIDE_MF` up to `MC = 70` would write past them. Every caller
  declares them with 44 elements (`w3updtmd.F90:284`, `:2221`, `ww3_prtide.F90:175`).
- ⚠ `JBASE=JL` sits inside the `DO L` loop (`:637`), so with `TIDE_MF = 0` `JBASE` never advances;
  nothing is computed then, so no value depends on it. Reproduced as written.
- `dh, dpp, ds, dp, dnp` are dummies the Fortran never reads.
- CUDA: not compiled (optional this batch). On a device the libm functions are CUDA's, not shown
  to return glibc's bits ⚠.
- (v) Fresh review 2026-10-16: no defect. The module was recompiled from `build_mixa`'s flags
  (identical to its CMake object; `setvuf_fast` disassembles the same in `build_lab`) and the
  assembly read against the C++ function's, instruction by instruction: the argument each of
  `II`…`NN`, `LDEL`…`NDEL` multiplies, the `.rodata` bits of every constant (`PI`, `180.`,
  `5.`, `1.`, `0.36309`, `2.59808`, `TWOPI`, and the `fint` bounds and `INT_MIN` result), the
  `maxss` operand order, the `sincosf` cosine and sine slots, the `atan2f(SUMS,SUMC)` argument
  order, the early `RETURN` that leaves `F`, `U`, `V` unwritten, and the serial order of the
  shallow-water products and sums. The only differences are commutative operand orders
  (`COS*RR` against `RR*COS`, `VA(L2)*COEF` against `COEF*VA(L2)`, `VA(K)+term` against
  `term+VA(K)`), which change only the payload of a NaN when both operands are NaN. Nothing was
  changed.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
