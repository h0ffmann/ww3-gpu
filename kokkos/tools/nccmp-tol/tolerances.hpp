// kokkos/tools/nccmp-tol/tolerances.hpp
// The tolerance file: one "name abs rel" row per judged variable. A value passes
// when |d| <= abs OR |d| / max(|ref|, eps) <= rel, so a row states both the noise
// floor of the field and the relative agreement expected above it.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <istream>
#include <string>
#include <vector>

namespace ww::nccmp {

/// One row of the tolerance file.
struct Tolerance {
  std::string name;
  double abs;
  double rel;
};

/// Parse "name abs rel" rows. '#' starts a comment (whole line or trailing);
/// blank lines are skipped. Throws std::runtime_error on a malformed row.
std::vector<Tolerance> parse_tolerances(std::istream& in);

}  // namespace ww::nccmp
