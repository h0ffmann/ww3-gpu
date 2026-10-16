// kokkos/src/ww_kokkos/srce_st6_sds6.hpp
// W3SDS6: observation-based dissipation of ST6 (Babanin et al. 2010, Rogers et al. 2012),
// inherent breaking T1 plus cumulative T2, one spectral point.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src6md.F90:566-770 at NOAA-EMC/WW3 03adda91;
// authors S. Zieger, Q. Liu. Evidence: kokkos/port_report/W3SDS6_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st6_util.hpp"

namespace ww::srce::st6 {

namespace sds6_detail {
// maxss of w3src6md.o, d the destination: a NaN in either operand or a tie gives s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

// TPI*BNT, BNT = 0.035**2 (:708, :715), folded by gfortran, 0x3bfc364c.
inline constexpr Real kTpiBnt = 0x1.f86c98p-8f;
static_assert(kTpiBnt == ww::srce::kTpi * (static_cast<Real>(0.035) * static_cast<Real>(0.035)));

// REAL**INTEGER with a run-time exponent (:734, :738): the object calls libgcc's
// __powisf2. The device branch is libgcc2.c's __powisf2 loop, in float.
KOKKOS_INLINE_FUNCTION Real powi(Real x, int m) {
  KOKKOS_IF_ON_HOST((return __builtin_powif(x, m);))
  KOKKOS_IF_ON_DEVICE((
    unsigned n = m < 0 ? 0u - static_cast<unsigned>(m) : static_cast<unsigned>(m);
    Real y = (n % 2u) ? x : static_cast<Real>(1);
    while (n >>= 1) {
      x = x * x;
      if (n % 2u) y = y * x;
    }
    return m < 0 ? static_cast<Real>(1) / y : y;
  ))
}
}  // namespace sds6_detail

/// IKN = IRANGE(1,NSPEC,NTH) (:696-704), host: ikn receives NK values 1+(IK-1)*NTH
/// (1-based, as the Fortran holds them). Returns 0, or 2 where W3SDS6 calls EXTCDE(2)
/// because SIZE(IKN) /= NK (nothing is written then). The Fortran IKN is a module SAVE
/// array (:104), THREADPRIVATE under OMPG only (the guard at :106 tests W3_OMPG alone),
/// built by whichever of W3SIN6 (:449) and W3SDS6 calls first and never rebuilt: once
/// per process (per thread under OMPG), not per grid. The shim passes that one table.
inline int w3sds6_ikn(const Params& p, int* ikn) {
  if (range_size(1, p.nspec, p.nth) != p.nk) return 2;
  irange(1, p.nspec, p.nth, ikn);
  return 0;
}

/// W3SDS6 for one point, arrays 0-based. a = A(NSPEC), cg = CG(1:NK), wn = WN(1:NK); s, d
/// = S, D(NSPEC) out. Module state: ikn from w3sds6_ikn(); sig2 = &SIG2(1) (NSPEC),
/// dsii = DSII(1:NK); dth, xfr, sds6a1, sds6a2, sds6p1, sds6p2, sds6et the W3GDATMD
/// scalars. work holds 10*NK reals for the automatic arrays FREQ, DFII, EDENS, ETDENS,
/// EXDENS, NEXDENS, T1, T2, T12, ADF (:674-684). ANAR (:707) is 1.0 everywhere; its
/// products are kept.
///
/// Both SUMs (:716 over directions, :744 over IK) add their terms one by one from 0.0
/// in index order: the object's vector loops extract the lanes and add them in sequence.
KOKKOS_INLINE_FUNCTION void w3sds6_point(const Real* a, const Real* cg, const Real* wn, Real* s,
                                         Real* d, const Params& p, const int* ikn,
                                         const Real* sig2, const Real* dsii, Real dth, Real xfr,
                                         Real sds6a1, Real sds6a2, int sds6p1, int sds6p2,
                                         bool sds6et, Real* work) {
  using sds6_detail::maxss;
  using sds6_detail::powi;
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);
  const Real one = static_cast<Real>(1.);
  const Real anar = one;
  const Real tpi = ww::srce::kTpi;
  Real* freq = work;
  Real* dfii = work + nk;
  Real* edens = work + 2 * nk;
  Real* etdens = work + 3 * nk;
  Real* exdens = work + 4 * nk;
  Real* nexdens = work + 5 * nk;
  Real* t1 = work + 6 * nk;
  Real* t2 = work + 7 * nk;
  Real* t12 = work + 8 * nk;
  Real* adf = work + 9 * nk;

  for (int ik = 0; ik < nk; ++ik) freq[ik] = sig2[ikn[ik] - 1] / tpi;  // :706
  for (int ik = 0; ik < nk; ++ik) {                                     // :709-711
    t1[ik] = zero;
    t2[ik] = zero;
    nexdens[ik] = zero;
  }

  // :715. WN**3 is WN*(WN*WN).
  for (int ik = 0; ik < nk; ++ik) {
    etdens[ik] = sds6_detail::kTpiBnt / ((anar * cg[ik]) * (wn[ik] * (wn[ik] * wn[ik])));
  }
  // :716
  for (int ik = 0; ik < nk; ++ik) {
    Real sum = zero;
    for (int ith = 0; ith < nth; ++ith) sum = sum + a[ik * nth + ith];
    edens[ik] = (((sum * tpi) * sig2[ikn[ik] - 1]) * dth) / cg[ik];
  }
  for (int ik = 0; ik < nk; ++ik) exdens[ik] = maxss(edens[ik] - etdens[ik], zero);  // :717

  if (sds6et) {
    for (int ik = 0; ik < nk; ++ik) nexdens[ik] = exdens[ik] / etdens[ik];  // :721
  } else {
    // :723 MAXVAL as gfortran emits it: the first non-NaN element starts the limit, then
    // `x > limit ? x : limit`; all NaN gives NaN.
    Real edensmax = Kokkos::quiet_NaN_v<Real>;
    int ik0 = 0;
    while (ik0 < nk && !(edens[ik0] >= -Kokkos::infinity_v<Real>)) ++ik0;
    if (ik0 < nk) {
      Real limit = edens[ik0];
      for (int ik = ik0 + 1; ik < nk; ++ik) limit = (edens[ik] > limit) ? edens[ik] : limit;
      edensmax = limit * static_cast<Real>(1E-5);
    }
    bool all = true;  // :724
    for (int ik = 0; ik < nk; ++ik) {
      if (!(edens[ik] > edensmax)) {
        all = false;
        break;
      }
    }
    if (all) {
      for (int ik = 0; ik < nk; ++ik) nexdens[ik] = exdens[ik] / edens[ik];  // :725
    } else {
      for (int ik = 0; ik < nk; ++ik) {  // :727-729
        if (edens[ik] > edensmax) nexdens[ik] = exdens[ik] / edens[ik];
      }
    }
  }

  for (int ik = 0; ik < nk; ++ik) {  // :734
    t1[ik] = ((sds6a1 * anar) * freq[ik]) * powi(nexdens[ik], sds6p1);
  }
  for (int ik = 0; ik < nk; ++ik) adf[ik] = anar * powi(nexdens[ik], sds6p2);  // :738
  const Real xfac = (one - one / xfr) / (xfr - one / xfr);                    // :739
  // :740-745, DFII reassigned whole on every IK, as written.
  for (int ik = 1; ik <= nk; ++ik) {
    for (int j = 0; j < nk; ++j) dfii[j] = dsii[j] / tpi;
    if (ik > 1 && ik < nk) dfii[ik - 1] = dfii[ik - 1] * xfac;
    Real sum = zero;
    for (int j = 0; j < ik; ++j) sum = sum + adf[j] * dfii[j];
    t2[ik - 1] = sds6a2 * sum;
  }

  // :748. -1.0* is a sign flip. The vector loops put T1 and T2 in the destination of
  // maxps; the scalar tail, the last element when NK is odd, has MAX(0.0,T1) as
  // maxss(0.0, T1): there a NaN T1 stays NaN and a -0 T1 stays -0.
  for (int ik = 0; ik < nk; ++ik) {
    const Real m2 = maxss(t2[ik], zero);
    const Real m1 = ((nk % 2) == 1 && ik == nk - 1) ? maxss(zero, t1[ik]) : maxss(t1[ik], zero);
    t12[ik] = -(m2 + m1);
  }
  for (int ith = 1; ith <= nth; ++ith) {  // :749-751
    for (int ik = 0; ik < nk; ++ik) d[ikn[ik] + ith - 2] = t12[ik];
  }
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];  // :753
  // :756-764 (W3_T6: STME21 and test WRITEs) are output only and not translated.
}

}  // namespace ww::srce::st6
