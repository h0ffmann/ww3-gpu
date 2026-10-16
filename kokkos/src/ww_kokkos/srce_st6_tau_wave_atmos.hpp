// kokkos/src/ww_kokkos/srce_st6_tau_wave_atmos.hpp
// TAU_WAVE_ATMOS: the stress carried from the waves to the atmosphere by the negative
// part of the ST6 wind input (opposing winds), extended to 10 Hz, one spectral point.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3src6md.F90:1157-1325 at NOAA-EMC/WW3 03adda91;
// authors S. Zieger, Q. Liu. Evidence: kokkos/port_report/TAU_WAVE_ATMOS_KOKKOS.md.
// ⚠ On a device logf is CUDA's, not glibc's.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_st6_util.hpp"
#include "st4_params.hpp"

namespace ww::srce::st6 {

namespace tau_wave_atmos_detail {
// FRQMAX = 10. (:1229) and TPI (constants.F90:73) as w3src6md.o loads them.
inline constexpr Real kFrqMax = static_cast<Real>(10.);
inline constexpr Real kTpi = 0x1.921fb6p+2f;
static_assert(kTpi == ww::srce::kTpi);
// 1/c = sigma/grav (:1293), the REAL literal 0.101978 as w3src6md.o multiplies by it, 0x3dd0d9d7.
inline constexpr Real kInvGrav = 0x1.a1b3aep-4f;
static_assert(kInvGrav == 0.101978f);

KOKKOS_INLINE_FUNCTION Real logf_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::logf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::log(x);))
#else
  return Kokkos::log(x);
#endif
}

// SUM(ABS(MIN(0.,S))*RESHAPE(E,(/NTH,NK/)),1) * DTH (:1282-1283, :1308-1309). MIN is
// minss/minps with S in the destination: S < 0 ? S : 0, so NaN and -0. give +0. The
// object's vector loop adds the four lanes one by one, in index order, from 0.0.
KOKKOS_INLINE_FUNCTION void sdens_neg(const Real* s, const Real* e, Real dth, int nth, int nk,
                                      Real* sdens) {
  for (int ik = 0; ik < nk; ++ik) {
    Real val = static_cast<Real>(0.);
    for (int ith = 0; ith < nth; ++ith) {
      const Real x = s[ik * nth + ith];
      const Real m = (x < static_cast<Real>(0.)) ? x : static_cast<Real>(0.);
      val = val + Kokkos::fabs(m) * e[ik * nth + ith];
    }
    sdens[ik] = dth * val;
  }
}
}  // namespace tau_wave_atmos_detail

/// NK10Hz of :1245-1246, MAX(NK, CEILING(ALOG(FRQMAX/(SIG(1)/TPI))/ALOG(XFR))+1).
/// CEILING is the object's cvttss2si/cvtsi2ss/ucomiss/adc: i = INT(q), then i+1 when
/// REAL(i) >= q, else i+2 (a NaN q gives INT_MIN+2, so NK). sig1 = SIG(1).
KOKKOS_INLINE_FUNCTION int tau_wave_atmos_nk10hz(Real sig1, Real xfr, int nk) {
  using namespace tau_wave_atmos_detail;
  const Real q = logf_(kFrqMax / (sig1 / kTpi)) / logf_(xfr);
  const int i = ww::st4::fint(q);
  const int n = (static_cast<Real>(i) >= q) ? i + 1 : i + 2;
  return (n > nk) ? n : nk;
}

/// TAU_WAVE_ATMOS(S, CINV, SIG, DSII, TAUNWX, TAUNWY) for one point. Returns 0, or 3
/// where the Fortran calls EXTCDE(3) (MK10Hz .NE. NK10Hz, :1274-1277; taunwx, taunwy
/// unwritten).
///
/// s = S(NTH,NK), cinv = CINV(NK), sig = SIG(NK), dsii = DSII(NK), 0-based, as W3SIN6
/// passes its locals (:530). p.nk, p.nth from W3GDATMD; dth, xfr its DTH, XFR;
/// ecos, esin = &ECOS(1), &ESIN(1), read as RESHAPE(ECOS(1:NSPEC),(/NTH,NK/)).
///
/// Module SAVE state of W3SRC6MD (:103-105), built on the host before the first call by
/// the first-call blocks of LFACTOR (:955-969, :995-1001, :1010-1013), which W3SIN6 always
/// calls first (:506, :530), or of this routine (:1248-1262, :1284-1290, :1303-1306):
/// mk10hz = MK10Hz; sig10hz, dsii10hz = SIG10Hz(1:MK10Hz), DSII10Hz(1:MK10Hz). The kernel
/// only reads them. When NK < NK10Hz the Fortran rewrites DSII10Hz every call
/// (:1294-1297) with the expressions that built it, which the object evaluates the same
/// way, so the same bits; the kernel writes that copy into dsii10hz_w instead.
///
/// Caller work, nk10hz = tau_wave_atmos_nk10hz(sig[0], xfr, p.nk) reals each (the
/// ALLOCATEs of :1265-1268; UCINV10Hz is never used): cinv10hz, sdensx10hz, sdensy10hz,
/// dsii10hz_w (dsii10hz_w is not touched when NK >= NK10Hz).
KOKKOS_INLINE_FUNCTION int tau_wave_atmos(const Real* s, const Real* cinv, const Real* sig,
                                          const Real* dsii, Real& taunwx, Real& taunwy,
                                          const ww::srce::Params& p, Real dth, Real xfr,
                                          const Real* ecos, const Real* esin, int mk10hz,
                                          const Real* sig10hz, const Real* dsii10hz,
                                          Real* cinv10hz, Real* sdensx10hz, Real* sdensy10hz,
                                          Real* dsii10hz_w) {
  using namespace tau_wave_atmos_detail;
  (void)dsii;  // read only by the first-call table build (:1305)
  const int nk = p.nk;
  const int nth = p.nth;
  const int nk10hz = tau_wave_atmos_nk10hz(sig[0], xfr, nk);
  if (mk10hz != nk10hz) return 3;

  if (nk < nk10hz) {
    sdens_neg(s, ecos, dth, nth, nk, sdensx10hz);
    sdens_neg(s, esin, dth, nth, nk, sdensy10hz);
    for (int ik = 0; ik < nk; ++ik) cinv10hz[ik] = cinv[ik];
    for (int ik = nk; ik < nk10hz; ++ik) cinv10hz[ik] = sig10hz[ik] * kInvGrav;
    // DSII10Hz = 0.5 * SIG10Hz * (XFR-1.0/XFR), then the first and last bins (:1294-1297).
    const Real c = xfr - static_cast<Real>(1.) / xfr;
    for (int ik = 0; ik < nk10hz; ++ik) dsii10hz_w[ik] = (sig10hz[ik] * static_cast<Real>(0.5)) * c;
    dsii10hz_w[0] = (sig10hz[0] * static_cast<Real>(0.5)) * (xfr - static_cast<Real>(1.));
    dsii10hz_w[nk10hz - 1] =
        ((sig10hz[nk10hz - 1] * static_cast<Real>(0.5)) * (xfr - static_cast<Real>(1.))) / xfr;
    // S_IN(F) prop. F**(-2) (:1300-1301); **2 is one multiplication.
    const Real sx = sdensx10hz[nk - 1];
    const Real sy = sdensy10hz[nk - 1];
    const Real snk = sig10hz[nk - 1];
    for (int ik = nk; ik < nk10hz; ++ik) {
      const Real r = snk / sig10hz[ik];
      sdensx10hz[ik] = sx * (r * r);
    }
    for (int ik = nk; ik < nk10hz; ++ik) {
      const Real r = snk / sig10hz[ik];
      sdensy10hz[ik] = sy * (r * r);
    }
    taunwx = tauwinds(sdensx10hz, cinv10hz, dsii10hz_w, nk10hz);
    taunwy = tauwinds(sdensy10hz, cinv10hz, dsii10hz_w, nk10hz);
  } else {
    for (int ik = 0; ik < nk; ++ik) cinv10hz[ik] = cinv[ik];
    sdens_neg(s, ecos, dth, nth, nk, sdensx10hz);
    sdens_neg(s, esin, dth, nth, nk, sdensy10hz);
    taunwx = tauwinds(sdensx10hz, cinv10hz, dsii10hz, nk10hz);
    taunwy = tauwinds(sdensy10hz, cinv10hz, dsii10hz, nk10hz);
  }
  return 0;
}

}  // namespace ww::srce::st6
