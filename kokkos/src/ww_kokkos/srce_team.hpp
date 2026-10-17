// kokkos/src/ww_kokkos/srce_team.hpp
// The team size of the batched W3SRCE launch (srce_shim.cpp, run_batch), and the
// diagnostic that sets it: WW_KOKKOS_SRCE_TEAM. Unset, empty or AUTO, the launch
// is TeamPolicy(n, Kokkos::AUTO), as before; a positive integer N asks for teams
// of N threads, capped at the execution space's concurrency (1 on Kokkos::Serial,
// the nested path, so the flag never breaks it). It exists so the team-parallel
// loops of w3srce_team run with more than one thread on the OpenMP backend, where
// AUTO is one thread per core: every loop split over a team is element-wise and
// every reduction runs on one thread, so the bits must not depend on N
// (srce_kernel.hpp). A value that is not a positive integer is refused.
//
// It also holds the two executors the team-parallel bodies are written against,
// so one body serves a team (TeamExec) and a single thread (SerialExec, the
// per-point shims of st4_shim.cpp and pdlib): for_(n, f) runs f(i) for every i
// in [0, n), each i on one thread; one(f) runs f once, on one thread; sync() is
// the team barrier. A body may split over for_ only a loop whose iterations are
// independent (each writes its own elements, reads nothing another writes);
// every reduction and running value runs inside one(), in the Fortran's order,
// and a value one() computes reaches the other threads only through memory the
// team shares, after a sync().
//
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include <Kokkos_Core.hpp>

namespace ww {

/// One thread does everything, in order: the per-point callers.
struct SerialExec {
  template <class F>
  KOKKOS_INLINE_FUNCTION void for_(int n, const F& f) const {
    for (int i = 0; i < n; ++i) f(i);
  }
  template <class F>
  KOKKOS_INLINE_FUNCTION void one(const F& f) const {
    f();
  }
  KOKKOS_INLINE_FUNCTION void sync() const {}
};

/// The whole team: for_ is a TeamThreadRange, one a single(PerTeam), sync a barrier.
template <class Member>
struct TeamExec {
  const Member& team;
  template <class F>
  KOKKOS_INLINE_FUNCTION void for_(int n, const F& f) const {
    Kokkos::parallel_for(Kokkos::TeamThreadRange(team, n), f);
  }
  template <class F>
  KOKKOS_INLINE_FUNCTION void one(const F& f) const {
    Kokkos::single(Kokkos::PerTeam(team), f);
  }
  KOKKOS_INLINE_FUNCTION void sync() const { team.team_barrier(); }
};

}  // namespace ww

namespace ww::srce {

/// WW_KOKKOS_SRCE_TEAM, read once: 0 for AUTO, else the requested team size.
inline int srce_team_request() {
  static const int n = [] {
    const char* v = std::getenv("WW_KOKKOS_SRCE_TEAM");
    if (v == nullptr || *v == '\0' || std::strcmp(v, "AUTO") == 0 || std::strcmp(v, "auto") == 0)
      return 0;
    char* end = nullptr;
    const long k = std::strtol(v, &end, 10);
    if (end == v || *end != '\0' || k < 1 || k > 1024)
      throw std::runtime_error(std::string("WW_KOKKOS_SRCE_TEAM=") + v +
                               ": expected AUTO or a team size from 1 to 1024");
    return static_cast<int>(k);
  }();
  return n;
}

/// The policy of n teams that run_batch launches.
template <class Policy>
Policy srce_team_policy(int n) {
  const int req = srce_team_request();
  if (req == 0) return Policy(n, Kokkos::AUTO);
  const int conc = typename Policy::execution_space().concurrency();
  return Policy(n, req < conc ? req : conc);
}

}  // namespace ww::srce
