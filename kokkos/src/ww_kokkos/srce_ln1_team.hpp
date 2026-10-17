// kokkos/src/ww_kokkos/srce_ln1_team.hpp
// W3SLN1 split over the threads of a team, for the team-parallel W3SRCE (srce_kernel_team.hpp,
// w3srce_split). The arithmetic of every element, sum, MIN/MAX and running value is that of
// w3sln1_point (srce_ln1.hpp), which stays the code of every host run; this file only says which
// thread does which element, and where the barriers are. Heritage, quirks and operand orders:
// srce_ln1.hpp.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

#include "srce_ln1.hpp"
#include "srce_team.hpp"

namespace ww::srce {

/// W3SLN1 for one point by the whole team; ex is the team's executor
/// (srce_team.hpp, TeamExec): no sum anywhere, so DIRF, WNF
#include "libm_device.hpp"
/// and S are each split over the team element by element; the scalars are of
/// the arguments only and every thread computes them. A team caller puts a
/// barrier after.
template <class X>
KOKKOS_INLINE_FUNCTION void w3sln1_team(const X& ex, const Params& p, const Real* sig,
                                        const Real* ecos, const Real* esin, const Real* k,
                                        Real fhigh, Real ustar, Real usdir, Real* s, Real* dirf,
                                        Real* wnf) {
  using ln1_detail::fmax2;
  using ln1_detail::fmin2;
  const int nk = p.nk;
  const int nth = p.nth;

  // :175-176. -O3 turns COS(USDIR), SIN(USDIR) into one sincosf; on the host
  // call it so the bits are glibc's sincosf, whatever cosf/sinf would give.
  Real cosu = 0;
  Real sinu = 0;
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((::sincosf(usdir, &sinu, &cosu);))
  KOKKOS_IF_ON_DEVICE((dm::dev::sincosf(usdir, &sinu, &cosu);))
#else
  cosu = Kokkos::cos(usdir);
  sinu = Kokkos::sin(usdir);
#endif

  // :178-180. MAX(0., x)**4 as -O3: t = MAX_EXPR <x, 0.>, (t*t)*(t*t).
  ex.for_(nth, [&](int ith0) {
    const int ith = ith0 + 1;
    const Real m = fmax2(static_cast<Real>(0.),
                         ecos[ith - 1] * cosu + esin[ith - 1] * sinu);
    const Real m2 = m * m;
    dirf[ith - 1] = m2 * m2;
  });

  // :182. USTAR**4 as (u*u)*(u*u).
  const Real u2 = ustar * ustar;
  const Real fac = p.slnc1 * (u2 * u2);
  // :183. FSPM*GRAV is evaluated first, then divided by 28.*USTAR.
  const Real ff1 = p.fspm * kGrav / (static_cast<Real>(28.) * ustar);
  const Real ff2 = p.fshf * fmin2(sig[nk - 1], fhigh);                           // :184
  // :185, as maxss/minss compute it: FF1>FF2?FF1:FF2, then M<2SIG?M:2SIG. FF1 is NaN
  // when RFPM = 0 (w3gridmd.F90:1690) and USTAR = 0, and the build then keeps FF2.
  const Real ffilt = fmin2(static_cast<Real>(2.) * sig[nk - 1], fmax2(ff2, ff1));

  // :186-193; the FAC/K(IK) quotient is formed before the EXP product.
  ex.for_(nk, [&](int ik0) {
    const int ik = ik0 + 1;
    const Real rfr = sig[ik - 1] / ffilt;
    if (rfr < static_cast<Real>(0.5)) {
      wnf[ik - 1] = 0;
    } else {
      // RFR**(-4) as -O3: 1. / ((r*r)*(r*r)).
      const Real r2 = rfr * rfr;
      const Real rm4 = static_cast<Real>(1.) / (r2 * r2);
      wnf[ik - 1] = fac / k[ik - 1] * dm::expf(-rm4);
    }
  });
  ex.sync();

  // :197-199, one (IK, ITH) per iteration.
  ex.for_(nk * nth, [&](int e) {
    const int ik = 1 + e / nth;
    const int ith = 1 + e % nth;
    s[(ik - 1) * nth + ith - 1] = wnf[ik - 1] * dirf[ith - 1];
  });
}


}  // namespace ww::srce
