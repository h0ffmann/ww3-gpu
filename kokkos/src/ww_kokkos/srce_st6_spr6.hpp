// kokkos/src/ww_kokkos/srce_st6_spr6.hpp
// W3SPR6: mean wave parameters (energy, mean frequency, mean wavenumber, maximum action,
// weighted peak frequency) for the ST6 source terms, one spectral point.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src6md.F90:129-264 at NOAA-EMC/WW3 03adda91;
// author S. Zieger. Evidence: kokkos/port_report/W3SPR6_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st6 {

namespace spr6_detail {
// maxss of w3src6md.o, d the destination: a NaN in either operand or a tie gives s.
// MAX(1.0E-7,X) (:245, :246) and MAX(1E-10,X) (:256) all put X in the destination.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

// TPIINV (constants.F90:75) as the object loads it, 0x3e22f983.
inline constexpr Real kTpiInv = 0x1.45f306p-3f;
static_assert(kTpiInv == ww::srce::kTpiInv);

// MAXVAL(A) (:230) as gfortran emits it for a REAL array: skip leading NaNs with a
// `limit <= a` test from -Inf, then `a > limit ? a : limit` (maxss, :230 disc. 6); all
// NaN gives NaN (0x7fc00000), an empty array gives -HUGE (0xff7fffff).
KOKKOS_INLINE_FUNCTION Real maxval_a(const Real* a, int nth, int nk) {
  Real limit = -Kokkos::infinity_v<Real>;
  bool nonempty = false;
  bool fast = false;
  for (int ik = 0; ik < nk; ++ik) {
    for (int ith = 0; ith < nth; ++ith) {
      const Real x = a[ik * nth + ith];
      if (fast) {
        limit = (x > limit) ? x : limit;
      } else {
        nonempty = true;
        if (limit <= x) {
          limit = x;
          fast = true;
        }
      }
    }
  }
  if (fast) return limit;
  if (nonempty) return Kokkos::quiet_NaN_v<Real>;
  return -Kokkos::finite_max_v<Real>;
}
}  // namespace spr6_detail

/// W3SPR6 for one point. a = A(NTH,NK) as A(NSPEC), cg = CG(1:NK), wn = WN(1:NK), 0-based.
/// Module state of W3GDATMD: sig = &SIG(1) (SIG is SIG(0:NK+1)), dden = DDEN(1:NK),
/// dsii = DSII(1:NK); dth, fte, ftf, ftwn its scalars DTH, FTE, FTF, FTWN.
/// eb holds NK reals of caller work (the automatic array EB, :217).
///
/// Every SUM (SUM(A,1), SUM(EB), ...) adds its terms one by one from 0.0 in index order:
/// the object's vector loops (:229, :233-235, :255-256) are in-order reductions that
/// extract the lanes and add them in sequence.
KOKKOS_INLINE_FUNCTION void w3spr6_point(const Real* a, const Real* cg, const Real* wn,
                                         Real& emean, Real& fmean, Real& wnmean, Real& amax,
                                         Real& fp, const Params& p, const Real* sig,
                                         const Real* dden, const Real* dsii, Real dth, Real fte,
                                         Real ftf, Real ftwn, Real* eb) {
  const int nk = p.nk;
  const int nth = p.nth;
  using spr6_detail::kTpiInv;
  using spr6_detail::maxss;

  // :229. EB = (SUM(A,1)*DDEN)/CG.
  for (int ik = 0; ik < nk; ++ik) {
    Real s = static_cast<Real>(0.);
    for (int ith = 0; ith < nth; ++ith) s = s + a[ik * nth + ith];
    eb[ik] = (s * dden[ik]) / cg[ik];
  }
  // :230.
  amax = spr6_detail::maxval_a(a, nth, nk);

  // :233-235.
  Real e = static_cast<Real>(0.);
  for (int ik = 0; ik < nk; ++ik) e = e + eb[ik];
  Real f = static_cast<Real>(0.);
  for (int ik = 0; ik < nk; ++ik) f = f + eb[ik] / sig[ik];
  Real w = static_cast<Real>(0.);
  for (int ik = 0; ik < nk; ++ik) w = w + eb[ik] / Kokkos::sqrt(wn[ik]);

  // :239-242.
  const Real eband = eb[nk - 1] / dden[nk - 1];
  e = e + eband * fte;
  f = f + eband * ftf;
  w = w + eband * ftwn;
  emean = e;

  // :245-246. ( )**2 is one product of the quotient.
  fmean = (kTpiInv * e) / maxss(f, 1.0E-7f);
  const Real q = e / maxss(w, 1.0E-7f);
  wnmean = q * q;

  // :252-258.
  fp = static_cast<Real>(0.);
  if (static_cast<Real>(4.0) * Kokkos::sqrt(e) > 0.05f) {
    // :255. EB = ((SUM(A,1)*SIG)/CG)*DTH, the column sums recomputed as in the Fortran.
    for (int ik = 0; ik < nk; ++ik) {
      Real s = static_cast<Real>(0.);
      for (int ith = 0; ith < nth; ++ith) s = s + a[ik * nth + ith];
      eb[ik] = ((s * sig[ik]) / cg[ik]) * dth;
    }
    // :256. EB**4 is (EB*EB)*(EB*EB) at -O3; numerator terms (SIG*EB**4)*DSII.
    Real num = static_cast<Real>(0.);
    for (int ik = 0; ik < nk; ++ik) {
      const Real e2 = eb[ik] * eb[ik];
      num = num + (sig[ik] * (e2 * e2)) * dsii[ik];
    }
    Real den = static_cast<Real>(0.);
    for (int ik = 0; ik < nk; ++ik) {
      const Real e2 = eb[ik] * eb[ik];
      den = den + (e2 * e2) * dsii[ik];
    }
    fp = (num / maxss(den, 1E-10f)) * kTpiInv;  // :256-257
  }
}

}  // namespace ww::srce::st6
