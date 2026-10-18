// kokkos/src/fortran_iface/outg_c.hpp
// The C ABI of the outg wiring unit (WFIP-0005 unit 5): W3OUTG's integrals on the resident VA.
// Declarations only; outg_shim.cpp implements ww_outg_va, and model/src/w3kokkos_outgmd.F90 on the
// fork (h0ffmann/WW3 ww3-gpu/kokkos-outg) mirrors it. The rules of ww_kokkos_c.hpp hold: Fortran
// order, caller-owned arrays, float is WW3's REAL, nothing throws.
//
// SPDX-License-Identifier: MIT
#pragma once

extern "C" {

/// The packed integers of ww_outg_va, in this order (1-based in the Fortran).
enum WwOutgIpar {
  WW_OUTG_NK = 0,
  WW_OUTG_NTH,
  WW_OUTG_NSEAL,
  WW_OUTG_NSEA,     ///< rows of MAPSF, columns 0:NSEA of CG, WN and DW
  WW_OUTG_NX,       ///< MAPSTA(NY,NX)
  WW_OUTG_NY,
  WW_OUTG_NFLD,     ///< SIZE(HS): the extent of every 1-D field, and the first of the 2-D ones
  WW_OUTG_NOGRP,    ///< FLOGRD(NOGRP,NGRPP)
  WW_OUTG_NGRPP,
  WW_OUTG_FLOUTG,   ///< 1 or 0
  WW_OUTG_FLOUTG2,  ///< 1 or 0
  WW_OUTG_OARST,    ///< W3IORSMD's OARST: W3IORS ('HOT') writes the FLOGRR fields
  WW_OUTG_OMPG,     ///< 1 when WW3 is built with W3_OMPG (W3OutgScalars::ompg)
  WW_OUTG_P2MSF2,   ///< P2MSF(2), P2MSF(3)
  WW_OUTG_P2MSF3,
  WW_OUTG_E3DF2,    ///< E3DF(2,1:5), five entries
  WW_OUTG_E3DF3 = WW_OUTG_E3DF2 + 5,  ///< E3DF(3,1:5), five entries
  WW_OUTG_NIPAR = WW_OUTG_E3DF3 + 5
};

/// The packed reals of ww_outg_va, in this order.
enum WwOutgRpar {
  WW_OUTG_DTH = 0,
  WW_OUTG_XFR,
  WW_OUTG_FTE,
  WW_OUTG_FTWL,
  WW_OUTG_FTTR,
  WW_OUTG_UNDEF,
  WW_OUTG_STEXU,
  WW_OUTG_STEYU,
  WW_OUTG_STEDU,
  WW_OUTG_WSCUT,
  WW_OUTG_NRPAR
};

/// The output fields W3OUTG computes on Kokkos, the order of `fields` (W3ADATMD arrays). The
/// first WW_OUTG_N1D are (NFLD); EF .. STH2M are (NFLD, E3DF(2,k):E3DF(3,k)), P2SMS is
/// (NFLD, P2MSF(2):P2MSF(3)); a 2-D field the grid does not allocate is a null pointer.
enum WwOutgField {
  WW_OUTG_HS = 0, WW_OUTG_WLM, WW_OUTG_T02, WW_OUTG_T0M1, WW_OUTG_T01, WW_OUTG_FP0, WW_OUTG_THM,
  WW_OUTG_THS, WW_OUTG_THP0, WW_OUTG_HSIG, WW_OUTG_QKK, WW_OUTG_QP, WW_OUTG_WBT,
  WW_OUTG_STMAXE, WW_OUTG_STMAXD, WW_OUTG_HCMAXE, WW_OUTG_HMAXE, WW_OUTG_HCMAXD, WW_OUTG_HMAXD,
  WW_OUTG_ABA, WW_OUTG_ABD, WW_OUTG_UBA, WW_OUTG_UBD, WW_OUTG_SXX, WW_OUTG_SYY, WW_OUTG_SXY,
  WW_OUTG_USSX, WW_OUTG_USSY, WW_OUTG_TUSX, WW_OUTG_TUSY, WW_OUTG_MSSX, WW_OUTG_MSSY,
  WW_OUTG_MSSD, WW_OUTG_MSCX, WW_OUTG_MSCY, WW_OUTG_MSCD, WW_OUTG_PRMS, WW_OUTG_TPMS,
  WW_OUTG_BHD, WW_OUTG_CGE,
  WW_OUTG_N1D,
  WW_OUTG_EF = WW_OUTG_N1D, WW_OUTG_TH1M, WW_OUTG_STH1M, WW_OUTG_TH2M, WW_OUTG_STH2M,
  WW_OUTG_P2SMS,
  WW_OUTG_NFIELD
};

/// W3OUTG (VA, .FALSE., FLOUTG, FLOUTG2) of w3iogomd.F90:1201-2380, sections 1-4 and the
/// space-time extremes, on d_va of grid `igrid` (ww::resident::device_va: VA crosses only when the
/// host holds the newer bits). The caller (W3KOKKOS_OUTG) has checked that no field it leaves to
/// the Fortran is requested: the partitions (FLPART), CALC_U3STOKES, SKEWNESS, CALC_WBT. On a
/// backend that reads host memory the fields are written in place; on a device they stay in
/// device buffers and only those a host reader needs come back: the FLOGRD (FLOUTG) and FLOGR2
/// (FLOUTG2) entries W3IOGO writes, and the FLOGRR entries W3IORS ('HOT') writes when OARST.
/// flogrd, flogr2, flogrr are LOGICAL(4) (NOGRP,NGRPP); va is VA(1,0), nspec = SIZE(VA,1),
/// nsealm = UBOUND(VA,2); cg, wn are (0:NK+1,0:NSEA), dw (0:NSEA), sig (0:NK+1), dden and dsii
/// (NK), ecos .. esc of W3GDATMD (at least NTH), mapsf (NSEA,3), mapsta (NY,NX). Returns a
/// WwKokkosStatus.
int ww_outg_va(int igrid, const int* ipar, int nipar, const float* rpar, int nrpar,
               const int* flogrd, const int* flogr2, const int* flogrr, float* va, int nspec,
               int nsealm, const float* cg, const float* wn, const float* dw, const float* sig,
               const float* dden, const float* dsii, const float* ecos, const float* esin,
               const float* ec2, const float* es2, const float* esc, const int* mapsf,
               const int* mapsta, float* const* fields, int nfields);

/// The packed integers of ww_outg_part, in this order.
enum WwOutgPartIpar {
  WW_OUTGP_NSEAL = 0,
  WW_OUTGP_NSEA,     ///< rows of MAPSF
  WW_OUTGP_NX,       ///< MAPSTA(NY,NX)
  WW_OUTGP_NY,
  WW_OUTGP_NROW,     ///< SIZE(PWST) = NSEALM: the rows of every partition field
  WW_OUTGP_NOSWLL,   ///< NOSWLL: the partition fields are (NROW, 0:NOSWLL)
  WW_OUTGP_NICP,     ///< SIZE(ICPRT,1) = NSEALM+1
  WW_OUTGP_NDTP,     ///< SIZE(DTPRT,2), the columns of DTPRT(DIMP,:)
  WW_OUTGP_DIMP,     ///< DIMP, 15
  WW_OUTGP_NOGRP,    ///< FLOGRD(NOGRP,NGRPP)
  WW_OUTGP_NGRPP,
  WW_OUTGP_FLOUTG,   ///< 1 or 0
  WW_OUTGP_FLOUTG2,  ///< 1 or 0
  WW_OUTGP_NIPAR
};

/// The packed reals of ww_outg_part.
enum WwOutgPartRpar { WW_OUTGP_UNDEF = 0, WW_OUTGP_WSCUT, WW_OUTGP_NRPAR };

/// The partition fields of ww_outg_part, the order of `fields` (W3ADATMD arrays): the first
/// fifteen are (NROW, 0:NOSWLL), PWST and PNR (NROW). FLOGRD(4,k) names field k+1.
enum WwOutgPartField {
  WW_OUTGP_PHS = 0, WW_OUTGP_PTP, WW_OUTGP_PLP, WW_OUTGP_PDIR, WW_OUTGP_PSI, WW_OUTGP_PWS,
  WW_OUTGP_PTHP0, WW_OUTGP_PQP, WW_OUTGP_PPE, WW_OUTGP_PGW, WW_OUTGP_PSW, WW_OUTGP_PTM1,
  WW_OUTGP_PT1, WW_OUTGP_PT2, WW_OUTGP_PEP,
  WW_OUTGP_N2D,
  WW_OUTGP_PWST = WW_OUTGP_N2D, WW_OUTGP_PNR,
  WW_OUTGP_NFIELD
};

/// Section 6 of W3OUTG (w3iogomd.F90:2242-2340), the partition fields from W3CPRT's table, alone
/// (ww::iogo::w3outg_part). It reads no VA. W3KOKKOS_OUTG calls it after ww_outg_va when FLPART
/// is the only reason the Fortran W3OUTG would run (PATCH.md section 27). On a backend that reads
/// host memory every field is written in place; on a device ICPRT and DTPRT are copied in and only
/// the fields W3IOGO ('WRITE') writes (FLOGRD(4,k) under FLOUTG, FLOGR2(4,k) under FLOUTG2,
/// w3iogomd.F90:3283-3367) come back. mapsf (NSEA,3), mapsta (NY,NX), icprt (NICP,2), dtprt
/// (DIMP,NDTP), flogrd, flogr2 LOGICAL(4) (NOGRP,NGRPP). Returns a WwKokkosStatus.
int ww_outg_part(const int* ipar, int nipar, const float* rpar, int nrpar, const int* flogrd,
                 const int* flogr2, const int* mapsf, const int* mapsta, const int* icprt,
                 const float* dtprt, float* const* fields, int nfields);

}  // extern "C"
