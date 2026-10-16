// kokkos/src/ww_kokkos/srce_st3_sin3.hpp
// W3SIN3: WAM4+ (Janssen) wind input source term, its diagonal and the wave-supported
// stress, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src3md.F90:384-717 at NOAA-EMC/WW3 03adda91 (module
// table TAUHFT, DELUST, DELALP :96-99); authors F. Ardhuin, H. L. Tolman.
// Evidence: kokkos/port_report/W3SIN3_KOKKOS.md.
// kStab3 selects the W3_STAB3 blocks (:557-560, :563-567, :642-649): two passes with the
// friction velocity moved by the gustiness spread, averaged. false is STAB0.
// TAUHFT, DELUST and DELALP are what TABU_TAUHF builds (srce_st3_tabu_tauhf.hpp).
// ⚠ On a device sincosf, logf, expf, powf and sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st3_tabu_tauhf.hpp"
#include "st4_params.hpp"

namespace ww::srce::st3 {

namespace sin3_detail {
// minss/maxss of w3src3md.o, d the destination: a NaN in d or a tie gives s.
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
KOKKOS_INLINE_FUNCTION int imin(int a, int b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION int imax(int a, int b) { return (a > b) ? a : b; }

inline constexpr Real kKappa = static_cast<Real>(0.40);   // KAPPA, constants.F90:70
inline constexpr Real kEps1 = static_cast<Real>(0.00001);  // EPS1 :512
inline constexpr Real kEps2 = static_cast<Real>(0.000001); // EPS2 :512
// Folded by gfortran (values in .rodata.cst4 of w3src3md.o):
inline constexpr Real kKappa2 = 0x1.47ae16p-3f;      // KAPPA**2 :575
inline constexpr Real kGrav2Tpi = 0x1.2e168ep+9f;    // (GRAV**2)*TPI :671
inline constexpr Real kEps2Sq = 0x1.197998p-40f;     // EPS2**2 :701
static_assert(kKappa2 == kKappa * kKappa);
static_assert(kGrav2Tpi == (ww::srce::kGrav * ww::srce::kGrav) * ww::srce::kTpi);
static_assert(kEps2Sq == kEps2 * kEps2);

// :700, MAX(USTAR,EPS2): STAB0's object has the constant as d, STAB3's has USTAR.
template <bool kStab3>
KOKKOS_INLINE_FUNCTION Real max_ustar_eps2(Real ustar) {
  if constexpr (kStab3) {
    return maxss(ustar, kEps2);
  } else {
    return maxss(kEps2, ustar);
  }
}
}  // namespace sin3_detail

/// Reals of per-point work storage w3sin3_point needs: the automatic array DSTAB(3,NSPEC)
/// (:526), element (ISTAB,IS) at dstab[(ISTAB-1) + 3*(IS-1)].
inline int sin3_work_reals(const Params& p) { return 3 * p.nspec; }

/// W3SIN3 (w3src3md.F90:384-717) for one point, arrays 0-based.
/// a, k, s, d: (NSPEC); cg: CG(1:NK); llws: LLWS(1:NSPEC) as 0/1. W3SRCE passes WN2 as k
/// and TAUWAX, TAUWAY as tauwnx, tauwny (w3srcemd.F90:1059-1061). CD, ICE, IX, IY are
/// arguments the routine never reads.
/// Grid arrays of W3GDATMD from their first element used: sig = SIG(1:NK) (SIG is
/// (0:NK+1), so pass &SIG(1)), sig2, dden2, ecos, esin = SIG2, DDEN2, ECOS, ESIN(1:NSPEC).
/// dth, bbeta, zzalp, sswellf1 = SSWELLF(1), ssinthp, aalpha: W3GDATMD scalars.
/// delust, delalp, tauhft: W3SRC3MD's DELUST, DELALP, TAUHFT(0:100,0:200) flat in Fortran
/// column-major order, TAUHFT(i,j) at tauhft[i + 101*j].
/// dstab: sin3_work_reals(p) reals of work.
template <bool kStab3>
KOKKOS_INLINE_FUNCTION void w3sin3_point(const Real* a, const Real* cg, const Real* k, Real u,
                                         Real ustar, Real drat, Real as, Real usdir, Real z0,
                                         Real cd, Real& tauwx, Real& tauwy, Real& tauwnx,
                                         Real& tauwny, Real ice, Real* s, Real* d, int* llws,
                                         int ix, int iy, const Params& p, const Real* sig,
                                         const Real* sig2, const Real* dden2, const Real* ecos,
                                         const Real* esin, Real dth, Real bbeta, Real zzalp,
                                         Real sswellf1, Real ssinthp, Real aalpha, Real delust,
                                         Real delalp, const Real* tauhft, Real* dstab) {
  using sin3_detail::imax;
  using sin3_detail::imin;
  using sin3_detail::kKappa;
  using sin3_detail::maxss;
  using sin3_detail::minss;
  using ww::srce::kGrav;
  using ww::srce::kTpi;
  using ww::st4::fint;
  (void)cd;
  (void)ice;
  (void)ix;
  (void)iy;
  (void)u;
  (void)as;
  constexpr int kLd = kTauhfIustar + 1;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);

  // :545
  for (int i = 0; i < 3 * nspec; ++i) dstab[i] = zero;
  Real stressstab[3][2] = {{zero, zero}, {zero, zero}, {zero, zero}};
  Real stressstabn[3][2] = {{zero, zero}, {zero, zero}, {zero, zero}};
  // Z0VISC (:549) and TAUX, TAUY (:568-569) are never read; -O3 drops them.

  Real ustarsigma = zero;
  if constexpr (kStab3) {
    const Real usigma = maxss(-(static_cast<Real>(0.025) * as), zero);              // :558
    ustarsigma = (one + u / (static_cast<Real>(10.) + u)) * usigma;                   // :559
  }
  Real ust = ustar;
  constexpr int istab_first = kStab3 ? 1 : 3;
  constexpr int istab_last = kStab3 ? 2 : 3;
  Real cosu = zero;
  Real sinu = zero;
  Real xstress = zero;
  Real ystress = zero;
  for (int istab = istab_first; istab <= istab_last; ++istab) {
    if constexpr (kStab3) {
      if (istab == 1) ust = ustar * (one - ustarsigma);  // :565
      if (istab == 2) ust = ustar * (one + ustarsigma);  // :566
    }
    stressstab[istab - 1][0] = zero;  // :573-574
    stressstab[istab - 1][1] = zero;
    stressstabn[istab - 1][0] = zero;
    stressstabn[istab - 1][1] = zero;
    const Real const0 = bbeta * drat / sin3_detail::kKappa2;                     // :575
    const Real const3 = sswellf1 * static_cast<Real>(2.) * kKappa * drat;        // :577
    cosu = Kokkos::cos(usdir);  // :579-580
    sinu = Kokkos::sin(usdir);
    for (int ik = 1; ik <= nk; ++ik) {
      int is = 1 + (ik - 1) * nth;
      const Real cm = k[is - 1] / sig2[is - 1];  // :583
      const Real ucn = ust * cm + zzalp;         // :584
      const Real const2 =
          dden2[is - 1] / cg[ik - 1] * kGrav / (sig[ik - 1] / k[is - 1] * drat);  // :591-592
      const Real cnst = sig2[is - 1] * const0;                                    // :593
      const Real zcn = Kokkos::log(k[is - 1] * z0);                               // :597
      for (int ith = 1; ith <= nth; ++ith) {
        is = ith + (ik - 1) * nth;
        Real& ds = dstab[(istab - 1) + 3 * (is - 1)];
        const Real coswind = ecos[is - 1] * cosu + esin[is - 1] * sinu;  // :602
        if (coswind > static_cast<Real>(0.01)) {
          const Real x = coswind * ucn;     // :604
          const Real zarg = kKappa / x;     // :607
          const Real zlog = zcn + zarg;     // :609
          const Real zbeta = const3 * (coswind + kKappa / (zcn * ust * cm)) * (ucn * ucn);
          if (zlog < zero) {
            const Real zlog2 = zlog * zlog;
            ds = cnst * Kokkos::exp(zlog) * (zlog2 * zlog2) * (ucn * ucn) *
                 Kokkos::pow(coswind, ssinthp);  // :614
            llws[is - 1] = 1;
          } else {
            ds = zbeta;  // :617
            llws[is - 1] = 1;
          }
        } else {
          const Real zbeta = const3 * (coswind + kKappa / (zcn * ust * cm)) * (ucn * ucn);
          ds = zbeta;  // :622
          llws[is - 1] = 0;
        }
        const Real temp2 = const2 * ds * a[is - 1];  // :626
        if (ds < zero) {
          stressstabn[istab - 1][0] = stressstabn[istab - 1][0] + temp2 * ecos[is - 1];
          stressstabn[istab - 1][1] = stressstabn[istab - 1][1] + temp2 * esin[is - 1];
        }
        stressstab[istab - 1][0] = stressstab[istab - 1][0] + temp2 * ecos[is - 1];
        stressstab[istab - 1][1] = stressstab[istab - 1][1] + temp2 * esin[is - 1];
      }
    }
    // :636-640, inside the ISTAB loop under STAB3 too, where row 3 is still zero.
    for (int i = 0; i < nspec; ++i) d[i] = dstab[2 + 3 * i];
    xstress = stressstab[2][0];
    ystress = stressstab[2][1];
    tauwnx = stressstabn[2][0];
    tauwny = stressstabn[2][1];
  }
  if constexpr (kStab3) {
    const Real half = static_cast<Real>(0.5);
    for (int i = 0; i < nspec; ++i) d[i] = half * (dstab[3 * i] + dstab[1 + 3 * i]);  // :644
    xstress = half * (stressstab[0][0] + stressstab[1][0]);
    ystress = half * (stressstab[0][1] + stressstab[1][1]);
    tauwnx = half * (stressstabn[0][0] + stressstabn[1][0]);
    tauwny = half * (stressstabn[0][1] + stressstabn[1][1]);
  }
  for (int i = 0; i < nspec; ++i) s[i] = d[i] * a[i];  // :650

  // :671-677. SIG(NK)**5 is s3*s2 with s3 = s*(s*s), MAX(...)**3 is m*(m*m) (GIMPLE).
  const Real signk = sig[nk - 1];
  const Real signk2 = signk * signk;
  const Real signk5 = (signk * signk2) * signk2;
  const Real const0 = dth * signk5 / sin3_detail::kGrav2Tpi * kTpi * signk / cg[nk - 1];
  Real temp = zero;
  for (int ith = 1; ith <= nth; ++ith) {
    const int is = ith + (nk - 1) * nth;
    const Real coswind = ecos[is - 1] * cosu + esin[is - 1] * sinu;
    const Real m = maxss(coswind, zero);
    temp = temp + a[is - 1] * (m * (m * m));
  }

  // :682-691
  const Real xi = ust / delust;
  const int ind = imax(imin(fint(xi), kTauhfIustar - 1), 1);
  const Real deli1 = maxss(minss(xi - static_cast<Real>(ind), one), zero);
  const Real deli2 = one - deli1;
  const Real ustm = maxss(ust, static_cast<Real>(0.00001));
  const Real xj = maxss((kGrav * z0 / (ustm * ustm) - aalpha) / delalp, zero);
  const int j = imax(imin(fint(xj), kTauhfIalpha - 1), 1);
  const Real delj1 = maxss(minss(xj - static_cast<Real>(j), one), zero);
  const Real delj2 = one - delj1;
  const Real tau1 = (tauhft[ind + kLd * j] * deli2 + tauhft[(ind + 1) + kLd * j] * deli1) * delj2 +
                    (tauhft[ind + kLd * (j + 1)] * deli2 +
                     tauhft[(ind + 1) + kLd * (j + 1)] * deli1) *
                        delj1;
  const Real tauhf = const0 * temp * (ust * ust) * tau1;  // :692
  tauwx = xstress + tauhf * Kokkos::cos(usdir);           // :693
  tauwy = ystress + tauhf * Kokkos::sin(usdir);           // :694

  // :699-705
  const Real tauw = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);
  const Real um = sin3_detail::max_ustar_eps2<kStab3>(ustar);
  const Real ust2 = um * um;
  const Real tauwb = minss(maxss(ust2 - sin3_detail::kEps1, sin3_detail::kEps2Sq), tauw);
  if (tauwb < tauw) {
    tauwx = tauwx * tauwb / tauw;
    tauwy = tauwy * tauwb / tauw;
  }
}

}  // namespace ww::srce::st3
