// kokkos/src/ww_kokkos/is2_funcd_fval.hpp
// FUNCD_FVAL of W3SIS2MD: the ice-covered dispersion relation residual
// F(k) = C5*k**5 + C1*k - ALP for a complex wavenumber guess, the function
// whose root FINDROOTS_NR seeks. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sis2md.F90:1420-1506 at NOAA-EMC/WW3 03adda91;
// author C. Sevigny. Evidence: kokkos/port_report/FUNCD_FVAL_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::is2 {

namespace funcd_fval_detail {
// Folded by gfortran (GIMPLE of w3sis2md.F90:1494-1502); built as the Fortran builds them.
inline constexpr float kYoung = 0x1.473acp+32f;  // YOUNG = 5.49E+9, :1489
static_assert(kYoung == 5.49e+9f);
// 12*(1-POISSON**2), POISSON = 0.3, in REAL: 10.92000007629...
inline constexpr float kRigidDen = 0x1.5d70a4p+3f;
static_assert(kRigidDen == 12.0f * (1.0f - 0.3f * 0.3f));
// DENS_ICE/DENS = 922.5/1025.0, :1495.
inline constexpr float kIceRatio = 0x1.ccccccp-1f;
static_assert(kIceRatio == 922.5f / 1025.0f);
// DENS*GRAV, GRAV = 9.806 (constants.F90:62), :1496.
inline constexpr float kDensGrav = 0x1.3a1932p+13f;
static_assert(kDensGrav == 1025.0f * 9.806f);
inline constexpr float kGrav = 9.806f;
}  // namespace funcd_fval_detail

/// libgfortran's _gfortran_pow_c8_i4 (COMPLEX(8)**INTEGER(4)) for n >= 0, which gfortran
/// calls for WN_GUESS**5 instead of expanding it: binary powering from POW = (1,0), the
/// multiply without the C99 NaN recovery (no __muldc3 in libgfortran.so 16.2.0). n < 0
/// (Smith's division) is not translated: no caller in this module passes one.
KOKKOS_INLINE_FUNCTION void gfortran_pow_c8_i4(double xr, double xi, int n, double& pr,
                                               double& pi) {
  pr = 1.0;
  pi = 0.0;
  if (n == 0) return;
  unsigned u = static_cast<unsigned>(n);
  for (;;) {
    if (u & 1u) {
      // POW = POW*X.
      const double tr = pr * xr - pi * xi;
      const double ti = pr * xi + pi * xr;
      pr = tr;
      pi = ti;
    }
    u >>= 1;
    if (u == 0u) break;
    // X = X*X; the object computes the imaginary part as t+t, t = xr*xi.
    const double t = xr * xi;
    xr = xr * xr - xi * xi;
    xi = t + t;
  }
}

/// FUNCD_FVAL(WN_GUESS, VISC_RP, ICEH, SIGMA), :1420-1506. WN_GUESS is DOUBLE COMPLEX;
/// C1 is built by CMPLX in default REAL (:1501) and only then widened to double.
KOKKOS_INLINE_FUNCTION Kokkos::complex<double> funcd_fval(const Kokkos::complex<double>& wn_guess,
                                                          float visc_rp, float iceh, float sigma) {
  using namespace funcd_fval_detail;
  // :1494, ICEH**3 as (ICEH*ICEH)*ICEH.
  const float flex_rigid = ((iceh * iceh) * iceh) * kYoung / kRigidDen;
  const float draft = iceh * kIceRatio;  // :1495
  const float alp = (sigma * sigma) / kGrav;  // :1498
  const float gamma = (visc_rp * sigma) / kDensGrav;  // :1500
  const double c1r = static_cast<double>(1.0f - draft * alp);  // :1501
  const double c1i = static_cast<double>(-gamma);
  const float c5 = flex_rigid / kDensGrav;  // :1502

  // :1504. C5 is widened to COMPLEX(8) (C5, 0) and multiplied in full.
  const double ar = wn_guess.real();
  const double ai = wn_guess.imag();
  double p5r = 0.0;
  double p5i = 0.0;
  gfortran_pow_c8_i4(ar, ai, 5, p5r, p5i);
  const double c5d = static_cast<double>(c5);
  const double t1r = c5d * p5r - p5i * 0.0;
  const double t1i = c5d * p5i + p5r * 0.0;
  const double t2r = c1r * ar - c1i * ai;
  const double t2i = c1r * ai + c1i * ar;
  const double fr = (t1r + t2r) - static_cast<double>(alp);
  const double fi = t1i + t2i;
  return Kokkos::complex<double>(fr, fi);
}

}  // namespace ww::is2
