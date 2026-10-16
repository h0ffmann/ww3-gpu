// kokkos/src/ww_kokkos/wavset_fd_wave_setup_solve_poisson_neumann_dir.hpp
// FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR: preconditioned conjugate gradient for the wave setup
// Poisson system on a finite-difference (PDLIB) grid. Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3wavset.F90:2742-2852 at NOAA-EMC/WW3 03adda91;
// authors M. Dutour-Sikiric, A. Roland. Evidence:
// kokkos/port_report/FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "wavset_fd_wave_setup_apply_fct.hpp"
#include "wavset_fd_wave_setup_apply_precond.hpp"
#include "wavset_fd_wave_setup_scalar_prod.hpp"

namespace ww::wavset {

/// FD_WAVE_SETUP_SOLVE_POISSON_NEUMANN_DIR, arrays 0-based. aspar (PDLIB_NNZ), b, theout (NX):
/// Fortran base 1. nx is W3GDATMD NX (the length of B, TheOut and the scalar products); nsea is
/// W3GDATMD NSEA, which the callees APPLY_FCT and APPLY_PRECOND loop over. pdlib_ia, pdlib_ja,
/// pdlib_i_diag: yowNodepool's arrays, 1-based values, passed to the callees.
/// v_x, v_r, v_z, v_p, v_y: caller-provided work arrays, the Fortran's automatic arrays of NX
/// reals (:2810, which the object mallocs per call), distinct from each other and from b, theout.
/// The callees write NSEA elements of v_z, v_y and the scalar products read NX: the Fortran
/// overruns its NX-element arrays when NSEA > NX and reads unset elements when NSEA < NX, so
/// give each work array max(NX, NSEA) reals; only NSEA == NX has a defined Fortran result.
/// REAL(rkind) is float (PDLIB/yowdatapool.F90:48).
KOKKOS_INLINE_FUNCTION void fd_wave_setup_solve_poisson_neumann_dir(
    const Real* aspar, const Real* b, Real* theout, int nx, int nsea, const int* pdlib_ia,
    const int* pdlib_ja, const int* pdlib_i_diag, Real* v_x, Real* v_r, Real* v_z, Real* v_p,
    Real* v_y) {
  // :2818 the default-REAL literal 0.00000001, 0x1.5798eep-27 in the object.
  const Real solverthr = 0.00000001f;
  Real uo, un, alphav, h2, enorm, beta;
  for (int ip = 0; ip < nx; ++ip) v_x[ip] = static_cast<Real>(0);
  for (int ip = 0; ip < nx; ++ip) v_r[ip] = b[ip];
  fd_wave_setup_apply_precond(aspar, v_r, v_z, nsea, pdlib_ia, pdlib_ja, pdlib_i_diag);
  for (int ip = 0; ip < nx; ++ip) v_p[ip] = v_z[ip];
  fd_wave_setup_scalar_prod(v_z, v_r, uo, nx);
  // :2825 DO with no bound: as in WW3, a residual that never drops to SOLVERTHR (or is NaN)
  // never exits.
  for (;;) {
    fd_wave_setup_apply_fct(aspar, v_p, v_y, nsea, pdlib_ia, pdlib_ja);
    fd_wave_setup_scalar_prod(v_p, v_y, h2, nx);
    alphav = uo / h2;
    for (int ip = 0; ip < nx; ++ip) {
      v_x[ip] = v_x[ip] + alphav * v_p[ip];
      v_r[ip] = v_r[ip] - alphav * v_y[ip];
    }
    fd_wave_setup_scalar_prod(v_r, v_r, enorm, nx);
    if (enorm <= solverthr) break;
    fd_wave_setup_apply_precond(aspar, v_r, v_z, nsea, pdlib_ia, pdlib_ja, pdlib_i_diag);
    fd_wave_setup_scalar_prod(v_z, v_r, un, nx);
    beta = un / uo;
    uo = un;
    for (int ip = 0; ip < nx; ++ip) v_p[ip] = v_z[ip] + beta * v_p[ip];
  }
  for (int ip = 0; ip < nx; ++ip) theout[ip] = v_x[ip];
}

}  // namespace ww::wavset
