// kokkos/src/ww_kokkos/srce_st2_sds2.hpp
// W3SDS2: Tolman and Chalikov (1996) whitecapping dissipation and its diagonal term,
// D = (1-FILT)*CDIST*K**2 - FILT*CDSA0*CDISH*SIG**3*(ALFA/SDSALN)**POW, S = D*A, one
// spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src2md.F90:582-825 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3SDS2_KOKKOS.md.
// The W3_T, W3_T0, W3_T1 and W3_T2 test output (POWMAX, DOUT) and W3_S tracing are dropped.
// ⚠ On a device logf, powf, fmodf and sqrtf are CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "st4_params.hpp"

namespace ww::srce::st2 {

namespace sds2_detail {
// minss/maxss of w3src2md.o in W3SDS2, d the destination: a NaN or a tie gives s.
// Every site puts the constant or FPIMIN second (:723, :736, :761, :762).
KOKKOS_INLINE_FUNCTION Real minss(Real d, Real s) { return (d < s) ? d : s; }
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }
KOKKOS_INLINE_FUNCTION int imin(int a, int b) { return (a < b) ? a : b; }

// CONSTANTS folded by gfortran (constants.F90:82-83; values in w3src2md.o's GIMPLE).
inline constexpr Real kG2Pi3I = 0x1.5fb1fcp-15f;  // G2PI3I = 1/(GRAV**2*TPI**3), :749
inline constexpr Real kG1Pi1I = 0x1.09eb14p-6f;   // G1PI1I = 1/(GRAV*TPI), :750
static_assert(kG2Pi3I == static_cast<Real>(1.) / ((kGrav * kGrav) * (kTpi * kTpi * kTpi)));
static_assert(kG1Pi1I == static_cast<Real>(1.) / (kGrav * kTpi));
static_assert(kTpiInv == 0x1.45f306p-3f);  // :736
}  // namespace sds2_detail

/// W3SDS2 for one point. a, s, d are NSPEC long, (ith, ik) at (ith-1) + NTH*(ik-1); cg, k,
/// alfa are NK long, 0-based. p gives NK, NTH, FACTI1, FACTI2; the scalars after it are the
/// W3GDATMD variables of the same name. sig = &SIG(1) (sig[nk] is SIG(NK+1), read at :721),
/// dden = &DDEN(1). ebd (NK) is work for the local EBD.
/// ⚠ IKHW = MIN(NK, INT(XHW+0.5)) (:713) has no lower bound: below 1 the Fortran writes EBD
/// and reads A, DDEN, CG out of bounds (:714-726), and so does this function.
KOKKOS_INLINE_FUNCTION void w3sds2_point(const Real* a, const Real* cg, const Real* k, Real fpi,
                                         Real ustar, const Real* alfa, Real* s, Real* d,
                                         const Params& p, const Real* sig, const Real* dden,
                                         Real dth, Real fte, Real fpimin, Real xf1, Real xf2,
                                         Real xfh, Real sdsaln, Real cdsa0, Real cdsa1, Real cdsa2,
                                         Real cdsb0, Real cdsb1, Real cdsb2, Real cdsb3,
                                         Real* ebd) {
  using sds2_detail::imin;
  using sds2_detail::maxss;
  using sds2_detail::minss;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const Real half = static_cast<Real>(0.5);

  // 1.a HW, :711-732. The sums over ITH and IK are vectorised in the object as in-order
  // (fold-left) reductions: the same additions in the same order as these loops.
  const Real fhw = xfh * fpi;
  Real xhw = p.facti2 + p.facti1 * Kokkos::log(fhw);
  const Real xhwh = xhw + half;
  const int ikhw = imin(nk, ww::st4::fint(xhwh));  // :713, cvttss2si
  for (int ik = ikhw; ik <= nk; ++ik) {
    ebd[ik - 1] = zero;
    for (int ith = 1; ith <= nth; ++ith) {
      ebd[ik - 1] = ebd[ik - 1] + a[(ith - 1) + nth * (ik - 1)];
    }
  }

  Real hw;
  if (fhw < sig[nk]) {
    // :722. MOD(x, 1.) is x87 fprem in the object: exact, as fmodf.
    xhw = one - Kokkos::fmod(xhwh, one);
    if (ikhw == nk) xhw = maxss(xhw - half, zero);  // :723
    hw = xhw * ebd[ikhw - 1] * dden[ikhw - 1] / cg[ikhw - 1];
    for (int ik = ikhw + 1; ik <= nk; ++ik) {
      hw = hw + ebd[ik - 1] * dden[ik - 1] / cg[ik - 1];
    }
    hw = static_cast<Real>(4.) * Kokkos::sqrt(hw + ebd[nk - 1] / cg[nk - 1] * fte);  // :728
  } else {
    // :730. (SIG(NK)/FHW)**5 as -O3 expands it: x2 = x*x, x3 = x*x2, x5 = x2*x3.
    const Real x = sig[nk - 1] / fhw;
    const Real x2 = x * x;
    const Real x3 = x * x2;
    const Real x5 = x2 * x3;
    const Real ehigh = ebd[nk - 1] / cg[nk - 1] * sig[nk - 1] * dth * x5;
    hw = static_cast<Real>(4.) * Kokkos::sqrt(static_cast<Real>(0.25) * fhw * ehigh);  // :731
  }

  // 1.b PHI, :736-737. FPIT**CDSB3 is powf.
  const Real fpit = maxss(fpi * kTpiInv * ustar / kGrav, fpimin);
  const Real phi = cdsb0 + cdsb1 * fpit + cdsb2 / Kokkos::pow(fpit, cdsb3);

  // 1.c Filter, :741-744.
  const Real af2 = xf2 * fpi;
  const Real af1 = xf1 * fpi;
  const Real bfilt = one / (af2 - af1);
  const Real afilt = -(bfilt * af1);

  // 1.d Constants, :748-750. USTAR**2 is USTAR*USTAR.
  const Real cdist = -(static_cast<Real>(2.) * ustar * hw * phi);
  const Real cdish = sds2_detail::kG2Pi3I * (ustar * ustar);
  const Real cdisp = sds2_detail::kG1Pi1I * ustar;

  // 2. Combined diagonal factor, :760-778. K**2 = K*K, SIG**3 = SIG*(SIG*SIG); both real
  // powers are powf.
  for (int ik = 1; ik <= nk; ++ik) {
    const Real sg = sig[ik - 1];
    const Real filt = minss(maxss(afilt + bfilt * sg, zero), one);  // :761
    const Real pw = minss(cdsa1 / Kokkos::pow(cdisp * sg, cdsa2), static_cast<Real>(25.));
    const Real kk = k[ik - 1] * k[ik - 1];
    const int is1 = nth * (ik - 1);
    if (filt > zero) {
      const Real sg3 = sg * (sg * sg);
      d[is1] = (one - filt) * cdist * kk -
               filt * cdsa0 * cdish * sg3 * Kokkos::pow(alfa[ik - 1] / sdsaln, pw);
    } else {
      d[is1] = (one - filt) * cdist * kk;
    }
  }

  // 3. 2-D diagonal array, :786-790.
  for (int ik = 1; ik <= nk; ++ik) {
    for (int ith = 2; ith <= nth; ++ith) {
      d[(ith - 1) + nth * (ik - 1)] = d[nth * (ik - 1)];
    }
  }

  // :792.
  for (int ik = 1; ik <= nk; ++ik) {
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = (ith - 1) + nth * (ik - 1);
      s[is] = d[is] * a[is];
    }
  }
}

}  // namespace ww::srce::st2
