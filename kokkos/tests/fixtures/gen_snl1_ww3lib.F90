!> @file gen_snl1_ww3lib.F90
!> @brief Optional cross-check: the same fixture record, written by the *real*
!>  W3SNL1/INSNL1 linked out of a configured WW3 build.
!>
!> snl1_ref.F90 claims to be a verbatim copy of WAVEWATCH III's DIA. This program
!> is how that claim is tested rather than asserted: it drives the genuine
!> W3SNL1MD through the W3GDATMD/W3ADATMD/W3ODATMD set-up the model itself uses,
!> on exactly the same three sea states, and writes the same binary record. The
!> two files must compare byte-identical (`just l1-crosscheck`).
!>
!> It is built only when -DWW_WW3_BUILD_DIR points at a WW3 build whose switch
!> includes NL1; see the note in kokkos/README.md.
!>
!> Usage: gen_snl1_ww3lib <output-path>
!>
!> SPDX-License-Identifier: MIT
!>
PROGRAM GEN_SNL1_WW3LIB
  USE CONSTANTS, ONLY: GRAV, PI, TPI, TPIINV
  USE W3GDATMD, ONLY: W3NMOD, W3DIMS, W3SETG,                        &
       NK, NTH, NSPEC, SIG, DTH, XFR, LAM, FACHFE,                   &
       SNLC1, KDCON, KDMN, SNLS1, SNLS2, SNLS3
  USE W3ADATMD, ONLY: W3NAUX, W3SETA
  USE W3ODATMD, ONLY: W3NOUT, W3SETO
  USE W3SNL1MD, ONLY: INSNL1, W3SNL1
  IMPLICIT NONE
  !
  INTEGER, PARAMETER :: NK_USE   = 25
  INTEGER, PARAMETER :: NTH_USE  = 24
  INTEGER, PARAMETER :: NPTS     = 3
  INTEGER, PARAMETER :: MAGIC    = INT(z'534E4C31')   ! 'SNL1'
  INTEGER, PARAMETER :: NDSE     = 6
  INTEGER, PARAMETER :: NDST     = 6
  REAL,    PARAMETER :: XFR_USE  = 1.1
  REAL,    PARAMETER :: FREQ1    = 0.04118
  REAL,    PARAMETER :: U10      = 10.0
  REAL,    PARAMETER :: FETCH    = 1.0E5
  REAL,    PARAMETER :: GAMMA_J  = 3.3
  REAL,    PARAMETER :: DEPTHS(NPTS) = (/ 1000., 50., 10. /)
  !
  CHARACTER(LEN=512)  :: PATH
  INTEGER             :: UNIT, IPT, IK
  REAL                :: KDMEAN
  REAL, ALLOCATABLE   :: A(:), S(:), D(:), CG(:), WN(:)
  !
  IF ( COMMAND_ARGUMENT_COUNT() /= 1 ) THEN
    WRITE (*,*) 'usage: gen_snl1_ww3lib <output-path>'
    STOP 1
  END IF
  CALL GET_COMMAND_ARGUMENT ( 1, PATH )
  !
  ! Minimal model set-up: one grid, one aux set, one output set.
  !
  CALL W3NOUT ( NDSE, NDST )
  CALL W3SETO ( 1, NDSE, NDST )
  CALL W3NMOD ( 1, NDSE, NDST )
  CALL W3DIMS ( 1, NK_USE, NTH_USE, NDSE, NDST )
  CALL W3SETG ( 1, NDSE, NDST )
  CALL W3NAUX ( NDSE, NDST )
  CALL W3SETA ( 1, NDSE, NDST )
  !
  ! The same parameters SETUP_REF installs in snl1_ref.F90, written through the
  ! W3GDATMD pointers instead of into module variables of our own.
  !
  XFR    = XFR_USE
  DTH    = TPI / REAL(NTH)
  DO IK=1, NK
    SIG(IK) = TPI * FREQ1 * XFR**(IK-1)
  END DO
  SIG(0)    = SIG(1) / XFR
  SIG(NK+1) = SIG(NK) * XFR
  LAM    =  0.25
  SNLC1  =  2.50E7 / GRAV**4
  KDCON  =  0.75
  KDMN   =  0.50
  SNLS1  =  5.5
  SNLS2  =  0.833
  SNLS3  = -1.25
  FACHFE = XFR**(-5.)
  !
  CALL INSNL1 ( 1 )
  !
  ALLOCATE ( A(NSPEC), S(NSPEC), D(NSPEC), CG(NK), WN(NK) )
  !
  OPEN ( NEWUNIT=UNIT, FILE=TRIM(PATH), FORM='UNFORMATTED', &
       ACCESS='STREAM', STATUS='REPLACE', ACTION='WRITE' )
  !
  CALL WRITE_HEADER_AND_TABLES ( UNIT )
  !
  DO IPT=1, NPTS
    CALL DISPERSION ( DEPTHS(IPT), WN, CG )
    CALL SEA_STATE  ( DEPTHS(IPT), WN, A, KDMEAN )
    CALL W3SNL1 ( A, CG, KDMEAN, S, D )
    WRITE (UNIT) KDMEAN
    WRITE (UNIT) CG
    WRITE (UNIT) A
    WRITE (UNIT) S
    WRITE (UNIT) D
  END DO
  !
  CLOSE ( UNIT )
  WRITE (*,'(A,A)') ' wrote ', TRIM(PATH)
  DEALLOCATE ( A, S, D, CG, WN )
  !
CONTAINS
  !/ ------------------------------------------------------------------- /
  !> @brief The header and INSNL1 tables, in the fixture's field order.
  !>
  SUBROUTINE WRITE_HEADER_AND_TABLES ( IU )
    USE W3ADATMD, ONLY: NFR, NFRHGH, NFRCHG, NSPECX, NSPECY,          &
         DAL1, DAL2, DAL3, AF11,                                      &
         AWG1, AWG2, AWG3, AWG4, AWG5, AWG6, AWG7, AWG8,              &
         SWG1, SWG2, SWG3, SWG4, SWG5, SWG6, SWG7, SWG8,              &
         IP11, IP12, IP13, IP14, IM11, IM12, IM13, IM14,              &
         IP21, IP22, IP23, IP24, IM21, IM22, IM23, IM24,              &
         IC11, IC12, IC21, IC22, IC31, IC32, IC41, IC42,              &
         IC51, IC52, IC61, IC62, IC71, IC72, IC81, IC82
    IMPLICIT NONE
    INTEGER, INTENT(IN) :: IU
    WRITE (IU) MAGIC, NK, NTH, NPTS
    WRITE (IU) XFR, DTH, LAM, SNLC1, KDCON, KDMN, SNLS1, SNLS2, SNLS3, FACHFE
    WRITE (IU) SIG(1:NK)
    WRITE (IU) NFR, NFRHGH, NFRCHG, NSPECX, NSPECY
    WRITE (IU) DAL1, DAL2, DAL3
    WRITE (IU) AWG1, AWG2, AWG3, AWG4, AWG5, AWG6, AWG7, AWG8
    WRITE (IU) SWG1, SWG2, SWG3, SWG4, SWG5, SWG6, SWG7, SWG8
    WRITE (IU) IP11, IP12, IP13, IP14, IM11, IM12, IM13, IM14
    WRITE (IU) IP21, IP22, IP23, IP24, IM21, IM22, IM23, IM24
    WRITE (IU) IC11, IC21, IC31, IC41, IC51, IC61, IC71, IC81
    WRITE (IU) IC12, IC22, IC32, IC42, IC52, IC62, IC72, IC82
    WRITE (IU) AF11
  END SUBROUTINE WRITE_HEADER_AND_TABLES
  !/ ------------------------------------------------------------------- /
  !> @brief Wavenumber and group velocity; identical to gen_snl1_fixture.F90.
  !>
  SUBROUTINE DISPERSION ( DEPTH, WNO, CGO )
    IMPLICIT NONE
    REAL, INTENT(IN)  :: DEPTH
    REAL, INTENT(OUT) :: WNO(NK), CGO(NK)
    INTEGER           :: IK, IT
    REAL              :: SI, K, KD, F, FP
    DO IK=1, NK
      SI = SIG(IK)
      K  = SI*SI / GRAV
      DO IT=1, 20
        KD = MIN ( K*DEPTH, 30. )
        F  = GRAV*K*TANH(KD) - SI*SI
        FP = GRAV*TANH(KD) + GRAV*K*DEPTH/COSH(KD)**2
        K  = K - F/FP
      END DO
      KD      = MIN ( K*DEPTH, 30. )
      WNO(IK) = K
      CGO(IK) = 0.5 * ( 1. + 2.*KD/SINH(2.*KD) ) * SI / K
    END DO
  END SUBROUTINE DISPERSION
  !/ ------------------------------------------------------------------- /
  !> @brief JONSWAP x cos^2 action spectrum; identical to gen_snl1_fixture.F90.
  !>
  SUBROUTINE SEA_STATE ( DEPTH, WNI, AO, KDM )
    IMPLICIT NONE
    REAL, INTENT(IN)  :: DEPTH, WNI(NK)
    REAL, INTENT(OUT) :: AO(NSPEC), KDM
    INTEGER           :: IK, ITH, ISP
    REAL              :: XT, ALPHA, FP, SP, SI, SA, R, PM, EF
    REAL              :: TH, DD, SPREAD, DSIG, EBAND, ESUM, KDSUM
    !
    XT    = GRAV * FETCH / U10**2
    ALPHA = 0.076 * XT**(-0.22)
    FP    = 3.5 * ( GRAV / U10 ) * XT**(-0.33)
    SP    = TPI * FP
    !
    ESUM  = 0.
    KDSUM = 0.
    DO IK=1, NK
      SI    = SIG(IK)
      IF ( SI <= SP ) THEN
        SA = 0.07
      ELSE
        SA = 0.09
      END IF
      R     = EXP ( -(SI-SP)**2 / (2.*(SA*SP)**2) )
      PM    = ALPHA * GRAV**2 * SI**(-5) * EXP ( -1.25 * (SP/SI)**4 )
      EF    = PM * GAMMA_J**R
      DSIG  = SI * ( XFR - 1./XFR ) * 0.5
      EBAND = 0.
      DO ITH=1, NTH
        TH = REAL(ITH-1) * DTH
        DD = TH
        DO WHILE ( DD >  PI ) ; DD = DD - TPI ; END DO
        DO WHILE ( DD < -PI ) ; DD = DD + TPI ; END DO
        IF ( ABS(DD) >= 0.5*PI ) THEN
          SPREAD = 0.
        ELSE
          SPREAD = ( 2. / PI ) * COS(DD)**2
        END IF
        ISP     = ITH + (IK-1)*NTH
        AO(ISP) = EF * SPREAD / SI
        EBAND   = EBAND + EF * SPREAD * DTH * DSIG
      END DO
      ESUM  = ESUM  + EBAND
      KDSUM = KDSUM + EBAND * WNI(IK) * DEPTH
    END DO
    !
    KDM = KDSUM / ESUM
    !
  END SUBROUTINE SEA_STATE
  !
END PROGRAM GEN_SNL1_WW3LIB
