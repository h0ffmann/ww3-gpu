// kokkos/src/ww_kokkos/is2_funcd_fderiv.hpp
// FUNCD_FDERIV of W3SIS2MD: derivative in k of the flexural ice-coupled dispersion relation,
// 5*C5*k**4 + C1, for the Newton iteration of FINDROOTS_NR.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sis2md.F90:1523-1611 at NOAA-EMC/WW3 03adda91;
// author C. Sevigny (module authors F. Ardhuin, P. Nicot, C. Sevigny, G. Boutin).
// Evidence: kokkos/port_report/FUNCD_FDERIV_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::is2 {

namespace fderiv_detail {
inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV, constants.F90:62
// Folded by gfortran (GIMPLE and .rodata of w3sis2md.F90.o):
inline constexpr Real kYoung = 0x1.473ac0p+32f;       // YOUNG = 5.49E+9 :1590, 0x4fa39d60
inline constexpr Real kFlexDen = 0x1.5d70a4p+3f;      // 12*(1-POISSON**2) :1597, 0x412eb852
inline constexpr Real kDraftRatio = 0x1.ccccccp-1f;   // DENS_ICE/DENS :1598, 0x3f666666
inline constexpr Real kDensGrav = 0x1.3a1932p+13f;    // DENS*GRAV :1599, 0x461d0c99
static_assert(kYoung == static_cast<Real>(5.49E+9));
static_assert(kFlexDen == 12 * (static_cast<Real>(1) - static_cast<Real>(0.3) * static_cast<Real>(0.3)));
static_assert(kDraftRatio == static_cast<Real>(922.5) / static_cast<Real>(1025.0));
static_assert(kDensGrav == static_cast<Real>(1025.0) * kGrav);

/// _gfortran_pow_c8_i4 for n >= 0, as libgfortran 16.2.0 computes it (binary powering from
/// POW = (1,0), full complex products, no NaN recovery: -fcx-fortran-rules). gfortran calls
/// this runtime routine for a DOUBLE COMPLEX ** literal integer; it does not expand it inline.
KOKKOS_INLINE_FUNCTION void pow_c8_i4_nonneg(double xr, double xi, int n, double& pr, double& pi) {
  pr = 1.0;
  pi = 0.0;
  unsigned u = static_cast<unsigned>(n);
  if (u == 0) return;
  for (;;) {
    if (u & 1u) {
      const double r = pr * xr - pi * xi;
      const double i = pr * xi + pi * xr;
      pr = r;
      pi = i;
    }
    u >>= 1;
    if (u == 0) break;
    const double r = xr * xr - xi * xi;
    const double i = xr * xi + xi * xr;
    xr = r;
    xi = i;
  }
}
}  // namespace fderiv_detail

/// FUNCD_FDERIV(WN_GUESS, VISC_RP, ICEH, SIGMA), COMPLEX(8) result.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> funcd_fderiv(const Kokkos::complex<double>& wn_guess,
                                                           Real visc_rp, Real iceh, Real sigma) {
  using namespace fderiv_detail;
  // :1597, ICEH**3 as (ICEH*ICEH)*ICEH.
  const Real flex_rigid = ((iceh * iceh) * iceh * kYoung) / kFlexDen;
  const Real draft = iceh * kDraftRatio;  // :1598
  const Real alp = (sigma * sigma) / kGrav;  // :1601
  const Real gamma = (sigma * visc_rp) / kDensGrav;  // :1605
  // :1606, CMPLX without KIND is COMPLEX(4), widened; -1.*GAMMA is folded to a negation.
  const double c1r = static_cast<double>(static_cast<Real>(1) - alp * draft);
  const double c1i = static_cast<double>(-gamma);
  const Real c5 = flex_rigid / kDensGrav;  // :1607
  // :1609, REAL 5*C5 widened to (5*C5, 0) times a full complex product.
  const double k = static_cast<double>(c5 * static_cast<Real>(5));
  double wr = 0.0;
  double wi = 0.0;
  pow_c8_i4_nonneg(wn_guess.real(), wn_guess.imag(), 4, wr, wi);
  const double tr = k * wr - wi * 0.0;
  const double ti = k * wi + wr * 0.0;
  return Kokkos::complex<double>(c1r + tr, c1i + ti);
}

}  // namespace ww::is2
