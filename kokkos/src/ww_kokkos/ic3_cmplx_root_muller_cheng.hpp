// kokkos/src/ww_kokkos/ic3_cmplx_root_muller_cheng.hpp
// CMPLX_ROOT_MULLER_CHENG: Muller's method for the complex root of F_ZHAO_CHENG (IC3 "Cheng"),
// the complex wavenumber of a wave in a viscoelastic ice layer.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:2540-2710 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger, X. Zhao, S. Cheng (from Zhao and Shen's Clarkson code).
// Evidence: kokkos/port_report/CMPLX_ROOT_MULLER_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_bsdet.hpp"
#include "ic3_drfun_dble_cheng.hpp"
#include "ic3_drfun_quad_cheng.hpp"
#include "ic3_f_zhao_cheng.hpp"
#include "ic3_func1_zhao.hpp"

namespace ww::ic3 {

/// Callable wrappers so the Cheng DRFUN functions can be passed as template arguments to
/// f_zhao_cheng and cmplx_root_muller_cheng; a function pointer is not usable in a kernel.
struct DrfunDbleCheng {
  KOKKOS_INLINE_FUNCTION Kokkos::complex<double> operator()(const Kokkos::complex<double>& wn,
                                                            double sigma, double es, double nu,
                                                            double dice, double hice, double depth,
                                                            int judge) const {
    return drfun_dble_cheng(wn, sigma, es, nu, dice, hice, depth, judge);
  }
};

struct DrfunQuadCheng {
  KOKKOS_INLINE_FUNCTION Kokkos::complex<double> operator()(const Kokkos::complex<double>& wn,
                                                            double sigma, double es, double nu,
                                                            double dice, double hice,
                                                            double depth) const {
    return drfun_quad_cheng(wn, sigma, es, nu, dice, hice, depth);
  }
};

/// CMPLX_ROOT_MULLER_CHENG(X0, X1, X2, JUDGE, SIGMA, ES, NU, DICE, HICE, DEPTH): DOUBLE COMPLEX
/// X0..X2, INTEGER JUDGE, DOUBLE PRECISION the rest; the DOUBLE COMPLEX result is written to
/// `p3`. Every complex product and quotient is written out as gfortran -O3 emits it (no __muldc3,
/// Smith's quotient inlined); an INTEGER or REAL literal operand is promoted to (x, 0.0) and its
/// products with 0.0 are kept.
template <class DrfunDble, class DrfunQuad>
KOKKOS_INLINE_FUNCTION void cmplx_root_muller_cheng(const Kokkos::complex<double>& x0,
                                                    const Kokkos::complex<double>& x1,
                                                    const Kokkos::complex<double>& x2, int judge,
                                                    double sigma, double es, double nu, double dice,
                                                    double hice, double depth,
                                                    const DrfunDble& drfun_dble,
                                                    const DrfunQuad& drfun_quad,
                                                    Kokkos::complex<double>& p3) {
  namespace f = func1_zhao_detail;
  namespace d = drfun_quad_cheng_detail;
  using f::cplx;
  // EPSI = DLTA = 1.E-8, a default REAL literal stored in DOUBLE PRECISION (:2646-2647).
  constexpr double epsi = 0x1.5798eep-27;
  static_assert(epsi == static_cast<double>(1.e-8f));
  constexpr double dlta = epsi;
  constexpr int imax = 200;
  const cplx one(1.0, 0.0);
  const cplx two(2.0, 0.0);
  const cplx four(4.0, 0.0);
  const cplx im(0.0, 1.0);

  // :2641-2644.
  cplx p0 = x0;
  cplx p1 = x1;
  cplx p2 = x2;
  p3 = cplx(0.0, 0.0);
  cplx y0 = f_zhao_cheng(judge, p0, sigma, es, nu, dice, hice, depth, drfun_dble, drfun_quad);
  cplx y1 = f_zhao_cheng(judge, p1, sigma, es, nu, dice, hice, depth, drfun_dble, drfun_quad);
  cplx y2 = f_zhao_cheng(judge, p2, sigma, es, nu, dice, hice, depth, drfun_dble, drfun_quad);

  for (int i = 1; i <= imax; ++i) {
    // :2653-2657. Q**2. and (1 + Q)**2. are cpow(z, (2.0, 0.0)) in the object, not z*z.
    const cplx d21 = f::sub(p2, p1);
    const cplx q = f::div(d21, f::sub(p1, p0));
    const cplx q1 = f::add(one, q);
    const cplx q2y0 = f::mul(d::cpow(q, 2.0), y0);
    const cplx a = f::add(f::sub(f::mul(q, y2), f::mul(f::mul(q, q1), y1)), q2y0);
    const cplx b = f::add(
        f::sub(f::mul(f::add(f::mul(two, q), one), y2), f::mul(d::cpow(q1, 2.0), y1)), q2y0);
    const cplx c = f::mul(q1, y2);

    if (bsdet_cabs(a) != 0.0) {
      // :2661-2670. ABS(DEN1) .LT. ABS(DEN2) is false for a NaN: DEN1 is used.
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
      // :2674-2675.
      p3 = f::sub(p2, f::mul(d21, f::div(c, b)));
    } else {
      // :2676-2678.
      p3 = p2;
      return;
    }

    // :2682-2684. IMAG(P3) < 0: flip the sign of the imaginary part.
    if (p3.imag() < 0.0) {
      p3 = cplx(p3.real(), -(p3.imag()));
    }
    // :2685-2687. REAL(P3) < 0: negate the real part.
    if (p3.real() < 0.0) {
      p3 = cplx(-(p3.real()), p3.imag());
    }
    // :2689-2690. NU == 0: zero the imaginary part.
    if (nu == 0.0) {
      p3 = cplx(p3.real(), 0.0);
    }

    // :2692. Convergence test.
    const cplx y3 = f_zhao_cheng(judge, p3, sigma, es, nu, dice, hice, depth, drfun_dble, drfun_quad);
    if (bsdet_cabs(f::sub(p3, p2)) < dlta || bsdet_cabs(y3) < epsi) {
      return;
    }

    // :2698-2704.
    p0 = p1;
    p1 = p2;
    p2 = p3;
    y0 = y1;
    y1 = y2;
    y2 = y3;
  }
  // :2707-2708.
  p3 = cplx(-100.0, 0.0);
}

/// CMPLX_ROOT_MULLER_CHENG as the callable `root` that W3IC3WNCG_CHENG takes, usable on the host
/// and in a kernel. Defaults to the Cheng DRFUN callables; pass alternatives only in tests.
struct CmplxRootMullerCheng {
  KOKKOS_INLINE_FUNCTION void operator()(const Kokkos::complex<double>& x0,
                                         const Kokkos::complex<double>& x1,
                                         const Kokkos::complex<double>& x2, int judge, double sigma,
                                         double es, double nu, double dice, double hice,
                                         double depth, Kokkos::complex<double>& p3) const {
    cmplx_root_muller_cheng(x0, x1, x2, judge, sigma, es, nu, dice, hice, depth,
                            DrfunDbleCheng{}, DrfunQuadCheng{}, p3);
  }
};

}  // namespace ww::ic3
