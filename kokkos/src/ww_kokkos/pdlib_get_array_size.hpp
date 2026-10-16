// kokkos/src/ww_kokkos/pdlib_get_array_size.hpp
// GET_ARRAY_SIZE: the number of columns of the PDLIB output exchange buffer, one per gridded
// output field (and per frequency, partition or extra field) whose output or coupling flag is on.
// Integer counting only, no REAL arithmetic. Phase 1, translated, not regtested.
// WW3 heritage: WW3/model/src/pdlib_field_vec.F90:53-393 at h0ffmann/WW3 8e433f99, identical at
// NOAA-EMC/WW3 03adda91; A. Roland (BGS IT&E GmbH), M. Dutour-Sikiric (IRB).
// Evidence: kokkos/port_report/GET_ARRAY_SIZE_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement WW3/regtests/ww3_tp2.17/input/switch_PDLIB compiles. The body has no
// preprocessor branch; W3_S (absent from that switch) only adds USE STRACE (:98-100), never
// called. Host only: a scalar count with no array work, called once per output exchange.
#pragma once

#include <cstdint>
#include <span>

namespace ww::pdlib {

namespace gas_detail {
// W3ODATMD PARAMETERs NOGRP, NGRPP, NOEXTR (w3odatmd.F90:330-335).
inline constexpr int kNogrp = 10;
inline constexpr int kNgrpp = 20;
inline constexpr int kNoextr = 2;
static_assert(sizeof(std::int32_t) == 4, "INTEGER and LOGICAL are 4 bytes in WW3's build");
}  // namespace gas_detail

/// @brief GET_ARRAY_SIZE (pdlib_field_vec.F90:53-393), TheSize as the return value.
/// @param flogrd FLOGRD(NOGRP,NGRPP), LOGICAL(4), column-major: flogrd[(j-1) + (k-1)*NOGRP].
/// @param flogr2 FLOGR2(NOGRP,NGRPP), same layout.
/// @param e3df   E3DF(3,5), INTEGER(4), column-major: E3DF(i,k) is e3df[(i-1) + (k-1)*3].
/// @param p2msf  P2MSF(3), INTEGER(4).
/// @param nk     NK (W3GDATMD), the number of frequencies.
/// @param noswll NOSWLL (W3ODATMD), the number of swell partitions.
/// @return IH, INTEGER(4). Like the Fortran, defined only while IH stays within INTEGER(4).
/// @details A flag is true when its 4-byte word is nonzero, as gfortran tests a LOGICAL; WW3
/// sets these words to 0 or 1. Every IF and DO is kept as written, in the Fortran's order.
[[nodiscard]] inline std::int32_t get_array_size(std::span<const std::int32_t, 200> flogrd,
                                                 std::span<const std::int32_t, 200> flogr2,
                                                 std::span<const std::int32_t, 15> e3df,
                                                 std::span<const std::int32_t, 3> p2msf,
                                                 std::int32_t nk, std::int32_t noswll) {
  using gas_detail::kNgrpp;
  using gas_detail::kNoextr;
  using gas_detail::kNogrp;
  bool flgrdall[kNogrp * kNgrpp];  // LOGICAL FLGRDALL(NOGRP,NGRPP), :107
  for (int j = 1; j <= kNogrp; ++j) {  // :112-116
    for (int k = 1; k <= kNgrpp; ++k) {
      const int at = (j - 1) + (k - 1) * kNogrp;
      flgrdall[at] = (flogrd[at] != 0) || (flogr2[at] != 0);
    }
  }
  const auto f = [&flgrdall](int j, int k) { return flgrdall[(j - 1) + (k - 1) * kNogrp]; };
  const auto e3 = [e3df](int i, int k) { return e3df[(i - 1) + (k - 1) * 3]; };

  std::int32_t ih = 0;  // :117
  // Group 2, one column each; (2,18) is not counted (:118-171).
  if (f(2, 1)) ih = ih + 1;
  if (f(2, 2)) ih = ih + 1;
  if (f(2, 3)) ih = ih + 1;
  if (f(2, 4)) ih = ih + 1;
  if (f(2, 5)) ih = ih + 1;
  if (f(2, 6)) ih = ih + 1;
  if (f(2, 7)) ih = ih + 1;
  if (f(2, 8)) ih = ih + 1;
  if (f(2, 9)) ih = ih + 1;
  if (f(2, 10)) ih = ih + 1;
  if (f(2, 11)) ih = ih + 1;
  if (f(2, 12)) ih = ih + 1;
  if (f(2, 13)) ih = ih + 1;
  if (f(2, 14)) ih = ih + 1;
  if (f(2, 15)) ih = ih + 1;
  if (f(2, 16)) ih = ih + 1;
  if (f(2, 17)) ih = ih + 1;
  if (f(2, 19)) ih = ih + 1;
  // Group 3, one column per frequency E3DF(2,k)..E3DF(3,k) (:172-196). ww3_grid clamps both to
  // 1..NK (w3gridmd.F90:2992-3005); at E3DF(3,k) = INT32_MAX the -O3 Fortran never leaves its
  // scalar loop and this one is undefined, so the two differ only where neither is defined.
  if (f(3, 1)) {
    for (std::int32_t ik = e3(2, 1); ik <= e3(3, 1); ++ik) ih = ih + 1;
  }
  if (f(3, 2)) {
    for (std::int32_t ik = e3(2, 2); ik <= e3(3, 2); ++ik) ih = ih + 1;
  }
  if (f(3, 3)) {
    for (std::int32_t ik = e3(2, 3); ik <= e3(3, 3); ++ik) ih = ih + 1;
  }
  if (f(3, 4)) {
    for (std::int32_t ik = e3(2, 4); ik <= e3(3, 4); ++ik) ih = ih + 1;
  }
  if (f(3, 5)) {
    for (std::int32_t ik = e3(2, 5); ik <= e3(3, 5); ++ik) ih = ih + 1;
  }
  // Group 4, partitions 0..NOSWLL (:197-241), then two single fields (:242-247).
  if (f(4, 1)) ih = ih + noswll + 1;
  if (f(4, 2)) ih = ih + noswll + 1;
  if (f(4, 3)) ih = ih + noswll + 1;
  if (f(4, 4)) ih = ih + noswll + 1;
  if (f(4, 5)) ih = ih + noswll + 1;
  if (f(4, 6)) ih = ih + noswll + 1;
  if (f(4, 7)) ih = ih + noswll + 1;
  if (f(4, 8)) ih = ih + noswll + 1;
  if (f(4, 9)) ih = ih + noswll + 1;
  if (f(4, 10)) ih = ih + noswll + 1;
  if (f(4, 11)) ih = ih + noswll + 1;
  if (f(4, 12)) ih = ih + noswll + 1;
  if (f(4, 13)) ih = ih + noswll + 1;
  if (f(4, 14)) ih = ih + noswll + 1;
  if (f(4, 15)) ih = ih + noswll + 1;
  if (f(4, 16)) ih = ih + 1;
  if (f(4, 17)) ih = ih + 1;
  // Group 5 (:248-281).
  if (f(5, 1)) {
    ih = ih + 1;
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(5, 2)) ih = ih + 1;
  if (f(5, 3)) ih = ih + 1;
  if (f(5, 4)) ih = ih + 1;
  if (f(5, 5)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(5, 6)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(5, 7)) ih = ih + 1;
  if (f(5, 8)) ih = ih + 1;
  if (f(5, 9)) ih = ih + 1;
  if (f(5, 10)) ih = ih + 1;
  // Group 6 (:282-334).
  if (f(6, 1)) {
    ih = ih + 1;
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(6, 2)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(6, 3)) ih = ih + 1;
  if (f(6, 4)) ih = ih + 1;
  if (f(6, 5)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(6, 6)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(6, 7)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(6, 8)) {
    for (std::int32_t ik = 1; ik <= 2 * nk; ++ik) ih = ih + 1;  // 2*NK in INTEGER(4), :310
  }
  if (f(6, 9)) {
    for (std::int32_t k = p2msf[1]; k <= p2msf[2]; ++k) ih = ih + 1;  // P2MSF(2)..P2MSF(3), :315
  }
  if (f(6, 10)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(6, 11)) ih = ih + 1;
  if (f(6, 12)) {
    for (std::int32_t ik = 1; ik <= 2 * nk; ++ik) ih = ih + 1;  // :327
  }
  if (f(6, 13)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  // Group 7 (:335-354).
  if (f(7, 1)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(7, 2)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(7, 3)) {
    ih = ih + 1;
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(7, 4)) ih = ih + 1;
  if (f(7, 5)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  // Group 8 (:355-371).
  if (f(8, 1)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(8, 2)) {
    ih = ih + 1;
    ih = ih + 1;
  }
  if (f(8, 3)) ih = ih + 1;
  if (f(8, 4)) ih = ih + 1;
  if (f(8, 5)) ih = ih + 1;
  // Group 9 (:372-386).
  if (f(9, 1)) ih = ih + 1;
  if (f(9, 2)) ih = ih + 1;
  if (f(9, 3)) ih = ih + 1;
  if (f(9, 4)) ih = ih + 1;
  if (f(9, 5)) ih = ih + 1;
  // Group 10, the user's extra fields (:387-391).
  for (int i = 1; i <= kNoextr; ++i) {
    if (f(10, i)) ih = ih + 1;
  }
  return ih;  // TheSize = IH, :392
}

}  // namespace ww::pdlib
