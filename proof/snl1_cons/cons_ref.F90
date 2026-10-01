!> @file cons_ref.F90
!> @brief Section 1 of W3SNL1 (the DIA propagation constant), verbatim, as a C-callable function.
!>
!> The three assignments below are the text of WW3/model/src/w3snl1md.F90 lines 340-342
!> (WAVEWATCH III 7.14 `develop`, pinned commit 761cf79d), the same lines that
!> kokkos/tests/fixtures/snl1_ref.F90 carries at 343-345. Only the wrapper is new: the
!> W3GDATMD parameters arrive as VALUE arguments instead of module variables, so a C++
!> driver can call this once per float32 bit pattern of KDMEAN (proof/snl1_cons/sweep.cpp).
!>
!> SPDX-License-Identifier: LGPL-3.0-or-later
!>
REAL(C_FLOAT) FUNCTION CONS_REF ( KDMEAN, KDCON, KDMN, SNLC1, SNLS1, SNLS2, SNLS3 ) &
     BIND(C, NAME='cons_ref')
  USE ISO_C_BINDING, ONLY: C_FLOAT
  IMPLICIT NONE
  REAL(C_FLOAT), VALUE    :: KDMEAN, KDCON, KDMN, SNLC1, SNLS1, SNLS2, SNLS3
  REAL                    :: X, X2, CONS
  !
  ! 1.  Calculate prop. constant --------------------------------------- *
  !
  X      = MAX ( KDCON*KDMEAN , KDMN )
  X2     = MAX ( -1.E15, SNLS3*X)
  CONS   = SNLC1 * ( 1. + SNLS1/X * (1.-SNLS2*X) * EXP(X2) )
  !
  CONS_REF = CONS
END FUNCTION CONS_REF
