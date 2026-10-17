// kokkos/src/ww_kokkos/prop_loops.hpp
// How the loops of w3xyp3 and w3qck3 are run: by one thread, in index order (SerialLoops, the
// translation as written), or by the threads of one Kokkos team (TeamLoops, WW_KOKKOS_XYP3_TEAM,
// PATCH.md section 16). A routine written against either calls par() only for a loop whose
// iterations write disjoint elements and read nothing another iteration of the same loop writes,
// so each element gets the same operations in the same order whichever thread runs it; a loop
// that does not qualify (a scatter with coinciding targets, a last-write-wins list, an in-order
// scan) goes through one() or one_value(). The team barrier after each call keeps the order
// between loops: a loop sees every store of the loops before it, as in the serial body.
#pragma once

#include <Kokkos_Core.hpp>

namespace ww::prop {

/// One thread: par() is the plain for loop, one() a call; nothing between loops.
struct SerialLoops {
  template <class F>
  KOKKOS_INLINE_FUNCTION void par(int lo, int hi, const F& f) const {
    for (int i = lo; i < hi; ++i) f(i);
  }
  template <class F>
  KOKKOS_INLINE_FUNCTION void one(const F& f) const {
    f();
  }
  template <class T, class F>
  KOKKOS_INLINE_FUNCTION void one_value(const F& f, T& v) const {
    f(v);
  }
};

/// The threads of team m: par() splits [lo, hi) over them (TeamThreadRange), one() runs on one
/// of them (single PerTeam), one_value() also broadcasts its result; a team barrier ends each.
template <class Member>
struct TeamLoops {
  const Member& m;
  template <class F>
  KOKKOS_INLINE_FUNCTION void par(int lo, int hi, const F& f) const {
    Kokkos::parallel_for(Kokkos::TeamThreadRange(m, lo, hi), f);
    m.team_barrier();
  }
  template <class F>
  KOKKOS_INLINE_FUNCTION void one(const F& f) const {
    Kokkos::single(Kokkos::PerTeam(m), f);
    m.team_barrier();
  }
  template <class T, class F>
  KOKKOS_INLINE_FUNCTION void one_value(const F& f, T& v) const {
    Kokkos::single(Kokkos::PerTeam(m), f, v);
    m.team_barrier();
  }
};

}  // namespace ww::prop
