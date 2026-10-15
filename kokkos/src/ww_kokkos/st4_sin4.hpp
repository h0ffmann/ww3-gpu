// kokkos/src/ww_kokkos/st4_sin4.hpp
// W3SIN4, ported to Kokkos (phase 1, translation). NOT YET TRANSLATED: this stub
// fixes the interface the shim (fortran_iface/st4_shim.cpp) calls; its body is
// written by the translation of W3SIN4.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:423-1003 at WW3@bb08623a.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"

namespace ww::st4 {

/// Reals and ints of per-point work storage w3sin4_point needs (no allocation in a kernel).
inline int sin4_work_reals(const Params& p) { (void)p; return 0; }
inline int sin4_work_ints(const Params& p) { (void)p; return 0; }

/// W3SIN4 for one point. a = A(1:NSPEC), cg = CG(1:NK), k = K(1:NSPEC) (WN2),
/// s, d = S, D(1:NSPEC), llws = LLWS(1:NSPEC) as 0/1, all 0-based. IX, IY and
/// BRLAMBDA are not read by the routine and are not passed. The OUT arguments
/// arrive holding the caller's values.
KOKKOS_INLINE_FUNCTION void w3sin4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* cg, const Real* k, Real u, Real ustar,
                                         Real drat, Real as, Real usdir, Real z0, Real cd,
                                         Real& tauwx, Real& tauwy, Real& tauwnx, Real& tauwny,
                                         Real* s, Real* d, int* llws, Real* work, int* iwork) {
  (void)p; (void)f; (void)a; (void)cg; (void)k; (void)u; (void)ustar; (void)drat; (void)as;
  (void)usdir; (void)z0; (void)cd; (void)tauwx; (void)tauwy; (void)tauwnx; (void)tauwny;
  (void)s; (void)d; (void)llws; (void)work; (void)iwork;
}

}  // namespace ww::st4
