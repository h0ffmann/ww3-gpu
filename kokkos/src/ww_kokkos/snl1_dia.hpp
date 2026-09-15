// kokkos/src/ww_kokkos/snl1_dia.hpp
// The DIA nonlinear interaction source term, ported to Kokkos.
//
// WW3 heritage: WW3/model/src/w3snl1md.F90 (WAVEWATCH III 7.14, `develop`),
// SUBROUTINE W3SNL1, lines 115-473.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "snl1_config.hpp"
#include "snl1_tables.hpp"

namespace ww::snl1 {

/// W3SNL1 for `npts` sea points in one kernel launch.
///
/// Where W3SNL1 takes one spectrum, this takes a batch: `a`, `s` and `d` are
/// (NSPEC, npts) and `cg` is (NK, npts), both LayoutLeft, so column `ip` is the
/// contiguous spectrum of point `ip` -- exactly the memory a Fortran caller owns.
/// `sig` is SIG(1:NK) on the device, `kdmean` the mean relative depth per point.
///
/// `s` and `d` are overwritten, not accumulated, as in the Fortran.
void snl1(const Config& c, const Tables& t, ConstRealView1D sig, ConstRealView2D a,
          ConstRealView2D cg, ConstRealView1D kdmean, RealView2D s, RealView2D d);

}  // namespace ww::snl1
