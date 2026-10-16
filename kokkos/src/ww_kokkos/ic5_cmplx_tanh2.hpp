// kokkos/src/ww_kokkos/ic5_cmplx_tanh2.hpp
// CMPLX_TANH2 of W3SIC5MD: tanh of a COMPLEX(KDPC) as (1 - EXP(-2*X)) / (1 + EXP(-2*X)).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:2160-2256 at NOAA-EMC/WW3 03adda91; author Q. Liu.
// Evidence: kokkos/port_report/CMPLX_TANH2_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include <complex>

namespace ww::ic5 {

/// CMPLX_TANH2(X), w3sic5md.F90:2252. The header's "Subroutines used" names CMPLX_SINH and
/// CMPLX_COSH, but the body calls neither.
/// Written out in scalars because gfortran's sequence is not Kokkos::complex's: 2*X is a full
/// complex product with (2,0) (the X%im*0 and X%re*0 terms stay, they carry NaN and signed zero),
/// EXP(-2*X) is evaluated once and is glibc's cexp, the constant 1 enters as (1,0) so the imaginary
/// parts are 0-e and e+0, and the quotient is Smith's algorithm (-fcx-fortran-rules), no scaling
/// and no NaN recovery.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cmplx_tanh2(const Kokkos::complex<double>& x) {
  const double xr = x.real();
  const double xi = x.imag();
  const double tr = xr * 2.0 - xi * 0.0;
  const double ti = xr * 0.0 + xi * 2.0;
  double er = 0.0;
  double ei = 0.0;
  KOKKOS_IF_ON_HOST((
    // libstdc++ lowers std::exp(std::complex<double>) to __builtin_cexp, the cexp call of the object.
    const std::complex<double> e = std::exp(std::complex<double>(-tr, -ti));
    er = e.real();
    ei = e.imag();
  ))
  KOKKOS_IF_ON_DEVICE((
    // No glibc cexp on a device: not bit-identical to the Fortran (port report).
    const Kokkos::complex<double> e = Kokkos::exp(Kokkos::complex<double>(-tr, -ti));
    er = e.real();
    ei = e.imag();
  ))
  const double nr = 1.0 - er;
  const double ni = 0.0 - ei;
  const double dr = er + 1.0;
  const double di = ei + 0.0;
  if (Kokkos::fabs(dr) < Kokkos::fabs(di)) {
    const double r = dr / di;
    const double den = di + dr * r;
    return Kokkos::complex<double>((ni + nr * r) / den, (ni * r - nr) / den);
  }
  const double r = di / dr;
  const double den = dr + di * r;
  return Kokkos::complex<double>((nr + ni * r) / den, (ni - nr * r) / den);
}

}  // namespace ww::ic5
