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

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_libmvec.hpp"
#include "st4_params.hpp"

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

/// NINT: lroundf, then truncation to INTEGER(4) (INVENTORY.md, global rules).
KOKKOS_INLINE_FUNCTION int nint(Real x) {
  return static_cast<int>(static_cast<long>(Kokkos::round(x)));
}

/// FLOOR(x) as the -O3 dump computes it at :2590: i = (int)x; (float)i <= x ? i : i-1,
/// with the wrap of -fno-strict-overflow, so a NaN gives INT_MAX as in the Fortran.
KOKKOS_INLINE_FUNCTION int ffloor(Real x) {
  const int i = fint(x);
  return static_cast<Real>(i) <= x ? i : static_cast<int>(static_cast<unsigned>(i) - 1u);
}

/// Fortran MAX/MIN of two REALs. NaN and the sign of a zero are unspecified in
/// gfortran (maxss operand order per site); only :2512 and :2514 were read in
/// the assembly and have their own expressions below.
KOKKOS_INLINE_FUNCTION Real max2(Real a, Real b) { return a > b ? a : b; }
KOKKOS_INLINE_FUNCTION Real min2(Real a, Real b) { return a < b ? a : b; }

}  // namespace sds4_detail

/// Reals of per-point work storage: 13 arrays of NK (COEF4, COEF5, S1, E1, DK,
/// HS, KBAR, DCK, EFDF, BTH0, QB, S2, BRM12), MSSSUM(NK,5), BTH, PB, PB2 of
/// NSPEC, LMODULATION of NTH.
inline int sds4_work_reals(const Params& p) { return 18 * p.nk + 3 * p.nspec + p.nth; }
/// Ints: NTIMES and IKSUP, NK each. NSMOOTH (:2163) is zeroed and never read: not kept.
inline int sds4_work_ints(const Params& p) { return 2 * p.nk; }

/// W3SDS4 for one point. a = A(1:NSPEC), k = K(1:NK), cg = CG(1:NK), srhs,
/// ddiag, brlambda = (1:NSPEC), whitecap = WHITECAP(1:4), all 0-based.
/// flogrd57, flogrd58 are FLOGRD(5,7), FLOGRD(5,8) as 0/1. IX, IY are not
/// read and not passed. The OUT arrays arrive holding the caller's values:
/// the early return at w3src4md.F90:2581 leaves WHITECAP as it was.
KOKKOS_INLINE_FUNCTION void w3sds4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* k, const Real* cg, Real ustar, Real usdir,
                                         Real depth, Real dair, Real dlwmean, int flogrd57,
                                         int flogrd58, Real* srhs, Real* ddiag, Real* brlambda,
                                         Real* whitecap, Real* work, int* iwork) {
  using sds4_detail::max2;
  using sds4_detail::min2;
  using sds4_detail::nint;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const int nk = p.nk, nth = p.nth, nspec = p.nspec;
  const Real dth = p.dth;
  const Real* ssdsc = p.ssdsc;  // SSDSC(i) is ssdsc[i-1]

  // Work storage, 1-based helpers below take the Fortran index.
  Real* coef4 = work;
  Real* coef5 = coef4 + nk;
  Real* s1 = coef5 + nk;
  Real* e1 = s1 + nk;
  Real* dk = e1 + nk;
  Real* hs = dk + nk;
  Real* kbar = hs + nk;
  Real* dck = kbar + nk;
  Real* efdf = dck + nk;
  Real* bth0 = efdf + nk;
  Real* qb = bth0 + nk;
  Real* s2 = qb + nk;
  Real* brm12 = s2 + nk;
  Real* msssum = brm12 + nk;  // MSSSUM(IK,J) = msssum[(J-1)*nk + IK-1]
  Real* bth = msssum + 5 * nk;
  Real* pb = bth + nspec;
  Real* pb2 = pb + nspec;
  Real* lmodulation = pb2 + nspec;
  int* ntimes = iwork;
  int* iksup = ntimes + nk;
  auto MSS = [&](int ik, int j) -> Real& { return msssum[(j - 1) * nk + (ik - 1)]; };

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
  Real renewalfreq = zero;
  int ik1 = 1;
  if (p.ig1) ik1 = nint(p.igpars5) + 1;  // W3_IG1, :2223

  // 1.b MSS parameters for the modulation of lambda, Romero (:2228-2254).
  if (ssdsc[7] > zero || ssdsc[10] > zero || ssdsc[17] > zero) {
    for (int ik = 1; ik <= nk; ++ik) {
      Real mssp = zero, msspc2 = zero, mssps2 = zero;
      const Real kp = Kokkos::pow(k[ik - 1], ssdsc[19]);
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
      Real mssd = Kokkos::atan2(MSS(ik, 4), MSS(ik, 3));
      if (mssd < zero) mssd = mssd + kPi;
      MSS(ik, 2) = mssd;
    }
  }

  // 2. Spontaneous breaking from local saturation (:2259).
  switch (nint(ssdsc[0])) {
    case 1: {
      // 2.a Direction-dependent breaking, Ardhuin et al. 2010 (:2260-2331).
      const Real epsr = Kokkos::sqrt(p.ssdsbr);
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
          for (int ith = 1; ith <= nth; ++ith) {
            const int is = ith + (ik - 1) * nth;
            // Full length 2*(MTH/2-1)+1 for any SDSNTH (:2283): the extra terms have weight 0
            // and index 1, and 0*x is not 0 for an infinite x; the length sets the order.
            Real val = zero;
            for (int j = 0; j < nsat; ++j)
              val = val + f.satweights(j, ith - 1) * a[is0 + f.satindices(j, ith - 1) - 1];
            bth[is - 1] = val * facsat;
          }
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
      // 2.a.2 Spontaneous breaking dissipation rate.
      for (int ik = ik1; ik <= nk; ++ik) {
        Real miche;
        if (p.ssdsbm[0] == one) {
          miche = one;
        } else {
          const Real x = Kokkos::tanh(min2(k[ik - 1] * depth, static_cast<Real>(10.)));
          const Real t =
              x * (p.ssdsbm[1] + x * (p.ssdsbm[2] + x * (p.ssdsbm[3] + x * p.ssdsbm[4])));
          miche = t * t;
        }
        const Real coef1 = p.ssdsbr * miche;
        const Real m = max2(bth0[ik - 1] / coef1 - one, zero);
        const Real sdiagiso = ((ssdsc[1] * f.sig(ik)) * ssdsc[5]) * (m * m);
        const Real coef2 = ((ssdsc[1] * f.sig(ik)) * (one - ssdsc[5])) / (coef1 * coef1);
        // LIBMVEC SITE: -O3 uses _ZGVbN4vv_powf here (INVENTORY.md, :2315), lane i = ITH-1.
        const int is0 = (ik - 1) * nth;
        const Real ssdsp = p.ssdsp;
        mvec_powf(
            nth, [&](int i) { return max2(bth[is0 + i] - coef1, zero); },
            [&](int) { return ssdsp; },
            [&](int i, Real v) { ddiag[is0 + i] = sdiagiso + coef2 * v; });
      }
      // Breaking probability (:2320-2327), lambda (:2330), source (:2331).
      for (int i = 0; i < nspec; ++i) {
        const Real m = max2(Kokkos::sqrt(bth[i]) - epsr, zero);
        pb[i] = m * m;
      }
      for (int i = 0; i < nspec; ++i) pb[i] = pb[i] * static_cast<Real>(28.16);
      for (int i = 0; i < nspec; ++i) brlambda[i] = pb[i] / sds4_detail::kTwoPiSq;
      for (int i = 0; i < nspec; ++i) srhs[i] = ddiag[i] * a[i];
      break;
    }
    case 2: {
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
          int ikd = fint(Kokkos::round(Kokkos::log(kb * depth) / sds4_detail::kLogFacKd1) +
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
          const Real xt = Kokkos::tanh(kb * depth);
          // GAM (:2411) is computed and never used.
          const Real gam = ((((xt * xt) * xt) * static_cast<Real>(1.0314) -
                             (xt * xt) * static_cast<Real>(1.9958)) +
                            xt * static_cast<Real>(1.5522)) +
                           static_cast<Real>(0.1885);
          (void)gam;
          dck[ikl - 1] = (Kokkos::pow(kb, static_cast<Real>(-2.5)) * (kb / sds4_detail::kTwoPi)) *
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
      break;
    }
    case 3: {
      // 2.c Romero (GRL 2019) (:2481-2520). Division by USTAR**2 kept: Inf at USTAR = 0 (:2485).
      const Real t28 = static_cast<Real>(28.) / ssdsc[15];
      const Real ko = (kGrav / (ustar * ustar)) / (t28 * t28);
      for (int ik = 1; ik <= nk; ++ik) {
        const int is0 = (ik - 1) * nth;
        const Real kk = k[ik - 1];
        const Real kloc = Kokkos::pow(kk, static_cast<Real>(2.) - ssdsc[19]);
        const Real k3 = (kk * kk) * kk;
        // BTH(1:NTH) is the scratch of this IK.
        for (int ith = 1; ith <= nth; ++ith)
          bth[ith - 1] =
              max2((a[is0 + ith - 1] * f.sig(ik)) * k3, static_cast<Real>(.00000000000001));
        Real dirforcum = dlwmean;
        if (ssdsc[10] > zero) dirforcum = MSS(ik, 2);
        const Real c = f.sig(ik) / kk;
        Real sb = zero;
        for (int ith = 1; ith <= nth; ++ith) sb = sb + bth[ith - 1] * dth;
        bth0[ik - 1] = sb;
        if (ssdsc[17] > zero) {
          const Real cd = Kokkos::cos(dirforcum), sd = Kokkos::sin(dirforcum);
          const Real pm = Kokkos::pow(MSS(ik, 1) * kloc, ssdsc[13]);
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
        const Real fk = one + ssdsc[12] * Kokkos::pow(max2(kk / ko, one), ssdsc[14]);
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
            ssdsc[1] * Kokkos::pow(zero > btover ? zero : btover, static_cast<Real>(2.5));
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
              ((srhs[is0 + ith - 1] * p.ssdsbr) / max2(bth[ith - 1], static_cast<Real>(1.e-20))) /
              max2(a[is0 + ith - 1], static_cast<Real>(1e-20));
      }
      break;
    }
    default:
      break;
  }

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
        const Real coswind =
            f.ecos(ith - 1) * Kokkos::cos(usdir) + f.esin(ith - 1) * Kokkos::sin(usdir);
        const Real dturb = facturb2 * max2(coswind, zero);
        const Real diag2 = (ssdsc[2] * renewalfreq + dturb) + dvisc;
        ddiag[is - 1] = ddiag[is - 1] + diag2;
        srhs[is - 1] = srhs[is - 1] + a[is - 1] * diag2;
      }
    }
  }

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
    const Real pw = Kokkos::pow(brm12[ik - 1] / ssdsc[1], static_cast<Real>(3.) / static_cast<Real>(5.));
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
                       Kokkos::exp(-((t - tstr) / static_cast<Real>(3.8)))) *
                          dt;
        }
      }
      mft = mft / tmax;
      whitecap[1] = whitecap[1] + coef4[ik - 1] * mft;
    }
  }
}

}  // namespace ww::st4
