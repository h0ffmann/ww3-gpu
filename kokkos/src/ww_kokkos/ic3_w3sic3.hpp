// kokkos/src/ww_kokkos/ic3_w3sic3.hpp
// W3SIC3: the sea-ice dissipation source term S_ice of IC3 (viscoelastic ice layer, Wang and Shen
// 2010; Cheng et al.), one spectral point: the diagonal term D and S = D * A. Thin ice or low
// concentration takes the attenuation from the complex dispersion relation (W3IC3WNCG_CHENG from
// a table, or W3IC3WNCG_V1 solved at the point); thicker ice takes the laminar and turbulent
// boundary-layer dissipation of IC2's Stopa et al. 2016 form. Phase 1, translated, not wired, not
// regtested.
// WW3 heritage: WW3/model/src/w3sic3md.F90:96-654 at h0ffmann/WW3 96892c95 (byte-identical to
// NOAA-EMC/WW3 03adda91: git -C WW3 diff 03adda91 HEAD -- model/src/w3sic3md.F90 is empty);
// authors E. Rogers, S. Zieger, F. Ardhuin, G. Boutin, X. Zhao, S. Cheng. Called from W3SRCE,
// W3EXPO, W3EXNC and GXEXPO.
// Evidence: kokkos/port_report/W3SIC3_KOKKOS.md.
// SPDX-FileCopyrightText: 2009 National Weather Service (NWS), National Oceanic and Atmospheric Administration
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// As switch_lab_shrd with IC3 in place of IC0 compiles it (no T/S/T0/T1/T38 prints). DEPTH is
// passed to the two wavenumber routines; the Fortran's ICECOEF5 and ICEP5 are not read.
#pragma once

#include <Kokkos_Core.hpp>

#include "ic3_cmplx_root_muller_cheng.hpp"
#include "ic3_w3ic3wncg_cheng.hpp"
#include "ic3_w3ic3wncg_v1.hpp"
#include "libm_device.hpp"
#include "prop_wavnu1.hpp"
#include "real.hpp"
#include "st4_params.hpp"

namespace ww::ic3 {

/// SIZEFWTABLE (constants.F90:92); FWTABLE is FWTABLE(0:300).
inline constexpr int kSizeFwTable = 300;
/// ITKNUM = CEILING(IC3_MAXITK / IC3_DITK) (w3sic3md.F90:1921), IC3_DITK = 0.01 and IC3_MAXITK = 3.
/// (:89), REAL: the quotient 3.0f / 0.01f is 300.0f.
inline constexpr int kItknum = 300;
static_assert(3.0f / 0.01f == 300.0f);

/// The grid scalars and arrays W3SIC3 reads (W3GDATMD, CONSTANTS, W3ADATMD).
struct Ic3Grid {
  int nk;                ///< NK
  int nth;               ///< NTH
  int nspec;             ///< NSPEC
  Real ic3pars[16];      ///< IC3PARS(1:16): ic3pars[i-1]
  Real delab;            ///< DELAB (constants.F90:94)
  bool flagll;           ///< FLAGLL
  bool rlgtype;          ///< GTYPE .EQ. RLGTYPE
};

/// The point's fields: INFLAGS2(-7), (-6), (-5), (-4), (4); ICEP1..ICEP4(IX,IY) and ICEI(IX,IY)
/// each read only when its flag is set (the others may hold anything); YGRD(IY,IX) is REAL(8).
struct Ic3Point {
  bool inflags_m7;
  bool inflags_m6;
  bool inflags_m5;
  bool inflags_m4;
  bool inflags_p4;
  Real icep1;
  Real icep2;
  Real icep3;
  Real icep4;
  Real iceconc;
  double ygrd;
};

/// IC3WN_R, IC3WN_I and IC3CG of W3ADATMD, the table W3IC3WNCG_CHENG reads, each (0:NK+1,
/// 0:ITKNUM) column-major with leading dimension NK + 2 (ic3_w3ic3wncg_cheng.hpp).
struct Ic3Tables {
  const Real* ic3wn_r;
  const Real* ic3wn_i;
  const Real* ic3cg;
};

/// The work arrays of W3SIC3's automatic arrays and of the two wavenumber routines, each NK
/// long: D1D, WN_I, WN_R, CG_IC3 (CG_TMP is a pointer to CG_IC3 or CG here), and the DELTA results (V1) or SMOOTH_K's work (Cheng).
struct Ic3Work {
  Real* d1d;
  Real* wn_i;
  Real* wn_r;
  Real* cg_ic3;
  Real* dsig;  ///< also SMOOTH_K's DIFF
  Real* dwn;   ///< also SMOOTH_K's REMOVEID
};

/// Returned where the Fortran writes its message and calls EXTCDE(2): an ice input given by
/// the input files and by the IC3 namelist both or by neither (:429-462, NUMIN /= 1), or the
/// code W3IC3WNCG_V1 returns (ic3_w3ic3wncg_v1.hpp). The caller does the WRITE.
inline constexpr int kExtcde = 2;

/// W3SIC3(A, DEPTH, CG, WN, IX, IY, S, D) for one point. Arrays are 0-based. a, s, d: NSPEC;
/// cg, wn, sig = SIG(1:NK), dden = DDEN(1:NK): NK; mapwn: NSPEC values of MAPWN, 1-based IK;
/// fwtable: 0:300. disp is the DISTAB table WAVNU1 reads, tables the IC3 table. Returns 0, or
/// a nonzero code with d = 0 (:402) and s not written.
///
/// The thick-ice branch (:563-611) with IC3PARS(2) <= 0 reads PTURB and PVISC that no statement
/// sets: undefined in the Fortran, whatever the object holds there. Here they are 0. ⚠
KOKKOS_INLINE_FUNCTION int w3sic3(const Ic3Grid& g, const Ic3Point& pt, const Real* a, Real depth,
                                  const Real* cg, const Real* wn, const Real* sig,
                                  const Real* dden, const int* mapwn, const Real* fwtable,
                                  const Ic3Tables& tables, const ww::prop::DispTable& disp,
                                  const Ic3Work& w, Real* s, Real* d) {
  const int nk = g.nk;
  const int nth = g.nth;
  const int nspec = g.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  // GRAV (constants.F90:62); VISCM = 1.83E-6 (:381) is a SAVEd variable the object folds.
  const Real grav = static_cast<Real>(9.806);
  const Real viscm = static_cast<Real>(1.83E-6);

  for (int is = 0; is < nspec; ++is) d[is] = zero;  // :402
  for (int ik = 0; ik < nk; ++ik) {
    w.d1d[ik] = zero;  // :403
    w.wn_r[ik] = wn[ik];  // :405
    w.wn_i[ik] = zero;    // :406
    w.cg_ic3[ik] = zero;  // :407
  }

  // :418-425.
  const Real maxthk = g.ic3pars[0];
  const Real maxcnc = g.ic3pars[7];
  const Real use_cheng = g.ic3pars[8];
  const Real use_cgice = g.ic3pars[11];
  const Real fixedhice = g.ic3pars[12];
  const Real fixedvisc = g.ic3pars[13];
  const Real fixeddens = g.ic3pars[14];
  const Real fixedelas = g.ic3pars[15];

  // :429-462. Each of the four inputs comes from exactly one source.
  if ((static_cast<int>(pt.inflags_m7) + static_cast<int>(fixedhice >= zero)) != 1) return kExtcde;
  if ((static_cast<int>(pt.inflags_m6) + static_cast<int>(fixedvisc >= zero)) != 1) return kExtcde;
  if ((static_cast<int>(pt.inflags_m5) + static_cast<int>(fixeddens >= zero)) != 1) return kExtcde;
  if ((static_cast<int>(pt.inflags_m4) + static_cast<int>(fixedelas >= zero)) != 1) return kExtcde;

  // :467-493.
  const Real icecoef1 = pt.inflags_m7 ? pt.icep1 : fixedhice;
  const Real icecoef2 = pt.inflags_m6 ? pt.icep2 : fixedvisc;
  const Real icecoef3 = pt.inflags_m5 ? pt.icep3 : fixeddens;
  const Real icecoef4 = pt.inflags_m4 ? pt.icep4 : fixedelas;
  const Real iceconc = pt.inflags_p4 ? pt.iceconc : zero;

  // :498-500.
  bool noice = false;
  if (icecoef1 == zero) noice = true;
  if (pt.inflags_p4 && (iceconc == zero)) noice = true;

  if (noice) {
    // :504, D1D = 0. already.
  } else if (use_cheng == one && ((icecoef1 <= maxthk) || (iceconc <= maxcnc))) {  // :507-508
    // :520-521.
    w3ic3wncg_cheng(w.wn_r, w.wn_i, w.cg_ic3, icecoef1, icecoef2, icecoef3, icecoef4, depth, nk,
                    nk, sig, g.ic3pars[9], g.ic3pars[10], tables.ic3wn_r, tables.ic3wn_i,
                    tables.ic3cg, kItknum, static_cast<Real>(0.01), disp, w.dsig, w.dwn,
                    CmplxRootMullerCheng{});
    const Real* cg_tmp = (use_cgice == one) ? w.cg_ic3 : cg;  // :525-529
    for (int ik = 0; ik < nk; ++ik) {
      w.d1d[ik] = -static_cast<Real>(2.0) * cg_tmp[ik] * w.wn_i[ik];  // :532
    }
  } else if ((icecoef1 <= maxthk) || (iceconc <= maxcnc)) {  // :536
    // :549-550.
    const int code = w3ic3wncg_v1(w.wn_r, w.wn_i, w.cg_ic3, icecoef1, icecoef2, icecoef3,
                                  icecoef4, depth, nk, nk, sig, g.ic3pars[9], g.ic3pars[10],
                                  disp, w.dsig, w.dwn);
    if (code != 0) return code;
    const Real* cg_tmp = (use_cgice == one) ? w.cg_ic3 : cg;  // :553-557
    for (int ik = 0; ik < nk; ++ik) {
      w.d1d[ik] = -static_cast<Real>(2.0) * cg_tmp[ik] * w.wn_i[ik];  // :560
    }
  } else {
    // :563-611. Thick ice.
    Real dturb;
    Real pturb = zero;
    Real pvisc = zero;
    if (g.ic3pars[1] > zero) {  // :565
      Real uorb = zero;
      Real aorb = zero;
      Real fturb = g.ic3pars[1];  // :568
      if (g.ic3pars[6] > zero) {  // :569
        if (pt.ygrd < 0.0 && g.rlgtype && g.flagll) fturb = g.ic3pars[6];  // :570-571
      }
      for (int ik = 0; ik < nk; ++ik) {
        Real eb = zero;
        for (int ith = 1; ith <= nth; ++ith) eb = eb + a[ith + ik * nth - 1];  // :574-578
        uorb = uorb + ((eb * (sig[ik] * sig[ik])) * dden[ik]) / cg[ik];  // :583
        aorb = aorb + (eb * dden[ik]) / cg[ik];                           // :584
      }
      aorb = static_cast<Real>(2) * Kokkos::sqrt(aorb);  // :588
      uorb = static_cast<Real>(2) * Kokkos::sqrt(uorb);  // :589

      const Real re = uorb * aorb / viscm;  // :591
      const Real smooth = static_cast<Real>(0.5) *
                          ww::dm::tanhf((re - g.ic3pars[3]) / g.ic3pars[4]);  // :592
      pturb = (static_cast<Real>(0.5) + smooth);  // :593
      pvisc = (static_cast<Real>(0.5) - smooth);  // :594

      // :596. MAX(AORB/IC3PARS(3), 3.) is a maxss of the object with 3. as the destination:
      // a tie or a NaN gives the quotient.
      const Real quo = aorb / g.ic3pars[2];
      const Real three = static_cast<Real>(3.);
      const Real xm = (three > quo) ? three : quo;
      const Real xi = (ww::dm::log10f(xm) - static_cast<Real>(-1.)) / g.delab;
      // :597-598. INT is cvttss2si; MIN(1., XI-FLOAT(IND)) is a minss of the object with the
      // difference as the destination: a tie or a NaN gives 1.
      const int ii = ww::st4::fint(xi);
      const int ind = ii < (kSizeFwTable - 1) ? ii : (kSizeFwTable - 1);
      const Real df = xi - static_cast<Real>(ind);
      const Real deli1 = (df < one) ? df : one;
      const Real deli2 = one - deli1;                                    // :599
      const Real fw = fwtable[ind] * deli2 + fwtable[ind + 1] * deli1;  // :600
      dturb = -static_cast<Real>(1.) * fturb * fw * uorb / grav;         // :601
    } else {
      dturb = zero;  // :603
    }
    for (int ik = 0; ik < nk; ++ik) {
      // :607-608.
      const Real dvisc = -static_cast<Real>(1.) * g.ic3pars[5] * wn[ik] *
                         Kokkos::sqrt(viscm * sig[ik] / static_cast<Real>(2.));
      w.d1d[ik] = pturb * dturb * (sig[ik] * sig[ik]) + pvisc * dvisc;
    }
  }

  // :615-617.
  for (int ikth = 0; ikth < nspec; ++ikth) d[ikth] = w.d1d[mapwn[ikth] - 1];
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];  // :624
  return 0;
}

}  // namespace ww::ic3
