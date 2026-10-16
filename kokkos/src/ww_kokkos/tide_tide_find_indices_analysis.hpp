// kokkos/src/ww_kokkos/tide_tide_find_indices_analysis.hpp
// TIDE_FIND_INDICES_ANALYSIS: selects the tidal constituents of an analysis from the 65 known ones,
// sets TIDE_MF, TIDECON_NAME and TIDE_FREQC, then calls TIDE_SET_INDICES.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:343-488 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/TIDE_FIND_INDICES_ANALYSIS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstring>

#include "tide_tide_find_indices_prediction.hpp"
#include "tide_tide_set_indices.hpp"

namespace ww::tide {

namespace find_indices_analysis_detail {

// TIDECON_NAME_ALL(65), :415-422, as 65 consecutive 5-byte records.
inline constexpr char name_all[65 * 5 + 1] =
    "Z0   SSA  MSM  MM   MSF  MF   ALP1 2Q1  SIG1 Q1   "
    "RHO1 O1   TAU1 BET1 NO1  CHI1 P1   K1   PHI1 THE1 "
    "J1   SO1  OO1  UPS1 OQ2  EPS2 2N2  MU2  N2   NU2  "
    "M2   MKS2 LDA2 L2   S2   K2   MSN2 ETA2 MO3  M3   "
    "SO3  MK3  SK3  MN4  M4   SN4  MS4  MK4  S4   SK4  "
    "2MK5 2SK5 2MN6 M6   2MS6 2MK6 2SM6 MSK6 3MK7 M8   "
    "N4   R2   S1   SA   T2   ";

// TIDE_FREQC_ALL(65), :424-436: default-REAL literals, each rounded once from decimal to binary32
// (the object's A.266 holds the same 260 bytes).
inline constexpr float freqc_all[65] = {
    0.0000000000f, 0.0002281591f, 0.0013097807f, 0.0015121520f, 0.0028219327f, 0.0030500918f,
    0.0343965698f, 0.0357063505f, 0.0359087218f, 0.0372185025f, 0.0374208738f,
    0.0387306544f, 0.0389588136f, 0.0400404351f, 0.0402685943f, 0.0404709655f,
    0.0415525871f, 0.0417807462f, 0.0420089053f, 0.0430905269f, 0.0432928982f,
    0.0446026789f, 0.0448308380f, 0.0463429900f, 0.0759749448f, 0.0761773160f,
    0.0774870967f, 0.0776894680f, 0.0789992487f, 0.0792016200f, 0.0805114007f,
    0.0807395598f, 0.0818211814f, 0.0820235526f, 0.0833333333f, 0.0835614924f,
    0.0848454853f, 0.0850736444f, 0.1192420551f, 0.1207671010f, 0.1220639878f,
    0.1222921469f, 0.1251140796f, 0.1595106494f, 0.1610228013f, 0.1623325820f,
    0.1638447340f, 0.1640728931f, 0.1666666667f, 0.1668948258f, 0.2028035476f,
    0.2084474129f, 0.2400220500f, 0.2415342020f, 0.2443561347f, 0.2445842938f,
    0.2471780673f, 0.2474062264f, 0.2833149482f, 0.3220456027f,
    0.157998497f,  0.083447407f,  0.041666667f,  0.000114080f,  0.083219259f};

// INDS(1:44) for 'FAST' (:442-444) and INDS(1:20) for 'VFAST' (:448).
inline constexpr int inds_fast[44] = {1,  2,  3,  4,  5,  6,  12, 17, 18, 26, 27, 28, 29, 30, 31,
                                      32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
                                      47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60};
inline constexpr int inds_vfast[20] = {1,  2,  3,  5,  6,  27, 28, 29, 30, 31,
                                       35, 36, 37, 44, 45, 47, 49, 54, 55, 60};

}  // namespace find_indices_analysis_detail

/// TIDE_FIND_INDICES_ANALYSIS, host (CHARACTER work, two WRITEs, an ALLOCATE).
/// list: LIST(70), CHARACTER(LEN=100), consecutive 100-byte records, list[0] is LIST(1).
/// Module state of W3TIDEMD:
///   tide_mf (TIDE_MF), out.
///   tide_freqc, tidecon_name: TIDE_FREQC and TIDECON_NAME (5-byte records), filled for 1..TIDE_MF.
///     The Fortran (re)allocates them with TIDE_MF entries at :478-479; here the shim gives
///     arrays of 65, enough on every defined path (TIDE_MF <= 44 or <= NTIDES <= 65, INDS(65)).
///   tidecon_name_allocated: ALLOCATED(TIDECON_NAME) on entry; :478 frees only TIDE_FREQC, so a
///     second call stops in the ALLOCATE at :479 ("already allocated"): returned as 1, after
///     TIDE_MF and the WRITEs, before the copy and TIDE_SET_INDICES; the shim raises the error.
///   iaproc_is_napout: IAPROC.EQ.NAPOUT (W3ODATMD).
///   ntidal_con ... n_indexjk: the arguments of tide_set_indices, called at :485.
/// inds: work array INDS(65), 0-based, values 1-based.
/// on_found(j, name, freq): the WRITE at :463-465, J, TRIM of the 5 bytes at name,
///   TIDE_FREQC_ALL(J), format (A,I4,2A,E12.2).
/// on_missing(tide_mf, rec): the WRITE at :468-469, TIDE_MF and TRIM of the 100 bytes at rec
///   (LIST(TIDE_MF)), format (A,I4,A,A). The shim does both WRITEs to NDSO.
/// Returns 0, or 1 for the ALLOCATE error at :479.
template <class OnFound, class OnMissing>
inline int tide_find_indices_analysis(const char* list, int& tide_mf, float* tide_freqc,
                                      char* tidecon_name, bool tidecon_name_allocated,
                                      bool iaproc_is_napout, int ntidal_con, int ntotal_con,
                                      const char* tidecon_allnames, const int* nj,
                                      const char* konco_con, int* tide_index2, int* tide_indexj,
                                      long n_indexj, int* tide_indexjk, long n_indexjk, int* inds,
                                      OnFound&& on_found, OnMissing&& on_missing) {
  namespace p = find_indices_prediction_detail;
  namespace d = find_indices_analysis_detail;
  constexpr long LL = 100;
  // :438.
  for (int i = 0; i < 65; ++i) inds[i] = 0;
  // :440-474. TRIM is dropped by gfortran: compare_string(100, LIST(1), 4, "FAST"), then
  // (100, LIST(1), 5, "VFAST").
  if (p::fstr_eq(LL, list, 4, "FAST")) {
    tide_mf = 44;
    for (int i = 0; i < 44; ++i) inds[i] = d::inds_fast[i];
  } else if (p::fstr_eq(LL, list, 5, "VFAST")) {
    tide_mf = 20;
    for (int i = 0; i < 20; ++i) inds[i] = d::inds_vfast[i];
  } else {
    tide_mf = 0;
    int ntides = 0;
    // :454 has no bound: with 70 non-blank names it reads LIST(71), as the Fortran does.
    while (p::flen_trim(LL, list + LL * tide_mf) != 0) {
      tide_mf = tide_mf + 1;
      int found = 0;
      for (int j = 1; j <= 65; ++j) {
        // :459, compare_string(5, TIDECON_NAME_ALL(J), 100, LIST(TIDE_MF)); no EXIT.
        if (p::fstr_eq(5, d::name_all + 5L * (j - 1), LL, list + LL * (tide_mf - 1))) {
          ntides = ntides + 1;
          // ntides > 65 writes past INDS(65) in the Fortran (stack) and past inds here.
          inds[ntides - 1] = j;
          found = 1;
          if (iaproc_is_napout) on_found(j, d::name_all + 5L * (j - 1), d::freqc_all[j - 1]);
        }
      }
      if (found == 0 && iaproc_is_napout) on_missing(tide_mf, list + LL * (tide_mf - 1));
    }
    tide_mf = ntides;
  }
  // :478-479.
  if (tidecon_name_allocated) return 1;
  // :481-484.
  for (int j = 1; j <= tide_mf; ++j) {
    std::memcpy(tidecon_name + 5L * (j - 1), d::name_all + 5L * (inds[j - 1] - 1), 5);
    tide_freqc[j - 1] = d::freqc_all[inds[j - 1] - 1];
  }
  // :485.
  tide_set_indices(tide_mf, ntidal_con, ntotal_con, tidecon_allnames, tidecon_name, nj, konco_con,
                   tide_index2, tide_indexj, n_indexj, tide_indexjk, n_indexjk);
  return 0;
}

}  // namespace ww::tide
