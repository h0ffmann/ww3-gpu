// kokkos/src/ww_kokkos/wavset_differentiate_xydir.hpp
// DIFFERENTIATE_XYDIR: the driver that takes the x and y derivatives of a nodal field
// on the unstructured grid by calling DIFFERENTIATE_XYDIR_MAPSTA. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:363-435 at NOAA-EMC/WW3 03adda91;
// authors Mathieu Dutour-Sikiric, Aron Roland.
// Evidence: kokkos/port_report/DIFFERENTIATE_XYDIR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <utility>

#include "real.hpp"

namespace ww::wavset {

/// DIFFERENTIATE_XYDIR (:363-435). var, dvdx, dvdy are VAR(npa), DVDX(npa), DVDY(npa),
/// REAL(rkind) with rkind = 4 (PDLIB/yowdatapool.F90:48), Fortran base 1, here 0-based.
/// The callee is taken as a callable invoked as differentiate_xydir_mapsta(var, dvdx, dvdy)
/// (:433), host code that binds the module state (npa, NE, INE, iplg, PDLIB_IEN, PDLIB_TRIA,
/// MAPSTA), the WEI work array and the MPI exchange of PDLIB_exchange1Dreal (:348-349) and
/// calls ww::wavset::differentiate_xydir_mapsta (wavset_differentiate_xydir_mapsta.hpp).
/// A direct call would pass that state through this driver and g++ inlines the callee
/// into it, so the object would no longer be the Fortran's single tail call.
/// The commented-out DIFFERENTIATE_XYDIR_NATIVE (:434) is not called and not translated.
template <class DifferentiateXydirMapsta>
inline void differentiate_xydir(const Real* var, Real* dvdx, Real* dvdy,
                                DifferentiateXydirMapsta&& differentiate_xydir_mapsta) {
  std::forward<DifferentiateXydirMapsta>(differentiate_xydir_mapsta)(var, dvdx, dvdy);
}

}  // namespace ww::wavset
