// kokkos/src/ww_kokkos/srce_bt1_team.hpp
// W3SBT1 split over the threads of a team, for the team-parallel W3SRCE (srce_kernel_team.hpp,
// w3srce_split). The arithmetic of every element, sum, MIN/MAX and running value is that of
// w3sbt1_point (srce_bt1.hpp), which stays the code of every host run; this file only says which
// thread does which element, and where the barriers are. Heritage, quirks and operand orders:
// srce_bt1.hpp.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <Kokkos_Core.hpp>

#include "srce_bt1.hpp"
#include "srce_team.hpp"

namespace ww::srce {

/// W3SBT1 for one point by the whole team; ex is the team's executor
/// (srce_team.hpp, TeamExec): the CBETA loop, which ends at
/// the first IK with WN(IK)*DEPTH > 6, runs on one thread and leaves that IK in
/// *ish, memory the team shares; D and S = D*A are then split element by
/// element. A team caller puts a barrier after.
template <class X>
KOKKOS_INLINE_FUNCTION void w3sbt1_team(const X& ex, const Params& p, const Real* sig,
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


}  // namespace ww::srce
