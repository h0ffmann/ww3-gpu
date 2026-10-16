// kokkos/src/ww_kokkos/wavset_fd_compute_lh_stress.hpp
// FD_COMPUTE_LH_STRESS: the wave forcing of the setup on the finite-difference grid, minus
// centred or one-sided differences of the radiation stress between the four neighbours of each
// sea point. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:2281-2418 at NOAA-EMC/WW3 03adda91;
// authors Mathieu Dutour-Sikiric, Aron Roland.
// Evidence: kokkos/port_report/FD_COMPUTE_LH_STRESS_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <Kokkos_Core.hpp>

namespace ww::wavset {

/// One ISEA of FD_COMPUTE_LH_STRESS (:2372-2417); isea is 1-based. REAL(rkind) is float.
///   fx, fy        FX(NSEA), FY(NSEA), Fortran base 1;
///   neigh         W3GDATMD NEIGH(NSEA,4) (allocated at :1853), column-major, leading
///                 dimension ld_neigh = NSEA, values 1-based sea indices or 0;
///   sxx, sxy, syy W3ADATMD SXX(NSEALM), SXY, SYY, Fortran base 1, indexed by the global ISEA
///                 as the Fortran does;
///   dist_x, dist_y the locals dist_X, dist_Y, which the Fortran never assigns (:2357). The
///                 -O3 object materialises both as +0.0 (`pxor`) and folds 2*dist_X to the
///                 same register, so every difference is divided by +0.0. They are inputs
///                 here so the C++ has no indeterminate read; 0.0f reproduces the object.
KOKKOS_INLINE_FUNCTION void fd_compute_lh_stress_point(int isea, float* fx, float* fy,
                                                       const int* neigh, std::int64_t ld_neigh,
                                                       const float* sxx, const float* sxy,
                                                       const float* syy, float dist_x,
                                                       float dist_y) {
  const std::int64_t i = static_cast<std::int64_t>(isea) - 1;
  const int jsea1 = neigh[i];
  const int jsea2 = neigh[i + ld_neigh];
  const int jsea3 = neigh[i + 2 * ld_neigh];
  const int jsea4 = neigh[i + 3 * ld_neigh];
  const std::int64_t j1 = static_cast<std::int64_t>(jsea1) - 1;
  const std::int64_t j2 = static_cast<std::int64_t>(jsea2) - 1;
  const std::int64_t j3 = static_cast<std::int64_t>(jsea3) - 1;
  const std::int64_t j4 = static_cast<std::int64_t>(jsea4) - 1;
  float sxx_x = 0.0f;
  const float sxy_y = 0.0f;
  float sxy_x = 0.0f;
  const float syy_y = 0.0f;
  float syy_x = 0.0f;
  // As written: every Y difference goes into the *_X variables, SYY_X subtracts SYY from SXY,
  // the block of :2408 repeats the condition of :2398 (so JSEA3 > 0, JSEA4 = 0 is never handled),
  // and SXY_Y, SYY_Y stay 0. SYY_X is never used; the object drops it.
  if ((jsea1 > 0) && (jsea2 > 0)) {
    sxx_x = (sxx[j1] - sxx[j2]) / (2.0f * dist_x);
    sxy_x = (sxy[j1] - sxy[j2]) / (2.0f * dist_x);
    syy_x = (sxy[j1] - syy[j2]) / (2.0f * dist_x);
  }
  if ((jsea1 > 0) && (jsea2 == 0)) {
    sxx_x = (sxx[j1] - sxx[i]) / dist_x;
    sxy_x = (sxy[j1] - sxy[i]) / dist_x;
    syy_x = (sxy[j1] - syy[i]) / dist_x;
  }
  if ((jsea1 == 0) && (jsea2 > 0)) {
    sxx_x = (sxx[i] - sxx[j2]) / dist_x;
    sxy_x = (sxy[i] - sxy[j2]) / dist_x;
    syy_x = (sxy[i] - syy[j2]) / dist_x;
  }
  if ((jsea3 > 0) && (jsea4 > 0)) {
    sxx_x = (sxx[j3] - sxx[j4]) / (2.0f * dist_y);
    sxy_x = (sxy[j3] - sxy[j4]) / (2.0f * dist_y);
    syy_x = (sxy[j3] - syy[j4]) / (2.0f * dist_y);
  }
  if ((jsea3 == 0) && (jsea4 > 0)) {
    sxx_x = (sxx[i] - sxx[j4]) / dist_y;
    sxy_x = (sxy[i] - sxy[j4]) / dist_y;
    syy_x = (sxy[i] - syy[j4]) / dist_y;
  }
  if ((jsea3 > 0) && (jsea4 > 0)) {
    sxx_x = (sxx[j3] - sxx[i]) / dist_y;
    sxy_x = (sxy[j3] - sxy[i]) / dist_y;
    syy_x = (sxy[j3] - syy[i]) / dist_y;
  }
  (void)syy_x;
  const float efx = -sxx_x - sxy_y;
  const float efy = -syy_y - sxy_x;
  fx[i] = efx;
  fy[i] = efy;
}

/// FD_COMPUTE_LH_STRESS (:2281-2418). The dummies SXX_t, SXY_t, SYY_t are declared but never
/// read by the Fortran; they are kept in the signature. Arrays as in the point function.
inline void fd_compute_lh_stress(const float* sxx_t, const float* sxy_t, const float* syy_t,
                                 float* fx, float* fy, int nsea, const int* neigh,
                                 std::int64_t ld_neigh, const float* sxx, const float* sxy,
                                 const float* syy, float dist_x, float dist_y) {
  (void)sxx_t;
  (void)sxy_t;
  (void)syy_t;
  for (int k = 0; k < nsea; ++k) fx[k] = 0.0f;
  for (int k = 0; k < nsea; ++k) fy[k] = 0.0f;
  for (int isea = 1; isea <= nsea; ++isea)
    fd_compute_lh_stress_point(isea, fx, fy, neigh, ld_neigh, sxx, sxy, syy, dist_x, dist_y);
}

}  // namespace ww::wavset
