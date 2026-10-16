// kokkos/src/ww_kokkos/tide_tide_find_indices_prediction.hpp
// TIDE_FIND_INDICES_PREDICTION: indices into TIDECON_NAME of the constituents named for a tidal
// prediction. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3tidemd.F90:244-339 at NOAA-EMC/WW3 03adda91;
// authors M. Foreman (tidal package), adapted to WW3 by F. Ardhuin.
// Evidence: kokkos/port_report/TIDE_FIND_INDICES_PREDICTION_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstring>

namespace ww::tide {

namespace find_indices_prediction_detail {

// _gfortran_compare_string(la, a, lb, b) == 0: the shorter operand is blank padded.
inline bool fstr_eq(long la, const char* a, long lb, const char* b) {
  const long n = la < lb ? la : lb;
  if (std::memcmp(a, b, static_cast<std::size_t>(n)) != 0) return false;
  const char* rest = la > lb ? a : b;
  const long lr = la > lb ? la : lb;
  for (long i = n; i < lr; ++i)
    if (rest[i] != ' ') return false;
  return true;
}

// _gfortran_string_len_trim: only ' ' counts as trailing blank.
inline long flen_trim(long len, const char* s) {
  long i = len;
  while (i > 0 && s[i - 1] == ' ') --i;
  return i;
}

}  // namespace find_indices_prediction_detail

/// TIDE_FIND_INDICES_PREDICTION, host (CHARACTER work and two WRITEs).
/// list: LIST(70), CHARACTER(LEN=100), consecutive 100-byte records, list[0] is LIST(1).
/// inds: INDS(70), 0-based; every value written is a 1-based index into TIDECON_NAME; entries not
///   written are left as they were (INTENT(OUT), undefined in the Fortran).
/// tide_prmf: TIDE_PRMF, out.
/// Module state: tide_mf (TIDE_MF), tidecon_name (TIDE_MF consecutive 5-byte records, no
///   terminator, element 1 first), iaproc_is_napout (IAPROC.EQ.NAPOUT from W3ODATMD).
/// on_found(prmf, j): the WRITE at :331-332 (name LIST(prmf), TIDE_FREQC(j), format (A,A,E12.2));
/// on_missing(prmf): the WRITE at :335-336. The shim does both WRITEs to NDSO.
/// The scan has no bound (:324): with 70 non-blank names it reads LIST(71) and, if that is non-blank,
/// writes INDS(71), past both caller arrays, in the Fortran and here alike.
template <class OnFound, class OnMissing>
inline void tide_find_indices_prediction(const char* list, int* inds, int& tide_prmf, int tide_mf,
                                         const char* tidecon_name, bool iaproc_is_napout,
                                         OnFound&& on_found, OnMissing&& on_missing) {
  namespace d = find_indices_prediction_detail;
  constexpr long LL = 100;
  // :315-322. TRIM is dropped by gfortran: the object compares 100 bytes against "VFAST" and "FAST".
  tide_prmf = 0;
  if (d::fstr_eq(LL, list, 5, "VFAST") || d::fstr_eq(LL, list, 4, "FAST")) {
    for (int j = 1; j <= tide_mf; ++j) inds[j - 1] = j;
    tide_prmf = tide_mf;
    return;
  }
  // :324-337.
  while (d::flen_trim(LL, list + LL * tide_prmf) != 0) {
    tide_prmf = tide_prmf + 1;
    int found = 0;
    for (int j = 1; j <= tide_mf; ++j) {
      // :328, compare_string(5, TIDECON_NAME(J), 100, LIST(TIDE_PRMF)); no EXIT, the last match wins.
      if (d::fstr_eq(5, tidecon_name + 5L * (j - 1), LL, list + LL * (tide_prmf - 1))) {
        inds[tide_prmf - 1] = j;
        found = 1;
        if (iaproc_is_napout) on_found(tide_prmf, j);
      }
    }
    if (found == 0 && iaproc_is_napout) on_missing(tide_prmf);
  }
}

}  // namespace ww::tide
