// kokkos/src/ww_kokkos/st4_insin4.cpp
// INSIN4, the ST4 initialisation: saturation indices and weights, the IKTAB,
// DCKI and QBI tables of the bandwidth-integrated breaking, the cumulative
// weights CUMULW and, when FLTABS, the stress tables. Host code: it runs once
// per grid at initialisation.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:1014-1288 at WW3@bb08623a, author
// F. Ardhuin (origination 30-Aug-2010); WAVNU2 from w3dispmd.F90:204-346, author
// H. L. Tolman. Phase 1, translated, not regtested. Section numbers are the
// Fortran's. Arithmetic order follows the -O3 GIMPLE of WW3's build (gfortran
// 16.2.0, -fdump-tree-optimized of w3src4md.F90 and w3dispmd.F90).
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "st4_tables_build.hpp"

namespace ww::st4 {
namespace {

/// Fortran x**n for a runtime INTEGER n: libgcc's __powisf2, which gfortran
/// calls at -O3 (__builtin_powif in the dump). Copied from snl1_tables.cpp:34.
Real powi(Real x, int n) {
  unsigned int m = (n < 0) ? static_cast<unsigned int>(-n) : static_cast<unsigned int>(n);
  Real y = (m & 1u) ? x : static_cast<Real>(1);
  while ((m >>= 1u) != 0u) {
    x = x * x;
    if ((m & 1u) != 0u) y = y * x;
  }
  return (n < 0) ? static_cast<Real>(1) / y : y;
}

/// Fortran NINT on REAL: round half away from zero (__builtin_iroundf).
int nint(Real x) { return static_cast<int>(std::lround(x)); }

/// WAVNU2(W, H, K, CG, EPS, NMAX, ICON), w3dispmd.F90:204-346: wavenumber by
/// Newton iteration on the dispersion relation. Compiled in its own unit, so
/// INSIN4 calls it out of line, as here.
void wavnu2(Real w, Real h, Real& k, Real& cg, Real eps, int nmax, int& icon) {
  const Real zero = static_cast<Real>(0);
  cg = zero;  // :287
  Real kold = zero;
  icon = 0;
  const Real w0 = std::fabs(w);
  // 1st approach (:295-299)
  if (w0 < std::sqrt(kGrav / h)) {
    k = w0 / std::sqrt(kGrav * h);
  } else {
    k = w0 * w0 / kGrav;
  }
  // Refinement (:303-322)
  Real dif, rdif;
  for (int i = 1; i <= nmax; ++i) {
    dif = std::fabs(k - kold);
    if (k != zero) {
      rdif = dif / k;
    } else {
      rdif = zero;
    }
    if (dif < eps && rdif < eps) {
      icon = 1;
      break;
    } else {
      kold = k;
      const Real f = kGrav * kold * std::tanh(kold * h) - w0 * w0;  // W0**2 (:313)
      Real fd;
      if (kold * h > static_cast<Real>(25)) {
        fd = kGrav * std::tanh(kold * h);
      } else {
        const Real ch = std::cosh(kold * h);
        fd = kGrav * std::tanh(kold * h) + kGrav * kold * h / (ch * ch);  // COSH**2 (:317)
      }
      k = kold - f / fd;
    }
  }
  if (icon == 0) {  // :324-328
    dif = std::fabs(k - kold);
    rdif = dif / k;
    if (dif < eps && rdif < eps) icon = 1;
  }
  const Real two = static_cast<Real>(2);
  const Real half = static_cast<Real>(0.5);
  if (two * k * h > static_cast<Real>(25)) {  // :329-333
    cg = w0 / k * half;
  } else {
    cg = w0 / k * half * (static_cast<Real>(1) + (two * k * h / std::sinh(two * k * h)));
  }
  if (w < static_cast<Real>(0.0)) {
    k = static_cast<Real>(-1) * k;
    cg = cg * static_cast<Real>(-1);
  }
}

}  // namespace

void insin4(Params& p, bool fltabs, HostArrays& a) {
  const Real zero = static_cast<Real>(0);
  const Real one = static_cast<Real>(1);
  const int nk = p.nk;
  const int nth = p.nth;

  // 1. Initializations (:1118-1129). TAUT, TAUHFT, TAUHFT2 are allocated by
  // allocate_tables() under the same conditions.
  if (p.sintailpar[0] > static_cast<Real>(0.5)) {
    if (fltabs) {
      tabu_stress(p, a);
      tabu_tauhf(p, a.sig(nk), a);  // SIG(NK): SIG is 0-based
    }
    if (p.ttauwshelter > 0) {
      if (fltabs) tabu_tauhf2(p, a.sig(nk), a);
    }
  }

  // 2. SPONTANEOUS BREAKING
  // 2.a Indices for integrating the spectrum to get saturation (:1134-1153).
  // SATINDICES/SATWEIGHTS are allocated with SDSNTH = MTH/2-1
  // (w3gdatmd.F90:2104), the fill uses this SDSNTH (:1135); the rest keeps 1/0.
  if (p.ssdsdth < static_cast<Real>(180)) {
    const int sdsnth = std::min(nint(p.ssdsdth / (p.dth * kRade)), nth / 2 - 1);
    Kokkos::deep_copy(a.satindices, 1);
    Kokkos::deep_copy(a.satweights, zero);
    for (int ith = 1; ith <= nth; ++ith) {
      for (int i_int = ith - sdsnth; i_int <= ith + sdsnth; ++i_int) {
        int j_int = i_int;
        if (i_int < 1) j_int = i_int + nth;
        if (i_int > nth) j_int = i_int - nth;
        const int row = i_int - (ith - sdsnth) + 1;  // Fortran index, 1-based
        a.satindices(row - 1, ith - 1) = j_int;      // a Fortran index, stored as such
        // REAL**REAL: powf (:1149-1150)
        a.satweights(row - 1, ith - 1) = std::pow(std::cos(a.th(ith - 1) - a.th(j_int - 1)), p.ssdscos);
      }
    }
  } else {
    Kokkos::deep_copy(a.satindices, 1);
    Kokkos::deep_copy(a.satweights, one);
  }

  // Precomputes QBI and DCKI (TEST 500) (:1158-1240)
  if (p.ssdsbck > 0) {
    const Real binf = (one - p.ssdsbint);  // :1162
    const Real bsup = (one + p.ssdsbint);
    Real kik = zero;
    Real cgg;
    int icon;
    // K1, K2, SIGTAB (:1168-1170) are only read at the element just written:
    // kept as scalars, same values.
    Kokkos::deep_copy(a.iktab, 0);  // IKTAB=0 (:1173)
    for (int id = 1; id <= kNdtab; ++id) {
      int toto = 0;
      const Real prof = static_cast<Real>(id);
      for (int ikl = 1; ikl <= nk; ++ikl) {
        wavnu2(a.sig(ikl), prof, kik, cgg, static_cast<Real>(1E-7), 15, icon);
        const Real k1 = kik;
        const Real r = bsup / binf;
        const Real k2 = (r * r) * k1;  // (BSUP/BINF)**2.: x*x at -O3 (:1181)
        const Real sigtab = std::sqrt(kGrav * k2 * std::tanh(k2 * static_cast<Real>(id)));
        if (sigtab <= a.sig(1)) a.iktab(ikl - 1, id - 1) = 1;
        if (sigtab > a.sig(nk)) {
          a.iktab(ikl - 1, id - 1) = nk + toto;  // :1187
          toto = 1;
        }
        for (int ik = 1; ik <= nk - 1; ++ik) {
          Real diff1 = zero;
          Real diff2 = zero;
          if (a.sig(ik) < sigtab && a.sig(ik + 1) >= sigtab) {
            diff1 = sigtab - a.sig(ik);
            diff2 = a.sig(ik + 1) - sigtab;
            if (diff1 < diff2) {
              a.iktab(ikl - 1, id - 1) = ik;
            } else {
              a.iktab(ikl - 1, id - 1) = ik + 1;
            }
          }
        }
      }
    }

    // Tabulates DCKI and QBI (:1206-1233)
    const Real dhs = kKhsMax / static_cast<Real>(kNkhs);
    const Real dkh = kKhMax / static_cast<Real>(kNkhi);
    // :1209-1210 re-ALLOCATEs DCKI and QBI, already allocated by W3DIMX: the old
    // arrays leak, the values are the same. Written into the existing arrays.
    Kokkos::deep_copy(a.dcki, zero);
    Kokkos::deep_copy(a.qbi, zero);
    const Real sqrt2 = std::sqrt(static_cast<Real>(2.));
    for (int ikd = 1; ikd <= kNkd; ++ikd) {
      Real khs = zero;
      Real xt, sqgxt;
      if (ikd == 1) {
        // gfortran -O3 peels IKD=1 and folds TANH(FAC_KD1**(1-FAC_KD2)) and
        // SQRT(GRAV/XT) at compile time (exact powi, MPFR), which differs from
        // the runtime __powisf2 chain by ~6e-6 relative: the dump's constants.
        xt = static_cast<Real>(4.818942034034989774227142333984375e-5);
        sqgxt = static_cast<Real>(4.51097137451171875e+2);
      } else {
        const Real kd = powi(kFacKd1, ikd - kFacKd2);  // :1216
        xt = std::tanh(kd);
        sqgxt = std::sqrt(kGrav / xt);
      }
      const Real xt2 = xt * xt;
      const Real xt3 = xt2 * xt;  // XT**3 = (XT*XT)*XT at -O3
      Real gam = static_cast<Real>(1.0314) * xt3 - static_cast<Real>(1.9958) * xt2 +
                 static_cast<Real>(1.5522) * xt + static_cast<Real>(0.1885);
      gam = gam / static_cast<Real>(2.15);
      for (int ikhs = 1; ikhs <= kNkhs; ++ikhs) {
        Real kh = zero;
        khs = khs + dhs;
        for (int ikh = 1; ikh <= kNkhi; ++ikh) {
          kh = kh + dkh;
          const Real q = kh / khs;
          const Real pr = (static_cast<Real>(4.) * kh / (khs * khs)) *
                          std::exp(-(static_cast<Real>(2) * (q * q)));  // **2. = x*x
          const Real s = khs / (sqrt2 * gam * xt);
          const Real w = p.ssdsabk * (s * s) *
                         (one - std::exp(-(std::pow(kh / (gam * xt), p.ssdspbk))));
          // **3. stays powf(x, 3.0) at -O3; /4 is *0.25 in the dump, same bits.
          const Real eps =
              -((std::pow((p.ssdsbck / std::pow(xt, p.ssdshck)) * kh, static_cast<Real>(3.))) /
                static_cast<Real>(4)) *
              sqgxt;
          a.dcki(ikhs - 1, ikd - 1) = a.dcki(ikhs - 1, ikd - 1) + pr * w * eps * dkh;
          a.qbi(ikhs - 1, ikd - 1) = a.qbi(ikhs - 1, ikd - 1) + pr * w * dkh;
        }
      }
    }

    // WHERE ( QBI .GT. 1. ) QBI = 1. (:1235-1237)
    for (int j = 0; j < kNkd; ++j)
      for (int i = 0; i < kNkhs; ++i)
        if (a.qbi(i, j) > one) a.qbi(i, j) = one;
  } else {
    Kokkos::deep_copy(a.iktab, 1);
    Kokkos::deep_copy(a.dcki, zero);
    Kokkos::deep_copy(a.qbi, zero);
  }

  // CUMULATIVE EFFECT (TEST 441 and 500) (:1252-1282).
  // DIKCUMUL is a W3SRC4MD module variable; W3SDS4 recomputes it (:2218), so it
  // stays local here.
  int dikcumul = 0;
  if (p.ssdsc[2] < zero) {
    dikcumul = nint(p.ssdsbrf1 / (p.xfr - one));
    Kokkos::deep_copy(a.cumulw, zero);
    for (int ik = 1; ik <= nk; ++ik) {
      const Real c = kGrav / a.sig(ik);
      for (int ith = 1; ith <= nth; ++ith) {
        const int is = ith + (ik - 1) * nth;
        for (int ik2 = 1; ik2 <= ik - dikcumul; ++ik2) {
          const Real c2 = kGrav / a.sig(ik2);
          for (int ith2 = 1; ith2 <= nth; ++ith2) {
            const int is2 = ith2 + (ik2 - 1) * nth;
            // ECOS(1+|ITH2-ITH|) as the cosine of the angle difference (:1269):
            // ECOS is 1-based, so ecos(|ITH2-ITH|).
            a.cumulw(is2 - 1, is - 1) =
                std::sqrt(c * c + c2 * c2 - static_cast<Real>(2) * c * c2 * a.ecos(std::abs(ith2 - ith))) *
                a.dsip(ik2) / (static_cast<Real>(0.5) * c2) * p.dth;
          }
        }
      }
    }
  } else {
    Kokkos::deep_copy(a.cumulw, zero);
  }
}

}  // namespace ww::st4
