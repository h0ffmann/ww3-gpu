// kokkos/src/ww_kokkos/iogo_skewness.hpp
// SKEWNESS: Srokosz's normalised skewness parameters of the sea surface at each sea point, and the
// electromagnetic and tracker bias coefficients built from them. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3iogomd.F90:4883-5054 at NOAA-EMC/WW3 03adda91; author P. Janssen.
// Evidence: kokkos/port_report/SKEWNESS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// The Fortran ALLOCATEs FAC0..FAC3 and fills them with CALL SECONDHH(NKHF,...) on every call
// (:4939-4944): here they are inputs, SECONDHH's output for NKHF = NK+13, computed by the caller.
// F2, SIGHF, DFIMHF and FAK are caller-provided work arrays (:4946-4947), one set per concurrent point.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::iogo {

namespace skewness_detail {
/// x**(0.5*n) for n = 0..3 as the -O3 object computes it: gfortran unrolls the I,J,K loops and folds
/// x**0. to 1. and x**1. to x; it calls powf only for 0.5 and 1.5. powf(x,1.) left to run time need
/// not return x's bits on every libm, so the folding is written out.
KOKKOS_INLINE_FUNCTION Real pow_half(Real x, int n) {
  if (n == 0) return Real(1.0);
  if (n == 2) return x;
  return Kokkos::pow(x, n == 1 ? Real(0.5) : Real(1.5));
}
}  // namespace skewness_detail

/// NKHF = NK+13 (:4937).
KOKKOS_INLINE_FUNCTION int skewness_nkhf(int nk) { return nk + 13; }

/// The body of the JSEA loop (:4957-5050) for one sea point.
///   a      A(1:NTH,1:NK,JSEA), Fortran order: a[(k-1) + (m-1)*nth].
///   cg     CG(0:NK+1,JSEA), base 0: cg[m] is CG(M,JSEA). The Fortran indexes CG by JSEA, not ISEA.
///   sig    SIG(0:NK+1), base 0. ecos, esin: ECOS(1:NTH), ESIN(1:NTH), base 1 stored at [0].
///   fac0..fac3  FAC*(1:NTH,1:NTH,1:NKHF,1:NKHF), Fortran order.
///   skew, embia1, embia2  SKEW(JSEA), EMBIA1(JSEA), EMBIA2(JSEA): written only when XMU(2,0,0) > 1.E-7.
///   f2     work, NTH*NKHF; sighf, dfimhf, fak  work, NKHF each.
KOKKOS_INLINE_FUNCTION void skewness_point(const Real* a, const Real* cg, int nk, int nth, const Real* sig,
                                           Real xfr, Real dth, const Real* ecos, const Real* esin,
                                           const Real* fac0, const Real* fac1, const Real* fac2,
                                           const Real* fac3, Real& skew, Real& embia1, Real& embia2,
                                           Real* f2, Real* sighf, Real* dfimhf, Real* fak) {
  using ww::srce::kGrav;
  using ww::srce::kTpiInv;
  const int nkhf = skewness_nkhf(nk);
  const int mstart = 1;
  const long nth_l = nth;
  const long nth2 = nth_l * nth_l;
  const long nth2nkhf = nth2 * nkhf;

  Real xmu[4][3][3];
  Real xlambda[4][3][3];
  for (int i = 0; i <= 3; ++i)
    for (int j = 0; j <= 2; ++j)
      for (int k = 0; k <= 2; ++k) xmu[i][j][k] = Real(0.0);

  for (int k = 1; k <= nth; ++k) {
    for (int m = 1; m <= nk; ++m) {
      const Real conx = kTpiInv / sig[m] * cg[m];
      f2[(k - 1) + (m - 1) * nth_l] = a[(k - 1) + (m - 1) * nth_l] / conx;
    }
  }

  sighf[0] = sig[1];
  for (int m = 2; m <= nkhf; ++m) sighf[m - 1] = xfr * sighf[m - 2];

  const Real co1 = Real(0.5) * (xfr - Real(1.)) * dth * kTpiInv;
  dfimhf[0] = co1 * sighf[0];
  for (int m = 2; m <= nkhf - 1; ++m) dfimhf[m - 1] = co1 * (sighf[m - 1] + sighf[m - 2]);
  dfimhf[nkhf - 1] = co1 * sighf[nkhf - 2];

  for (int m = 1; m <= nkhf; ++m) fak[m - 1] = (sighf[m - 1] * sighf[m - 1]) / kGrav;

  for (int m = nk + 1; m <= nkhf; ++m) {
    // x**5 as gfortran -O3 expands it (:4984): x2 = x*x, x3 = x*x2, x5 = x2*x3.
    const Real x = sighf[nk - 1] / sighf[m - 1];
    const Real x2 = x * x;
    const Real fh = x2 * (x * x2);
    for (int k = 1; k <= nth; ++k) f2[(k - 1) + (m - 1) * nth_l] = f2[(k - 1) + (nk - 1) * nth_l] * fh;
  }

  // In-order sums: gfortran vectorises the K2 loop with a fold-left reduction that keeps this order.
  for (int m1 = mstart; m1 <= nkhf; ++m1) {
    for (int m2 = mstart; m2 <= nkhf; ++m2) {
      for (int k1 = 1; k1 <= nth; ++k1) {
        for (int k2 = 1; k2 <= nth; ++k2) {
          const Real delf = dfimhf[m1 - 1] * dfimhf[m2 - 1] * f2[(k1 - 1) + (m1 - 1) * nth_l] *
                            f2[(k2 - 1) + (m2 - 1) * nth_l];
          const long idx = (k1 - 1) + (k2 - 1) * nth_l + (m1 - 1) * nth2 + (m2 - 1) * nth2nkhf;
          xmu[3][0][0] = xmu[3][0][0] + Real(3.0) * fac0[idx] * delf;
          xmu[1][2][0] = xmu[1][2][0] + fac1[idx] * delf;
          xmu[1][0][2] = xmu[1][0][2] + fac2[idx] * delf;
          xmu[1][1][1] = xmu[1][1][1] + fac3[idx] * delf;
        }
      }
    }
  }

  for (int k1 = 1; k1 <= nth; ++k1) {
    for (int m1 = mstart; m1 <= nkhf; ++m1) {
      const Real xk1 = fak[m1 - 1] * fak[m1 - 1];
      const Real delf = dfimhf[m1 - 1] * f2[(k1 - 1) + (m1 - 1) * nth_l];
      const Real ec = ecos[k1 - 1];
      const Real es = esin[k1 - 1];
      xmu[2][0][0] = xmu[2][0][0] + delf;
      xmu[0][2][0] = xmu[0][2][0] + xk1 * (ec * ec) * delf;
      xmu[0][0][2] = xmu[0][0][2] + xk1 * (es * es) * delf;
      xmu[0][1][1] = xmu[0][1][1] + xk1 * ec * es * delf;
    }
  }

  // XN = XMU(2,0,0)**XPI*XMU(0,2,0)**XPJ*XMU(0,0,2)**XPK with XP* = 0.5*FLOAT(n) (:5027-5033).
  for (int i = 0; i <= 3; ++i) {
    for (int j = 0; j <= 2; ++j) {
      for (int k = 0; k <= 2; ++k) {
        const Real xn = skewness_detail::pow_half(xmu[2][0][0], i) * skewness_detail::pow_half(xmu[0][2][0], j) *
                        skewness_detail::pow_half(xmu[0][0][2], k);
        if (xn != Real(0)) {
          xlambda[i][j][k] = xmu[i][j][k] / xn;
        } else {
          xlambda[i][j][k] = Real(0);
        }
      }
    }
  }
  if (xmu[2][0][0] > 1.E-7f) {
    skew = xlambda[3][0][0];
    const Real delta = (xlambda[1][2][0] + xlambda[1][0][2] - Real(2.0) * xlambda[0][1][1] * xlambda[1][1][1]) /
                       (Real(1.0) - xlambda[0][1][1] * xlambda[0][1][1]);
    embia1 = -(Real(0.125) * delta);
    embia2 = -(Real(0.125) * xlambda[3][0][0] / Real(3.0));
  }
}

/// SKEWNESS(A) over JSEA = 1..NSEAL (:4957), serially as without W3_OMPG.
///   a       A(1:NTH,1:NK,0:NSEAL), Fortran order, base 0 at A(1,1,0).
///   cg      CG(0:NK+1,0:NSEA), Fortran order, base 0 at CG(0,0).
///   skew, embia1, embia2  SKEW, EMBIA1, EMBIA2 (1:NSEALM), base 1 stored at [0].
inline void skewness(const Real* a, int nk, int nth, int nseal, const Real* sig, Real xfr, Real dth,
                     const Real* ecos, const Real* esin, const Real* cg, Real* skew, Real* embia1,
                     Real* embia2, const Real* fac0, const Real* fac1, const Real* fac2, const Real* fac3,
                     Real* f2, Real* sighf, Real* dfimhf, Real* fak) {
  const long aspec = static_cast<long>(nth) * nk;
  const long cgcol = static_cast<long>(nk) + 2;
  for (int jsea = 1; jsea <= nseal; ++jsea) {
    skewness_point(a + jsea * aspec, cg + jsea * cgcol, nk, nth, sig, xfr, dth, ecos, esin, fac0, fac1, fac2,
                   fac3, skew[jsea - 1], embia1[jsea - 1], embia2[jsea - 1], f2, sighf, dfimhf, fak);
  }
}

}  // namespace ww::iogo
