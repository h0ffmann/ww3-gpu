# FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The preconditioned conjugate gradient that solves the wave setup Poisson system `ASPAR * zeta = B`
on a grid that is not unstructured (`GTYPE /= UNGTYPE`), switch `SETUP`, stopping when the
squared residual norm `sum(r**2)` is at most `1e-8`.

## What was ported

| | |
|---|---|
| WW3 routine | `FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR`, `model/src/w3wavset.F90:2742-2852` at NOAA-EMC/WW3 `03adda91`; called only by `FD_WAVE_SETUP_COMPUTATION` (`:3042`, on rank 1), which `WAVE_SETUP_COMPUTATION` calls when `DO_WAVE_SETUP` and the grid is not unstructured (`:3155-3160`), from `W3WAVE` under `W3_SETUP` (`w3wavemd.F90:1123-1124`). No regtest switch and no NOAA operational switch carries `SETUP` (`git grep -lw SETUP -- '*switch*'` finds only `model/src/cmake/switches.json`), so no regtest exercises it. Its only `#ifdef` blocks are `W3_S` tracing |
| Kokkos code | [`../src/ww_kokkos/wavset_fd_wave_setup_solve_poisson_neumann_dir.hpp`](../src/ww_kokkos/wavset_fd_wave_setup_solve_poisson_neumann_dir.hpp), `ww::wavset::fd_wave_setup_solve_poisson_neumann_dir`; calls the existing `fd_wave_setup_apply_precond`, `fd_wave_setup_apply_fct`, `fd_wave_setup_scalar_prod`; arrays 0-based, `REAL(rkind)` is `float` (`PDLIB/yowdatapool.F90:48`) |
| Structural change | the five automatic arrays `V_X, V_R, V_Z, V_P, V_Y(NX)` (`:2810`, `malloc`ed per call in the object) are caller-provided work arrays. The unused iteration counter `nbIter` (`:2819`, `:2826`; dead in the GIMPLE) is not kept |
| C shim | none yet; it passes `NX`, `NSEA` and yowNodepool's `PDLIB_IA`, `PDLIB_JA`, `PDLIB_I_DIAG` |
| Plan | issue #42, batch 3 |

## Arithmetic

Reference: the `-O3` object of `w3wavset.F90` in WW3's CMake Release build with
`regtests/ww3_tp2.6/input/switch_PDLIB` plus `SETUP` (`DIST MPI PDLIB SCOTCH`), gfortran 16.2;
the module recompiled with that build's `flags.make` gives an identical disassembly. Compared
with a C++ unit at the openmp-release flags (g++ 16.2, `-march=x86-64-v3 -ffp-contract=off`, no
warning), 2026-10-16.

- (v) `SOLVERTHR = 0.00000001` is a default-REAL literal: `9.99999993922529e-9`
  (`0x1.5798eep-27`) in the GIMPLE, the same `float` as `0.00000001f`. The exit test
  `eNorm <= SOLVERTHR` is an ordered `comiss` in both objects.
- (v) The divisions `alphaV = uO/h2` and `beta = uN/uO` are one `divss` each, in that operand
  order, in both objects.
- (v) The updates of `V_X`, `V_R` (`:2832-2833`) and `V_P` (`:2848`) are element-wise: each
  element is one rounded multiply and one rounded add or subtract, no FMA, so the vector width
  (4 in the Fortran object, 8 in the C++) does not change a bit. The add takes the product as its
  first operand in both objects, vector body and scalar tail. The multiply takes `V*alpha` in
  the Fortran vector body and `alpha*V` in its scalar tail and in the C++; a NaN `alphaV` or
  `beta` makes every later residual NaN, so that order changes no result that is ever returned.
- (v) The callees are separate calls in the Fortran object and inlined in the C++; the scalar
  products remain a serial `vaddss` chain in `IP` order from `+0`, as their own report
  (`FD_WAVE_SETUP_SCALAR_PROD_KOKKOS.md`) shows for the Fortran.
- (v) The callees loop over `NSEA` while this routine's arrays and scalar products run over
  `NX`; the C++ takes both, as the Fortran does through `W3GDATMD`.
- ⚠ The `DO` loop (`:2825`) has no iteration bound: a residual that stalls above `1e-8` or turns
  NaN (`h2 = 0`, for instance) never exits, in WW3 and in this translation.
- ⚠ CUDA not compiled.
- ⚠ The callees `FD_WAVE_SETUP_APPLY_PRECOND` and `FD_WAVE_SETUP_APPLY_FCT` declare and fill
  `NSEA` elements (`:2039`, `:1945`) of `V_Z`, `V_Y`, which this routine allocates with `NX`
  elements (`:2810`) and sums over `NX`: when `NSEA > NX` WW3 writes past its arrays, and when
  `NSEA < NX` the scalar products read elements no statement set, so the Fortran result depends
  on the heap. Only `NSEA == NX` has a defined result to match; the header asks for work arrays
  of `max(NX, NSEA)` reals.
- (v) Fresh review 2026-10-16: no arithmetic defect. The Fortran object recompiled from the
  `build_setup` flags (identical to the CMake object) read against a new C++ unit: `uO/h2` and
  `uN/uO` in that order, the exit test against `0x1.5798eep-27` with `<=`, element-wise updates
  with no FMA, scalar products as in-order sums (vectorised products folded left in `IP` order
  in the C++). Found and documented, not fixable in a translation: the `NSEA`/`NX` mismatch
  above; the header's work-array comment was corrected.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
