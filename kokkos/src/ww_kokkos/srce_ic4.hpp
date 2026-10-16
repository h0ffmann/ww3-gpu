// kokkos/src/ww_kokkos/srce_ic4.hpp
// W3SIC4: wave attenuation by sea ice from empirical and parametric decay rates k_i(f),
// every IC4METHOD (1-10, default), one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic4md.F90:121-803 at NOAA-EMC/WW3 03adda91;
// authors C. Collins, E. Rogers (method 10 E. Thomas). Evidence:
// kokkos/port_report/W3SIC4_KOKKOS.md. The object vectorises four loops with libmvec
// (LIBMVEC SITE below), emulated by st4_libmvec.hpp. ⚠ On a device they and
// expf/powf/sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "st4_libmvec.hpp"

namespace ww::srce {

/// NIC4 and NIC42, w3gdatmd.F90:623 (INTEGER PARAMETERs).
inline constexpr int kNic4 = 16;
inline constexpr int kNic42 = 5;

/// W3SIC4's grid scalars and namelist arrays (W3GDATMD, w3gdatmd.F90:2330-2336).
struct Ic4Params {
  int ic4method;       ///< IC4PARS(1) (:530)
  Real ki[kNic4];      ///< IC4_KI(1:NIC4): ki[i-1]
  Real fc[kNic4];      ///< IC4_FC(1:NIC4)
  Real cn[kNic42];     ///< IC4_CN(1:NIC42)
  Real fmin;           ///< IC4_FMIN
  Real kibk;           ///< IC4_KIBK
};

/// The point's fields (:511-527): icecoef[j-1] = ICECOEFj, 0 when its INFLAGS2 flag is
/// false; inflags[0..4] = INFLAGS2(-7..-3).
struct Ic4Point {
  Real icecoef[7];
  bool inflags[5];
};

namespace ic4_detail {

/// Returned where the Fortran stops in EXTCDE(201) (methods 5, 6, :617-622, :639-644): the
/// kernel cannot stop the run, the shim must.
inline constexpr int kExtcde = 201;

// Inexact REAL literals are float literals: both round the decimal once to REAL.

// SIG at element i of SIG(0:NK+1); sig = SIG(1:NK).
KOKKOS_INLINE_FUNCTION Real sig_at(int i, int nk, const Real* sig, Real sig_0, Real sig_nkp1) {
  return (i == 0) ? sig_0 : ((i == nk + 1) ? sig_nkp1 : sig[i - 1]);
}

}  // namespace ic4_detail

/// W3SIC4 for one point, arrays 0-based, mapwn values 1-based IK. sig = SIG(1:NK); sig_0,
/// sig_nkp1 = SIG(0), SIG(NK+1), lanes of the :544 and :585 vector loops. wn_i (NK) is work
/// for WN_I, then D1D. Returns 0, or kExtcde with d = 0 (:472) and s not written.
KOKKOS_INLINE_FUNCTION int w3sic4_point(const Params& p, const Ic4Params& ic,
                                        const Ic4Point& pt, const Real* sig, Real sig_0,
                                        Real sig_nkp1, const Real* dden, const int* mapwn,
                                        const Real* a, const Real* cg, Real* s, Real* d,
                                        Real* wn_i) {
  using ic4_detail::sig_at;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real half = static_cast<Real>(0.5);

  // :472-499. ALPHA, MARG, KARG are not kept: each WN_I is finished where computed.
  for (int is = 0; is < nspec; ++is) d[is] = zero;
  for (int ik = 0; ik < nk; ++ik) wn_i[ik] = zero;

  const Real icecoef1 = pt.icecoef[0];
  const Real icecoef2 = pt.icecoef[1];
  const Real icecoef3 = pt.icecoef[2];
  const Real icecoef4 = pt.icecoef[3];
  const Real icecoef5 = pt.icecoef[4];
  const Real icecoef6 = pt.icecoef[5];
  const Real icecoef7 = pt.icecoef[6];

  switch (ic.ic4method) {
    case 1: {
      // :544-545. LIBMVEC SITE: _ZGVbN4v_expf over ALPHA(0:NK+1), lane l = element l.
      const Real c1tpi = icecoef1 * kTpi;
      ww::st4::mvec_expf(
          nk + 2,
          [&](int i) { return -(c1tpi / sig_at(i, nk, sig, sig_0, sig_nkp1)) - icecoef2; },
          [&](int i, Real v) {
            if (i >= 1 && i <= nk) wn_i[i - 1] = half * v;
          });
      break;
    }
    case 2: {
      // :552-578.
      bool nml_input = true;
      if (pt.inflags[0] || pt.inflags[1] || pt.inflags[2] || pt.inflags[3] || pt.inflags[4])
        nml_input = false;
      Real cice1, cice2, cice3, cice4, cice5;
      if (nml_input) {
        cice1 = ic.cn[0];
        cice2 = ic.cn[1];
        cice3 = ic.cn[2];
        cice4 = ic.cn[3];
        cice5 = ic.cn[4];
      } else {
        cice1 = icecoef1;
        cice2 = icecoef2;
        cice3 = icecoef3;
        cice4 = icecoef4;
        cice5 = icecoef5;
      }
      for (int ik = 1; ik <= nk; ++ik) {
        const Real f = sig[ik - 1] / kTpi;
        const Real f2 = f * f;
        const Real marg1 = cice1 + cice2 * f + cice3 * f2;
        const Real marg2 = cice4 * (f2 * f) + cice5 * (f2 * f2);
        const Real alpha = marg1 + marg2;
        wn_i[ik - 1] = half * alpha;
      }
      break;
    }
    case 3: {
      // :581. maxss: HICE = (0 > ICECOEF1) ? 0 : ICECOEF1 (NaN stays NaN).
      const Real hice = (zero > icecoef1) ? zero : icecoef1;
      // :582-586. LIBMVEC SITE: _ZGVbN4v_expf over ALPHA(0:NK+1), lane l = element l.
      ww::st4::mvec_expf(
          nk + 2,
          [&](int i) {
            const Real t = kTpi / sig_at(i, nk, sig, sig_0, sig_nkp1);
            const Real karg1 =
                -0.3203f + 2.058f * hice - 0.9375f * t;
            const Real karg2 = -(0.4269f * (hice * hice)) + 0.1566f * hice * t;
            const Real karg3 = 0.0006f * (t * t);
            return karg1 + karg2 + karg3;
          },
          [&](int i, Real v) {
            if (i >= 1 && i <= nk) wn_i[i - 1] = half * v;
          });
      break;
    }
    case 4: {
      // :590-600. The two IK loops fused; EB and the EMEAN sum keep the Fortran's order.
      Real emean = zero;
      for (int ik = 1; ik <= nk; ++ik) {
        Real eb = zero;
        for (int ith = 1; ith <= nth; ++ith) eb = eb + a[ith + (ik - 1) * nth - 1];
        eb = eb * dden[ik - 1] / cg[ik - 1];
        emean = emean + eb;
      }
      // maxss: (EMEAN > 0) ? EMEAN : 0 (NaN gives 0).
      const Real hs = static_cast<Real>(4.) * Kokkos::sqrt((emean > zero) ? emean : zero);
      // :602-606. The INTEGER 3 compared as REAL 3. A NaN HS takes neither branch.
      if (hs <= static_cast<Real>(3.)) {
        for (int ik = 0; ik < nk; ++ik) wn_i[ik] = icecoef1;
      } else if (hs > static_cast<Real>(3.)) {
        const Real w = icecoef2 / hs;
        for (int ik = 0; ik < nk; ++ik) wn_i[ik] = w;
      }
      break;
    }
    case 5: {
      // :610-634.
      const Real ki1 = icecoef1, ki2 = icecoef2, ki3 = icecoef3, ki4 = icecoef4;
      const Real fc5 = icecoef5, fc6 = icecoef6, fc7 = icecoef7;
      if ((ki1 == zero) || (ki2 == zero) || (ki3 == zero) || (ki4 == zero) ||
          (fc5 == zero) || (fc6 == zero) || (fc7 == zero))
        return ic4_detail::kExtcde;
      for (int ik = 1; ik <= nk; ++ik) {
        const Real f = sig[ik - 1] / kTpi;
        if (f < fc5) {
          wn_i[ik - 1] = ki1;
        } else if (f < fc6) {
          wn_i[ik - 1] = ki2;
        } else if (f < fc7) {
          wn_i[ik - 1] = ki3;
        } else {
          wn_i[ik - 1] = ki4;
        }
      }
      break;
    }
    case 6: {
      // :639-654. No step matched (or a NaN FREQ) leaves WN_I = 0.
      if ((ic.ki[0] == zero) || (ic.ki[1] == zero) || (ic.ki[2] == zero) ||
          (ic.fc[0] == zero) || (ic.fc[1] == zero))
        return ic4_detail::kExtcde;
      for (int ik = 1; ik <= nk; ++ik) {
        const Real f = sig[ik - 1] / kTpi;
        for (int ifc = 1; ifc <= kNic4; ++ifc) {
          if (f < ic.fc[ifc - 1]) {
            wn_i[ik - 1] = ic.ki[ifc - 1];
            break;
          }
        }
      }
      break;
    }
    case 7: {
      // :658. maxss: HICE = (ICECOEF1 > 0) ? ICECOEF1 : 0 (NaN gives 0).
      const Real hice = (icecoef1 > zero) ? icecoef1 : zero;
      // :659-662. LIBMVEC SITE: _ZGVbN4vv_powf over IK = 1..NK, lane l = IK-1.
      ww::st4::mvec_powf(
          nk, [&](int l) { return sig[l] / kTpi; }, [&](int) { return 2.13f; },
          [&](int l, Real v) {
            const Real alpha = 0.2f * v * hice;
            wn_i[l] = half * alpha;
          });
      break;
    }
    case 8: {
      // :666-684.
      const Real chf = pt.inflags[1] ? icecoef2 : ic.cn[0];
      // :680. maxss: HICE = (ICECOEF1 > 0) ? ICECOEF1 : 0 (NaN gives 0).
      const Real hice = (icecoef1 > zero) ? icecoef1 : zero;
      for (int ik = 1; ik <= nk; ++ik) {
        const Real f = sig[ik - 1] / kTpi;
        wn_i[ik - 1] = chf * hice * (f * f * f);
      }
      break;
    }
    case 9: {
      // :688-709.
      const bool nml_input = !(pt.inflags[1] || pt.inflags[2]);
      const Real chf = nml_input ? ic.cn[0] : icecoef2;
      const Real npow = nml_input ? ic.cn[1] : icecoef3;
      // :704. maxss: HICE = (0 > ICECOEF1) ? 0 : ICECOEF1 (NaN stays NaN).
      const Real hice = (zero > icecoef1) ? zero : icecoef1;
      const Real mpow = half * npow - static_cast<Real>(1.0);
      // -O3 computes CHF*HICE**MPOW once, before the loop (the GIMPLE's _717).
      const Real chm = chf * Kokkos::pow(hice, mpow);
      // :707-709. LIBMVEC SITE: _ZGVbN4vv_powf over IK = 1..NK, lane l = IK-1.
      ww::st4::mvec_powf(
          nk, [&](int l) { return sig[l] / kTpi; }, [&](int) { return npow; },
          [&](int l, Real v) { wn_i[l] = chm * v; });
      break;
    }
    case 10: {
      // :716-719. minss/maxss operand order from the object:
      // X3 = (3.5 < ICECOEF1) ? 3.5 : ICECOEF1, then (X3 > 0.1) ? X3 : 0.1;
      // X2 = (X2 < 100) ? X2 : 100, then (X2 > 2.5) ? X2 : 2.5. A NaN gives 0.1, 2.5.
      Real x3 = (3.5f < icecoef1) ? 3.5f : icecoef1;
      x3 = (x3 > 0.1f) ? x3 : 0.1f;
      Real x2 = icecoef5 * half;
      x2 = (x2 < 100.0f) ? x2 : 100.0f;
      x2 = (x2 > 2.5f) ? x2 : 2.5f;
      const Real x2sqr = x2 * x2;
      const Real x3sqr = x3 * x3;
      const Real amhb = 2.12e-3f;
      const Real bmhb = 4.59e-2f;
      for (int ik = 1; ik <= nk; ++ik) {
        const Real x1 = kTpi / sig[ik - 1];
        const Real x1sqr = x1 * x1;
        // :728-736, left to right as written.
        Real karg1 = -0.26982f + 1.5043f * x3 - 0.70112f * x3sqr + 0.011037f * x2 +
                     (-0.0073178f) * x2 * x3 + 0.00036604f * x2 * x3sqr +
                     (-0.00045789f) * x2sqr + 1.8034e-05f * x2sqr * x3 +
                     (-0.7246f) * x1 + 0.12068f * x1 * x3 +
                     (-0.0051311f) * x1 * x3sqr + 0.0059241f * x1 * x2 +
                     0.00010771f * x1 * x2 * x3 - 1.0171e-05f * x1 * x2sqr +
                     0.0035412f * x1sqr - 0.0031893f * x1sqr * x3 +
                     (-0.00010791f) * x1sqr * x2 +
                     0.00031073f * (x1 * x1 * x1) + 1.5996e-06f * (x2 * x2 * x2) +
                     0.090994f * (x3 * x3 * x3);
        // :737. minss: (K < 0) ? K : 0 (NaN gives 0).
        karg1 = (karg1 < zero) ? karg1 : zero;
        // :738. A scalar powf(10.,K).
        Real alpha = Kokkos::pow(static_cast<Real>(10.0), karg1);
        const Real perfour = x1sqr * x1sqr;
        if ((x1 > static_cast<Real>(5.0)) && (x1 < static_cast<Real>(20.0))) {
          alpha = alpha + amhb / x1sqr + bmhb / perfour;
        } else if (x1 > static_cast<Real>(20.0)) {
          alpha = amhb / x1sqr + bmhb / perfour;
        }
        wn_i[ik - 1] = alpha * half;
      }
      break;
    }
    default:
      // :749.
      for (int ik = 0; ik < nk; ++ik) wn_i[ik] = icecoef1;
      break;
  }

  // :756-763. wn_i becomes D1D.
  for (int ik = 1; ik <= nk; ++ik) {
    if (sig[ik - 1] / kTpi < ic.fmin) wn_i[ik - 1] = ic.kibk;
    wn_i[ik - 1] = -(static_cast<Real>(2.) * cg[ik - 1] * wn_i[ik - 1]);
  }

  // :768-774. MAPWN holds 1-based IK.
  for (int is = 0; is < nspec; ++is) d[is] = wn_i[mapwn[is] - 1];
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
  return 0;
}

}  // namespace ww::srce
