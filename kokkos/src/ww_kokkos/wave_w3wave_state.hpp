// kokkos/src/ww_kokkos/wave_w3wave_state.hpp
// What W3WAVE reads and writes, as the C++ driver of wave_w3wave.hpp takes it:
// per-grid scalars by value (Params), the module scalars and integer dates by
// address (Module), the module arrays as Kokkos Views (Arrays), and the
// parameter blocks of the translated callees (Callees). Phase 1, translated,
// not wired, not regtested.
//
// WW3 heritage: the USE lists of W3WAVE, WW3-noaa/model/src/w3wavemd.F90:409-533
// at NOAA-EMC/WW3 03adda91, as switches/switch_lab_shrd compiles it (SHRD, so
// NSEAL = NSEA and INIT_GET_ISEA gives ISEA = JSEA, w3parall.F90:1484), and
// the allocation shapes of w3gdatmd.F90:1733-1756, w3wdatmd.F90:491, 561-573
// and w3adatmd.F90:1004-1021, 1164-1252, 1290-1294, 1312-1325, 1344-1350,
// 1375-1393, 1450. Every index map below is written Fortran -> C++.
#pragma once

#include <Kokkos_Core.hpp>

#include "prop_cflxy.hpp"
#include "prop_ktp3.hpp"
#include "prop_map3.hpp"
#include "prop_params.hpp"
#include "real.hpp"
#include "snl1_config.hpp"
#include "snl1_tables.hpp"
#include "srce_kernel.hpp"
#include "srce_params.hpp"
#include "st4_arrays.hpp"
#include "st4_params.hpp"
#include "updt_utrn.hpp"

namespace ww::wave::w3wave {

// W3GDATMD grid types (w3gdatmd.F90 PARAMETERs RLGTYPE = 1, CLGTYPE = 2,
// UNGTYPE = 3, SMCTYPE = 4) and W3ODATMD's NOGRP, NGRPP (w3odatmd.F90:330-331).
inline constexpr int kUngtype = 3;
inline constexpr int kSmctype = 4;
inline constexpr int kNogrp = 10;
inline constexpr int kNgrpp = 20;

/// Per-grid scalars that do not change during one W3WAVE call, under WW3's
/// names in lower case. LOGICALs as 0/1. The shim fills it for grid IMOD.
struct Params {
  // W3GDATMD
  int nx, ny, nsea, nseal, nk, nth, nspec, gtype, iclose;
  int flcth, fsrefraction, flck, fsfreqshift, flagll, fldry, fstotalimp, flcx, flcy, flsou;
  Real dtmax, dtcfli, dth, dmin;
  Real sig_nk;  ///< SIG(NK), for FCUT = SIG(NK) * TPIINV (:753)
  // CONSTANTS (constants.F90:85, 102): variables, not PARAMETERs.
  Real undef;
  int lpdlib;
  // W3IDATMD
  int fllev, flcur, flwind, flice, fltaua, flrhoa, flic1;
  int inflags1_10;  ///< INFLAGS1(10), the moving-grid speed (:1164)
  // W3ODATMD process numbers and units
  int iaproc, naproc, naplog, napout, naperr, napfld, nappnt, napbpt, screen, ndso, notype;
};

/// Module scalars and small arrays, by address: the driver and the Fortran
/// hooks share one copy. Integer dates are int[2]; tonext and tolast are
/// (2,8) column major, TONEXT(I,J) = tonext[(J-1)*2 + I-1]; flogrd and flogr2
/// are (NOGRP,NGRPP), FLOGRD(I,J) = flogrd[(J-1)*kNogrp + I-1].
struct Module {
  // W3WDATMD
  int* time;
  const int* tlev;  ///< changed by W3ULEV
  const int* tice;  ///< changed by W3UICE
  const int* tic1;  ///< changed by W3UIC1
  // W3ADATMD
  int* itime;
  int* ipass;
  int* idlast;
  int* itstep;
  const int* flcold;
  int* fliwnd;
  // W3IDATMD
  const int *tln, *tc0, *tcn, *tw0, *twn, *tin, *tu0, *tun, *ti1, *tg0, *tgn, *tdn, *tr0, *trn;
  const Real *ga0, *gan, *gd0, *gdn;
  // W3ODATMD
  int* tofrst;
  int* tonext;
  const int* tolast;
  const Real* dtout;  ///< DTOUT(8)
  int* flout;         ///< FLOUT(8), LOGICAL
  const int* flbpi;   ///< changed by W3IOBC
  const int* tbpi0;
  const int* tbpin;
  const int* flogrd;
  const int* flogr2;
  const int* noge;  ///< NOGE(NOGRP)
  // W3ADATMD
  const int* iappro;  ///< IAPPRO(NSPEC): iappro[ISPEC-1]
  /// NMX0..NMY2, NACT, NCENT (W3ADATMD), written by W3MAP3 and kept across
  /// calls of W3WAVE, since FLMAP is true only on some steps (:741, :1353).
  prop::Map3Counts* map3_counts;
};

/// The module arrays, in the memory space of the execution space, LayoutLeft
/// (Fortran order). A(lb:ub) maps to a(I - lb).
template <class MemSpace>
struct Arrays {
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using R2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  using R3 = Kokkos::View<Real***, Kokkos::LayoutLeft, MemSpace>;
  using I1 = Kokkos::View<int*, Kokkos::LayoutLeft, MemSpace>;
  using I2 = Kokkos::View<int**, Kokkos::LayoutLeft, MemSpace>;

  // W3WDATMD
  R2 va;              ///< VA(NSPEC,0:NSEALM): va(ISPEC-1, JSEA)
  R1 ust, ustdir;     ///< UST(0:NSEATM): ust(ISEA)
  R1 asf;             ///< ASF(NSEATM): asf(ISEA-1)
  R1 rhoair;          ///< RHOAIR(NSEA): rhoair(ISEA-1)
  R1 ice;             ///< ICE(0:NSEA): ice(ISEA)
  R1 berg;            ///< BERG(NSEA): berg(ISEA-1)
  // W3ADATMD
  R1 dw, cx, cy, ua, ud, as, taua, tauadir;  ///< (0:NSEA): x(ISEA)
  R1 u10, u10d;                              ///< (NSEA): x(ISEA-1)
  R2 wn, cg;                                 ///< (0:NK+1,0:NSEA): x(IK, ISEA)
  /// DDDX..DCYDY (NY,NX), or (1,NSEAL) with LPDLIB (w3adatmd.F90:1375-1393):
  /// x(IY-1, IXrel-1).
  R2 dddx, dddy, dcxdx, dcxdy, dcydx, dcydy;
  R3 dcdx, dcdy;  ///< (0:NK+1,NY,NX): x(IK, IY-1, IXrel-1)
  /// (NSEALM): x(JSEA-1)
  R1 cflxymax, cflthmax, cflkmax, dtdyn, fcut;
  R1 tauox, tauoy, tauwix, tauwiy, tauwnx, tauwny, phiaw, charn, tws, phioc, phibbl, phice,
      tauocx, tauocy, wnmean;
  R2 whitecap;            ///< WHITECAP(NSEALM,4): whitecap(JSEA-1, K-1)
  R2 taubbl, tauice;      ///< (NSEALM,2)
  R1 ua0, uai, ud0, udi, as0, asi;  ///< (NSEA): x(ISEA-1), only with FLWIND
  // W3IDATMD
  R2 wx0, wy0, dt0, wxn, wyn, dtn;  ///< (NX,NY): x(IX-1, IY-1)
  // W3GDATMD
  I2 mapsta, mapfs;   ///< (NY,NX): x(IY-1, IX-1)
  I2 mapsf;           ///< MAPSF(NSEA,3): mapsf(ISEA-1, J-1)
  I1 flagst;          ///< FLAGST(NSEA), LOGICAL: flagst(ISEA-1)
  I1 iobp;            ///< IOBP(NX) of an unstructured grid: iobp(ISEA-1)
  R1 cthg0s;          ///< CTHG0S(0:NSEA): cthg0s(ISEA)
  R1 trnx, trny;      ///< TRNX(NY,NX) flat: trnx((IX-1)*NY + IY-1)
  R2 dpdx, dpdy, dqdx, dqdy, hpfac, hqfac;  ///< (NY,NX)
  // W3ADATMD, the PR3 maps (prop_map3.hpp, prop_mapt.hpp, updt_utrn.hpp)
  I1 mapx2, mapy2, mapaxy, mapcxy, mapth2, mapwn2, maptrn;
  I1 map3_work;      ///< prop::kMap3NCounts ints, W3MAP3's counters on the device
  R2 atrnx, atrny;   ///< ATRNX(NY*NX,-1:1): atrnx(IXY-1, K+1)
  R1 trix, triy;     ///< W3UTRN's locals TRIX, TRIY (NY*NX)
};

/// The parameter blocks of the translated callees, for this grid. The pointer
/// members of xyg, xys, ktg and sg point into the same Views as Arrays (the
/// shim builds them once per grid); the driver writes only the per-step fields
/// (the map counts of xyg, the DSEC21 results and FLBPI of xys).
struct Callees {
  prop::Map3Scalars map3;
  updt::UtrnScalars utrn;
  Real rwndc;  ///< W3GDATMD RWNDC, W3UWND's current factor
  prop::Params xyp;
  prop::Grid xyg;
  prop::Step xys;
  prop::Ktp3Params ktp;
  prop::Ktp3Grid ktg;
  // W3SRCE (srce_kernel.hpp), with the DIA and ST4 state of this grid.
  srce::Params sp;
  srce::Grid sg;
  st4::Params q;
  st4::Fields f;
  snl1::Config nc;
  snl1::Tables nt;
  snl1::RealView1D nsig;
  int inflags2_4;  ///< INFLAGS2(4)
  int flogrd_5_7;  ///< FLOGRD(5,7)
  int flogrd_5_8;  ///< FLOGRD(5,8)
  /// Teams of the source-term kernel; each team takes the points JSEA =
  /// t+1, t+1+league, ... with its own work slot. Any value >= 1 gives the
  /// same bits (the points are independent, the OMPG loop of :2241-2243).
  int srce_league;
};

}  // namespace ww::wave::w3wave
