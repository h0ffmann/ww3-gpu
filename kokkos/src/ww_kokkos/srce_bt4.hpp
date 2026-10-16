// kokkos/src/ww_kokkos/srce_bt4.hpp
// W3SBT4, ported to Kokkos: SHOWEX bottom friction with movable-bed roughness
// (Ardhuin et al. 2003) and the sub-grid depth treatment of Tolman (1995), one
// spectral point; with INSBT4 and TABU_ERF, the host build of its ERF table.
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3sbt4md.F90 at NOAA-EMC/WW3 03adda91 (identical
// in WW3/ at c9a6a0c6): INSBT4 :145-231, TABU_ERF :242-312, W3SBT4 :340-636;
// authors F. Ardhuin, J. Lepesqueur. Switch BT4 (ww3_tp2.17's switch_PDLIB,
// switch_NO_PDLIB, switch_RWPS; ww3_tp2.14's OASIS switches; model/bin
// switch_Ifremer*, switch_ite_pdlib, switch_USACE_1, switch_ugdev2). Called by W3SRCE
// at w3srcemd.F90:1342-1343 with SPEC, CG1, WN1, and by the point post-processors
// ww3_outp.F90:2309, ww3_ounp.F90:2485 and gx_outp.F90:1252. The only #ifdef in the three
// routines is W3_S (STRACE), not ported; no switch changes their arithmetic.
// IX and IY (:338) are never read and are not passed. The Fortran's FIRST flag
// (:465-468) calls INSBT4 on the first call of the process: the caller builds the
// table with insbt4() before the kernel runs. ERFTABLE and DELXERF are module
// variables, one copy per process, not per grid. A shim selects SBTCX per grid
// (MPARS(IMOD)) and reads DELAB and FWTABLE at call time: each mod_def read
// rewrites those CONSTANTS module variables (w3iogrmd.F90:1265).
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3sbt4md.F90 (WW3's CMake Release flags, switch_lab_shrd with BT1 -> BT4):
//   :300   DELXERF = (2*XERFMAX)/300 folded to 0x3cda740e;
//   :302   x = REAL(I)*DELXERF - 4.;
//   :304-6 2**(1/2) is INTEGER 2**0 = 1, so y = (1 -|erff(x)|)*0.5 or
//          (1 + erff(x))*0.5; /2 became *0.5 (exact). erff is a libm call;
//   :489   DSUB = DEPTH*(XSUB(ISUB) + 1.);
//   :496   WN*DSUB < 6., the same product passed to sinhf at :511;
//   :511   (SIG*SIG*0.5) / (sinhf(WN*DSUB)**2 * GRAV), a true division;
//   :513   FACTOR = (((DDEN/CG)*2)*CBETA)*GRAV;
//   :516   AORB += VARU/(SIG*SIG);
//   :527-8 sqrtss (inline, no call); :533-4 ((AORB*1.7)*AX)/sqrtf((AX*AX+AY*AY)+1E-12);
//   :538,610 log10f, then (log + 1.)/DELAB (ABMIN = -1 folded);
//   :544   (FW*UORB2)/(DD50*32.3598) with 2*GRAV*(SED_SG-1) folded to 0x42017070;
//   :551   ((SHIELDS(2)-SHIELDS(1))/0.002)*SBTCX(5), XSUB(2)-XSUB(1) folded
//          to 0x3b03126f, a true division;
//   :562,593 powf(SHIELDS(3) or PSIN2, SBTCX(2)) with a REAL exponent;
//   :564,594 powf(UORB2/(AORB*16.1799), 1.4) with (SED_SG-1)*GRAV folded to
//          0x41817070; 1.4 is a REAL literal, so powf, not a power tree;
//   :590   expf(-((PSIX*PSIX)*0.5)) / SQRT(TPI) * DPSI / (PROBA2+0.0001), SQRT(TPI)
//          folded to 0x40206c99, both true divisions;
//   :619-20 CONST2 = ((DDEN/CG)*GRAV)/(SIG/WN);
//   :621   DSUM = -((FW*UORB)*CBETA); :625 TEMP2 = (CONST2*D)*A.
// No FMA, no reciprocal multiply, no libmvec: the only vectorisation is the pair
// TAUBBL(1:2) updated as one 8-byte vector (:626-627) and the BEDFORM store, both
// element-wise, so the scalar loop below gives the same bits. TAUBBL is summed in
// IS order, as the Fortran. MIN and MAX: each is written as the minss/maxss of
// w3sbt4md.o, minss(d, s) = d < s ? d : s and maxss(d, s) = d > s ? d : s, with
// d the destination register. Three sites put the constant first (:474, :527,
// :610), so a NaN there propagates where the other sites return the constant.
// Integer MIN/MAX (:539, :584, :611) are cmov and have no such choice. INT()
// at :539, :584, :611 is ww::st4::fint (st4_params.hpp:54), cvttss2si's INT_MIN
// for NaN or out of range, so the MAX(...,0) at :584 is kept: a C++ cast there
// is undefined and let g++ drop it.
//
// ⚠ On a device CUDA's sinhf, log10f, powf, expf, erff and sqrtf are not glibc's
// (AGENTS.md, "Bit for bit", GPU); the table builder runs on the host.
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "st4_params.hpp"
#include "real.hpp"

namespace ww::srce {

/// SIZEERFTABLE (w3sbt4md.F90:130) and SIZEFWTABLE (constants.F90:92): both tables
/// are (0:300).
inline constexpr int kSizeErfTable = 300;
inline constexpr int kSizeFwTableBt4 = 300;

namespace bt4_detail {
// The minss/maxss of w3sbt4md.o: d is the destination operand, returned when the
// comparison holds; s is returned on a NaN or a tie.
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

/// W3SBT4(A, CG, WN, DEPTH, D50, PSIC, TAUBBL, BEDFORM, S, D, IX, IY), one point.
/// sig, dden = SIG(1:NK), DDEN(1:NK); cg, wn = CG(1:NK), WN(1:NK); ecos, esin =
/// ECOS(1:NSPEC), ESIN(1:NSPEC) (:503-504 read them at ITH, :626-627 at IS);
/// a, s, d = A, S, D(1:NSPEC); fwtable = FWTABLE(0:300), erftable = ERFTABLE(0:300);
/// taubbl[2] out, bedform[3] in/out (only written). All 0-based. cbeta (NK) is
/// caller-provided work for the Fortran local CBETA (:441); DSUM (:449) is only
/// written and is a scalar here.
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
