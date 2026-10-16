// kokkos/src/ww_kokkos/iogo_w3flgrdflag.hpp
// W3FLGRDFLAG: set the output field flags FLG2D(NOGRP,NGRPP) and group flags FLG1D(NOGRP)
// from the blank-separated list of field names FLDOUT.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3iogomd.F90:589-749 at NOAA-EMC/WW3 03adda91;
// author F. Ardhuin (name matching factored out by C. Bunney).
// Evidence: kokkos/port_report/W3FLGRDFLAG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

namespace ww::iogo {

namespace flgrdflag_detail {
// w3odatmd.F90:330-331, PARAMETERs.
inline constexpr int kNogrp = 10;
inline constexpr int kNgrpp = 20;
// OUT_NAMES(100), CHARACTER(LEN=100) (:672); FLDOUT is CHARACTER(1024) (:669).
inline constexpr int kNames = 100;
inline constexpr int kNameLen = 100;
inline constexpr int kFldoutLen = 1024;

// LEN_TRIM(s) /= 0 as _gfortran_string_len_trim tests it: any byte other than a blank.
inline bool nonblank(const char* s) {
  for (int k = 0; k < kNameLen; ++k)
    if (s[k] != ' ') return true;
  return false;
}
}  // namespace flgrdflag_detail

/// W3FLGRDFLAG, :589-749. Host only: it calls three Fortran routines of other units, which the
/// shim passes in, as the object calls them (none is inlined):
///   strsplit(const char* fldout, char* tab)  STRSPLIT (w3servmd.F90:1882) on FLDOUT(1024) and
///                                            OUT_NAMES, kNames x kNameLen bytes, blank-filled;
///   str_to_upper(char* s)                    STR_TO_UPPER (w3servmd.F90:1942) on one name of
///                                            kNameLen bytes, in place;
///   fldtoij(const char* fld, int& i, int& j) W3FLDTOIJ (:764) on TESTSTR(100), with IAPROC,
///                                            NAPOUT, NDSEN bound by the shim; it does its own
///                                            WRITEs (formats 1004, 1008) and its #ifdef cases.
/// flg1d: FLG1D(NOGRP), flg2d: FLG2D(NOGRP,NGRPP) column-major, LOGICAL(4), 0-based, written as
/// 0 or 1. NDSO, NDSS, NDSEN are not used here (NDSS is unused in the Fortran too).
/// Returns the number of lines the shim writes to NDSO (:717-735): 0 when IAPROC /= NAPOUT;
/// otherwise one per true FLG2D(IFI,IFJ), IFI outer and IFJ inner, IDOUT(IFI,IFJ) with format
/// 1945 for the first and 1946 for the rest, or 1 for format 1945 'no fields defined'.
template <class Strsplit, class StrToUpper, class Fldtoij>
inline int w3flgrdflag(const char* fldout, std::int32_t* flg1d, std::int32_t* flg2d,
                       int iaproc, int napout, int& ierr, Strsplit&& strsplit,
                       StrToUpper&& str_to_upper, Fldtoij&& fldtoij) {
  using namespace flgrdflag_detail;
  char out_names[kNames * kNameLen];
  char teststr[kNameLen];

  // :692-694
  ierr = 0;
  for (int k = 0; k < kNogrp * kNgrpp; ++k) flg2d[k] = 0;
  for (int k = 0; k < kNogrp; ++k) flg1d[k] = 0;

  // :698-699
  for (int k = 0; k < kNames * kNameLen; ++k) out_names[k] = ' ';
  strsplit(fldout, out_names);

  // :700-715. ⚠ The Fortran has no bound on IOUT: with 100 names it reads OUT_NAMES(101), past
  // the array (in the -O3 object that is TESTSTR, then the stack canary). Stopping at 100 is
  // the only defined reading.
  int iout = 0;
  while (iout < kNames && nonblank(out_names + iout * kNameLen)) {
    char* name = out_names + iout * kNameLen;
    str_to_upper(name);
    for (int k = 0; k < kNameLen; ++k) teststr[k] = name[k];
    int ifi = 0;
    int ifj = 0;
    fldtoij(static_cast<const char*>(teststr), ifi, ifj);
    if (ifi != -1) flg2d[(ifi - 1) + (ifj - 1) * kNogrp] = 1;
    ++iout;
  }

  // :717-735; ANY(FLG2D(IFI,:)) stops at the first non-zero word, as the object's loop does.
  bool flt = true;
  int lines = 0;
  for (int ifi = 1; ifi <= kNogrp; ++ifi) {
    if (iaproc == napout) {
      for (int ifj = 1; ifj <= kNgrpp; ++ifj) {
        if (flg2d[(ifi - 1) + (ifj - 1) * kNogrp] != 0) {
          flt = false;
          ++lines;
        }
      }
    }
    for (int ifj = 1; ifj <= kNgrpp; ++ifj) {
      if (flg2d[(ifi - 1) + (ifj - 1) * kNogrp] != 0) {
        flg1d[ifi - 1] = 1;
        break;
      }
    }
  }
  if (iaproc == napout && flt) lines = 1;
  return lines;
}

}  // namespace ww::iogo
