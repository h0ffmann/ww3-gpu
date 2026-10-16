// kokkos/src/ww_kokkos/pdlib_boundary_va.hpp
// APPLY_BOUNDARY_CONDITION_VA: the open-boundary spectra BBPI0/BBPIN, interpolated in
// time, times CG, written into VA. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:5037-5148 at NOAA-EMC/WW3
// 03adda91; A. Roland, M. Dutour-Sikiric. Evidence:
// kokkos/port_report/APPLY_BOUNDARY_CONDITION_VA_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "pdlib_params.hpp"
#include "real.hpp"

namespace ww::pdlib {

namespace bcva_detail {
KOKKOS_INLINE_FUNCTION Real min_ss(Real a, Real b) { return (a < b) ? a : b; }
KOKKOS_INLINE_FUNCTION Real max_ss(Real a, Real b) { return (a > b) ? a : b; }
}  // namespace bcva_detail

/// ISEA_TO_JSEA(NSEA) (:289), 1-based values, <= 0 off this rank: isea_to_jsea(isea-1).
template <class MemSpace>
using BcvaIseaToJsea = Kokkos::View<const int*, Kokkos::LayoutLeft, MemSpace>;
/// CG(0:NK+1,0:NSEA) (w3adatmd.F90:1325), REAL: cg(ik, isea), bases kept.
template <class MemSpace>
using BcvaCgView = Kokkos::View<const Real**, Kokkos::LayoutLeft, MemSpace>;
/// VA(NSPEC,0:NSEALM) (w3wdatmd.F90:491), REAL: va(isp-1, jsea).
template <class MemSpace>
using BcvaVaView = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;

/// The body from :5122, for a caller that has checked GTYPE == UNGTYPE, FLBPI and
/// IAPROC <= NAPROC and computed rd10, rd20 = DSEC21 (:5116-5117).
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
        for (int ibi = 1; ibi <= nbi; ++ibi) {  // :5132, in order: the last IBI wins
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
