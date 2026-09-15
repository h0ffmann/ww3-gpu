// kokkos/tools/nccmp-tol/compare.hpp
// Per-field comparison of two NetCDF files. Every numeric variable present in
// both files is read as double and reduced to n / max|d| / rms / max relative
// error, with _FillValue cells and NaNs excluded on either side. A variable is
// *judged* only when the tolerance list names it; the rest are reported so the
// reader can see what else moved, but they never affect the verdict.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "nccmp-tol/tolerances.hpp"

namespace ww::nccmp {

/// The reduction of one variable over its compared cells.
struct Stats {
  std::string name;
  std::size_t n;        ///< cells compared (fill and NaN on either side excluded)
  std::size_t dropped;  ///< cells where ref is valid but test is NaN or fill
  double max_abs;       ///< max |test - ref|
  double rms;           ///< sqrt(mean (test - ref)^2)
  double max_rel;       ///< max |test - ref| / max(|ref|, eps), eps = DBL_EPSILON
  bool judged;          ///< listed in the tolerance file
  bool pass;            ///< judged, n > 0 and every cell within abs OR rel
};

/// Compare `test` against `ref`, variable by variable, in `ref`'s variable order.
/// Throws std::runtime_error on any NetCDF error or on a shape mismatch.
std::vector<Stats> compare_files(const std::string& ref, const std::string& test,
                                 const std::vector<Tolerance>& tolerances);

/// True iff every judged variable passes (unjudged ones do not count).
bool all_pass(const std::vector<Stats>& stats);

/// Fixed-width text table, one row per variable, header included.
std::string format_table(const std::vector<Stats>& stats);

}  // namespace ww::nccmp
