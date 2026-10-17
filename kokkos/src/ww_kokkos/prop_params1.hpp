// kokkos/src/ww_kokkos/prop_params1.hpp
// What W3XYP1 reads beyond prop_params.hpp: the two flux factor maps W3MAP1
// fills. Phase 1: host pointers, one spectral bin per call.
//
// WW3 heritage: WW3-noaa/model/src/w3pro1md.F90:301-873 (W3XYP1, the USE lists
// at :426-438) at NOAA-EMC/WW3 03adda91, as switch_lab_shrd with PR1 in place of
// "PR3 UQ" compiles it. Everything else it reads is already in Params, Grid and
// Step (prop_params.hpp): NX, NY, NSEA, NK, NTH, NSPEC, ICLOSE, FLCX, FLCY,
// FLAGLL, FLCUR, DTCFL; MAPSF, CLATS, SIG, ECOS, ESIN, DPDX, DPDY, DQDX, DQDY,
// GSQRT, ATRNX, ATRNY; CG, CX, CY and the boundary input. W3MAP1 is not
// translated here: FACVX and FACVY are copied from the Fortran after each FLMAP
// (w3wavemd.F90:1457), never rebuilt.
#pragma once

#include "real.hpp"

namespace ww::prop {

/// FACVX, FACVY (NY*NX), lower bound 1 (w3adatmd.F90:1417-1418, filled by
/// W3MAP1, w3pro1md.F90:201-265): FACVX(IXY) = facvx[IXY-1]. The inverse of
/// the number of sea cells on each side of a cell face, 0 where both are land.
struct Facv {
  const Real* facvx;
  const Real* facvy;
};

}  // namespace ww::prop
