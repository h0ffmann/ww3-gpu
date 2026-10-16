// kokkos/src/ww_kokkos/ic3_ic3table_cheng.hpp
// IC3TABLE_CHENG: the deep-water table of k_r, k_i and c_g of the IC3 (Cheng) ice dispersion
// relation for every ice thickness 0.01 m to 3 m, built once on the host. Phase 1, translated,
// not regtested.
// WW3 heritage: WW3-noaa/model/src/w3sic3md.F90:1962-2044 (and the parameters at :89) at
// NOAA-EMC/WW3 03adda91; authors E. Rogers, S. Zieger, X. Zhao, S. Cheng.
// Evidence: kokkos/port_report/IC3TABLE_CHENG_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstddef>

#include "real.hpp"

namespace ww::ic3 {

inline constexpr Real kIc3TableGrav = static_cast<Real>(9.806);  // GRAV, constants.F90:62
// IC3_DITK = 0.01 (:89); ICE1 = JITK*IC3_DITK is (REAL)JITK * 0.01f in the object.
inline constexpr Real kIc3Ditk = static_cast<Real>(0.01);
// CEILING(IC3_MAXITK/IC3_DITK) = CEILING(3./0.01) (:2034), folded to 300 by gfortran.
inline constexpr int kIc3Itknum = 300;

/// IC3TABLE_CHENG. ice2, ice3, ice4: the rheology (FIXEDVISC, FIXEDDENS, FIXEDELAS from
/// w3wavemd.F90:998). Module state: nk; sig is SIG(0:NK+1); ic3wn_r, ic3wn_i, ic3cg are
/// IC3WN_R, IC3WN_I, IC3CG(0:NK+1, 0:300) of w3adatmd.F90:1316-1330, column-major, here
/// 0-based with leading dimension nk + 2. Column 0 of ic3cg is not written, as in the Fortran.
/// ic3precalc_cheng is IC3PRECALC_CHENG (:2047-2160), not translated yet: a host callable
/// (Real* wn_r, Real* wn_i, Real* cg, Real ice1, Real ice2, Real ice3, Real ice4, Real dpt),
/// each array 0:NK+1 here 0-based, which allocates its own work array.
template <class Ic3PrecalcCheng>
inline void ic3table_cheng(Real ice2, Real ice3, Real ice4, int nk, const Real* sig,
                           Real* ic3wn_r, Real* ic3wn_i, Real* ic3cg,
                           Ic3PrecalcCheng&& ic3precalc_cheng) {
  const std::ptrdiff_t ld = static_cast<std::ptrdiff_t>(nk) + 2;
  // :2032. DPT is passed by reference and read as DBLE(DPT) = 999.0 in the callee.
  const Real dpt = static_cast<Real>(999.);
  // :2035. SIG**2 is SIG*SIG; the division by GRAV stays a division.
  for (std::ptrdiff_t ik = 0; ik < ld; ++ik) ic3wn_r[ik] = sig[ik] * sig[ik] / kIc3TableGrav;
  // :2036.
  for (std::ptrdiff_t ik = 0; ik < ld; ++ik) ic3wn_i[ik] = static_cast<Real>(0);
  for (int jitk = 1; jitk <= kIc3Itknum; ++jitk) {
    // :2038.
    const Real ice1 = static_cast<Real>(jitk) * kIc3Ditk;
    // :2039-2040. The object packs each column (_gfortran_internal_pack); a copy, no arithmetic.
    const std::ptrdiff_t col = static_cast<std::ptrdiff_t>(jitk) * ld;
    ic3precalc_cheng(ic3wn_r + col, ic3wn_i + col, ic3cg + col, ice1, ice2, ice3, ice4, dpt);
  }
}

}  // namespace ww::ic3
