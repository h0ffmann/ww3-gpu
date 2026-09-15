!> @file shim_driver.F90
!> @brief Round trip: Fortran -> W3KOKKOSMD -> bind(C) shim -> Kokkos -> Fortran.
!>
!> The CTest case `shim_roundtrip`. It is the only test that exercises the whole
!> boundary the way WAVEWATCH III will: a Fortran caller with Fortran-owned,
!> column-major arrays, calling through the ISO_C_BINDING interfaces of
!> W3KOKKOSMD, and checking the answer against W3SNL1_REF -- the verbatim WW3
!> reference -- in the same process, on the same sea state.
!>
!> It deliberately does not read the committed binary fixture: that path is
!> already covered by L1_test_snl1_shim.cpp. What can only be caught here is a
!> mistake in the interface itself -- a missing VALUE, a transposed batch, a
!> result that never made it back through the pointer.
!>
!> Exit status: 0 on success, non-zero (STOP n) on any mismatch or error code, so
!> CTest needs no output parsing.
!>
!> SPDX-License-Identifier: MIT
!>
PROGRAM SHIM_DRIVER
  USE, INTRINSIC :: ISO_C_BINDING, ONLY: C_INT, C_FLOAT
  USE W3KOKKOSMD
  USE SNL1_REF
  USE SNL1_SEA_STATE
  IMPLICIT NONE
  !
  REAL, PARAMETER :: RELTOL   = 1.0E-5   ! the task brief's parity tolerance
  REAL, PARAMETER :: ABSFLOOR = 1.0E-30  ! so a denormal reference cannot demand
  !                                        infinite precision
  !
  INTEGER              :: IERR, IPT, ISP
  REAL                 :: WORST_S, WORST_D, REL
  REAL,    ALLOCATABLE :: AB(:,:), CGB(:,:), SB(:,:), DB(:,:), KDB(:)
  REAL,    ALLOCATABLE :: SREF(:), DREF(:), WN(:)
  !
  ! 1.  The reference side: the same grid, tables and sea state the committed
  !     fixture is built from.
  !
  CALL SETUP_REF ( NK_REF, NTH_REF, XFR_REF, FREQ1 )
  CALL INSNL1_REF
  !
  ALLOCATE ( AB(NSPEC,NPTS_REF), CGB(NK,NPTS_REF), SB(NSPEC,NPTS_REF),  &
             DB(NSPEC,NPTS_REF), KDB(NPTS_REF) )
  ALLOCATE ( SREF(NSPEC), DREF(NSPEC), WN(NK) )
  !
  DO IPT=1, NPTS_REF
    CALL DISPERSION ( DEPTHS(IPT), WN, CGB(:,IPT) )
    CALL SEA_STATE  ( DEPTHS(IPT), WN, AB(:,IPT), KDB(IPT) )
  END DO
  !
  ! Poisoned, not zeroed: a shim that forgets to copy its results back must fail
  ! this test rather than compare zeros against zeros.
  !
  SB = -1.0
  DB = -1.0
  !
  ! 2.  The shim side.
  !
  IERR = WW_KOKKOS_INIT ( -1_C_INT )
  IF ( IERR /= 0 ) THEN
    WRITE (*,'(A,I0)') 'shim_roundtrip: ww_kokkos_init failed, code ', IERR
    STOP 1
  END IF
  CALL W3KOKKOS_SETUP
  WRITE (*,'(A,L1)') ' shim_roundtrip: KOKKOS_SNL1 = ', KOKKOS_SNL1
  !
  IERR = WW_SNL1_INIT ( NK, NTH, XFR, DTH, LAM, SNLC1, KDCON, KDMN,     &
                        SNLS1, SNLS2, SNLS3, FACHFE, SIG )
  IF ( IERR /= 0 ) THEN
    WRITE (*,'(A,I0)') 'shim_roundtrip: ww_snl1_init failed, code ', IERR
    STOP 2
  END IF
  !
  CALL WW_SNL1 ( NPTS_REF, AB, CGB, KDB, SB, DB )
  IERR = WW_SNL1_LAST_ERROR()
  IF ( IERR /= 0 ) THEN
    WRITE (*,'(A,I0)') 'shim_roundtrip: ww_snl1 failed, code ', IERR
    STOP 3
  END IF
  !
  CALL WW_KOKKOS_FINALIZE
  !
  ! 3.  Compare, point by point, against the Fortran reference.
  !
  WORST_S = 0.
  WORST_D = 0.
  DO IPT=1, NPTS_REF
    CALL W3SNL1_REF ( AB(:,IPT), CGB(:,IPT), KDB(IPT), SREF, DREF )
    DO ISP=1, NSPEC
      REL     = ABS(SB(ISP,IPT)-SREF(ISP)) / MAX(ABS(SREF(ISP)),ABSFLOOR)
      WORST_S = MAX ( WORST_S, REL )
      REL     = ABS(DB(ISP,IPT)-DREF(ISP)) / MAX(ABS(DREF(ISP)),ABSFLOOR)
      WORST_D = MAX ( WORST_D, REL )
    END DO
  END DO
  !
  WRITE (*,'(A,I0,A,ES10.3,A,ES10.3,A,ES8.1)')                          &
       ' shim_roundtrip: ', NPTS_REF, ' points, max relative error S ',  &
       WORST_S, ', D ', WORST_D, ', tolerance ', RELTOL
  !
  DEALLOCATE ( AB, CGB, SB, DB, KDB, SREF, DREF, WN )
  CALL FREE_REF
  !
  IF ( WORST_S > RELTOL .OR. WORST_D > RELTOL ) THEN
    WRITE (*,'(A)') 'shim_roundtrip: FAILED -- the shim does not match W3SNL1_REF'
    STOP 1
  END IF
  !
END PROGRAM SHIM_DRIVER
