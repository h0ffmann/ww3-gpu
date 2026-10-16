// kokkos/src/ww_kokkos/srce_st2_spr2.hpp
// W3SPR2: mean wave parameters (energy, mean frequency, mean wavenumber, spectral maximum,
// Phillips' constant per IK, peak frequency) for the Tolman and Chalikov (1996) source
// terms, one spectral point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src2md.F90:125-288 at NOAA-EMC/WW3 03adda91; authors
// H. L. Tolman, D. Chalikov. Evidence: kokkos/port_report/W3SPR2_KOKKOS.md.
// The routine's only #ifdef blocks are W3_S tracing and the W3_T USE of NDST, which nothing
// reads; DEPTH, U, NITTIN, ZWIND, CINXSI and the W3DISPMD tables it USEs are never read.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce::st2 {

namespace spr2_detail {
// maxss of w3src2md.o, d the destination: a NaN or a tie gives s. Every site in W3SPR2 has
// the variable as d and the constant or A(ITH,IK) as s, whatever the source order.
KOKKOS_INLINE_FUNCTION Real maxss(Real d, Real s) { return (d > s) ? d : s; }

inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV constants.F90:62
// TPIINV (constants.F90:75), .rodata 0x3e22f983 of w3src2md.o.
inline constexpr Real kTpiInv = 0x1.45f306p-3f;
static_assert(kTpiInv == ww::srce::kTpiInv);
}  // namespace spr2_detail

/// W3SPR2 for one point, arrays 0-based: a = A(NTH,NK) as A(NSPEC), cg = CG(1:NK),
/// wn = WN(1:NK), alfa = ALFA(1:NK) out; sig = SIG(1:NK) and dden = DDEN(1:NK) of
/// W3GDATMD start at element 1, not at SIG(0). dth, fte, ftf, ftwn: W3GDATMD DTH, FTE,
/// FTF, FTWN. eb (NK) is work for the automatic array EB. depth and u are unused, as in
/// the Fortran.
KOKKOS_INLINE_FUNCTION void w3spr2_point(const Real* a, const Real* cg, const Real* wn,
                                         Real depth, Real fpi, Real u, Real ustar,
                                         Real& emean, Real& fmean, Real& wnmean, Real& amax,
                                         Real* alfa, Real& fp, const Params& p, Real dth,
                                         const Real* sig, const Real* dden, Real fte,
                                         Real ftf, Real ftwn, Real* eb) {
  using spr2_detail::maxss;
  const int nk = p.nk;
  const int nth = p.nth;

  // :240.
  const Real ust = maxss(ustar, static_cast<Real>(0.0001));

  emean = static_cast<Real>(0.);
  fmean = static_cast<Real>(0.);
  wnmean = static_cast<Real>(0.);
  amax = static_cast<Real>(0.);

  // :249-255. The sum over ITH stays in order. For NTH >= 8 the object takes MAX over
  // ITH in four lanes seeded with AMAX (maxps, lane j holds ITH-1 = j mod 4), folds them
  // as max(max(l3,l1), max(l2,l0)) (movhlps, shufps), then the NTH mod 4 tail in order;
  // for NTH 1..7 it is the scalar chain. Same value for finite inputs without signed
  // zeros; the lanes are kept so a NaN or a -0 gives the same bits too.
  for (int ik = 1; ik <= nk; ++ik) {
    const Real* acol = a + static_cast<long>(nth) * (ik - 1);
    Real sum = static_cast<Real>(0.);
    if (nth >= 8) {
      Real lane[4] = {amax, amax, amax, amax};
      const int nvec = nth / 4;
      for (int v = 0; v < nvec; ++v) {
        for (int j = 0; j < 4; ++j) {
          const Real x = acol[4 * v + j];
          sum = sum + x;
          lane[j] = maxss(lane[j], x);
        }
      }
      const Real r0 = maxss(lane[2], lane[0]);
      const Real r1 = maxss(lane[3], lane[1]);
      Real m = maxss(r1, r0);
      for (int ith = 4 * nvec + 1; ith <= nth; ++ith) {
        const Real x = acol[ith - 1];
        sum = sum + x;
        m = maxss(m, x);
      }
      amax = m;
    } else {
      for (int ith = 1; ith <= nth; ++ith) {
        const Real x = acol[ith - 1];
        sum = sum + x;
        amax = maxss(amax, x);
      }
    }
    eb[ik - 1] = sum;
  }

  // :259-265. WN**3 is (WN*WN)*WN in the object.
  const Real twodth = static_cast<Real>(2.) * dth;
  for (int ik = 1; ik <= nk; ++ik) {
    const Real w = wn[ik - 1];
    const Real w3 = (w * w) * w;
    alfa[ik - 1] = ((twodth * sig[ik - 1]) * eb[ik - 1]) * w3;
    eb[ik - 1] = (eb[ik - 1] * dden[ik - 1]) / cg[ik - 1];
    emean = emean + eb[ik - 1];
    fmean = fmean + eb[ik - 1] / sig[ik - 1];
    wnmean = wnmean + eb[ik - 1] / Kokkos::sqrt(w);
  }

  // :270-276.
  const Real eband = eb[nk - 1] / dden[nk - 1];
  emean = emean + eband * fte;
  fmean = fmean + eband * ftf;
  wnmean = wnmean + eband * ftwn;

  const Real eps = static_cast<Real>(1.E-7);
  fmean = (spr2_detail::kTpiInv * emean) / maxss(fmean, eps);
  const Real r = emean / maxss(wnmean, eps);
  wnmean = r * r;

  // :280-282. FPISTR**3 is (F*F)*F in the object.
  const Real fpistr = maxss((fpi * ust) / spr2_detail::kGrav, static_cast<Real>(0.008));
  const Real f3 = (fpistr * fpistr) * fpistr;
  fp = (((static_cast<Real>(3.6E-4) + static_cast<Real>(0.92) * fpistr) -
         static_cast<Real>(6.3E-10) / f3) /
        ust) *
       spr2_detail::kGrav;
  fp = fp * spr2_detail::kTpiInv;
}

}  // namespace ww::srce::st2
