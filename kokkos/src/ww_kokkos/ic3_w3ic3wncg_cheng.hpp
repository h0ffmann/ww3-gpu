// kokkos/src/ww_kokkos/ic3_w3ic3wncg_cheng.hpp
// W3IC3WNCG_CHENG: update the complex wavenumber and group velocity for the IC3 (Cheng)
// viscoelastic ice-layer dispersion relation at each frequency, from the pre-calculated table.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1800-1959 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, X. Zhao, S. Cheng.
// Evidence: kokkos/port_report/W3IC3WNCG_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_cginic3_cheng.hpp"
#include "ic3_cmplx_root_muller_cheng.hpp"
#include "ic3_smooth_k.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"

namespace ww::ic3 {

/// W3IC3WNCG_CHENG. wn_r, wn_i, cg: Fortran (1:NK) or (0:NK+1), here 0-based with the same n.
/// sig is SIG(1:NK) or SIG(0:NK+1), also 0-based. The tables ic3wn_r/i/cg are the Fortran's
/// IC3WN_R/I/CG(0:NK+1, 0:ITKNUM) in column-major order with leading dimension nk + 2; this
/// routine reads column `i` (the one closest to HICE and not larger than it). diff and removeid
/// are work arrays of length n for SMOOTH_K. root_finder is a callable with the signature of
/// CmplxRootMullerCheng.
template <class RootFinder>
inline void w3ic3wncg_cheng(Real* wn_r, Real* wn_i, Real* cg, Real ice1, Real ice2, Real ice3,
                            Real ice4, Real dpt, int nk, int n, const Real* sig, Real ic3hilim,
                            Real ic3kilim, const Real* ic3wn_r, const Real* ic3wn_i,
                            const Real* ic3cg, int itknum, Real ic3_ditk,
                            const ww::prop::DispTable& disp, Real* diff, Real* removeid,
                            RootFinder&& root_finder) {
  const std::ptrdiff_t ld = static_cast<std::ptrdiff_t>(nk) + 2;
  // :1912-1914. DEPTH, HICE, NU, DICE, ES_MOD are widened to REAL(8) once.
  double depth = static_cast<double>(dpt);
  double hice = static_cast<double>(ice1);
  const double nu = static_cast<double>(ice2);
  const double dice = static_cast<double>(ice3);
  const double es_mod = static_cast<double>(ice4);

  // :1916. HICE = MIN(DBLE(IC3HILIM), HICE), both double.
  hice = Kokkos::fmin(static_cast<double>(ic3hilim), hice);

  // :1921-1922. ITKNUM and I are integers; NINT rounds half away from zero.
  const int itkn = Kokkos::fmin(
      static_cast<double>(Kokkos::round(hice / static_cast<double>(ic3_ditk))),
      static_cast<double>(itknum));
  const std::ptrdiff_t col = static_cast<std::ptrdiff_t>(itkn) * ld;

  // :1896-1907. n is either NK or NK+2; kl and ku are 1-based loop bounds.
  const int kl = 1;
  const int ku = n;
  const std::ptrdiff_t i1 = (n == nk) ? 1 : 0;
  const std::ptrdiff_t i2 = (n == nk) ? nk : nk + 1;

  // :1926-1928. Copy the selected table column. Fortran slices X(I1:I2) map to 0-based offsets.
  for (std::ptrdiff_t idx = i1; idx <= i2; ++idx) {
    const std::ptrdiff_t out = idx - i1;
    wn_r[out] = ic3wn_r[col + idx];
    wn_i[out] = ic3wn_i[col + idx];
    cg[out] = ic3cg[col + idx];
  }

  // :1929.
  constexpr double rr = 0.01;

  // :1932-1944. Recompute the shallow-water entries.
  for (int ik = kl; ik <= ku; ++ik) {
    // WN_R(IK)*DEPTH > 4.0: WN_R is Real, promoted to double; 4.0 is default Real, promoted.
    if (static_cast<double>(wn_r[ik - 1]) * depth > 4.0) {
      break;
    }
    const Kokkos::complex<double> x1(static_cast<double>(wn_r[ik - 1]),
                                     static_cast<double>(wn_i[ik - 1]));
    const Kokkos::complex<double> x0(x1.real() * (1.0 - rr) - x1.imag() * 0.0,
                                     x1.imag() * (1.0 - rr) + x1.real() * 0.0);
    const Kokkos::complex<double> x2(x1.real() * (1.0 + rr) - x1.imag() * 0.0,
                                     x1.imag() * (1.0 + rr) + x1.real() * 0.0);
    Kokkos::complex<double> wncomplex;
    root_finder(x0, x1, x2, 1, static_cast<double>(sig[ik - 1]), es_mod, nu, dice, hice, depth,
                wncomplex);
    // :1941-1942. REAL() of a DOUBLE COMPLEX rounds to the default REAL kind.
    wn_i[ik - 1] = static_cast<Real>(wncomplex.imag());
    wn_r[ik - 1] = static_cast<Real>(wncomplex.real());
  }

  // :1946. SMOOTH_K marks the switch point with 0 (none); n = KU - KL + 1.
  smooth_k(wn_r, wn_i, sig, n, 0, diff, removeid);

  // :1948-1950. Limit ki.
  for (int ik = kl; ik <= ku; ++ik) {
    wn_i[ik - 1] = Kokkos::fmin(wn_i[ik - 1], ic3kilim);
  }

  // :1953. Update group velocity.
  cginic3_cheng(cg, sig, wn_r, n);
}

}  // namespace ww::ic3
