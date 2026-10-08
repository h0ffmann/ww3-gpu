// proof/snl1_cons/cons_port.cpp
// Section 1 of the Kokkos port of W3SNL1, as a plain C++ function.
//
// The three statements are kokkos/src/ww_kokkos/snl1_dia.cpp lines 108-112, copied.
// Kokkos::max and Kokkos::exp are replaced by what they expand to in Kokkos 5.2.0, so
// this file needs no Kokkos: max(a, b) is `(a < b) ? b : a` (core/src/Kokkos_MinMax.hpp)
// and exp(float) is std::exp(float) on the host (core/src/Kokkos_MathematicalFunctions.hpp,
// KOKKOS_IMPL_MATH_UNARY_FUNCTION with namespace std). On a CUDA device the same call is
// CUDA's expf, which this file does not model.
//
// -DWW_EXP_VIA_DOUBLE swaps exp for (float)exp((double)x), the lowering a float32 language
// may use for exp (Bend 2's F32.exp did), to measure that entry point on the same inputs.
//
// The copy is the weak link of this pilot: the sweep proves this file, not snl1_dia.cpp.
// Closing it means moving these lines into one inline function both files include
// (docs/BITWISE_PROOF_202610.md, pilot step 3).
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cmath>

namespace {

using Real = float;

inline const Real& kmax(const Real& a, const Real& b) { return (a < b) ? b : a; }

inline Real kexp(Real x) {
#ifdef WW_EXP_VIA_DOUBLE
  return static_cast<Real>(std::exp(static_cast<double>(x)));
#else
  return std::exp(x);
#endif
}

}  // namespace

extern "C" float cons_port(float kdmean, float kdcon, float kdmn, float snlc1, float snls1,
                           float snls2, float snls3) {
  const Real x = kmax(kdcon * kdmean, kdmn);
  const Real x2 = kmax(static_cast<Real>(-1.e15), snls3 * x);
  const Real cons =
      snlc1 * (static_cast<Real>(1) +
               snls1 / x * (static_cast<Real>(1) - snls2 * x) * kexp(x2));
  return cons;
}
