// kokkos/src/ww_kokkos/prop_parall.hpp
// PROP_REFRACTION_PR3 and PROP_FREQ_SHIFT_M2 of W3PARALL: the per-point refraction velocity
// CAD and the frequency-shift velocity CWNB_M2 (with DWNI_M2) of the PDLIB implicit scheme.
// Phase 1, translated, not regtested. WW3 heritage: WW3-noaa/model/src/w3parall.F90:447-589,
// :754-894 at NOAA-EMC/WW3 03adda91; A. Roland, M. Dutour-Sikiric. Evidence:
// kokkos/port_report/PROP_REFRACTION_PR3_KOKKOS.md, PROP_FREQ_SHIFT_M2_KOKKOS.md.
// Each call writes only its outputs and its work buffer: one point per thread needs one
// buffer per thread. ⚠ nvcc may turn a MIN/MAX against 0 into max.f32 (-0/NaN unchecked).
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"  // the PDLIB conventions (1-based index values, LayoutLeft)
#include "real.hpp"
#include "st4_params.hpp"  // max_r(a,b) = a>b?a:b, min_r(a,b) = a<b?a:b

namespace ww::pdlib {

/// The module arrays and scalars both routines read (USE lists :495-499, :803-806), as
/// 0-based pointers at the Fortran's first element; device memory on the device.
struct PropParallArrays {
  int nk = 0;     ///< W3GDATMD NK
  int nth = 0;    ///< W3GDATMD NTH
  int nspec = 0;  ///< W3GDATMD NSPEC = NK*NTH
  int msea = 0;   ///< first extent of MAPSF(MSEA,3) (w3gdatmd.F90:1736)
  /// First extent of DDDX..DCYDY: 1 when LPDLIB, NY otherwise (w3adatmd.F90:1374-1392).
  int ny = 1;
  int lpdlib = 1;  ///< CONSTANTS LPDLIB (constants.F90:102), 0/1; read by M2 only
  int flcur = 0;   ///< W3IDATMD FLCUR, 0/1; read by PR3 only
  Real dmin = 0;   ///< W3GDATMD DMIN
  Real ctmax = 0;  ///< W3GDATMD CTMAX; read by PR3 only
  Real dth = 0;    ///< W3GDATMD DTH; read by PR3 only

  /// SIG(0:MK+1), DSIP(0:MK+1) (w3gdatmd.F90:2066, :2068): SIG(IK) = sig[IK].
  const Real* sig = nullptr;
  const Real* dsip = nullptr;
  /// ECOS..ES2, MAPWN (MSPEC+MTH; MAPWN values 1-based IK): X(ISP) = x[ISP-1].
  const Real* ecos = nullptr;
  const Real* esin = nullptr;
  const Real* ec2 = nullptr;
  const Real* esc = nullptr;
  const Real* es2 = nullptr;
  const int* mapwn = nullptr;
  /// MAPSF(MSEA,3): mapsf[(ISEA-1) + (J-1)*msea]; M2's non-PDLIB branch only.
  const int* mapsf = nullptr;
  /// CTHG0S(0:MSEA) (w3gdatmd.F90:1743): cthg0s[ISEA]. PR3 only.
  const Real* cthg0s = nullptr;
  /// CG, WN(0:NK+1,0:NSEA) (w3adatmd.F90:1312, :1325): X(IK,ISEA) = x[IK + ISEA*(NK+2)].
  const Real* cg = nullptr;
  const Real* wn = nullptr;
  /// DW(0:NSEA), CX(0:NSEA), CY(0:NSEA) (w3adatmd.F90:1004, :1008): x[ISEA].
  const Real* dw = nullptr;
  const Real* cx = nullptr;
  const Real* cy = nullptr;
  /// DDDX..DCYDY (ny,*): X(I,J) = x[(I-1) + (J-1)*ny].
  const Real* dddx = nullptr;
  const Real* dddy = nullptr;
  const Real* dcxdx = nullptr;
  const Real* dcxdy = nullptr;
  const Real* dcydx = nullptr;
  const Real* dcydy = nullptr;
};

/// Work reals per call of either routine (PR3 DSDD, FRK, FRG; M2 DSDD, FKC, VCWN): one
/// buffer serves both, one per thread.
KOKKOS_INLINE_FUNCTION constexpr int prop_parall_work_reals(int nk, int nth, int nspec) {
  const int pr3 = (nk + 2) + nk + nk;
  const int m2 = (nk + 2) + nth + (nspec + 2 * nth);
  return pr3 > m2 ? pr3 : m2;
}

namespace parall_detail {
// DEPTH and DSDD(0:NK+1), :533-540 and :866-873. DEPTH*WN < 5. is comiss on WN*DEPTH:
// a NaN takes the ELSE branch, as .LT. does.
KOKKOS_INLINE_FUNCTION Real dsdd_fill(const PropParallArrays& a, int isea, Real* dsdd) {
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r05 = static_cast<Real>(0.5);
  constexpr Real r5 = static_cast<Real>(5.);
  const int nk2 = a.nk + 2;
  const Real* cg = a.cg + static_cast<long>(isea) * nk2;
  const Real* wn = a.wn + static_cast<long>(isea) * nk2;
  const Real depth = ww::st4::max_r(a.dw[isea], a.dmin);  // maxss DMIN,DW
  for (int ik = 0; ik <= a.nk + 1; ++ik) {
    if (wn[ik] * depth < r5) {
      const Real t = wn[ik] * cg[ik] - a.sig[ik] * r05;
      dsdd[ik] = ww::st4::max_r(t, r0) / depth;  // maxss 0,t
    } else {
      dsdd[ik] = r0;
    }
  }
  return depth;
}
}  // namespace parall_detail

/// PROP_REFRACTION_PR3, :447-589. ip, isea 1-based; cad = CAD(1:NSPEC), out. cad also holds
/// VCFLT during the call: each VCFLT(ISP) is read and rewritten at the same ISP only.
KOKKOS_INLINE_FUNCTION void prop_refraction_pr3(const PropParallArrays& a, int ip, int isea,
                                                Real dtg, Real* cad, bool do_limiter,
                                                Real* work) {
  using ww::st4::min_r;
  constexpr Real r0 = static_cast<Real>(0.);
  constexpr Real r1 = static_cast<Real>(1.0);
  const int nk = a.nk;
  const int nspec = a.nspec;
  Real* dsdd = work;          // DSDD(0:NK+1): DSDD(IK) = dsdd[IK]
  Real* frk = dsdd + nk + 2;  // FRK(NK):      FRK(IK)  = frk[IK-1]
  Real* frg = frk + nk;       // FRG(NK):      FRG(IK)  = frg[IK-1]
  Real* vcflt = cad;          // VCFLT(NSPEC): VCFLT(ISP) = vcflt[ISP-1]
  const Real* cg = a.cg + static_cast<long>(isea) * (nk + 2);
  const Real* wn = a.wn + static_cast<long>(isea) * (nk + 2);

  // :527-530. DDDX(1,IP), DDDY(1,IP): element (IP-1)*ny of a (ny,*) array, where ny is
  // the first extent, NY or 1 as LPDLIB allocates it (w3adatmd.F90:1375,1384).
  const Real edddx = a.dddx[static_cast<long>(ip - 1) * a.ny];
  const Real edddy = a.dddy[static_cast<long>(ip - 1) * a.ny];
  const Real ecthg0 = a.cthg0s[isea];
  const Real facth = r1 / a.dth;  // :530, divss
  const Real fdg = ecthg0 * facth;  // :532
  // :533-540, DEPTH and DSDD.
  parall_detail::dsdd_fill(a, isea, dsdd);
  // :541-544. (FACTH*DSDD)/WN, FDG*CG.
  for (int ik = 1; ik <= nk; ++ik) {
    frk[ik - 1] = (facth * dsdd[ik]) / wn[ik];
    frg[ik - 1] = fdg * cg[ik];
  }
  if (a.flcur != 0) {
    // :546-552. DCXDX(1,IP).. read only here, as the object does.
    const Real edcxdx = a.dcxdx[ip - 1];
    const Real edcxdy = a.dcxdy[ip - 1];
    const Real edcydx = a.dcydx[ip - 1];
    const Real edcydy = a.dcydy[ip - 1];
    const Real dcyx = facth * edcydx;
    const Real dcxxyy = facth * (edcxdx - edcydy);  // the parenthesis is kept, :551
    const Real dcxy = facth * edcxdy;
    // :553-555. (ES2*DCYX + ESC*DCXXYY) - EC2*DCXY.
    for (int isp = 1; isp <= nspec; ++isp) {
      vcflt[isp - 1] =
          (a.es2[isp - 1] * dcyx + a.esc[isp - 1] * dcxxyy) - a.ec2[isp - 1] * dcxy;
    }
  } else {
    for (int isp = 1; isp <= nspec; ++isp) vcflt[isp - 1] = r0;  // :557, VCFLT=0
  }
  // :568-582. (VCFLT + FRG*ECOS) + FRK*(ESIN*DDDX - ECOS*DDDY), then the CTMAX
  // limiter: copysign(minss C,|v|) with C = CTMAX/DTG (:567, divss).
  const Real ctmax_eff = a.ctmax / dtg;
  for (int isp = 1; isp <= nspec; ++isp) {
    const int mw = a.mapwn[isp - 1];
    const Real velnofilt =
        (vcflt[isp - 1] + frg[mw - 1] * a.ecos[isp - 1]) +
        frk[mw - 1] * (a.esin[isp - 1] * edddx - a.ecos[isp - 1] * edddy);
    if (do_limiter) {
      vcflt[isp - 1] = Kokkos::copysign(min_r(Kokkos::fabs(velnofilt), ctmax_eff), velnofilt);
    } else {
      vcflt[isp - 1] = velnofilt;
    }
  }
  // :583-585. CAD = DBLE(VCFLT) into REAL: the same bits, already in place.
}

/// PROP_FREQ_SHIFT_M2, :754-894. cwnb_m2 = CWNB_M2(1-NTH:NSPEC), CWNB_M2(ISP) =
/// cwnb_m2[ISP+NTH-1]; dwni_m2 = DWNI_M2(NK). Both out.
KOKKOS_INLINE_FUNCTION void prop_freq_shift_m2(const PropParallArrays& a, int ip, int isea,
                                               Real* cwnb_m2, Real* dwni_m2, Real dtg,
                                               Real* work) {
  constexpr Real r05 = static_cast<Real>(0.5);
  const int nk = a.nk;
  const int nth = a.nth;
  const int nspec = a.nspec;
  Real* dsdd = work;          // DSDD(0:NK+1):          DSDD(IK) = dsdd[IK]
  Real* fkc = dsdd + nk + 2;  // FKC(NTH):              FKC(ITH) = fkc[ITH-1]
  Real* vcwn = fkc + nth;     // VCWN(1-NTH:NSPEC+NTH): VCWN(ISP) = vcwn[ISP+NTH-1]
  const Real* cg = a.cg + static_cast<long>(isea) * (nk + 2);
  const Real* wn = a.wn + static_cast<long>(isea) * (nk + 2);

  // :836-852.
  long j;
  if (a.lpdlib != 0) {
    j = ip - 1;  // X(1,IP)
  } else {
    const int ix = a.mapsf[(isea - 1) + 0 * a.msea];
    const int iy = a.mapsf[(isea - 1) + 1 * a.msea];
    j = (iy - 1) + static_cast<long>(ix - 1) * a.ny;  // X(IY,IX)
  }
  const Real edcxdx = a.dcxdx[j];
  const Real edcxdy = a.dcxdy[j];
  const Real edcydx = a.dcydx[j];
  const Real edcydy = a.dcydy[j];
  const Real edddx = a.dddx[j];
  const Real edddy = a.dddy[j];
  // :854-860. Three sign flips (xorps) of products; FKD = (CX*DDDX + CY*DDDY)*FACK.
  const Real ecx = a.cx[isea];
  const Real ecy = a.cy[isea];
  const Real fack = dtg;
  const Real dcxx = -(fack * edcxdx);
  const Real dcxyyx = -(fack * (edcxdy + edcydx));
  const Real dcyy = -(fack * edcydy);
  const Real fkd = fack * (ecx * edddx + ecy * edddy);
  // :862-864. (EC2*DCXX + ESC*DCXYYX) + ES2*DCYY.
  for (int ith = 1; ith <= nth; ++ith) {
    fkc[ith - 1] = (a.ec2[ith - 1] * dcxx + a.esc[ith - 1] * dcxyyx) + a.es2[ith - 1] * dcyy;
  }
  // :866-873.
  parall_detail::dsdd_fill(a, isea, dsdd);
  // :874-881. FKD0 = (FKD/CG)*DSDD; VCWN = WN*FKC + FKD0, ISP from 1-NTH.
  int isp = -nth;
  for (int ik = 0; ik <= nk + 1; ++ik) {
    const Real fkd0 = (fkd / cg[ik]) * dsdd[ik];
    for (int ith = 1; ith <= nth; ++ith) {
      isp = isp + 1;
      vcwn[isp + nth - 1] = wn[ik] * fkc[ith - 1] + fkd0;
    }
  }
  // :884-887. 0.5*(VCWN(ISP) + VCWN(ISP+NTH)); sumDiff (:883, :886) is dead.
  for (int i = 1 - nth; i <= nspec; ++i) {
    cwnb_m2[i + nth - 1] = r05 * (vcwn[i + nth - 1] + vcwn[i + 2 * nth - 1]);
  }
  // :888-890. CG/DSIP.
  for (int ik = 1; ik <= nk; ++ik) dwni_m2[ik - 1] = cg[ik] / a.dsip[ik];
}

}  // namespace ww::pdlib
