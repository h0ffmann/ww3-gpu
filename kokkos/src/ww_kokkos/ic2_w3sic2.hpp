// kokkos/src/ww_kokkos/ic2_w3sic2.hpp
// W3SIC2: the sea-ice dissipation source term S_ice of IC2 (Liu et al. 1991; Stopa et al. 2016,
// with the turbulent boundary layer of F. Ardhuin), one spectral point: the diagonal term D and
// S = D * A, from the ice-modified wavenumber, group velocity and attenuation. Phase 1,
// translated, not wired, not regtested.
// WW3 heritage: WW3/model/src/w3sic2md.F90:120-513 at h0ffmann/WW3 96892c95 (byte-identical to
// NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD -- model/src/w3sic2md.F90 is empty);
// authors E. Rogers, S. Zieger, F. Ardhuin, G. Boutin. Called from W3SRCE, W3EXPO, W3EXNC and
// GXEXPO.
// Evidence: kokkos/port_report/W3SIC2_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// As switch_lab_shrd with IC2 in place of IC0 compiles it (SHRD, ST4, no IS2, no T/S/T0/T1/T38):
// the IS2 floe-size factor SMOOTH_DMAX (:429-432 of the source, under W3_IS2) is the constant 1.
// that the Fortran stores at :351 and never changes, so it is kept as the constant. WN, DEPTH and
// ICEF are arguments of the Fortran routine the compiled body never reads, and ICECOEF2 =
// ICEP2(IX,IY) (:354) is read into a variable nothing uses: none is a parameter here.
#pragma once

#include <Kokkos_Core.hpp>

#include "libm_device.hpp"
#include "real.hpp"
#include "st4_params.hpp"

namespace ww::ic2 {

/// SIZEFWTABLE (constants.F90:92); FWTABLE is FWTABLE(0:300).
inline constexpr int kSizeFwTable = 300;

/// The grid scalars W3SIC2 reads (W3GDATMD, W3IDATMD, CONSTANTS).
struct Ic2Grid {
  int nk;                ///< NK
  int nth;               ///< NTH
  int nspec;             ///< NSPEC
  Real ic2pars[9];       ///< IC2PARS(1:9): ic2pars[i-1]
  Real delab;            ///< DELAB (constants.F90:94, set by the CONSTANTS table build)
  bool flagll;           ///< FLAGLL
  bool rlgtype;          ///< GTYPE .EQ. RLGTYPE
};

/// The point's fields: INFLAGS2(-7), INFLAGS2(-6) and INFLAGS2(4), ICEI(IX,IY) (read only
/// when INFLAGS2(4), so a caller need not read it otherwise), YGRD(IY,IX) (REAL(8)).
struct Ic2Point {
  bool inflags_m7;
  bool inflags_m6;
  bool inflags_p4;
  Real iceconc;
  double ygrd;
};

/// Returned where the Fortran writes its message and calls EXTCDE(2): IC2PARS(1) > 0.5 with
/// INFLAGS2(-7) or INFLAGS2(-6) false (:387-393). The caller does the WRITE.
inline constexpr int kExtcde = 2;

/// W3SIC2(A, DEPTH, ICEH, ICEF, CG, WN, IX, IY, S, D, WN_R, CG_ICE, ALPHA, R) for one point.
/// Arrays are 0-based. a, s, d: NSPEC; cg, sig, dden, wn_r, cg_ice, alpha, r: NK (sig =
/// SIG(1:NK), dden = DDEN(1:NK)); mapwn: NSPEC values of MAPWN, 1-based IK; fwtable: 0:300.
/// d1d is the NK work array of D1D. Returns 0, or kExtcde with d = 0 (:343) and s not written.
KOKKOS_INLINE_FUNCTION int w3sic2(const Ic2Grid& g, const Ic2Point& pt, const Real* a, Real iceh,
                                  const Real* cg, const Real* sig, const Real* dden,
                                  const int* mapwn, const Real* fwtable, const Real* wn_r,
                                  const Real* cg_ice, const Real* alpha, const Real* r, Real* d1d,
                                  Real* s, Real* d) {
  const int nk = g.nk;
  const int nth = g.nth;
  const int nspec = g.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real half = static_cast<Real>(0.5);
  // GRAV (constants.F90:62); VISCM = 1.83E-6 (:326) is a SAVEd variable the object folds.
  const Real grav = static_cast<Real>(9.806);
  const Real viscm = static_cast<Real>(1.83E-6);
  const Real sm = static_cast<Real>(1.);  // SMOOTH_DMAX(IK), :351

  for (int is = 0; is < nspec; ++is) d[is] = zero;  // :343

  // :347-362.
  const Real icecoef1 = iceh;
  const Real iceconc = pt.inflags_p4 ? pt.iceconc : zero;
  bool noice = false;
  if (icecoef1 == zero) noice = true;
  if (pt.inflags_p4 && (iceconc == zero)) noice = true;

  if (noice) {
    // :365, D = 0. already.
  } else {
    if (g.ic2pars[0] > half) {  // :386
      if (!pt.inflags_m7) return kExtcde;
      if (!pt.inflags_m6) return kExtcde;
      for (int ik = 0; ik < nk; ++ik) {
        const Real wn_i = half * alpha[ik];  // :396
        // :402. D1D = S/E = -2 Cg k_i; CG, not CG_ICE.
        d1d[ik] = -static_cast<Real>(2.0) * cg[ik] * wn_i;
      }
    } else {
      Real dturb;
      Real pturb;
      Real pvisc;
      if (g.ic2pars[1] > zero) {  // :409
        Real uorb = zero;
        Real aorb = zero;
        Real fturb = g.ic2pars[1];  // :412
        if (g.ic2pars[6] > zero) {  // :414
          if (pt.ygrd < 0.0 && g.rlgtype && g.flagll) fturb = g.ic2pars[6];
        }
        for (int ik = 0; ik < nk; ++ik) {
          Real eb = zero;
          for (int ith = 1; ith <= nth; ++ith) eb = eb + a[ith + ik * nth - 1];  // :418-421
          if (r[ik] > static_cast<Real>(1.)) {  // :435
            const Real q = (r[ik] * cg_ice[ik]) / cg[ik];
            uorb = uorb + ((((eb * sm) * (sig[ik] * sig[ik])) * dden[ik]) / cg[ik]) / q;  // :436
            aorb = aorb + (((eb * sm) * dden[ik]) / cg[ik]) / q;                           // :438
          } else {
            uorb = uorb + (((eb * sm) * (sig[ik] * sig[ik])) * dden[ik]) / cg[ik];  // :441
            aorb = aorb + ((eb * sm) * dden[ik]) / cg[ik];                           // :442
          }
        }
        aorb = static_cast<Real>(2) * Kokkos::sqrt(aorb);  // :447
        uorb = static_cast<Real>(2) * Kokkos::sqrt(uorb);  // :448

        const Real re = uorb * aorb / viscm;  // :450
        const Real smooth =
            half * ww::dm::tanhf((re - g.ic2pars[3]) / g.ic2pars[4]);  // :451
        pturb = (half + smooth);  // :452
        pvisc = (half - smooth);  // :453

        // :455. MAX(AORB/IC2PARS(3), 3.) is a maxss of the object with 3. as the destination:
        // a tie or a NaN gives the quotient.
        const Real quo = aorb / g.ic2pars[2];
        const Real three = static_cast<Real>(3.);
        const Real xm = (three > quo) ? three : quo;
        const Real xi = (ww::dm::log10f(xm) - static_cast<Real>(-1.)) / g.delab;
        // :456-457. INT is cvttss2si; MIN(1., XI-FLOAT(IND)) is a minss of the object with the
        // difference as the destination: a tie or a NaN gives 1.
        const int ind = ww::st4::fint(xi) < (kSizeFwTable - 1) ? ww::st4::fint(xi)
                                                                : (kSizeFwTable - 1);
        const Real df = xi - static_cast<Real>(ind);
        const Real one = static_cast<Real>(1.);
        const Real deli1 = (df < one) ? df : one;
        const Real deli2 = static_cast<Real>(1.) - deli1;  // :458
        const Real fw = fwtable[ind] * deli2 + fwtable[ind + 1] * deli1;  // :459
        dturb = fturb * fw * uorb / grav;                                  // :460
      } else {
        dturb = zero;  // :462
        pturb = zero;
        pvisc = static_cast<Real>(1.);
      }
      for (int ik = 0; ik < nk; ++ik) {
        // :469-470. MAX(DTURB*SIG**2, DVISC) is a maxss/maxps of the object with DTURB*SIG**2
        // as the destination: a tie or a NaN gives DVISC.
        const Real dvisc = g.ic2pars[5] * wn_r[ik] * Kokkos::sqrt(viscm * sig[ik] / static_cast<Real>(2.));
        const Real t = dturb * (sig[ik] * sig[ik]);
        const Real mx = (t > dvisc) ? t : dvisc;
        d1d[ik] = -static_cast<Real>(1.) * (pturb * mx + pvisc * dvisc) * sm;
      }
    }
    // :478-480. D(IKTH) = D1D(MAPWN(IKTH)).
    for (int ikth = 0; ikth < nspec; ++ikth) d[ikth] = d1d[mapwn[ikth] - 1];
  }
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];  // :484
  return 0;
}

}  // namespace ww::ic2
