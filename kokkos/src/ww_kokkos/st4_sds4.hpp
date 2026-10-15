// kokkos/src/ww_kokkos/st4_sds4.hpp
// W3SDS4, ported to Kokkos (phase 1, translation). NOT YET TRANSLATED: this stub
// fixes the interface the shim (fortran_iface/st4_shim.cpp) calls; its body is
// written by the translation of W3SDS4.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:2043-2659 at WW3@bb08623a.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"

namespace ww::st4 {

/// Reals and ints of per-point work storage w3sds4_point needs (no allocation in a kernel).
inline int sds4_work_reals(const Params& p) { (void)p; return 0; }
inline int sds4_work_ints(const Params& p) { (void)p; return 0; }

/// W3SDS4 for one point. a = A(1:NSPEC), k = K(1:NK), cg = CG(1:NK), srhs,
/// ddiag, brlambda = (1:NSPEC), whitecap = WHITECAP(1:4), all 0-based.
/// flogrd57, flogrd58 are FLOGRD(5,7), FLOGRD(5,8) as 0/1. IX, IY are not
/// read and not passed. The OUT arrays arrive holding the caller's values:
/// the early return at w3src4md.F90:2581 leaves WHITECAP as it was.
KOKKOS_INLINE_FUNCTION void w3sds4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* k, const Real* cg, Real ustar, Real usdir,
                                         Real depth, Real dair, Real dlwmean, int flogrd57,
                                         int flogrd58, Real* srhs, Real* ddiag, Real* brlambda,
                                         Real* whitecap, Real* work, int* iwork) {
  (void)p; (void)f; (void)a; (void)k; (void)cg; (void)ustar; (void)usdir; (void)depth;
  (void)dair; (void)dlwmean; (void)flogrd57; (void)flogrd58; (void)srhs; (void)ddiag;
  (void)brlambda; (void)whitecap; (void)work; (void)iwork;
}

}  // namespace ww::st4
