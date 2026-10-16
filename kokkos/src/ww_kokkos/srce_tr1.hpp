// kokkos/src/ww_kokkos/srce_tr1.hpp
// W3STR1, ported to Kokkos: triad interactions by the Lumped Triad Approximation
// (LTA) of Eldeberky (1996), as in SWAN 40.91, one spectral point. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3str1md.F90:183-543 at NOAA-EMC/WW3 03adda91;
// authors A. J. van der Westhuysen, A. Roland. Switch TR1 (NOAA's switch_NWS_rwps,
// the ww3_tp2.17 and ww3_tp2.19 PDLIB/RWPS switches, ww3_tp1.9's switch and switch_MPI).
// Called by W3SRCE at w3srcemd.F90:1283; under PDLIB only when .NOT. FSSOURCE .OR. LSLOC
// (:1280-1286). VSTR/VDTR enter the VS/VD sums without PDLIB (:1490, :1509) and, under
// PDLIB, B_JAC/ASPAR_JAC (:1643-1655) and the direct update (:1782-1787). The shim
// copies S and D in as well as out: the early return and the IK = NK row keep the
// caller's values, which W3SRCE zeroes once before its sub-steps (:899-900). The
// routine has no #ifdef but W3_S (STRACE), which is not ported: nothing in its
// arithmetic depends on a switch. IX is a dummy argument never read, and is not
// passed. PTRIAD(1:5) are literals in the source (:435-439), folded here as there.
//
// Dead locals not carried: ED (:406-410, stored, never read), EF (:488, :493),
// EDM (:503, :509), ED0 (:497), HMAX (:403); SA2, A2, A3 are never touched. None
// reaches S or D, so dropping them changes no output bit.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3str1md.F90 (WW3's CMake Release flags, switch_lab_shrd with TR0 -> TR1):
//   :395-397 MAXVAL(A) inlined: the first A >= -Inf starts the maximum, then
//          A > M ? A : M (maxss); all NaN gives NaN and does not return;
//   :405-420 EB summed over ITH in order, then (EB*DDEN)/CG, EMEAN and SIGM01
//          in-order reductions (the vectorised :416 loop folds lanes left to right);
//   :441   4*sqrtf(MAX(0,EMEAN)), maxss with EMEAN as destination;
//   :442   (GRAV*HS) / (((SIGM01*SIGM01)*(2*SQRT(2.))) * (DEPTH*DEPTH)), 2*SQRT(2.)
//          folded to 0x403504f3;
//   :446-448 DEP**2 = D*D, DEP**3 = D*(D*D) (power tree), G9DEP = GRAV*D;
//   :452   INT(FLOAT(NK)*0.5);
//   :455-465 logf; LOG(2.) folded to 0x3f317218, LOG(0.5) to its negation; INT
//          truncates; XIS**ISP, **ISP1, **ISM, **ISM1 are __powisf2 calls;
//   :486-487 BIPH = (tanhf(0.2/URSELL) - 1) * 1.5707964 (0.5*PI folded), |sinf|;
//   :492   E = ((A*TPI)*SIG)/CG (the vectorised :491 loop is element-wise);
//   :502-506 the interpolation in source order, WISM*X(IK+ISM1) + WISM1*X(IK+ISM);
//   :514-518 AUX1 = WNM**2*(G9DEP+2*CM**2); AUX2 = (WN0*DEP) *
//          ((G9DEP + (1.3074667*DEP_3)*WN0**2) - (0.4*W0**2)*DEP_2), (2./15.)*GRAV
//          folded to 0x3fa75b12, 2./5. to 0x3ecccccd; FT = ((C0*CG)*RINT**2)*SINBPH
//          (PTRIAD(1) = 1. folded away); SA = MAX(0, FT*(EM*EM - (2*EM)*E0)),
//          maxss with the product as destination, so NaN gives 0;
//   :522-534 SIGPICG = (SIG*TPI)/CG; STRI = SA - 2*(WISP*SA(+ISP1) + WISP1*SA(+ISP));
//          D = STRI/(A*SIGPICG), S = STRI/SIGPICG, true divisions.
// No FMA, no reciprocal multiply, no libmvec: every libm call (logf, tanhf, sinf,
// __powisf2) is scalar. No reduction is reordered.
//
// Quirks kept from the Fortran:
//   - MAXVAL(A) < TINY(1.) returns before S and D are written (:395-397): the
//     caller's VSTR, VDTR keep what they held (zero from w3srcemd.F90:899-900, or
//     the previous sub-step's values).
//   - On the URSELL >= 0.01 path S and D are written for IK = 1..NK-1 only (:522);
//     the last frequency row keeps what the caller had there.
// sig points at SIG(1) inside the whole SIG(0:NK+1) (w3gdatmd.F90:2066): with NK = 2
// or 3, I1 = 0 and the Fortran reads SIG(0) legally, which here is sig[-1].
// Preconditions the Fortran assumes and does not check (out of bounds there):
// ISP+1 <= NK (E(1:ISMAX) is read with
// ISMAX >= ISP+1), NK-1+ISP+1 <= 100 (SA has 100 columns).
//
// ⚠ On a device logf, tanhf, sinf and sqrtf are CUDA's, not glibc's
// (AGENTS.md, "Bit for bit", GPU).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "st4_params.hpp"

namespace ww::srce {

/// SA(NTH,100), w3str1md.F90:379: the column count is a literal in the source.
inline constexpr int kTr1SaCols = 100;

namespace tr1_detail {
// Fortran MAX of two REALs: fmax2(a,b) = b>a?b:a. A NaN or a +0/-0 tie gives `a`;
// each call passes the operands in the order of the build's maxss (w3str1md.o:
// `maxss src,dst` is dst > src ? dst : src, i.e. fmax2(src,dst)).
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }

// x**n for a run-time INTEGER n: libgcc's __powisf2 (libgcc2.c; srce_ref1.hpp has
// the same function). Never std::pow.
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

/// W3STR1(A, CG, WN, DEPTH, IX, S, D) for one point.
/// p gives NK, NTH, NSPEC. sig = SIG(1:NK), dden = DDEN(1:NK), fte, ftf = W3GDATMD
/// FTE, FTF; a = A(1:NSPEC), cg = CG(1:NK), wn = WN(1:NK), s, d = S, D(1:NSPEC),
/// all 0-based: A(ITH+(IK-1)*NTH) is a[ITH+(IK-1)*NTH-1]. e (NK), eb (NK) and
/// sa (NTH*kTr1SaCols, SA(ITH,J) at sa[(J-1)*NTH+ITH-1]) are caller-provided work
/// for the Fortran locals E, EB and SA (w3str1md.F90:379-380).
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

  // :405-412 (ED dropped, see the header).
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
