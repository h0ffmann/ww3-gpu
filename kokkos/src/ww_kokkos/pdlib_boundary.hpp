// kokkos/src/ww_kokkos/pdlib_boundary.hpp
// APPLY_BOUNDARY_CONDITION: the open-boundary spectra, interpolated in time, written into VA on
// the boundary nodes of this rank. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd_pdlib.F90:5150-5336 at NOAA-EMC/WW3 03adda91;
// A. Roland, M. Dutour-Sikiric. Evidence: kokkos/port_report/APPLY_BOUNDARY_CONDITION_KOKKOS.md.
// Called from PDLIB_JACOBI_GAUSS_SEIDEL_BLOCK (:5788) after VA was divided by CG (:5692-5708);
// the caller computes RD10, RD20 = DSEC21(..) (:5251-5253) only when FLBPI.
// ISBPI can name one node twice: each thread owns one ISP and walks IBI in order, last wins.
// SPDX-License-Identifier: LGPL-3.0-or-later
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
