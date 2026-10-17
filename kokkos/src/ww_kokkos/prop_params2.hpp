// kokkos/src/ww_kokkos/prop_params2.hpp
// What W3XYP2 reads beyond prop_params.hpp: the swell-age dispersion
// correction (DTME, CLATMN, HPFAC, HQFAC, U10, WN) and the cross-term map
// W3MAP2 fills. Phase 1: host pointers, one spectral bin per call.
//
// WW3 heritage: WW3-noaa/model/src/w3pro2md.F90:508-1264 (W3XYP2, the USE lists
// at :673-694) at NOAA-EMC/WW3 03adda91, as switch_lab_shrd with PR2 in place of
// PR3 (UQ kept) compiles it. Everything else it reads is already in Params, Grid
// and Step (prop_params.hpp): NX, NY, NSEA, NK, NTH, NSPEC, ICLOSE, FLCX, FLCY,
// FLAGLL, FLCUR, DTCFL, DTH, XFR; MAPSF, CLATS, SIG, ECOS, ESIN, DPDX, DPDY,
// DQDX, DQDY, GSQRT, ATRNX, ATRNY, MAPX2, MAPY2, MAPAXY and their counts; CG,
// CX, CY and the boundary input. Grid's MAPCXY, NCENT and MAPTRN are W3MAP3's
// and W3MAPT's and are not read. Under PR2 the maps are W3MAP2's
// (w3pro2md.F90:135-492, called at w3wavemd.F90:1460), copied from the Fortran
// after each FLMAP, never rebuilt (ADR-0011).
#pragma once

#include "prop_params.hpp"
#include "real.hpp"
#include "st4_params.hpp"

namespace ww::prop {

// Constants as gfortran folds them into w3pro2md.o. The static_asserts are the
// bit values read from the -O3 GIMPLE of W3XYP2.
inline constexpr Real kCgdFac = static_cast<Real>(0.5) * ww::st4::kGrav;  // 0.5*GRAV, :841
/// DFAC = 1./RFAC**2 with RFAC = DERA*RADIUS (:817-818): RFAC**2 is one
/// multiply, the whole line folded to a constant.
inline constexpr Real kXyp2Dfac = static_cast<Real>(1.) / (kDeraRadius * kDeraRadius);
inline constexpr Real kXyp2TrnMin = static_cast<Real>(0.95);  // TRNMIN, w3pro2md.F90:125
static_assert(kCgdFac == 4.9029998779296875e+0f, "0.5*GRAV as REAL");
static_assert(kXyp2Dfac == 8.099999837529736623764620162546634674072265625e-11f,
              "1./(DERA*RADIUS)**2 as REAL");
static_assert(kXyp2TrnMin == 9.49999988079071044921875e-1f, "TRNMIN as REAL");

/// The per-grid fields of the dispersion correction (section 2.b and 3.c).
struct Grid2 {
  /// MPARS%PROPS%DTME and CLATMN (w3gdatmd.F90:825, pointers at :2564-2565):
  /// the swell age of namelist PRO2 (DTIME, default 0, w3gridmd.F90:2606-2659)
  /// and COS(LATMIN*DERA). DTME == 0 skips all the diffusion. Constant per grid
  /// without TDYN (w3pro2md.F90:835), which switch_lab_shrd does not set.
  Real dtme;
  Real clatmn;
  /// HPFAC, HQFAC (NY,NX), lower bounds (1,1) (w3gdatmd.F90:1755-1756):
  /// X(IY,IX) = x[(IX-1)*ny + IY-1], as Grid's DPDX.
  const Real* hpfac;
  const Real* hqfac;
  /// MAPXY(NSEA), lower bound 1 (w3adatmd.F90:1426), values 1-based IXY: the
  /// cells whose cross-term stencil is all sea, NMXY entries (W3MAP2,
  /// w3pro2md.F90:390-403).
  const int* mapxy;
  int nmxy;
};

/// What changes between calls, read from W3ADATMD.
struct Step2 {
  /// U10(NSEA), lower bound 1 (w3adatmd.F90:1015): U10(ISEA) = u10[ISEA-1].
  const Real* u10;
  /// WN(0:NK+1,0:NSEA) (w3adatmd.F90:1312): WN(IK,ISEA) = wn[ISEA*(nk+2) + IK],
  /// the stride of Step's CG.
  const Real* wn;
};

}  // namespace ww::prop
