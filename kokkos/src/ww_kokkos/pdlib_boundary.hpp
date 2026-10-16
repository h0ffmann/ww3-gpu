// kokkos/src/ww_kokkos/pdlib_boundary.hpp
// APPLY_BOUNDARY_CONDITION, ported to Kokkos: the open-boundary spectra
// BBPI0/BBPIN, interpolated in time, times CLATS and the dry-node mask, written
// as action density into VA on the boundary nodes of this rank. Phase 1,
// translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:5150-5336 at NOAA-EMC/WW3
// 03adda91, switch PDLIB (A. Roland, M. Dutour-Sikiric). Its one caller is
// PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK (:5788), which has already divided VA by CG
// (:5692-5708), so VA holds action density per node JX = 1..npa here;
// pdlib_jgs_block.hpp calls it through hooks.apply_boundary_condition().
//
// Host side, in the caller: IF (FLBPI), RD10 = DSEC21(TBPI0,TIME) and RD20 =
// DSEC21(TBPI0,TBPIN) (:5251-5253), both REAL, DSEC21 called in that order on
// packed copies of the time arrays. The ELSE (RD10=1., RD20=0., :5255-5256) is
// dead, since the second IF (:5258) also needs FLBPI; the C++ returns at once
// when b.flbpi is 0.
//
// Compile-time variants. The #ifdefs of :5150-5336 are W3_S (STRACE),
// W3_DEBUGSOLVERCOH, W3_DEBUGSOLVER, W3_DEBUGIOBC and W3_DEBUGSOLVERALL: tracing
// and diagnostic WRITEs and sums that never feed VA. None is defined by any switch
// file with PDLIB of regtests/ww3_tp2.17/input*/, regtests/ww3_ufs1.1/input_unstr/
// or model/bin/switch_NWS_rwps (11 files; 6 add REF1, 1 TRNK, none S or DEBUG*,
// v, grep -w, 2026-10-16), and none touches the arithmetic. Not ported; no
// template flag.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3profsmd_pdlib.F90 (WW3's CMake flags, switch_PDLIB; the -fdump object equals
// the CMake one byte for byte, v), all REAL:
//   :5259-5266  rd20 > 0x1.0624dep-10f (comiss); divss rd10/rd20; maxss(.,0)
//               then minss(.,1), the computed value the destination both times;
//               subss 1 - rd2;
//   :5286-5287  mulss BBPI0*RD1, mulss BBPIN*RD2, addss, mulss CLATS(ISEA), then
//               mulss by REAL(IOBDP_LOC(JX)) (cvtsi2ss of the INTEGER*1): four
//               roundings, no FMA.
//
// Parallel over ISP, serial over IBI. ISBPI can name one node for two IBI, so
// VA(ISP,JX) can be written more than once and the last IBI wins. The Fortran
// walks IBI, then ITH, then IK (ISP = ITH + (IK-1)*NTH covers 1..NSPEC once per
// IBI); each thread here owns one ISP and walks IBI in order, so every element of
// VA receives the same sequence of stores. Nothing is reduced, no atomics.
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace bc_detail {
// SSE MIN/MAX as emitted (dest OP src ? dest : src); the computed value is the
// destination at both sites of :5262.
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }
}  // namespace bc_detail

/// VA(NSPEC,0:NSEALM) (w3wdatmd.F90:491), REAL, bases kept: va(isp-1, jx).
template <class MemSpace>
using BcVaView = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;

/// APPLY_BOUNDARY_CONDITION(IMOD), :5150-5336. rd10 and rd20 are DSEC21's two
/// results, which the caller computes only when b.flbpi (see the header). IMOD is
/// not read by the routine outside the DEBUGSOLVERCOH prints.
template <class ExecSpace>
void apply_boundary_condition(const ExecSpace& exec,
                              const PdlibGeometry<typename ExecSpace::memory_space>& g,
                              const PdlibBoundary<typename ExecSpace::memory_space>& b, Real rd10,
                              Real rd20, const BcVaView<typename ExecSpace::memory_space>& va) {
  using bc_detail::max_ss;
  using bc_detail::min_ss;
  if (b.flbpi == 0) return;  // :5251, :5258
  Real rd1 = rd10;           // :5259-5260
  Real rd2 = rd20;
  if (rd2 > kRd2Floor) {                                                 // :5261
    rd2 = min_ss(max_ss(rd1 / rd2, kZero), static_cast<Real>(1.0f));  // :5262
    rd1 = static_cast<Real>(1.0f) - rd2;                               // :5263
  } else {
    rd1 = kZero;                    // :5265
    rd2 = static_cast<Real>(1.0f);  // :5266
  }
  const int nbi = b.nbi;
  const int nspec = g.nk * g.nth;
  const auto isbpi = b.isbpi;
  const auto bbpi0 = b.bbpi0;
  const auto bbpin = b.bbpin;
  const auto mapsf = g.mapsf;
  const auto ipgl_npa = g.ipgl_npa;
  const auto clats = g.clats;
  const auto iobdp = g.iobdp_loc;
  Kokkos::parallel_for(
      "pdlib_apply_boundary_condition", Kokkos::RangePolicy<ExecSpace>(exec, 0, nspec),
      KOKKOS_LAMBDA(const int isp0) {
        for (int ibi = 1; ibi <= nbi; ++ibi) {  // :5278, in order (header)
          const int isea = isbpi(ibi - 1);      // :5279
          const int ip_glob = mapsf(isea - 1, 0);   // :5280
          const int jx = ipgl_npa(ip_glob - 1);     // :5281
          if (jx > 0) {                             // :5282
            const Real e = rd1 * bbpi0(isp0, ibi) + rd2 * bbpin(isp0, ibi);  // :5286
            va(isp0, jx) = (e * clats(isea)) * static_cast<Real>(iobdp(jx - 1));  // :5287
          }
        }
      });
  exec.fence();
}

}  // namespace ww::pdlib
