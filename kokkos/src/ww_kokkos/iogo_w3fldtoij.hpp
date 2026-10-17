// kokkos/src/ww_kokkos/iogo_w3fldtoij.hpp
// W3FLDTOIJ: the group and field indices (I, J) of the output tables for one output field name
// (for example 'HS' is I = 2, J = 1), or -1, -1 with a warning when the name is not recognised.
// Host only. Phase 1, translated, not wired into WW3, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3iogomd.F90:764-1188 at NOAA-EMC/WW3 03adda91 (identical in
// the WW3/ fork); author C. Bunney. Evidence: kokkos/port_report/W3FLDTOIJ_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported as switches/switch_lab_shrd compiles it: the 97 CASE values of the -O3 object's
// jump table (_gfortran_select_string, 98 entries with the default). Not compiled, not ported:
// 'D50' (W3_BT4, :837-841), 'IC1', 'IC5' (W3_IS2, :842-849), 'AHS' and 'ACHA' (W3_OASACM,
// :852-856, :1005-1009), 'OHS', 'THM' and 'OCHA' (W3_OASOCM, :857-861, :919-923, :1010-1014),
// 'DRY' (W3_COU, :1171-1173).
//
// Fortran semantics kept:
//  - The selector is TRIM(FLD(1:6)): only the first 6 bytes count, so 'HSXYZW' is not 'HS' and
//    'SDMHC1' is not 'SDMHC'; the object calls _gfortran_string_trim on 6 bytes whatever LEN(FLD)
//    is (both callers pass 10 or 100).
//  - SELECT CASE compares as Fortran character equality: the shorter string is blank padded
//    (libgfortran compare_string, used by select_string). The object binary-searches a sorted
//    table; the 97 keys are distinct, so a linear scan in source order finds the same entry.
//  - 'U1' and 'U2' both give I = 10, J = 1 (:1164-1169), as in the Fortran.
//  - 'UNSET' (:1174) is a CASE with no statement: I = J = -1 and no warning.
//  - 'USF' and 'USP' read US3DF(1) and USSPF(1) of the current grid (W3GDATMD pointers, set by
//    W3SETG, w3gdatmd.F90:2311-2312) only on their branch; a value < 1 leaves I = J = -1 and
//    writes FORMAT 1008 when IAPROC = NAPOUT.
//  - The default writes FORMAT 1004 with TRIM(FLD), the whole dummy, not its first 6 bytes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>

namespace ww::iogo {

/// The value of ww::iogr::kExitMissingHook: the write_formatted hook is empty on a path that
/// writes.
inline constexpr int kFldtoijExitMissingHook = -101;

struct FldtoijHooks {
  /// One formatted WRITE to unit, the bytes gfortran writes (every record ended by '\n').
  std::function<void(int unit, const std::string& bytes)> write_formatted;
};

namespace fldtoij_detail {

// Fortran character equality: memcmp on the common length, then the rest of the longer string
// against blanks (libgfortran runtime compare_string).
inline bool fstr_eq(const char* a, std::size_t la, const char* b, std::size_t lb) {
  const std::size_t n = la < lb ? la : lb;
  if (std::memcmp(a, b, n) != 0) return false;
  const char* rest = la > lb ? a : b;
  const std::size_t lr = la > lb ? la : lb;
  for (std::size_t k = n; k < lr; ++k)
    if (rest[k] != ' ') return false;
  return true;
}

enum class Kind : std::uint8_t { kSet, kUsf, kUsp, kUnset };

struct Case {
  const char* name;
  std::int8_t i;
  std::int8_t j;
  Kind kind;
};

// :810-1174, in source order, the cases switch_lab_shrd compiles.
inline constexpr Case kCases[] = {
    // Group 1
    {"DPT", 1, 1, Kind::kSet}, {"CUR", 1, 2, Kind::kSet}, {"WND", 1, 3, Kind::kSet},
    {"AST", 1, 4, Kind::kSet}, {"WLV", 1, 5, Kind::kSet}, {"ICE", 1, 6, Kind::kSet},
    {"IBG", 1, 7, Kind::kSet}, {"TAU", 1, 8, Kind::kSet}, {"RHO", 1, 9, Kind::kSet},
    // Group 2
    {"HS", 2, 1, Kind::kSet}, {"LM", 2, 2, Kind::kSet}, {"T02", 2, 3, Kind::kSet},
    {"T0M1", 2, 4, Kind::kSet}, {"T01", 2, 5, Kind::kSet}, {"FP", 2, 6, Kind::kSet},
    {"DIR", 2, 7, Kind::kSet}, {"SPR", 2, 8, Kind::kSet}, {"DP", 2, 9, Kind::kSet},
    {"HIG", 2, 10, Kind::kSet}, {"MXE", 2, 11, Kind::kSet}, {"MXES", 2, 12, Kind::kSet},
    {"MXH", 2, 13, Kind::kSet}, {"MXHC", 2, 14, Kind::kSet}, {"SDMH", 2, 15, Kind::kSet},
    {"SDMHC", 2, 16, Kind::kSet}, {"WBT", 2, 17, Kind::kSet}, {"TP", 2, 18, Kind::kSet},
    {"WNM", 2, 19, Kind::kSet},
    // Group 3
    {"EF", 3, 1, Kind::kSet}, {"TH1M", 3, 2, Kind::kSet}, {"STH1M", 3, 3, Kind::kSet},
    {"TH2M", 3, 4, Kind::kSet}, {"STH2M", 3, 5, Kind::kSet}, {"WN", 3, 6, Kind::kSet},
    // Group 4
    {"PHS", 4, 1, Kind::kSet}, {"PTP", 4, 2, Kind::kSet}, {"PLP", 4, 3, Kind::kSet},
    {"PDIR", 4, 4, Kind::kSet}, {"PSPR", 4, 5, Kind::kSet}, {"PWS", 4, 6, Kind::kSet},
    {"PDP", 4, 7, Kind::kSet}, {"PQP", 4, 8, Kind::kSet}, {"PPE", 4, 9, Kind::kSet},
    {"PGW", 4, 10, Kind::kSet}, {"PSW", 4, 11, Kind::kSet}, {"PTM10", 4, 12, Kind::kSet},
    {"PT01", 4, 13, Kind::kSet}, {"PT02", 4, 14, Kind::kSet}, {"PEP", 4, 15, Kind::kSet},
    {"TWS", 4, 16, Kind::kSet}, {"PNR", 4, 17, Kind::kSet},
    // Group 5
    {"UST", 5, 1, Kind::kSet}, {"CHA", 5, 2, Kind::kSet}, {"CGE", 5, 3, Kind::kSet},
    {"FAW", 5, 4, Kind::kSet}, {"TAW", 5, 5, Kind::kSet}, {"TWA", 5, 6, Kind::kSet},
    {"WCC", 5, 7, Kind::kSet}, {"WCF", 5, 8, Kind::kSet}, {"WCH", 5, 9, Kind::kSet},
    {"WCM", 5, 10, Kind::kSet}, {"FWS", 5, 11, Kind::kSet},
    // Group 6
    {"SXY", 6, 1, Kind::kSet}, {"TWO", 6, 2, Kind::kSet}, {"BHD", 6, 3, Kind::kSet},
    {"FOC", 6, 4, Kind::kSet}, {"TUS", 6, 5, Kind::kSet}, {"USS", 6, 6, Kind::kSet},
    {"P2S", 6, 7, Kind::kSet}, {"USF", 6, 8, Kind::kUsf}, {"P2L", 6, 9, Kind::kSet},
    {"TWI", 6, 10, Kind::kSet}, {"FIC", 6, 11, Kind::kSet}, {"USP", 6, 12, Kind::kUsp},
    {"TOC", 6, 13, Kind::kSet},
    // Group 7
    {"ABR", 7, 1, Kind::kSet}, {"UBR", 7, 2, Kind::kSet}, {"BED", 7, 3, Kind::kSet},
    {"FBB", 7, 4, Kind::kSet}, {"TBB", 7, 5, Kind::kSet},
    // Group 8
    {"MSS", 8, 1, Kind::kSet}, {"MSC", 8, 2, Kind::kSet}, {"MSD", 8, 3, Kind::kSet},
    {"MCD", 8, 4, Kind::kSet}, {"QP", 8, 5, Kind::kSet}, {"QKK", 8, 6, Kind::kSet},
    {"SKW", 8, 7, Kind::kSet}, {"EMB", 8, 8, Kind::kSet}, {"EMC", 8, 9, Kind::kSet},
    // Group 9
    {"DTD", 9, 1, Kind::kSet}, {"FC", 9, 2, Kind::kSet}, {"CFX", 9, 3, Kind::kSet},
    {"CFD", 9, 4, Kind::kSet}, {"CFK", 9, 5, Kind::kSet},
    // Group 10: U1 and U2 are both (10, 1) in the Fortran.
    {"U1", 10, 1, Kind::kSet}, {"U2", 10, 1, Kind::kSet},
    // :1174, no statement
    {"UNSET", -1, -1, Kind::kUnset},
};
static_assert(sizeof(kCases) / sizeof(kCases[0]) == 97, "97 CASE values, as the -O3 jump table");

}  // namespace fldtoij_detail

/// The bytes gfortran writes for FORMAT 1004 (a = TRIM(FLD)) or 1008 (a, b = 'USF', 'US3D' or
/// 'USP', 'USSP') of W3FLDTOIJ (:1181-1186). Checked against gfortran 16.2 output of the same
/// FORMATs. Empty for another fmt.
inline std::string fldtoij_message(int fmt, std::string_view a, std::string_view b) {
  std::string s = "\n *** WAVEWATCH III WARNING  : \n";
  if (fmt == 1004) {
    s += "     REQUESTED OUTPUT FIELD ";
    s += a;
    s += " WAS NOT RECOGNIZED.\n\n";
  } else if (fmt == 1008) {
    s += "     PARAMETER ";
    s += a;
    s += " not allowed: need to set parameter ";
    s += b;
    s += " in OUTS namelist (in ww3_grid.inp)\n";
  } else {
    return std::string();
  }
  return s;
}

/// @brief W3FLDTOIJ(FLD, I, J, IAPROC, NAPOUT, NDSEN), w3iogomd.F90:764-1188. Host only.
/// @param fld, fld_len  FLD, CHARACTER(LEN=fld_len), not NUL terminated.
/// @param us3df  US3DF(1) of the current grid; read only for 'USF'.
/// @param usspf  USSPF(1) of the current grid; read only for 'USP'.
/// @param h      write_formatted, called with unit ndsen for FORMATs 1004 and 1008.
/// @pre fld_len >= 6: the object reads FLD(1:6) for any length (callers pass 100 and 10).
/// @return 0, or kFldtoijExitMissingHook when a warning is due and write_formatted is empty
///         (I and J are already set then). The W3FLGRDFLAG hook (iogo_w3flgrdflag.hpp) binds
///         iaproc, napout, ndsen, us3df, usspf and h, and passes fld_len = 100.
inline int w3fldtoij(const char* fld, std::size_t fld_len, int& i, int& j, int iaproc,
                     int napout, int ndsen, const std::int32_t* us3df,
                     const std::int32_t* usspf, const FldtoijHooks& h) {
  using namespace fldtoij_detail;
  // :803-804
  i = -1;
  j = -1;

  // :806, TRIM(FLD(1:6))
  std::size_t nsel = 6;
  while (nsel > 0 && fld[nsel - 1] == ' ') --nsel;

  const Case* hit = nullptr;
  for (const Case& c : kCases) {
    if (fstr_eq(fld, nsel, c.name, std::strlen(c.name))) {
      hit = &c;
      break;
    }
  }

  auto warn = [&](int fmt, std::string_view a, std::string_view b) {
    if (iaproc != napout) return 0;
    if (!h.write_formatted) return kFldtoijExitMissingHook;
    h.write_formatted(ndsen, fldtoij_message(fmt, a, b));
    return 0;
  };

  if (hit == nullptr) {
    // :1175-1178, CASE DEFAULT
    i = -1;
    j = -1;
    std::size_t n = fld_len;
    while (n > 0 && fld[n - 1] == ' ') --n;  // TRIM(FLD), _gfortran_string_len_trim
    return warn(1004, std::string_view(fld, n), std::string_view());
  }
  switch (hit->kind) {
    case Kind::kSet:
      i = hit->i;
      j = hit->j;
      return 0;
    case Kind::kUsf:  // :1069-1075
      if (*us3df >= 1) {
        i = hit->i;
        j = hit->j;
        return 0;
      }
      return warn(1008, "USF", "US3D");
    case Kind::kUsp:  // :1085-1091
      if (*usspf >= 1) {
        i = hit->i;
        j = hit->j;
        return 0;
      }
      return warn(1008, "USP", "USSP");
    case Kind::kUnset:  // :1174
      return 0;
  }
  return 0;
}

}  // namespace ww::iogo
