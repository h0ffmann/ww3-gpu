# W3SBT9_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Dissipation by a viscous fluid-mud layer after Ng (2000), for one spectral point (switch `BT9`):
`D(IS) = -SMUDWD(MAPWN(IS))` and `S = D*AC`, where `SMUDWD(IK) = 2*DMW(IK)*CG_MUD` and `DMW`, `K_MUD`
come from `NG` for each frequency whose rocky wave number has `k*h < KDCUTOFF` and a mud layer
thicker than `1.0E-5`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SBT9`, `model/src/w3sbt9md.F90:118-406` at h0ffmann/WW3 `96892c95`, byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD --quiet -- model/src/w3sbt9md.F90` exits 0, v). The only `#ifdef` in it is `W3_S` |
| Kokkos code | [`../src/ww_kokkos/bt9_w3sbt9.hpp`](../src/ww_kokkos/bt9_w3sbt9.hpp), `ww::bt9::w3sbt9_point` for one point; it calls the earlier `ww::bt9::ng` ([`NG_KOKKOS.md`](NG_KOKKOS.md)) |
| Host side, not translated | the three `INFLAGS1` tests, the loads of `MUDD`, `MUDT`, `MUDV` at `(IX,IY)` and their `WRITE(NDSE,*)` plus `EXTCDE(1..3)` (`:264-281`); the caller passes the three values. The NaN diagnostics (`:386-390`) and `EXTCDE(1)` (`:391`): `w3sbt9_point` returns 1 and fills `Bt9Nan` with the values the `WRITE`s print, the caller prints and stops |
| Dead in the object | the two `CALC_ND` calls (`:307`, `:355`) and everything only they feed (`ND_ROCK`, `ND_MUD`, `CG_ROCK`, `NWAVE_ROCK`, `SBLTW`, the `SND2 = 0`, `CWAVE = 0` resets): `CALC_ND` is pure and its results are never read, so `-O3` removes the calls (v, GIMPLE has no `calc_nd` call). `bt9_calc_nd.hpp` is therefore not used here |
| Work memory | `smudwd` (`NK` reals), the Fortran's `malloc`ed `SMUDWD(NK)`; `DMW(NK)` is a scalar here, since only `DMW(IS)` is read |
| Caller | `W3SRCE`, `model/src/w3srcemd.F90:1689` (`BT9`) |

## Arithmetic

Reference: the `-O3` object of `w3sbt9md.F90`, WW3's CMake Release flags (`-O3 -g -fno-second-underscore
-ffree-line-length-none -fallow-argument-mismatch`), gfortran 16.2.0, recompiled by hand with the
flags and defines of a CMake build of `switches/switch_lab_shrd` with `BS0` -> `BS1` and `BT1` ->
`BT8` and its `.mod` files, read as `-fdump-tree-optimized` GIMPLE, 2026-11-12.

- (v) `ROOTDG = SQRT(H/GRAV)`, `SND = SIG*ROOTDG`; the three `K_ROCK` branches: `SIG*SIG/GRAV`
  (`SND >= 2.5`), `SND/H` (`SND < 1e-6`), and the shallow-water polynomial with `SND2**2`, `**3`,
  `**4` as `s4 = s2*s2`, `s6 = s2*s4`, `s8 = s4*s4`, summed left to right with `1./(...)` as a
  division and `PAREN_EXPR` around the sum, then `SQRT(GRAV*H/(SND2 + 1/poly))` and `SIG/CWAVE`.
  The constants are the `REAL` values of `0.666`, `0.445`, `0.105`, `0.272`, `1.E-6`, `1.0E-5`,
  `NU_WATER = 1.31E-6`, `DWAT = 1000`.
- (v) `NG` is a call in the Fortran object (not inlined); g++ inlines `ng` into `w3sbt9_point`.
  That changes no value: `ng` has the same operations in both ([`NG_KOKKOS.md`](NG_KOKKOS.md)).
- (v) `NWAVE_MUD = 0.5*(1 + (H*(2K))/ZTMP)`, `ZTMP` the sinhf of that product when it is below 70,
  else `1.0E+30`; `SMUDWD(IS) = CG_MUD*(DMW*2)` (the object multiplies `CG_MUD` by `2*DMW`, the
  other operand order of the source's commutative product: a NaN payload at most); `D = -SMUDWD`;
  `S = AC*D`. The `INAN` test is the two ordered compares against `+-HUGE`.
- (v) libm: one `sinhf` in `W3SBT9` itself (the others are inside `NG`), glibc scalar. g++ and
  gfortran both fold nothing across it.
- ⚠ A throwaway differential program (not kept, ADR-0010) linked the recompiled Fortran object
  and `w3sbt9_point` and compared S and D bit for bit: 20 000 random calls (NK 2-15, NTH 4-11,
  depth 0.3-1020 m, mud depth 1e-4 to 0.03 m, `KINVISM` 1e-4 to 0.1, `RHOM` 1020-1620 and 3 % at 900, 5 % with no mud),
  1 735 042 elements compared, 0 differ (`-O3`, `-O3 -march=x86-64-v3`, and `-O0` under
  ASan/UBSan). No command in the repository reproduces these counts. The generator avoided the
  region where `NG` overflows to NaN (the model then stops, `:391`).
- ⚠ `SMUDWD(IS)` is stored only where `KD < KDCUTOFF` (`:377-379`) and read for every
  `MAPWN(IS)` (`:399`) from `malloc`ed, never initialised memory (see below). The differential
  program left out the 396 401 elements whose value the Fortran does not define; the C++ takes
  what is in the caller's `smudwd` array.
- ⚠ The CMake object under a `BT9` switch was not built (the lab switch's `ww3_lib` has no
  `w3sbt9md.F90`; the reference of the plan, `ww3_tbt2.1/input/switch_BT9_MPI` with `SHRD`, would). The
  module's code has no switch-dependent line, so the object compiled here with the same flags is
  the one that build would give, but this was not checked by building it.
- ⚠ On a device `sinh`, `cosh`, `sin`, `cos` (in `NG`) and `sinh` here are CUDA's, not shown to
  return glibc's bits. CUDA not compiled.
- (v) Compiles with no warning of the file under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`
  at `-O2`, `-O3 -march=x86-64-v3` and `-O0 -g -fsanitize=address,undefined`, instantiated in a
  `Kokkos::parallel_for` on `Serial` and `OpenMP`; no `vfmadd` in the optimised object.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk
in the fork.

## WW3 defects found

- ⚠ `W3SBT9` reads `SMUDWD(MAPWN(IS))` (`w3sbt9md.F90:399`) for frequencies where `KD >= KDCUTOFF`
  and `SMUDWD` was never assigned (the array is not zeroed, unlike `W3SBT8`'s `:322`): undefined
  values go into `D` and `S` for deep or high-frequency bins. Read from the source and seen in
  the object (a `malloc`, no store before the read); not reproduced numerically. Not fixed.
