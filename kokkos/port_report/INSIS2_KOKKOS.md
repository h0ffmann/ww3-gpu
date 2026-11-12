# INSIS2_KOKKOS

**Status:** translated, phase 1, not wired, not regtested (2026-11-12).

Initialisation of the IS2 module: the tables SIS1FTABLE, SIS1ALPHATABLE (two variants chosen by
`IS2PARS(18)`) and SIS1ALPHATABLE2 are interpolated in frequency to the model's `SIG`, the mean
floe diameters `ICEDAVETAB` are built, and the directional scattering matrix `IS2SCATMAT` is
filled, diagonalised by `DIAGONALIZE`, giving `IS2EIGVEC` and `IS2EIGVAL`. Once per grid.

## What was ported

| | |
|---|---|
| WW3 routine | `INSIS2`, `model/src/w3sis2md.F90:103-620` at h0ffmann/WW3 `96892c95`, with `DIAGONALIZE` ([`DIAGONALIZE_KOKKOS.md`](DIAGONALIZE_KOKKOS.md)) |
| Reference switch | as [`W3SIS2_KOKKOS.md`](W3SIS2_KOKKOS.md) |
| Kokkos code | [`../src/ww_kokkos/is2_insis2.hpp`](../src/ww_kokkos/is2_insis2.hpp), `ww::is2::insis2(Insis2Grid, Is2Tables&)`, host-only C++ (once per grid; vectors are the outputs) |
| Tables | the DATA of `:195-198`, `:200-300`, `:306-432`, `:454-559` are copied as `constexpr` arrays generated from the Fortran text and compared with the arrays in the GIMPLE (v) |
| Not ported | the test output, the commented `WRITE`s, `SIS1HTABLE` (assigned, never read), `ICEDMIN` |

## Arithmetic

- (v) Every table value is the `REAL` the compiler stored: same bits as the GIMPLE array
  initialisers, compared element by element for the 25, 500, 500 and 500 values.
- (v) `SIS1ALPHATABLE2` has 520 values in its constructor and the `reshape` uses 500; the C++ keeps
  the first 500.
- (v) The interpolation in frequency and the floe-diameter table follow the order of the GIMPLE (checked through the differential run below); no `vfmadd`.
- (v) `IS2SCATMAT`, `IS2EIGVEC` and `IS2EIGVAL` are `DOUBLE PRECISION` as in the Fortran.
- (v) Compiles without warning under the repo flags and under ASan/UBSan (see
  [`W3SIS2_KOKKOS.md`](W3SIS2_KOKKOS.md)).
- (v) A throwaway differential program (not kept, ADR-0010) compared the tables, `IS2SCATMAT`,
  eigenvectors and eigenvalues bit by bit, on the 9 grids and both `IS2PARS(18)` values of the
  `W3SIS2` run: 0 mismatches. ⚠ No command in the repository reproduces it.

## Benchmarks

None. No timing without parity.

## How it was regtested

Not regtested: postponed to the validation phase (ADR-0016). Not wired.

## WW3 defects found

- ⚠ `SIS1ALPHATABLE2`'s constructor lists 520 values, 500 are used.
- ⚠ `DIAGONALIZE` prints 'Too many iterations in DIAGONALIZE' to unit 6; the C++ returns a flag.
