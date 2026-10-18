// kokkos/src/ww_kokkos/st4_sds4.hpp
// W3SDS4, the ST4 dissipation source term (Ardhuin et al. 2010, Filipot and
// Ardhuin 2012, Romero 2019), for one spectral point. Phase 1, translated, not
// regtested.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:2043-2659 at WW3@bb08623a; authors
// F. Ardhuin, F. Leckler, L. Romero (Fortran header, :2047-2048).
//
// Arithmetic follows the -O3 GIMPLE inventory of w3src4md (ST4 plan, Task 1):
// integer literal powers as GCC's multiplication tree, REAL exponents as powf,
// folded constants as the dump shows them, every SUM / DOT_PRODUCT left to right
// (in-order fold-left in the dump). Fortran operations that -O3 merely commutes
// (a*b -> b*a) are written in the source order: same bits.
//
// Three loops (:2315, :2500, :2509) are vectorised by gfortran -O3 with glibc
// libmvec (_ZGVbN4vv_powf, _ZGVbN4v_expf) in groups of four from ITH = 1, scalar
// for the last NTH mod 4; libmvec bits differ from scalar powf/expf. Each is
// marked LIBMVEC SITE and goes through st4_libmvec.hpp (the owner, 2026-10-15).
#pragma once

#include <Kokkos_Core.hpp>

#include "libm_device.hpp"
#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_libmvec.hpp"
#include "st4_params.hpp"
#include "st4_vec8.hpp"

namespace ww::st4 {

namespace sds4_detail {

// Constants gfortran folds (INVENTORY.md, W3SDS4): the same bits as the float
// expression evaluated left to right.
inline constexpr Real kTwoPiSq = static_cast<Real>(2.) * (kPi * kPi);  // 2.*PI**2.  :2330
static_assert(kTwoPiSq == 1.97392101287841796875e+1f, "2*PI**2 as folded at -O3");
inline constexpr Real kGrav2 = kGrav * kGrav;  // GRAV**2  :2516
static_assert(kGrav2 == 9.615763092041015625e+1f, "GRAV**2 as folded at -O3");
inline constexpr Real kTwoPi = static_cast<Real>(2) * kPi;  // 2*PI  :2416
static_assert(kTwoPi == 6.283185482025146484375e+0f, "2*PI as folded at -O3");
inline constexpr Real kTstrFac = (static_cast<Real>(0.8) * static_cast<Real>(2)) * kPi;  // :2627
static_assert(kTstrFac == 5.0265483856201171875e+0f, "0.8*2*PI as folded at -O3");
inline constexpr Real kTmaxFac = (static_cast<Real>(5.) * static_cast<Real>(2)) * kPi;  // :2629
static_assert(kTmaxFac == 3.1415927886962890625e+1f, "5.*2*PI as folded at -O3");
inline constexpr Real kDkhs = kKhsMax / static_cast<Real>(kNkhs);  // KHSMAX/NKHS  :2391
static_assert(kDkhs == 1.000000047497451305389404296875e-3f, "DKHS as folded at -O3");
// LOG(FAC_KD1) folded = glibc logf(1.01f) (INVENTORY.md, :2395); not constexpr in C++.
inline constexpr Real kLogFacKd1 = 9.9503211677074432373046875e-3f;

/// FLOOR(x) as the -O3 dump computes it at :2590: i = (int)x; (float)i <= x ? i : i-1,
/// with the wrap of -fno-strict-overflow, so a NaN gives INT_MAX as in the Fortran.
KOKKOS_INLINE_FUNCTION int ffloor(Real x) {
  const int i = fint(x);
  return static_cast<Real>(i) <= x ? i : static_cast<int>(static_cast<unsigned>(i) - 1u);
}


}  // namespace sds4_detail

/// Reals of per-point work storage: 13 arrays of NK (COEF4, COEF5, S1, E1, DK,
/// HS, KBAR, DCK, EFDF, BTH0, QB, S2, BRM12), MSSSUM(NK,5), BTH, PB, PB2 of
/// NSPEC, LMODULATION of NTH.
inline int sds4_work_reals(const Params& p) { return 18 * p.nk + 3 * p.nspec + p.nth; }
/// Ints: NTIMES and IKSUP, NK each. NSMOOTH (:2163) is zeroed and never read: not kept.
inline int sds4_work_ints(const Params& p) { return 2 * p.nk; }

/// The arguments of w3sds4_point that its parts read and write (IX, IY are not read and not
/// passed), so that the same parts serve one point (w3sds4_point) and the points of a team
/// (w3sds4_lanes).
struct Sds4Args {
  const Real* a;
  const Real* k;
  const Real* cg;
  Real ustar, usdir, depth, dair, dlwmean;
  int flogrd57, flogrd58;
  Real *srhs, *ddiag, *brlambda, *whitecap;
};

/// The per-point work arrays of w3sds4_point, by name (the layout of sds4_work_reals).
struct Sds4Work {
  Real *coef4, *coef5, *s1, *e1, *dk, *hs, *kbar, *dck, *efdf, *bth0, *qb, *s2, *brm12, *msssum,
      *bth, *pb, *pb2, *lmodulation;
  int *ntimes, *iksup;
};

/// The locals that cross from section 1 to the later sections.
struct Sds4Mid {
  Real facturb;
  int dikcumul, ik1;
};

KOKKOS_INLINE_FUNCTION Sds4Work sds4_layout(const Params& p, Real* work, int* iwork) {
  const int nk = p.nk, nspec = p.nspec;
  Sds4Work w;
  w.coef4 = work;
  w.coef5 = w.coef4 + nk;
  w.s1 = w.coef5 + nk;
  w.e1 = w.s1 + nk;
  w.dk = w.e1 + nk;
  w.hs = w.dk + nk;
  w.kbar = w.hs + nk;
  w.dck = w.kbar + nk;
  w.efdf = w.dck + nk;
  w.bth0 = w.efdf + nk;
  w.qb = w.bth0 + nk;
  w.s2 = w.qb + nk;
  w.brm12 = w.s2 + nk;
  w.msssum = w.brm12 + nk;  // MSSSUM(IK,J) = msssum[(J-1)*nk + IK-1]
  w.bth = w.msssum + 5 * nk;
  w.pb = w.bth + nspec;
  w.pb2 = w.pb + nspec;
  w.lmodulation = w.pb2 + nspec;
  w.ntimes = iwork;
  w.iksup = w.ntimes + nk;
  return w;
}

/// Sections 0 and 1 of W3SDS4 (:2203-2224): the pre-initialisation, FACTURB, DIKCUMUL, IK1.
KOKKOS_INLINE_FUNCTION void sds4_prep(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, Sds4Mid& mid) {
  (void)f;
  using ww::st4::nint;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const int nk = p.nk;
  const int nspec = p.nspec;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]
  const Real ustar = pt.ustar;
  const Real dair = pt.dair;
  Real* const srhs = pt.srhs;
  Real* const ddiag = pt.ddiag;
  Real* const s1 = wk.s1;
  Real* const e1 = wk.e1;
  Real* const dk = wk.dk;
  Real* const hs = wk.hs;
  Real* const kbar = wk.kbar;
  Real* const dck = wk.dck;
  Real* const efdf = wk.efdf;
  Real* const bth0 = wk.bth0;
  Real* const qb = wk.qb;
  Real* const s2 = wk.s2;
  Real* const brm12 = wk.brm12;
  Real* const msssum = wk.msssum;
  Real* const bth = wk.bth;
  Real* const pb = wk.pb;
  Real* const pb2 = wk.pb2;
  int* const ntimes = wk.ntimes;
  int* const iksup = wk.iksup;
  // 0. Pre-initialization (:2203-2213).
  for (int i = 0; i < nk; ++i) {
    s1[i] = zero; e1[i] = zero; ntimes[i] = 0; iksup[i] = 0;
    dk[i] = zero; hs[i] = zero; kbar[i] = zero; dck[i] = zero; efdf[i] = zero;
    bth0[i] = zero; qb[i] = zero; s2[i] = zero; brm12[i] = zero;
  }
  for (int i = 0; i < nspec; ++i) {
    bth[i] = zero; ddiag[i] = zero; srhs[i] = zero; pb[i] = zero; pb2[i] = zero;
  }
  for (int i = 0; i < 5 * nk; ++i) msssum[i] = zero;

  // 1. Initialization and numerical factors (:2217-2224).
  const Real facturb = (((ssdsc[4] * (ustar * ustar)) / kGrav) * dair) / kDwat;
  // DIKCUMUL is a W3SRC4MD module variable written here (:2218); kept local, not written back.
  const int dikcumul = nint(p.ssdsbrf1 / (p.xfr - one));
  int ik1 = 1;
  if (p.ig1) ik1 = nint(p.igpars5) + 1;  // W3_IG1, :2223
  mid.facturb = facturb;
  mid.dikcumul = dikcumul;
  mid.ik1 = ik1;
}

/// Section 1.b (:2228-2254): the mean-square slope for the modulation of lambda.
KOKKOS_INLINE_FUNCTION void sds4_mss(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, const Sds4Mid& mid) {
  (void)mid;
  const Real zero = static_cast<Real>(0.);
  const int nk = p.nk;
  const int nth = p.nth;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]
  const Real* const a = pt.a;
  const Real* const k = pt.k;
  const Real* const cg = pt.cg;
  Real* const msssum = wk.msssum;
  auto MSS = [&](int ik, int j) -> Real& { return msssum[(j - 1) * nk + (ik - 1)]; };
  // 1.b MSS parameters for the modulation of lambda, Romero (:2228-2254).
  if (ssdsc[7] > zero || ssdsc[10] > zero || ssdsc[17] > zero) {
    for (int ik = 1; ik <= nk; ++ik) {
      Real mssp = zero, msspc2 = zero, mssps2 = zero;
      const Real kp = dm::powf(k[ik - 1], ssdsc[19]);
      for (int ith = 1; ith <= nth; ++ith) {
        const int is = ith + (ik - 1) * nth;
        const Real msslong = ((kp * a[is - 1]) * f.dden(ik - 1)) / cg[ik - 1];
        msspc2 = msspc2 + msslong * f.ecos(ith - 1);
        mssps2 = mssps2 + msslong * f.esin(ith - 1);
        mssp = mssp + msslong;
      }
      for (int i = ik; i <= nk; ++i) {
        MSS(i, 1) = MSS(i, 1) + mssp;
        MSS(i, 3) = MSS(i, 3) + msspc2;
        MSS(i, 4) = MSS(i, 4) + mssps2;
      }
      Real mssd = dm::atan2f(MSS(ik, 4), MSS(ik, 3));
      if (mssd < zero) mssd = mssd + kPi;
      MSS(ik, 2) = mssd;
    }
  }
}

/// Case 1, 2.a.1 (:2262-2290): the saturation BTH and BTH0.
KOKKOS_INLINE_FUNCTION void sds4_sat(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, const Sds4Mid& mid) {
  const Real zero = static_cast<Real>(0.);
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real dth = p.dth;
  const Real* const a = pt.a;
  const Real* const k = pt.k;
  Real* const bth0 = wk.bth0;
  Real* const bth = wk.bth;
  const int ik1 = mid.ik1;
  for (int i = 0; i < nspec; ++i) bth[i] = zero;
  // 2.a.1 Saturation.
  const int nsat = static_cast<int>(f.satweights.extent(0));
  for (int ik = ik1; ik <= nk; ++ik) {
    const Real kk = k[ik - 1];
    const Real facsat = (f.sig(ik) * ((kk * kk) * kk)) * dth;
    const int is0 = (ik - 1) * nth;
    bth[is0] = zero;
    Real asum = zero;
    for (int ith = 1; ith <= nth; ++ith) asum = asum + a[is0 + ith - 1];
    bth0[ik - 1] = asum * facsat;
    if (p.ssdsdth >= static_cast<Real>(180)) {
      for (int i = is0; i < is0 + nth; ++i) bth[i] = bth0[ik - 1];
    } else {
      // Full length 2*(MTH/2-1)+1 for any SDSNTH (:2283): the extra terms have weight 0
      // and index 1, and 0*x is not 0 for an infinite x; the length sets the order.
      // J outside, ITH inside: each BTH(IS) still adds its products in the order
      // J = 1..NSAT, starting from 0, so its bits are DOT_PRODUCT's; only the NTH sums are
      // interleaved, which removes the add-latency chain of one sum. bth and a are distinct
      // work arrays, so the ITH loop carries no dependence.
      // The transposed tables make the ITH loop's loads contiguous; without them (a Fields not
      // built by st4_shim) the same products are read from the originals.
      for (int ith = 0; ith < nth; ++ith) bth[is0 + ith] = zero;
      const bool tr = f.satweights_t.extent(1) == static_cast<size_t>(nsat);
      for (int j = 0; j < nsat; ++j) {
        if (tr) {
          const Real* const w = &f.satweights_t(0, j);
          const int* const x = &f.satindices_t(0, j);
#if defined(_OPENMP) && !defined(__CUDACC__) && !defined(__HIPCC__)
#pragma omp simd
#endif
          for (int ith = 0; ith < nth; ++ith)
            bth[is0 + ith] = bth[is0 + ith] + w[ith] * a[is0 + x[ith] - 1];
        } else {
          for (int ith = 0; ith < nth; ++ith)
            bth[is0 + ith] = bth[is0 + ith] + f.satweights(j, ith) * a[is0 + f.satindices(j, ith) - 1];
        }
      }
      for (int ith = 0; ith < nth; ++ith) bth[is0 + ith] = bth[is0 + ith] * facsat;
      // MAXVAL (:2287), as gfortran lowers it: first element >= -Inf, then strictly greater.
      Real lim = -Kokkos::Experimental::infinity_v<Real>;
      int i = is0;
      bool found = false;
      for (; i < is0 + nth; ++i) {
        if (bth[i] >= lim) { lim = bth[i]; ++i; found = true; break; }
      }
      if (!found) lim = Kokkos::Experimental::quiet_NaN_v<Real>;
      for (; i < is0 + nth; ++i)
        if (bth[i] > lim) lim = bth[i];
      bth0[ik - 1] = lim;
    }
  }
}

/// Case 1, 2.a.2 and the rest (:2291-2331): the dissipation rate, PB, BRLAMBDA, SRHS.
KOKKOS_INLINE_FUNCTION void sds4_case1(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, const Sds4Mid& mid) {
  using ww::st4::max_r;
  using ww::st4::min_r;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]
  const Real* const a = pt.a;
  const Real* const k = pt.k;
  const Real depth = pt.depth;
  Real* const srhs = pt.srhs;
  Real* const ddiag = pt.ddiag;
  Real* const brlambda = pt.brlambda;
  Real* const bth0 = wk.bth0;
  Real* const bth = wk.bth;
  Real* const pb = wk.pb;
  const int ik1 = mid.ik1;
  const Real epsr = Kokkos::sqrt(p.ssdsbr);
  // 2.a.2 Spontaneous breaking dissipation rate.
  for (int ik = ik1; ik <= nk; ++ik) {
    Real miche;
    if (p.ssdsbm[0] == one) {
      miche = one;
    } else {
      const Real x = dm::tanhf(min_r(k[ik - 1] * depth, static_cast<Real>(10.)));
      const Real t =
          x * (p.ssdsbm[1] + x * (p.ssdsbm[2] + x * (p.ssdsbm[3] + x * p.ssdsbm[4])));
      miche = t * t;
    }
    const Real coef1 = p.ssdsbr * miche;
    const Real m = max_r(bth0[ik - 1] / coef1 - one, zero);
    const Real sdiagiso = ((ssdsc[1] * f.sig(ik)) * ssdsc[5]) * (m * m);
    const Real coef2 = ((ssdsc[1] * f.sig(ik)) * (one - ssdsc[5])) / (coef1 * coef1);
    // LIBMVEC SITE: -O3 uses _ZGVbN4vv_powf here (INVENTORY.md, :2315), lane i = ITH-1.
    const int is0 = (ik - 1) * nth;
    const Real ssdsp = p.ssdsp;
    mvec_powf(
        nth, [&](int i) { return max_r(bth[is0 + i] - coef1, zero); },
        [&](int) { return ssdsp; },
        [&](int i, Real v) { ddiag[is0 + i] = sdiagiso + coef2 * v; });
  }
  // Breaking probability (:2320-2327), lambda (:2330), source (:2331).
  for (int i = 0; i < nspec; ++i) {
    const Real m = max_r(Kokkos::sqrt(bth[i]) - epsr, zero);
    pb[i] = m * m;
  }
  for (int i = 0; i < nspec; ++i) pb[i] = pb[i] * static_cast<Real>(28.16);
  for (int i = 0; i < nspec; ++i) brlambda[i] = pb[i] / sds4_detail::kTwoPiSq;
  for (int i = 0; i < nspec; ++i) srhs[i] = ddiag[i] * a[i];
}

/// Case 2, 2.b (:2338-2477), unchanged.
KOKKOS_INLINE_FUNCTION void sds4_case2(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, const Sds4Mid& mid) {
  using ww::st4::nint;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real dth = p.dth;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]
  const Real* const a = pt.a;
  const Real* const k = pt.k;
  const Real* const cg = pt.cg;
  const Real depth = pt.depth;
  Real* const srhs = pt.srhs;
  Real* const ddiag = pt.ddiag;
  Real* const brlambda = pt.brlambda;
  Real* const s1 = wk.s1;
  Real* const e1 = wk.e1;
  Real* const dk = wk.dk;
  Real* const hs = wk.hs;
  Real* const kbar = wk.kbar;
  Real* const dck = wk.dck;
  Real* const efdf = wk.efdf;
  Real* const bth0 = wk.bth0;
  Real* const qb = wk.qb;
  Real* const s2 = wk.s2;
  Real* const pb = wk.pb;
  Real* const pb2 = wk.pb2;
  int* const ntimes = wk.ntimes;
  int* const iksup = wk.iksup;
  const int ik1 = mid.ik1;
  // 2.b Spontaneous breaking for T500, Filipot et al. JGR 2010 (:2338-2477).
  for (int i = 0; i < nk; ++i) { e1[i] = zero; hs[i] = zero; }
  for (int i = 0; i < nspec; ++i) { srhs[i] = zero; ddiag[i] = zero; pb2[i] = zero; }
  for (int ik = ik1; ik <= nk; ++ik) {
    e1[ik - 1] = zero;
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      e1[ik - 1] = e1[ik - 1] + (a[is - 1] * f.sig(ik)) * dth;
    }
    dk[ik - 1] = f.dden(ik - 1) / ((dth * f.sig(ik)) * cg[ik - 1]);
  }
  int id = nint(depth);
  if (id > kNdtab) id = kNdtab;
  if (id < 1) {
    id = 1;
  } else if (id > kNdtab) {
    id = kNdtab;
  }
  for (int i = 0; i < nk; ++i) { hs[i] = zero; efdf[i] = zero; kbar[i] = zero; }
  int nkl = 0;
  for (int ikl = 1; ikl <= nk; ++ikl) {
    iksup[ikl - 1] = f.iktab(ikl - 1, id - 1);
    if (iksup[ikl - 1] <= nk) {
      Real v = zero;
      for (int i = ikl; i <= iksup[ikl - 1] - 1; ++i) v = v + e1[i - 1] * dk[i - 1];
      efdf[ikl - 1] = v;
      if (efdf[ikl - 1] != zero) {
        Real w = zero;
        for (int i = ikl; i <= iksup[ikl - 1] - 1; ++i)
          w = w + (k[i - 1] * e1[i - 1]) * dk[i - 1];
        kbar[ikl - 1] = w / efdf[ikl - 1];
      } else {
        kbar[ikl - 1] = zero;
      }
      hs[ikl - 1] = static_cast<Real>(4) * Kokkos::sqrt(efdf[ikl - 1]);
      nkl = nkl + 1;
    }
  }
  // Dissipation and breaking probability per scale. The loop runs to the count NKL,
  // not over the scales that passed IKSUP <= NK (:2392).
  for (int i = 0; i < nk; ++i) { dck[i] = zero; qb[i] = zero; }
  for (int ikl = 1; ikl <= nkl; ++ikl) {
    const Real kb = kbar[ikl - 1];
    if (hs[ikl - 1] != zero && kb != zero) {
      // FAC_KD2 + ANINT(...) is REAL, truncated on assignment to INTEGER.
      int ikd = fint(Kokkos::round(dm::logf(kb * depth) / sds4_detail::kLogFacKd1) +
                     static_cast<Real>(kFacKd2));
      int ikhs = fint(one + Kokkos::round((kb * hs[ikl - 1]) / sds4_detail::kDkhs));
      if (ikd > kNkd) {
        ikd = kNkd;
      } else if (ikd < 1) {
        ikd = 1;
      }
      if (ikhs > kNkhs) {
        ikhs = kNkhs;
      } else if (ikhs < 1) {
        ikhs = 1;
      }
      const Real xt = dm::tanhf(kb * depth);
      // GAM (:2411) is computed and never used.
      const Real gam = ((((xt * xt) * xt) * static_cast<Real>(1.0314) -
                         (xt * xt) * static_cast<Real>(1.9958)) +
                        xt * static_cast<Real>(1.5522)) +
                       static_cast<Real>(0.1885);
      (void)gam;
      dck[ikl - 1] = (dm::powf(kb, static_cast<Real>(-2.5)) * (kb / sds4_detail::kTwoPi)) *
                     f.dcki(ikhs - 1, ikd - 1);
      qb[ikl - 1] = f.qbi(ikhs - 1, ikd - 1);
    } else {
      dck[ikl - 1] = zero;
      qb[ikl - 1] = zero;
    }
  }
  // Distributes scale dissipation over the frequency spectrum (:2429-2452).
  for (int i = 0; i < nk; ++i) { s1[i] = zero; s2[i] = zero; ntimes[i] = 0; }
  for (int ikl = 1; ikl <= nkl; ++ikl) {
    if (efdf[ikl - 1] > zero) {
      for (int i = ikl; i <= iksup[ikl - 1]; ++i)
        s1[i - 1] = s1[i - 1] + (dck[ikl - 1] * e1[i - 1]) / efdf[ikl - 1];
      for (int i = ikl; i <= iksup[ikl - 1]; ++i)
        s2[i - 1] = s2[i - 1] + (qb[ikl - 1] * e1[i - 1]) / efdf[ikl - 1];
      for (int i = ikl; i <= iksup[ikl - 1]; ++i) ntimes[i - 1] = ntimes[i - 1] + 1;
    }
  }
  for (int i = 0; i < nk; ++i) {
    if (ntimes[i] > 0) {
      s1[i] = s1[i] / static_cast<Real>(ntimes[i]);
      s2[i] = s2[i] / static_cast<Real>(ntimes[i]);
    } else {
      s1[i] = zero;
      s2[i] = zero;
    }
  }
  for (int ik = 1; ik <= nk; ++ik) s1[ik - 1] = s1[ik - 1] / f.sig(ik);
  // Isotropic distribution (:2456-2471).
  for (int ik = 1; ik <= nk; ++ik) {
    Real v = zero;
    for (int is = (ik - 1) * nth + 1; is <= ik * nth; ++is) v = v + a[is - 1];
    const Real asum = v * dth;
    if (asum > static_cast<Real>(1.E-8)) {
      for (int is = 1 + (ik - 1) * nth; is <= ik * nth; ++is) ddiag[is - 1] = s1[ik - 1] / asum;
      for (int is = 1 + (ik - 1) * nth; is <= ik * nth; ++is) pb2[is - 1] = s2[ik - 1] / asum;
    } else {
      for (int is = 1 + (ik - 1) * nth; is <= ik * nth; ++is) ddiag[is - 1] = zero;
      for (int is = 1 + (ik - 1) * nth; is <= ik * nth; ++is) pb2[is - 1] = zero;
    }
    if (pb2[(ik - 1) * nth] > static_cast<Real>(0.001)) {
      bth0[ik - 1] = static_cast<Real>(2.) * p.ssdsbr;
    } else {
      bth0[ik - 1] = zero;
    }
  }
  // Kept as written (:2473): PB is 0 here, so this is (1-SSDSC(1))*PB2*A + SSDSC(1)*0.
  for (int i = 0; i < nspec; ++i)
    pb[i] = ((one - ssdsc[0]) * pb2[i]) * a[i] + ssdsc[0] * pb[i];
  for (int i = 0; i < nspec; ++i) brlambda[i] = pb[i] / sds4_detail::kTwoPiSq;
  for (int i = 0; i < nspec; ++i) srhs[i] = ddiag[i] * a[i];
}

/// Case 3, 2.c (:2481-2520), unchanged.
KOKKOS_INLINE_FUNCTION void sds4_case3(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, const Sds4Mid& mid) {
  (void)mid;
  using ww::st4::max_r;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const int nk = p.nk;
  const int nth = p.nth;
  const Real dth = p.dth;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]
  const Real* const a = pt.a;
  const Real* const k = pt.k;
  const Real ustar = pt.ustar;
  const Real dlwmean = pt.dlwmean;
  Real* const srhs = pt.srhs;
  Real* const ddiag = pt.ddiag;
  Real* const brlambda = pt.brlambda;
  Real* const bth0 = wk.bth0;
  Real* const brm12 = wk.brm12;
  Real* const msssum = wk.msssum;
  Real* const bth = wk.bth;
  Real* const lmodulation = wk.lmodulation;
  auto MSS = [&](int ik, int j) -> Real& { return msssum[(j - 1) * nk + (ik - 1)]; };
  // 2.c Romero (GRL 2019) (:2481-2520). Division by USTAR**2 kept: Inf at USTAR = 0 (:2485).
  const Real t28 = static_cast<Real>(28.) / ssdsc[15];
  const Real ko = (kGrav / (ustar * ustar)) / (t28 * t28);
  for (int ik = 1; ik <= nk; ++ik) {
    const int is0 = (ik - 1) * nth;
    const Real kk = k[ik - 1];
    const Real kloc = dm::powf(kk, static_cast<Real>(2.) - ssdsc[19]);
    const Real k3 = (kk * kk) * kk;
    // BTH(1:NTH) is the scratch of this IK.
    for (int ith = 1; ith <= nth; ++ith)
      bth[ith - 1] =
          max_r((a[is0 + ith - 1] * f.sig(ik)) * k3, static_cast<Real>(.00000000000001));
    Real dirforcum = dlwmean;
    if (ssdsc[10] > zero) dirforcum = MSS(ik, 2);
    const Real c = f.sig(ik) / kk;
    Real sb = zero;
    for (int ith = 1; ith <= nth; ++ith) sb = sb + bth[ith - 1] * dth;
    bth0[ik - 1] = sb;
    if (ssdsc[17] > zero) {
      Real cd, sd;
      dm::sincosf(dirforcum, sd, cd);
      const Real pm = dm::powf(MSS(ik, 1) * kloc, ssdsc[13]);
      for (int ith = 1; ith <= nth; ++ith) {
        const Real q = f.ecos(ith - 1) * cd + f.esin(ith - 1) * sd;
        lmodulation[ith - 1] = one + ssdsc[17] * (pm * (q * q));  // FACSTRAINL
      }
      // LIBMVEC SITE: -O3 uses _ZGVbN4vv_powf here (INVENTORY.md, :2500), lane i = ITH-1.
      const Real e19 = ssdsc[18];
      mvec_powf(
          nth, [&](int i) { return lmodulation[i]; }, [&](int) { return e19; },
          [&](int i, Real v) { lmodulation[i] = v; });
    } else {
      for (int ith = 1; ith <= nth; ++ith) lmodulation[ith - 1] = one;
    }
    // Lambda (:2507-2509).
    const Real fk = one + ssdsc[12] * dm::powf(max_r(kk / ko, one), ssdsc[14]);
    // LIBMVEC SITE: -O3 uses _ZGVbN4v_expf here (INVENTORY.md, :2509), lane i = ITH-1;
    // the exponential first, then (((e*SSDSC(9))*F)/(SSDSC(13)+1))*LMODULATION per element.
    const Real ssdsbr = p.ssdsbr, c9 = ssdsc[8], c13p1 = ssdsc[12] + one;
    mvec_expf(
        nth, [&](int i) { return -(ssdsbr / bth[i]); },
        [&](int i, Real v) { brlambda[is0 + i] = (((c9 * v) * fk) / c13p1) * lmodulation[i]; });
    // Breaking strength, a generalisation of Duncan's b (:2511-2512).
    const Real btover = Kokkos::sqrt(bth0[ik - 1]) - Kokkos::sqrt(p.ssdsbt);
    // MAX(0.,BTOVER) computes 0 > b ? 0 : b (INVENTORY.md, :2512 assembly).
    brm12[ik - 1] =
        ssdsc[1] * dm::powf(zero > btover ? zero : btover, static_cast<Real>(2.5));
    // MAX(0.,SIGN(x,BTOVER)) computes x > 0 ? x : +0. (INVENTORY.md, :2514 assembly).
    for (int ith = 1; ith <= nth; ++ith) {
      const Real x = Kokkos::copysign(brlambda[is0 + ith - 1], btover);
      brlambda[is0 + ith - 1] = x > zero ? x : zero;
    }
    const Real c2 = c * c;
    const Real c5 = (c * c2) * c2;
    for (int ith = 1; ith <= nth; ++ith)
      srhs[is0 + ith - 1] =
          (((brm12[ik - 1] / sds4_detail::kGrav2) * brlambda[is0 + ith - 1]) * c5) / f.sig(ik);
    for (int ith = 1; ith <= nth; ++ith)
      ddiag[is0 + ith - 1] =
          ((srhs[is0 + ith - 1] * p.ssdsbr) / max_r(bth[ith - 1], static_cast<Real>(1.e-20))) /
          max_r(a[is0 + ith - 1], static_cast<Real>(1e-20));
  }
}

/// Wave-turbulence interaction and cumulative effect (:2533-2575).
KOKKOS_INLINE_FUNCTION void sds4_turb(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, const Sds4Mid& mid) {
  using ww::st4::max_r;
  const Real zero = static_cast<Real>(0.);
  const int nk = p.nk;
  const int nth = p.nth;
  const Real dth = p.dth;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]
  const Real* const a = pt.a;
  const Real* const k = pt.k;
  const Real usdir = pt.usdir;
  Real* const srhs = pt.srhs;
  Real* const ddiag = pt.ddiag;
  Real* const brlambda = pt.brlambda;
  Real* const bth0 = wk.bth0;
  const Real facturb = mid.facturb;
  const int dikcumul = mid.dikcumul;
  const int ik1 = mid.ik1;
  Real renewalfreq = zero;
  // Wave-turbulence interaction and cumulative effect (:2533-2575).
  if ((ssdsc[2] != zero) || (ssdsc[4] != zero) || (ssdsc[20] != zero)) {
    for (int ik = ik1; ik <= nk; ++ik) {
      renewalfreq = zero;
      const Real facturb2 = -(((f.sig(ik) * static_cast<Real>(2.)) * k[ik - 1]) * facturb);
      const Real dvisc = -(((ssdsc[20] * static_cast<Real>(4.)) * k[ik - 1]) * k[ik - 1]);
      const Real c = f.sig(ik) / k[ik - 1];
      if (ssdsc[2] > zero && ik > dikcumul) {
        // The cheap isotropic version.
        for (int ik2 = ik1; ik2 <= ik - dikcumul; ++ik2) {
          const Real c2 = f.sig(ik2) / k[ik2 - 1];
          const int is2 = (ik2 - 1) * nth;
          const Real cumulwiso =
              ((Kokkos::fabs(c2 - c) * f.dsip(ik2)) / (c2 * static_cast<Real>(0.5))) * dth;
          Real v = zero;
          for (int i = is2; i < is2 + nth; ++i) v = v + brlambda[i];
          renewalfreq = renewalfreq - cumulwiso * v;
        }
      }
      for (int ith = 1; ith <= nth; ++ith) {
        const int is = ith + (ik - 1) * nth;
        if (ssdsc[2] < zero && ik > dikcumul) {
          // The default (SSDSC(3) < 0), O(NK^2 NTH^2) branch, kept (:2554-2563).
          renewalfreq = zero;
          for (int ik2 = ik1; ik2 <= ik - dikcumul; ++ik2) {
            if (bth0[ik2 - 1] > p.ssdsbr) {
              const int is2 = (ik2 - 1) * nth;
              Real v = zero;
              for (int j = 1; j <= nth; ++j)
                v = v + f.cumulw(is2 + j - 1, is - 1) * brlambda[is2 + j - 1];
              renewalfreq = renewalfreq + v;
            }
          }
        }
        // Wave-turbulence interaction; COS/SIN(USDIR) recomputed per ITH as written (:2567).
        Real cu, su;
        dm::sincosf(usdir, su, cu);
        const Real coswind = f.ecos(ith - 1) * cu + f.esin(ith - 1) * su;
        const Real dturb = facturb2 * max_r(coswind, zero);
        const Real diag2 = (ssdsc[2] * renewalfreq + dturb) + dvisc;
        ddiag[is - 1] = ddiag[is - 1] + diag2;
        srhs[is - 1] = srhs[is - 1] + a[is - 1] * diag2;
      }
    }
  }
}

/// Whitecap parameters (:2581-2650). Early return (:2581): WHITECAP keeps the caller's values.
KOKKOS_INLINE_FUNCTION void sds4_whitecap(const Params& p, const Fields& f, const Sds4Args& pt,
                                const Sds4Work& wk, const Sds4Mid& mid) {
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const int nk = p.nk;
  const int nth = p.nth;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]
  const Real* const k = pt.k;
  const Real* const cg = pt.cg;
  const int flogrd57 = pt.flogrd57;
  const int flogrd58 = pt.flogrd58;
  Real* const brlambda = pt.brlambda;
  Real* const whitecap = pt.whitecap;
  Real* const coef4 = wk.coef4;
  Real* const coef5 = wk.coef5;
  Real* const brm12 = wk.brm12;
  const int ik1 = mid.ik1;
  // Whitecap parameters. Early return (:2581): WHITECAP keeps the caller's values;
  // WHITECAP(3) is never set.
  if (!(flogrd57 || flogrd58)) return;
  for (int i = 0; i < 4; ++i) whitecap[i] = zero;

  // COEF4: whitecap coverage; COEF5: air entrainment rate, Romero 2019 (:2589-2604).
  const int ikc = sds4_detail::ffloor(p.aaircmin) < nk ? sds4_detail::ffloor(p.aaircmin) : nk;
  for (int ik = 1; ik <= ikc; ++ik) {
    const Real c = f.sig(ik) / k[ik - 1];
    const int is0 = (ik - 1) * nth;
    Real v = zero;
    for (int i = is0; i < is0 + nth; ++i) v = v + brlambda[i];
    coef4[ik - 1] = ((((((c * c) * v) * static_cast<Real>(2.)) * kPi) / kGrav) * ssdsc[6]) *
                    f.dden(ik - 1) / (f.sig(ik) * cg[ik - 1]);
    const Real pw = dm::powf(brm12[ik - 1] / ssdsc[1], static_cast<Real>(3.) / static_cast<Real>(5.));
    Real w = zero;
    for (int i = is0; i < is0 + nth; ++i) w = w + (brlambda[i] * (-ssdsc[1])) * pw;
    coef5[ik - 1] = ((((c * c) * c) * w) * p.aairgb / kGrav) * f.dden(ik - 1) /
                    (f.sig(ik) * cg[ik - 1]);
  }
  for (int ik = ikc + 1; ik <= nk; ++ik) {
    coef4[ik - 1] = zero;
    coef5[ik - 1] = zero;
  }

  if (flogrd57) {
    // Total whitecap coverage, Reul and Chapron 2003. A recurrence in IK order (:2615).
    for (int ik = ik1; ik <= ikc; ++ik) {
      whitecap[0] = whitecap[0] + coef4[ik - 1] * (one - whitecap[0]);
      whitecap[3] = whitecap[3] + coef5[ik - 1];
    }
  }
  if (flogrd58) {
    // Mean foam thickness, Reul and Chapron 2003, Fig. 3 (:2620-2645).
    for (int ik = ik1; ik <= nk; ++ik) {
      const Real tstr = sds4_detail::kTstrFac / f.sig(ik);
      const Real tmax = sds4_detail::kTmaxFac / f.sig(ik);
      const Real dt = tmax / static_cast<Real>(50);
      Real mft = zero;
      for (int it = 1; it <= 50; ++it) {
        const Real t = static_cast<Real>(it) * dt;
        if (t < tstr) {
          mft = mft + ((static_cast<Real>(0.4) / (k[ik - 1] * tstr)) * t) * dt;
        } else {
          mft = mft + ((static_cast<Real>(0.4) / k[ik - 1]) *
                       dm::expf(-((t - tstr) / static_cast<Real>(3.8)))) *
                          dt;
        }
      }
      mft = mft / tmax;
      whitecap[1] = whitecap[1] + coef4[ik - 1] * mft;
    }
  }
}

/// W3SDS4 for one point. a = A(1:NSPEC), k = K(1:NK), cg = CG(1:NK), srhs,
/// ddiag, brlambda = (1:NSPEC), whitecap = WHITECAP(1:4), all 0-based.
/// flogrd57, flogrd58 are FLOGRD(5,7), FLOGRD(5,8) as 0/1. IX, IY are not
/// read and not passed. The OUT arrays arrive holding the caller's values:
/// the early return at w3src4md.F90:2581 leaves WHITECAP as it was.
/// The routine's sections are the sds4_* parts above, called in the routine's order.
KOKKOS_INLINE_FUNCTION void w3sds4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* k, const Real* cg, Real ustar, Real usdir,
                                         Real depth, Real dair, Real dlwmean, int flogrd57,
                                         int flogrd58, Real* srhs, Real* ddiag, Real* brlambda,
                                         Real* whitecap, Real* work, int* iwork) {
  const Sds4Args pt{a, k, cg, ustar, usdir, depth, dair, dlwmean, flogrd57, flogrd58,
                    srhs, ddiag, brlambda, whitecap};
  const Sds4Work w = sds4_layout(p, work, iwork);
  Sds4Mid mid;
  sds4_prep(p, f, pt, w, mid);
  sds4_mss(p, f, pt, w, mid);
  switch (ww::st4::nint(p.ssdsc[0])) {  // :2259
    case 1:
      sds4_sat(p, f, pt, w, mid);
      sds4_case1(p, f, pt, w, mid);
      break;
    case 2:
      sds4_case2(p, f, pt, w, mid);
      break;
    case 3:
      sds4_case3(p, f, pt, w, mid);
      break;
    default:
      break;
  }
  sds4_turb(p, f, pt, w, mid);
  sds4_whitecap(p, f, pt, w, mid);
}

namespace sds4_detail {

using lanes::M8;
using lanes::V8;
using lanes::vload;

/// Section 1.b (:2228-2254) for the `n` points of a team, one point per vector lane. The loops
/// over IK and ITH stay outside, each lane keeps its own three sums in ITH order; powf and
/// atan2f are called per lane. Lanes n..W-1 repeat point 0's inputs and are not stored.
template <int W>
inline void mss_lanes(const Params& p, const Fields& f, const int n, const Sds4Args* pt,
                      const Sds4Work* w) {
  static_assert(W == 8, "the ITH loops are written for V8");
  const Real zero = static_cast<Real>(0.);
  const Real* ssdsc = p.ssdsc;
  if (!(ssdsc[7] > zero || ssdsc[10] > zero || ssdsc[17] > zero)) return;
  const int nk = p.nk, nth = p.nth;
  const Real *a[W], *k[W], *cg[W];
  for (int l = 0; l < W; ++l) {
    const int q = l < n ? l : 0;
    a[l] = pt[q].a;
    k[l] = pt[q].k;
    cg[l] = pt[q].cg;
  }
  for (int ik = 1; ik <= nk; ++ik) {
    Real kp[W], cgk[W];
    for (int l = 0; l < W; ++l) {
      kp[l] = dm::powf(k[l][ik - 1], ssdsc[19]);
      cgk[l] = cg[l][ik - 1];
    }
    V8 kp_v, cg_v;
    vload(kp_v, kp);
    vload(cg_v, cgk);
    const Real dden = f.dden(ik - 1);
    V8 mssp = {}, msspc2 = {}, mssps2 = {};
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      const Real ecos = f.ecos(ith - 1), esin = f.esin(ith - 1);
      V8 av;
      for (int l = 0; l < W; ++l) av[l] = a[l][is - 1];
      const V8 msslong = ((kp_v * av) * dden) / cg_v;
      msspc2 = msspc2 + msslong * ecos;
      mssps2 = mssps2 + msslong * esin;
      mssp = mssp + msslong;
    }
    for (int l = 0; l < n; ++l) {
      Real* const msssum = w[l].msssum;
      auto MSS = [&](int i, int j) -> Real& { return msssum[(j - 1) * nk + (i - 1)]; };
      for (int i = ik; i <= nk; ++i) {
        MSS(i, 1) = MSS(i, 1) + mssp[l];
        MSS(i, 3) = MSS(i, 3) + msspc2[l];
        MSS(i, 4) = MSS(i, 4) + mssps2[l];
      }
      Real mssd = dm::atan2f(MSS(ik, 4), MSS(ik, 3));
      if (mssd < zero) mssd = mssd + kPi;
      MSS(ik, 2) = mssd;
    }
  }
}

/// Case 1, 2.a.1 (:2262-2290) for the `n` points of a team. The sum over the SATWEIGHTS terms
/// is DOT_PRODUCT's, from zero in J order, one accumulator per ITH and lane, so each BTH(IS)
/// has the bits of sds4_sat's; the zero weights are multiplied like the others. Without the
/// transposed tables the points go one after the other through sds4_sat.
template <int W>
inline void sat_lanes(const Params& p, const Fields& f, const int n, const Sds4Args* pt,
                      const Sds4Work* w, const Sds4Mid* mid) {
  const int nsat = static_cast<int>(f.satweights.extent(0));
  const bool tr = f.satweights_t.extent(1) == static_cast<size_t>(nsat);
  if (!tr) {
    for (int l = 0; l < n; ++l) sds4_sat(p, f, pt[l], w[l], mid[l]);
    return;
  }
  const Real zero = static_cast<Real>(0.);
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  const Real dth = p.dth;
  const Real *a[W], *k[W];
  Real *bth[W], *bth0[W];
  for (int l = 0; l < W; ++l) {
    const int q = l < n ? l : 0;
    a[l] = pt[q].a;
    k[l] = pt[q].k;
    bth[l] = w[q].bth;
    bth0[l] = w[q].bth0;
  }
  for (int l = 0; l < n; ++l)
    for (int i = 0; i < nspec; ++i) bth[l][i] = zero;
  const int ik1 = mid[0].ik1;
  for (int ik = ik1; ik <= nk; ++ik) {
    Real facsat[W];
    for (int l = 0; l < W; ++l) {
      const Real kk = k[l][ik - 1];
      facsat[l] = (f.sig(ik) * ((kk * kk) * kk)) * dth;
    }
    V8 facsat_v;
    vload(facsat_v, facsat);
    const int is0 = (ik - 1) * nth;
    for (int l = 0; l < n; ++l) bth[l][is0] = zero;
    V8 asum = {};
    for (int ith = 1; ith <= nth; ++ith) {
      V8 av;
      for (int l = 0; l < W; ++l) av[l] = a[l][is0 + ith - 1];
      asum = asum + av;
    }
    const V8 b0 = asum * facsat_v;
    for (int l = 0; l < n; ++l) bth0[l][ik - 1] = b0[l];
    if (p.ssdsdth >= static_cast<Real>(180)) {
      for (int l = 0; l < n; ++l)
        for (int i = is0; i < is0 + nth; ++i) bth[l][i] = bth0[l][ik - 1];
    } else {
      for (int ith = 0; ith < nth; ++ith) {
        V8 acc = {};
        for (int j = 0; j < nsat; ++j) {
          const Real wj = f.satweights_t(ith, j);
          const int xj = f.satindices_t(ith, j);
          V8 av;
          for (int l = 0; l < W; ++l) av[l] = a[l][is0 + xj - 1];
          acc = acc + wj * av;
        }
        acc = acc * facsat_v;
        for (int l = 0; l < n; ++l) bth[l][is0 + ith] = acc[l];
      }
      for (int l = 0; l < n; ++l) {
        Real* const bl = bth[l];
        // MAXVAL (:2287), as gfortran lowers it: first element >= -Inf, then strictly greater.
        Real lim = -Kokkos::Experimental::infinity_v<Real>;
        int i = is0;
        bool found = false;
        for (; i < is0 + nth; ++i) {
          if (bl[i] >= lim) { lim = bl[i]; ++i; found = true; break; }
        }
        if (!found) lim = Kokkos::Experimental::quiet_NaN_v<Real>;
        for (; i < is0 + nth; ++i)
          if (bl[i] > lim) lim = bl[i];
        bth0[l][ik - 1] = lim;
      }
    }
  }
}

/// The wave-turbulence interaction and cumulative effect (:2533-2575) for the `n` points of a
/// team. Each lane keeps its own RENEWALFREQ and its own sums in the routine's order; the
/// branch on BTH0 is a select of the value the taken side computes; COS/SIN(USDIR) is computed
/// once per lane, the same call the routine repeats for each ITH.
template <int W>
inline void turb_lanes(const Params& p, const Fields& f, const int n, const Sds4Args* pt,
                       const Sds4Work* w, const Sds4Mid* mid) {
  const Real zero = static_cast<Real>(0.);
  const Real* ssdsc = p.ssdsc;
  if (!((ssdsc[2] != zero) || (ssdsc[4] != zero) || (ssdsc[20] != zero))) return;
  const int nk = p.nk, nth = p.nth;
  const Real dth = p.dth;
  const int dikcumul = mid[0].dikcumul, ik1 = mid[0].ik1;
  const Real *a[W], *k[W];
  Real *ddiag[W], *srhs[W], *brlambda[W], *bth0[W];
  Real su[W], cu[W], facturb[W];
  for (int l = 0; l < W; ++l) {
    const int q = l < n ? l : 0;
    a[l] = pt[q].a;
    k[l] = pt[q].k;
    ddiag[l] = pt[q].ddiag;
    srhs[l] = pt[q].srhs;
    brlambda[l] = pt[q].brlambda;
    bth0[l] = w[q].bth0;
    facturb[l] = mid[q].facturb;
    dm::sincosf(pt[q].usdir, su[l], cu[l]);
  }
  V8 su_v, cu_v;
  vload(su_v, su);
  vload(cu_v, cu);
  for (int ik = ik1; ik <= nk; ++ik) {
    V8 renewalfreq = {};
    Real facturb2[W], dvisc[W], c[W];
    for (int l = 0; l < W; ++l) {
      const Real kk = k[l][ik - 1];
      facturb2[l] = -(((f.sig(ik) * static_cast<Real>(2.)) * kk) * facturb[l]);
      dvisc[l] = -(((ssdsc[20] * static_cast<Real>(4.)) * kk) * kk);
      c[l] = f.sig(ik) / kk;
    }
    V8 facturb2_v, dvisc_v;
    vload(facturb2_v, facturb2);
    vload(dvisc_v, dvisc);
    if (ssdsc[2] > zero && ik > dikcumul) {
      // The cheap isotropic version.
      for (int ik2 = ik1; ik2 <= ik - dikcumul; ++ik2) {
        Real cw[W];
        for (int l = 0; l < W; ++l) {
          const Real c2 = f.sig(ik2) / k[l][ik2 - 1];
          cw[l] = ((Kokkos::fabs(c2 - c[l]) * f.dsip(ik2)) / (c2 * static_cast<Real>(0.5))) * dth;
        }
        const int is2 = (ik2 - 1) * nth;
        V8 v = {};
        for (int i = is2; i < is2 + nth; ++i) {
          V8 bl;
          for (int l = 0; l < W; ++l) bl[l] = brlambda[l][i];
          v = v + bl;
        }
        V8 cw_v;
        vload(cw_v, cw);
        renewalfreq = renewalfreq - cw_v * v;
      }
    }
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      if (ssdsc[2] < zero && ik > dikcumul) {
        // The default (SSDSC(3) < 0), O(NK^2 NTH^2) branch (:2554-2563).
        renewalfreq = V8{};
        for (int ik2 = ik1; ik2 <= ik - dikcumul; ++ik2) {
          M8 on;
          for (int l = 0; l < W; ++l) on[l] = bth0[l][ik2 - 1] > p.ssdsbr ? -1 : 0;
          // No lane takes the branch: the sum below would be discarded.
          bool any = false;
          for (int l = 0; l < W; ++l) any = any || on[l] != 0;
          if (!any) continue;
          const int is2 = (ik2 - 1) * nth;
          V8 v = {};
          for (int j = 1; j <= nth; ++j) {
            const Real cw = f.cumulw(is2 + j - 1, is - 1);
            V8 bl;
            for (int l = 0; l < W; ++l) bl[l] = brlambda[l][is2 + j - 1];
            v = v + cw * bl;
          }
          renewalfreq = on ? renewalfreq + v : renewalfreq;
        }
      }
      const Real ecos = f.ecos(ith - 1), esin = f.esin(ith - 1);
      const V8 coswind = ecos * cu_v + esin * su_v;
      // MAX(COSWIND, 0.) is coswind > 0 ? coswind : 0 (max_r).
      const V8 dturb = facturb2_v * (coswind > zero ? coswind : V8{});
      const V8 diag2 = (ssdsc[2] * renewalfreq + dturb) + dvisc_v;
      for (int l = 0; l < n; ++l) {
        ddiag[l][is - 1] = ddiag[l][is - 1] + diag2[l];
        srhs[l][is - 1] = srhs[l][is - 1] + a[l][is - 1] * diag2[l];
      }
    }
  }
}

}  // namespace sds4_detail

/// W3SDS4 for the `n` points of a team (1 <= n <= W), `pt[l]` and `w[l]` being the arguments and
/// work arrays of point l: the sections of w3sds4_point in its order, the loops that gain from it
/// with one point per vector lane (plan 2026-10-18-005, unit L3), the rest point after point. The
/// libmvec sites (sds4_case1, sds4_case3) stay one grouped call per point. Every point gets the
/// bits w3sds4_point gives it.
template <int W>
inline void w3sds4_lanes(const Params& p, const Fields& f, const int n, const Sds4Args* pt,
                         const Sds4Work* w) {
  Sds4Mid mid[W] = {};
  for (int l = 0; l < n; ++l) sds4_prep(p, f, pt[l], w[l], mid[l]);
  sds4_detail::mss_lanes<W>(p, f, n, pt, w);
  switch (ww::st4::nint(p.ssdsc[0])) {  // :2259
    case 1:
      sds4_detail::sat_lanes<W>(p, f, n, pt, w, mid);
      for (int l = 0; l < n; ++l) sds4_case1(p, f, pt[l], w[l], mid[l]);
      break;
    case 2:
      for (int l = 0; l < n; ++l) sds4_case2(p, f, pt[l], w[l], mid[l]);
      break;
    case 3:
      for (int l = 0; l < n; ++l) sds4_case3(p, f, pt[l], w[l], mid[l]);
      break;
    default:
      break;
  }
  sds4_detail::turb_lanes<W>(p, f, n, pt, w, mid);
  for (int l = 0; l < n; ++l) sds4_whitecap(p, f, pt[l], w[l], mid[l]);
}

}  // namespace ww::st4
