// kokkos/src/ww_kokkos/srce_is1.hpp
// W3SIS1, ported to Kokkos: spectral reflection (scattering) by sea ice, a
// linear transfer of action from each direction to the other NTH-1 directions of
// the same frequency, one spectral point. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3sis1md.F90:74-227 at NOAA-EMC/WW3 03adda91;
// author S. Zieger. Switch IS1. Called by W3SRCE at w3srcemd.F90:1388 (IC_NUMERICS,
// ICE > 0) and :2172 (not IC_NUMERICS, unconditionally; the routine tests ICE > 0
// itself, :190). No switch file at the pin has IS1 (model/bin/switch_*,
// regtests/*/input*/switch_*), and the only #ifdef branches inside the routine are
// W3_S and W3_T output, which are not ported: a kernel never prints. SIG, SIG2,
// DDEN2, TH, DTH, ECOS and NDSE are in the USE list (:138-141) but never read.
// W3SIS1 writes S only: W3SRCE's VDIR is not an argument (:1388).
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3sis1md.F90 (WW3's CMake Release flags, switch_lab_shrd with IS0 -> IS1):
//   :185   (IS1C1*ICE) + IS1C2, then maxss: (v > 0) ? v : 0 (NaN gives 0);
//   :195   A(IS) >= 0 (comiss/jb: a NaN A(IS) is skipped);
//   :196   S(IS) - A(IS)*ALPHA;
//   :200   S(IS2) + (A(IS)*ALPHA) / REAL(NTH-1), a true division (cvtsi2ss of
//          NTH-1, no reciprocal), the product shared with :196;
//   :207   S / DTMIN, a true division per element.
// No FMA, no libm call. The ITH/ITH2 loops are scalar and in source order (each
// S(IS2) accumulates in the Fortran's order); only the S/DTMIN loop is vectorised,
// element-wise (divps, no bits change).
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "srce_params.hpp"

namespace ww::srce {

/// W3SIS1's grid scalars (W3GDATMD SICPS%IS1C1, IS1C2, per grid via W3SETG,
/// w3gdatmd.F90:2845-2846). DTMIN is Params::dtmin. Packing is the shim's job.
struct Is1Params {
  Real is1c1;  ///< IS1C1, scale factor for the ice concentration
  Real is1c2;  ///< IS1C2, offset
};

/// W3SIS1(A, ICE, S) for one point. a/s = A/S(1:NSPEC), 0-based, IS = ITH +
/// (IK-1)*NTH. p.nk, p.nth, p.nspec and p.dtmin are read.
KOKKOS_INLINE_FUNCTION void w3sis1_point(const Params& p, const Is1Params& is1,
                                         const Real* a, Real ice, Real* s) {
  const int nk = p.nk;
  const int nth = p.nth;
  const int nspec = p.nspec;
  const Real zero = static_cast<Real>(0.);

  // :179.
  for (int is = 0; is < nspec; ++is) s[is] = zero;

  // :185. maxss operand order from w3sis1md.o: (v > 0) ? v : 0.
  const Real v = is1.is1c1 * ice + is1.is1c2;
  const Real alpha = (v > zero) ? v : zero;

  // :190-220.
  if (alpha > zero && ice > zero) {
    for (int ik = 1; ik <= nk; ++ik) {
      for (int ith = 1; ith <= nth; ++ith) {
        const int is = ith + (ik - 1) * nth;
        if (a[is - 1] >= zero) {
          const Real aa = a[is - 1] * alpha;
          s[is - 1] = s[is - 1] - aa;
          for (int ith2 = 1; ith2 <= nth; ++ith2) {
            const int is2 = ith2 + (ik - 1) * nth;
            if (is2 != is) {
              s[is2 - 1] = s[is2 - 1] + aa / static_cast<Real>(nth - 1);
            }
          }
        }
      }
    }
    // :207.
    for (int is = 0; is < nspec; ++is) s[is] = s[is] / p.dtmin;
  }
}

}  // namespace ww::srce
