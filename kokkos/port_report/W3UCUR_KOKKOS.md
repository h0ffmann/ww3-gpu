# W3UCUR_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Surface current at the model time: linear interpolation of the speed and the direction between
the two input current fields (`CX0`/`CY0` at `TC0`, `CXN`/`CYN` at `TCN`), stored as the
components `CX`, `CY` of each sea point.

## What was ported

| | |
|---|---|
| WW3 routine | `W3UCUR`, `model/src/w3updtmd.F90:171-472` at h0ffmann/WW3 `96892c95` (302 lines), byte-identical to NOAA-EMC/WW3 `03adda91` (`git -C WW3 diff 03adda91 HEAD -- model/src/w3updtmd.F90` is empty), as `switches/switch_lab_shrd` compiles it (`CRT1`) |
| Kokkos code | [`../src/ww_kokkos/updt_ucur.hpp`](../src/ww_kokkos/updt_ucur.hpp): device functions `w3ucur_first`, `w3ucur_rd`, `w3ucur_point` and the launcher `ww::updt::w3ucur<ExecSpace>`; it reuses W3UWND's helpers (`updt_uwnd.hpp`) |
| C shim | none yet |
| Caller | `W3WAVE`, `model/src/w3wavemd.F90:1208` at the pin |

Not compiled under the lab switch and not ported: `CRT0`, `CRT2`, `OASOCM`, `SMC`, `TIDE`,
`TIDET`, `S`, `T`. `DSEC21` stays on the host: the caller passes `DT0N = DSEC21(TC0,TCN)` and
`DT0T = DSEC21(TC0,TIME)` as the `REAL`s the Fortran stores (`w3updtmd.F90:337-338`).

## Arithmetic

The reference is the `-O3` object of `w3updtmd.F90` (WW3's CMake Release flags, gfortran 16.2.0,
recompiled with `-fdump-tree-optimized-lineno -fopt-info-vec-optimized`; its disassembly equals
the `switch_lab_shrd` CMake build's object, `cmp` of `objdump -d`), read as GIMPLE and assembly,
2026-11-12. The C++ is a throwaway translation unit with the `openmp-release` flags (`-O3
-march=x86-64-v3 -std=c++20 -fopenmp -ffp-contract=off`), Kokkos 5.2.2.

- (v) `:311-312` `SQRT(x**2+y**2)` is `(x*x)+(y*y)` then `sqrtss`; `:314`, `:319` `ATAN2` is a
  libm `atan2f` call and `MOD(a,TPI)` an inline x87 `fprem` loop; `:329` `SIGN(1.,DD)` is
  `copysignf`; `:331` is `CAI-CA0`. The C++ object has the same operations, `atan2f` calls and
  `fprem` loops (plus `fmodf` calls on a NaN `fprem` result only, for `errno`, whose return
  value is discarded; `W3UWND_KOKKOS.md`). No `sqrtf` call: both objects use `sqrtss` inline.
- (v) `:344` `MAX(1.E-7,DT0N)` is `maxss .LC10,%xmm0` with `DT0N` in `%xmm0`: a NaN or a tie
  gives `1.E-7`. The C++ emits `vmaxss .LC,%xmm1,%xmm1` with `DT0N` in `%xmm1`, the same
  selection; then one `divss` in both.
- (v) `:436`, `:442` one `mulss` and one `addss` each; `:452-453` one `sincosf` call, then two
  `mulss`; the same in the C++.
- (v) Operand positions, which decide the payload when both operands of a product are NaN: in
  the C++ kernel the `:436` product has `RD` first (the Fortran `CAI`) and the `:452-453`
  products `CABS` first (the Fortran `COS`/`SIN`); g++ puts the memory operand second and
  swapping the C++ operands does not change the object. The `:311` sum (`CX0**2` first), the
  `:442` product (`RD` first) and every `addss` have the Fortran's positions. Both operands are
  never NaN together: `DSEC21` is `REAL(NS)+86400.*REAL(ND)` of integers
  (`w3timemd.F90:330-568`), so `RD` is finite; `CD0` and `CDI` are finite, since a NaN `ATAN2`
  needs a NaN component, whose `CA0` or `CAI` then fails the `1.E-7` test; so `CDIR`, `COS` and
  `SIN` are finite (read from the source and both objects, 2026-11-12).
- (v) No loop of `:171-472` is vectorised (`vec.txt`), no FMA in either object
  (`grep -c vfmadd` is 0 on the C++ object).
- ⚠ On CUDA, `atan2f`, `sincosf` are `libm_device.hpp`'s copies and `fmod` is `Kokkos::fmod`;
  not compiled for CUDA here, never run on a GPU.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested, postponed to the validation phase (ADR-0016). Not wired: no shim, no caller hunk.
The header compiled with no warning under the `openmp-release` flags and at `-O0 -g
-fsanitize=address,undefined` with `-Wall -Wextra -Wpedantic` (v, 2026-11-12); nothing was run.

## WW3 defects found

None.
