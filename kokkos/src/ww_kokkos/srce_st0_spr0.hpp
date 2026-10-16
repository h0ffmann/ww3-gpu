// kokkos/src/ww_kokkos/srce_st0_spr0.hpp
// W3SPR0: mean energy, frequency and wavenumber and the largest action density of the
// spectrum of one point, for the case without input and dissipation (switch ST0).
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src0md.F90:81-231 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3SPR0_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st0 {

namespace spr0_detail {
// maxss/maxps of w3src0md.o with d the destination: a NaN or a +0/-0 tie gives s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

inline constexpr Real kMeanFloor = 1.e-7f;  // 1.E-7 :214-215, 0x33d6bf95
}  // namespace spr0_detail

/// W3SPR0 for one point, arrays 0-based. a = A(NTH,NK) flat (SPEC of W3SRCE), cg, wn,
/// dden = CG, WN, DDEN(1:NK); sig = SIG(1:NK), the pointer at SIG(1), not SIG(0).
/// fte, ftf, ftwn are W3GDATMD FTE, FTF, FTWN. eb (NK) is work for the automatic EB.
/// Precondition nk >= 1: with NK <= 0 the object reads EB(0), outside its allocation.
KOKKOS_INLINE_FUNCTION void w3spr0_point(const Real* a, const Real* cg, const Real* wn,
                                         Real& emean, Real& fmean, Real& wnmean, Real& amax,
                                         const Params& p, const Real* sig, const Real* dden,
                                         Real fte, Real ftf, Real ftwn, Real* eb) {
  using spr0_detail::maxss;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real zero = static_cast<Real>(0.);

  // :180-183.
  emean = zero;
  fmean = zero;
  wnmean = zero;
  amax = zero;

  // :187-193. gfortran vectorises the ITH loop when NTH >= 8 (and NTH /= HUGE): four
  // MAX lanes seeded with AMAX, reduced as max(max(m3,m1), max(m2,m0)), then the
  // NTH mod 4 tail in order. EB keeps the Fortran's order (fold-left). The lanes only
  // change the result with a NaN or a signed zero in A; they are kept for that.
  const bool lanes = (nth >= 8 && nth != 2147483647);
  Real amx = amax;
  for (int ik = 0; ik < nk; ++ik) {
    const Real* aik = a + static_cast<long>(ik) * nth;
    Real s = zero;
    if (lanes) {
      const int nv = nth / 4;
      Real m0 = amx, m1 = amx, m2 = amx, m3 = amx;
      for (int v = 0; v < nv; ++v) {
        const Real x0 = aik[4 * v], x1 = aik[4 * v + 1];
        const Real x2 = aik[4 * v + 2], x3 = aik[4 * v + 3];
        s = s + x0;
        s = s + x1;
        s = s + x2;
        s = s + x3;
        m0 = maxss(m0, x0);
        m1 = maxss(m1, x1);
        m2 = maxss(m2, x2);
        m3 = maxss(m3, x3);
      }
      const Real r0 = maxss(m2, m0);
      const Real r1 = maxss(m3, m1);
      amx = maxss(r1, r0);
      for (int ith = 4 * nv; ith < nth; ++ith) {
        const Real x = aik[ith];
        s = s + x;
        amx = maxss(amx, x);
      }
    } else {
      for (int ith = 0; ith < nth; ++ith) {
        const Real x = aik[ith];
        s = s + x;
        amx = maxss(amx, x);
      }
    }
    eb[ik] = s;
  }
  amax = amx;

  // :197-202.
  Real em = zero, fm = zero, wm = zero;
  for (int ik = 0; ik < nk; ++ik) {
    const Real e = (eb[ik] * dden[ik]) / cg[ik];
    eb[ik] = e;
    em = em + e;
    fm = fm + e / sig[ik];
    wm = wm + e / Kokkos::sqrt(wn[ik]);
  }

  // :207-210.
  const Real eband = eb[nk - 1] / dden[nk - 1];
  em = em + eband * fte;
  fm = fm + eband * ftf;
  wm = wm + eband * ftwn;

  // :214-215. MAX(1.E-7, X) is maxss X,1e-7: a NaN X gives 1e-7.
  emean = em;
  fmean = (kTpiInv * em) / maxss(fm, spr0_detail::kMeanFloor);
  const Real w = em / maxss(wm, spr0_detail::kMeanFloor);
  wnmean = w * w;
}

}  // namespace ww::srce::st0
