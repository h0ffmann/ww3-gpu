# SKEWNESS_KOKKOS

**Status:** ported, phase 1 (translated). Not wired into WW3, not regtested: translation only,
no test run (the owner, 2026-10-16).

The skewness parameters of a nearly Gaussian sea surface at each sea point, after Srokosz (1986,
J. Geophys. Res. 91, 995-1006) with Zakharov's second-order coefficients, and from them the
electromagnetic bias `EMBIA1` and tracker bias `EMBIA2` of an altimeter. `W3OUTG` calls `SKEWNESS`
only when output field 8-7, 8-8 or 8-9 (`SKW`, `EMB`, `EMC`) is requested (`w3iogomd.F90:2355-2357`);
`ww3_ts1` requests all three (`regtests/ww3_ts1/input/ww3_shel.nml:24`).

## What was ported

| | |
|---|---|
| WW3 routine | `SKEWNESS`, `model/src/w3iogomd.F90:4883-5054` at NOAA-EMC/WW3 `03adda91`. `w3iogomd` is compiled by every switch. Its only `#ifdef` is `W3_OMPG` around the `JSEA` loop (`:4954-4956`, `:5047-5049`), which does not change the arithmetic of a point |
| Callers | `W3OUTG` only (`:2356`) |
| Kokkos code | [`../src/ww_kokkos/iogo_skewness.hpp`](../src/ww_kokkos/iogo_skewness.hpp): `ww::iogo::skewness_point` (the body of the `JSEA` loop, `:4957-5050`) and the host loop `ww::iogo::skewness` |
| Not translated | `CALL SECONDHH(NKHF,FAC0,FAC1,FAC2,FAC3)` (`:4944`): `FAC0..FAC3` are inputs, `SECONDHH`'s output for `NKHF = NK+13`, which the caller builds (it depends on the grid only, yet the Fortran rebuilds it on every call). `SECONDHH` itself is not translated here; its two functions are (`iogo_vmin_d.hpp`, `iogo_vplus_d.hpp`) |
| Structural changes | The `ALLOCATE`s of `F2`, `SIGHF`, `DFIMHF`, `FAK` (`:4946-4947`) are caller-provided work arrays, one set per concurrent point; `XMU` and `XLAMBDA` are locals of the point |
| C shim | none yet |

## Arithmetic

Reference: the `-O3` object of `w3iogomd.F90` recompiled with the flags of WW3's CMake Release build
under `switch_lab_shrd` (gfortran 16.2.0, pinned shell), disassembly identical to the CMake object
of `ref2/build_lab`. Compared with a throwaway C++ unit compiled with the `openmp-release` flags
(warning-free), 2026-10-16.

- (v) Reductions. gfortran vectorises the `K2` loop of the four `FAC*` sums (`:4996`) and the `M1`
  loop of the four direct moments (`:5008`), each as a fold-left reduction: the vector products are
  extracted lane by lane and added to the accumulator in index order, so every `XMU` is summed in
  the Fortran's loop order. g++ emits the same in-order fold-left reductions (8 lanes under
  `x86-64-v3`). No tree reduction and no FMA in either object.
- ⚠ When both operands of one of those additions are NaN, the result carries the payload of the
  first source operand. The Fortran object alternates which register is first (`addss %xmm0,%xmm11`
  then `addss %xmm11,%xmm2` in the same lane sequence); the C++ object keeps the accumulator first.
  Bits differ only for two NaNs of different payloads.
- (v) Products in the Fortran order: `DELF = ((DFIMHF(M1)*DFIMHF(M2))*F2(K1,M1))*F2(K2,M2)`,
  `(3.0*FAC0)*DELF`, `(XK1*ECOS**2)*DELF`, `((XK1*ECOS)*ESIN)*DELF`, `CONX = (TPIINV/SIG)*CG`,
  `CO1 = (((XFR-1)*0.5)*DTH)*TPIINV`, `FAK = (SIGHF*SIGHF)/GRAV` (a division, not a reciprocal).
- (v) `(SIGHF(NK)/SIGHF(M))**5` as gfortran's power tree: `x2 = x*x`, `x3 = x*x2`, `x5 = x2*x3`.
- (v) `XMU**XPI` with `XPI = 0.5*FLOAT(I)`: gfortran unrolls the `I,J,K` loops (`:5022-5035`),
  folds `x**0.` to 1 and `x**1.` to `x`, and calls `powf` only for exponents 0.5 and 1.5 (four calls
  in the object). g++ does not unroll those loops, so the folding is written out in
  `skewness_detail::pow_half`; the C++ calls `powf` with the same arguments (more times, since it
  keeps every `XLAMBDA`), and the products `XN` keep the `(a*b)*c` order. Unused `XLAMBDA` entries do
  not reach an output.
- (v) `DELTA = ((L120+L102) - (2*L011)*L111) / (1 - L011*L011)`, `EMBIA1 = -(0.125*DELTA)`,
  `EMBIA2 = -((0.125*L300)/3.0)`, the threshold `XMU(2,0,0) > 1.E-7` with the float
  `0x1.ad7f2ap-24`: the same operations in both dumps. `TPIINV` is `0x3e22f983` and `GRAV`
  `9.805999755859375` in both.
- (v) libm: `powf` only, no libmvec variant in either object.
- ⚠ Defect of WW3: under `W3_OMPG` the `!$OMP PARALLEL DO PRIVATE(JSEA)` (`:4955`) leaves `XMU`,
  `F2`, `SIGHF`, `DFIMHF`, `FAK`, `CONX`, `DELF`, `XN`, `XLAMBDA`, `DELTA` and the scalars shared:
  a data race between threads. The translation gives each point its own work arrays, which is the
  serial result. `switch_lab_shrd` has no `OMPG`.
- ⚠ Possible defect of WW3: `CG(M,JSEA)` (`:4961`) is indexed by the local sea index, where the
  rest of `W3IOGOMD` uses `CG(IK,ISEA)` (`:1530`, `:1559`). The two agree when `JSEA = ISEA`
  (shared memory); under `W3_MPI` the caller must pass the column the Fortran reads, column `JSEA`,
  to reproduce it.
- ⚠ CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not yet, and nothing was run.
