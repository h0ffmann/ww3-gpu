// kokkos/src/ww_kokkos/st4_sin4.hpp
// W3SIN4, ported to Kokkos: phase 1, translated, not regtested.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:423-1003 at WW3@bb08623a (F. Ardhuin,
// H. L. Tolman; SHOM). Wind input source term and its diagonal (WAM4 Janssen,
// gustiness, Ardhuin et al. swell dissipation) and the wave-supported stress.
//
// Arithmetic follows the -O3 GIMPLE of WW3's build (w3src4md.F90.273t.optimized,
// function w3sin4): folded constants, x**4 as (x*x)*(x*x), x**5 as x2*(x2*x),
// x**3 as x*(x*x), a REAL exponent as powf. Branches under W3_T, W3_T0, W3_T1 and
// W3_S are debug output and are not ported.
#pragma once

#include <Kokkos_Core.hpp>

#include "libm_device.hpp"
#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"
#include "st4_vec8.hpp"

namespace ww::st4 {

namespace sin4_detail {


// Constants gfortran folds at compile time; each checked against the dump.
inline constexpr Real kKappa2 = kKappa * kKappa;  // KAPPA**2, :606
static_assert(kKappa2 == 1.6000001132488250732421875e-1f, "KAPPA**2 as folded by gfortran");
inline constexpr Real kZ0viscNum = static_cast<Real>(0.1) * kNuAir;  // 0.1*nu_air, :611
static_assert(kZ0viscNum == 1.4000000874148099683225154876708984375e-6f, "0.1*NU_AIR");
inline constexpr Real kTwoNuAir = static_cast<Real>(2) * kNuAir;  // 2*NU_AIR, :737
inline constexpr Real kGrav2Tpi = (kGrav * kGrav) * kTpi;  // (GRAV**2)*tpi, :847
static_assert(kGrav2Tpi == 6.0417620849609375e+2f, "(GRAV**2)*TPI");
// LOG(TPI/0.005) at :879, folded by gfortran (MPFR), not a logf call at run time.
inline constexpr Real kLogTpiOver005 = 7.1361942291259765625e+0f;
inline constexpr Real kEps1 = static_cast<Real>(0.00001);   // EPS1, :553
inline constexpr Real kEps2 = static_cast<Real>(0.000001);  // EPS2, :553
inline constexpr Real kEps2Sq = kEps2 * kEps2;              // EPS2**2, :972
static_assert(kEps2Sq == 9.999999960041972002500187954865396022796630859375e-13f, "EPS2**2");
inline constexpr int kJtot = 50;                            // JTOT, :574
inline constexpr Real kKm = static_cast<Real>(363.);        // KM, :575
inline constexpr Real kCmm = static_cast<Real>(0.2325);     // CMM, :575

/// W3SIN4's locals that cross from section 1 to the stress scan of section 2, and from the scan
/// to the rest; the parts copy them in and out, which changes no bit.
struct Mid {
  Real const1, const0, uorb, pturb, pvisc, fw, fu, fud, ustarsigma;
  Real ust, taux, tauy, cosu, sinu, ustp, usdirp;
  Real stressstab[3][2], stressstabn[3][2];
};


using lanes::M8;
using lanes::V8;
using lanes::vload;
using lanes::vstore;

}  // namespace sin4_detail

/// Reals and ints of per-point work storage w3sin4_point needs (no allocation in a kernel).
/// DSTAB(3,NSPEC), w3src4md.F90:567 (declared), :600 (zeroed), stored as dstab[(ISTAB-1)*NSPEC + IS-1].
inline int sin4_work_reals(const Params& p) { return 3 * p.nspec; }
inline int sin4_work_ints(const Params& p) { (void)p; return 0; }

/// Section 1 of W3SIN4 (:600-675) and the stability correction of section 2 (:683-692).
KOKKOS_INLINE_FUNCTION void sin4_prep(const Params& p, const Fields& f, const Real* a,
                                      const Real* cg, Real u, Real ustar, Real drat, Real as,
                                      Real z0, Real* work, sin4_detail::Mid& m) {
  using namespace sin4_detail;
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  Real* dstab = work;
  // 1.  Preparations (:600-602)
  for (int i = 0; i < 3 * nspec; ++i) dstab[i] = static_cast<Real>(0.);
  Real stressstab[3][2] = {{0, 0}, {0, 0}, {0, 0}};
  Real stressstabn[3][2] = {{0, 0}, {0, 0}, {0, 0}};

  const Real const1 = p.bbeta / kKappa2;  // :606
  Real const0 = const1 * drat;            // :607

  // 1.a  surface roughness (:611-614); FACLN1/FACLN2 are not used further.
  const Real z0visc = kZ0viscNum / max_r(ustar, static_cast<Real>(0.0001));
  const Real z0noz = max_r(z0visc, p.zz0rat * z0);

  // 1.b  orbital velocity and displacement (:618-638)
  Real uorb = static_cast<Real>(0.);
  Real aorb = static_cast<Real>(0.);
  for (int ik = 1; ik <= nk; ++ik) {
    Real eb = static_cast<Real>(0.);
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      eb = eb + a[is - 1];
    }
    const Real sigk = f.sig(ik);
    uorb = uorb + eb * (sigk * sigk) * f.dden(ik - 1) / cg[ik - 1];
    aorb = aorb + eb * f.dden(ik - 1) / cg[ik - 1];
  }
  uorb = static_cast<Real>(2) * Kokkos::sqrt(uorb);
  // AORB**(1-0.5*SSWELLF(6)): REAL exponent, powf in the dump (:637).
  const Real aorb1 = static_cast<Real>(2) *
                     dm::powf(aorb, static_cast<Real>(1) - static_cast<Real>(0.5) * p.sswellf[5]);
  const Real re = static_cast<Real>(4) * uorb * aorb1 / kNuAir;

  // Swell dissipation regime from the Reynolds number (:642-660)
  Real pturb, pvisc;
  if (p.sswellf[3] > static_cast<Real>(0)) {
    if (p.sswellf[6] > static_cast<Real>(0.)) {
      const Real smooth = static_cast<Real>(0.5) * dm::tanhf((re - p.sswellf[3]) / p.sswellf[6]);
      pturb = (static_cast<Real>(0.5) + smooth);
      pvisc = (static_cast<Real>(0.5) - smooth);
    } else {
      if (re <= p.sswellf[3]) {
        pturb = static_cast<Real>(0.);
        pvisc = static_cast<Real>(1.);
      } else {
        pturb = static_cast<Real>(1.);
        pvisc = static_cast<Real>(0.);
      }
    }
  } else {
    pturb = static_cast<Real>(1.);
    pvisc = static_cast<Real>(1.);
  }

  // :662-675
  Real fw, fu, fud;
  if (p.sswellf[1] == static_cast<Real>(0)) {
    fw = Kokkos::fabs(p.sswellf[2]);  // -O3 drops the MAX(...,0.) of :663 (dump)
    fu = static_cast<Real>(0.);
    fud = static_cast<Real>(0.);
  } else {
    fu = Kokkos::fabs(p.sswellf[2]);
    fud = p.sswellf[1];
    aorb = static_cast<Real>(2) * Kokkos::sqrt(aorb);
    const Real xi = (dm::log10f(max_r(aorb / z0noz, static_cast<Real>(3.))) - kAbMin) / p.delab;
    const int ind = min_i(kSizeFwTable - 1, fint(xi));
    const Real deli1 = min_r(static_cast<Real>(1.), xi - static_cast<Real>(ind));
    const Real deli2 = static_cast<Real>(1.) - deli1;
    fw = f.fwtable(ind) * deli2 + f.fwtable(ind + 1) * deli1;
  }

  // 2.  Diagonal (:677-813)
  Real ustarsigma = static_cast<Real>(0);
  if (p.stab3) {  // W3_STAB3, :683-692
    Real usigma;
    if (Kokkos::isnan(as)) {
      usigma = static_cast<Real>(0.);
    } else {
      usigma = max_r(static_cast<Real>(0.), static_cast<Real>(-0.025) * as);
    }
    ustarsigma = (static_cast<Real>(1.0) + u / (static_cast<Real>(10.) + u)) * usigma;
  }
  m.const1 = const1;
  m.const0 = const0;
  m.uorb = uorb;
  m.pturb = pturb;
  m.pvisc = pvisc;
  m.fw = fw;
  m.fu = fu;
  m.fud = fud;
  m.ustarsigma = ustarsigma;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 2; ++j) {
      m.stressstab[i][j] = stressstab[i][j];
      m.stressstabn[i][j] = stressstabn[i][j];
    }
}

/// The stress scan of section 2 (:696-797): DSTAB, LLWS and the stresses, IK after IK.
KOKKOS_INLINE_FUNCTION void sin4_scan(const Params& p, const Fields& f, const Real* a,
                                      const Real* cg, const Real* k, Real ustar, Real drat,
                                      Real usdir, Real z0, int* llws, Real* work,
                                      sin4_detail::Mid& m) {
  using namespace sin4_detail;
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  Real* dstab = work;
  auto DSTAB = [&](int istab, int is) -> Real& { return dstab[(istab - 1) * nspec + (is - 1)]; };
  Real(&stressstab)[3][2] = m.stressstab;
  Real(&stressstabn)[3][2] = m.stressstabn;
  const Real const0 = m.const0, uorb = m.uorb, pturb = m.pturb, pvisc = m.pvisc, fw = m.fw,
             fu = m.fu, fud = m.fud, ustarsigma = m.ustarsigma;
  Real ust = ustar;
  Real taux = static_cast<Real>(0), tauy = static_cast<Real>(0);
  Real cosu = static_cast<Real>(0), sinu = static_cast<Real>(0);
  Real ustp = static_cast<Real>(0), usdirp = static_cast<Real>(0);
  // Without W3_STAB3 a single pass with ISTAB=3; with it, ISTAB=1,2 (:696-701).
  const int istab_first = p.stab3 ? 1 : 3;
  const int istab_last = p.stab3 ? 2 : 3;
  for (int istab = istab_first; istab <= istab_last; ++istab) {
    if (istab == 1) ust = ustar * (static_cast<Real>(1.) - ustarsigma);
    if (istab == 2) ust = ustar * (static_cast<Real>(1.) + ustarsigma);
    Real cu, su;
    dm::sincosf(usdir, su, cu);
    taux = (ust * ust) * cu;
    tauy = (ust * ust) * su;
    stressstab[istab - 1][0] = static_cast<Real>(0.);
    stressstab[istab - 1][1] = static_cast<Real>(0.);
    stressstabn[istab - 1][0] = static_cast<Real>(0.);
    stressstabn[istab - 1][1] = static_cast<Real>(0.);
    // Sequential scan over IK: each IK reads the stress of the earlier ones (:713-797).
    for (int ik = 1; ik <= nk; ++ik) {
      const Real taupx = taux - Kokkos::fabs(p.ttauwshelter) * stressstab[istab - 1][0];
      const Real taupy = tauy - Kokkos::fabs(p.ttauwshelter) * stressstab[istab - 1][1];
      // (..)**0.25 is powf(x, 0.25) in the dump (:717).
      ustp = min_r(dm::powf((taupx * taupx + taupy * taupy), static_cast<Real>(0.25)),
                   max_r(ust, static_cast<Real>(0.3)));
      usdirp = dm::atan2f(taupy, taupx);
      dm::sincosf(usdirp, sinu, cosu);
      int is = 1 + (ik - 1) * nth;
      const Real kis = k[is - 1];
      const Real sig2is = f.sig2(is - 1);
      const Real cm = kis / sig2is;
      const Real ucn = ustp * cm + p.zzalp;
      const Real const2 = f.dden2(is - 1) / cg[ik - 1] * kGrav / (f.sig(ik) / kis * drat);
      const Real cnst = sig2is * const0;
      const Real zcn = dm::logf(kis * z0);
      // -(...) of the whole product (:737-738); sqrt(2*NU_AIR*SIG2) with 2*NU_AIR folded.
      const Real swellcoefv =
          -(p.sswellf[4] * drat * static_cast<Real>(2) * kis * Kokkos::sqrt(kTwoNuAir * sig2is));
      const Real swellcoeft =
          -(drat * p.sswellf[0] * static_cast<Real>(16) * (sig2is * sig2is) / kGrav);
      for (int ith = 1; ith <= nth; ++ith) {
        is = ith + (ik - 1) * nth;
        const Real ecos = f.ecos(is - 1), esin = f.esin(is - 1);
        const Real coswind = (ecos * cosu + esin * sinu);
        if (coswind > static_cast<Real>(0.01)) {
          const Real x = coswind * ucn;
          const Real zarg = kKappa / x;
          const Real zlog = zcn + zarg;
          if (zlog < static_cast<Real>(0.)) {
            const Real zlog2 = zlog * zlog;
            // COSWIND**SSINTHP: REAL exponent, powf in the dump (:756).
            DSTAB(istab, is) = cnst * dm::expf(zlog) * (zlog2 * zlog2) * ucn * ucn *
                               dm::powf(coswind, p.ssinthp);
            llws[is - 1] = 1;
          } else {
            DSTAB(istab, is) = static_cast<Real>(0.);
            llws[is - 1] = 0;
          }
          // Uses USTAR, not USTP (:769), as ECWAM implsch.F does.
          if (static_cast<Real>(28.) * cm * ustar * coswind >= static_cast<Real>(1)) {
            llws[is - 1] = 1;
          }
        } else {
          DSTAB(istab, is) = static_cast<Real>(0.);
          llws[is - 1] = 0;
        }
        // COSWIND here is the value of this IS even when it is <= 0.01 (:777-783).
        if ((p.sswellf[0] != static_cast<Real>(0) &&
             DSTAB(istab, is) < static_cast<Real>(1E-7) * sig2is) ||
            p.sswellf[2] > static_cast<Real>(0)) {
          const Real dvisc = swellcoefv;
          const Real dturb = swellcoeft * (fw * uorb + (fu + fud * coswind) * ustp);
          DSTAB(istab, is) = DSTAB(istab, is) + pturb * dturb + pvisc * dvisc;
        }
        // Wave-supported stress, "direction to", hence the plus sign (:785-795).
        const Real temp2 = const2 * DSTAB(istab, is) * a[is - 1];
        if (DSTAB(istab, is) < static_cast<Real>(0)) {
          stressstabn[istab - 1][0] = stressstabn[istab - 1][0] + temp2 * ecos;
          stressstabn[istab - 1][1] = stressstabn[istab - 1][1] + temp2 * esin;
        } else {
          stressstab[istab - 1][0] = stressstab[istab - 1][0] + temp2 * ecos;
          stressstab[istab - 1][1] = stressstab[istab - 1][1] + temp2 * esin;
        }
      }
    }
  }
  m.ust = ust;
  m.taux = taux;
  m.tauy = tauy;
  m.cosu = cosu;
  m.sinu = sinu;
  m.ustp = ustp;
  m.usdirp = usdirp;
}

/// sin4_scan for the `n` points of a team at once (1 <= n <= W), one point per vector lane
/// (plan 2026-10-18-005, unit L2). Lane l computes the statements sin4_scan computes for point
/// l, in its order: the loops over IK and ITH stay outside, the loop over the lanes is innermost,
/// each lane keeps its own stress sums, so no sum mixes two points. A branch becomes a select of
/// the value its taken side computes; expf and powf are called per lane, for the lanes whose
/// branch calls them. Lanes n..W-1 repeat point 0's inputs and their results are not stored.
template <int W>
inline void sin4_scan_lanes(const Params& p, const Fields& f, const int n,
                                            const Real* const* a_, const Real* const* cg_,
                                            const Real* const* k_, const Real* ustar_,
                                            const Real* drat_, const Real* usdir_,
                                            const Real* z0_, int* const* llws, Real* const* work,
                                            sin4_detail::Mid* m) {
  using namespace sin4_detail;
  static_assert(W == 8, "the ITH loop is written for V8");
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  const Real* a[W];
  const Real* cg[W];
  const Real* k[W];
  Real ustar[W], drat[W], usdir[W], z0[W];
  Real const0[W], uorb[W], pturb[W], pvisc[W], fw[W], fu[W], fud[W], ustarsigma[W];
  Real stressstab[3][2][W], stressstabn[3][2][W];
  for (int l = 0; l < W; ++l) {
    const int q = l < n ? l : 0;
    a[l] = a_[q];
    cg[l] = cg_[q];
    k[l] = k_[q];
    ustar[l] = ustar_[q];
    drat[l] = drat_[q];
    usdir[l] = usdir_[q];
    z0[l] = z0_[q];
    const0[l] = m[q].const0;
    uorb[l] = m[q].uorb;
    pturb[l] = m[q].pturb;
    pvisc[l] = m[q].pvisc;
    fw[l] = m[q].fw;
    fu[l] = m[q].fu;
    fud[l] = m[q].fud;
    ustarsigma[l] = m[q].ustarsigma;
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 2; ++j) {
        stressstab[i][j][l] = m[q].stressstab[i][j];
        stressstabn[i][j][l] = m[q].stressstabn[i][j];
      }
  }
  Real ust[W], taux[W], tauy[W], cosu[W], sinu[W], ustp[W], usdirp[W];
  for (int l = 0; l < W; ++l) {
    ust[l] = ustar[l];
    taux[l] = static_cast<Real>(0);
    tauy[l] = static_cast<Real>(0);
    cosu[l] = static_cast<Real>(0);
    sinu[l] = static_cast<Real>(0);
    ustp[l] = static_cast<Real>(0);
    usdirp[l] = static_cast<Real>(0);
  }
  // Without W3_STAB3 a single pass with ISTAB=3; with it, ISTAB=1,2 (:696-701).
  const int istab_first = p.stab3 ? 1 : 3;
  const int istab_last = p.stab3 ? 2 : 3;
  for (int istab = istab_first; istab <= istab_last; ++istab) {
    Real(&ss)[2][W] = stressstab[istab - 1];
    Real(&sn)[2][W] = stressstabn[istab - 1];
    for (int l = 0; l < W; ++l) {
      if (istab == 1) ust[l] = ustar[l] * (static_cast<Real>(1.) - ustarsigma[l]);
      if (istab == 2) ust[l] = ustar[l] * (static_cast<Real>(1.) + ustarsigma[l]);
      Real cu, su;
      dm::sincosf(usdir[l], su, cu);
      taux[l] = (ust[l] * ust[l]) * cu;
      tauy[l] = (ust[l] * ust[l]) * su;
      ss[0][l] = static_cast<Real>(0.);
      ss[1][l] = static_cast<Real>(0.);
      sn[0][l] = static_cast<Real>(0.);
      sn[1][l] = static_cast<Real>(0.);
    }
    // Sequential scan over IK: each IK reads the stress of the earlier ones (:713-797).
    for (int ik = 1; ik <= nk; ++ik) {
      const int is0 = 1 + (ik - 1) * nth;
      const Real sig2is = f.sig2(is0 - 1);
      Real cm[W], ucn[W], const2[W], cnst[W], zcn[W], swellcoefv[W], swellcoeft[W];
      for (int l = 0; l < W; ++l) {
        const Real taupx = taux[l] - Kokkos::fabs(p.ttauwshelter) * ss[0][l];
        const Real taupy = tauy[l] - Kokkos::fabs(p.ttauwshelter) * ss[1][l];
        // (..)**0.25 is powf(x, 0.25) in the dump (:717).
        ustp[l] = min_r(dm::powf((taupx * taupx + taupy * taupy), static_cast<Real>(0.25)),
                        max_r(ust[l], static_cast<Real>(0.3)));
        usdirp[l] = dm::atan2f(taupy, taupx);
        dm::sincosf(usdirp[l], sinu[l], cosu[l]);
        const Real kis = k[l][is0 - 1];
        cm[l] = kis / sig2is;
        ucn[l] = ustp[l] * cm[l] + p.zzalp;
        const2[l] = f.dden2(is0 - 1) / cg[l][ik - 1] * kGrav / (f.sig(ik) / kis * drat[l]);
        cnst[l] = sig2is * const0[l];
        zcn[l] = dm::logf(kis * z0[l]);
        // -(...) of the whole product (:737-738); sqrt(2*NU_AIR*SIG2) with 2*NU_AIR folded.
        swellcoefv[l] = -(p.sswellf[4] * drat[l] * static_cast<Real>(2) * kis *
                          Kokkos::sqrt(kTwoNuAir * sig2is));
        swellcoeft[l] =
            -(drat[l] * p.sswellf[0] * static_cast<Real>(16) * (sig2is * sig2is) / kGrav);
      }
      // The ITH loop on 8-wide vectors, lane l being point l. Each vector operation is the IEEE
      // operation of the scalar statement it replaces, lane by lane, with its operands in the
      // same order; a comparison gives a lane mask and `m ? x : y` selects per lane. ss and sn
      // stay in registers for the whole ITH loop, one sum per lane, added in ITH order.
      V8 cosu_v, sinu_v, ucn_v, zcn_v, cnst_v, cm_v, ustar_v, swc_v, swt_v, fw_v, uorb_v, fu_v,
         fud_v, ustp_v, pturb_v, pvisc_v, const2_v;
      vload(cosu_v, cosu);
      vload(sinu_v, sinu);
      vload(ucn_v, ucn);
      vload(zcn_v, zcn);
      vload(cnst_v, cnst);
      vload(cm_v, cm);
      vload(ustar_v, ustar);
      vload(swc_v, swellcoefv);
      vload(swt_v, swellcoeft);
      vload(fw_v, fw);
      vload(uorb_v, uorb);
      vload(fu_v, fu);
      vload(fud_v, fud);
      vload(ustp_v, ustp);
      vload(pturb_v, pturb);
      vload(pvisc_v, pvisc);
      vload(const2_v, const2);
      const M8 s0nz = p.sswellf[0] != static_cast<Real>(0) ? ~M8{} : M8{};
      const M8 s2pos = p.sswellf[2] > static_cast<Real>(0) ? ~M8{} : M8{};
      V8 sn0, sn1, ss0, ss1;
      vload(sn0, sn[0]);
      vload(sn1, sn[1]);
      vload(ss0, ss[0]);
      vload(ss1, ss[1]);
      for (int ith = 1; ith <= nth; ++ith) {
        const int is = ith + (ik - 1) * nth;
        const Real ecos = f.ecos(is - 1), esin = f.esin(is - 1);
        const V8 coswind = (ecos * cosu_v + esin * sinu_v);
        const M8 wind = coswind > static_cast<Real>(0.01);
        const V8 x = coswind * ucn_v;
        const V8 zarg = kKappa / x;
        const V8 zlog = zcn_v + zarg;
        const M8 grow = wind & (zlog < static_cast<Real>(0.));
        V8 ex = {}, pw = {};
        for (int l = 0; l < W; ++l)
          if (grow[l]) {
            ex[l] = dm::expf(zlog[l]);
            // COSWIND**SSINTHP: REAL exponent, powf in the dump (:756).
            pw[l] = dm::powf(coswind[l], p.ssinthp);
          }
        const V8 zlog2 = zlog * zlog;
        const V8 grown = cnst_v * ex * (zlog2 * zlog2) * ucn_v * ucn_v * pw;
        V8 dst = grow ? grown : V8{};
        // Uses USTAR, not USTP (:769), as ECWAM implsch.F does.
        const M8 gust =
            wind & (static_cast<Real>(28.) * cm_v * ustar_v * coswind >= static_cast<Real>(1));
        const M8 lw = grow | gust;
        // COSWIND here is the value of this IS even when it is <= 0.01 (:777-783).
        const M8 swell = (s0nz & (dst < static_cast<Real>(1E-7) * sig2is)) | s2pos;
        const V8 dturb = swt_v * (fw_v * uorb_v + (fu_v + fud_v * coswind) * ustp_v);
        const V8 withswell = dst + pturb_v * dturb + pvisc_v * swc_v;
        dst = swell ? withswell : dst;
        // Wave-supported stress, "direction to", hence the plus sign (:785-795).
        V8 av;
        for (int l = 0; l < W; ++l) av[l] = a[l][is - 1];
        const V8 temp2 = const2_v * dst * av;
        const M8 neg = dst < static_cast<Real>(0);
        sn0 = neg ? sn0 + temp2 * ecos : sn0;
        sn1 = neg ? sn1 + temp2 * esin : sn1;
        ss0 = neg ? ss0 : ss0 + temp2 * ecos;
        ss1 = neg ? ss1 : ss1 + temp2 * esin;
        for (int l = 0; l < n; ++l) {
          work[l][(istab - 1) * nspec + (is - 1)] = dst[l];
          llws[l][is - 1] = lw[l] ? 1 : 0;
        }
      }
      vstore(sn[0], sn0);
      vstore(sn[1], sn1);
      vstore(ss[0], ss0);
      vstore(ss[1], ss1);
    }
  }
  for (int l = 0; l < n; ++l) {
    m[l].ust = ust[l];
    m[l].taux = taux[l];
    m[l].tauy = tauy[l];
    m[l].cosu = cosu[l];
    m[l].sinu = sinu[l];
    m[l].ustp = ustp[l];
    m[l].usdirp = usdirp[l];
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 2; ++j) {
        m[l].stressstab[i][j] = stressstab[i][j][l];
        m[l].stressstabn[i][j] = stressstabn[i][j][l];
      }
  }
}

/// The rest of W3SIN4 (:799-976): D and S, the tail stress and the stress limit.
KOKKOS_INLINE_FUNCTION void sin4_tail(const Params& p, const Fields& f, const Real* a,
                                      const Real* cg, Real ustar, Real z0, Real& tauwx,
                                      Real& tauwy, Real& tauwnx, Real& tauwny, Real* s, Real* d,
                                      Real* work, sin4_detail::Mid& m) {
  using namespace sin4_detail;
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  Real* dstab = work;
  auto DSTAB = [&](int istab, int is) -> Real& { return dstab[(istab - 1) * nspec + (is - 1)]; };
  Real(&stressstab)[3][2] = m.stressstab;
  Real(&stressstabn)[3][2] = m.stressstabn;
  const Real const1 = m.const1;
  Real const0 = m.const0;
  Real ust = m.ust, taux = m.taux, tauy = m.tauy, cosu = m.cosu, sinu = m.sinu, ustp = m.ustp,
       usdirp = m.usdirp;
  Real xstress, ystress;
  if (!p.stab3) {  // :799-803
    for (int is = 1; is <= nspec; ++is) d[is - 1] = DSTAB(3, is);
    xstress = stressstab[2][0];
    ystress = stressstab[2][1];
    tauwnx = stressstabn[2][0];
    tauwny = stressstabn[2][1];
  } else {
    // :806-812; the in-loop copy of row 3 (:799-803) is overwritten here, so it is skipped.
    for (int is = 1; is <= nspec; ++is)
      d[is - 1] = static_cast<Real>(0.5) * (DSTAB(1, is) + DSTAB(2, is));
    xstress = static_cast<Real>(0.5) * (stressstab[0][0] + stressstab[1][0]);
    ystress = static_cast<Real>(0.5) * (stressstab[0][1] + stressstab[1][1]);
    tauwnx = static_cast<Real>(0.5) * (stressstabn[0][0] + stressstabn[1][0]);
    tauwny = static_cast<Real>(0.5) * (stressstabn[0][1] + stressstabn[1][1]);
  }
  for (int is = 1; is <= nspec; ++is) s[is - 1] = d[is - 1] * a[is - 1];  // :817

  // TAUX, TAUY are those of the last ISTAB pass (:835-836); USTP has no MIN here (:837).
  {
    const Real taupx = taux - Kokkos::fabs(p.ttauwshelter) * xstress;
    const Real taupy = tauy - Kokkos::fabs(p.ttauwshelter) * ystress;
    ustp = dm::powf((taupx * taupx + taupy * taupy), static_cast<Real>(0.25));  // :837, no MIN
    usdirp = dm::atan2f(taupy, taupx);
  }
  ust = ustp;

  // High-frequency tail (:847-854)
  {
    const Real signk = f.sig(nk);
    const Real s2 = signk * signk;
    const Real s5 = s2 * (s2 * signk);  // SIG(NK)**5 as x2*x3 in the dump
    const0 = p.dth * s5 / kGrav2Tpi * kTpi * signk / cg[nk - 1];
  }
  Real temp = static_cast<Real>(0.);
  for (int ith = 1; ith <= nth; ++ith) {
    const int is = ith + (nk - 1) * nth;
    // COSU/SINU are from the last IK=NK pass (:719-720), not from USDIRP of :838.
    const Real coswind = (f.ecos(is - 1) * cosu + f.esin(is - 1) * sinu);
    const Real cp = (max_r(coswind, static_cast<Real>(0.)));
    temp = temp + a[is - 1] * (cp * (cp * cp));
  }
  const Real levtail0 = const0 * temp;

  Real tauhf;
  if (p.sintailpar[0] < static_cast<Real>(0.5)) {
    // Analytic tail: a recurrence over J=1..JTOT through USTR and TAUR (:862-930).
    Real w[kJtot];  // ALLOCATE(W(JTOT)) at :863
    for (int j = 2; j <= kJtot - 1; ++j) w[j - 1] = static_cast<Real>(1.);
    w[0] = static_cast<Real>(0.5);
    w[kJtot - 1] = static_cast<Real>(0.5);
    // X0, OMEGACC and YC (:867-872) are dead stores and are not computed.
    Real ustr = ust;
    const Real zz0 = z0;
    const Real signk = f.sig(nk);
    const Real zinf = dm::logf(signk * signk / kGrav);
    const Real dely = (kLogTpiOver005 - zinf) / static_cast<Real>(kJtot);
    Real taur = ust * ust;
    Real tau1 = static_cast<Real>(0.);
    for (int j = 1; j <= kJtot; ++j) {
      const Real y = dm::expf(zinf + static_cast<Real>(j - 1) * dely);
      const Real ykm = (y / kKm);
      const Real ykm2 = ykm * ykm;
      const Real tensk = static_cast<Real>(1) + ykm2;
      // OMEGA (:894) is never read and is not computed.
      const Real cm = Kokkos::sqrt(kGrav * tensk / y);
      const Real cgtail = static_cast<Real>(0.5) * (static_cast<Real>(3) * ykm2 + static_cast<Real>(1)) *
                          Kokkos::sqrt(kGrav / (y * tensk));
      const Real zx = ustr / cm + p.zzalp;
      const Real zarg = min_r(kKappa / zx, static_cast<Real>(20.));
      const Real zmu = min_r(kGrav * zz0 / (cm * cm) * dm::expf(zarg), static_cast<Real>(1.));
      const Real zlog = min_r(dm::logf(zmu), static_cast<Real>(0.));
      const Real zlog2 = zlog * zlog;
      const Real zbeta = const1 * zmu * (zlog2 * zlog2);
      Real levtail;
      if (p.sintailpar[2] > static_cast<Real>(0)) {  // capillary peak, :907-919
        Real alpham;
        if (ustr < cm) {
          alpham = max_r(static_cast<Real>(0.),
                         static_cast<Real>(0.01) * (static_cast<Real>(1.) + dm::logf(ustr / cm)));
        } else {
          alpham = static_cast<Real>(0.01) * (static_cast<Real>(1) + static_cast<Real>(3.) * dm::logf(ustr / cm));
        }
        const Real ykm1 = (ykm - static_cast<Real>(1));
        const Real fm = dm::expf(static_cast<Real>(-0.25) * (ykm1 * ykm1));
        const Real alphat = alpham * (kCmm / cm) * fm;
        const Real th = dm::tanhf((y - static_cast<Real>(20)) / static_cast<Real>(5));
        levtail = levtail0 * static_cast<Real>(0.5) * (static_cast<Real>(1) - th) +
                  p.sintailpar[2] * static_cast<Real>(0.5) * (static_cast<Real>(1) + th) * alphat;
      } else {
        levtail = levtail0;
      }
      tau1 = tau1 + w[j - 1] * zbeta * (ustr * ustr) * levtail * dely * cgtail / cm;
      taur = taur - w[j - 1] * p.sintailpar[1] * (ustr * ustr) * zbeta * levtail * dely * cgtail / cm;
      ustr = Kokkos::sqrt(max_r(taur, static_cast<Real>(0.)));
    }
    tauhf = tau1;
  } else {
    // Tabulated tail stress TAUHFT / TAUHFT2 (:936-962); tables are 0-based like the Fortran.
    const Real xi = ust / p.delust;
    const int ind = max_i(1, min_i(kIustar - 1, fint(xi)));
    const Real deli1 = max_r(min_r(static_cast<Real>(1.), xi - static_cast<Real>(ind)), static_cast<Real>(0.));
    const Real deli2 = static_cast<Real>(1.) - deli1;
    const Real ustm = max_r(ust, static_cast<Real>(0.00001));
    const Real xj = max_r(static_cast<Real>(0.), (kGrav * z0 / (ustm * ustm) - p.aalpha) / p.delalp);
    const int j = max_i(1, min_i(kIalpha - 1, fint(xj)));
    const Real delj1 = max_r(static_cast<Real>(0.), min_r(static_cast<Real>(1.), xj - static_cast<Real>(j)));
    const Real delj2 = static_cast<Real>(1.) - delj1;
    Real tau1;
    if (p.ttauwshelter > static_cast<Real>(0)) {
      const Real xk = levtail0 / p.deltail;
      const int i = min_i(kIlevTail - 1, fint(xk));
      const Real delk1 = min_r(static_cast<Real>(1.), xk - static_cast<Real>(i));
      const Real delk2 = static_cast<Real>(1.) - delk1;
      const auto& t2 = f.tauhft2;
      tau1 = ((t2(ind, j, i) * deli2 + t2(ind + 1, j, i) * deli1) * delj2 +
              (t2(ind, j + 1, i) * deli2 + t2(ind + 1, j + 1, i) * deli1) * delj1) * delk2 +
             ((t2(ind, j, i + 1) * deli2 + t2(ind + 1, j, i + 1) * deli1) * delj2 +
              (t2(ind, j + 1, i + 1) * deli2 + t2(ind + 1, j + 1, i + 1) * deli1) * delj1) * delk1;
    } else {
      const auto& t = f.tauhft;
      tau1 = (t(ind, j) * deli2 + t(ind + 1, j) * deli1) * delj2 +
             (t(ind, j + 1) * deli2 + t(ind + 1, j + 1) * deli1) * delj1;
    }
    tauhf = levtail0 * (ust * ust) * tau1;
  }

  // USDIRP of :838 here (:964-965).
  Real cp, sp;
  dm::sincosf(usdirp, sp, cp);
  tauwx = xstress + tauhf * cp;
  tauwy = ystress + tauhf * sp;
  // Tail reduced so the wave-supported stress stays below the total, ECWAM Stresso.F (:970-976).
  const Real tauw = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);
  const Real ust2b = max_r(ustar, kEps2);
  const Real ust2 = ust2b * ust2b;
  const Real tauwb = min_r(tauw, max_r(ust2 - kEps1, kEps2Sq));
  if (tauwb < tauw) {
    tauwx = tauwx * tauwb / tauw;
    tauwy = tauwy * tauwb / tauw;
  }
}

/// W3SIN4 for one point. a = A(1:NSPEC), cg = CG(1:NK), k = K(1:NSPEC) (WN2),
/// s, d = S, D(1:NSPEC), llws = LLWS(1:NSPEC) as 0/1, all 0-based. IX, IY and
/// BRLAMBDA are not read by the routine and are not passed. The OUT arguments
/// arrive holding the caller's values.
KOKKOS_INLINE_FUNCTION void w3sin4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* cg, const Real* k, Real u, Real ustar,
                                         Real drat, Real as, Real usdir, Real z0, Real cd,
                                         Real& tauwx, Real& tauwy, Real& tauwnx, Real& tauwny,
                                         Real* s, Real* d, int* llws, Real* work, int* iwork) {
  (void)cd;     // CD is an argument of W3SIN4 but never read.
  (void)iwork;
  sin4_detail::Mid m;
  sin4_prep(p, f, a, cg, u, ustar, drat, as, z0, work, m);
  sin4_scan(p, f, a, cg, k, ustar, drat, usdir, z0, llws, work, m);
  sin4_tail(p, f, a, cg, ustar, z0, tauwx, tauwy, tauwnx, tauwny, s, d, work, m);
}

/// W3SIN4 for the `n` points of a team (1 <= n <= W), each argument an array of the n points'
/// arguments of w3sin4_point: section 1 and the rest per point, the stress scan with one point
/// per vector lane (sin4_scan_lanes). Every point gets the bits w3sin4_point gives it.
template <int W>
inline void w3sin4_lanes(const Params& p, const Fields& f, const int n,
                                         const Real* const* a, const Real* const* cg,
                                         const Real* const* k, const Real* u, const Real* ustar,
                                         const Real* drat, const Real* as, const Real* usdir,
                                         const Real* z0, Real* const* tauwx, Real* const* tauwy,
                                         Real* const* tauwnx, Real* const* tauwny, Real* const* s,
                                         Real* const* d, int* const* llws, Real* const* work) {
  sin4_detail::Mid m[W];
  for (int l = 0; l < n; ++l)
    sin4_prep(p, f, a[l], cg[l], u[l], ustar[l], drat[l], as[l], z0[l], work[l], m[l]);
  sin4_scan_lanes<W>(p, f, n, a, cg, k, ustar, drat, usdir, z0, llws, work, m);
  for (int l = 0; l < n; ++l)
    sin4_tail(p, f, a[l], cg[l], ustar[l], z0[l], *tauwx[l], *tauwy[l], *tauwnx[l], *tauwny[l],
              s[l], d[l], work[l], m[l]);
}

}  // namespace ww::st4
