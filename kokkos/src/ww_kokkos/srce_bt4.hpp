// kokkos/src/ww_kokkos/srce_bt4.hpp
// W3SBT4: SHOWEX bottom friction with movable-bed roughness and sub-grid depth, one
// spectral point; INSBT4 and TABU_ERF, the host build of its ERF table. Phase 1,
// translated, not regtested. WW3 heritage: WW3-noaa/model/src/w3sbt4md.F90 at
// NOAA-EMC/WW3 03adda91 (INSBT4 :145-231, TABU_ERF :242-312, W3SBT4 :340-636); authors
// F. Ardhuin, J. Lepesqueur. Evidence: kokkos/port_report/W3SBT4_KOKKOS.md,
// INSBT4_KOKKOS.md, TABU_ERF_KOKKOS.md.
// The caller builds the table with insbt4() once per process (the FIRST flag, :465-468)
// and reads SBTCX, DELAB and FWTABLE of the current grid at call time (w3iogrmd.F90:1265).
// ⚠ On a device sinhf, log10f, powf, expf and sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "st4_params.hpp"
#include "real.hpp"

namespace ww::srce {

/// SIZEERFTABLE (w3sbt4md.F90:130), SIZEFWTABLE (constants.F90:92): tables are (0:300).
inline constexpr int kSizeErfTable = 300;
inline constexpr int kSizeFwTableBt4 = 300;

namespace bt4_detail {
// minss/maxss of w3sbt4md.o, d the destination: a NaN or a tie gives s. Three sites put
// the constant first (:474, :527, :610).
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
KOKKOS_INLINE_FUNCTION int imin(int a, int b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION int imax(int a, int b) { return (a > b) ? a : b; }

inline constexpr Real kGrav = static_cast<Real>(9.806);    // GRAV constants.F90:62
inline constexpr Real kXerfMax = static_cast<Real>(4.);    // XERFMAX :133
// Folded by gfortran (values in .rodata.cst4 of w3sbt4md.o):
inline constexpr Real k2GSed = 0x1.02e0ep+5f;      // 2.*GRAV*(SED_SG-1)  :544, 0x42017070
inline constexpr Real kSedG = 0x1.02e0ep+4f;       // (SED_SG-1)*GRAV :564,:594, 0x41817070
inline constexpr Real kSqrtTpi = 0x1.40d932p+1f;   // SQRT(TPI)           :590, 0x40206c99
inline constexpr Real kDxsub = 0x1.0624dep-9f;     // XSUB(2)-XSUB(1)     :551, 0x3b03126f
static_assert(k2GSed == (static_cast<Real>(2.) * kGrav) * (static_cast<Real>(2.65) - 1));
static_assert(kSedG == (static_cast<Real>(2.65) - 1) * kGrav);
static_assert(kDxsub == static_cast<Real>(0.001) - static_cast<Real>(-0.001));
}  // namespace bt4_detail

/// TABU_ERF (w3sbt4md.F90:242-312), host. erftable[i] = ERFTABLE(i), i = 0..300;
/// delxerf receives DELXERF.
inline void tabu_erf(Real* erftable, Real& delxerf) {
  const Real dx = (static_cast<Real>(2) * bt4_detail::kXerfMax) /
                  static_cast<Real>(kSizeErfTable);  // :300
  delxerf = dx;
  for (int i = 0; i <= kSizeErfTable; ++i) {
    const Real x = -static_cast<Real>(1.) * bt4_detail::kXerfMax + static_cast<Real>(i) * dx;
    const Real e = std::erf(x);  // erff
    Real y;
    if (x < static_cast<Real>(0.)) {
      y = (static_cast<Real>(1) - std::fabs(e)) * static_cast<Real>(0.5);  // :304
    } else {
      y = (static_cast<Real>(1) + e) * static_cast<Real>(0.5);  // :306
    }
    erftable[i] = y;
  }
}

/// INSBT4 (w3sbt4md.F90:145-231), host: tabulates ERF.
inline void insbt4(Real* erftable, Real& delxerf) { tabu_erf(erftable, delxerf); }

/// W3SBT4's grid scalars and the module tables it reads.
struct Bt4Params {
  int nk, nth, nspec;  ///< W3GDATMD NK, NTH, NSPEC
  Real sbtcx[10];      ///< W3GDATMD SBTCX(1:10): sbtcx[i-1]
  Real delab;          ///< CONSTANTS DELAB (set by TABU_FW)
  Real delxerf;        ///< W3SBT4MD DELXERF (set by tabu_erf)
};

/// W3SBT4 for one point, arrays 0-based. ecos, esin = ECOS, ESIN(1:NSPEC), read at ITH
/// (:503-504) and at IS (:626-627); taubbl[2] out, bedform[3] written. cbeta (NK) is work
/// for the local CBETA; DSUM is a scalar here.
KOKKOS_INLINE_FUNCTION void w3sbt4_point(const Bt4Params& p, const Real* sig, const Real* dden,
                                         const Real* ecos, const Real* esin,
                                         const Real* fwtable, const Real* erftable,
                                         const Real* a, const Real* cg, const Real* wn,
                                         Real depth, Real d50, Real psic, Real* taubbl,
                                         Real* bedform, Real* s, Real* d, Real* cbeta) {
  using bt4_detail::imax;
  using bt4_detail::imin;
  using bt4_detail::kGrav;
  using bt4_detail::maxss;
  using bt4_detail::minss;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real* sbtcx = p.sbtcx;
  const Real one = static_cast<Real>(1.);
  const Real xsub[3] = {static_cast<Real>(-0.001), static_cast<Real>(0.001),
                        static_cast<Real>(0.)};  // :452

  // :473-478
  Real dd50 = maxss(d50, static_cast<Real>(1E-5));
  dd50 = minss(one, dd50);
  const Real background = maxss(sbtcx[6] * dd50, sbtcx[5]);

  taubbl[0] = 0;  // :483
  taubbl[1] = 0;

  Real shields[3];
  Real uorb = 0, uorb2 = 0, aorb = 0, lx = 0, ly = 0;
  for (int isub = 1; isub <= 3; ++isub) {
    const Real dsub = depth * (xsub[isub - 1] + one);  // :489
    uorb = 0;
    aorb = 0;
    Real ax = 0;
    Real ay = 0;
    for (int ik = 1; ik <= nk; ++ik) {
      const Real kd = wn[ik - 1] * dsub;
      if (kd < static_cast<Real>(6.)) {  // :496
        Real eb = 0, ebx = 0, eby = 0;
        for (int ith = 1; ith <= nth; ++ith) {
          const int is = ith + (ik - 1) * nth;
          eb = eb + a[is - 1];
          ebx = ebx + a[is - 1] * ecos[ith - 1];
          eby = eby + a[is - 1] * esin[ith - 1];
        }
        const Real sig2 = sig[ik - 1] * sig[ik - 1];
        const Real sh = Kokkos::sinh(kd);
        cbeta[ik - 1] = static_cast<Real>(0.5) * sig2 / (kGrav * (sh * sh));  // :511
        const Real factor = (dden[ik - 1] / cg[ik - 1]) * 2 * cbeta[ik - 1] * kGrav;  // :513
        const Real varu = eb * factor;
        uorb = uorb + varu;
        aorb = aorb + varu / sig2;
        ax = ax + (ebx * factor);
        ay = ay + (eby * factor);
      } else {
        cbeta[ik - 1] = 0.;  // :520
      }
    }
    uorb2 = 2 * uorb;                                                     // :526
    uorb = Kokkos::sqrt(maxss(static_cast<Real>(1.0E-7), uorb2));         // :527
    aorb = Kokkos::sqrt(maxss(2 * aorb, static_cast<Real>(1.0E-7)));      // :528
    const Real den = Kokkos::sqrt(ax * ax + ay * ay + static_cast<Real>(1E-12));
    lx = aorb * static_cast<Real>(1.7) * ax / den;  // :533
    ly = aorb * static_cast<Real>(1.7) * ay / den;  // :534

    // :538-542, first use of FWTABLE
    const Real xi = maxss((Kokkos::log10(maxss(aorb / dd50, static_cast<Real>(0.3))) -
                           static_cast<Real>(-1.)) / p.delab,
                          one);
    const int ind = imin(kSizeFwTableBt4 - 1, ww::st4::fint(xi));
    const Real deli1 = minss(xi - static_cast<Real>(ind), one);
    const Real deli2 = one - deli1;
    const Real fw = fwtable[ind] * deli2 + fwtable[ind + 1] * deli1;

    const Real psi = fw * uorb2 / (bt4_detail::k2GSed * dd50);  // :544
    shields[isub - 1] = psi / psic;                             // :548
  }
  const Real dpsi = (shields[1] - shields[0]) / bt4_detail::kDxsub * sbtcx[4];  // :551

  Real ksubn;
  const Real adpsi = Kokkos::fabs(dpsi);
  if (adpsi < static_cast<Real>(0.0001) * shields[2] || adpsi < static_cast<Real>(1.E-8)) {
    // :559-575, no subgrid
    if (shields[2] > sbtcx[2]) {
      const Real ksubr = aorb * sbtcx[0] * Kokkos::pow(shields[2], sbtcx[1]);
      const Real ksubs = aorb * static_cast<Real>(0.0655) *
                         Kokkos::pow(uorb2 / (bt4_detail::kSedG * aorb), static_cast<Real>(1.4));
      ksubn = ksubr + ksubs;
      bedform[1] = lx;
      bedform[2] = ly;
    } else {
      ksubn = maxss(aorb * sbtcx[3], background);  // :570
      bedform[1] = -lx;
      bedform[2] = -ly;
    }
    bedform[0] = ksubn;
  } else {
    // :581-602, subgrid
    const Real psix = (sbtcx[2] - shields[2]) / dpsi;
    const Real psixt = maxss((psix + bt4_detail::kXerfMax) / p.delxerf, static_cast<Real>(0.));
    // :584. fint is cvttss2si (INT_MIN for NaN): the MAX(...,0) is kept, a cast would be UB.
    const int inde = imax(imin(kSizeErfTable - 1, ww::st4::fint(psixt)), 0);
    const Real deli1 = minss(psixt - static_cast<Real>(inde), one);
    const Real deli2 = one - deli1;
    const Real proba2 =
        maxss(minss(erftable[inde] * deli2 + erftable[inde + 1] * deli1, one), static_cast<Real>(0.));
    const Real proba1 = one - proba2;
    const Real psin2 =
        maxss(shields[2] + Kokkos::exp(-(static_cast<Real>(0.5) * (psix * psix))) /
                               bt4_detail::kSqrtTpi * dpsi / (proba2 + static_cast<Real>(0.0001)),
              sbtcx[2]);  // :590
    ksubn = proba1 * maxss(aorb * sbtcx[3], background) +
            proba2 * aorb *
                (sbtcx[0] * Kokkos::pow(psin2, sbtcx[1]) +
                 static_cast<Real>(0.0655) *
                     Kokkos::pow(uorb2 / (bt4_detail::kSedG * aorb), static_cast<Real>(1.4)));
    if (proba2 > static_cast<Real>(0.5)) {
      bedform[1] = lx;
      bedform[2] = ly;
    } else {
      bedform[1] = -lx;
      bedform[2] = -ly;
    }
  }
  bedform[0] = ksubn;  // :605

  // :610-614, second use of FWTABLE
  const Real xi = maxss((Kokkos::log10(maxss(static_cast<Real>(0.3), aorb / ksubn)) -
                         static_cast<Real>(-1.)) / p.delab,
                        one);
  const int ind = imin(kSizeFwTableBt4 - 1, ww::st4::fint(xi));
  const Real deli1 = minss(xi - static_cast<Real>(ind), one);
  const Real deli2 = one - deli1;
  const Real fw = fwtable[ind] * deli2 + fwtable[ind + 1] * deli1;

  // :618-630
  for (int ik = 1; ik <= nk; ++ik) {
    const Real const2 = dden[ik - 1] / cg[ik - 1] * kGrav / (sig[ik - 1] / wn[ik - 1]);
    const Real dsum = -(fw * uorb * cbeta[ik - 1]);
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      d[is - 1] = dsum;
      const Real temp2 = const2 * d[is - 1] * a[is - 1];
      taubbl[0] = taubbl[0] - temp2 * ecos[is - 1];
      taubbl[1] = taubbl[1] - temp2 * esin[is - 1];
      s[is - 1] = d[is - 1] * a[is - 1];
    }
  }
}

}  // namespace ww::srce
