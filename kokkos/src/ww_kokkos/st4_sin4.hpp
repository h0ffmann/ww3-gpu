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

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"

namespace ww::st4 {

namespace sin4_detail {

// Fortran MAX/MIN of two REALs (GIMPLE MAX_EXPR/MIN_EXPR). NaN operands are
// not reproduced ⚠: MAX_EXPR leaves the NaN result unspecified.
KOKKOS_INLINE_FUNCTION Real fmax_(Real a, Real b) { return a > b ? a : b; }
KOKKOS_INLINE_FUNCTION Real fmin_(Real a, Real b) { return a < b ? a : b; }
KOKKOS_INLINE_FUNCTION int imax_(int a, int b) { return a > b ? a : b; }
KOKKOS_INLINE_FUNCTION int imin_(int a, int b) { return a < b ? a : b; }

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

}  // namespace sin4_detail

/// Reals and ints of per-point work storage w3sin4_point needs (no allocation in a kernel).
/// DSTAB(3,NSPEC), w3src4md.F90:567 (declared), :600 (zeroed), stored as dstab[(ISTAB-1)*NSPEC + IS-1].
inline int sin4_work_reals(const Params& p) { return 3 * p.nspec; }
inline int sin4_work_ints(const Params& p) { (void)p; return 0; }

/// W3SIN4 for one point. a = A(1:NSPEC), cg = CG(1:NK), k = K(1:NSPEC) (WN2),
/// s, d = S, D(1:NSPEC), llws = LLWS(1:NSPEC) as 0/1, all 0-based. IX, IY and
/// BRLAMBDA are not read by the routine and are not passed. The OUT arguments
/// arrive holding the caller's values.
KOKKOS_INLINE_FUNCTION void w3sin4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* cg, const Real* k, Real u, Real ustar,
                                         Real drat, Real as, Real usdir, Real z0, Real cd,
                                         Real& tauwx, Real& tauwy, Real& tauwnx, Real& tauwny,
                                         Real* s, Real* d, int* llws, Real* work, int* iwork) {
  using namespace sin4_detail;
  (void)cd;     // CD is an argument of W3SIN4 but never read.
  (void)iwork;
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  Real* dstab = work;
  auto DSTAB = [&](int istab, int is) -> Real& { return dstab[(istab - 1) * nspec + (is - 1)]; };

  // 1.  Preparations (:600-602)
  for (int i = 0; i < 3 * nspec; ++i) dstab[i] = static_cast<Real>(0.);
  Real stressstab[3][2] = {{0, 0}, {0, 0}, {0, 0}};
  Real stressstabn[3][2] = {{0, 0}, {0, 0}, {0, 0}};

  const Real const1 = p.bbeta / kKappa2;  // :606
  Real const0 = const1 * drat;            // :607

  // 1.a  surface roughness (:611-614); FACLN1/FACLN2 are not used further.
  const Real z0visc = kZ0viscNum / fmax_(ustar, static_cast<Real>(0.0001));
  const Real z0noz = fmax_(z0visc, p.zz0rat * z0);

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
                     Kokkos::pow(aorb, static_cast<Real>(1) - static_cast<Real>(0.5) * p.sswellf[5]);
  const Real re = static_cast<Real>(4) * uorb * aorb1 / kNuAir;

  // Swell dissipation regime from the Reynolds number (:642-660)
  Real pturb, pvisc;
  if (p.sswellf[3] > static_cast<Real>(0)) {
    if (p.sswellf[6] > static_cast<Real>(0.)) {
      const Real smooth = static_cast<Real>(0.5) * Kokkos::tanh((re - p.sswellf[3]) / p.sswellf[6]);
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
    const Real xi = (Kokkos::log10(fmax_(aorb / z0noz, static_cast<Real>(3.))) - kAbMin) / p.delab;
    const int ind = imin_(kSizeFwTable - 1, fint(xi));
    const Real deli1 = fmin_(static_cast<Real>(1.), xi - static_cast<Real>(ind));
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
      usigma = fmax_(static_cast<Real>(0.), static_cast<Real>(-0.025) * as);
    }
    ustarsigma = (static_cast<Real>(1.0) + u / (static_cast<Real>(10.) + u)) * usigma;
  }
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
    taux = (ust * ust) * Kokkos::cos(usdir);
    tauy = (ust * ust) * Kokkos::sin(usdir);
    stressstab[istab - 1][0] = static_cast<Real>(0.);
    stressstab[istab - 1][1] = static_cast<Real>(0.);
    stressstabn[istab - 1][0] = static_cast<Real>(0.);
    stressstabn[istab - 1][1] = static_cast<Real>(0.);
    // Sequential scan over IK: each IK reads the stress of the earlier ones (:713-797).
    for (int ik = 1; ik <= nk; ++ik) {
      const Real taupx = taux - Kokkos::fabs(p.ttauwshelter) * stressstab[istab - 1][0];
      const Real taupy = tauy - Kokkos::fabs(p.ttauwshelter) * stressstab[istab - 1][1];
      // (..)**0.25 is powf(x, 0.25) in the dump (:717).
      ustp = fmin_(Kokkos::pow((taupx * taupx + taupy * taupy), static_cast<Real>(0.25)),
                   fmax_(ust, static_cast<Real>(0.3)));
      usdirp = Kokkos::atan2(taupy, taupx);
      cosu = Kokkos::cos(usdirp);
      sinu = Kokkos::sin(usdirp);
      int is = 1 + (ik - 1) * nth;
      const Real kis = k[is - 1];
      const Real sig2is = f.sig2(is - 1);
      const Real cm = kis / sig2is;
      const Real ucn = ustp * cm + p.zzalp;
      const Real const2 = f.dden2(is - 1) / cg[ik - 1] * kGrav / (f.sig(ik) / kis * drat);
      const Real cnst = sig2is * const0;
      const Real zcn = Kokkos::log(kis * z0);
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
            DSTAB(istab, is) = cnst * Kokkos::exp(zlog) * (zlog2 * zlog2) * ucn * ucn *
                               Kokkos::pow(coswind, p.ssinthp);
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
    ustp = Kokkos::pow((taupx * taupx + taupy * taupy), static_cast<Real>(0.25));  // :837, no MIN
    usdirp = Kokkos::atan2(taupy, taupx);
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
    const Real cp = (fmax_(coswind, static_cast<Real>(0.)));
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
    const Real zinf = Kokkos::log(signk * signk / kGrav);
    const Real dely = (kLogTpiOver005 - zinf) / static_cast<Real>(kJtot);
    Real taur = ust * ust;
    Real tau1 = static_cast<Real>(0.);
    for (int j = 1; j <= kJtot; ++j) {
      const Real y = Kokkos::exp(zinf + static_cast<Real>(j - 1) * dely);
      const Real ykm = (y / kKm);
      const Real ykm2 = ykm * ykm;
      const Real tensk = static_cast<Real>(1) + ykm2;
      // OMEGA (:894) is never read and is not computed.
      const Real cm = Kokkos::sqrt(kGrav * tensk / y);
      const Real cgtail = static_cast<Real>(0.5) * (static_cast<Real>(3) * ykm2 + static_cast<Real>(1)) *
                          Kokkos::sqrt(kGrav / (y * tensk));
      const Real zx = ustr / cm + p.zzalp;
      const Real zarg = fmin_(kKappa / zx, static_cast<Real>(20.));
      const Real zmu = fmin_(kGrav * zz0 / (cm * cm) * Kokkos::exp(zarg), static_cast<Real>(1.));
      const Real zlog = fmin_(Kokkos::log(zmu), static_cast<Real>(0.));
      const Real zlog2 = zlog * zlog;
      const Real zbeta = const1 * zmu * (zlog2 * zlog2);
      Real levtail;
      if (p.sintailpar[2] > static_cast<Real>(0)) {  // capillary peak, :907-919
        Real alpham;
        if (ustr < cm) {
          alpham = fmax_(static_cast<Real>(0.),
                         static_cast<Real>(0.01) * (static_cast<Real>(1.) + Kokkos::log(ustr / cm)));
        } else {
          alpham = static_cast<Real>(0.01) * (static_cast<Real>(1) + static_cast<Real>(3.) * Kokkos::log(ustr / cm));
        }
        const Real ykm1 = (ykm - static_cast<Real>(1));
        const Real fm = Kokkos::exp(static_cast<Real>(-0.25) * (ykm1 * ykm1));
        const Real alphat = alpham * (kCmm / cm) * fm;
        const Real th = Kokkos::tanh((y - static_cast<Real>(20)) / static_cast<Real>(5));
        levtail = levtail0 * static_cast<Real>(0.5) * (static_cast<Real>(1) - th) +
                  p.sintailpar[2] * static_cast<Real>(0.5) * (static_cast<Real>(1) + th) * alphat;
      } else {
        levtail = levtail0;
      }
      tau1 = tau1 + w[j - 1] * zbeta * (ustr * ustr) * levtail * dely * cgtail / cm;
      taur = taur - w[j - 1] * p.sintailpar[1] * (ustr * ustr) * zbeta * levtail * dely * cgtail / cm;
      ustr = Kokkos::sqrt(fmax_(taur, static_cast<Real>(0.)));
    }
    tauhf = tau1;
  } else {
    // Tabulated tail stress TAUHFT / TAUHFT2 (:936-962); tables are 0-based like the Fortran.
    const Real xi = ust / p.delust;
    const int ind = imax_(1, imin_(kIustar - 1, fint(xi)));
    const Real deli1 = fmax_(fmin_(static_cast<Real>(1.), xi - static_cast<Real>(ind)), static_cast<Real>(0.));
    const Real deli2 = static_cast<Real>(1.) - deli1;
    const Real ustm = fmax_(ust, static_cast<Real>(0.00001));
    const Real xj = fmax_(static_cast<Real>(0.), (kGrav * z0 / (ustm * ustm) - p.aalpha) / p.delalp);
    const int j = imax_(1, imin_(kIalpha - 1, fint(xj)));
    const Real delj1 = fmax_(static_cast<Real>(0.), fmin_(static_cast<Real>(1.), xj - static_cast<Real>(j)));
    const Real delj2 = static_cast<Real>(1.) - delj1;
    Real tau1;
    if (p.ttauwshelter > static_cast<Real>(0)) {
      const Real xk = levtail0 / p.deltail;
      const int i = imin_(kIlevTail - 1, fint(xk));
      const Real delk1 = fmin_(static_cast<Real>(1.), xk - static_cast<Real>(i));
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
  tauwx = xstress + tauhf * Kokkos::cos(usdirp);
  tauwy = ystress + tauhf * Kokkos::sin(usdirp);
  // Tail reduced so the wave-supported stress stays below the total, ECWAM Stresso.F (:970-976).
  const Real tauw = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);
  const Real ust2b = fmax_(ustar, kEps2);
  const Real ust2 = ust2b * ust2b;
  const Real tauwb = fmin_(tauw, fmax_(ust2 - kEps1, kEps2Sq));
  if (tauwb < tauw) {
    tauwx = tauwx * tauwb / tauw;
    tauwy = tauwy * tauwb / tauw;
  }
}

}  // namespace ww::st4
