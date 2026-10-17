// kokkos/src/fortran_iface/part_flags.cpp
// The one place a WW_KOKKOS_<PART> run-time flag is resolved: every shim's
// ww_*_enabled() and W3KOKKOS_SETUP in w3kokkosmd.F90 ask ww_kokkos_flag(), so the
// Fortran LOGICALs and the shims cannot disagree about which parts run on Kokkos.
//
// WW_KOKKOS_<PART> set: "1" is on, any other value is off (the rule of every flag
// since 2026-10-14). WW_KOKKOS_<PART> unset: the master WW_KOKKOS_ALL decides, "1"
// on, anything else or unset off. So an unset master keeps today's defaults (all
// off), WW_KOKKOS_ALL=1 turns every part on, WW_KOKKOS_ALL=0 every part off, and a
// part set explicitly wins over the master. The *_COUNT diagnostics are not parts:
// they are read by each shim directly and ignore the master.
//
// The parts of kOptInOnly ignore the master too: the owner, 2026-10-16, asked that the PR1, PR2
// and unstructured explicit propagation (W3XYP1, W3KTP1, W3XYP2, W3KTP2, W3XYPUG) stay on the
// Fortran unless their own flag is "1", so WW_KOKKOS_ALL=1 cannot route them to Kokkos.
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/ww_kokkos_c.hpp"

#include <cstdlib>
#include <cstring>

namespace {
constexpr const char* kOptInOnly[] = {"WW_KOKKOS_XYP1", "WW_KOKKOS_KTP1", "WW_KOKKOS_XYP2",
                                      "WW_KOKKOS_KTP2", "WW_KOKKOS_XYPUG"};
}  // namespace

extern "C" int ww_kokkos_flag(const char* name) {
  if (name == nullptr) return 0;
  if (const char* v = std::getenv(name)) return std::strcmp(v, "1") == 0 ? 1 : 0;
  for (const char* p : kOptInOnly)
    if (std::strcmp(name, p) == 0) return 0;
  const char* all = std::getenv("WW_KOKKOS_ALL");
  return (all != nullptr && std::strcmp(all, "1") == 0) ? 2 : 0;
}
