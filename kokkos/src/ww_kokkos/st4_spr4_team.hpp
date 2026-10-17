// kokkos/src/ww_kokkos/st4_spr4_team.hpp
// W3SPR4 split over the threads of a team, for the team-parallel W3SRCE (srce_kernel_team.hpp,
// w3srce_split). The arithmetic of every element, sum, MIN/MAX and running value is that of
// w3spr4_point (st4_spr4.hpp), which stays the code of every host run; this file only says which
// thread does which element, and where the barriers are. Heritage, quirks and operand orders:
// st4_spr4.hpp.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

#include "st4_spr4.hpp"
#include "srce_team.hpp"

namespace ww::st4 {

#include "libm_device.hpp"
/// W3SPR4 for one point by the whole team; ex is the team's executor
/// (srce_team.hpp, TeamExec). Only EB(IK), EB2(IK) of section 1 are split
/// over the team, one IK per iteration (each sums its own row in ITH order);
/// ELCS, ELSN and AMAX run over IK then ITH into one scalar each, so they and
/// everything after them (the libmvec powers, the sums over IK, CALC_USTAR) run
/// on one thread. A team caller puts a barrier before and after.
template <class X>
KOKKOS_INLINE_FUNCTION void w3spr4_team(const X& ex, const Params& p, const Fields& f,
                                        const Real* a, const Real* cg, const Real* wn, Real u,
                                        Real udir, Real tauwx, Real tauwy, const int* llws,
                                        Real& ustar, Real& usdir, Real& emean, Real& fmean,
                                        Real& fmean1, Real& wnmean, Real& amax, Real& cd,
                                        Real& z0, Real& charn, Real& fmeanws, Real& dlwmean,
                                        Real* work, int* iwork) {
  (void)iwork;
  const int nk = p.nk;
  const int nth = p.nth;
  const Real eps = static_cast<Real>(1.E-7);
  Real* eb = work;        // EB(IK)  = eb[IK-1]
  Real* eb2 = work + nk;  // EB2(IK) = eb2[IK-1]
  Real* pwsig = work + 2 * nk;  // SIG(IK)**(2.*WWNMEANPTAIL), :316 and :319
  Real* pwwn = work + 3 * nk;   // WN(IK)**WWNMEANP, :317

  // 1.  EB, EB2: the sums of each IK over ITH (:293-305).
  // Summed in locals and stored once per IK, as w3spr4_point does.
  ex.for_(nk, [&](int ik0) {
    const int ik = ik0 + 1;
    Real ebk = 0;
    Real eb2k = 0;
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      const Real aa = a[(ik - 1) * nth + ith - 1];
      ebk = ebk + aa;
      if (llws[is - 1] != 0) eb2k = eb2k + aa;
    }
    eb[ik - 1] = ebk;
    eb2[ik - 1] = eb2k;
  });
  ex.sync();

  ex.one([&]() {
  Real unz = max_r(static_cast<Real>(0.01), u);  // :280
  ustar = max_r(static_cast<Real>(0.0001), ustar);  // :278

  emean = 0;
  Real emeanws = 0;
  fmeanws = 0;
  fmean = 0;
  fmean1 = 0;
  wnmean = 0;
  amax = 0;
  dlwmean = 0;
  Real elcs = 0;
  Real elsn = 0;

  // 1.  Integral over directions and maximum (:293-305), EB and EB2 summed above.
  // Quirk 11 (:300-301): ELCS and ELSN run over IK, then ITH, into one scalar each.
  Real amaxl = amax;
  for (int ik = 1; ik <= nk; ++ik) {
    const Real sigfac = dm::powf(f.sig(ik), p.ssdsc[11]) * f.dden(ik - 1) / cg[ik - 1];
    for (int ith = 1; ith <= nth; ++ith) {
      const int is = ith + (ik - 1) * nth;
      const Real aa = a[(ik - 1) * nth + ith - 1];
      elcs = elcs + aa * f.ecos(is - 1) * sigfac;
      elsn = elsn + aa * f.esin(is - 1) * sigfac;
      amaxl = max_r(amaxl, aa);  // MAX(AMAX, A): -O3 MAX_EXPR <A, AMAX>
    }
  }
  amax = amaxl;

  dlwmean = dm::atan2f(elsn, elcs);  // :307

  // 2.  Integrate over directions (:311-320).
  // The powers are computed before the sum loop (mvec_powf); the values
  // are the same, and the sums below keep the IK order (in-order reductions at -O3).
  const Real two_wtail = static_cast<Real>(2.) * p.wwnmeanptail;
  // Lane l is IK = l+1.
  // LIBMVEC SITE: -O3 uses _ZGVbN4vv_powf here (INVENTORY.md), emulated on the host
  mvec_powf(
      nk, [&](int l) { return f.sig(l + 1); }, [&](int) { return two_wtail; },
      [&](int l, Real v) { pwsig[l] = v; });
  // LIBMVEC SITE: -O3 uses _ZGVbN4vv_powf here (INVENTORY.md), emulated on the host
  mvec_powf(
      nk, [&](int l) { return wn[l]; }, [&](int) { return p.wwnmeanp; },
      [&](int l, Real v) { pwwn[l] = v; });
  Real emeanl = emean, fmeanl = fmean, fmean1l = fmean1, wnmeanl = wnmean, fmeanwsl = fmeanws;
  for (int ik = 1; ik <= nk; ++ik) {
    const Real ebk = eb[ik - 1] * f.dden(ik - 1) / cg[ik - 1];
    const Real eb2k = eb2[ik - 1] * f.dden(ik - 1) / cg[ik - 1];
    eb[ik - 1] = ebk;
    eb2[ik - 1] = eb2k;
    emeanl = emeanl + ebk;
    fmeanl = fmeanl + ebk / f.sig(ik);
    fmean1l = fmean1l + ebk * pwsig[ik - 1];
    wnmeanl = wnmeanl + ebk * pwwn[ik - 1];
    emeanws = emeanws + eb2k;
    fmeanwsl = fmeanwsl + eb2k * pwsig[ik - 1];  // :319 reuses :316 (CSE)
  }
  emean = emeanl;
  fmean = fmeanl;
  fmean1 = fmean1l;
  wnmean = wnmeanl;
  fmeanws = fmeanwsl;

  // 3.  Add tail beyond discrete spectrum and get mean pars (:325-331).
  Real eband = eb[nk - 1] / f.dden(nk - 1);
  emean = emean + eband * p.fte;
  fmean = fmean + eband * p.ftf;
  fmean1 = fmean1 + eband * p.sstxftftail;
  wnmean = wnmean + eband * p.sstxftwn;
  eband = eb2[nk - 1] / f.dden(nk - 1);
  emeanws = emeanws + eband * p.fte;
  fmeanws = fmeanws + eband * p.sstxftftail;

  // 4.  Final processing (:336-350).
  fmean = kTpiInv * emean / max_r(eps, fmean);
  if (fmean1 < eps) {
    fmean1 = kTpiInv * f.sig(nk);
  } else {
    fmean1 = kTpiInv * dm::powf(max_r(eps, fmean1) / max_r(eps, emean),
                                   static_cast<Real>(1) / (static_cast<Real>(2.) * p.wwnmeanptail));
  }
  wnmean = dm::powf(max_r(eps, wnmean) / max_r(eps, emean),
                       static_cast<Real>(1) / p.wwnmeanp);
  if (fmeanws < eps || emeanws < eps) {
    fmeanws = kTpiInv * f.sig(nk);
  } else {
    fmeanws = kTpiInv * dm::powf(max_r(eps, fmeanws) / max_r(eps, emeanws),
                                    static_cast<Real>(1) / (static_cast<Real>(2.) * p.wwnmeanptail));
  }

  // 5.  Cd and z0 (:355-364), the #else branch of W3_FLX5.
  const Real tauw = Kokkos::sqrt(tauwx * tauwx + tauwy * tauwy);  // TAUWX**2: x*x at -O3
  calc_ustar(p, f, u, tauw, ustar, z0, charn);
  unz = max_r(static_cast<Real>(0.01), u);
  const Real r = ustar / unz;
  cd = r * r;  // (USTAR/UNZ)**2: one division, squared, at -O3
  usdir = udir;
  });
}


}  // namespace ww::st4
