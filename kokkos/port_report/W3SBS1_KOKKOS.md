# W3SBS1_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Bottom scattering source term (switch `BS1`) after Ardhuin and Herbers (2000) without a current
and Magne and Ardhuin (2007) with one, for one spectral point. Outputs `S`, `D`, `TAUSCX`, `TAUSCY`.
Without a current the scattering matrices `SCATMATV`, `SCATMATD` (filled once by `INSBS1(2)` from
the bottom spectrum `BOTSPEC`) give `S(ISPEC)` per wave number; with a current the routine builds the
Doppler-shifted wave numbers `WN2`, the bottom wave numbers `KA`, `KB`, the spectrum `B` and an
integral over the bottom spectrum.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SBS1`, `model/src/w3sbs1md.F90:112-462` at h0ffmann/WW3 `96892c95`; the `#ifdef` regions are `W3_S` and `W3_T` |
| Kokkos code | [`../src/ww_kokkos/bs1_w3sbs1.hpp`](../src/ww_kokkos/bs1_w3sbs1.hpp), namespace `ww::bs1`: `w3sbs1_point` (both branches), `w3sbs1_krange` (`KWMIN`, `KWMAX`, `:252-253`), `matmul_tn` (the library call of `:289-291`), and the structs `Sbs1Module`, `Sbs1Grid`, `Sbs1Work`, `Sbs1Diag` |
| Host side, not translated | `INSBS1(1)` (reads `bottomspectrum.inp`, allocates `BOTSPEC`), `INSBS1(2)` (`DIAGONALIZE`, fills `SCATMAT*`), the `FIRST` flag and `MATRICES`, the "k range" `WRITE` (`:254`), `CURTX`, `CURTY`; the "Energy not conserved" `WRITE` (`:298`) is reported through `Sbs1Diag` |
| Dead in the object | `SB`, `WNBOT`, `SMATRIX`, `SMATRIX2`, the accumulations `INTEGRAL1`, `INTEGRAL1B`, `INTEGRAL2`, `INTEGRAL3`, `CGPK` and everything under `W3_T` (`:437-462`) |
| Work memory | `Sbs1Work`: `dwork` (4*NTH doubles), `wn2`, `kb`, `b` (NSPEC*NTH reals), `ka` (NSPEC reals) |
| Caller | `W3SRCE`, `model/src/w3srcemd.F90:1693` (`BS1`) |

## Arithmetic

Reference: the `-O3` object of `w3sbs1md.F90`, WW3's CMake Release flags, gfortran 16.2.0, from a
CMake build of `switches/switch_lab_shrd` with `BS0` -> `BS1`; the module recompiled by hand with the
build's flags, its disassembly equal to the CMake object's (v, 2026-11-12).

- (v) Real `MIN`/`MAX` are `minss`/`maxss` with the NaN or tie result going to the source operand;
  the operand order is read per site from the object. `KWMIN`, `KWMAX` are checked bit for bit
  against the Fortran in the differential program below.
- (v) `SINH`, `TANH` are glibc scalar `sinhf`, `tanhf`, `SQRT` is `sqrtss`, `MOD(x,1.)` is `fmodf`;
  no libmvec, no FMA (`-ffp-contract=off`).
- The no-current branch calls libgfortran's `MATMUL` on `TRANSPOSE(SCATMATV(IBK,:,:))`
  (`:289-291`); the object has a call to `_gfortran_matmul_r8` with the first argument's strides of
  the 3rd and 2nd dimension swapped, `try_blas` 0 and no `gemm` (the `-fdump-tree-optimized` of the
  module, `_gfortran_matmul_r8 (&D.5325, &parm.87, &parm.88, 0, 0, 0B)`). The sum order of that
  routine is neither the source's nor fixed: it depends on NTH, on the libgfortran build and on the
  CPU (AVX2 and AVX-512 variants are dispatched at run time). (v, 2026-11-12) The earlier hand model
  (two interleaved accumulators, unfused) matched the library of this build only for even NTH from 4
  and failed NTH = 2 (the library fuses the multiply-add there) and every odd NTH (5200 cases over
  NTH 1..130, 2408 outputs vectors differ, all NTH 2 or odd); a reimplementation cannot give parity
  for every NTH or CPU, so it was removed. `matmul_tn` now calls `_gfortran_matmul_r8` itself with
  the object's descriptors, the Fortran's own call, so the bits are the executable's for every NTH,
  libgfortran build and CPU. (v) 5200 random cases (NTH 1..130, two magnitudes) compared with
  `MATMUL(TRANSPOSE(..))` of gfortran 16.2.0 on an AVX-512 CPU: 0 differences (throwaway harness, no
  repo command ⚠). Costs: the header depends on libgfortran's `gfc_array_r8` layout (GCC 8 or
  later) ⚠ not checked on another GCC, the symbol is declared weak and aborts if absent, and the
  branch is host only (a device build aborts there). The outer `MATMUL(SCATMATV(IB,:,:), ..)` is
  inline in the object (`j` outer, `i` inner, no library call, no size test) and is translated as
  such.
- ⚠ A throwaway differential program (not kept, ADR-0010; the repository has no command that
  reproduces the counts) linked the Fortran object with real `W3GDATMD`, `W3SBS1MD` state and compared
  `S`, `D`, `TAUSCX`, `TAUSCY` bit for bit with `w3sbs1_point`: 30 000 random calls at NK 10, NTH 12
  (15 065 without a current, 14 935 with one), 7 200 000 elements, 0 differ, `KWMIN`/`KWMAX` bits
  equal; also 8 000 calls each at NK/NTH = 8/24, 9/15 (odd NTH) and 6/36, 0 differ; also at
  `-O3 -march=x86-64-v3` and at `-O0` under ASan/UBSan, clean. Those runs used the earlier hand
  model of the product, not the library call; they exercise the rest of the routine, the library
  call was compared on its own as above.
- ⚠ The "Energy not conserved" diagnostic never triggered in these draws, so the `Sbs1Diag` path is
  unexercised.
- (v) Compiles with no warning of the file under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`,
  instantiated in a `Kokkos::parallel_for` on `Serial` and `OpenMP`. The no-current branch is
  host-only in effect (libgfortran).
- ⚠ On a device `sinhf`, `tanhf`, `sqrtf`, `fmodf` are CUDA's, not shown to return glibc's bits.
  CUDA not compiled.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk.

## WW3 defects found

⚠ `W3SBS1MD` keeps the bottom spectrum, the scattering matrices and `MATRICES` as module singletons,
filled by the first call (read from the source, not run): a `ww3_multi` run with several grids that
differ in `NTH` or `NK` would reuse the first grid's matrices. Not measured here.
