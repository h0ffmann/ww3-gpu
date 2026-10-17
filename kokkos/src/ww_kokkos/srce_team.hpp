// kokkos/src/ww_kokkos/srce_team.hpp
// Which W3SRCE body the batched launch runs (srce_shim.cpp, run_batch), with how many threads
// per point, and the executor the team-parallel body is written against.
//
// Two bodies compute the same bits. w3srce_team (srce_kernel.hpp) runs W3SRCE on one thread of
// its team and only the DIA over the team: the code of every host run by default, where the
// sea points already fill the threads one each. w3srce_split (srce_kernel_team.hpp) splits every
// loop whose iterations are independent over the team and keeps every sum, MIN/MAX and running
// value on one thread in the Fortran's order: the code of a device run, where one thread per
// point leaves the GPU idle.
//
// WW_KOKKOS_SRCE_TEAM, read once:
//   unset, empty or AUTO  w3srce_team on a host backend, as before (TeamPolicy(n, AUTO));
//                         w3srce_split on a device, kDeviceTeamShared or kDeviceTeamGlobal
//                         threads per point
//   0                     w3srce_team on every backend (the device's old kernel, for an A/B)
//   N (1 to 1024)         w3srce_split, teams of N threads, capped at what the backend offers
//                         (the concurrency of a host backend, team_size_max on a device)
// so that the team path, the device's, can be run and checked bit for bit on the OpenMP
// backend. Anything else is refused.
//
// WW_KOKKOS_SRCE_SCRATCH, read once, where W3SNL1's team scratch lives (both bodies): unset or
// AUTO, level 0 when it fits (a device's shared memory), else level 1; 0 or 1 that level. On a
// device the level sets how many points share a streaming multiprocessor (kDeviceTeam), so it is
// the knob of a supervised A/B; it moves no bit, since each point owns its scratch either way.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include <Kokkos_Core.hpp>

namespace ww {

/// The whole team as an executor: for_(n, f) runs f(i) for every i in [0, n), each i on one
/// thread (TeamThreadRange); one(f) runs f once, on one thread (single(PerTeam)); sync() is the
/// team barrier. A body may split over for_ only a loop whose iterations are independent (each
/// writes its own elements and reads nothing another writes); every reduction and running value
/// runs inside one(), in the Fortran's order, and what one() computes reaches the other threads
/// only through memory the team shares, after a sync().
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

/// Threads per point of w3srce_split on a device when WW_KOKKOS_SRCE_TEAM does not say, by the
/// level of W3SNL1's team scratch. Much of W3SRCE is sums and running values on one thread, so
/// what matters first is how many points a streaming multiprocessor runs at once, then that its
/// warps are used: ⚠ an estimate from ptxas and the hardware limits, not measured. On the RTX
/// 4090 (sm_89: 100 KiB of shared memory, 64 Ki registers, 48 warps and 24 blocks per SM) with
/// the full case (NK = NTH = 24, NSPECX = 672, NSPECY = 744: about 27 KiB of scratch per team,
/// plus 1 KiB the driver keeps per block) three teams fit in shared memory (level 0), and
/// w3srce_split's 203 registers (ptxas, with libm_device.hpp's routines) allow 9 warps: three
/// teams of three warps (96 threads) use all 9. With the scratch in global memory (level 1) the
/// registers alone limit, and 9 teams of one warp run at once.
/// kokkos/port_report/W3SRCE_KOKKOS.md has the table.
inline constexpr int kDeviceTeamShared = 96;
inline constexpr int kDeviceTeamGlobal = 32;

/// WW_KOKKOS_SRCE_TEAM, read once: -1 unset/AUTO, 0 w3srce_team everywhere, N > 0 a team size.
inline int srce_team_request() {
  static const int n = [] {
    const char* v = std::getenv("WW_KOKKOS_SRCE_TEAM");
    if (v == nullptr || *v == '\0' || std::strcmp(v, "AUTO") == 0 || std::strcmp(v, "auto") == 0)
      return -1;
    char* end = nullptr;
    const long k = std::strtol(v, &end, 10);
    if (end == v || *end != '\0' || k < 0 || k > 1024)
      throw std::runtime_error(std::string("WW_KOKKOS_SRCE_TEAM=") + v +
                               ": expected AUTO, 0 or a team size from 1 to 1024");
    return static_cast<int>(k);
  }();
  return n;
}

/// WW_KOKKOS_SRCE_SCRATCH, read once: -1 unset/AUTO, else the scratch level, 0 or 1.
inline int srce_scratch_request() {
  static const int n = [] {
    const char* v = std::getenv("WW_KOKKOS_SRCE_SCRATCH");
    if (v == nullptr || *v == '\0' || std::strcmp(v, "AUTO") == 0 || std::strcmp(v, "auto") == 0)
      return -1;
    if (std::strcmp(v, "0") == 0) return 0;
    if (std::strcmp(v, "1") == 0) return 1;
    throw std::runtime_error(std::string("WW_KOKKOS_SRCE_SCRATCH=") + v +
                             ": expected AUTO, 0 or 1");
  }();
  return n;
}

/// Whether run_batch runs w3srce_split on ExecSpace.
template <class ExecSpace>
bool srce_split_on() {
  const int req = srce_team_request();
  if (req < 0) return !Kokkos::SpaceAccessibility<ExecSpace, Kokkos::HostSpace>::accessible;
  return req > 0;
}

/// The team size w3srce_split asks for on ExecSpace with W3SNL1's scratch at `level`, before
/// the backend's cap.
template <class ExecSpace>
int srce_split_team(int level) {
  const int req = srce_team_request();
  if (req > 0) return req;
  if (Kokkos::SpaceAccessibility<ExecSpace, Kokkos::HostSpace>::accessible) return 1;
  return level == 0 ? kDeviceTeamShared : kDeviceTeamGlobal;
}

}  // namespace ww::srce
