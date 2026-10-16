// kokkos/src/ww_kokkos/tide_setvuf.hpp
// SETVUF: the nodal corrections f, u and the astronomical argument V of every tidal
// constituent at one time hr, written into column ITIME of F_ARG, U_ARG and V_ARG.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:1893-2030 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/SETVUF_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "st4_params.hpp"
#include "tide_astr.hpp"

namespace ww::tide {

namespace setvuf_detail {
/// Returned when ITIME is -1 (:2014-2027): TIDE_INDEX is written here, and the shim does the
/// WRITEs on unit 992 that the Fortran interleaves with it.
inline constexpr int kTestOutput = 2;

// The object calls glibc's sinf, sincosf (COS and SIN of UU*TWOPI, :1976-1977), atan2f and
// powf; g++ merges ::cosf and ::sinf of one argument into the same sincosf.
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
/// INT() of a REAL(8): cvttsd2si, 0x80000000 outside the int range or for NaN.
KOKKOS_INLINE_FUNCTION int dint(double x) {
  return (x > -2147483649. && x < 2147483648.) ? static_cast<int>(x)
                                               : static_cast<int>(0x80000000u);
}
/// Equal-length CHARACTER*5 .eq. is a 5-byte memcmp_eq in the object.
KOKKOS_INLINE_FUNCTION bool name_eq(const char* a, const char* b) {
  bool eq = true;
  for (int c = 0; c < 5; ++c) eq = eq && (a[c] == b[c]);
  return eq;
}
// PI=3.1415926536 and TWOPI=2.*3.1415926536 as REAL(KIND=4) PARAMETERs (:1909-1910).
inline constexpr float kPi = 3.1415926536f;
inline constexpr float kTwoPi = 2.f * 3.1415926536f;
}  // namespace setvuf_detail

/// SETVUF(hr, XLAT, ITIME). itime is the 1-based column. Then the W3TIDEMD state it reads:
/// ntidal_con, ntotal_con, tide_mf; tidecon_allnames (NTOTAL_CON) and tidecon_name (TIDE_MF)
/// as consecutive 5-byte records, read only on the ITIME = -1 path; ii, jj, kk, ll, mm, nn,
/// nj, semi (index K-1); coef_con, tide_indexjk (index J-1); tide_indexj (index K-1, its
/// values 1-based J); ee, ph, ldel, mdel, ndel, ir (the 180-element tables, index J-1);
/// v_arg, f_arg, u_arg (REAL, column major, element (k,itime) at (k-1) + ld_arg*(itime-1),
/// ld_arg = 170 in both allocations, ww3_prnc.F90:1522, ww3_shel.F90:1966); tide_index (MC,
/// written only when itime is -1).
/// The two name searches whose result is never read (INDX at :1948-1952, IFLAG at :1998-2003)
/// change no value and are not translated. Returns 0, or kTestOutput when itime is -1.
KOKKOS_INLINE_FUNCTION int setvuf(double hr, float xlat, int itime, int ntidal_con,
                                  int ntotal_con, int tide_mf, const char* tidecon_allnames,
                                  const char* tidecon_name, const int* ii, const int* jj,
                                  const int* kk, const int* ll, const int* mm, const int* nn,
                                  const int* nj, const float* semi, const float* coef_con,
                                  float* v_arg, float* f_arg, float* u_arg, long ld_arg,
                                  const float* ee, const float* ph, const int* ldel,
                                  const int* mdel, const int* ndel, const int* ir,
                                  const int* tide_indexj, const int* tide_indexjk,
                                  int* tide_index) {
  namespace d = setvuf_detail;
  // MAX_EXPR <ABS(XLAT), 5.> is maxss with ABS(XLAT) as destination (:1918).
  const float cxlat = ww::st4::max_r(Kokkos::fabs(xlat), 5.f);
  const float slat = d::sin_(d::kPi * cxlat / 180.f);
  // :1927-1935, KD0 = 2415020.
  double d1 = hr / 24.0;
  d1 = d1 - 2415020.0;
  double h, pp, s, p, enp, dh, dpp, ds, dp, dnp;
  astr(d1, h, pp, s, p, enp, dh, dpp, ds, dp, dnp);
  // 0.00001 is a default REAL literal, widened to REAL(8): 0x1.4f8b58p-17.
  static_assert(static_cast<double>(0.00001f) == 0x1.4f8b58p-17);
  const int intdys = d::dint((hr + static_cast<double>(0.00001f)) / 24.0);
  const double hh = hr - static_cast<double>(intdys * 24);
  const double tau = hh / 24.0 + h - s;
  const long col = ld_arg * static_cast<long>(itime - 1);

  int jbase = 0;
  for (int k = 1; k <= ntidal_con; ++k) {
    const float vdbl = static_cast<float>(
        static_cast<double>(ii[k - 1]) * tau + static_cast<double>(jj[k - 1]) * s +
        static_cast<double>(kk[k - 1]) * h + static_cast<double>(ll[k - 1]) * p +
        static_cast<double>(mm[k - 1]) * enp + static_cast<double>(nn[k - 1]) * pp +
        static_cast<double>(semi[k - 1]));
    int iv = ww::st4::fint(vdbl);
    iv = (iv / 2) * 2;
    const float vv = vdbl - static_cast<float>(iv);
    const int j1 = jbase + 1;
    const int jl = jbase + nj[k - 1];
    float sumc = 1.f;
    float sums = 0.f;
    for (int j = j1; j <= jl; ++j) {
      float rr = ee[j - 1];
      const int l = ir[j - 1] + 1;
      if (l == 2) {
        rr = ee[j - 1] * 0.36309f * (1.f - 5.f * slat * slat) / slat;
      } else if (l == 3) {
        rr = ee[j - 1] * 2.59808f * slat;
      }
      const float uudbl = static_cast<float>(
          static_cast<double>(ldel[j - 1]) * p + static_cast<double>(mdel[j - 1]) * enp +
          static_cast<double>(ndel[j - 1]) * pp + static_cast<double>(ph[j - 1]));
      const int iuu = ww::st4::fint(uudbl);
      const float uu = uudbl - static_cast<float>(iuu);
      sumc = sumc + rr * d::cos_(uu * d::kTwoPi);
      sums = sums + rr * d::sin_(uu * d::kTwoPi);
    }
    const long at = static_cast<long>(k - 1) + col;
    f_arg[at] = Kokkos::sqrt(sumc * sumc + sums * sums);
    v_arg[at] = vv;
    u_arg[at] = d::atan2_(sums, sumc) / d::kTwoPi;
    jbase = jl;
  }

  // :1990-2010, the shallow-water constituents from their main constituents. F_ARG(L) is read
  // after F_ARG(K) is written, as the object does, so L = K reads the updated value.
  const int k1 = ntidal_con + 1;
  if (k1 > ntotal_con) return 0;
  for (int k = k1; k <= ntotal_con; ++k) {
    const long at = static_cast<long>(k - 1) + col;
    f_arg[at] = 1.0f;
    v_arg[at] = 0.0f;
    u_arg[at] = 0.f;
    for (int j = tide_indexj[k - 1]; j <= tide_indexj[k - 1] + nj[k - 1] - 1; ++j) {
      const long al = static_cast<long>(tide_indexjk[j - 1] - 1) + col;
      f_arg[at] = f_arg[at] * d::pow_(f_arg[al], Kokkos::fabs(coef_con[j - 1]));
      v_arg[at] = v_arg[at] + coef_con[j - 1] * v_arg[al];
      u_arg[at] = u_arg[at] + coef_con[j - 1] * u_arg[al];
    }
  }

  // :2014-2027. ⚠ With ITIME = -1 the loops above already wrote column -1, outside both
  // allocations; the only caller passes ITIME >= 1 (ww3_prnc.F90:1672).
  if (itime == -1) {
    for (int l = 1; l <= tide_mf; ++l) {
      for (int k = 1; k <= ntotal_con; ++k) {
        if (d::name_eq(tidecon_allnames + 5L * (k - 1), tidecon_name + 5L * (l - 1))) {
          tide_index[l - 1] = k;
        }
      }
    }
    return d::kTestOutput;
  }
  return 0;
}

}  // namespace ww::tide
