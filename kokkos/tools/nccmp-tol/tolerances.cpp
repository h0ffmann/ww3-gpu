// kokkos/tools/nccmp-tol/tolerances.cpp
// Parser for the "name abs rel" tolerance file.
//
// SPDX-License-Identifier: MIT
#include "nccmp-tol/tolerances.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace ww::nccmp {

std::vector<Tolerance> parse_tolerances(std::istream& in) {
  std::vector<Tolerance> out;
  std::string line;
  std::size_t lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    if (const auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
    std::istringstream row(line);
    Tolerance t;
    if (!(row >> t.name)) continue;  // blank or comment-only
    std::string trailing;
    if (!(row >> t.abs >> t.rel) || (row >> trailing)) {
      throw std::runtime_error("tolerances: line " + std::to_string(lineno) +
                               ": expected \"name abs rel\", got \"" + line + "\"");
    }
    if (t.abs < 0.0 || t.rel < 0.0) {
      throw std::runtime_error("tolerances: line " + std::to_string(lineno) +
                               ": tolerances must be non-negative");
    }
    out.push_back(t);
  }
  return out;
}

}  // namespace ww::nccmp
