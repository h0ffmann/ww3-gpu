// kokkos/src/ww_kokkos/srce_tr1.hpp
// W3STR1: triad interactions by the Lumped Triad Approximation (Eldeberky 1996), one
// spectral point. Phase 1, translated, not regtested. WW3 heritage:
// WW3-noaa/model/src/w3str1md.F90:183-543 at NOAA-EMC/WW3 03adda91; authors
// A. J. van der Westhuysen, A. Roland. Evidence: kokkos/port_report/W3STR1_KOKKOS.md.
// The shim copies S and D in as well as out: MAXVAL(A) < TINY returns before writing
// (:395-397), and with URSELL >= 0.01 the IK = NK row is not written (:522).
// sig points at SIG(1) inside SIG(0:NK+1): with NK = 2 or 3 the Fortran reads SIG(0), sig[-1].
// Unchecked, as in the Fortran: ISP+1 <= NK and NK+ISP <= 100 (the columns of SA).
// ⚠ On a device logf, tanhf, sinf and sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "st4_params.hpp"

namespace ww::srce {

/// SA(NTH,100), w3str1md.F90:379: the column count is a literal in the source.
inline constexpr int kTr1SaCols = 100;

namespace tr1_detail {
// Fortran MAX as maxss: fmax2(a,b) = b>a?b:a, a NaN or a +0/-0 tie gives `a`. Each call
// passes the operands in the order of w3str1md.o.
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }

// x**n for a run-time INTEGER n as libgcc's __powisf2; never std::pow.
KOKKOS_INLINE_FUNCTION Real powi(Real x, int m) {
  unsigned int n = (m < 0) ? 0u - static_cast<unsigned int>(m) : static_cast<unsigned int>(m);
  Real y = (n % 2u) != 0u ? x : static_cast<Real>(1);
  while ((n >>= 1u) != 0u) {
    x = x * x;
    if ((n % 2u) != 0u) y = y * x;
  }
  return (m < 0) ? static_cast<Real>(1) / y : y;
}

// Folded REAL constants, as the bits of w3str1md.o's GIMPLE.
inline constexpr Real k2Sqrt2 = 0x1.6a09e6p+1f;    // 2.*SQRT(2.) :442, 0x403504f3
inline constexpr Real kC215G = 0x1.4eb624p+0f;     // (2./15.)*GRAV :515, 0x3fa75b12
static_assert(kC215G == (static_cast<Real>(2.) / static_cast<Real>(15.)) * kGrav,
              "(2./15.)*GRAV folded as REAL");
inline constexpr Real kLn2 = 0x1.62e430p-1f;       // LOG(2.) :457, 0x3f317218
inline constexpr Real kLnHalf = -0x1.62e430p-1f;   // LOG(0.5) :462, 0xbf317218
inline constexpr Real kHalfPi = 0x1.921fb6p+0f;    // 0.5*PI :486, 0x3fc90fdb
static_assert(kHalfPi == static_cast<Real>(0.5) * kPi, "0.5*PI folded as REAL");
inline constexpr Real kTiny1 = 0x1p-126f;          // TINY(1.) :395
inline constexpr Real kJacEps = 1E-12f;            // JACEPS :401, 0x2b8cbccc
}  // namespace tr1_detail

/// W3STR1 for one point, arrays 0-based. e, eb (NK) and sa (NTH*kTr1SaCols, SA(ITH,J) at
/// sa[(J-1)*NTH+ITH-1]) are work for the locals E, EB, SA.
KOKKOS_INLINE_FUNCTION void w3str1_point(const Params& p, const Real* sig, const Real* dden,
                                         Real fte, Real ftf, const Real* a, const Real* cg,
                                         const Real* wn, Real depth, Real* s, Real* d,
                                         Real* e, Real* eb, Real* sa) {
  using tr1_detail::fmax2;
  using tr1_detail::powi;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;

  // :395-397. MAXVAL as the build inlines it; NSPEC <= 0 gives -HUGE and returns.
  {
    int i = 0;
    while (i < nspec && !(a[i] >= -Kokkos::infinity_v<Real>)) ++i;
    if (nspec <= 0) return;
    if (i < nspec) {
      Real m = a[i];
      for (int j = i + 1; j < nspec; ++j) m = (a[j] > m) ? a[j] : m;
      if (m < tr1_detail::kTiny1) return;
    }
  }

  Real sigm01 = 0;
  Real emean = 0;
  const Real jaceps = tr1_detail::kJacEps;

  // :405-412 (ED, never read, dropped).
  for (int ik = 1; ik <= nk; ++ik) {
    eb[ik - 1] = 0;
    for (int ith = 1; ith <= nth; ++ith) {
      eb[ik - 1] = eb[ik - 1] + a[ith + (ik - 1) * nth - 1];
    }
  }

  // :416-420.
  for (int ik = 1; ik <= nk; ++ik) {
    eb[ik - 1] = eb[ik - 1] * dden[ik - 1] / cg[ik - 1];
    emean = emean + eb[ik - 1];
    sigm01 = sigm01 + eb[ik - 1] * sig[ik - 1];
  }

  // :425-427.
  const Real eband = eb[nk - 1] / dden[nk - 1];
  emean = emean + eband * fte;
  sigm01 = sigm01 + eband * ftf;

  // :431.
  sigm01 = sigm01 / emean;

  // :435-439. PTRIAD(3) is not used.
  const Real ptriad2 = static_cast<Real>(10.);
  const Real ptriad4 = static_cast<Real>(0.2);
  const Real ptriad5 = static_cast<Real>(0.01);

  // :441-442.
  const Real hs = static_cast<Real>(4.) * Kokkos::sqrt(fmax2(static_cast<Real>(0.), emean));
  const Real ursell = (kGrav * hs) /
                      (tr1_detail::k2Sqrt2 * (sigm01 * sigm01) * (depth * depth));

  // :445-448.
  const Real dep = depth;
  const Real dep_2 = dep * dep;
  const Real dep_3 = dep_2 * dep;
  const Real g9dep = kGrav * dep;

  // :452-465. SIG(I2), SIG(I1) with 1-based I1, I2.
  const int i2 = ww::st4::fint(static_cast<Real>(nk) / static_cast<Real>(2.));
  const int i1 = i2 - 1;
  const Real xis = sig[i2 - 1] / sig[i1 - 1];
  const Real xisln = Kokkos::log(xis);

  const int isp = ww::st4::fint(tr1_detail::kLn2 / xisln);  // INT() as cvttss2si
  const int isp1 = isp + 1;
  const Real xis_isp = powi(xis, isp);
  const Real wisp = (static_cast<Real>(2.) - xis_isp) / (powi(xis, isp1) - xis_isp);
  const Real wisp1 = static_cast<Real>(1.) - wisp;

  const int ism = ww::st4::fint(tr1_detail::kLnHalf / xisln);
  const int ism1 = ism - 1;
  const Real xis_ism = powi(xis, ism);
  const Real wism = (xis_ism - static_cast<Real>(0.5)) / (xis_ism - powi(xis, ism1));
  const Real wism1 = static_cast<Real>(1.) - wism;

  // :467-468.
  for (int ik = 1; ik <= nk; ++ik) e[ik - 1] = 0;
  for (int j = 0; j < nth * kTr1SaCols; ++j) sa[j] = 0;

  // :472-478. The last IK with SIG(IK) < PTRIAD(2)*SIGM01.
  int ismax = 1;
  for (int ik = 1; ik <= nk; ++ik) {
    if (sig[ik - 1] < (ptriad2 * sigm01)) ismax = ik;
  }
  ismax = (ismax > isp1) ? ismax : isp1;

  // :482.
  if (ursell >= ptriad5) {
    // :486-487.
    const Real biph =
        tr1_detail::kHalfPi * (Kokkos::tanh(ptriad4 / ursell) - static_cast<Real>(1.));
    const Real sinbph = Kokkos::fabs(Kokkos::sin(biph));

    for (int ith = 1; ith <= nth; ++ith) {
      // :491-494.
      for (int ik = 1; ik <= nk; ++ik) {
        e[ik - 1] = a[ith + (ik - 1) * nth - 1] * kTpi * sig[ik - 1] / cg[ik - 1];
      }
      // :495-519.
      for (int ik = 1; ik <= ismax; ++ik) {
        const Real e0 = e[ik - 1];
        const Real w0 = sig[ik - 1];
        const Real wn0 = wn[ik - 1];
        const Real c0 = w0 / wn0;
        Real em, wm, wnm, cm;
        if (ik > -ism1) {
          em = wism * e[ik + ism1 - 1] + wism1 * e[ik + ism - 1];
          wm = wism * sig[ik + ism1 - 1] + wism1 * sig[ik + ism - 1];
          wnm = wism * wn[ik + ism1 - 1] + wism1 * wn[ik + ism - 1];
          cm = wm / wnm;
        } else {
          em = 0;
          wm = 0;
          wnm = 0;
          cm = 0;
        }
        const Real aux1 = (wnm * wnm) * (g9dep + static_cast<Real>(2.) * (cm * cm));
        const Real aux2 = wn0 * dep *
                          (g9dep + tr1_detail::kC215G * dep_3 * (wn0 * wn0) -
                           static_cast<Real>(0.4) * (w0 * w0) * dep_2);
        const Real rint = aux1 / aux2;
        const Real ft = c0 * cg[ik - 1] * (rint * rint) * sinbph;
        sa[(ik - 1) * nth + ith - 1] =
            fmax2(static_cast<Real>(0.), ft * (em * em - static_cast<Real>(2.) * em * e0));
      }
    }

    // :522-534.
    for (int ik = 1; ik <= nk - 1; ++ik) {
      const Real sigpicg = sig[ik - 1] * kTpi / cg[ik - 1];
      for (int ith = 1; ith <= nth; ++ith) {
        const Real stri = sa[(ik - 1) * nth + ith - 1] -
                          static_cast<Real>(2.) * (wisp * sa[(ik + isp1 - 1) * nth + ith - 1] +
                                                   wisp1 * sa[(ik + isp - 1) * nth + ith - 1]);
        const int is = ith + (ik - 1) * nth - 1;
        if (a[is] > jaceps) {
          d[is] = stri / (a[is] * sigpicg);
          s[is] = stri / sigpicg;
        } else {
          d[is] = 0;
          s[is] = 0;
        }
      }
    }
  } else {
    // :535-537.
    for (int is = 0; is < nspec; ++is) d[is] = 0;
    for (int is = 0; is < nspec; ++is) s[is] = 0;
  }
}

}  // namespace ww::srce
