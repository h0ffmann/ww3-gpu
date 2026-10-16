// kokkos/src/ww_kokkos/pdlib_w3xypug_block_implicit.hpp
// PDLIB_W3XYPUG_BLOCK_IMPLICIT: the driver of the block-implicit propagation on WW3's
// unstructured grids. With B_JGS_USE_JACOBI it calls PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK
// (pdlib_jgs_block.hpp) with its own arguments; otherwise the Fortran writes an error line and
// stops. No floating-point arithmetic. Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/w3profsmd_pdlib.F90:2742-2813 at h0ffmann/WW3 8e433f99, text
// identical at NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/PDLIB_W3XYPUG_BLOCK_IMPLICIT_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles (no S, no
// DEBUGSOLVER). Not ported: W3_S (the USE of STRACE, :2788-2790; no call) and W3_DEBUGSOLVER
// (the USE of IAPROC, :2792-2794, and the WRITE/FLUSH to unit 740+IAPROC, :2800-2803).
//
// Errors: the Fortran's WRITE(*,*) and STOP 'Correct your implicit solver options' (:2808-2809)
// are returned as kXypugBlockImplicitStopNoJacobi, for the shim to write the line and stop, as
// W3DIMS and W3IOGR return EXTCDE's code: a host routine of the port never prints or stops.
// The code is negative, as kPdlibInitStop (pdlib_pdlib_init.hpp) for PDLIB_INIT's bare STOP:
// the STOP exits with status 0, so no positive exit or abort code may stand for it.
// LCALC (:2797) is passed on by the Fortran and never read by PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK
// (in :5463-6406 it appears only in the dummy list, :5463, and its declaration, :5577), whose
// C++ has no such argument.
// The JGS hooks take no arguments: calcarray_jacobi_vec (DTG, FACX, FACY, VGX, VGY) and
// apply_boundary_condition (IMOD) must read from their own context the values passed here.
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_jgs_block.hpp"
#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

/// What pdlib_w3xypug_block_implicit returns. Ok: the Fortran returned. NormNoCasSig: the
/// value 2 of pdlib_jacobi_gauss_seidel_block, passed through (see its header). StopNoJacobi:
/// the Fortran reached WRITE(*,*) 'Error: You need to use with JGS_USE_JACOBI' and
/// STOP 'Correct your implicit solver options' (:2808-2809), which gfortran ends with exit
/// status 0; nothing was computed.
inline constexpr int kXypugBlockImplicitOk = 0;
inline constexpr int kXypugBlockImplicitNormNoCasSig = 2;
inline constexpr int kXypugBlockImplicitStopNoJacobi = -1;  ///< STOP, :2809; exit status 0

/// PDLIB_W3XYPUG_BLOCK_IMPLICIT(IMOD, FACX, FACY, DTG, VGX, VGY, LCALC),
/// w3profsmd_pdlib.F90:2742-2813, under switch_PDLIB. b_jgs_use_jacobi is W3GDATMD
/// B_JGS_USE_JACOBI (w3gdatmd.F90:1418, :2867), LOGICAL(4) as the word gfortran tests
/// (`/= 0`). module_params carries the module settings PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK reads;
/// its imod, facx, facy, dtg, vgx, vgy are replaced by the dummies, as the Fortran passes them
/// by reference (:2805). kRef1, kTrnk, kSt4: W3_REF1, W3_TRNK, W3_ST4, the defaults those of
/// switch_PDLIB (REF0, no TRNK, ST4); the routine's own object does not depend on them.
template <bool kRef1 = false, bool kTrnk = false, bool kSt4 = true, class ExecSpace, class Hooks,
          class Spr4>
int pdlib_w3xypug_block_implicit(const ExecSpace& exec, int imod, Real facx, Real facy, Real dtg,
                                 Real vgx, Real vgy, bool lcalc, int b_jgs_use_jacobi,
                                 const JgsParams& module_params,
                                 const PdlibGeometry<typename ExecSpace::memory_space>& g,
                                 const JgsMesh<typename ExecSpace::memory_space>& m,
                                 const JgsArrays<typename ExecSpace::memory_space>& a,
                                 const JgsWork<typename ExecSpace::memory_space>& w, Hooks& hooks,
                                 const Spr4& spr4) {
  (void)lcalc;
  if (b_jgs_use_jacobi != 0) {  // :2804
    JgsParams p = module_params;
    p.imod = imod;
    p.facx = facx;
    p.facy = facy;
    p.dtg = dtg;
    p.vgx = vgx;
    p.vgy = vgy;
    return pdlib_jacobi_gauss_seidel_block<kRef1, kTrnk, kSt4>(exec, p, g, m, a, w, hooks,
                                                               spr4);  // :2805-2806
  }
  return kXypugBlockImplicitStopNoJacobi;  // :2808-2809
}

}  // namespace ww::pdlib
