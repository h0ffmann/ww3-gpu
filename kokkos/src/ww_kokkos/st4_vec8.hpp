// kokkos/src/ww_kokkos/st4_vec8.hpp
// Eight lanes of REAL and their masks, GCC's vector extensions, for the routines that run one
// sea point per lane (plan 2026-10-18-005). Host code only: the lane path runs on host backends.
// Arithmetic on these types is lane-wise IEEE; with -ffp-contract=off nothing fuses.
#pragma once

#include <cstring>

#include "real.hpp"

namespace ww::st4::lanes {

using V8 = Real __attribute__((vector_size(8 * sizeof(Real))));
using M8 = int __attribute__((vector_size(8 * sizeof(int))));
static_assert(sizeof(Real) == sizeof(int), "a mask lane per REAL lane");
// By reference, not by value: a function returning a 32-byte vector warns (-Wpsabi) in a build
// without AVX (serial-debug has no -march).
inline void vload(V8& v, const Real* x) { __builtin_memcpy(&v, x, sizeof v); }
inline void vstore(Real* x, const V8& v) { __builtin_memcpy(x, &v, sizeof v); }

}  // namespace ww::st4::lanes
