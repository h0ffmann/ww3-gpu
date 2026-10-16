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

namespace ww::srce {

/// W3SBT1(A, CG, WN, DEPTH, S, D) for one point.
/// sig = SIG(1:NK), cg = CG(1:NK), wn = WN(1:NK), a/s/d = A/S/D(1:NSPEC), all
/// 0-based. mapwn = MAPWN(1:NSPEC), whose values are 1-based IK. cbeta (NK) is
/// caller-provided work for the Fortran local CBETA (w3sbt1md.F90:205).
KOKKOS_INLINE_FUNCTION void w3sbt1_point(const Params& p, const Real* sig, const int* mapwn,
                                         const Real* a, const Real* cg, const Real* wn,
                                         Real depth, Real* s, Real* d, Real* cbeta) {
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;

  // :218-221. The INTEGER 6 is compared as REAL 6.
  if (depth * wn[0] > static_cast<Real>(6)) {
    for (int is = 0; is < nspec; ++is) d[is] = 0;
    for (int is = 0; is < nspec; ++is) s[is] = 0;
    return;
  }

  const Real factor = p.sbtc1 / depth;  // :229

  // :237-241. After the EXIT, IK is the first IK with WN(IK)*DEPTH > 6.;
  // when the loop runs to its end, IK = NK+1 (Fortran DO semantics).
  int ik = 1;
  for (; ik <= nk; ++ik) {
    if (wn[ik - 1] * depth > static_cast<Real>(6.)) break;
    const Real x = cg[ik - 1] * wn[ik - 1] / sig[ik - 1] - static_cast<Real>(0.5);
    cbeta[ik - 1] = factor * (x > static_cast<Real>(0.) ? x : static_cast<Real>(0.));  // MAX_EXPR <x, 0.>
  }

  const int nscut = (ik - 1) * nth;  // :245

  // :247-253. MAPWN holds 1-based IK.
  for (int is = 1; is <= nscut; ++is) d[is - 1] = cbeta[mapwn[is - 1] - 1];
  for (int is = nscut + 1; is <= nspec; ++is) d[is - 1] = 0;

  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];  // :255
}

}  // namespace ww::srce
