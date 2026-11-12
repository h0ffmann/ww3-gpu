# W3SIS2_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Scattering of waves by ice floes and flexural break-up of the floes (IS2, Williams et al. 2013;
⚠ reference not fetched while writing), for one spectral point: the attenuation coefficient comes from the
table SIS2ALPHAS (Dumont et al. 2011, ⚠ not fetched) at the floe size, the creep or
anelastic dissipation is added, and the floe size is updated from the break-up spectrum. The routine
fills `S` and `D` for the whole spectrum and returns the new `ICEF` and `ICEDMAX`.

## What was ported

| | |
|---|---|
| WW3 routine | `W3SIS2`, `model/src/w3sis2md.F90:652-1086` at h0ffmann/WW3 `96892c95`, byte-identical to NOAA-EMC/WW3 `03adda91` in this file (v) |
| Reference switch | `mww3_test_03/input/switch_PR3_UQ_MPI_IS2` with `SHRD` in place of `MPI` (IS2 on, no MPI) |
| Kokkos code | [`../src/ww_kokkos/is2_w3sis2.hpp`](../src/ww_kokkos/is2_w3sis2.hpp), `ww::is2::w3sis2_point`, a device function for one point; grid in `Sis2Grid`, scratch in `Sis2Work` (the Fortran's automatic and allocatable arrays) |
| Callees | `W3FSD_DAVE` (`is2_w3fsd_dave.hpp`), `W3RPWNICE` (`is2_w3rpwnice.hpp`), already translated |
| Tables | filled by `INSIS2`, [`INSIS2_KOKKOS.md`](INSIS2_KOKKOS.md) |
| Not ported | the test output (`W3_T`), `STRACE` |

## Arithmetic

The reference is the `-O3` object of `w3sis2md.F90` (WW3's CMake Release flags, gfortran 16.2.0,
`-ffp-contract=off`), read as GIMPLE (`-fdump-tree-optimized-lineno`) and assembly.

- (v) `S_D`, the creep term, calls libmvec `_ZGVbN4v_logf` in groups of four lanes from the first
  element and a scalar tail. The C++ uses the same function on the host (`log_scaled`), declared
  `extern "C"`; a scalar `logf` differs by up to about 6 ulp (seen in the first differential run).
- (v) The constants of the GIMPLE (`kMp2`, `kBoltzT`, `kDwatGrav`, `kPi4Young`, `kFlexDen`,
  `kRDen`, `kYoungMp2Sq`) are `static_assert`ed against the values read in the object, with the
  double or float rounding the compiler applied.
- (v) The upper bound of the break-up loop, `IKBREAK`, is evaluated once at loop entry in the
  Fortran; the C++ keeps it in a `const int ikhi`. Using the live variable changed `ICEF` by about
  5 % in the first run.
- (v) `FLOOR` is `fint` then a compare, `NINT` is `lroundf` with the low 32 bits kept, `MAX` and
  `MIN` use the operand order of the `maxss`/`minss` in the object.
- (v) The statement `S = S + DISSIP*CICE*A` is in the `IK` loop over the whole spectrum, as in the
  Fortran (see defects).
- (v) No `vfmadd` in the object.
- (v) The C++ unit compiles with no warning under `-O3 -march=x86-64-v3 -std=c++20 -fopenmp
  -ffp-contract=off -Wall -Wextra -Wpedantic -Wshadow -Wconversion` and under `-O0 -g
  -fsanitize=address,undefined`.
- (v) A throwaway differential program (not kept, ADR-0010) called `__w3sis2md_MOD_w3sis2` and
  `w3sis2_point` on the same inputs and compared `S`, `D`, `ICEF`, `ICEDMAX` and the other outputs
  bit by bit: 9 grids (`NK`/`NTH` 25/24, 3/8, 4/6, 5/7, 7/9, 12/16, 31/25, 6/5, 33/36), 4 seeds,
  `IS2PARS(9)` in 0, 1, 2, `IS2PARS(18)` in 0, 1, 400 calls each, plus a mode with NaN injected
  in the inputs: 0 mismatches. ⚠ No command in the repository reproduces these counts.
- ⚠ Not checked: the function on a device. `expf`, `powf`, `tanhf` and the libmvec `logf` are
  CUDA's there, not glibc's (`docs/GPU_LIBM_BITS_202610.md`). The odd-`NTH` split of the
  `maxpd`/`maxsd` pair matters only for a NaN or `-0`.

## Benchmarks

None. No timing without parity (`AGENTS.md`).

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired: no shim and no caller hunk
in the fork.

## WW3 defects found

Read in the source and the object, not fixed (WW3 is read-only).

- ⚠ `S = S + DISSIP*CICE*A` is inside the `IK` loop over the whole spectrum, so `S` carries
  the sum of the earlier bins; the C++ repeats it.
- ⚠ A NaN or infinite `ICEH` gives an index outside `SIS2ALPHAS`: the Fortran segfaults on it.
  The differential test leaves those inputs out; the C++ reads the same address.
- ⚠ `-4*4/3` in the source is integer division (-5), not the real -5.33 it looks like.
- ⚠ An unset `BRK_CRIT_W` is read only where it was set (argued from the control flow, not run).
