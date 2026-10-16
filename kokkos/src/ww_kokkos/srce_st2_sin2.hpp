// kokkos/src/ww_kokkos/srce_st2_sin2.hpp
// W3SIN2: Tolman and Chalikov (1996) wind input source term, its input peak frequency FPI
// and the swell filter, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src2md.F90:308-566 at NOAA-EMC/WW3 03adda91 (module
// table :93-99); authors H. L. Tolman, D. Chalikov.
// Evidence: kokkos/port_report/W3SIN2_KOKKOS.md.
// BETATB, DSIGA and DDRAG are W3SRC2MD module state that INPTAB fills
// (srce_st2_inptab.hpp); this header only reads them.
// ⚠ On a device sinf, cosf and logf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st2_inptab.hpp"
#include "st4_params.hpp"

namespace ww::srce::st2 {

namespace sin2_detail {
// minss/maxss of w3src2md.o, d the destination: a NaN in either operand or a tie gives s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
KOKKOS_INLINE_FUNCTION int imin(int a, int b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION int imax(int a, int b) { return (a > b) ? a : b; }

inline constexpr Real kTransf = static_cast<Real>(0.75);  // TRANSF :427
inline constexpr Real kPeakfc = static_cast<Real>(0.8);   // PEAKFC :428
}  // namespace sin2_detail

/// W3SIN2 (w3src2md.F90:308-566) for one point, arrays 0-based.
/// a, k, s, d: (NSPEC); cg: CG(1:NK). fpi receives FPI.
/// Grid arrays of W3GDATMD from their first element used: sig = SIG(1:NK) (SIG is
/// (0:NK+1), so pass &SIG(1)), dden = DDEN(1:NK), sig2 = SIG2(1:NSPEC),
/// ecos, esin = ECOS, ESIN(1:NSPEC). p supplies NK, NTH, NSPEC, FACTI1, FACTI2.
/// betatb: W3SRC2MD BETATB(-400:401, 1:21) flat in Fortran column-major order, element
/// (i, j) at (i+400) + 802*(j-1), the layout and constants of srce_st2_inptab.hpp. sin1a (NK) is work for the automatic array SIN1A.
KOKKOS_INLINE_FUNCTION void w3sin2_point(const Real* a, const Real* cg, const Real* k, Real u,
                                         Real udir, Real cd, Real z0, Real& fpi, Real* s,
                                         Real* d, const Params& p, const Real* sig,
                                         const Real* sig2, const Real* dden, const Real* ecos,
                                         const Real* esin, Real xfr, Real fte, Real fttr,
                                         Real fpimin, Real zwind, Real fswell, Real dsiga,
                                         Real ddrag, const Real* betatb, Real* sin1a) {
  using sin2_detail::imax;
  using sin2_detail::imin;
  using sin2_detail::kPeakfc;
  using sin2_detail::kTransf;
  using sin2_detail::maxss;
  using ww::st4::fint;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real one = static_cast<Real>(1.);
  const Real zero = static_cast<Real>(0.);

  // :445-446. The object calls sincosf.
  const Real cosu = std::cos(udir);
  const Real sinu = std::sin(udir);

  // :454-455.
  const Real facln1 = u / std::log(zwind / z0);
  const Real facln2 = std::log(z0);

  // :457-481.
  for (int is = 0; is < nspec; ++is) {
    Real cosfac = ecos[is] * cosu + esin[is] * sinu;
    cosfac = std::copysign(maxss(std::fabs(cosfac), static_cast<Real>(0.0087)), cosfac);
    const Real lambda = kTpi / (k[is] * std::fabs(cosfac));
    const Real ulam = facln1 * (std::log(lambda) - facln2);
    const Real ul = u / ulam;
    const Real clam = cd * (ul * ul);
    const Real oma = k[is] * ulam * cosfac / sig2[is];
    // :465-466. SIGN(-1.1,OMA) is copysign(1.1,OMA); INT is cvttss2si.
    const Real omad = oma / dsiga;
    int ioma = fint(omad) + imin(0, fint(std::copysign(static_cast<Real>(1.1), oma)));
    const Real clamd = clam / ddrag;
    int icl = fint(clamd);
    const Real rd1 = omad - static_cast<Real>(ioma);
    const Real rd2 = clamd - static_cast<Real>(icl);
    ioma = imax(-kNrSiga, imin(kNrSiga, ioma));
    icl = imax(1, imin(kNrDrag, icl));
    const int b00 = (ioma + kNrSiga) + kBetatbN1 * (icl - 1);  // BETATB(IOMA, ICL)
    const int b01 = b00 + kBetatbN1;                            // BETATB(IOMA, ICL+1)
    const Real beta = (one - rd1) * (one - rd2) * betatb[b00] +
                      rd1 * (one - rd2) * betatb[b00 + 1] +
                      (one - rd1) * rd2 * betatb[b01] + rd1 * rd2 * betatb[b01 + 1];
    d[is] = beta * sig2[is];
    s[is] = a[is] * d[is];
  }

  // :485-490.
  for (int ik = 1; ik <= nk; ++ik) {
    sin1a[ik - 1] = zero;
    for (int is = (ik - 1) * nth + 1; is <= ik * nth; ++is) {
      sin1a[ik - 1] = sin1a[ik - 1] + maxss(s[is - 1], zero);
    }
  }

  // :492-498. The object vectorises this loop but adds M0 and M1 lane by lane in order.
  Real m0 = zero;
  Real m1 = zero;
  for (int ik = 1; ik <= nk; ++ik) {
    const Real sg = sig[ik - 1];
    sin1a[ik - 1] = sin1a[ik - 1] * dden[ik - 1] / (cg[ik - 1] * (sg * sg * sg));
    m0 = m0 + sin1a[ik - 1];
    m1 = m1 + sin1a[ik - 1] / sg;
  }

  // :500-507.
  sin1a[nk - 1] = sin1a[nk - 1] / dden[nk - 1];
  m0 = m0 + sin1a[nk - 1] * fte;
  m1 = m1 + sin1a[nk - 1] * fttr;
  if (m1 < static_cast<Real>(1E-20)) {
    fpi = xfr * sig[nk - 1];
  } else {
    fpi = m0 / m1;
  }

  // :511-514. FPI*USTAR/GRAV against FPIMIN: the object puts FPIMIN second.
  const Real ustar = u * std::sqrt(cd);
  const Real fpistr = maxss(fpi * ustar / kGrav, fpimin);
  const Real fp1str = static_cast<Real>(3.6E-4) + static_cast<Real>(0.92) * fpistr -
                      static_cast<Real>(6.3E-10) / (fpistr * fpistr * fpistr);
  const Real fp1 = kPeakfc * fp1str * kGrav / ustar;

  // :516-518.
  const int nkfilt = imin(nk, fint(p.facti2 + p.facti1 * std::log(fp1)));
  int nkfil2 = imin(nk, fint(p.facti2 + p.facti1 * std::log(kTransf * fp1)));
  nkfil2 = imax(0, nkfil2);

  // :520-523. MAX(D, FSWELL*D): the object puts FSWELL*D first, here and at :528.
  for (int is = 0; is < nkfil2 * nth; ++is) {
    d[is] = maxss(fswell * d[is], d[is]);
    s[is] = a[is] * d[is];
  }

  // :525-531.
  for (int ik = nkfil2 + 1; ik <= nkfilt; ++ik) {
    const Real trans = (sig[ik - 1] / fp1 - kTransf) / (one - kTransf);
    for (int is = (ik - 1) * nth + 1; is <= ik * nth; ++is) {
      d[is - 1] = (one - trans) * maxss(fswell * d[is - 1], d[is - 1]) + trans * d[is - 1];
      s[is - 1] = a[is - 1] * d[is - 1];
    }
  }
}

}  // namespace ww::srce::st2
