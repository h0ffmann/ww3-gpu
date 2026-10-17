// kokkos/src/ww_kokkos/srce_ln1.hpp
// W3SLN1, ported to Kokkos: linear wind input (Cavaleri and Malanotte-Rizzoli
// 1982, filtered after Tolman 1992), one spectral point. Phase 1, translated, not
// regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3sln1md.F90:56-211 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. W3_T output (S, the test print) is not ported: a kernel
// never prints.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE of w3sln1md.F90 (WW3's own
// flags, switch_lab_shrd): the **4 and **(-4) powers are GCC's power tree (x*x,
// then squared; 1/(that) for the negative one), COS/SIN of one argument are one
// sincosf call (__builtin_cexpif), EXP is scalar expf (the loop is not
// vectorised), MIN/MAX are MIN_EXPR/MAX_EXPR. The DIRF and S loops are
// vectorised element by element, which changes no bits.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_team.hpp"

namespace ww::srce {

namespace ln1_detail {
// Fortran MIN/MAX of two REALs: fmin2(a,b) = b<a?b:a, fmax2(a,b) = b>a?b:a. A NaN or
// a +0/-0 tie gives `a`; each call site passes the operands in the order of the
// build's minss/maxss (w3sln1md.o), which is not always the GIMPLE order.
KOKKOS_INLINE_FUNCTION Real fmin2(Real a, Real b) { return (b < a) ? b : a; }
KOKKOS_INLINE_FUNCTION Real fmax2(Real a, Real b) { return (b > a) ? b : a; }
}  // namespace ln1_detail

/// W3SLN1(K, FHIGH, USTAR, USDIR, S) for one point.
/// sig = SIG(1:NK), ecos/esin = ECOS/ESIN (the first NTH entries are read),
/// k = K(1:NK), all 0-based. s = S(NTH,NK) column-major: S(ITH,IK) is
/// s[(IK-1)*NTH + ITH-1]. dirf (NTH) and wnf (NK) are caller-provided work for
/// the Fortran locals DIRF and WNF (w3sln1md.F90:160-161).
///
/// Written against an executor (srce_team.hpp): no sum anywhere, so DIRF, WNF
/// and S are each split over the team element by element; the scalars are of
/// the arguments only and every thread computes them. A team caller puts a
/// barrier after.
template <class X>
KOKKOS_INLINE_FUNCTION void w3sln1_body(const X& ex, const Params& p, const Real* sig,
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
  KOKKOS_IF_ON_DEVICE((cosu = Kokkos::cos(usdir); sinu = Kokkos::sin(usdir);))
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
      wnf[ik - 1] = fac / k[ik - 1] * Kokkos::exp(-rm4);
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

/// W3SLN1 for one point, on one thread.
KOKKOS_INLINE_FUNCTION void w3sln1_point(const Params& p, const Real* sig, const Real* ecos,
                                         const Real* esin, const Real* k, Real fhigh,
                                         Real ustar, Real usdir, Real* s, Real* dirf,
                                         Real* wnf) {
  w3sln1_body(SerialExec{}, p, sig, ecos, esin, k, fhigh, ustar, usdir, s, dirf, wnf);
}

/// W3SLN1 for one point, by the whole team.
template <class Member>
KOKKOS_INLINE_FUNCTION void w3sln1_team(const Member& team, const Params& p, const Real* sig,
                                        const Real* ecos, const Real* esin, const Real* k,
                                        Real fhigh, Real ustar, Real usdir, Real* s, Real* dirf,
                                        Real* wnf) {
  w3sln1_body(TeamExec<Member>{team}, p, sig, ecos, esin, k, fhigh, ustar, usdir, s, dirf, wnf);
}

}  // namespace ww::srce
