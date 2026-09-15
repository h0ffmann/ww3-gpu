!> @file snl1_ref.F90
!> @brief Standalone reference copy of WAVEWATCH III's DIA nonlinear interactions.
!>
!> Purpose: produce the bit-for-bit Fortran answer that the C++/Kokkos port of
!> W3SNL1/INSNL1 is validated against, without linking the whole model. The
!> computational bodies below are the verbatim text of
!>
!>     WW3/model/src/w3snl1md.F90   (WAVEWATCH III 7.14, `develop`)
!>       W3SNL1 sections 1-4 ......... lines 338-440   -> W3SNL1_REF
!>       INSNL1 sections 1-9 ......... lines 602-774   -> INSNL1_REF
!>
!> The only edits made to that text are the ones a standalone copy forces:
!>   * the USE statements are gone -- what they imported are module variables here,
!>     spelled with the same WW3 names (NK, NTH, NSPEC, SIG, LAM, ... , AF11);
!>   * `CALL W3DMNL (...)` (INSNL1 section 6) is replaced by the ALLOCATE it
!>     performs in W3ADATMD, i.e. IP11..IM24 and AF11 of size NSPECX, IC11..IC82
!>     of size NSPEC;
!>   * the `#ifdef W3_T*` test output, tracing and the trailing `RETURN` are gone.
!> No expression, no order of operations and no intrinsic was changed: this file
!> is a transcription, not a reimplementation.
!>
!> SETUP_REF fills the W3GDATMD-side parameters with the WW3 defaults of
!> `w3gridmd.F90`. The lab's switch files use ST4 (see switches/switch_lab_shrd),
!> hence NLPROP = 2.50E7 (w3gridmd.F90:1888) and FACHF = 5. (w3gridmd.F90:1946),
!> giving SNLC1 = NLPROP/GRAV**4 (w3gridmd.F90:1915) and
!> FACHFE = XFR**(-FACHF) (w3gridmd.F90:3565). GRAV, PI and TPIINV are the
!> single-precision PARAMETERs of constants.F90 lines 62, 72 and 75.
!>
!> SPDX-License-Identifier: LGPL-3.0-or-later
!>
MODULE SNL1_REF
  IMPLICIT NONE
  PUBLIC
  !
  ! CONSTANTS (constants.F90 lines 62, 72-75), default REAL as in WW3.
  !
  REAL, PARAMETER :: GRAV   = 9.806
  REAL, PARAMETER :: PI     = 3.141592653589793
  REAL, PARAMETER :: TPI    = 2.0 * PI
  REAL, PARAMETER :: TPIINV = 1. / TPI
  !
  ! W3GDATMD side: the grid and the SNL1 namelist parameters.
  !
  INTEGER              :: NK, NTH, NSPEC
  REAL                 :: XFR, DTH, LAM, FACHFE
  REAL                 :: KDCON, KDMN, SNLC1, SNLS1, SNLS2, SNLS3
  REAL,    ALLOCATABLE :: SIG(:)
  !
  ! W3ADATMD side: everything INSNL1 precomputes.
  !
  INTEGER              :: NFR, NFRHGH, NFRCHG, NSPECX, NSPECY
  REAL                 :: DAL1, DAL2, DAL3
  REAL                 :: AWG1, AWG2, AWG3, AWG4, AWG5, AWG6, AWG7, AWG8
  REAL                 :: SWG1, SWG2, SWG3, SWG4, SWG5, SWG6, SWG7, SWG8
  INTEGER, ALLOCATABLE :: IP11(:), IP12(:), IP13(:), IP14(:)
  INTEGER, ALLOCATABLE :: IM11(:), IM12(:), IM13(:), IM14(:)
  INTEGER, ALLOCATABLE :: IP21(:), IP22(:), IP23(:), IP24(:)
  INTEGER, ALLOCATABLE :: IM21(:), IM22(:), IM23(:), IM24(:)
  INTEGER, ALLOCATABLE :: IC11(:), IC12(:), IC21(:), IC22(:)
  INTEGER, ALLOCATABLE :: IC31(:), IC32(:), IC41(:), IC42(:)
  INTEGER, ALLOCATABLE :: IC51(:), IC52(:), IC61(:), IC62(:)
  INTEGER, ALLOCATABLE :: IC71(:), IC72(:), IC81(:), IC82(:)
  REAL,    ALLOCATABLE :: AF11(:)
  !
CONTAINS
  !/ ------------------------------------------------------------------- /
  !> @brief Fill the W3GDATMD-side parameters with WW3's ST4/NL1 defaults.
  !>
  !> Not part of WW3: this stands in for W3GRID's namelist processing, so the
  !> reference can run without a mod_def file. SIG follows the geometric grid
  !> WW3 builds in w3gridmd.F90 (SIG(IK) = TPI*FR1*XFR**(IK-1)).
  !>
  SUBROUTINE SETUP_REF ( NK_IN, NTH_IN, XFR_IN, FREQ1 )
    IMPLICIT NONE
    INTEGER, INTENT(IN) :: NK_IN, NTH_IN
    REAL,    INTENT(IN) :: XFR_IN, FREQ1
    INTEGER             :: IK
    !
    NK     = NK_IN
    NTH    = NTH_IN
    NSPEC  = NK * NTH
    XFR    = XFR_IN
    DTH    = TPI / REAL(NTH)
    !
    IF ( ALLOCATED(SIG) ) DEALLOCATE ( SIG )
    ALLOCATE ( SIG(NK) )
    DO IK=1, NK
      SIG(IK) = TPI * FREQ1 * XFR**(IK-1)
    END DO
    !
    ! &SNL1 defaults, w3gridmd.F90 lines 1884-1898 with the ST4 NLPROP.
    !
    LAM    =  0.25
    SNLC1  =  2.50E7 / GRAV**4
    KDCON  =  0.75
    KDMN   =  0.50
    SNLS1  =  5.5
    SNLS2  =  0.833
    SNLS3  = -1.25
    !
    ! FACHF = 5. for ST4 (w3gridmd.F90:1946); FACHFE per w3gridmd.F90:3565.
    !
    FACHFE = XFR**(-5.)
    !
  END SUBROUTINE SETUP_REF
  !/ ------------------------------------------------------------------- /
  !> @brief Preprocessing for nonlinear interactions (weights).
  !>
  !> Verbatim body of INSNL1, w3snl1md.F90 lines 602-774 (sections 1-9).
  !>
  SUBROUTINE INSNL1_REF
    IMPLICIT NONE
    !
    INTEGER                 :: IFR, ITH, ISP, ITHP, ITHP1, ITHM,    &
         ITHM1,IFRP, IFRP1, IFRM, IFRM1
    INTEGER, ALLOCATABLE    :: IF1(:), IF2(:), IF3(:), IF4(:),      &
         IF5(:), IF6(:), IF7(:), IF8(:),      &
         IT1(:), IT2(:), IT3(:), IT4(:),      &
         IT5(:), IT6(:), IT7(:), IT8(:)
    REAL                    :: DELTH3, DELTH4, LAMM2, LAMP2, CTHP,  &
         WTHP, WTHP1, CTHM, WTHM, WTHM1,      &
         XFRLN, WFRP, WFRP1, WFRM, WFRM1, FR, &
         AF11A
    !
    NFR     = NK
    !
    ! 1.  Internal angles of quadruplet.
    !
    LAMM2  = (1.-LAM)**2
    LAMP2  = (1.+LAM)**2
    DELTH3 = ACOS( (LAMM2**2+4.-LAMP2**2) / (4.*LAMM2) )
    DELTH4 = ASIN(-SIN(DELTH3)*LAMM2/LAMP2)
    !
    ! 2.  Lambda dependend weight factors.
    !
    DAL1   = 1. / (1.+LAM)**4
    DAL2   = 1. / (1.-LAM)**4
    DAL3   = 2. * DAL1 * DAL2
    !
    ! 3.  Directional indices.
    !
    CTHP   = ABS(DELTH4/DTH)
    ITHP   = INT(CTHP)
    ITHP1  = ITHP + 1
    WTHP   = CTHP - REAL(ITHP)
    WTHP1  = 1.- WTHP
    !
    CTHM   = ABS(DELTH3/DTH)
    ITHM   = INT(CTHM)
    ITHM1  = ITHM + 1
    WTHM   = CTHM - REAL(ITHM)
    WTHM1  = 1.- WTHM
    !
    ! 4.  Frequency indices.
    !
    XFRLN  = LOG(XFR)
    !
    IFRP   = INT( LOG(1.+LAM) / XFRLN )
    IFRP1  = IFRP + 1
    WFRP   = (1.+LAM - XFR**IFRP) / (XFR**IFRP1 - XFR**IFRP)
    WFRP1  = 1. - WFRP
    !
    IFRM   = INT( LOG(1.-LAM) / XFRLN )
    IFRM1  = IFRM - 1
    WFRM   = (XFR**IFRM -(1.-LAM)) / (XFR**IFRM - XFR**IFRM1)
    WFRM1  = 1. - WFRM
    !
    ! 5.  Range of calculations
    !
    NFRHGH = NFR + IFRP1 - IFRM1
    NFRCHG = NFR - IFRM1
    NSPECY = NFRHGH * NTH
    NSPECX = NFRCHG * NTH
    !
    ! 6.  Allocate arrays or check array sizes
    !
    !     Stands in for CALL W3DMNL (IMOD, NDSE, NDST, NSPEC, NSPECX): the
    !     allocation W3DMNL performs on the W3ADATMD tables, nothing else.
    !
    CALL DMNL_REF
    !
    ALLOCATE ( IF1(NFRCHG), IF2(NFRCHG), IF3(NFRCHG), IF4(NFRCHG),  &
         IF5(NFRCHG), IF6(NFRCHG), IF7(NFRCHG), IF8(NFRCHG),  &
         IT1(NTH), IT2(NTH), IT3(NTH), IT4(NTH),              &
         IT5(NTH), IT6(NTH), IT7(NTH), IT8(NTH) )
    !
    ! 7.  Spectral addresses
    !
    DO IFR=1, NFRCHG
      IF1(IFR) =           IFR+IFRP
      IF2(IFR) =           IFR+IFRP1
      IF3(IFR) = MAX ( 0 , IFR+IFRM  )
      IF4(IFR) = MAX ( 0 , IFR+IFRM1 )
      IF5(IFR) = MAX ( 0 , IFR-IFRP  )
      IF6(IFR) = MAX ( 0 , IFR-IFRP1 )
      IF7(IFR) =           IFR-IFRM
      IF8(IFR) =           IFR-IFRM1
    END DO
    !
    DO ITH=1, NTH
      IT1(ITH) = ITH + ITHP
      IT2(ITH) = ITH + ITHP1
      IT3(ITH) = ITH + ITHM
      IT4(ITH) = ITH + ITHM1
      IT5(ITH) = ITH - ITHP
      IT6(ITH) = ITH - ITHP1
      IT7(ITH) = ITH - ITHM
      IT8(ITH) = ITH - ITHM1
      IF ( IT1(ITH).GT.NTH) IT1(ITH) = IT1(ITH) - NTH
      IF ( IT2(ITH).GT.NTH) IT2(ITH) = IT2(ITH) - NTH
      IF ( IT3(ITH).GT.NTH) IT3(ITH) = IT3(ITH) - NTH
      IF ( IT4(ITH).GT.NTH) IT4(ITH) = IT4(ITH) - NTH
      IF ( IT5(ITH).LT. 1 ) IT5(ITH) = IT5(ITH) + NTH
      IF ( IT6(ITH).LT. 1 ) IT6(ITH) = IT6(ITH) + NTH
      IF ( IT7(ITH).LT. 1 ) IT7(ITH) = IT7(ITH) + NTH
      IF ( IT8(ITH).LT. 1 ) IT8(ITH) = IT8(ITH) + NTH
    END DO
    !
    DO ISP=1, NSPECX
      IFR       = 1 + (ISP-1)/NTH
      ITH       = 1 + MOD(ISP-1,NTH)
      IP11(ISP) = IT2(ITH) + (IF2(IFR)-1)*NTH
      IP12(ISP) = IT1(ITH) + (IF2(IFR)-1)*NTH
      IP13(ISP) = IT2(ITH) + (IF1(IFR)-1)*NTH
      IP14(ISP) = IT1(ITH) + (IF1(IFR)-1)*NTH
      IM11(ISP) = IT8(ITH) + (IF4(IFR)-1)*NTH
      IM12(ISP) = IT7(ITH) + (IF4(IFR)-1)*NTH
      IM13(ISP) = IT8(ITH) + (IF3(IFR)-1)*NTH
      IM14(ISP) = IT7(ITH) + (IF3(IFR)-1)*NTH
      IP21(ISP) = IT6(ITH) + (IF2(IFR)-1)*NTH
      IP22(ISP) = IT5(ITH) + (IF2(IFR)-1)*NTH
      IP23(ISP) = IT6(ITH) + (IF1(IFR)-1)*NTH
      IP24(ISP) = IT5(ITH) + (IF1(IFR)-1)*NTH
      IM21(ISP) = IT4(ITH) + (IF4(IFR)-1)*NTH
      IM22(ISP) = IT3(ITH) + (IF4(IFR)-1)*NTH
      IM23(ISP) = IT4(ITH) + (IF3(IFR)-1)*NTH
      IM24(ISP) = IT3(ITH) + (IF3(IFR)-1)*NTH
    END DO
    !
    DO ISP=1, NSPEC
      IFR       = 1 + (ISP-1)/NTH
      ITH       = 1 + MOD(ISP-1,NTH)
      IC11(ISP) = IT6(ITH) + (IF6(IFR)-1)*NTH
      IC21(ISP) = IT5(ITH) + (IF6(IFR)-1)*NTH
      IC31(ISP) = IT6(ITH) + (IF5(IFR)-1)*NTH
      IC41(ISP) = IT5(ITH) + (IF5(IFR)-1)*NTH
      IC51(ISP) = IT4(ITH) + (IF8(IFR)-1)*NTH
      IC61(ISP) = IT3(ITH) + (IF8(IFR)-1)*NTH
      IC71(ISP) = IT4(ITH) + (IF7(IFR)-1)*NTH
      IC81(ISP) = IT3(ITH) + (IF7(IFR)-1)*NTH
      IC12(ISP) = IT2(ITH) + (IF6(IFR)-1)*NTH
      IC22(ISP) = IT1(ITH) + (IF6(IFR)-1)*NTH
      IC32(ISP) = IT2(ITH) + (IF5(IFR)-1)*NTH
      IC42(ISP) = IT1(ITH) + (IF5(IFR)-1)*NTH
      IC52(ISP) = IT8(ITH) + (IF8(IFR)-1)*NTH
      IC62(ISP) = IT7(ITH) + (IF8(IFR)-1)*NTH
      IC72(ISP) = IT8(ITH) + (IF7(IFR)-1)*NTH
      IC82(ISP) = IT7(ITH) + (IF7(IFR)-1)*NTH
    END DO
    !
    DEALLOCATE ( IF1, IF2, IF3, IF4, IF5, IF6, IF7, IF8,  &
         IT1, IT2, IT3, IT4, IT5, IT6, IT7, IT8 )
    !
    ! 8.  Fill scaling array (f**11)
    !
    DO IFR=1, NFR
      AF11A  = (SIG(IFR)*TPIINV)**11
      DO ITH=1, NTH
        AF11(ITH+(IFR-1)*NTH) = AF11A
      END DO
    END DO
    !
    FR     = SIG(NFR)*TPIINV
    DO IFR=NFR+1, NFRCHG
      FR     = FR * XFR
      AF11A  = FR**11
      DO ITH=1, NTH
        AF11(ITH+(IFR-1)*NTH) = AF11A
      END DO
    END DO
    !
    ! 9.  Interpolation weights
    !
    AWG1   = WTHP  * WFRP
    AWG2   = WTHP1 * WFRP
    AWG3   = WTHP  * WFRP1
    AWG4   = WTHP1 * WFRP1
    AWG5   = WTHM  * WFRM
    AWG6   = WTHM1 * WFRM
    AWG7   = WTHM  * WFRM1
    AWG8   = WTHM1 * WFRM1
    !
    SWG1   = AWG1**2
    SWG2   = AWG2**2
    SWG3   = AWG3**2
    SWG4   = AWG4**2
    SWG5   = AWG5**2
    SWG6   = AWG6**2
    SWG7   = AWG7**2
    SWG8   = AWG8**2
    !
  END SUBROUTINE INSNL1_REF
  !/ ------------------------------------------------------------------- /
  !> @brief The allocation W3DMNL (w3adatmd.F90) performs for the SNL1 tables.
  !>
  SUBROUTINE DMNL_REF
    IMPLICIT NONE
    ALLOCATE ( IP11(NSPECX), IP12(NSPECX), IP13(NSPECX), IP14(NSPECX),  &
         IM11(NSPECX), IM12(NSPECX), IM13(NSPECX), IM14(NSPECX),  &
         IP21(NSPECX), IP22(NSPECX), IP23(NSPECX), IP24(NSPECX),  &
         IM21(NSPECX), IM22(NSPECX), IM23(NSPECX), IM24(NSPECX),  &
         IC11(NSPEC),  IC12(NSPEC),  IC21(NSPEC),  IC22(NSPEC),   &
         IC31(NSPEC),  IC32(NSPEC),  IC41(NSPEC),  IC42(NSPEC),   &
         IC51(NSPEC),  IC52(NSPEC),  IC61(NSPEC),  IC62(NSPEC),   &
         IC71(NSPEC),  IC72(NSPEC),  IC81(NSPEC),  IC82(NSPEC),   &
         AF11(NSPECX) )
  END SUBROUTINE DMNL_REF
  !/ ------------------------------------------------------------------- /
  !> @brief Calculate nonlinear interactions and the diagonal term of
  !>  its derivative.
  !>
  !> Verbatim body of W3SNL1, w3snl1md.F90 lines 338-440 (sections 1-4),
  !> preceded by its own local declarations (lines 305-326).
  !>
  SUBROUTINE W3SNL1_REF ( A, CG, KDMEAN, S, D )
    IMPLICIT NONE
    !
    REAL, INTENT(IN)        :: A(NSPEC), CG(NK), KDMEAN
    REAL, INTENT(OUT)       :: S(NSPEC), D(NSPEC)
    !
    INTEGER                 :: ITH, IFR, ISP
    REAL                    :: X, X2, CONS, CONX, FACTOR,           &
         E00, EP1, EM1, EP2, EM2,             &
         SA1A, SA1B, SA2A, SA2B
    REAL               ::  UE  (1-NTH:NSPECY), SA1 (1-NTH:NSPECX),  &
         SA2 (1-NTH:NSPECX), DA1C(1-NTH:NSPECX),  &
         DA1P(1-NTH:NSPECX), DA1M(1-NTH:NSPECX),  &
         DA2C(1-NTH:NSPECX), DA2P(1-NTH:NSPECX),  &
         DA2M(1-NTH:NSPECX), CON (      NSPEC )
    !
    ! 1.  Calculate prop. constant --------------------------------------- *
    !
    X      = MAX ( KDCON*KDMEAN , KDMN )
    X2     = MAX ( -1.E15, SNLS3*X)
    CONS   = SNLC1 * ( 1. + SNLS1/X * (1.-SNLS2*X) * EXP(X2) )
    !
    ! 2.  Prepare auxiliary spectrum and arrays -------------------------- *
    !
    DO IFR=1, NFR
      CONX = TPIINV / SIG(IFR) * CG(IFR)
      DO ITH=1, NTH
        ISP       = ITH + (IFR-1)*NTH
        UE (ISP) = A(ISP) / CONX
        CON(ISP) = CONX
      END DO
    END DO
    !
    DO IFR=NFR+1, NFRHGH
      DO ITH=1, NTH
        ISP      = ITH + (IFR-1)*NTH
        UE(ISP) = UE(ISP-NTH) * FACHFE
      END DO
    END DO
    !
    DO ISP=1-NTH, 0
      UE  (ISP) = 0.
      SA1 (ISP) = 0.
      SA2 (ISP) = 0.
      DA1C(ISP) = 0.
      DA1P(ISP) = 0.
      DA1M(ISP) = 0.
      DA2C(ISP) = 0.
      DA2P(ISP) = 0.
      DA2M(ISP) = 0.
    END DO
    !
    ! 3.  Calculate interactions for extended spectrum ------------------- *
    !
    DO ISP=1, NSPECX
      !
      ! 3.a Energy at interacting bins
      !
      E00    =        UE(ISP)
      EP1    = AWG1 * UE(IP11(ISP)) + AWG2 * UE(IP12(ISP))        &
           + AWG3 * UE(IP13(ISP)) + AWG4 * UE(IP14(ISP))
      EM1    = AWG5 * UE(IM11(ISP)) + AWG6 * UE(IM12(ISP))        &
           + AWG7 * UE(IM13(ISP)) + AWG8 * UE(IM14(ISP))
      EP2    = AWG1 * UE(IP21(ISP)) + AWG2 * UE(IP22(ISP))        &
           + AWG3 * UE(IP23(ISP)) + AWG4 * UE(IP24(ISP))
      EM2    = AWG5 * UE(IM21(ISP)) + AWG6 * UE(IM22(ISP))        &
           + AWG7 * UE(IM23(ISP)) + AWG8 * UE(IM24(ISP))
      !
      ! 3.b Contribution to interactions
      !
      FACTOR = CONS * AF11(ISP) * E00
      !
      SA1A   = E00 * ( EP1*DAL1 + EM1*DAL2 )
      SA1B   = SA1A - EP1*EM1*DAL3
      SA2A   = E00 * ( EP2*DAL1 + EM2*DAL2 )
      SA2B   = SA2A - EP2*EM2*DAL3
      !
      SA1 (ISP) = FACTOR * SA1B
      SA2 (ISP) = FACTOR * SA2B
      !
      DA1C(ISP) = CONS * AF11(ISP) * ( SA1A + SA1B )
      DA1P(ISP) = FACTOR * ( DAL1*E00 - DAL3*EM1 )
      DA1M(ISP) = FACTOR * ( DAL2*E00 - DAL3*EP1 )
      !
      DA2C(ISP) = CONS * AF11(ISP) * ( SA2A + SA2B )
      DA2P(ISP) = FACTOR * ( DAL1*E00 - DAL3*EM2 )
      DA2M(ISP) = FACTOR * ( DAL2*E00 - DAL3*EP2 )
      !
    END DO
    !
    ! 4.  Put source and diagonal term together -------------------------- *
    !
    DO ISP=1, NSPEC
      !
      S(ISP) = CON(ISP) * ( - 2. * ( SA1(ISP) + SA2(ISP) )       &
           + AWG1 * ( SA1(IC11(ISP)) + SA2(IC12(ISP)) )    &
           + AWG2 * ( SA1(IC21(ISP)) + SA2(IC22(ISP)) )    &
           + AWG3 * ( SA1(IC31(ISP)) + SA2(IC32(ISP)) )    &
           + AWG4 * ( SA1(IC41(ISP)) + SA2(IC42(ISP)) )    &
           + AWG5 * ( SA1(IC51(ISP)) + SA2(IC52(ISP)) )    &
           + AWG6 * ( SA1(IC61(ISP)) + SA2(IC62(ISP)) )    &
           + AWG7 * ( SA1(IC71(ISP)) + SA2(IC72(ISP)) )    &
           + AWG8 * ( SA1(IC81(ISP)) + SA2(IC82(ISP)) ) )
      !
      D(ISP) =  - 2. * ( DA1C(ISP) + DA2C(ISP) )                 &
           + SWG1 * ( DA1P(IC11(ISP)) + DA2P(IC12(ISP)) )     &
           + SWG2 * ( DA1P(IC21(ISP)) + DA2P(IC22(ISP)) )     &
           + SWG3 * ( DA1P(IC31(ISP)) + DA2P(IC32(ISP)) )     &
           + SWG4 * ( DA1P(IC41(ISP)) + DA2P(IC42(ISP)) )     &
           + SWG5 * ( DA1M(IC51(ISP)) + DA2M(IC52(ISP)) )     &
           + SWG6 * ( DA1M(IC61(ISP)) + DA2M(IC62(ISP)) )     &
           + SWG7 * ( DA1M(IC71(ISP)) + DA2M(IC72(ISP)) )     &
           + SWG8 * ( DA1M(IC81(ISP)) + DA2M(IC82(ISP)) )
      !
    END DO
    !
  END SUBROUTINE W3SNL1_REF
  !/ ------------------------------------------------------------------- /
  !> @brief Release everything SETUP_REF and INSNL1_REF allocated.
  !>
  !> Not part of WW3 (the model keeps its tables for the whole run); it exists so
  !> the generator can exit clean under the serial-debug preset's LeakSanitizer.
  !>
  SUBROUTINE FREE_REF
    IMPLICIT NONE
    IF ( ALLOCATED(SIG) ) DEALLOCATE ( SIG )
    IF ( ALLOCATED(AF11) ) DEALLOCATE (                                 &
         IP11, IP12, IP13, IP14, IM11, IM12, IM13, IM14,                &
         IP21, IP22, IP23, IP24, IM21, IM22, IM23, IM24,                &
         IC11, IC12, IC21, IC22, IC31, IC32, IC41, IC42,                &
         IC51, IC52, IC61, IC62, IC71, IC72, IC81, IC82, AF11 )
  END SUBROUTINE FREE_REF
  !
END MODULE SNL1_REF
