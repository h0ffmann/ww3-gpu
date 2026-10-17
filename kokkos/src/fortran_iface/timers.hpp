// kokkos/src/fortran_iface/timers.hpp
// Wall time of the shims' phases, to tell what a run spends in copies, in kernels and outside
// Kokkos. Off unless WW_KOKKOS_TIMERS=1, a diagnostic like the *_COUNT flags (it ignores
// WW_KOKKOS_ALL); off, a Region is one branch and changes nothing. On, a Region fences the default
// execution space at its start and stop (unless told not to: a caller inside an OpenMP parallel
// region runs on Kokkos::Serial, which is synchronous), so a kernel's time is its work, not its
// launch, and it pushes a Kokkos profiling region of the same name for kokkos-tools. A fence
// changes when the host waits, never what a kernel computes. The totals print to stderr at exit,
// one line per region: "ww_kokkos: time <region>: <calls> calls, <seconds> s".
//
// Region names are "<part>.<phase>": .call is the whole entry point, .h2d the copies to the
// device, .kernel the launch, .d2h the copies back; scripts/ww3-gpu.sh --timers prints them.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <chrono>

namespace ww::timers {

/// WW_KOKKOS_TIMERS=1, read once.
bool enabled();

/// Adds seconds to the region's total, registering the printer at exit on first use.
void add(const char* name, double seconds);

class Region {
 public:
  explicit Region(const char* name, bool fence = true);
  Region(const Region&) = delete;
  Region& operator=(const Region&) = delete;
  ~Region() { stop(); }
  /// Ends the region early; the destructor then does nothing.
  void stop();

 private:
  const char* name_;
  bool fence_;
  bool on_;
  std::chrono::steady_clock::time_point t0_;
};

}  // namespace ww::timers
