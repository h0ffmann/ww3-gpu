// kokkos/src/ww_kokkos/uost_uost_setgrid.hpp
// UOST_SETGRID: points the module object SRCTRM of the unresolved-obstacles source term at
// grid IGRID, allocating it on first use. Host setup, called per grid by W3INIT and on every
// W3WAVE entry. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3uostmd.F90:231-278 at NOAA-EMC/WW3 03adda91;
// author L. Mentaschi. Evidence: kokkos/port_report/UOST_SETGRID_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <optional>

#include "real.hpp"

namespace ww::uost {

/// UOST_SETGRID on the host; allocates, so it never runs in a kernel.
/// srctrm is the module's CLASS(UOST_SOURCETERM), ALLOCATABLE :: SRCTRM (:96): empty means
/// not allocated. Srctrm must be default-constructible with COSTH/SINTH unallocated and have
/// Real members gammaup, gammadown. grids, sgrds point at GRIDS(nlow), SGRDS(nlow) (Fortran
/// base NLOW, w3gdatmd.F90:1544-1545); igrid is the Fortran index.
/// `setgrid` stands for SRCTRM%SETGRID (:277), called as setgrid(srctrm, grd, sgd). SRCTRM is
/// only ever allocated with its declared type (:274), so the object's vtable call (slot +0x40)
/// always lands in UOST_SOURCETERM_SETGRID (:606-667), which is not translated here.
template <class Srctrm, class Grid, class Sgrd, class Setgrid>
inline void uost_setgrid(int igrid, std::optional<Srctrm>& srctrm, Grid* grids, Sgrd* sgrds,
                         int nlow, Setgrid&& setgrid) {
  // :273-275. ALLOCATE default-initialises the components (:77-78): 1.0e+1, 2.0e+1 in the
  // object; GRD, SGD are left undefined there and set by SETGRID before any use.
  if (!srctrm.has_value()) {
    srctrm.emplace();
    srctrm->gammaup = static_cast<Real>(10);
    srctrm->gammadown = static_cast<Real>(20);
  }

  // :277.
  setgrid(*srctrm, grids[igrid - nlow], sgrds[igrid - nlow]);
}

}  // namespace ww::uost
