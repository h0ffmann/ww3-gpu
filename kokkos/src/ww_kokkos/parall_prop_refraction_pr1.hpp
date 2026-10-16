// kokkos/src/ww_kokkos/parall_prop_refraction_pr1.hpp
// PROP_REFRACTION_PR1 of W3PARALL: the refraction velocity CAD of one sea point, the PR1 form
// (no CTMAX limiter, structured-grid indexing through MAPSF).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3parall.F90:285-431 at NOAA-EMC/WW3 03adda91;
// authors Aron Roland (BGS IT&E GmbH), Mathieu Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/PROP_REFRACTION_PR1_KOKKOS.md.
// No caller in WW3: both CALLs are commented out (w3profsmd_pdlib.F90:4540, :4697).
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_parall.hpp"  // PropParallArrays, parall_detail::dsdd_fill, prop_parall_work_reals
#include "real.hpp"

namespace ww::parall {

/// PROP_REFRACTION_PR1(ISEA, DTG, CAD), :285-431. isea 1-based; cad = CAD(1:NSPEC), out; it
/// also holds VCFLT during the call (each VCFLT(ISP) is read and rewritten at ISP only).
/// Reads nk, nspec, dmin, dth, flcur, sig, ecos, esin, ec2, esc, es2, mapwn, mapsf, msea,
/// cthg0s, cg, wn, dw, ny and dddx..dcydy of a. Refrx: the W3_REFRX switch (:399-411), which
/// also reads dcdx, dcdy = DCDX, DCDY(0:NK+1, NY or 1, NX or NSEAL) (w3adatmd.F90:1377,1386)
/// at DCDX(0,1,1), with dcd_stride3 = (NK+2) times their second extent; null otherwise.
/// work: at least ww::pdlib::prop_parall_work_reals(nk, nth, nspec) reals, one per thread.
template <bool Refrx = false>
KOKKOS_INLINE_FUNCTION void prop_refraction_pr1(const ww::pdlib::PropParallArrays& a, int isea,
                                                Real dtg, Real* cad, Real* work,
                                                const Real* dcdx = nullptr,
                                                const Real* dcdy = nullptr,
                                                long dcd_stride3 = 0) {
  const int nk = a.nk;
  const int nspec = a.nspec;
  Real* dsdd = work;          // DSDD(0:NK+1): DSDD(IK) = dsdd[IK]
  Real* frk = dsdd + nk + 2;  // FRK(NK):      FRK(IK)  = frk[IK-1]
  Real* frg = frk + nk;       // FRG(NK):      FRG(IK)  = frg[IK-1]
  Real* vcflt = cad;          // VCFLT(NSPEC): VCFLT(ISP) = vcflt[ISP-1]
  const Real* cg = a.cg + static_cast<long>(isea) * (nk + 2);
  const Real* wn = a.wn + static_cast<long>(isea) * (nk + 2);

  // :369-376. DDDX(IY,IX) of a (ny,*) array; ⚠ under LPDLIB the arrays are (1,NSEAL) while
  // MAPSF gives the global node (w3adatmd.F90:1384), so this reads past them when NSEAL < NX.
  const int ix = a.mapsf[(isea - 1) + 0 * a.msea];
  const int iy = a.mapsf[(isea - 1) + 1 * a.msea];
  const long j = (iy - 1) + static_cast<long>(ix - 1) * a.ny;
  const Real edddx = a.dddx[j];
  const Real edddy = a.dddy[j];
  const Real ecthg0 = a.cthg0s[isea];
  const Real facth = dtg / a.dth;   // :374, divss
  const Real fdg = facth * ecthg0;  // :376
  // :377-384, DEPTH and DSDD(0:NK+1); the object computes all NK+2 entries.
  ww::pdlib::parall_detail::dsdd_fill(a, isea, dsdd);
  // :385-388, FDDMAX: dead (its only use, :391, is commented out); the object drops it.
  // :389-393. (FACTH*DSDD)/WN, FDG*CG.
  for (int ik = 1; ik <= nk; ++ik) {
    frk[ik - 1] = (facth * dsdd[ik]) / wn[ik];
    frg[ik - 1] = fdg * cg[ik];
  }
  // :394-397. FRG*ECOS + FRK*(ESIN*DDDX - ECOS*DDDY). Kept under REFRX too: the object
  // stores it before :401 overwrites it.
  for (int isp = 1; isp <= nspec; ++isp) {
    const int mw = a.mapwn[isp - 1];
    vcflt[isp - 1] = frg[mw - 1] * a.ecos[isp - 1] +
                     frk[mw - 1] * (a.esin[isp - 1] * edddx - a.ecos[isp - 1] * edddy);
  }
  if constexpr (Refrx) {
    // :401-410. ((FACTH*CG)*WN)/SIG (:404). ⚠ DCDX(ISP,1,MAPWN(ISP)) indexes the first
    // dimension, 0:NK+1, with ISP up to NSPEC and the third, NX or NSEAL, with IK: WW3
    // reads outside the array's first extent; the same linear element is read here.
    constexpr Real r0 = static_cast<Real>(0.);
    for (int isp = 1; isp <= nspec; ++isp) vcflt[isp - 1] = r0;  // :401
    for (int ik = 1; ik <= nk; ++ik) frk[ik - 1] = r0;            // :402
    for (int ik = 1; ik <= nk; ++ik) {
      frk[ik - 1] = ((facth * cg[ik]) * wn[ik]) / a.sig[ik];
    }
    for (int isp = 1; isp <= nspec; ++isp) {
      const int mw = a.mapwn[isp - 1];
      const long k = isp + static_cast<long>(mw - 1) * dcd_stride3;
      vcflt[isp - 1] = frg[mw - 1] * a.ecos[isp - 1] +
                       frk[mw - 1] * (a.esin[isp - 1] * dcdx[k] - a.ecos[isp - 1] * dcdy[k]);
    }
  }
  if (a.flcur != 0) {
    // :414-424. DCXDX(IY,IX)..; ((VCFLT + ES2*DCYX) + ESC*DCXXYY) - EC2*DCXY.
    const Real edcxdx = a.dcxdx[j];
    const Real edcxdy = a.dcxdy[j];
    const Real edcydx = a.dcydx[j];
    const Real edcydy = a.dcydy[j];
    const Real dcyx = facth * edcydx;
    const Real dcxxyy = facth * (edcxdx - edcydy);  // the parenthesis is kept, :419
    const Real dcxy = facth * edcxdy;
    for (int isp = 1; isp <= nspec; ++isp) {
      vcflt[isp - 1] = vcflt[isp - 1] + a.es2[isp - 1] * dcyx + a.esc[isp - 1] * dcxxyy -
                       a.ec2[isp - 1] * dcxy;
    }
  }
  // :425-427. CAD = DBLE(VCFLT) into REAL CAD: the same bits, already in place.
}

}  // namespace ww::parall
