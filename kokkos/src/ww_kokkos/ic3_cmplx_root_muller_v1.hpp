// kokkos/src/ww_kokkos/ic3_cmplx_root_muller_v1.hpp
// CMPLX_ROOT_MULLER_V1: Muller's method for the complex root of F_ZHAO_V1 (IC3 "V1"), the
// complex wavenumber of a wave in ice (JUDGE = 1) or in open water (JUDGE = 0).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1106-1276 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger (from Zhao and Shen's Clarkson code).
// Evidence: kokkos/port_report/CMPLX_ROOT_MULLER_V1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_bsdet.hpp"
#include "ic3_drfun_quad_cheng.hpp"
#include "ic3_f_zhao_v1.hpp"
#include "ic3_func1_zhao.hpp"

namespace ww::ic3 {

/// CMPLX_ROOT_MULLER_V1(X0, X1, X2, JUDGE, SIGMA, ES, NU, DICE, HICE, DEPTH): DOUBLE COMPLEX
/// X0..X2, INTEGER JUDGE, DOUBLE PRECISION the rest; the DOUBLE COMPLEX result is written to
/// `p3`. Returns 0, or 2 where the Fortran writes its diagnostic to NDSE and calls EXTCDE(2):
/// B == 0 with A == 0 (:1239-1244), and no convergence in IMAX = 1000 steps (:1264-1268). The
/// caller does the WRITE; on a nonzero return `p3` holds the last iterate. Every complex product
/// and quotient is written out as gfortran -O3 emits it (no __muldc3, Smith's quotient inlined);
/// an INTEGER or REAL literal operand is promoted to (x, 0.0) and its products with 0.0 are kept.
KOKKOS_INLINE_FUNCTION int cmplx_root_muller_v1(const Kokkos::complex<double>& x0,
                                                const Kokkos::complex<double>& x1,
                                                const Kokkos::complex<double>& x2, int judge,
                                                double sigma, double es, double nu, double dice,
                                                double hice, double depth,
                                                Kokkos::complex<double>& p3) {
  namespace f = func1_zhao_detail;
  namespace d = drfun_quad_cheng_detail;
  using f::cplx;
  // EPSI = DLTA = 1.E-5, a REAL literal stored in DOUBLE PRECISION (:1210-1211).
  constexpr double epsi = 0x1.4f8b58p-17;
  static_assert(epsi == static_cast<double>(1.e-5f));
  constexpr double dlta = epsi;
  constexpr int imax = 1000;
  const cplx one(1.0, 0.0);
  const cplx two(2.0, 0.0);
  const cplx four(4.0, 0.0);
  const Func1Zhao func1{};

  // :1204-1214.
  cplx p0 = x0;
  cplx p1 = x1;
  cplx p2 = x2;
  p3 = cplx(0.0, 0.0);
  cplx y0 = f_zhao_v1(p0, judge, sigma, es, nu, dice, hice, depth, func1);
  cplx y1 = f_zhao_v1(p1, judge, sigma, es, nu, dice, hice, depth, func1);
  cplx y2 = f_zhao_v1(p2, judge, sigma, es, nu, dice, hice, depth, func1);

  for (int i = 1; i <= imax; ++i) {
    // :1217-1220. Q**2. and (1 + Q)**2. are cpow(z, (2.0, 0.0)) in the object, not z*z.
    const cplx d21 = f::sub(p2, p1);
    const cplx q = f::div(d21, f::sub(p1, p0));
    const cplx q1 = f::add(one, q);
    const cplx q2y0 = f::mul(d::cpow(q, 2.0), y0);
    const cplx a = f::add(f::sub(f::mul(q, y2), f::mul(f::mul(q, q1), y1)), q2y0);
    const cplx b = f::add(
        f::sub(f::mul(f::add(f::mul(two, q), one), y2), f::mul(d::cpow(q1, 2.0), y1)), q2y0);
    const cplx c = f::mul(q1, y2);

    if (bsdet_cabs(a) != 0.0) {
      // :1224-1233. ABS(DEN1) .LT. ABS(DEN2) is false for a NaN: DEN1 is used.
      const cplx disc = f::sub(d::cpow(b, 2.0), f::mul(f::mul(four, a), c));
      const cplx sq = d::csqrt(disc.real(), disc.imag());
      const cplx den1 = f::add(b, sq);
      const cplx den2 = f::sub(b, sq);
      const cplx c2 = f::mul(two, c);
      if (bsdet_cabs(den1) < bsdet_cabs(den2)) {
        p3 = f::sub(p2, f::mul(d21, f::div(c2, den2)));
      } else {
        p3 = f::sub(p2, f::mul(d21, f::div(c2, den1)));
      }
    } else if (bsdet_cabs(b) != 0.0) {
      // :1237-1238.
      p3 = f::sub(p2, f::mul(d21, f::div(c, b)));
    } else {
      // :1239-1244.
      return 2;
    }

    // :1248-1252.
    const cplx y3 = f_zhao_v1(p3, judge, sigma, es, nu, dice, hice, depth, func1);
    if (bsdet_cabs(f::sub(p3, p2)) < dlta || bsdet_cabs(y3) < epsi) {
      return 0;
    }

    // :1254-1260.
    p0 = p1;
    p1 = p2;
    p2 = p3;
    y0 = y1;
    y1 = y2;
    y2 = y3;
  }
  // :1264-1268.
  return 2;
}

/// CMPLX_ROOT_MULLER_V1 as the callable `root` that wn_cmplx_hf (ic3_wn_cmplx_hf.hpp) takes,
/// usable on the host and in a kernel.
struct CmplxRootMullerV1 {
  KOKKOS_INLINE_FUNCTION int operator()(const Kokkos::complex<double>& x0,
                                        const Kokkos::complex<double>& x1,
                                        const Kokkos::complex<double>& x2, int judge, double sigma,
                                        double es, double nu, double dice, double hice,
                                        double depth, Kokkos::complex<double>& p3) const {
    return cmplx_root_muller_v1(x0, x1, x2, judge, sigma, es, nu, dice, hice, depth, p3);
  }
};

}  // namespace ww::ic3
