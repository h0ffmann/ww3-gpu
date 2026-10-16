// kokkos/src/ww_kokkos/ic5_nr_corr.hpp
// NR_CORR of W3SIC5MD: the Newton-Raphson step f(k)/f'(k) of the ice-covered dispersion
// relation f(k) = (C1*k**4 + C2)*k*tanh(kH) - 1, for a complex wavenumber.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic5md.F90:1602-1750 at NOAA-EMC/WW3 03adda91;
// author Q. Liu (module authors Q. Liu, E. Rogers).
// Evidence: kokkos/port_report/NR_CORR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <complex>

#include <Kokkos_Core.hpp>

#include "ic5_cmplx_sinh.hpp"
#include "ic5_cmplx_tanh2.hpp"
#include "is2_funcd_fval.hpp"

namespace ww::ic5 {

namespace nr_corr_detail {
/// T**2. with a REAL exponent is glibc's cpow(T, (2,0)) in the object (`__builtin_cpow`), not
/// T*T; libstdc++'s complex/complex std::pow lowers to the same builtin. ⚠ A device has no
/// glibc cpow: Kokkos::pow is not bit for bit with it.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> cpow2(const Kokkos::complex<double>& z) {
  KOKKOS_IF_ON_HOST((const std::complex<double> p =
                         std::pow(std::complex<double>(z.real(), z.imag()), std::complex<double>(2.0, 0.0));
                     return Kokkos::complex<double>(p.real(), p.imag());))
  KOKKOS_IF_ON_DEVICE((return Kokkos::pow(z, Kokkos::complex<double>(2.0, 0.0));))
}
}  // namespace nr_corr_detail

/// NR_CORR(K, C1, C2, H), :1602-1750. K, C1, C2 are COMPLEX(KDPC) (C2 complex at :1703, though
/// the header's table says real), H is REAL(KDP). Every complex product is gfortran's full
/// (ac-bd, ad+bc) without NaN recovery, a REAL or INTEGER factor entering as (x, 0), and the
/// quotient is Smith's algorithm (-fcx-fortran-rules).
/// ⚠ g++ puts the operands of some commutative products and sums in the other order than
/// gfortran (port report): the same bits unless both operands are NaN with different encodings.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> nr_corr(const Kokkos::complex<double>& k,
                                                       const Kokkos::complex<double>& c1,
                                                       const Kokkos::complex<double>& c2, double h) {
  const double kr = k.real();
  const double ki = k.imag();
  const double c1r = c1.real();
  const double c1i = c1.imag();
  const double c2r = c2.real();
  const double c2i = c2.imag();
  // :1726, H widened to (H, 0).
  const double tr = kr * h - ki * 0.0;
  const double ti = kr * 0.0 + ki * h;
  // K**4 is libgfortran's _gfortran_pow_c8_i4(K, 4), not expanded by gfortran.
  double k4r = 0.0;
  double k4i = 0.0;
  ww::is2::gfortran_pow_c8_i4(kr, ki, 4, k4r, k4i);
  // :1727.
  const double lamr = (c1r * k4r - c1i * k4i) + c2r;
  const double lami = (c1r * k4i + c1i * k4r) + c2i;
  // :1729, (5*C1)*K**4 with 5 as (5, 0).
  const double f5r = c1r * 5.0 - c1i * 0.0;
  const double f5i = c1r * 0.0 + c1i * 5.0;
  const double lamprr = (f5r * k4r - f5i * k4i) + c2r;
  const double lampri = (f5r * k4i + f5i * k4r) + c2i;
  // LAM*K and LAM*TKH, used by both branches.
  const double lkr = lamr * kr - lami * ki;
  const double lki = lamr * ki + lami * kr;
  const double ltr = lamr * tr - lami * ti;
  const double lti = lamr * ti + lami * tr;

  double fvr = 0.0;
  double fvi = 0.0;
  double dfr = 0.0;
  double dfi = 0.0;
  // :1731, REAL(REAL(TKH)) rounds to default REAL; a NaN takes the ELSE branch, as in the object.
  if (static_cast<float>(tr) <= 7.5f) {
    // :1735-1736. gfortran inlines CMPLX_SINH and CMPLX_COSH on TKH and shares one pair of cexp
    // calls, EXP(TKH) first, between them.
    const Kokkos::complex<double> ep = cmplx_sinh_detail::cexp(Kokkos::complex<double>(tr, ti));
    const Kokkos::complex<double> em = cmplx_sinh_detail::cexp(Kokkos::complex<double>(-tr, -ti));
    const double dr = ep.real() - em.real();
    const double di = ep.imag() - em.imag();
    const double sr = dr * 0.5 - di * 0.0;
    const double si = dr * 0.0 + di * 0.5;
    const double ar = ep.real() + em.real();
    const double ai = ep.imag() + em.imag();
    const double cr = ar * 0.5 - ai * 0.0;
    const double ci = ar * 0.0 + ai * 0.5;
    fvr = (lkr * sr - lki * si) - cr;
    fvi = (lkr * si + lki * sr) - ci;
    // LAMPR - H subtracts H from the real part only.
    const double pr = lamprr - h;
    dfr = (ltr * cr - lti * ci) + (pr * sr - lampri * si);
    dfi = (ltr * ci + lti * cr) + (pr * si + lampri * sr);
  } else {
    // :1741-1743. The three CMPLX_TANH2(TKH) are one value.
    const Kokkos::complex<double> t = cmplx_tanh2(Kokkos::complex<double>(tr, ti));
    const double t_r = t.real();
    const double t_i = t.imag();
    fvr = (lkr * t_r - lki * t_i) - 1.0;
    fvi = lkr * t_i + lki * t_r;
    const Kokkos::complex<double> t2 = nr_corr_detail::cpow2(t);
    // 1 enters as (1, 0): the imaginary part is 0 - x, which keeps the sign of a zero.
    const double or_ = 1.0 - t2.real();
    const double oi = 0.0 - t2.imag();
    dfr = (lamprr * t_r - lampri * t_i) + (ltr * or_ - lti * oi);
    dfi = (lamprr * t_i + lampri * t_r) + (ltr * oi + lti * or_);
  }
  // :1746, Smith's division.
  if (Kokkos::fabs(dfr) < Kokkos::fabs(dfi)) {
    const double r = dfr / dfi;
    const double den = dfi + dfr * r;
    return Kokkos::complex<double>((fvi + fvr * r) / den, (fvi * r - fvr) / den);
  }
  const double r = dfi / dfr;
  const double den = dfr + dfi * r;
  return Kokkos::complex<double>((fvr + fvi * r) / den, (fvi - fvr * r) / den);
}

}  // namespace ww::ic5
