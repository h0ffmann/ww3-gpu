// kokkos/src/fortran_iface/timers.cpp
// The registry behind timers.hpp. Calls arrive serialised by the Fortran's !$OMP CRITICAL, or
// from one thread, but a mutex keeps the totals right whatever the caller does.
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/timers.hpp"

#include <Kokkos_Core.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

namespace ww::timers {
namespace {

struct Total {
  long long calls = 0;
  double seconds = 0.0;
};

std::mutex& mu() {
  static auto* m = new std::mutex();
  return *m;
}

// Never destroyed, as the shims' maps: the printer runs from atexit.
std::map<std::string, Total>& totals() {
  static auto* m = new std::map<std::string, Total>();
  return *m;
}

void print() {
  std::lock_guard<std::mutex> lock(mu());
  for (const auto& [name, t] : totals())
    std::fprintf(stderr, "ww_kokkos: time %s: %lld calls, %.6f s\n", name.c_str(), t.calls,
                 t.seconds);
}

}  // namespace

bool enabled() {
  static const bool on = [] {
    const char* v = std::getenv("WW_KOKKOS_TIMERS");
    return v != nullptr && std::strcmp(v, "1") == 0;
  }();
  return on;
}

void add(const char* name, double seconds) {
  std::lock_guard<std::mutex> lock(mu());
  static bool registered = false;
  if (!registered) {
    // Registered after ww_kokkos_init's finalize handler, so it prints first.
    std::atexit(print);
    registered = true;
  }
  Total& t = totals()[name];
  t.calls += 1;
  t.seconds += seconds;
}

Region::Region(const char* name, bool fence) : name_(name), fence_(fence), on_(enabled()) {
  if (!on_) return;
  if (fence_) Kokkos::fence("ww_kokkos.timers.start");
  Kokkos::Profiling::pushRegion(name_);
  t0_ = std::chrono::steady_clock::now();
}

void Region::stop() {
  if (!on_) return;
  on_ = false;
  if (fence_) Kokkos::fence("ww_kokkos.timers.stop");
  const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count();
  Kokkos::Profiling::popRegion();
  add(name_, s);
}

}  // namespace ww::timers
