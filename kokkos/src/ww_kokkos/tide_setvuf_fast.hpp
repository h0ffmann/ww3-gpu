// kokkos/src/ww_kokkos/tide_setvuf_fast.hpp
// SETVUF_FAST: the nodal corrections f, u and V of the TIDE_MF analysed constituents at one
// time and latitude, the shallow-water ones built from their main constituents.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:531-667 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/SETVUF_FAST_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

namespace ww::tide {

namespace setvuf_fast_detail {
// The object calls glibc's scalar sinf, sincosf (for COS and SIN of UU*TWOPI, :629-630), atan2f
// and powf; g++ merges ::cosf and ::sinf of one argument into the same sincosf.
// ⚠ On a device, Kokkos' functions are CUDA's and not shown to return glibc's bits.
KOKKOS_INLINE_FUNCTION float sin_(float x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sin(x);))
#else
  return Kokkos::sin(x);
#endif
}
KOKKOS_INLINE_FUNCTION float cos_(float x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::cosf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::cos(x);))
#else
  return Kokkos::cos(x);
#endif
}
KOKKOS_INLINE_FUNCTION float atan2_(float y, float x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::atan2f(y, x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::atan2(y, x);))
#else
  return Kokkos::atan2(y, x);
#endif
}
KOKKOS_INLINE_FUNCTION float pow_(float x, float y) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::powf(x, y);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::pow(x, y);))
#else
  return Kokkos::pow(x, y);
#endif
}
// INT() of a REAL: cvttss2si, 0x80000000 outside the int range or for NaN.
KOKKOS_INLINE_FUNCTION int fint(float x) {
  return (x > -2147483904.f && x < 2147483648.f) ? static_cast<int>(x)
                                                 : static_cast<int>(0x80000000u);
}
// PI=3.1415926536 and TWOPI=2.*3.1415926536 as REAL(KIND=4) PARAMETERs (:543-544).
inline constexpr float kPi = 3.1415926536f;
inline constexpr float kTwoPi = 2.f * 3.1415926536f;
static_assert(kPi == 0x1.921fb6p+1f && kTwoPi == 0x1.921fb6p+2f);
}  // namespace setvuf_fast_detail

/// SETVUF_FAST(h,pp,s,p,enp,dh,dpp,ds,dp,dnp,tau,XLAT,F,U,V). dh, dpp, ds, dp, dnp are never
/// read. f, u, v: F(NTIL=44), U, V, written for L=1..TIDE_MF only, and not at all when
/// NTIDAL_CON+1 > NTOTAL_CON (the RETURN at :645) ⚠.
/// Then the W3TIDEMD state it reads (arrays 0-based, Fortran base 1; every index they hold is
/// 1-based): ntidal_con, ntotal_con, tide_mf; nj (NTOTAL_CON); ii, jj, kk, ll, mm, nn, semi
/// (NTIDAL_CON); ee, ph, ldel, mdel, ndel, ir (180); tide_index2 (MC = 70); tide_indexj
/// (NTOTAL_CON); tide_indexjk, coef_con (NKONCO).
/// FA, UA, VA (170) are locals, as in the Fortran (:538).
KOKKOS_INLINE_FUNCTION void setvuf_fast(
    double h, double pp, double s, double p, double enp, double dh, double dpp, double ds,
    double dp, double dnp, double tau, float xlat, float* f, float* u, float* v, int ntidal_con,
    int ntotal_con, int tide_mf, const int* nj, const int* ii, const int* jj, const int* kk,
    const int* ll, const int* mm, const int* nn, const float* semi, const float* ee,
    const float* ph, const int* ldel, const int* mdel, const int* ndel, const int* ir,
    const int* tide_index2, const int* tide_indexj, const int* tide_indexjk,
    const float* coef_con) {
  namespace d = setvuf_fast_detail;
  (void)dh;
  (void)dpp;
  (void)ds;
  (void)dp;
  (void)dnp;
  // :558-559. maxss |XLAT|,5. keeps 5. for a NaN.
  const float axlat = Kokkos::fabs(xlat);
  const float cxlat = axlat > 5.f ? axlat : 5.f;
  const float slat = d::sin_(d::kPi * cxlat / 180.f);

  float fa[170];
  float ua[170];
  float va[170];
  // :600-602
  for (int i = 0; i < 170; ++i) fa[i] = 0.f;
  for (int i = 0; i < 170; ++i) ua[i] = 0.f;
  for (int i = 0; i < 170; ++i) va[i] = 0.f;

  int jbase = 0;
  for (int k = 1; k <= ntidal_con; ++k) {
    const int j1 = jbase + 1;
    const int jl = jbase + nj[k - 1];
    for (int l = 1; l <= tide_mf; ++l) {
      if (tide_index2[l - 1] == k) {
        // :609. Evaluated in REAL(8) left to right, stored to the REAL VDBL.
        const float vdbl = static_cast<float>(
            static_cast<double>(ii[k - 1]) * tau + static_cast<double>(jj[k - 1]) * s +
            static_cast<double>(kk[k - 1]) * h + static_cast<double>(ll[k - 1]) * p +
            static_cast<double>(mm[k - 1]) * enp + static_cast<double>(nn[k - 1]) * pp +
            static_cast<double>(semi[k - 1]));
        int iv = d::fint(vdbl);
        iv = (iv / 2) * 2;
        float sumc = 1.f;
        float sums = 0.f;
        for (int j = j1; j <= jl; ++j) {
          float rr = ee[j - 1];
          const int l2 = ir[j - 1] + 1;
          if (l2 == 2) {
            // :622. The parenthesised factor is a PAREN_EXPR in the GIMPLE.
            rr = ee[j - 1] * 0.36309f * (1.f - 5.f * slat * slat) / slat;
          } else if (l2 == 3) {
            rr = ee[j - 1] * 2.59808f * slat;
          }
          const float uudbl = static_cast<float>(
              static_cast<double>(ldel[j - 1]) * p + static_cast<double>(mdel[j - 1]) * enp +
              static_cast<double>(ndel[j - 1]) * pp + static_cast<double>(ph[j - 1]));
          const int iuu = d::fint(uudbl);
          const float uu = uudbl - static_cast<float>(iuu);
          sumc = sumc + rr * d::cos_(uu * d::kTwoPi);
          sums = sums + rr * d::sin_(uu * d::kTwoPi);
        }
        // :633-635
        fa[k - 1] = Kokkos::sqrt(sumc * sumc + sums * sums);
        va[k - 1] = vdbl - static_cast<float>(iv);
        ua[k - 1] = d::atan2_(sums, sumc) / d::kTwoPi;
      }
      jbase = jl;
    }
  }
  // :644-645
  const int k1 = ntidal_con + 1;
  if (k1 > ntotal_con) return;
  for (int k = k1; k <= ntotal_con; ++k) {
    fa[k - 1] = 1.0f;
    va[k - 1] = 0.0f;
    ua[k - 1] = 0.f;
    // :651. DO bounds evaluated once.
    const int jfirst = tide_indexj[k - 1];
    const int jlast = jfirst + nj[k - 1] - 1;
    for (int j = jfirst; j <= jlast; ++j) {
      // L2 = 0 (no match in TIDE_SET_INDICES) reads FA(0), outside the array ⚠.
      const int l2 = tide_indexjk[j - 1];
      fa[k - 1] = fa[k - 1] * d::pow_(fa[l2 - 1], Kokkos::fabs(coef_con[j - 1]));
      va[k - 1] = va[k - 1] + coef_con[j - 1] * va[l2 - 1];
      ua[k - 1] = ua[k - 1] + coef_con[j - 1] * ua[l2 - 1];
    }
  }
  // :659-663. TIDE_MF > 44 writes past F, U, V ⚠.
  for (int l = 1; l <= tide_mf; ++l) {
    f[l - 1] = fa[tide_index2[l - 1] - 1];
    u[l - 1] = ua[tide_index2[l - 1] - 1];
    v[l - 1] = va[tide_index2[l - 1] - 1];
  }
}

}  // namespace ww::tide
