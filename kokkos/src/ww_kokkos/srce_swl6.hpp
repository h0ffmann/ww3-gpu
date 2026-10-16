// kokkos/src/ww_kokkos/srce_swl6.hpp
// W3SWL6: turbulent dissipation of narrow-banded swell (Babanin 2011, section 7.5),
// D = -(2/3) B1 sigma sqrt(Bn) per wavenumber, one spectral point.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3swldmd.F90:264-439 at NOAA-EMC/WW3 03adda91;
// authors H. L. Tolman, S. Zieger. Evidence: kokkos/port_report/W3SWL6_KOKKOS.md.
// W3SRCE calls it under SWL6S6 (w3srcemd.F90:1332).
// ⚠ On a device sqrtf is CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_swl_util.hpp"

namespace ww::srce::swl {

namespace swl6_detail {
/// Returned where the Fortran stops in EXTCDE(2) (SIZE(IKN) /= NK, :374-377).
inline constexpr int kExtcde = 2;

// Folded by gfortran (w3swldmd.o, W3SWL6):
inline constexpr Real kTwoThirds = 0x1.555556p-1f;  // 2./3. :415, 0x3f2aaaab
static_assert(kTwoThirds == static_cast<Real>(2.) / static_cast<Real>(3.));

// libgfortran 16.2's _gfortran_maxval_r4 (:388): -HUGE for an empty column, a quiet
// NaN when every element is NaN.
KOKKOS_INLINE_FUNCTION Real neg_huge() { return Kokkos::bit_cast<Real>(std::uint32_t{0xff7fffffu}); }
KOKKOS_INLINE_FUNCTION Real neg_inf() { return Kokkos::bit_cast<Real>(std::uint32_t{0xff800000u}); }
KOKKOS_INLINE_FUNCTION Real qnan() { return Kokkos::bit_cast<Real>(std::uint32_t{0x7fc00000u}); }
}  // namespace swl6_detail

/// IKN = IRANGE(1,NSPEC,NTH) (:370-372), the SAVE table of W3SWL6, built on the host:
/// call once with ikn = nullptr for the count, then with storage of that size.
inline int w3swl6_ikn(const Params& p, int* ikn) { return irange(1, p.nspec, p.nth, ikn); }

/// W3SWL6 for one point, arrays 0-based. a, s, d have NSPEC elements, a(ith,ik) at
/// a[(ik-1)*NTH + ith-1]; cg, wn have NK. sig and dden point at SIG(1) and DDEN(1)
/// of W3GDATMD (SIG is SIG(0:NK+1)); dth, swl6cstb1, swl6b1 are W3GDATMD's DTH,
/// SWL6CSTB1, SWL6B1. ikn/nikn are the table of w3swl6_ikn and its count. aband, kmax,
/// anar, bn, ddis (NK each) and k (NSPEC) are work for the Fortran locals of those
/// names. Returns 0, or kExtcde with s and d not written.
KOKKOS_INLINE_FUNCTION int w3swl6_point(const Real* a, const Real* cg, const Real* wn, Real* s,
                                        Real* d, const Params& p, Real dth, const Real* sig,
                                        const Real* dden, bool swl6cstb1, Real swl6b1,
                                        const int* ikn, int nikn, Real* aband, Real* kmax,
                                        Real* anar, Real* bn, Real* ddis, Real* k) {
  using namespace swl6_detail;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);

  // :374-377.
  if (nikn != nk) return kExtcde;

  // :379. SUM(...,1) in ITH order from 0.0, as the object adds (unrolled, in order).
  for (int ik = 1; ik <= nk; ++ik) {
    Real sum = zero;
    for (int ith = 1; ith <= nth; ++ith) sum = sum + a[(ik - 1) * nth + ith - 1];
    aband[ik - 1] = sum;
  }
  // :380-382.
  for (int ik = 0; ik < nk; ++ik) ddis[ik] = zero;
  for (int is = 0; is < nspec; ++is) d[is] = zero;
  Real b1 = swl6b1;

  // :387.
  for (int is = 0; is < nspec; ++is) k[is] = a[is];
  // :388, _gfortran_maxval_r4: skip to the first element >= -Inf (not NaN), then
  // keep the larger with maxss (a tie or a NaN keeps the running value).
  for (int ik = 1; ik <= nk; ++ik) {
    const Real* col = k + (ik - 1) * nth;
    Real r;
    if (nth <= 0) {
      r = neg_huge();
    } else {
      r = neg_inf();
      int n = 0;
      for (; n < nth; ++n) {
        if (col[n] >= r) break;
      }
      if (n >= nth) {
        r = qnan();
      } else {
        for (; n < nth; ++n) r = (col[n] > r) ? col[n] : r;
      }
    }
    kmax[ik - 1] = r;
  }
  // :389-395.
  for (int ik = 1; ik <= nk; ++ik) {
    Real* col = k + (ik - 1) * nth;
    if (kmax[ik - 1] < static_cast<Real>(1.0E-34)) {
      for (int ith = 0; ith < nth; ++ith) col[ith] = one;
    } else {
      for (int ith = 0; ith < nth; ++ith) col[ith] = col[ith] / kmax[ik - 1];
    }
  }
  // :396.
  for (int ik = 1; ik <= nk; ++ik) {
    Real sum = zero;
    for (int ith = 1; ith <= nth; ++ith) sum = sum + k[(ik - 1) * nth + ith - 1];
    anar[ik - 1] = one / (sum * dth);
  }
  // :397. WN**3 is WN*(WN*WN).
  for (int ik = 0; ik < nk; ++ik) {
    bn[ik] = (anar[ik] * ((aband[ik] * sig[ik]) * dth)) * (wn[ik] * (wn[ik] * wn[ik]));
  }

  if (!swl6cstb1) {
    // :406. MAXLOC(ABAND,1): the first element >= -Inf, then a strict > keeps the first
    // maximum; all NaN gives 1. ⚠ NK = 0 (only with NTH = 1 past :374) reads wn[0] out of
    // bounds where the Fortran reads WN(0); the value is never used.
    int ikp = 1;
    {
      int i = 1;
      for (; i <= nk; ++i) {
        if (aband[i - 1] >= neg_inf()) break;
      }
      if (i <= nk) {
        Real lim = aband[i - 1];
        ikp = i;
        for (int j = i; j <= nk; ++j) {
          if (lim < aband[j - 1]) {
            lim = aband[j - 1];
            ikp = j;
          }
        }
      }
    }
    // :408. SUM in IK order from 0.0 of (DDEN*ABAND)/CG.
    Real sum = zero;
    for (int ik = 0; ik < nk; ++ik) sum = sum + (aband[ik] * dden[ik]) / cg[ik];
    b1 = swl6b1 * ((static_cast<Real>(2.) * Kokkos::sqrt(sum)) * wn[ikp - 1]);
  }

  // :413-417. -(...) of the whole product.
  for (int ik = 0; ik < nk; ++ik) {
    if (aband[ik] > static_cast<Real>(1.E-30)) {
      ddis[ik] = -(((kTwoThirds * b1) * sig[ik]) * Kokkos::sqrt(bn[ik]));
    }
  }

  // :420-422. IKN holds 1-based spectral indices.
  for (int ith = 1; ith <= nth; ++ith) {
    for (int ik = 1; ik <= nk; ++ik) d[ikn[ik - 1] + (ith - 1) - 1] = ddis[ik - 1];
  }

  // :424.
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
  return 0;
}

}  // namespace ww::srce::swl
