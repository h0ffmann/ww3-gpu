// kokkos/src/ww_kokkos/srce_ic1.hpp
// W3SIC1, ported to Kokkos: wave attenuation by sea ice with a decay rate k_i
// uniform in frequency, D = S/E = -2*Cg*k_i, one spectral point. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3sic1md.F90:93-369 at NOAA-EMC/WW3 03adda91;
// authors E. Rogers, S. Zieger. Switch IC1 (at the pin only
// regtests/ww3_tic1.4/input/switch_IC1IS2, without W3_S or W3_T). Called by W3SRCE
// at w3srcemd.F90:1367 (IC_NUMERICS, ICE > 0) and :2150 (not IC_NUMERICS). The
// only #ifdef branches inside the routine are W3_S, W3_T, W3_T0, W3_T1 and W3_T38
// output, which are not ported: a kernel never prints. DEPTH (read only by W3_T38)
// and IX, IY (only to index ICEP1) are not passed: the caller gathers ICEP1(IX,IY).
// ICECOEF2..5 are set to 0 (:294-297) and never read; -O3 drops them.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3sic1md.F90 (WW3's CMake Release flags, switch_lab_shrd with IC0 -> IC1):
//   :309   ICECOEF1 == 0 (ucomiss: a NaN takes the ice branch);
//   :329   -((CG*2)*WN_I): -2.*CG*WN_I with the sign moved out (xorps) and
//          CG*2 as CG+CG (addps), both exact, so the same bits as the source;
//   :340   D*A per element.
// No FMA, no libm call, no reduction. The vectorised loops (WN_I fill, D1D, S)
// are element-wise (no bits change); the MAPWN gather is scalar.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce {

namespace ic1_detail {
/// W3SIC1 returns this when INFLAGS2(-7) is false: the Fortran writes "ICE
/// PARAMETER 1 REQUIRED BUT NOT SELECTED" and calls EXTCDE(2) (:299-302). The
/// kernel cannot stop the run: the shim must.
inline constexpr int kExtcde = 2;
}  // namespace ic1_detail

/// W3SIC1(A, DEPTH, CG, IX, IY, S, D) for one point. icep1 = ICEP1(IX,IY);
/// inflag_icep1 = INFLAGS2(-7). cg = CG(1:NK) (W3SRCE's CG1), a/s/d =
/// A/S/D(1:NSPEC), all 0-based; mapwn = MAPWN(1:NSPEC), whose values are 1-based
/// IK. d1d (NK) is caller-provided work for the Fortran local D1D. The local
/// WN_I(1:NK) is ICECOEF1 in every element (:292, :324) and is read as that scalar.
///
/// Returns 0, or 2 when the Fortran would stop in EXTCDE(2); d is then 0 (D = 0.0
/// at :289) and s is not written.
KOKKOS_INLINE_FUNCTION int w3sic1_point(const Params& p, Real icep1, bool inflag_icep1,
                                        const int* mapwn, const Real* a, const Real* cg,
                                        Real* s, Real* d, Real* d1d) {
  const int nk = p.nk;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);

  // :289.
  for (int is = 0; is < nspec; ++is) d[is] = zero;

  // :299-302.
  if (!inflag_icep1) return ic1_detail::kExtcde;

  // :304.
  const Real icecoef1 = icep1;

  if (icecoef1 == zero) {
    // :310.
    for (int is = 0; is < nspec; ++is) d[is] = zero;
  } else {
    // :324-330. WN_I = ICECOEF1.
    const Real wn_i = icecoef1;
    for (int ik = 1; ik <= nk; ++ik) {
      d1d[ik - 1] = -((static_cast<Real>(2.) * cg[ik - 1]) * wn_i);
    }
    // :334-336. MAPWN holds 1-based IK.
    for (int ikth = 0; ikth < nspec; ++ikth) d[ikth] = d1d[mapwn[ikth] - 1];
  }

  // :340.
  for (int is = 0; is < nspec; ++is) s[is] = d[is] * a[is];
  return 0;
}

}  // namespace ww::srce
