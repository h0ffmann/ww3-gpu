# PREPKGRID_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The grid setup of the generalized kinetic equation module of the `NL5` nonlinear source term
(switch `NL5`, `W3GKEMD`, Gramstad and Stiassnie 2013). For the fixed radian frequencies
`σ_i` and directions `θ_j` and one water depth `d` it computes, per frequency, the wavenumber
`k_i` and group velocity `c_{g,i}` of the linear dispersion relation `σ² = g k tanh(kd)` by
`WAVNU1` (table interpolation, `w3dispmd.F90`), and per bin `(i, j)`

`k_x = k_i cos θ_j`, `k_y = k_i sin θ_j`, `Δk⃗ = k_i Δσ_i / c_{g,i} Δθ`, `ω = σ_i`,

with `Δθ = 2π / N_θ`, `Δσ_i` the centred half-difference (one-sided at the first and last bin),
and `sin θ`, `cos θ` snapped to `0`, `±1` when the other is within `10⁻⁵` of 0.

## What was ported

| | |
|---|---|
| WW3 routine | `PrepKGrid`, `model/src/w3gkemd.F90:1066-1197` at NOAA-EMC/WW3 `03adda91` (132 lines), private to `W3GKEMD`. Only caller: `PrepKernelIO` (`:1272`, with `dpt = qr_depth`, an out-of-line call in the object), called by `INSNL5` (`w3snl5md.F90:522`). Compiled by the switches of `ww3_ts1/input_nl5_gaussian`, `input_nl5_growth`, `input_nl5_matrix` (`git grep -lw NL5 -- '*switch*'`), by no NOAA operational switch |
| Kokkos code | [`../src/ww_kokkos/gke_prepkgrid.hpp`](../src/ww_kokkos/gke_prepkgrid.hpp), `ww::gke::prepkgrid(nk, nth, dpt, sig, th, qr_kx, qr_ky, qr_dk, qr_om, qr_wn1, disp, esin, ecos)`; calls `ww::prop::wavnu1` ([`../src/ww_kokkos/prop_wavnu1.hpp`](../src/ww_kokkos/prop_wavnu1.hpp)). The module arrays `qr_kx`, `qr_ky`, `qr_dk`, `qr_om` (`ns = nk*nth`) and `qr_wn1` (`nk`), allocated by `PrepKernelIO` (`:1251-1252`, `:1269-1271`), are output pointers; the automatic arrays `esin`, `ecos` (`:1108`, `malloc` in the object) are caller-provided work arrays of `nth` floats; `disp` is the `DISTAB` table `WAVNU1` reads |
| Variants | `dispopt` is a `PARAMETER` fixed to 1 (`:1103`): the Pade branch (`:1147-1161`) is dead code removed by gfortran and not translated. `W3_TS` (`:1167-1169`, `:1188-1193`) is output only, not translated |
| C shim | none yet |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object and GIMPLE of `w3gkemd.F90` (WW3's CMake Release flags, gfortran
16.2, `switch_lab_shrd` with `NL5`, the `build_nl5` reference build; recompiled object identical
to the CMake one), out-of-line `__w3gkemd_MOD_prepkgrid`, against a throwaway C++ unit with the
openmp-release flags, 2026-10-16.

- (v) The two optimised GIMPLEs agree statement by statement: five whole-array zero fills
  (`memset` in both), `dth = 0x40C90FDB / (float) nth` (`qr_tpi`, folded to
  `6.283185482025146484375`; the header's `kQrTpi` with a `static_assert` of how `:200` builds
  it), one `sincosf` per direction, the two `|x| < 1e-5` tests and `> 0.5` selections in the same
  order, `dsii = (σ_a - σ_b) * 0.5`, `qr_dk = ((dsii * k) / cg) * dth` hoisted out of the `ith`
  loop in both, `qr_kx = ecos * k`, `qr_ky = esin * k`. gfortran writes `k*dsii` and
  `k*ecos` with commuted operands in its GIMPLE, as the C++ does.
- ⚠ In the assembly the `qr_kx` and `qr_ky` products (`:1181-1182`) have a different first
  operand: the Fortran object (SSE, two-operand `mulss`) loads `ecos`/`esin` and multiplies by
  `k`, the C++ (`vmulss` with the memory operand second) has `k` first. The products are equal
  for every input except when `k` and `cos θ` (or `sin θ`) are both NaN with different payloads,
  where the first operand's payload is returned. The `qr_dk` chain has the same first operands
  in both (`dsii`, then the quotient, then the product).
- (v) Library calls: gfortran fuses `SIN` and `COS` of `:1124-1125` into `sincosf`; the host C++
  calls glibc's `sincosf` explicitly. `WAVNU1` is an external call in the Fortran object
  (`__w3dispmd_MOD_wavnu1`) and inlined in the C++; its arithmetic is that of
  `prop_wavnu1.hpp`, checked against `w3dispmd.F90`'s object by its own translation. The C++
  calls `sqrtf` on the errno path of a negative `dpt` (inside `wavnu1`); it returns the NaN
  `sqrtss` returns.
- (v) No FMA, no packed arithmetic, no libmvec in either object; no reduction, no MIN/MAX.
  Compiled warning-free.
- ⚠ A NaN `sin θ` or `cos θ` fails the `< 1e-5` test in both and is kept; a NaN in the
  `> 0.5` test selects `-1` in both.
- ⚠ `nk == 1` reads `sig(2)` past the end of `sig(nk)` at `:1172` (the first-bin test comes
  before the last-bin test); reproduced.
- ⚠ CUDA compiled only (nvcc_wrapper, `sm_89`, `--fmad=false`, 2026-10-16), never run. On the
  device `sin`/`cos` are CUDA's, whose bits are not shown to equal glibc's `sincosf`; the SASS
  holds `FFMA` from CUDA's `sinf`/`cosf` and division sequences.
- (v) Fresh review 2026-10-16: no defect in the header. `w3gkemd.F90` recompiled under the
  `build_nl5` flags (object identical to the CMake one) and a new C++ unit (warning-free) read
  side by side in GIMPLE and assembly: zero fills, `sincosf`, the snap tests and selects, the
  `1e-5` and `qr_tpi` constants, `dsii`, the `qr_dk` chain, `WAVNU1`'s argument order. Found and
  added above: the both-NaN first-operand difference of `qr_kx`/`qr_ky`, which the source
  cannot steer under `vmulss`. `WAVNU1`'s own body was not re-read here (its report).

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run. The routine runs once per `PrepKernelIO` call (model setup), so
its checkpoint (ADR-0011) is an `NL5` regtest entry (`ww3_ts1` with `input_nl5_*`) once a shim
and the caller's hunk exist.
