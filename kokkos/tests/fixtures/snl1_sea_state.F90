!> @file snl1_sea_state.F90
!> @brief The deterministic sea state every W3SNL1 parity artefact is built on.
!>
!> The spectral grid, the linear dispersion solve and the JONSWAP x cos^2 action
!> spectrum that gen_snl1_fixture.F90 writes into the committed fixture, and that
!> shim_driver.F90 replays through the bind(C) shim. It lives in its own module so
!> the two programs cannot drift apart: a parity test that builds its own inputs
!> is a test of two spectra, not of one kernel.
!>
!> Nothing here is random and nothing is read from the environment -- the same
!> compiler flags must reproduce the same bytes. Moving this code out of
!> gen_snl1_fixture.F90 changed no expression and no order of operations, so
!> `just snl1-fixtures` still reproduces snl1_nk25_nth24.bin byte for byte.
!>
!> SPDX-License-Identifier: MIT
!>
MODULE SNL1_SEA_STATE
  USE SNL1_REF
  IMPLICIT NONE
  PUBLIC
  !
  ! The grid and the sea state. SETUP_REF(NK_REF, NTH_REF, XFR_REF, FREQ1_REF)
  ! reproduces the header of the committed fixture.
  !
  INTEGER, PARAMETER :: NK_REF   = 25
  INTEGER, PARAMETER :: NTH_REF  = 24
  INTEGER, PARAMETER :: NPTS_REF = 3
  REAL,    PARAMETER :: XFR_REF  = 1.1
  REAL,    PARAMETER :: FREQ1    = 0.04118
  REAL,    PARAMETER :: U10      = 10.0               ! m/s
  REAL,    PARAMETER :: FETCH    = 1.0E5              ! m
  REAL,    PARAMETER :: GAMMA_J  = 3.3
  REAL,    PARAMETER :: DEPTHS(NPTS_REF) = (/ 1000., 50., 10. /)
  !
CONTAINS
  !/ ------------------------------------------------------------------- /
  !> @brief Wavenumber and group velocity of the linear dispersion relation.
  !>
  !> Solves sigma^2 = g k tanh(k d) by 20 Newton steps from the deep-water
  !> guess, then CG = 0.5 (1 + 2kd/sinh(2kd)) sigma / k.
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
  !> @brief JONSWAP x cos^2 action spectrum and its energy-weighted mean kd.
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
END MODULE SNL1_SEA_STATE
