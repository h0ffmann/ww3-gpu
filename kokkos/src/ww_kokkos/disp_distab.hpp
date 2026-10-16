// kokkos/src/ww_kokkos/disp_distab.hpp
// DISTAB and WAVNU2, ported to Kokkos: the interpolation tables of nondimensional
// wavenumber EWN1 and group velocity ECG1 against nondimensional frequency that
// WAVNU1 reads, each entry from a Newton solution of the linear dispersion relation
// w**2 = g k tanh(k h) at depth h = 1. Phase 1, translated, not regtested.
//
// WW3 heritage: WW3-noaa/model/src/w3dispmd.F90:552-683 (DISTAB) and :204-346
// (WAVNU2) at NOAA-EMC/WW3 03adda91; author H. L. Tolman. W3_S (STRACE) is not
// compiled under switch_lab_shrd and is not ported.
//
// Arithmetic checked against the gfortran 16.2 -O3 GIMPLE and object of
// w3dispmd.F90 (WW3's flags, switch_lab_shrd):
// - DISTAB calls the out-of-line __w3dispmd_MOD_wavnu2 (no clone, no inlining):
//   EPS and NMAX arrive by reference from .rodata, 1E-7 = 0x33d6bf95 and 15.
// - SIMAX, DSIE and ECG1(0) are folded at compile time: DSIE = 0x3d82e8c3,
//   ECG1(0) = SQRT(GRAV) = 0x404869bb. SQRT is the sqrtss instruction, correctly
//   rounded, so the run-time sqrt below gives the same bits.
// - TANH, COSH, SINH are scalar calls of glibc's tanhf, coshf, sinhf; no sincos,
//   no libmvec, no vectorised loop (w3dispmd.vec.txt is empty).
// - (COSH(x))**2 is c*c; W0**2 and W0*W0 are both w*w on the signed W (same bits).
// - Every division is a divss, never a reciprocal multiply; no MIN/MAX; no FMA.
#pragma once

#include <cmath>

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::disp {

inline constexpr Real kGrav = static_cast<Real>(9.806);  // GRAV, constants.F90:62
inline constexpr int kNar1d = 121;                         // NAR1D, w3dispmd.F90:75
inline constexpr Real kDfac = static_cast<Real>(6.);       // DFAC, w3dispmd.F90:76
inline constexpr int kN1max = kNar1d - 1;                  // N1MAX, :652
// DISTAB's actual arguments of WAVNU2 (:666, :675). 1E-7 is a REAL literal, read as
// a float literal (0x33d6bf95), never a double narrowed.
inline constexpr Real kDistabEps = 1e-7f;
inline constexpr int kDistabNmax = 15;

namespace detail {
// glibc's scalar tanhf/coshf/sinhf on the host, as the Fortran object calls them.
// ⚠ On a device, Kokkos' (CUDA libdevice) tanh/cosh/sinh use other algorithms and
// are not shown to return glibc's bits (AGENTS.md, "Bit for bit with the Fortran").
KOKKOS_INLINE_FUNCTION Real tanh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::tanhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::tanh(x);))
#else
  return Kokkos::tanh(x);
#endif
}
KOKKOS_INLINE_FUNCTION Real cosh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::coshf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::cosh(x);))
#else
  return Kokkos::cosh(x);
#endif
}
KOKKOS_INLINE_FUNCTION Real sinh_(Real x) {
#if defined(__GLIBC__)
  KOKKOS_IF_ON_HOST((return ::sinhf(x);))
  KOKKOS_IF_ON_DEVICE((return Kokkos::sinh(x);))
#else
  return Kokkos::sinh(x);
#endif
}
}  // namespace detail

/// WAVNU2(W, H, K, CG, EPS, NMAX, ICON), w3dispmd.F90:204-346: wavenumber K (rad/m)
/// and group velocity CG (m/s) for intrinsic frequency W (rad/s) and depth H (m) by
/// Newton iteration; ICON = 1 when |dK| < EPS and |dK|/K < EPS was reached. Operand
/// order of each statement is the -O3 GIMPLE's (commuted products are noted; a
/// commuted IEEE product is the same bits).
KOKKOS_INLINE_FUNCTION void wavnu2(Real w, Real h, Real& k, Real& cg, Real eps, int nmax,
                                   int& icon) {
  const Real zero = static_cast<Real>(0);
  cg = zero;  // :287, overwritten at :331/:333 on every path
  Real kold = zero;
  icon = 0;
  const Real w0 = Kokkos::fabs(w);  // :290
  // :295-299. sqrt is IEEE sqrt (sqrtss) on both sides. W0*W0 is emitted as W*W.
  if (Kokkos::sqrt(kGrav / h) > w0) {
    k = w0 / Kokkos::sqrt(h * kGrav);
  } else {
    k = w * w / kGrav;
  }
  // :303-323. On EXIT K keeps the value tested; otherwise K = KOLD - F/FD.
  for (int i = 1; i <= nmax; ++i) {
    const Real dif = Kokkos::fabs(k - kold);  // :304
    const Real rdif = (k != zero) ? dif / k : zero;  // :305-309
    if (dif < eps && rdif < eps) {  // :310
      icon = 1;
      break;
    }
    kold = k;
    // :315: F = (KOLD*GRAV)*TANH(H*KOLD) - W*W.
    const Real gk = kold * kGrav;
    const Real kh = h * kold;
    const Real th = detail::tanh_(kh);
    const Real f = gk * th - w * w;
    Real fd;
    if (kh > static_cast<Real>(25)) {  // :316
      fd = th * kGrav;  // :317
    } else {
      // :319: TANH*GRAV + (H*(KOLD*GRAV)) / (C*C), C = COSH(KOLD*H).
      const Real c = detail::cosh_(kh);
      fd = th * kGrav + (h * gk) / (c * c);
    }
    k = kold - f / fd;  // :321
  }
  if (icon == 0) {  // :325-329, no K /= 0 guard, as in the Fortran
    const Real dif = Kokkos::fabs(k - kold);
    const Real rdif = dif / k;
    if (dif < eps && rdif < eps) icon = 1;
  }
  // :330-334. 2*K*H is emitted as H*(K*2), written H*(K+K) here (the same bits); W0/K*0.5*(...) as ((W0/K)*0.5)*(1+x/SINH(x)).
  const Real kh2 = h * (k + k);
  const Real cgd = w0 / k * static_cast<Real>(0.5);
  if (kh2 > static_cast<Real>(25)) {
    cg = cgd;
  } else {
    cg = cgd * (static_cast<Real>(1) + kh2 / detail::sinh_(kh2));
  }
  if (w < zero) {  // :335-338, (-1)*X is a sign flip (xorps)
    k = -k;
    cg = -cg;
  }
}

/// One entry I of DISTAB's tables (w3dispmd.F90:659-677), I = 0..NAR1D. Entries
/// 1..N1MAX are the loop at :664-669 and entry N1MAX+1 = NAR1D is the statement block
/// at :673-677; both are SI = REAL(I)*DSIE, WAVNU2(SI, 1., K, CG, 1E-7, 15, ICON).
/// ecg0 is SQRT(GRAV) (:660). ICON is discarded, as in the Fortran.
KOKKOS_INLINE_FUNCTION void distab_entry(int i, Real dsie, Real ecg0, Real& ewn, Real& ecg) {
  if (i == 0) {
    ewn = static_cast<Real>(0.);  // :659
    ecg = ecg0;                   // :660
    return;
  }
  const Real si = static_cast<Real>(i) * dsie;  // :665, :674 (cvtsi2ss, mulss)
  const Real depth = static_cast<Real>(1.);     // :653
  Real k = 0;
  Real cg = 0;
  int icon = 0;
  wavnu2(si, depth, k, cg, kDistabEps, kDistabNmax, icon);
  ewn = k;
  ecg = cg;
}

/// What DISTAB writes besides the tables: N1MAX (:652) and DSIE (:655).
struct DistabScalars {
  int n1max;
  Real dsie;
};

/// DISTAB (w3dispmd.F90:552-683). ewn1 and ecg1 hold EWN1(0:NAR1D) and
/// ECG1(0:NAR1D) (w3dispmd.F90:79), 122 entries each, index i = Fortran index i
/// (the Fortran lower bound is 0, so no shift). Entries are independent: SI depends
/// only on I and the constant DSIE, and WAVNU2 has no state between calls, so one
/// thread per entry with no reduction gives the bits of the Fortran's serial loop on
/// any execution space and in any order. The Fortran writes 0, 1..120, then 121.
template <class ExecSpace>
DistabScalars distab(const ExecSpace& space,
                     const Kokkos::View<Real*, typename ExecSpace::memory_space>& ewn1,
                     const Kokkos::View<Real*, typename ExecSpace::memory_space>& ecg1) {
  if (ewn1.extent(0) != static_cast<std::size_t>(kNar1d + 1) ||
      ecg1.extent(0) != static_cast<std::size_t>(kNar1d + 1)) {
    Kokkos::abort("ww::disp::distab: EWN1/ECG1 must have NAR1D+1 = 122 entries");
  }
  // :654-655, :660 on the host; gfortran folds them, and an IEEE sqrt and divide at
  // run time give the folded bits.
  const Real simax = std::sqrt(kGrav * kDfac);
  const Real dsie = simax / static_cast<Real>(kN1max);
  const Real ecg0 = std::sqrt(kGrav);
  Kokkos::parallel_for(
      "ww::disp::distab", Kokkos::RangePolicy<ExecSpace>(space, 0, kNar1d + 1),
      KOKKOS_LAMBDA(const int i) { distab_entry(i, dsie, ecg0, ewn1(i), ecg1(i)); });
  space.fence("ww::disp::distab");
  return DistabScalars{kN1max, dsie};
}

}  // namespace ww::disp
