// kokkos/src/ww_kokkos/prop_params.hpp
// The scalars and the grid and module arrays W3XYP3 reads, and how they cross
// the C ABI. Phase 1: host pointers, one spectral bin per call.
//
// WW3 heritage: WW3-noaa/model/src/w3pro3md.F90:641-1477 (W3XYP3, the USE lists
// at :796-805) at NOAA-EMC/WW3 03adda91, as switches/switch_lab_shrd compiles it.
// It reads W3GDATMD, W3WDATMD (TIME), W3ADATMD (the maps W3MAP3 and W3MAPT fill,
// CG, CX, CY, ATRNX/Y), W3IDATMD (FLCUR), W3ODATMD (boundary input) and
// CONSTANTS; it writes VQ and its own locals only. W3MAP3 and W3MAPT are not
// translated: their maps are copied from the Fortran after each FLMAP
// (w3wavemd.F90:1461-1467), never rebuilt (ADR-0011).
//
// As for ST4 and W3SRCE, the Fortran side packs the scalars into REAL(C_FLOAT)
// RPAR and INTEGER(C_INT) IPAR at the 0-based indices below; W3KOKKOSMD on the
// fork mirrors them, and the init call refuses a packet whose length differs.
#pragma once

#include <Kokkos_Core.hpp>

#include "real.hpp"
#include "st4_params.hpp"

namespace ww::prop {

// CONSTANTS as gfortran folds them into w3pro3md.o (constants.F90:62, 71, 78,
// 80): every operation on two REAL PARAMETERs rounded to REAL, which is what a
// float constexpr does too. The static_asserts are the bit values read from the
// -O3 GIMPLE of W3XYP3 (spec_W3XYP3.md, section 3.1).
inline constexpr Real kCg0Fac = static_cast<Real>(0.575) * ww::st4::kGrav;  // 0.575*GRAV, :882
inline constexpr Real kDera = ww::kPi / static_cast<Real>(180.);            // DERA, constants.F90:78
inline constexpr Real kRadius = static_cast<Real>(4.E7) * ww::st4::kTpiInv;  // RADIUS, constants.F90:80
inline constexpr Real kDeraRadius = kDera * kRadius;                        // DERA*RADIUS, :926
static_assert(kCg0Fac == 5.63844966888427734375e+0f, "0.575*GRAV as REAL, 0x40b46e2e");
static_assert(kDera == 1.745329238474369049072265625e-2f, "DERA as REAL, 0x3c8efa35");
static_assert(kRadius == 6.3661975e+6f, "RADIUS as REAL, 0x4ac247eb");
static_assert(kDeraRadius == 1.11111109375e+5f, "DERA*RADIUS as REAL, 0x47d9038e");

// Closure codes of W3GDATMD (w3gsrumd.F90:314-319). A tripole grid stops
// W3XYP3 with EXTCDE (:868-874): the caller refuses it before the kernel.
inline constexpr int kIcloseNone = -1;
inline constexpr int kIcloseSmpl = 2;
inline constexpr int kIcloseTrpl = 8;

/// RPAR indices (0-based). The comment names the Fortran variable packed there.
enum Rpar : int {
  R_DTCFL = 0,  // W3GDATMD DTCFL, the CFL time step of the grid (:923)
  R_DTH,        // W3GDATMD DTH (:961)
  R_XFR,        // W3GDATMD XFR (:962)
  R_WDCG,       // W3GDATMD WDCG, MPARS%PROPS (:962)
  R_WDTH,       // W3GDATMD WDTH (:961)
  NRPAR
};

/// IPAR indices (0-based).
enum Ipar : int {
  I_NX = 0,  // W3GDATMD NX
  I_NY,      // W3GDATMD NY
  I_NSEA,    // W3GDATMD NSEA
  I_NK,      // W3GDATMD NK, for the CG(0:NK+1,0:NSEA) stride
  I_NTH,     // W3GDATMD NTH
  I_NSPEC,   // W3GDATMD NSPEC, for the BBPI0(NSPEC,0:NBI) stride
  I_ICLOSE,  // W3GDATMD ICLOSE: -1, 2 or 8
  I_FLCX,    // W3GDATMD FLCX, 0/1
  I_FLCY,    // W3GDATMD FLCY, 0/1
  I_FLAGLL,  // W3GDATMD FLAGLL, 0/1
  I_FLCUR,   // W3IDATMD FLCUR, 0/1
  NIPAR
};

/// Everything above, unpacked, under WW3's names in lower case. Fixed per grid.
struct Params {
  int nx, ny, nsea, nk, nth, nspec;
  int iclose;
  int flcx, flcy, flagll, flcur;
  Real dtcfl, dth, xfr, wdcg, wdth;
};

/// RPAR/IPAR -> Params. The lengths are checked by the caller.
inline Params unpack(const float* r, const int* i) {
  Params p{};
  p.nx = i[I_NX];
  p.ny = i[I_NY];
  p.nsea = i[I_NSEA];
  p.nk = i[I_NK];
  p.nth = i[I_NTH];
  p.nspec = i[I_NSPEC];
  p.iclose = i[I_ICLOSE];
  p.flcx = i[I_FLCX];
  p.flcy = i[I_FLCY];
  p.flagll = i[I_FLAGLL];
  p.flcur = i[I_FLCUR];
  p.dtcfl = r[R_DTCFL];
  p.dth = r[R_DTH];
  p.xfr = r[R_XFR];
  p.wdcg = r[R_WDCG];
  p.wdth = r[R_WDTH];
  return p;
}

/// The grid and map arrays W3XYP3 reads, as the Fortran modules hold them:
/// 0-based pointers at the Fortran's first element, so a non-1 lower bound
/// becomes an offset in the index formula. Fixed per grid except the maps,
/// which change after each W3MAP3/W3MAPT/W3UTRN (w3wavemd.F90:1463-1467).
struct Grid {
  /// MAPSF(NSEA,3) (w3gdatmd.F90:1736, MSEA = NSEA at W3DIMX's call), column
  /// major: MAPSF(ISEA,J) = mapsf[(J-1)*nsea + ISEA-1]; J = 1 IX, 2 IY, 3 IXY.
  const int* mapsf;
  /// CLATS(0:NSEA) (w3gdatmd.F90:1741): CLATS(ISEA) = clats[ISEA]. Cosine of
  /// the latitude at sea points, 1 on a Cartesian grid (w3gridmd.F90:5031-5035).
  const Real* clats;
  /// SIG(0:NK+1) (w3gdatmd.F90): SIG(IK) = sig[IK]; SIG(1) and SIG(IK) read.
  const Real* sig;
  /// ECOS(:), ESIN(:) (w3gdatmd.F90:800): ECOS(ITH) = ecos[ITH-1], ITH <= NTH read.
  const Real* ecos;
  const Real* esin;
  /// DPDX, DPDY, DQDX, DQDY, GSQRT (NY,NX), lower bounds (1,1)
  /// (w3gdatmd.F90:1750-1754, filled by W3CGDM for RLGTYPE and CLGTYPE alike):
  /// X(IY,IX) = x[(IX-1)*ny + IY-1].
  const Real* dpdx;
  const Real* dpdy;
  const Real* dqdx;
  const Real* dqdy;
  const Real* gsqrt;
  /// ATRNX, ATRNY (NY*NX,-1:1) (w3adatmd.F90:1370-1371, filled by W3UTRN),
  /// pointer at ATRNX(1,-1): ATRNX(IXY,J) = atrnx[(J+1)*nx*ny + IXY-1]. Passed
  /// through to w3qck3 as TRANS; W3XYP3 itself never reads them.
  const Real* atrnx;
  const Real* atrny;
  /// MAPX2, MAPY2, MAPAXY (NY*NX), lower bound 1 (w3adatmd.F90:1439-1445),
  /// values 1-based IXY, filled by W3MAP3 with the counts below.
  const int* mapx2;
  const int* mapy2;
  const int* mapaxy;
  int nmx0, nmx1, nmx2;  ///< W3ADATMD NMX0, NMX1, NMX2: MAPX2 sections
  int nmy0, nmy1, nmy2;  ///< W3ADATMD NMY0, NMY1, NMY2: MAPY2 sections
  int nact;              ///< W3ADATMD NACT: entries of MAPAXY
  /// MAPCXY(NSEA) (w3adatmd.F90:524), values ISEA: 1..NCENT the central
  /// points, NCENT+1..NSEA the others (W3MAP3, w3pro3md.F90:255-392).
  const int* mapcxy;
  int ncent;  ///< W3ADATMD NCENT
  /// MAPTRN(NY*NX), LOGICAL (w3adatmd.F90:525) as int, nonzero true: the
  /// averaging is off at that cell (W3MAPT, w3pro3md.F90:615).
  const int* maptrn;
};

/// What changes between propagation steps, read from the modules by the
/// Fortran caller at w3wavemd.F90:1991 and passed per call. DSEC21 stays in
/// Fortran (W3TIMEMD): the three results W3XYP3 needs arrive as REAL.
struct Step {
  /// CG(0:NK+1,0:NSEA) (w3adatmd.F90:1325): CG(IK,ISEA) = cg[ISEA*(nk+2) + IK].
  const Real* cg;
  /// CX, CY (0:NSEA) (w3adatmd.F90:1008): CX(ISEA) = cx[ISEA]; read when FLCUR.
  const Real* cx;
  const Real* cy;
  /// DSEC21((/TIME(1), 0/), TIME) (:928-930): seconds since 00:00 of the
  /// current day, as the REAL DSEC21 returns; the kernel truncates it to DTTST.
  Real dsec_day;
  int flbpi;  ///< W3ODATMD FLBPI, 0/1
  int nbi;    ///< W3ODATMD NBI
  /// ISBPI(NBI) (w3odatmd.F90:416): ISBPI(IBI) = isbpi[IBI-1], the ISEA of
  /// boundary point IBI.
  const int* isbpi;
  /// BBPI0, BBPIN (NSPEC,0:NBI) (w3odatmd.F90:1486), pointer at BBPI0(1,0):
  /// BBPI0(ISP,IBI) = bbpi0[IBI*nspec + ISP-1].
  const Real* bbpi0;
  const Real* bbpin;
  Real dsec_bpi0;  ///< DSEC21(TBPI0, TIME) (:1396)
  Real dsec_bpin;  ///< DSEC21(TBPI0, TBPIN) (:1398)
};

}  // namespace ww::prop
