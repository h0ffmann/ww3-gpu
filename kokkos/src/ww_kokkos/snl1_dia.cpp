// kokkos/src/ww_kokkos/snl1_dia.cpp
// W3SNL1 ported to Kokkos: the batched launch over sea points.
//
// WW3 heritage: WW3/model/src/w3snl1md.F90 (WAVEWATCH III 7.14, `develop`),
// SUBROUTINE W3SNL1, lines 115-473. The per-point body, with the Fortran's
// arithmetic, is snl1_team() in snl1_team.hpp; this file chooses who runs it.
//
// Parallel decomposition: one team per sea point, because W3SNL1's working set is
// per-point (an extended spectrum plus nine helper arrays) and wants to live in
// team scratch.
//
// WW_DETERMINISTIC does not change this file: the body has no reduction, so there
// is no summation order to pin down.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ww_kokkos/snl1_dia.hpp"

#include <stdexcept>
#include <string>

#include "ww_kokkos/snl1_team.hpp"

namespace ww::snl1 {
// Named, not anonymous: nvcc rejects an extended __device__ lambda whose enclosing
// function has internal linkage.
namespace detail {

template <class ExecSpace>
void run(const Config& c, const Tables& t, ConstRealView1D sig, ConstRealView2D a,
         ConstRealView2D cg, ConstRealView1D kdmean, RealView2D s, RealView2D d) {
  using TeamPolicy = Kokkos::TeamPolicy<ExecSpace>;
  using TeamMember = typename TeamPolicy::member_type;

  const int npts = a.extent_int(1);
  if (npts == 0) return;

  const Config cfg = c;  // the lambda captures copies; Views are handles
  const Tables tab = t;
  const size_t scratch_bytes = snl1_team_scratch_bytes<ExecSpace>(c, t);

  TeamPolicy policy(npts, Kokkos::AUTO);
  const int level =
      scratch_bytes <= static_cast<size_t>(policy.scratch_size_max(0)) ? 0 : 1;
  if (scratch_bytes > static_cast<size_t>(policy.scratch_size_max(level))) {
    throw std::runtime_error("ww::snl1::snl1: the extended spectrum needs " +
                             std::to_string(scratch_bytes) +
                             " B of team scratch, more than this backend offers at level 0 or 1");
  }
  policy.set_scratch_size(level, Kokkos::PerTeam(scratch_bytes));

  // Column ipt of a LayoutLeft (NSPEC, npts) View is the point's contiguous
  // spectrum, which is what snl1_team() reads and writes.
  Kokkos::parallel_for(
      "srce.snl1.dia", policy, KOKKOS_LAMBDA(const TeamMember& team) {
        const int ipt = team.league_rank();
        const TeamScratch w = snl1_team_scratch(team, level, cfg, tab);
        snl1_team(team, cfg, tab, sig.data(), &a(0, ipt), &cg(0, ipt), kdmean(ipt),
                  &s(0, ipt), &d(0, ipt), w);
      });
}

}  // namespace detail

void snl1(const Config& c, const Tables& t, ConstRealView1D sig, ConstRealView2D a,
          ConstRealView2D cg, ConstRealView1D kdmean, RealView2D s, RealView2D d) {
  detail::run<Kokkos::DefaultExecutionSpace>(c, t, sig, a, cg, kdmean, s, d);
}

#if defined(WW_SNL1_SERIAL_NESTED)
void snl1_serial(const Config& c, const Tables& t, ConstRealView1D sig, ConstRealView2D a,
                 ConstRealView2D cg, ConstRealView1D kdmean, RealView2D s, RealView2D d) {
  detail::run<Kokkos::Serial>(c, t, sig, a, cg, kdmean, s, d);
}
#endif

}  // namespace ww::snl1
