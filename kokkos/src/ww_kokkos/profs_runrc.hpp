// kokkos/src/ww_kokkos/profs_runrc.hpp
// RUNRC: reverse-communication driver of a SPARSKIT iterative solver (BCGSTAB in WW3): calls the
// solver until it stops, applying A, A^T or the ILU preconditioner each time it asks.
// Phase 1, translated, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3profsmd.F90:3801-3886 at NOAA-EMC/WW3 03adda91;
// module authors A. Roland, F. Ardhuin (SPARSKIT routine, Y. Saad).
// Evidence: kokkos/port_report/RUNRC_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <cstdio>

#include "profs_amux.hpp"
#include "profs_atmux.hpp"
#include "profs_lusol.hpp"
#include "profs_lutsol.hpp"

namespace ww::profs {

/// RUNRC(N, RHS, SOL, IPAR, FPAR, WK, GUESS, A, JA, IA, AU, JAU, JU, SOLVER), REAL*8 throughout.
/// Host function: it writes the solver's exit messages to stdout, as the Fortran's WRITE(*,*).
/// rhs, sol, guess: n; ipar, fpar: 16; wk: 8*n and more; a, ja: nnz; ia: n+1; au, jau, ju: the
/// ILU factors in modified sparse row form. All Fortran base 1, 0-based here; ipar(8), ipar(9) and
/// the index arrays hold the Fortran's 1-based positions.
/// solver is the EXTERNAL argument (BCGSTAB at :1239), any callable
/// solver(int n, const double* rhs, double* sol, int* ipar, double* fpar, double* w)
/// with w the (n,8) work array, column-major. n is passed by value: the Fortran passes it by
/// reference and rereads it after each solver call, which matters only to a solver that changes n.
template <class Solver>
inline void runrc(int n, const double* rhs, double* sol, int* ipar, double* fpar, double* wk,
                  const double* guess, const double* a, const int* ja, const int* ia,
                  const double* au, const int* jau, const int* ju, Solver&& solver) {
  // :3836-3839. ITS (SAVE, :3828, :3832) is written and compared, never read outside: not kept.
  if (ipar[1] > 2) {
    std::printf(" %s\n", "I can not do both left and right preconditioning.");
    return;
  }
  // :3843-3847.
  for (int i = 1; i <= n; ++i) sol[i - 1] = guess[i - 1];
  ipar[0] = 0;
  // :3849-3852. W_2D(1:n,1:8) => WK(1:n*8) is contiguous, so _gfortran_internal_pack hands the
  // solver wk itself (no copy).
  for (;;) {
    solver(n, rhs, sol, ipar, fpar, wk);
    const int code = ipar[0];
    if (code == 1) {
      amux(n, wk + (ipar[7] - 1), wk + (ipar[8] - 1), a, ja, ia);
      continue;
    } else if (code == 2) {
      atmux(n, wk + (ipar[7] - 1), wk + (ipar[8] - 1), a, ja, ia);
      continue;
    } else if (code == 3 || code == 5) {
      lusol(n, wk + (ipar[7] - 1), wk + (ipar[8] - 1), au, jau, ju);
      continue;
    } else if (code == 4 || code == 6) {
      lutsol(n, wk + (ipar[7] - 1), wk + (ipar[8] - 1), au, jau, ju);
      continue;
    } else if (code <= 0) {
      // :3869-3883, gfortran list-directed layout: a leading blank, INTEGER(4) in 12 columns,
      // one blank before the next item. ipar(1) = 0 (converged) writes nothing (:3870-3871).
      if (code == 0) {
      } else if (code == -1) {
        std::printf(" %s\n", "Iterative solver has iterated too many times.");
      } else if (code == -2) {
        std::printf(" %s\n", "Iterative solver was not given enough work space.");
        std::printf(" %s%12d %s\n", "The work space should at least have ", ipar[3],
                    " elements.");
      } else if (code == -3) {
        std::printf(" %s\n", "Iterative sovler is facing a break-down.");
      } else {
        std::printf(" %s%12d\n", "Iterative solver terminated. code =", code);
      }
      break;
    }
    // ipar(1) > 6 falls through the IF chain and calls the solver again, as the Fortran does.
  }
}

}  // namespace ww::profs
