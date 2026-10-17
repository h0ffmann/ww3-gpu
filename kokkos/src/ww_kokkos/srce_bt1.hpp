// kokkos/src/ww_kokkos/srce_bt1.hpp
// W3SBT1, ported to Kokkos: bottom friction after JONSWAP, one spectral point.
// Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3sbt1md.F90:88-285 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. W3_T output is not ported: a kernel never prints.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE of w3sbt1md.F90 (WW3's own
// flags, switch_lab_shrd): no libm call, MAX as MAX_EXPR, the CBETA loop scalar,
// S = D*A vectorised element by element (no bits change).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"
#include "srce_team.hpp"

namespace ww::srce {

/// W3SBT1(A, CG, WN, DEPTH, S, D) for one point.
/// sig = SIG(1:NK), cg = CG(1:NK), wn = WN(1:NK), a/s/d = A/S/D(1:NSPEC), all
/// 0-based. mapwn = MAPWN(1:NSPEC), whose values are 1-based IK. cbeta (NK) is
/// caller-provided work for the Fortran local CBETA (w3sbt1md.F90:205).
///
/// Written against an executor (srce_team.hpp): the CBETA loop, which ends at
/// the first IK with WN(IK)*DEPTH > 6, runs on one thread and leaves that IK in
/// *ish, memory the team shares; D and S = D*A are then split element by
/// element. A team caller puts a barrier after.
template <class X>
KOKKOS_INLINE_FUNCTION void w3sbt1_body(const X& ex, const Params& p, const Real* sig,
                                        const int* mapwn, const Real* a, const Real* cg,
                                        const Real* wn, Real depth, Real* s, Real* d, Real* cbeta,
                                        int* ish) {
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;

  // :218-221. The INTEGER 6 is compared as REAL 6.
  if (depth * wn[0] > static_cast<Real>(6)) {
    ex.for_(nspec, [&](int is) {
      d[is] = 0;
      s[is] = 0;
    });
    return;
  }

  const Real factor = p.sbtc1 / depth;  // :229

  // :237-241. After the EXIT, IK is the first IK with WN(IK)*DEPTH > 6.;
  // when the loop runs to its end, IK = NK+1 (Fortran DO semantics).
  ex.one([&]() {
    int ik = 1;
    for (; ik <= nk; ++ik) {
      if (wn[ik - 1] * depth > static_cast<Real>(6.)) break;
      const Real x = cg[ik - 1] * wn[ik - 1] / sig[ik - 1] - static_cast<Real>(0.5);
      cbeta[ik - 1] = factor * (x > static_cast<Real>(0.) ? x : static_cast<Real>(0.));  // MAX_EXPR <x, 0.>
    }
    *ish = ik;
  });
  ex.sync();

  const int nscut = (*ish - 1) * nth;  // :245

  // :247-253 (MAPWN holds 1-based IK), then :255, element by element.
  ex.for_(nspec, [&](int i) {
    const int is = i + 1;
    if (is <= nscut) {
      d[is - 1] = cbeta[mapwn[is - 1] - 1];
    } else {
      d[is - 1] = 0;
    }
    s[is - 1] = d[is - 1] * a[is - 1];
  });
}

/// W3SBT1 for one point, on one thread.
KOKKOS_INLINE_FUNCTION void w3sbt1_point(const Params& p, const Real* sig, const int* mapwn,
                                         const Real* a, const Real* cg, const Real* wn,
                                         Real depth, Real* s, Real* d, Real* cbeta) {
  int ik = 0;
  w3sbt1_body(SerialExec{}, p, sig, mapwn, a, cg, wn, depth, s, d, cbeta, &ik);
}

/// W3SBT1 for one point, by the whole team; ish is one int the team shares.
template <class Member>
KOKKOS_INLINE_FUNCTION void w3sbt1_team(const Member& team, const Params& p, const Real* sig,
                                        const int* mapwn, const Real* a, const Real* cg,
                                        const Real* wn, Real depth, Real* s, Real* d, Real* cbeta,
                                        int* ish) {
  w3sbt1_body(TeamExec<Member>{team}, p, sig, mapwn, a, cg, wn, depth, s, d, cbeta, ish);
}

}  // namespace ww::srce
