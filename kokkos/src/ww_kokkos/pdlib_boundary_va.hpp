// kokkos/src/ww_kokkos/pdlib_boundary_va.hpp
// APPLY_BOUNDARY_CONDITION_VA, ported to Kokkos: the open-boundary spectra
// BBPI0/BBPIN, interpolated in time, times CG, written into VA on the boundary
// sea points of this rank. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:5037-5148 at NOAA-EMC/WW3
// 03adda91, switch PDLIB (A. Roland, M. Dutour-Sikiric). Called from W3WAVE
// (w3wavemd.F90:1321) under IF (FLBPI), followed by PDLIB_exchange2DREAL_zero(VA),
// which stays in Fortran. Host side, in the shim: GTYPE .eq. UNGTYPE (:5114),
// FLBPI .and. IAPROC .le. NAPROC (:5122) and RD10 = DSEC21(TBPI0,TIME),
// RD20 = DSEC21(TBPI0,TBPIN) (:5116-5117), both REAL; the ELSE of :5118-5121
// (RD10=1., RD20=0.) is dead, since :5122 needs FLBPI.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and the object of
// w3profsmd_pdlib.F90 (WW3's own flags, switch_PDLIB), all REAL, no REAL(8):
//   :5125  rd20 > 0x1.0624dep-10f (0.001 as REAL, .rodata.cst4), comiss;
//   :5126  divss rd10/rd20, then maxss(.,0), then minss(.,1);
//   :5127  subss 1 - rd2;
//   :5139  mulss BBPI0*RD1, mulss BBPIN*RD2, addss (two roundings, no FMA);
//   :5140  mulss CG*eAC, maxss(.,0).
// SSE MIN/MAX as emitted (dest OP src ? dest : src); every call puts the
// computed value first and the constant second, as the object does.
//
// Parallel over ISP, serial over IBI. ISBPI can name one sea point for two IBI,
// so VA(ISP,JSEA) can be written more than once and the last IBI wins. Each
// thread owns one ISP and walks IBI in the Fortran's order, so every element
// of VA receives the same sequence of writes as in the Fortran; no thread
// writes another's ISP, and nothing is reduced.
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace bcva_detail {
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }
}  // namespace bcva_detail

/// ISEA_TO_JSEA(NSEA) (w3profsmd_pdlib.F90:289), integer, 1-based values,
/// <= 0 for a sea point not on this rank: isea_to_jsea(isea-1).
template <class MemSpace>
using BcvaIseaToJsea = Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace>;
/// CG(0:NK+1,0:NSEA) (w3adatmd.F90:1325), REAL: cg(ik, isea), bases kept.
template <class MemSpace>
using BcvaCgView = Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace>;
/// VA(NSPEC,0:NSEALM) (w3wdatmd.F90:491), REAL: va(isp-1, jsea).
template <class MemSpace>
using BcvaVaView = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;

/// The body of APPLY_BOUNDARY_CONDITION_VA from :5122, for a caller that has
/// already checked GTYPE == UNGTYPE, FLBPI and IAPROC <= NAPROC.
template <class ExecSpace>
void apply_boundary_condition_va(const ExecSpace& exec,
                                 const PdlibGeometry<typename ExecSpace::memory_space>& g,
                                 const PdlibBoundary<typename ExecSpace::memory_space>& b,
                                 const BcvaIseaToJsea<typename ExecSpace::memory_space>& isea_to_jsea,
                                 const BcvaCgView<typename ExecSpace::memory_space>& cg, Real rd10,
                                 Real rd20, const BcvaVaView<typename ExecSpace::memory_space>& va) {
  using bcva_detail::max_ss;
  using bcva_detail::min_ss;
  Real rd1 = rd10;  // :5123-5124
  Real rd2 = rd20;
  if (rd2 > kRd2Floor) {
    rd2 = min_ss(max_ss(rd1 / rd2, kZero), static_cast<Real>(1.0f));  // :5126
    rd1 = static_cast<Real>(1.0f) - rd2;                               // :5127
  } else {
    rd1 = kZero;                   // :5129
    rd2 = static_cast<Real>(1.0f);  // :5130
  }
  const int nbi = b.nbi;
  const int nth = g.nth;
  const int nspec = g.nk * g.nth;
  const auto isbpi = b.isbpi;
  const auto bbpi0 = b.bbpi0;
  const auto bbpin = b.bbpin;
  Kokkos::parallel_for(
      "pdlib_boundary_va", Kokkos::RangePolicy<ExecSpace>(exec, 0, nspec),
      KOKKOS_LAMBDA(const int isp0) {
        const int ik = 1 + isp0 / nth;  // ISP = ITH + (IK-1)*NTH, :5138
        for (int ibi = 1; ibi <= nbi; ++ibi) {  // :5132, in order (header)
          const int isea = isbpi(ibi - 1);
          const int jsea = isea_to_jsea(isea - 1);  // :5134
          if (jsea > 0) {
            const Real eac = rd1 * bbpi0(isp0, ibi) + rd2 * bbpin(isp0, ibi);  // :5139
            va(isp0, jsea) = max_ss(cg(ik, isea) * eac, kZero);                // :5140-5141
          }
        }
      });
  exec.fence();
}

}  // namespace ww::pdlib
