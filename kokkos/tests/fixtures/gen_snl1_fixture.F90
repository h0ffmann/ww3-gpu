!> @file gen_snl1_fixture.F90
!> @brief Write the committed W3SNL1/INSNL1 parity fixture.
!>
!> Runs SNL1_REF -- the verbatim WAVEWATCH III 7.14 reference in snl1_ref.F90 --
!> on a deterministic JONSWAP x cos^2 sea state at three depths and streams the
!> inputs, the INSNL1 tables and the W3SNL1 answers to one little-endian binary
!> file. `kokkos/tests/fixtures/README.md` documents the layout byte for byte;
!> `ww::fixture::load()` reads it back.
!>
!> Nothing here is random and nothing is read from the environment: the same
!> compiler flags must reproduce the same bytes. The grid, the dispersion solve
!> and the spectrum itself live in SNL1_SEA_STATE so that shim_driver.F90 replays
!> exactly this sea state through the bind(C) shim.
!>
!> Usage: gen_snl1_fixture <output-path>
!>
!> SPDX-License-Identifier: MIT
!>
PROGRAM GEN_SNL1_FIXTURE
  USE SNL1_REF
  USE SNL1_SEA_STATE
  IMPLICIT NONE
  !
  INTEGER, PARAMETER :: NPTS     = NPTS_REF
  INTEGER, PARAMETER :: MAGIC    = INT(z'534E4C31')   ! 'SNL1'
  !
  CHARACTER(LEN=512)  :: PATH
  INTEGER             :: UNIT, IPT
  REAL                :: KDMEAN
  REAL, ALLOCATABLE   :: A(:), S(:), D(:), CG(:), WN(:)
  !
  IF ( COMMAND_ARGUMENT_COUNT() /= 1 ) THEN
    WRITE (*,*) 'usage: gen_snl1_fixture <output-path>'
    STOP 1
  END IF
  CALL GET_COMMAND_ARGUMENT ( 1, PATH )
  !
  ! The fixture layout is fixed-width; refuse to write it from a build whose
  ! default REAL or INTEGER is not the 32-bit one WW3 assumes.
  !
  IF ( STORAGE_SIZE(1.0) /= 32 .OR. STORAGE_SIZE(1) /= 32 ) THEN
    WRITE (*,*) 'default REAL/INTEGER are not 32-bit; refusing to write'
    STOP 2
  END IF
  !
  CALL SETUP_REF ( NK_REF, NTH_REF, XFR_REF, FREQ1 )
  CALL INSNL1_REF
  !
  ALLOCATE ( A(NSPEC), S(NSPEC), D(NSPEC), CG(NK), WN(NK) )
  !
  OPEN ( NEWUNIT=UNIT, FILE=TRIM(PATH), FORM='UNFORMATTED', &
       ACCESS='STREAM', STATUS='REPLACE', ACTION='WRITE' )
  !
  ! --- header ------------------------------------------------------------
  !
  WRITE (UNIT) MAGIC, NK, NTH, NPTS
  WRITE (UNIT) XFR, DTH, LAM, SNLC1, KDCON, KDMN, SNLS1, SNLS2, SNLS3, FACHFE
  WRITE (UNIT) SIG(1:NK)
  !
  ! --- INSNL1 tables -----------------------------------------------------
  !
  WRITE (UNIT) NFR, NFRHGH, NFRCHG, NSPECX, NSPECY
  WRITE (UNIT) DAL1, DAL2, DAL3
  WRITE (UNIT) AWG1, AWG2, AWG3, AWG4, AWG5, AWG6, AWG7, AWG8
  WRITE (UNIT) SWG1, SWG2, SWG3, SWG4, SWG5, SWG6, SWG7, SWG8
  ! 16 index tables of length NSPECX, in the order INSNL1 assigns them.
  WRITE (UNIT) IP11, IP12, IP13, IP14, IM11, IM12, IM13, IM14
  WRITE (UNIT) IP21, IP22, IP23, IP24, IM21, IM22, IM23, IM24
  ! 16 index tables of length NSPEC, in the order INSNL1 assigns them.
  WRITE (UNIT) IC11, IC21, IC31, IC41, IC51, IC61, IC71, IC81
  WRITE (UNIT) IC12, IC22, IC32, IC42, IC52, IC62, IC72, IC82
  WRITE (UNIT) AF11
  !
  ! --- one record per sea point ------------------------------------------
  !
  DO IPT=1, NPTS
    CALL DISPERSION ( DEPTHS(IPT), WN, CG )
    CALL SEA_STATE  ( DEPTHS(IPT), WN, A, KDMEAN )
    CALL W3SNL1_REF ( A, CG, KDMEAN, S, D )
    WRITE (UNIT) KDMEAN
    WRITE (UNIT) CG
    WRITE (UNIT) A
    WRITE (UNIT) S
    WRITE (UNIT) D
    WRITE (*,'(A,I2,A,F8.1,A,F9.4,A,E13.6,A,E13.6)')                  &
         ' point ', IPT, ': depth ', DEPTHS(IPT), ' m, kdmean ',      &
         KDMEAN, ', max|S| ', MAXVAL(ABS(S)), ', max|D| ', MAXVAL(ABS(D))
  END DO
  !
  CLOSE ( UNIT )
  WRITE (*,'(A,A)') ' wrote ', TRIM(PATH)
  !
  DEALLOCATE ( A, S, D, CG, WN )
  CALL FREE_REF
  !
END PROGRAM GEN_SNL1_FIXTURE
