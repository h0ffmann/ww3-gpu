# W3SBT8_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Dissipation by a viscous fluid-mud layer after Dalrymple and Liu (1978), "thin model", for one
spectral point (switch `BT8`). For each frequency it iterates (secant steps, at most 20) on the
complex wave number `K` of the mud-covered bed until the residual `F` of the two-layer dispersion
relation vanishes, then sets `SMUDWD(IK) = 2*IMAG(KMUD)*CG_MUD`, `D(IS) = -SMUDWD(MAPWN(IS))` and
`S = D*AC`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SBT8`, `model/src/w3sbt8md.F90:111-492` at h0ffmann/WW3 `96892c95`, byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD --quiet -- model/src/w3sbt8md.F90` exits 0, v). The only `#ifdef` in it is `W3_S` |
| Kokkos code | [`../src/ww_kokkos/bt8_w3sbt8.hpp`](../src/ww_kokkos/bt8_w3sbt8.hpp), `ww::bt8::w3sbt8_point` for one point; it calls `ww::bt8::csinh` and `ccosh` ([`CSINH_KOKKOS.md`](CSINH_KOKKOS.md), [`CCOSH_KOKKOS.md`](CCOSH_KOKKOS.md)) and `ww::prop::wavnu1` ([`WAVNU1_KOKKOS.md`](WAVNU1_KOKKOS.md)) with the caller's `DispTable` |
| Host side, not translated | the three `INFLAGS1` tests, the loads of `MUDD`, `MUDT`, `MUDV` at `(IX,IY)` and their `WRITE(NDSE,*)` plus `EXTCDE(1..3)` (`:290-307`); the caller passes `rhom`, `thickm`, `kinvism` |
| Dropped | `KMIMAG(IK)` (`:473`) is stored and never read; the object has no store |
| Work memory | `smudwd` (`NK` reals), zeroed at `:322` when used |
| Caller | `W3SRCE`, `model/src/w3srcemd.F90:1686` (`BT8`); `gx_outp.F90:1258` |

## Arithmetic

Reference: the `-O3` object of `w3sbt8md.F90`, WW3's CMake Release flags, gfortran 16.2.0, from a
CMake build of `switches/switch_lab_shrd` with `BS0` -> `BS1` and `BT1` -> `BT8`; the module
recompiled by hand with the build's flags and defines, its disassembly (text and relocations)
equal to the CMake object's (v, 2026-11-12; only the file-name line of `objdump` differs).

The routine is a long run of `COMPLEX(4)` arithmetic, and the port writes each operation the way
gfortran emits it rather than through `Kokkos::complex` or `std::complex` (header of the file):

- (v) A product is `(ar*br - ai*bi, ar*bi + ai*br)` with all four products, so a `REAL` or `INTEGER`
  operand, converted to `(x, 0.)`, keeps its `ai*0.` and `ar*0.` terms (`K*H_WDEPTH` is
  `h*kre - kim*0.`, `kre*0. + h*kim` in the GIMPLE). A quotient is Smith's algorithm
  (`-fcx-fortran-rules`: branch on `|br| < |bi|`, `r = bi/br`, no `__divsc3`, no NaN recovery),
  also when the denominator is a converted `REAL` (`/SIGMA`, `/KINVISW`: `r = 0/sigma`).
- (v) `SQRT` of a `COMPLEX` is glibc's `csqrtf` (2 calls), `EXP(-LAM1*H)` is `cexpf` (1 call) and
  only its real part is kept (`EXPH` is `REAL`); `SIN(Y)`/`COS(Y)` of `CSINH`/`CCOSH` are fused into
  `sincosf` by both compilers; the other libm calls are `sinhf`/`coshf` of the real part. No libmvec,
  no FMA. On the host `std::sqrt` and `std::exp` of `std::complex<float>` lower to those two glibc
  calls.
- (v) The precedence of the source is kept where it moves a sign: `-LAM2*SHD/K+SHLAM2` is
  `-(LAM2*SHD/K) + SHLAM2`; `BET0 = -EXPH/C0` is the real part of `-(EXPH/C0)`;
  `CD = -(...)/C(3,1)` is `-((...)/C(3,1))`; `-RHOM*KINVISM*...` negates the whole term; `(-1)` and `2`
  are the complex constants `(-1,0)` and `(2,0)`; `RHOM*KINVISM`, `2*RHOM*KINVISM`, `2*RHOW*KINVISW`,
  `RHOW*KINVISW`, `(RHOM-RHOW)*GRAV` are `REAL` products until they meet a `COMPLEX`.
  `K = K*(.995) + .001*I` adds the constant `(0, .001)`, so the real part gets `+ 0.`.
- (v) `C(4,1)`, `B(3)`, `B(4)` are never read and the object does not compute them.
- (v) The C++ keeps `ICOUNT` exit as the Fortran: `EXIT` after setting `KMUD = K` (then `KMUD = K`
  again after the loop), the 20th iteration still takes its step, `F.EQ.FM1`, `K.EQ.KM1` are
  componentwise equal tests.
- ⚠ The operation counts of the two GIMPLE dumps are not equal (Fortran 664 mul, 231 div, 349 add,
  255 sub; C++ 635, 230, 332, 258): g++ peels the first iteration of the `ICOUNT` loop and inlines
  `wavnu1`, gfortran calls it. The two were therefore not compared operation by operation; the
  check is the next item.
- ⚠ A throwaway differential program (not kept, ADR-0010) linked the Fortran object (the WW3 lib of
  the build, with `W3DISPMD`'s `DISTAB` table filled by `DISTAB`) and `w3sbt8_point` and compared S
  and D bit for bit: 300 000 random calls (NK 2-15, NTH 4-11, sigma 0.1-30 rad/s from `fmin` 0.02-0.12 Hz, depth 0.3-1020 m,
  `THICKM` 5e-4 to 0.5 m, `KINVISM` 1e-6 to 0.1, `RHOM` 1020-1620 and 3 % at 900, 5 % with `THICKM = 0`),
  38 249 970 elements compared, 0 differ, 0 NaN outputs, 10 587 064 nonzero D values; the same at
  `-O3 -march=x86-64-v3` (5 000 calls) and at `-O0` under ASan/UBSan (5 000 calls). No command in the
  repository reproduces these counts. ⚠ NaN and infinite inputs were not tried, nor `ICOUNT` runs
  that reach 20 without converging in a measured fraction.
- (v) Compiles with no warning of the file under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`
  at `-O2`, `-O3 -march=x86-64-v3` and `-O0 -g -fsanitize=address,undefined`, instantiated in a
  `Kokkos::parallel_for` on `Serial` and `OpenMP`; no `vfmadd`.
- ⚠ On a device `csqrtf`, `cexpf`, `sinhf`, `coshf`, `sinf`, `cosf` are Kokkos'/CUDA's, not shown to
  return glibc's bits. CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk
in the fork.

## WW3 defects found

None. (`KMIMAG` is written and never read, and `C(4,1)`, `B(3)`, `B(4)` are computed and never
read; harmless.)
