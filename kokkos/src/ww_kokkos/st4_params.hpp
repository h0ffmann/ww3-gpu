// kokkos/src/ww_kokkos/st4_params.hpp
// The scalars WAVEWATCH III's ST4 package reads, and how they cross the C ABI.
//
// WW3 heritage: WW3/model/src/w3src4md.F90 at WW3@bb08623a (W3SPR4 :142-384,
// W3SIN4 :423-1003, INSIN4 :1014-1288, TABU_STRESS :1290-1425, TABU_TAUHF
// :1427-1596, TABU_TAUHF2 :1598-1831, CALC_USTAR :1849-2011, W3SDS4 :2043-2659),
// which reads them from W3GDATMD (SRCP, SGRD, GRID), W3SRC4MD's own module
// variables (:62-70) and CONSTANTS (constants.F90:62-95).
//
// SRCP is a derived type, and no derived type crosses the boundary
// (docs/AGENTS_KOKKOS_202609.md §3.2): the Fortran side packs the scalars into
// REAL(C_FLOAT) RPAR(NRPAR) and INTEGER(C_INT) IPAR(NIPAR), at the 0-based
// indices below (RPAR(R_X+1) in Fortran). This header is the single source of the
// indices; W3KOKKOSMD on the fork branch mirrors them one for one, and
// ww_st4_init() refuses a packet whose length differs.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"

namespace ww::st4 {

// CONSTANTS, as constants.F90:62-95 builds them: each operation rounded to REAL,
// which is what both gfortran's PARAMETER folding and float constexpr do.
inline constexpr Real kGrav = static_cast<Real>(9.806);      // GRAV   :62
inline constexpr Real kDwat = static_cast<Real>(1000.);      // DWAT   :63
inline constexpr Real kNuAir = static_cast<Real>(1.4E-5);    // NU_AIR :65
inline constexpr Real kKappa = static_cast<Real>(0.40);      // KAPPA  :70
inline constexpr Real kPi = ww::kPi;                         // PI     :71
inline constexpr Real kTpi = static_cast<Real>(2.0) * kPi;   // TPI    :72
inline constexpr Real kTpiInv = static_cast<Real>(1.) / kTpi;  // TPIINV :74
inline constexpr Real kRade = static_cast<Real>(180.) / kPi;   // RADE   :76
inline constexpr int kSizeFwTable = 300;                     // SIZEFWTABLE :92
inline constexpr Real kAbMin = static_cast<Real>(-1.);       // ABMIN  :95

// W3SRC4MD's PARAMETERs (w3src4md.F90:61-71).
inline constexpr int kItauMax = 200, kJuMax = 200;                  // ITAUMAX, JUMAX
inline constexpr int kIustar = 100, kIalpha = 200, kIlevTail = 50;  // IUSTAR, IALPHA, ILEVTAIL
inline constexpr Real kUmax = static_cast<Real>(50.);               // UMAX
inline constexpr Real kTauwMax = static_cast<Real>(2.2361);         // TAUWMAX
inline constexpr int kNkhi = 100, kFacKd2 = 1000;                   // NKHI, FAC_KD2
inline constexpr Real kFacKd1 = static_cast<Real>(1.01);            // FAC_KD1
inline constexpr Real kKhsMax = static_cast<Real>(2.);              // KHSMAX
inline constexpr Real kKhMax = static_cast<Real>(2.);               // KHMAX
inline constexpr Real kKdMax = static_cast<Real>(200000.);          // KDMAX

// W3GDATMD's PARAMETERs (w3gdatmd.F90:637-638).
inline constexpr int kNkhs = 2000, kNkd = 1300, kNdtab = 2000;  // NKHS, NKD, NDTAB

/// RPAR indices (0-based). The comment names the Fortran variable packed there.
enum Rpar : int {
  R_DTH = 0,      // SGRD DTH
  R_XFR,          // SGRD XFR
  R_FTE,          // SGRD FTE
  R_FTF,          // SGRD FTF
  R_AAIRCMIN,     // GRID AAIRCMIN
  R_AAIRGB,       // GRID AAIRGB
  R_IGPARS5,      // IGPARS(5) under W3_IG1, else 0
  R_WWNMEANPTAIL, // SRCP WWNMEANPTAIL
  R_SSTXFTFTAIL,  // SRCP SSTXFTFTAIL
  R_AALPHA,
  R_BBETA,
  R_ZZ0MAX,
  R_ZZ0RAT,
  R_ZZALP,
  R_SSINTHP,
  R_TTAUWSHELTER,
  R_SSWELLF,  // SSWELLF(1:7): R_SSWELLF + 0..6
  R_SSDSC = R_SSWELLF + 7,  // SSDSC(1:21)
  R_SSDSBR = R_SSDSC + 21,
  R_SINTAILPAR,  // SINTAILPAR(1:5)
  R_SSDSP = R_SINTAILPAR + 5,
  R_WWNMEANP,
  R_SSTXFTF,
  R_SSTXFTWN,
  R_FFXPM,
  R_FFXFM,
  R_FFXFA,
  R_SSDSBRF1,
  R_SSDSBRF2,
  R_SSDSBINT,
  R_SSDSBCK,
  R_SSDSHCK,
  R_SSDSABK,
  R_SSDSPBK,
  R_SSINBR,
  R_CAPCHNK,  // CAPCHNK(1:10)
  R_ZZWND = R_CAPCHNK + 10,
  R_SSDSCOS,
  R_SSDSDTH,
  R_SSDSBT,
  R_SSDSBM,  // SSDSBM(0:4)
  R_DELUST = R_SSDSBM + 5,  // W3SRC4MD module variables (w3src4md.F90:65-66)
  R_DELALP,
  R_DELTAUW,
  R_DELU,
  R_DELTAIL,
  R_DELAB,  // CONSTANTS DELAB (constants.F90:94), set by TABU_FW
  NRPAR
};

/// IPAR indices (0-based).
enum Ipar : int {
  I_NK = 0,      // SGRD NK
  I_NTH,         // SGRD NTH
  I_NSPEC,       // SGRD NSPEC
  I_MK,          // W3GDATMD MK, the allocated frequency extent of the grid tables
  I_MTH,         // W3GDATMD MTH
  I_MSPEC,       // W3GDATMD MSPEC
  I_SSWELLFPAR,  // SRCP SSWELLFPAR
  I_SSDSISO,     // SRCP SSDSISO
  I_SSDSBRFDF,   // SRCP SSDSBRFDF
  I_STAB3,       // 1 when W3_STAB3 is compiled in
  I_IG1,         // 1 when W3_IG1 is compiled in
  I_FLX5,        // 1 when W3_FLX5 is compiled in: ww_st4_init refuses it
  NIPAR
};

/// Everything above, unpacked, under WW3's names in lower case. A plain
/// aggregate, captured by value in every kernel.
struct Params {
  int nk, nth, nspec, mk, mth, mspec;
  int sswellfpar, ssdsiso, ssdsbrfdf;
  int stab3, ig1;
  Real dth, xfr, fte, ftf;
  Real aaircmin, aairgb, igpars5;
  Real wwnmeanptail, sstxftftail;
  Real aalpha, bbeta, zz0max, zz0rat, zzalp, ssinthp, ttauwshelter;
  Real sswellf[7];     ///< SSWELLF(1:7): sswellf[i-1]
  Real ssdsc[21];      ///< SSDSC(1:21): ssdsc[i-1]
  Real ssdsbr;
  Real sintailpar[5];  ///< SINTAILPAR(1:5): sintailpar[i-1]
  Real ssdsp, wwnmeanp, sstxftf, sstxftwn, ffxpm, ffxfm, ffxfa;
  Real ssdsbrf1, ssdsbrf2, ssdsbint, ssdsbck, ssdshck, ssdsabk, ssdspbk, ssinbr;
  Real capchnk[10];    ///< CAPCHNK(1:10): capchnk[i-1]
  Real zzwnd, ssdscos, ssdsdth, ssdsbt;
  Real ssdsbm[5];      ///< SSDSBM(0:4): ssdsbm[i], the Fortran lower bound is 0
  Real delust, delalp, deltauw, delu, deltail;  ///< W3SRC4MD module scalars
  Real delab;                                   ///< CONSTANTS DELAB
};

/// RPAR/IPAR -> Params. The lengths are checked by the caller (ww_st4_init).
inline Params unpack(const float* r, const int* i) {
  Params p{};
  p.nk = i[I_NK];
  p.nth = i[I_NTH];
  p.nspec = i[I_NSPEC];
  p.mk = i[I_MK];
  p.mth = i[I_MTH];
  p.mspec = i[I_MSPEC];
  p.sswellfpar = i[I_SSWELLFPAR];
  p.ssdsiso = i[I_SSDSISO];
  p.ssdsbrfdf = i[I_SSDSBRFDF];
  p.stab3 = i[I_STAB3];
  p.ig1 = i[I_IG1];
  p.dth = r[R_DTH];
  p.xfr = r[R_XFR];
  p.fte = r[R_FTE];
  p.ftf = r[R_FTF];
  p.aaircmin = r[R_AAIRCMIN];
  p.aairgb = r[R_AAIRGB];
  p.igpars5 = r[R_IGPARS5];
  p.wwnmeanptail = r[R_WWNMEANPTAIL];
  p.sstxftftail = r[R_SSTXFTFTAIL];
  p.aalpha = r[R_AALPHA];
  p.bbeta = r[R_BBETA];
  p.zz0max = r[R_ZZ0MAX];
  p.zz0rat = r[R_ZZ0RAT];
  p.zzalp = r[R_ZZALP];
  p.ssinthp = r[R_SSINTHP];
  p.ttauwshelter = r[R_TTAUWSHELTER];
  for (int j = 0; j < 7; ++j) p.sswellf[j] = r[R_SSWELLF + j];
  for (int j = 0; j < 21; ++j) p.ssdsc[j] = r[R_SSDSC + j];
  p.ssdsbr = r[R_SSDSBR];
  for (int j = 0; j < 5; ++j) p.sintailpar[j] = r[R_SINTAILPAR + j];
  p.ssdsp = r[R_SSDSP];
  p.wwnmeanp = r[R_WWNMEANP];
  p.sstxftf = r[R_SSTXFTF];
  p.sstxftwn = r[R_SSTXFTWN];
  p.ffxpm = r[R_FFXPM];
  p.ffxfm = r[R_FFXFM];
  p.ffxfa = r[R_FFXFA];
  p.ssdsbrf1 = r[R_SSDSBRF1];
  p.ssdsbrf2 = r[R_SSDSBRF2];
  p.ssdsbint = r[R_SSDSBINT];
  p.ssdsbck = r[R_SSDSBCK];
  p.ssdshck = r[R_SSDSHCK];
  p.ssdsabk = r[R_SSDSABK];
  p.ssdspbk = r[R_SSDSPBK];
  p.ssinbr = r[R_SSINBR];
  for (int j = 0; j < 10; ++j) p.capchnk[j] = r[R_CAPCHNK + j];
  p.zzwnd = r[R_ZZWND];
  p.ssdscos = r[R_SSDSCOS];
  p.ssdsdth = r[R_SSDSDTH];
  p.ssdsbt = r[R_SSDSBT];
  for (int j = 0; j < 5; ++j) p.ssdsbm[j] = r[R_SSDSBM + j];
  p.delust = r[R_DELUST];
  p.delalp = r[R_DELALP];
  p.deltauw = r[R_DELTAUW];
  p.delu = r[R_DELU];
  p.deltail = r[R_DELTAIL];
  p.delab = r[R_DELAB];
  return p;
}

}  // namespace ww::st4
