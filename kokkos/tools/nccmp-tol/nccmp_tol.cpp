// kokkos/tools/nccmp-tol/nccmp_tol.cpp
// CLI: nccmp-tol REF TEST [TOLERANCES]
//
// Prints one table row per numeric variable present in both files and a verdict
// line. Exit status: 0 iff every judged variable passes, 1 if any fails, 2 on an
// I/O or usage error (unreadable file, bad tolerance row, shape mismatch).
//
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "nccmp-tol/compare.hpp"
#include "nccmp-tol/tolerances.hpp"

namespace {

constexpr int kExitPass = 0;
constexpr int kExitFail = 1;
constexpr int kExitError = 2;

int usage() {
  std::cerr << "usage: nccmp-tol REF TEST [TOLERANCES]\n"
               "  default TOLERANCES: " NCCMP_DEFAULT_TOLERANCES "\n";
  return kExitError;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 && argc != 4) return usage();
  const std::string ref = argv[1];
  const std::string test = argv[2];
  const std::string tol_path = argc == 4 ? argv[3] : NCCMP_DEFAULT_TOLERANCES;

  try {
    std::ifstream tol_in(tol_path);
    if (!tol_in) {
      std::cerr << "nccmp-tol: cannot open tolerances file " << tol_path << '\n';
      return kExitError;
    }
    const std::vector<ww::nccmp::Tolerance> tolerances = ww::nccmp::parse_tolerances(tol_in);
    const std::vector<ww::nccmp::Stats> stats = ww::nccmp::compare_files(ref, test, tolerances);

    std::cout << format_table(stats);
    std::size_t judged = 0;
    for (const auto& s : stats) {
      judged += s.judged ? 1U : 0U;
      // Not a failure by itself, but a test value missing where the reference
      // has one is exactly what a broken kernel looks like: say so.
      if (s.dropped > 0) {
        std::cerr << "nccmp-tol: note: '" << s.name << "': " << s.dropped
                  << " cell(s) NaN/fill in TEST where REF is valid\n";
      }
    }
    for (const auto& t : tolerances) {
      bool seen = false;
      for (const auto& s : stats) seen = seen || s.name == t.name;
      if (!seen) std::cerr << "nccmp-tol: note: '" << t.name << "' is listed but not in both files\n";
    }
    const bool ok = ww::nccmp::all_pass(stats);
    std::cout << "nccmp-tol: " << (ok ? "PASS" : "FAIL") << " (" << judged << " judged, "
              << stats.size() - judged << " unlisted)\n";
    return ok ? kExitPass : kExitFail;
  } catch (const std::exception& e) {
    std::cerr << "nccmp-tol: " << e.what() << '\n';
    return kExitError;
  }
}
