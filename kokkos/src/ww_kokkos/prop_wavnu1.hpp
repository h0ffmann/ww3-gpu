// kokkos/src/ww_kokkos/prop_wavnu1.hpp
// WAVNU1, ported to Kokkos: wavenumber and group velocity of one (sigma, depth)
// pair by linear interpolation in the DISTAB table. Phase 1, translated, not
// regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3dispmd.F90:85-202 at NOAA-EMC/WW3 03adda91;
// author H. L. Tolman. The table it reads (EWN1, ECG1, DSIE, N1MAX, :74-78) is
// filled by DISTAB (:552-683) through WAVNU2 when W3IOGR reads mod_def
// (w3iogrmd.F90:2019); the port copies it from the Fortran module after that
// call and never recomputes it. W3_S (STRACE) is not ported.
//
// Arithmetic checked against gfortran 16.2 -O3 GIMPLE and assembly of
// w3dispmd.F90 (WW3's own flags, switch_lab_shrd): SQRT is the inline sqrtss,
// no libm call; one quotient SIX/DSIE is shared by INT() and R1; DSIE and N1MAX
// are run-time loads from the module, not folded; GRAV and 0.5*GRAV are folded
// to 0x411CE560 and 0x409CE560; no MIN/MAX, no FMA, nothing vectorised.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::prop {

/// GRAV = 9.806 of constants.F90:62, rounded to REAL once (0x411CE560).
inline constexpr Real kGrav = ww::st4::kGrav;

/// 0.5*GRAV as the object folds it at :195 (0x409CE560); exact in float.
inline constexpr Real kHalfGrav = static_cast<Real>(0.5) * kGrav;

/// The DISTAB table as the Fortran module holds it (w3dispmd.F90:74-78), copied
/// from __w3dispmd_MOD_ewn1 / _ecg1 / _dsie / _n1max after W3IOGR ran. ewn1 and
/// ecg1 point at NAR1D + 1 = 122 floats, indices 0..121, EWN1(0:NAR1D). Host
/// pointers in phase 1, device pointers in phase 2.
struct DispTable {
  const Real* ewn1;  ///< EWN1(0:NAR1D): k* = kh at h = 1 (:614-619)
  const Real* ecg1;  ///< ECG1(0:NAR1D): cg* = cg / sqrt(g) at h = 1
  Real dsie;         ///< DSIE = SQRT(GRAV*DFAC) / REAL(N1MAX) (:658), 0x3D82E8C3
  int n1max;         ///< N1MAX = NAR1D - 1 = 120 (:655)
};

/// WAVNU1(SI, H, K, CG): si intrinsic frequency (rad/s), h depth (m); k
/// wavenumber (rad/m), cg group velocity (m/s), both always written.
/// Called per sea point per IK by W3INIT (w3initmd.F90:1431) and W3ULEV
/// (w3updtmd.F90:2380), not in the per-step propagation path. No guard for
/// si <= 0 or h <= 0 (:134, "check in main program assumed").
KOKKOS_INLINE_FUNCTION void wavnu1(const DispTable& t, Real si, Real h, Real& k, Real& cg) {
  const Real sqrth = Kokkos::sqrt(h);            // :183, sqrtss; h < 0 gives NaN
  const Real six = si * sqrth;                   // :184
  const Real x = six / t.dsie;                   // :185, the one quotient :189 reuses (CSE)
  const int i1 = ww::st4::fint(x);               // :185, INT truncates; NaN or overflow -> INT_MIN
  if (i1 <= t.n1max && i1 >= 1) {                // :187, integer compares; I1 == 0 goes deep
    const int i2 = i1 + 1;                       // :188, up to NAR1D = 121, the last entry
    const Real r1 = x - static_cast<Real>(i1);   // :189, no clamp of r1
    const Real r2 = static_cast<Real>(1.) - r1;  // :190
    k = (r2 * t.ewn1[i1] + r1 * t.ewn1[i2]) / h;       // :191, two mulss, addss, divss by h
    cg = (r2 * t.ecg1[i1] + r1 * t.ecg1[i2]) * sqrth;  // :192, then mulss by the sqrt of :183
  } else {                                       // :193, deep water, also the I1 == 0 end
    k = si * si / kGrav;                         // :194, divss by 0x411CE560, no reciprocal
    cg = kHalfGrav / si;                         // :195, 0x409CE560 / si; si == 0 gives +inf
  }
}

}  // namespace ww::prop
