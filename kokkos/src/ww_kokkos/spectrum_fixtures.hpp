// kokkos/src/ww_kokkos/spectrum_fixtures.hpp
// Analytic wave spectra used as inputs by the intro programs and the L1 tests.
//
// These are fixtures, not physics the model runs: they give every lesson and every
// test the same reproducible E(theta, sigma) without reading a WW3 restart. The
// JONSWAP form is the fetch-limited one of Hasselmann et al. (1973); the directional
// factor is the cos^2 spread used by WW3's default output. Both are written as
// KOKKOS_INLINE_FUNCTION so the same code runs inside a parallel_for on any backend.
//
// The reason a test can assert anything at all: with the peak enhancement switched
// off (gamma == 1) the frequency integral of the JONSWAP form has a closed solution,
//
//     m0 = integral_0^inf alpha g^2 sigma^-5 exp(-1.25 (sigma_p/sigma)^4) dsigma
//        = alpha g^2 / (5 sigma_p^4),
//
// which jonswap_m0() returns. Comparing a discrete sum against it measures exactly
// the discretisation and tail-truncation error of the spectral grid.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww {

inline constexpr Real kPi = static_cast<Real>(3.14159265358979323846);
inline constexpr Real kGravity = static_cast<Real>(9.81);
/// JONSWAP peak enhancement for a young, fetch-limited sea.
inline constexpr Real kJonswapGamma = static_cast<Real>(3.3);

/// Dimensionless fetch g*X/U10^2, the single parameter of the fetch-limited laws.
KOKKOS_INLINE_FUNCTION Real dimensionless_fetch(Real u10, Real fetch) {
  return kGravity * fetch / sqr(u10);
}

/// Phillips-like scale alpha = 0.076 * Xtilde^-0.22.
KOKKOS_INLINE_FUNCTION Real jonswap_alpha(Real u10, Real fetch) {
  return static_cast<Real>(0.076) *
         Kokkos::pow(dimensionless_fetch(u10, fetch), static_cast<Real>(-0.22));
}

/// Peak radian frequency: f_p = 3.5 (g/U10) Xtilde^-0.33, sigma_p = 2 pi f_p.
KOKKOS_INLINE_FUNCTION Real jonswap_peak_sigma(Real u10, Real fetch) {
  const Real fp = static_cast<Real>(3.5) * (kGravity / u10) *
                  Kokkos::pow(dimensionless_fetch(u10, fetch), static_cast<Real>(-0.33));
  return static_cast<Real>(2) * kPi * fp;
}

/// JONSWAP variance density at radian frequency `sigma`, in m^2 s/rad.
/// Pass gamma = 1 to get the form whose integral jonswap_m0() knows exactly.
KOKKOS_INLINE_FUNCTION Real jonswap(Real sigma, Real u10, Real fetch,
                                    Real gamma = kJonswapGamma) {
  if (sigma <= static_cast<Real>(0)) return static_cast<Real>(0);
  const Real sp = jonswap_peak_sigma(u10, fetch);
  const Real sigma_a = (sigma <= sp) ? static_cast<Real>(0.07) : static_cast<Real>(0.09);
  const Real r = Kokkos::exp(-sqr(sigma - sp) / (static_cast<Real>(2) * sqr(sigma_a * sp)));
  const Real pm = jonswap_alpha(u10, fetch) * sqr(kGravity) *
                  Kokkos::pow(sigma, static_cast<Real>(-5)) *
                  Kokkos::exp(static_cast<Real>(-1.25) *
                              Kokkos::pow(sp / sigma, static_cast<Real>(4)));
  return pm * Kokkos::pow(gamma, r);
}

/// Closed-form zeroth moment of jonswap(..., gamma = 1). Exact, not a quadrature.
KOKKOS_INLINE_FUNCTION Real jonswap_m0(Real u10, Real fetch) {
  const Real sp = jonswap_peak_sigma(u10, fetch);
  return jonswap_alpha(u10, fetch) * sqr(kGravity) /
         (static_cast<Real>(5) * sqr(sqr(sp)));
}

/// cos^2 directional spread about `theta_mean`, in 1/rad, clipped outside +-pi/2.
/// Normalised so that its integral over a full circle is exactly 1.
KOKKOS_INLINE_FUNCTION Real cos2_spread(Real theta, Real theta_mean) {
  Real d = theta - theta_mean;
  const Real two_pi = static_cast<Real>(2) * kPi;
  while (d > kPi) d -= two_pi;
  while (d < -kPi) d += two_pi;
  if (Kokkos::fabs(d) >= static_cast<Real>(0.5) * kPi) return static_cast<Real>(0);
  return (static_cast<Real>(2) / kPi) * sqr(Kokkos::cos(d));
}

/// A WW3-shaped spectral grid: `nth` equally spaced directions and `nk` frequency
/// bands in geometric progression, which is what w3gdatmd's SIG/DSII arrays hold.
struct SpectralGrid {
  int nth;      ///< number of directions
  int nk;       ///< number of frequency bands
  Real th0;     ///< first direction, rad
  Real sigma0;  ///< lowest band centre, rad/s
  Real xfr;     ///< band ratio sigma(k+1)/sigma(k)

  KOKKOS_INLINE_FUNCTION Real dtheta() const {
    return static_cast<Real>(2) * kPi / static_cast<Real>(nth);
  }
  KOKKOS_INLINE_FUNCTION Real theta(int ith) const {
    return th0 + static_cast<Real>(ith) * dtheta();
  }
  KOKKOS_INLINE_FUNCTION Real sigma(int ik) const {
    return sigma0 * Kokkos::pow(xfr, static_cast<Real>(ik));
  }
  /// Band width, WW3's DSII = sigma * (XFR - 1/XFR) / 2.
  KOKKOS_INLINE_FUNCTION Real dsigma(int ik) const {
    return sigma(ik) * (xfr - static_cast<Real>(1) / xfr) * static_cast<Real>(0.5);
  }
};

/// WW3's own defaults: 24 directions, 32 bands from 0.0373 Hz with XFR = 1.1.
KOKKOS_INLINE_FUNCTION SpectralGrid default_grid() {
  return SpectralGrid{24, 32, static_cast<Real>(0),
                      static_cast<Real>(2) * kPi * static_cast<Real>(0.0373),
                      static_cast<Real>(1.1)};
}

}  // namespace ww
