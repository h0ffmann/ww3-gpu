// kokkos/src/ww_kokkos/srce_st1_spr1.hpp
// W3SPR1: mean wave parameters (energy, frequency, wavenumber, maximum action) for the
// WAM-3 (ST1) source terms, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src1md.F90:87-240 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. Evidence: kokkos/port_report/W3SPR1_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st1 {

namespace spr1_detail {
// maxss/maxps of w3src1md.o, d the destination: a NaN or a +0/-0 tie gives s.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

// Folded by gfortran (.rodata.cst4 of w3src1md.o).
inline constexpr Real kTpiInv = 0x1.45f306p-3f;  // TPIINV constants.F90:75, 0x3e22f983
inline constexpr Real kEps = 0x1.ad7f2ap-24f;    // 1.E-7 :223-224, 0x33d6bf95
static_assert(kTpiInv == ww::srce::kTpiInv);
static_assert(kEps == static_cast<Real>(1.E-7));
}  // namespace spr1_detail

/// W3SPR1 for one point, arrays 0-based: a = A(NTH,NK) as A(NSPEC), cg = CG(1:NK),
/// wn = WN(1:NK), sig = SIG(1:NK) (not SIG(0)), dden = DDEN(1:NK); fte, ftf, ftwn are
/// W3GDATMD's FTE, FTF, FTWN. eb (NK) is work: EB(IK) = eb[IK-1].
KOKKOS_INLINE_FUNCTION void w3spr1_point(const Params& p, const Real* a, const Real* cg,
                                         const Real* wn, Real& emean, Real& fmean,
                                         Real& wnmean, Real& amax, const Real* sig,
                                         const Real* dden, Real fte, Real ftf, Real ftwn,
                                         Real* eb) {
  using spr1_detail::maxss;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real zero = static_cast<Real>(0.);

  // :189-192.
  emean = zero;
  fmean = zero;
  wnmean = zero;
  amax = zero;

  // 1. :196-202. EB is summed in ITH order on both paths. AMAX is not: for NTH >= 8 the
  // object keeps four maxps lanes (ITH = 4g+j+1 in lane j, each started from AMAX), folds
  // them as movhlps/maxps/shufps/maxps, then the NTH mod 4 tail one by one; for NTH < 8 it
  // is one scalar chain. A NaN or a signed-zero tie therefore depends on the path.
  for (int ik = 1; ik <= nk; ++ik) {
    const Real* ak = a + (ik - 1) * nth;
    Real e = zero;
    if (nth >= 8) {
      Real l0 = amax, l1 = amax, l2 = amax, l3 = amax;
      const int nv = (nth / 4) * 4;
      for (int i = 0; i < nv; i += 4) {
        e = e + ak[i];
        e = e + ak[i + 1];
        e = e + ak[i + 2];
        e = e + ak[i + 3];
        l0 = maxss(l0, ak[i]);
        l1 = maxss(l1, ak[i + 1]);
        l2 = maxss(l2, ak[i + 2]);
        l3 = maxss(l3, ak[i + 3]);
      }
      const Real r0 = maxss(l2, l0);  // maxps %xmm3,%xmm2 after movhlps
      const Real r1 = maxss(l3, l1);
      Real m = maxss(r1, r0);  // shufps $0x55, maxps %xmm2,%xmm0
      for (int i = nv; i < nth; ++i) {
        e = e + ak[i];
        m = maxss(m, ak[i]);
      }
      amax = m;
    } else {
      for (int i = 0; i < nth; ++i) {
        e = e + ak[i];
        amax = maxss(amax, ak[i]);
      }
    }
    eb[ik - 1] = e;
  }

  // 2. :206-211. Vectorised in the object with in-order sums: the same bits.
  Real es = zero, fs = zero, ws = zero;
  for (int ik = 1; ik <= nk; ++ik) {
    const Real e = eb[ik - 1] * dden[ik - 1] / cg[ik - 1];
    eb[ik - 1] = e;
    es = es + e;
    fs = fs + e / sig[ik - 1];
    ws = ws + e / Kokkos::sqrt(wn[ik - 1]);
  }

  // 3. :216-219.
  const Real eband = eb[nk - 1] / dden[nk - 1];
  es = es + eband * fte;
  fs = fs + eband * ftf;
  ws = ws + eband * ftwn;

  // 4. :223-224. maxss 1.E-7,FMEAN: a NaN sum gives 1.E-7. **2 is t*t.
  emean = es;
  fmean = spr1_detail::kTpiInv * es / maxss(fs, spr1_detail::kEps);
  const Real t = es / maxss(ws, spr1_detail::kEps);
  wnmean = t * t;
}

}  // namespace ww::srce::st1
