// kokkos/src/ww_kokkos/gke_prepkgrid.hpp
// PrepKGrid of W3GKEMD: the (sigma, theta) grid mapped to (kx, ky), the area element
// k dk dtheta and sigma per spectral bin, and k per frequency for a given depth.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:1066-1197 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/PREPKGRID_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "prop_wavnu1.hpp"
#include "real.hpp"

namespace ww::gke {

/// qr_tpi = 2 * qr_pi, :199-200: the double literal rounded to REAL, then doubled (exact).
inline constexpr Real kQrTpi = 0x1.921fb6p+2f;
static_assert(kQrTpi == static_cast<Real>(2) * static_cast<Real>(3.141592653589793),
              "qr_tpi as w3gkemd.F90:200 builds it");

namespace prepkgrid_detail {
// gfortran fuses SIN and COS of the same angle into one sincosf call (the object calls
// sincosf, :1124-1125); the host calls glibc's sincosf too.
// ⚠ On a device Kokkos::sin/cos (CUDA libdevice) are not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION void sincos_(Real x, Real& s, Real& c) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((::sincosf(x, &s, &c);))
  KOKKOS_IF_ON_DEVICE((s = Kokkos::sin(x); c = Kokkos::cos(x);))
#else
  s = Kokkos::sin(x);
  c = Kokkos::cos(x);
#endif
}
}  // namespace prepkgrid_detail

/// PrepKGrid(nk, nth, dpt, sig, th), :1066-1197. Called once, by PrepKernelIO (:1272) with
/// dpt = qr_depth; runs on one thread.
/// sig[nk], th[nth] (Fortran base 1). Outputs, the module arrays PrepKernelIO allocated
/// (:1251-1252, :1269-1271): qr_kx, qr_ky, qr_dk, qr_om [nk*nth], index ith + (ik-1)*nth (base 1),
/// qr_wn1 [nk]. disp is the DISTAB table WAVNU1 reads. esin, ecos: nth floats each, the
/// Fortran's automatic arrays (:1108).
/// dispopt is a PARAMETER fixed to 1 (:1103): only the WAVNU1 path is compiled in; the Pade
/// branch (:1147-1161) is dead code and not translated.
/// ⚠ nk == 1 reads sig[1] past the end at :1172, as the Fortran does.
KOKKOS_INLINE_FUNCTION void prepkgrid(int nk, int nth, Real dpt, const Real* sig, const Real* th,
                                      Real* qr_kx, Real* qr_ky, Real* qr_dk, Real* qr_om,
                                      Real* qr_wn1, const ww::prop::DispTable& disp, Real* esin,
                                      Real* ecos) {
  const int ns = nk * nth;  // :1111
  // :1113-1117, whole arrays (memset in the object).
  for (int i = 0; i < ns; ++i) qr_kx[i] = static_cast<Real>(0.);
  for (int i = 0; i < ns; ++i) qr_ky[i] = static_cast<Real>(0.);
  for (int i = 0; i < ns; ++i) qr_dk[i] = static_cast<Real>(0.);
  for (int i = 0; i < ns; ++i) qr_om[i] = static_cast<Real>(0.);
  for (int i = 0; i < nk; ++i) qr_wn1[i] = static_cast<Real>(0.);

  const Real dth = kQrTpi / static_cast<Real>(nth);  // :1120, divss by cvtsi2ss nth

  for (int ith = 0; ith < nth; ++ith) {  // :1122-1144
    const Real angr = th[ith];
    prepkgrid_detail::sincos_(angr, esin[ith], ecos[ith]);
    if (Kokkos::fabs(esin[ith]) < static_cast<Real>(1.E-5)) {  // :1127; NaN goes on
      esin[ith] = static_cast<Real>(0.);
      if (ecos[ith] > static_cast<Real>(0.5)) {  // NaN gives -1
        ecos[ith] = static_cast<Real>(1.);
      } else {
        ecos[ith] = static_cast<Real>(-1.);
      }
    }
    if (Kokkos::fabs(ecos[ith]) < static_cast<Real>(1.E-5)) {  // :1136
      ecos[ith] = static_cast<Real>(0.);
      if (esin[ith] > static_cast<Real>(0.5)) {
        esin[ith] = static_cast<Real>(1.);
      } else {
        esin[ith] = static_cast<Real>(-1.);
      }
    }
  }

  for (int ik = 1; ik <= nk; ++ik) {  // :1146-1187
    Real k;
    Real cg;
    ww::prop::wavnu1(disp, sig[ik - 1], dpt, k, cg);  // :1164, an out-of-line call in the object
    qr_wn1[ik - 1] = k;                                // :1166
    Real dsii;
    if (ik == 1) {  // :1171-1177, first bin tested first
      dsii = static_cast<Real>(0.5) * (sig[1] - sig[0]);
    } else if (ik == nk) {
      dsii = static_cast<Real>(0.5) * (sig[nk - 1] - sig[nk - 2]);
    } else {
      dsii = static_cast<Real>(0.5) * (sig[ik] - sig[ik - 2]);
    }
    for (int ith = 1; ith <= nth; ++ith) {  // :1179-1186
      const int jkth = ith + (ik - 1) * nth;
      qr_kx[jkth - 1] = k * ecos[ith - 1];
      qr_ky[jkth - 1] = k * esin[ith - 1];
      qr_dk[jkth - 1] = k * dsii / cg * dth;  // ((k*dsii)/cg)*dth, hoisted out of ith
      qr_om[jkth - 1] = sig[ik - 1];
    }
  }
}

}  // namespace ww::gke
