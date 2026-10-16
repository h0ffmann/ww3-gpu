// kokkos/src/ww_kokkos/gke_biinterpwt.hpp
// BiInterpWT of W3GKEMD: indices and weights of the bilinear (k, theta) interpolation of the
// fourth wavenumber of each quartet, and its tail weight outside [kmin, kmax].
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3gkemd.F90:1483-1618 at NOAA-EMC/WW3 03adda91;
// authors O. Gramstad, Q. Liu. Evidence: kokkos/port_report/BIINTERPWT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "gke_prepkgrid.hpp"
#include "real.hpp"
#include "st4_params.hpp"

namespace ww::gke {

namespace biinterpwt_detail {

/// qr_fpow / 2. - 2., :188 and :1520, folded: -4.5 is the exponent the object passes to powf.
inline constexpr Real kQrKpow = static_cast<Real>(-5.) / static_cast<Real>(2.) - static_cast<Real>(2.);
static_assert(kQrKpow == -4.5f, "qr_kpow as folded at -O3");

/// HUGE(1.0): MINVAL and MAXVAL of an empty wn.
inline constexpr Real kHuge = 0x1.fffffep+127f;

/// 32-bit integer add and multiply as the object does them (wrapping), for the index sums of
/// a NaN angle or wavenumber, where FLOOR gives INT_MAX.
KOKKOS_INLINE_FUNCTION int add_w(int a, int b) {
  return static_cast<int>(static_cast<unsigned>(a) + static_cast<unsigned>(b));
}
KOKKOS_INLINE_FUNCTION int mul_w(int a, int b) {
  return static_cast<int>(static_cast<unsigned>(a) * static_cast<unsigned>(b));
}

/// FLOOR(x) as the -O3 object computes it at :1558 and :1585: i = INT(x) (cvttss2si), then
/// REAL(i) <= x ? i : i-1; a NaN gives INT_MIN - 1, which wraps to INT_MAX.
KOKKOS_INLINE_FUNCTION int ffloor(Real x) {
  const int i = ww::st4::fint(x);
  return static_cast<Real>(i) <= x ? i : add_w(i, -1);
}

/// MINVAL(wn), :1522, as gfortran inlines it: the first non-NaN element starts the scan,
/// later NaNs are skipped, all NaN gives NaN, nk <= 0 gives HUGE.
KOKKOS_INLINE_FUNCTION Real minval(int nk, const Real* wn) {
  if (nk <= 0) return kHuge;
  int i = 0;
  while (!(wn[i] <= Kokkos::Experimental::infinity_v<Real>)) {
    if (++i == nk) return Kokkos::Experimental::quiet_NaN_v<Real>;
  }
  Real lim = wn[i];
  for (++i; i < nk; ++i) lim = wn[i] < lim ? wn[i] : lim;
  return lim;
}

/// MAXVAL(wn), :1523, the mirror of minval: nk <= 0 gives -HUGE.
KOKKOS_INLINE_FUNCTION Real maxval(int nk, const Real* wn) {
  if (nk <= 0) return -kHuge;
  int i = 0;
  while (!(wn[i] >= -Kokkos::Experimental::infinity_v<Real>)) {
    if (++i == nk) return Kokkos::Experimental::quiet_NaN_v<Real>;
  }
  Real lim = wn[i];
  for (++i; i < nk; ++i) lim = wn[i] > lim ? wn[i] : lim;
  return lim;
}

}  // namespace biinterpwt_detail

/// BiInterpWT(nk, nth, wn, th), :1483-1618, with qi_bound = 1 (a PARAMETER, :184): the
/// qi_bound = 0 branches are dead code in the Fortran and are not translated.
/// wn[nk] (base 1, qr_wn1 from the only caller, PrepKernelIO :1317), th[nth] (base 1).
/// Module state: qi_nnz (INTEGER(8), :209), qr_k4x[qi_nnz], qr_k4y[qi_nnz] (base 1).
/// Out: qi_bind(4, qi_nnz), qr_bwgh(4, qi_nnz), column-major, quartet iq (1-based) at
/// [4*(iq-1) .. 4*(iq-1)+3]; qr_bdry[qi_nnz]. The three are allocated by PrepKernelIO
/// (:1312-1316) with qi_nnz, which is the extent the whole-array initialisation covers.
/// qi_bind holds Fortran (1-based) spectral indices jth + (jk-1)*nth. Runs on one thread.
KOKKOS_INLINE_FUNCTION void biinterpwt(int nk, int nth, const Real* wn, const Real* th,
                                       std::int64_t qi_nnz, const Real* qr_k4x,
                                       const Real* qr_k4y, int* qi_bind, Real* qr_bwgh,
                                       Real* qr_bdry) {
  using namespace biinterpwt_detail;
  const Real one = static_cast<Real>(1.);
  // :1511-1513.
  for (std::int64_t j = 0; j < 4 * qi_nnz; ++j) qi_bind[j] = 0;
  for (std::int64_t j = 0; j < 4 * qi_nnz; ++j) qr_bwgh[j] = static_cast<Real>(0.);
  for (std::int64_t j = 0; j < qi_nnz; ++j) qr_bdry[j] = one;
  // :1522-1523.
  const Real kmin = minval(nk, wn);
  const Real kmax = maxval(nk, wn);
  // The iq loop runs to INT(qi_nnz, 4) in the object (:1531).
  const int nnz4 = static_cast<int>(qi_nnz);
  if (nnz4 <= 0) return;
  // :1527-1528.
  const Real dth = kQrTpi / static_cast<Real>(nth);
  const Real aRef = th[0];
  for (int iq = 1; iq <= nnz4; ++iq) {
    const Real kx = qr_k4x[iq - 1];
    const Real ky = qr_k4y[iq - 1];
    // :1532. **2. becomes x*x; the object adds ky^2 + kx^2 in that order.
    const Real k4R = Kokkos::sqrt(ky * ky + kx * kx);
    const Real angR = Kokkos::atan2(ky, kx);  // :1533, atan2f
    // :1536, MAX(kmin, MIN(k4R, kmax)) with the object's minss/maxss operand order: a NaN
    // k4R gives kmin.
    const Real m1 = kmax < k4R ? kmax : k4R;
    const Real k4T = m1 > kmin ? m1 : kmin;
    // :1557-1562.
    const Real r_jth = (angR - aRef) / dth + one;
    const int jth4T = ffloor(r_jth);
    const Real w_th4 = r_jth - static_cast<Real>(jth4T);
    const int jth4 = add_w(add_w(jth4T, nth), -1) % nth + 1;
    const int jth4p = add_w(jth4T, nth) % nth + 1;
    // :1572-1576.
    int jkU = 1;
    while (k4T > wn[jkU - 1]) {
      jkU = jkU + 1;
      if (jkU > nk) break;
    }
    // :1578-1589.
    Real r_jk;
    if (jkU == 1) {
      r_jk = one;
    } else {
      const Real delK = wn[jkU - 1] - wn[jkU - 2];
      r_jk = (static_cast<Real>(jkU) - one) + (k4T - wn[jkU - 2]) / delK;
    }
    const int jk4 = ffloor(r_jk);
    const Real w_k4 = r_jk - static_cast<Real>(jk4);
    const int jk4p = add_w(jk4, 1) < nk ? add_w(jk4, 1) : nk;
    // :1590-1593.
    int* bind = qi_bind + 4 * static_cast<std::int64_t>(iq - 1);
    bind[0] = add_w(jth4, mul_w(add_w(jk4, -1), nth));
    bind[1] = add_w(jth4, mul_w(add_w(jk4p, -1), nth));
    bind[2] = add_w(jth4p, mul_w(add_w(jk4p, -1), nth));
    bind[3] = add_w(jth4p, mul_w(add_w(jk4, -1), nth));
    // :1596-1599. With both factors NaN, the first operand's payload wins: g++ puts
    // (1-w_th4) first in [0] and w_th4 first in [2], the Fortran object the other factor ⚠
    // (port report); [1] and [3] match.
    Real* bwgh = qr_bwgh + 4 * static_cast<std::int64_t>(iq - 1);
    bwgh[0] = (one - w_k4) * (one - w_th4);
    bwgh[1] = (one - w_th4) * w_k4;
    bwgh[2] = w_k4 * w_th4;
    bwgh[3] = w_th4 * (one - w_k4);
    // :1608-1615.
    if (k4R < kmin) {
      qr_bdry[iq - 1] = static_cast<Real>(0.);
    } else if (k4R > kmax) {
      qr_bdry[iq - 1] = Kokkos::pow(k4R / kmax, kQrKpow);  // powf
    }
  }
}

}  // namespace ww::gke
