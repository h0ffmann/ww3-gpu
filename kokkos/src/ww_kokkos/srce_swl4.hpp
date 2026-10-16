// kokkos/src/ww_kokkos/srce_swl4.hpp
// W3SWL4: swell dissipation by a laminar or turbulent air-side boundary layer chosen on
// a Reynolds number of the orbital motion, one spectral point.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3swldmd.F90:93-242 at NOAA-EMC/WW3 03adda91;
// authors H. L. Tolman, S. Zieger. Evidence: kokkos/port_report/W3SWL4_KOKKOS.md.
// Nothing in model/src calls W3SWL4 (W3SRCE calls only W3SWL6, w3srcemd.F90:575).
// ⚠ On a device sqrtf is CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_swl_util.hpp"

namespace ww::srce::swl {

namespace swl4_detail {
/// Returned where the Fortran stops in EXTCDE(2) (SIZE(IKN) /= NK, :194-197).
inline constexpr int kExtcde = 2;

inline constexpr Real kVa = static_cast<Real>(1.4E-5);  // VA :181
// Folded by gfortran (constants of w3swldmd.o's W3SWL4):
inline constexpr Real k16OverGrav = 0x1.a1b416p+0f;  // 16.0/GRAV :223
inline constexpr Real kTwoVa = 0x1.d5c316p-16f;      // 2.0*VA     :225
static_assert(kVa == 0x1.d5c316p-17f);
static_assert(k16OverGrav == static_cast<Real>(16.0) / ww::srce::kGrav);
static_assert(kTwoVa == static_cast<Real>(2.0) * kVa);
}  // namespace swl4_detail

/// IKN = IRANGE(1,NSPEC,NTH) (:191-193), the SAVE table of W3SWL4, built on the host:
/// call once with ikn = nullptr for the count, then with storage of that size.
inline int w3swl4_ikn(const Params& p, int* ikn) { return irange(1, p.nspec, p.nth, ikn); }

/// W3SWL4 for one point, arrays 0-based. a, s, d, sig2 (SIG2(1:NSPEC)) and wn2 have
/// NSPEC elements, A(ITH,IK) at a[(ik-1)*NTH + ith-1]. cg, wn, dden
/// (DDEN(1:NK)) and eb have NK. ikn/nikn are the table of w3swl4_ikn and its count;
/// eb and wn2 are work (the Fortran locals EB, WN2). fte, swl6b1 are W3GDATMD's FTE,
/// SWL6B1. Returns 0, or kExtcde with s and d not written.
KOKKOS_INLINE_FUNCTION int w3swl4_point(const Real* a, const Real* cg, const Real* wn, Real dair,
                                        Real* s, Real* d, const Params& p, const Real* sig2,
                                        const Real* dden, Real fte, Real swl6b1, const int* ikn,
                                        int nikn, Real* eb, Real* wn2) {
  using namespace swl4_detail;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);

  // :194-197.
  if (nikn != nk) return kExtcde;

  // :199-200.
  for (int is = 0; is < nspec; ++is) d[is] = zero;
  for (int is = 0; is < nspec; ++is) wn2[is] = zero;

  // :202-204. IKN holds 1-based spectral indices.
  for (int ith = 1; ith <= nth; ++ith) {
    for (int ik = 1; ik <= nk; ++ik) wn2[ikn[ik - 1] + (ith - 1) - 1] = wn[ik - 1];
  }

  // :206. SUM(...,1) in ITH order from 0.0, as the object adds (unrolled, in order).
  for (int ik = 1; ik <= nk; ++ik) {
    Real sum = zero;
    for (int ith = 1; ith <= nth; ++ith) sum = sum + a[(ik - 1) * nth + ith - 1];
    eb[ik - 1] = (sum * dden[ik - 1]) / cg[ik - 1];
  }
  // :207.
  Real sumeb = zero;
  for (int ik = 1; ik <= nk; ++ik) sumeb = sumeb + eb[ik - 1];
  const Real emean = sumeb + (eb[nk - 1] / dden[nk - 1]) * fte;

  // :209.
  const Real aorb = static_cast<Real>(2.0) * Kokkos::sqrt(emean);

  // :211. A*SIG2**2 is A*(SIG2*SIG2).
  for (int ik = 1; ik <= nk; ++ik) {
    Real sum = zero;
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = (ik - 1) * nth + ith - 1;
      sum = sum + a[is] * (sig2[is] * sig2[is]);
    }
    eb[ik - 1] = (sum * dden[ik - 1]) / cg[ik - 1];
  }
  // :212.
  sumeb = zero;
  for (int ik = 1; ik <= nk; ++ik) sumeb = sumeb + eb[ik - 1];
  const Real uosig = static_cast<Real>(2.0) * Kokkos::sqrt(sumeb);

  // :214-220.
  const Real fe = swl6b1;
  const Real cdsv = static_cast<Real>(1.2);
  const Real recrit = static_cast<Real>(1.0E5);
  const Real re = ((static_cast<Real>(4.0) * uosig) * aorb) / kVa;

  if (re > recrit) {
    // :223, -(...) of the whole product.
    const Real fac = (k16OverGrav * (dair / ww::srce::kDwat)) * fe;
    for (int is = 0; is < nspec; ++is) d[is] = -((fac * (sig2[is] * sig2[is])) * uosig);
  } else {
    // :225, -(...) of the whole product; 2.0*VA folded.
    const Real fac = (static_cast<Real>(2.0) * (dair / ww::srce::kDwat)) * cdsv;
    for (int is = 0; is < nspec; ++is) {
      d[is] = -((fac * wn2[is]) * Kokkos::sqrt(kTwoVa * sig2[is]));
    }
  }

  // :228.
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
  return 0;
}

}  // namespace ww::srce::swl
