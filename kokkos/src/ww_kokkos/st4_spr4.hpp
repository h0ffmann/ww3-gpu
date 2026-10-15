// kokkos/src/ww_kokkos/st4_spr4.hpp
// W3SPR4, ported to Kokkos (phase 1, translation). NOT YET TRANSLATED: this stub
// fixes the interface the shim (fortran_iface/st4_shim.cpp) calls; its body is
// written by the translation of W3SPR4.
//
// WW3 heritage: WW3/model/src/w3src4md.F90:142-384 at WW3@bb08623a.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"
#include "st4_calc_ustar.hpp"

namespace ww::st4 {

/// Reals and ints of per-point work storage w3spr4_point needs (no allocation in a kernel).
inline int spr4_work_reals(const Params& p) { (void)p; return 0; }
inline int spr4_work_ints(const Params& p) { (void)p; return 0; }

/// W3SPR4 for one point. Arrays are the caller's, 0-based: a = A(NTH,NK) as
/// A(NSPEC), cg = CG(1:NK), wn = WN(1:NK), llws = LLWS(1:NSPEC) as 0/1.
/// ustar, usdir are INOUT; the other references are OUT and arrive holding the
/// caller's values, so a path that leaves one unassigned keeps it.
KOKKOS_INLINE_FUNCTION void w3spr4_point(const Params& p, const Fields& f, const Real* a,
                                         const Real* cg, const Real* wn, Real u, Real udir,
                                         Real tauwx, Real tauwy, const int* llws, Real& ustar,
                                         Real& usdir, Real& emean, Real& fmean, Real& fmean1,
                                         Real& wnmean, Real& amax, Real& cd, Real& z0,
                                         Real& charn, Real& fmeanws, Real& dlwmean, Real* work,
                                         int* iwork) {
  (void)p; (void)f; (void)a; (void)cg; (void)wn; (void)u; (void)udir; (void)tauwx;
  (void)tauwy; (void)llws; (void)ustar; (void)usdir; (void)emean; (void)fmean; (void)fmean1;
  (void)wnmean; (void)amax; (void)cd; (void)z0; (void)charn; (void)fmeanws; (void)dlwmean;
  (void)work; (void)iwork;
}

}  // namespace ww::st4
