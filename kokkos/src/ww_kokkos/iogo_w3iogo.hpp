// kokkos/src/ww_kokkos/iogo_w3iogo.hpp
// W3IOGO, ported: writes and reads the gridded output file out_grd.<ext> record by record: the
// header, then per output time the time and the FLOGRD flags, the packed map MAPSTA + 8*MAPST2
// and one or more records per requested field; on a WRITE it first sets the fields of land and
// ice points to UNDEF, and it turns the wind, the momentum flux, the friction velocity and the
// bottom amplitude and velocity into Cartesian components. Phase 1, translated, not wired into
// WW3, not regtested.
// WW3 heritage: WW3-noaa/model/src/w3iogomd.F90:2395-4097 at NOAA-EMC/WW3 03adda91 (identical
// in the h0ffmann/WW3 fork at the WW3/ pin 8e433f99: `git diff 03adda91 HEAD --
// model/src/w3iogomd.F90` is empty), author H. L. Tolman and the authors of its history block.
// Evidence: kokkos/port_report/W3IOGO_KOKKOS.md.
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Ported: every statement switches/switch_lab_shrd compiles (gfortran -E with the defines of
// WW3's flags.make). Not compiled under it, not ported: W3_ASCII (the NDSOA text copy and its
// OPENs, :2654-2657, :2743-2746, and every WRITE (NDSOA,*)), W3_MPI (W3XETA, :2607), W3_SMC (the
// Arctic rotation of UD and TAUADIR, :3052-3058, :3097-3103), W3_BT4 (SED_D50, :3125-3130),
// W3_IS2 (ICEH, ICEF, :3132-3143), W3_SETUP (ZETA_SETUP, :3144-3150, and their READs), W3_T
// (the NDST prints) and W3_S (STRACE).
//
// File format: gfortran sequential UNFORMATTED, CONVERT=file_endian = 'big_endian' (:2651,
// :2660; constants.F90:87 and WW3's CMake), the records of iogr_unformatted.hpp. Every field
// item is REAL(4): the -O3 GIMPLE has only _gfortran_transfer_real(_write) and
// _gfortran_transfer_array(_write) of kind 4 for them, INTEGER(4) and CHARACTER for the header,
// INTEGER(4) and LOGICAL(4) for the time record. Two layouts are not the obvious ones:
// - MAPTMP is written as ((MAPTMP(IY,IX),IX=1,NX),IY=1,NY) (:2834-2835, :2841-2842): IX
//   fastest, the transpose of the array's storage order. The object makes one
//   _gfortran_transfer_array per IY with stride NY. Checked against out_grd.ww3 files WW3 wrote.
// - AUX1 and AUX2 are automatic arrays AUX1(NSEA) (:2577) whose bound is taken at entry, before
//   W3SETG(IGRD) (:2604) re-points NSEA (the object mallocs them first, then calls W3SETO), so
//   their records have the NSEA of the grid selected before the call. Reproduced.
//
// Arithmetic, checked against the -O3 GIMPLE and assembly of w3iogomd.F90 (gfortran 16.2.0,
// WW3's Release flags, the switch_lab_shrd defines; .text and .rodata identical to the CMake
// object except for the source path in the runtime-error strings):
// - AUX1 = UA*COS(UD), AUX2 = UA*SIN(UD) when UA .NE. UNDEF (:3059-3064), the same for
//   TAUA/TAUADIR (:3104-3110), ABA/ABD (:3564-3570), UBA/UBD (:3584-3590): one cosf call, one
//   mulss, one sinf call, one mulss; the compare is ucomiss with UNDEF reloaded per point, so a
//   NaN amplitude takes the first branch. AUX1 = UST*ASF*COS(USTDIR) when MAPSTA(IY,IX) .EQ. 1
//   (:3375-3383): (UST*ASF)*cosf, the product recomputed for the SIN line. No libmvec call, no
//   sincosf in the Fortran object; glibc's sincosf returns the bits of sinf and cosf for every
//   float (exhaustive, glibc 2.44, W3OUTG_KOKKOS.md), so the C++ may let g++ merge them.
// - MAPTMP = MAPSTA + 8*MAPST2 (:2833), MAPSTA = MOD(MAPTMP+2,8) - 2 and then MAPST2 =
//   (MAPTMP-MAPSTA)/8 with the new MAPSTA (:2844-2845): integer % and / truncating toward zero,
//   iogr::encode_maptmp and iogr::decode_maptmp.
// - Everything else is a copy or a store of the module variable UNDEF.
//
// Calls not ported here are hooks (W3IogoHooks): W3SETO, W3SETG, W3SETW and W3DIMW. W3SETA and
// W3DIMA have ports (adat_w3seta.hpp, adat_w3dima.hpp); iogo_w3seta_port and iogo_w3dima_port
// make hooks of them. An empty hook on the path ends with iogr::kExitMissingHook.
#pragma once

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Kokkos_Core.hpp>

#include "adat_w3dima.hpp"
#include "adat_w3seta.hpp"
#include "adat_wadat.hpp"
#include "iogr_state.hpp"
#include "iogr_unformatted.hpp"
#include "iogr_w3iogr.hpp"
#include "real.hpp"

namespace ww::iogo {

namespace w3iogo_detail {

inline constexpr int kNogrp = 10;  // NOGRP, PARAMETER, w3odatmd.F90:330
inline constexpr int kNgrpp = 20;  // NGRPP, PARAMETER, w3odatmd.F90:331

inline const iogr::FChar<30>& idstr() {  // IDSTR, w3iogomd.F90:161-162
  static const iogr::FChar<30> v{std::string("WAVEWATCH III GRID OUTPUT FILE")};
  return v;
}
inline const iogr::FChar<10>& verogr() {  // VEROGR, w3iogomd.F90:160
  static const iogr::FChar<10> v{std::string("2019-10-04")};
  return v;
}

/// Fortran .EQ. of two CHARACTER values, the shorter padded with blanks
/// (_gfortran_compare_string, :2614, :2620, :2622).
inline bool fstr_eq(std::string_view a, std::string_view b) {
  const std::size_t n = a.size() > b.size() ? a.size() : b.size();
  for (std::size_t i = 0; i < n; ++i) {
    const char ca = i < a.size() ? a[i] : ' ';
    const char cb = i < b.size() ? b[i] : ' ';
    if (ca != cb) return false;
  }
  return true;
}

/// The Iw.m edit descriptor of an internal WRITE: at least m digits, zero padded, a '-' for a
/// negative value, right-justified in w columns, w asterisks when it does not fit (v, gfortran
/// 16.2: I8.8 of -1 and of 123456789 and I6.6 of -3 give asterisks).
inline std::string fortran_iwm(std::int64_t v, int w, int m) {
  std::string d = std::to_string(v < 0 ? -v : v);
  if (static_cast<int>(d.size()) < m) d.insert(0, static_cast<std::size_t>(m) - d.size(), '0');
  if (v < 0) d.insert(0, 1, '-');
  if (static_cast<int>(d.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - d.size(), ' ') + d;
}

/// The first n characters of a CHARACTER value, n = LEN_TRIM as the Fortran's (:J) and (:I).
template <std::size_t N>
std::string head(const iogr::FChar<N>& s, std::size_t n) {
  return std::string(s.c.data(), n < N ? n : N);
}

}  // namespace w3iogo_detail

// ---- Element functions of the Cartesian components (host and device) -----------------------

/// AUX1(ISEA), AUX2(ISEA) of an amplitude a and a direction d in radians: UA/UD :3059-3064,
/// TAUA/TAUADIR :3104-3110, ABA/ABD :3564-3570, UBA/UBD :3584-3590.
KOKKOS_INLINE_FUNCTION void aux_polar(Real a, Real d, Real undef, Real& x, Real& y) {
  if (a != undef) {
    x = a * Kokkos::cos(d);
    y = a * Kokkos::sin(d);
  } else {
    x = undef;
    y = undef;
  }
}

/// AUX1(ISEA), AUX2(ISEA) of the friction velocity, :3375-3383: (UST*ASF)*COS(USTDIR) where
/// MAPSTA(IY,IX) .EQ. 1.
KOKKOS_INLINE_FUNCTION void aux_ust(bool active, Real ust, Real asf, Real dir, Real undef,
                                    Real& x, Real& y) {
  if (active) {
    x = ust * asf * Kokkos::cos(dir);
    y = ust * asf * Kokkos::sin(dir);
  } else {
    x = undef;
    y = undef;
  }
}

// ---- State -------------------------------------------------------------------------------------

/// The unit NDSOG. A Fortran unit stays connected between calls: with OFILES(1) = 0 W3IOGO
/// opens it on the first call only (:2642-2664) and every later call appends to it. A unit
/// that refused one direction at OPEN carries the other only, as libgfortran's regular_file2
/// falls back (iogr_w3iogr.hpp), so the first transfer fails as gfortran's does.
struct W3IogoUnit {
  struct Closer {
    void operator()(std::FILE* f) const { std::fclose(f); }
  };
  std::unique_ptr<std::FILE, Closer> f;
  bool readable = false;
  bool writable = false;
  /// A READ met the end of the file at a record start; the next READ or WRITE on the unit fails
  /// until a REWIND (libgfortran AFTER_ENDFILE).
  bool after_endfile = false;
};

/// OUTPTS(IGRD) of W3ODATMD as W3SETO(IGRD) selects it (w3odatmd.F90:350-361, :452, :1683-1689).
struct W3IogoOutput {
  /// FLOGRD(NOGRP,NGRPP), LOGICAL(4) storage, column-major: FLOGRD(i,j) = flogrd[(i-1) + 10*(j-1)].
  std::array<iogr::FLogical, 200> flogrd{};
  std::int32_t ipass1 = 0;  ///< OUT1%IPASS1, 0 from W3NOUT (w3odatmd.F90:704)
  /// OUT1%WRITE1. W3NOUT never sets it, so the Fortran value is undefined until the first
  /// W3IOGO with OFILES(1) = 0 or 1 sets it; 0 here.
  iogr::FLogical write1 = 0;
  std::array<std::int32_t, 8> ofiles{};  ///< OFILES(8)
};

/// WDATAS(IGRD) of W3WDATMD as W3SETW(IGRD) selects it, with the bases of W3DIMW
/// (w3wdatmd.F90:560-573): wlv, berg, rhoair (NSEA) and asf (NSEATM) take [isea-1]; ice
/// (0:NSEA), ust and ustdir (0:NSEATM) take [isea].
struct W3IogoWdat {
  std::array<std::int32_t, 2> time{};  ///< TIME(2)
  iogr::FLogical dinit = 0;             ///< DINIT
  std::vector<Real> wlv, ice, berg, rhoair, ust, ustdir, asf;
};

/// The module state W3IOGO reads and writes. Each pointer stands for the Fortran module pointers
/// the W3SET* routines re-point; a selection hook may re-point it at grid IGRD's struct.
/// - grid: W3GDATMD (NSEA, NX, NY, NK, GNAME, FILEXT, MAPSTA, MAPST2, MAPSF, E3DF, P2MSF,
///   US3DF, USSPF) and the W3ODATMD values W3IOGR keeps there (NOSWLL, FNMPRE). At entry it
///   must be the grid the last W3SETG selected, not necessarily IMOD's: AUX1 and AUX2 take its
///   NSEA before the w3setg hook re-points it (:2577, :2604).
/// - adat, targets: W3ADATMD; the fields are those of the model of W3SETA's `ainit` group
///   (DW .. TAUADIR, WN, and HS .. USERO while targets->x is 0, always under W3_SHRD), AINIT is
///   that of the `base` group (adat_w3seta.hpp).
/// - undef: CONSTANTS' UNDEF, a variable (constants.F90:85) that a READ overwrites (:2683).
/// - fnmgrd: W3ODATMD FNMGRD (w3odatmd.F90:340).
template <class ExecSpace>
struct W3IogoModules {
  iogr::IogrState* grid = nullptr;
  W3IogoOutput* out = nullptr;
  W3IogoWdat* wdat = nullptr;
  adat::Wadats<ExecSpace>* adat = nullptr;
  adat::W3setaTargets* targets = nullptr;
  Real* undef = nullptr;
  iogr::FChar<256>* fnmgrd = nullptr;
};

/// A routine W3IOGO calls and this file does not port. Returns 0, or the EXTCDE code it ends
/// with. igrd is W3IOGO's IGRD.
template <class ExecSpace>
using W3IogoHook = std::function<int(W3IogoModules<ExecSpace>&, int igrd)>;

/// The calls of W3IOGO, in its order.
template <class ExecSpace>
struct W3IogoHooks {
  W3IogoHook<ExecSpace> w3seto;  ///< :2603
  W3IogoHook<ExecSpace> w3setg;  ///< :2604
  W3IogoHook<ExecSpace> w3seta;  ///< :2605
  W3IogoHook<ExecSpace> w3setw;  ///< :2609
  W3IogoHook<ExecSpace> w3dimw;  ///< :3018, READ, W3DIMW(IGRD, NDSE, NDST, .TRUE.) when .NOT.DINIT
  W3IogoHook<ExecSpace> w3dima;  ///< :3019, READ, W3DIMA(IGRD, NDSE, NDST, .TRUE.) when .NOT.AINIT
};

/// Codes outside the Fortran's EXTCDE codes (iogr's kExitGfortranRuntime 2, kExitMissingHook
/// -101 and kExitShape -102 are used as well).
inline constexpr int kExitAllocate = 1;  ///< ALLOCATE(MAPTMP) without STAT failed: os_error, exit 1 (v)
inline constexpr int kExitUnassociated = -110;   ///< a module pointer the path reads was never set
inline constexpr int kExitUnitNotConnected = -111;  ///< a transfer on a unit no OPEN connected

/// IOSTAT of a READ after one that met the end of the file, LIBERROR_OPTION_CONFLICT, as
/// W3FLDG_KOKKOS.md measured; a gfortran 16.2 program built with WW3's flags gave 5001 for a READ
/// of TIME, FLOGRD after the -1 (v, 2026-10-16).
inline constexpr int kIostatAfterEndfile = 5001;

/// What W3IOGO ends with. exit_code 0: it returned. Otherwise the code EXTCDE would exit with and
/// the FORMAT label and values of the message W3IOGO writes to NDSE before it (900-906;
/// 0 for EXTOPN and EXTIOF, which write their own); the caller prints them, as in W3IOGR's port.
struct W3IogoStatus {
  int exit_code = 0;
  int fmt = 0;
  int iostat = 0;               ///< IERR handed to EXTOPN or EXTIOF
  int record = 0;               ///< 1-based number, in this call, of the failing transfer statement
  std::string found, expected;  ///< the A values of FORMATs 900-903
  std::array<int, 2> ifound{}, iexpected{};  ///< the I values of 904 (MOGRP, MGRPP / NOGRP, NGRPP) and 906
  /// FORMAT 905 (ILEGAL GNAME, :2698-2700) is a warning: the Fortran writes it and goes on.
  int warn_fmt = 0;
  std::string warn_found, warn_expected;
  std::string filename;  ///< the name of the last OPEN
};

// ---- Hooks made of existing ports ---------------------------------------------------------------

/// W3SETA(IGRD) through adat::w3seta. in(igrd) supplies INPUTS(IGRD)%INFLAGS1 and the GTYPE of
/// the last W3SETG.
template <class ExecSpace>
W3IogoHook<ExecSpace> iogo_w3seta_port(adat::WadatsNl1<ExecSpace>& nl1,
                                       std::function<adat::W3setaInputs(int)> in) {
  return [&nl1, in](W3IogoModules<ExecSpace>& m, int igrd) -> int {
    if (!m.adat || !m.targets) return kExitUnassociated;
    return static_cast<int>(adat::w3seta(*m.adat, nl1, *m.targets, igrd, in(igrd)));
  };
}

/// W3DIMA(IGRD, NDSE, NDST, .TRUE.) through adat::w3dima, followed by the W3SETA(IMOD) W3DIMA
/// itself makes at w3adatmd.F90:1467, which that port leaves to its caller. grid(m, igrd)
/// supplies W3DIMA's view of the other modules (W3GDATMD of IGRD after the header READ, NSEA
/// included).
template <class ExecSpace>
W3IogoHook<ExecSpace> iogo_w3dima_port(
    const ExecSpace& exec, adat::WadatsNl1<ExecSpace>& nl1,
    std::function<adat::W3dimaGrid(const W3IogoModules<ExecSpace>&, int)> grid,
    std::function<adat::W3setaInputs(int)> in) {
  return [exec, &nl1, grid, in](W3IogoModules<ExecSpace>& m, int igrd) -> int {
    if (!m.adat || !m.targets) return kExitUnassociated;
    const auto s = adat::w3dima(exec, *m.adat, *m.targets, igrd, grid(m, igrd), true);
    if (s != adat::W3dimaStatus::ok) return static_cast<int>(s);
    return static_cast<int>(adat::w3seta(*m.adat, nl1, *m.targets, igrd, in(igrd)));
  };
}

// ---- Host staging of the W3ADATMD fields --------------------------------------------------------

/// The W3ADATMD arrays W3IOGO touches, as host mirrors of the model's Views: the same View when
/// ExecSpace's memory is host-accessible, a copy otherwise. Phase 1 copies per call: stage()
/// before the first access, unstage() at every return after it. Bases as adat_wadat.hpp:
/// DW .. TAUADIR (0:NSEA) take (isea); the (NSEALM) fields take (isea-1); WN (0:NK+1,0:NSEA)
/// takes (ik, isea).
template <class ExecSpace>
struct W3IogoHostFields {
  using W = adat::Wadat<ExecSpace>;
  using H1 = decltype(Kokkos::create_mirror_view(std::declval<typename W::R1>()));
  using H2 = decltype(Kokkos::create_mirror_view(std::declval<typename W::R2>()));

  H1 dw, cx, cy, ua, ud, as, taua, tauadir;
  H1 hs, wlm, t02, t0m1, t01, fp0, thm, ths, thp0, hsig, stmaxe, stmaxd, hmaxe, hcmaxe, hmaxd,
      hcmaxd, wbt, wnmean;
  H2 ef, th1m, sth1m, th2m, sth2m, wn;
  H2 phs, ptp, plp, pdir, psi, pws, pthp0, pqp, ppe, pgw, psw, ptm1, pt1, pt2, pep;
  H1 pwst, pnr;
  H1 charn, cge, phiaw, tauwix, tauwiy, tauwnx, tauwny, tws;
  H2 whitecap;
  H1 sxx, syy, sxy, tauox, tauoy, bhd, phioc, tusx, tusy, ussx, ussy, prms, tpms, phice, tauocx,
      tauocy;
  H2 us3d, p2sms, tauice, ussp;
  H1 aba, abd, uba, ubd, phibbl;
  H2 bedforms, taubbl;
  H1 mssx, mssy, mscx, mscy, mssd, mscd, qp, qkk, skew, embia1, embia2;
  H1 dtdyn, fcut, cflxymax, cflthmax, cflkmax;
  H2 usero;
  int ef_lb = 0, th1m_lb = 0, sth1m_lb = 0, th2m_lb = 0, sth2m_lb = 0, p2sms_lb = 0;

  template <class F>
  void each(W& w, F&& f) {
    f(w.dw, dw); f(w.cx, cx); f(w.cy, cy); f(w.ua, ua); f(w.ud, ud); f(w.as, as);
    f(w.taua, taua); f(w.tauadir, tauadir);
    f(w.hs, hs); f(w.wlm, wlm); f(w.t02, t02); f(w.t0m1, t0m1); f(w.t01, t01); f(w.fp0, fp0);
    f(w.thm, thm); f(w.ths, ths); f(w.thp0, thp0); f(w.hsig, hsig); f(w.stmaxe, stmaxe);
    f(w.stmaxd, stmaxd); f(w.hmaxe, hmaxe); f(w.hcmaxe, hcmaxe); f(w.hmaxd, hmaxd);
    f(w.hcmaxd, hcmaxd); f(w.wbt, wbt); f(w.wnmean, wnmean);
    f(w.ef, ef); f(w.th1m, th1m); f(w.sth1m, sth1m); f(w.th2m, th2m); f(w.sth2m, sth2m);
    f(w.wn, wn);
    f(w.phs, phs); f(w.ptp, ptp); f(w.plp, plp); f(w.pdir, pdir); f(w.psi, psi); f(w.pws, pws);
    f(w.pthp0, pthp0); f(w.pqp, pqp); f(w.ppe, ppe); f(w.pgw, pgw); f(w.psw, psw);
    f(w.ptm1, ptm1); f(w.pt1, pt1); f(w.pt2, pt2); f(w.pep, pep); f(w.pwst, pwst); f(w.pnr, pnr);
    f(w.charn, charn); f(w.cge, cge); f(w.phiaw, phiaw); f(w.tauwix, tauwix);
    f(w.tauwiy, tauwiy); f(w.tauwnx, tauwnx); f(w.tauwny, tauwny); f(w.tws, tws);
    f(w.whitecap, whitecap);
    f(w.sxx, sxx); f(w.syy, syy); f(w.sxy, sxy); f(w.tauox, tauox); f(w.tauoy, tauoy);
    f(w.bhd, bhd); f(w.phioc, phioc); f(w.tusx, tusx); f(w.tusy, tusy); f(w.ussx, ussx);
    f(w.ussy, ussy); f(w.prms, prms); f(w.tpms, tpms); f(w.phice, phice); f(w.tauocx, tauocx);
    f(w.tauocy, tauocy); f(w.us3d, us3d); f(w.p2sms, p2sms); f(w.tauice, tauice);
    f(w.ussp, ussp);
    f(w.aba, aba); f(w.abd, abd); f(w.uba, uba); f(w.ubd, ubd); f(w.phibbl, phibbl);
    f(w.bedforms, bedforms); f(w.taubbl, taubbl);
    f(w.mssx, mssx); f(w.mssy, mssy); f(w.mscx, mscx); f(w.mscy, mscy); f(w.mssd, mssd);
    f(w.mscd, mscd); f(w.qp, qp); f(w.qkk, qkk); f(w.skew, skew); f(w.embia1, embia1);
    f(w.embia2, embia2);
    f(w.dtdyn, dtdyn); f(w.fcut, fcut); f(w.cflxymax, cflxymax); f(w.cflthmax, cflthmax);
    f(w.cflkmax, cflkmax); f(w.usero, usero);
  }
  void stage(W& w) {
    each(w, [](auto& d, auto& h) {
      h = Kokkos::create_mirror_view(d);
      Kokkos::deep_copy(h, d);
    });
    ef_lb = w.ef_lb;
    th1m_lb = w.th1m_lb;
    sth1m_lb = w.sth1m_lb;
    th2m_lb = w.th2m_lb;
    sth2m_lb = w.sth2m_lb;
    p2sms_lb = w.p2sms_lb;
  }
  void unstage(W& w) {
    each(w, [](auto& d, auto& h) { Kokkos::deep_copy(d, h); });
  }
};

namespace w3iogo_detail {

/// OPEN(NDSOG, FILE=name, FORM=..., CONVERT='big_endian', IOSTAT=IERR[, STATUS='OLD']) as
/// libgfortran makes it (io/open.c, io/unix.c regular_file2, as read for iogr_w3iogr.hpp):
/// a unit connected to another file is closed first; an OPEN refused for reading and writing
/// retries read-only, then write-only, and succeeds with the one direction it got. Returns IERR:
/// 0, or errno (LIBERROR_OS). "wb" truncates at the OPEN where gfortran truncates at the first
/// WRITE's end (st_write_done); every OPEN here is followed by a REWIND and a WRITE, so the file
/// ends the same.
inline int open_unit(W3IogoUnit& u, const std::string& name, bool for_write) {
  u.f.reset();
  u.readable = u.writable = u.after_endfile = false;
  if (std::FILE* f = std::fopen(name.c_str(), for_write ? "wb" : "rb")) {
    u.f.reset(f);
    u.readable = !for_write;
    u.writable = for_write;
    return 0;
  }
  const int err = errno;
  if (err == EACCES || err == EPERM || err == EROFS) {
    if (std::FILE* f = std::fopen(name.c_str(), for_write ? "rb" : "ab")) {
      u.f.reset(f);
      u.readable = for_write;
      u.writable = !for_write;
      return 0;
    }
  }
  return err != 0 ? err : 1;
}

inline void rewind_unit(W3IogoUnit& u) {  // REWIND (NDSOG), :2664, :2753
  if (u.f) std::rewind(u.f.get());
  u.after_endfile = false;
}

}  // namespace w3iogo_detail

// ---- W3IOGO ------------------------------------------------------------------------------------

/// W3IOGO(INXOUT, NDSOG, IOTST, IMOD), w3iogomd.F90:2395-4097, host side. inxout is 'READ' or
/// 'WRITE'; ndsog the unit, connected or not, kept by the caller between calls; iotst is IOTST
/// (0, or -1 past the end of the file); imod the OPTIONAL IMOD (IGRD = 1 when absent, :2597-2601).
/// The W3ADATMD fields live in ExecSpace's memory and are staged through host mirrors
/// (W3IogoHostFields), so the cos and sin of the Cartesian components are glibc's on any
/// backend. The function does not fence or launch kernels except in the staging deep_copy.
template <class ExecSpace>
W3IogoStatus w3iogo(const std::string& inxout, W3IogoUnit& ndsog, int& iotst,
                    W3IogoModules<ExecSpace>& m, const W3IogoHooks<ExecSpace>& hooks,
                    std::optional<int> imod = std::nullopt) {
  using namespace w3iogo_detail;
  using iogr::kExitGfortranRuntime;
  using iogr::kExitMissingHook;
  using iogr::kExitShape;
  W3IogoStatus st;

  // AUX1(NSEA), AUX2(NSEA) are sized at entry, before W3SETG(IGRD) re-points NSEA (:2577).
  const std::int32_t nsea_entry = m.grid ? m.grid->nsea : 0;
  std::vector<Real> aux1, aux2;
  try {
    aux1.assign(static_cast<std::size_t>(nsea_entry > 0 ? nsea_entry : 0), Real(0));
    aux2.assign(aux1.size(), Real(0));
  } catch (const std::bad_alloc&) {
    st.exit_code = kExitAllocate;
    return st;
  }

  const int igrd = imod ? *imod : 1;  // :2597-2601
  auto call = [&](const W3IogoHook<ExecSpace>& h) {
    if (!h) {
      st.exit_code = kExitMissingHook;
      return false;
    }
    if (const int code = h(m, igrd); code != 0) {
      st.exit_code = code;
      return false;
    }
    return true;
  };
  // :2603-2609. Under W3_SHRD there is no W3XETA (:2606-2608).
  if (!call(hooks.w3seto) || !call(hooks.w3setg) || !call(hooks.w3seta) ||
      !call(hooks.w3setw))
    return st;
  if (!m.grid || !m.out || !m.wdat || !m.adat || !m.targets || !m.undef || !m.fnmgrd) {
    st.exit_code = kExitUnassociated;
    return st;
  }
  iogr::IogrState& g = *m.grid;
  W3IogoOutput& o = *m.out;
  W3IogoWdat& wd = *m.wdat;
  Real& undef = *m.undef;

  o.ipass1 = o.ipass1 + 1;  // :2611
  iotst = 0;                // :2612

  const bool is_read = fstr_eq(inxout, "READ");
  const bool is_write = fstr_eq(inxout, "WRITE");
  if (!is_read && !is_write) {  // :2614-2617
    st.exit_code = 1;
    st.fmt = 900;
    st.found = inxout;
    return st;
  }
  auto mixed = [&]() {  // :2622-2625, :2722-2725
    if (o.write1 != 0 && is_read) {
      st.exit_code = 2;
      st.fmt = 901;
      st.found = inxout;
      return true;
    }
    return false;
  };
  if (o.ipass1 == 1 && o.ofiles[0] == 0) {  // :2619-2626
    o.write1 = is_write ? 1 : 0;
  } else if (mixed()) {
    return st;
  }

  // FNMPRE_LOCAL, CHARACTER(LEN=256) (:2636-2640): FNMPRE (LEN=80) blank padded, or FNMGRD.
  iogr::FChar<256> fnmpre_local;
  if (m.fnmgrd->len_trim() == 0)
    fnmpre_local.assign(std::string(g.fnmpre.c.data(), g.fnmpre.c.size()));
  else
    fnmpre_local = *m.fnmgrd;

  int ierr = 0;
  std::size_t ilen = 0, jlen = 0;  // I, J
  std::string timetag;             // TIMETAG, CHARACTER(LEN=15)

  // ---- Transfer statements ---------------------------------------------------------------
  auto writer_ok = [&]() {
    ++st.record;
    if (!ndsog.f) {
      st.exit_code = kExitUnitNotConnected;  // gfortran would connect fort.<NDSOG>
      return false;
    }
    if (!ndsog.writable || ndsog.after_endfile) {
      st.exit_code = kExitGfortranRuntime;  // the WRITE has no IOSTAT
      return false;
    }
    return true;
  };
  auto wr_end = [&](iogr::RecordWriter& rw) {
    if (!rw.end()) {
      st.exit_code = kExitGfortranRuntime;
      return false;
    }
    return true;
  };
  // A READ statement with IOSTAT=IERR: body(rr) transfers the items; ierr is IOSTAT. Returns
  // false only when the port cannot go on (no unit); a nonzero ierr is left to the caller.
  auto read_stmt = [&](auto&& body) {
    ++st.record;
    if (!ndsog.f) {
      st.exit_code = kExitUnitNotConnected;
      return false;
    }
    if (ndsog.after_endfile) {
      ierr = kIostatAfterEndfile;
      return true;
    }
    if (!ndsog.readable) {
      ierr = iogr::kIostatBadAction;
      return true;
    }
    iogr::RecordReader rr(ndsog.f.get());
    rr.begin();
    body(rr);
    rr.finish();
    ierr = rr.iostat();
    if (ierr == iogr::kIostatEnd) ndsog.after_endfile = true;
    return true;
  };
  auto extiof = [&]() {  // EXTIOF(NDSE, IERR, 'W3IOGO', '', 42), w3servmd.F90:910-923
    st.iostat = ierr;
    st.exit_code = ierr > 0 ? 43 : 42;
    return false;
  };

  // ---- Header record (:2669-2706 and :2759-2795) -------------------------------------------
  auto header = [&]() {
    if (o.write1 != 0) {
      if (!writer_ok()) return false;
      iogr::RecordWriter rw(ndsog.f.get());
      rw.chars(idstr().c.data(), 30);
      rw.chars(verogr().c.data(), 10);
      rw.chars(g.gname.c.data(), 30);
      rw.scalar(std::int32_t{kNogrp});
      rw.scalar(std::int32_t{kNgrpp});
      rw.scalar(g.nsea);
      rw.scalar(g.nx);
      rw.scalar(g.ny);
      rw.scalar(undef);
      rw.scalar(g.noswll);
      return wr_end(rw);
    }
    iogr::FChar<30> idtst, tname;
    iogr::FChar<10> vertst;
    std::int32_t mogrp = 0, mgrpp = 0, moswll = 0;
    if (!read_stmt([&](iogr::RecordReader& rr) {
          rr.chars(idtst.c.data(), 30);
          rr.chars(vertst.c.data(), 10);
          rr.chars(tname.c.data(), 30);
          rr.scalar(mogrp);
          rr.scalar(mgrpp);
          rr.scalar(g.nsea);
          rr.scalar(g.nx);
          rr.scalar(g.ny);
          rr.scalar(undef);
          rr.scalar(moswll);
        }))
      return false;
    if (ierr != 0) return extiof();
    if (!(idtst == idstr())) {  // :2686-2689
      st.exit_code = 20;
      st.fmt = 902;
      st.found = std::string(idtst.c.data(), 30);
      st.expected = std::string(idstr().c.data(), 30);
      return false;
    }
    if (!(vertst == verogr())) {  // :2690-2693
      st.exit_code = 21;
      st.fmt = 903;
      st.found = std::string(vertst.c.data(), 10);
      st.expected = std::string(verogr().c.data(), 10);
      return false;
    }
    if (kNogrp != mogrp || kNgrpp != mgrpp) {  // :2694-2697
      st.exit_code = 22;
      st.fmt = 904;
      st.ifound = {mogrp, mgrpp};
      st.iexpected = {kNogrp, kNgrpp};
      return false;
    }
    if (!(tname == g.gname)) {  // :2698-2700, a warning only
      st.warn_fmt = 905;
      st.warn_found = std::string(tname.c.data(), 30);
      st.warn_expected = std::string(g.gname.c.data(), 30);
    }
    if (g.noswll != moswll) {  // :2701-2704
      st.exit_code = 24;
      st.fmt = 906;
      st.ifound = {moswll, 0};
      st.iexpected = {g.noswll, 0};
      return false;
    }
    return true;
  };

  // ---- Open, OFILES(1) = 0 (:2642-2713) ----------------------------------------------------
  if (o.ipass1 == 1 && o.ofiles[0] == 0) {
    ilen = g.filext.len_trim();
    jlen = fnmpre_local.len_trim();
    st.filename = head(fnmpre_local, jlen) + "out_grd." + head(g.filext, ilen);
    ierr = open_unit(ndsog, st.filename, o.write1 != 0);
    if (ierr != 0) {  // EXTOPN(NDSE, IERR, 'W3IOGO', '', 41), :2652, :2661
      st.iostat = ierr;
      st.exit_code = 41;
      return st;
    }
    rewind_unit(ndsog);
    if (!header()) return st;
  }

  // ---- Open, OFILES(1) = 1, one file per output time (:2719-2802) --------------------------
  if (o.ipass1 >= 1 && o.ofiles[0] == 1) {  // :2719-2726
    o.write1 = is_write ? 1 : 0;
  } else if (mixed()) {
    return st;
  }
  if (o.ipass1 >= 1 && o.ofiles[0] == 1) {
    ilen = g.filext.len_trim();
    jlen = fnmpre_local.len_trim();
    timetag = fortran_iwm(wd.time[0], 8, 8) + "." + fortran_iwm(wd.time[1], 6, 6);  // :2734
    if (o.write1 != 0)
      st.filename = head(fnmpre_local, jlen) + timetag + ".out_grd." + head(g.filext, ilen);
    else
      st.filename = head(fnmpre_local, jlen) + "out_grd." + head(g.filext, ilen);
    ierr = open_unit(ndsog, st.filename, o.write1 != 0);
    if (ierr != 0) {  // :2741, :2750
      st.iostat = ierr;
      st.exit_code = 41;
      return st;
    }
    rewind_unit(ndsog);
    if (!header()) return st;
  }

  // ---- TIME and flags (:2807-2823) ---------------------------------------------------------
  if (o.write1 != 0) {
    if (!writer_ok()) return st;
    iogr::RecordWriter rw(ndsog.f.get());
    rw.array(wd.time.data(), 2);
    rw.array(o.flogrd.data(), o.flogrd.size());
    if (!wr_end(rw)) return st;
  } else {
    if (!read_stmt([&](iogr::RecordReader& rr) {
          rr.array(wd.time.data(), 2);
          rr.array(o.flogrd.data(), o.flogrd.size());
        }))
      return st;
    if (ierr < 0) {  // :2814-2819
      iotst = -1;
      return st;
    }
    if (ierr > 0) {
      extiof();
      return st;
    }
  }

  // ---- MAPSTA (:2831-2847) -----------------------------------------------------------------
  const auto nx = static_cast<std::size_t>(g.nx > 0 ? g.nx : 0);
  const auto ny = static_cast<std::size_t>(g.ny > 0 ? g.ny : 0);
  const std::size_t nxy = nx * ny;
  {
    std::vector<std::int32_t> maptmp;  // ALLOCATE(MAPTMP(NY,NX)), :2831
    try {
      maptmp.assign(nxy, 0);
    } catch (const std::bad_alloc&) {
      st.exit_code = kExitAllocate;
      return st;
    }
    if (g.mapsta.size() != nxy || g.mapst2.size() != nxy) {
      st.exit_code = kExitShape;
      return st;
    }
    if (o.write1 != 0) {
      for (std::size_t i = 0; i < nxy; ++i)  // :2833
        maptmp[i] = iogr::encode_maptmp(g.mapsta[i], g.mapst2[i]);
      if (!writer_ok()) return st;
      iogr::RecordWriter rw(ndsog.f.get());
      for (std::size_t iy = 0; iy < ny; ++iy)  // ((MAPTMP(IY,IX),IX=1,NX),IY=1,NY)
        for (std::size_t ix = 0; ix < nx; ++ix) rw.scalar(maptmp[iy + ny * ix]);
      if (!wr_end(rw)) return st;
    } else {
      if (!read_stmt([&](iogr::RecordReader& rr) {
            for (std::size_t iy = 0; iy < ny; ++iy)
              for (std::size_t ix = 0; ix < nx; ++ix) rr.scalar(maptmp[iy + ny * ix]);
          }))
        return st;
      if (ierr != 0) {
        extiof();
        return st;
      }
      for (std::size_t i = 0; i < nxy; ++i)  // :2844-2845
        iogr::decode_maptmp(maptmp[i], g.mapsta[i], g.mapst2[i]);
    }
  }  // DEALLOCATE(MAPTMP), :2847

  // ---- W3ADATMD fields on the host ---------------------------------------------------------
  adat::W3setaTargets& t = *m.targets;
  if (o.write1 == 0) {  // :3017-3020
    if (wd.dinit == 0 && !call(hooks.w3dimw)) return st;
    const adat::Wadat<ExecSpace>* base = adat::selected(*m.adat, t.base);  // the AINIT alias
    if (!m.wdat || !base) {
      st.exit_code = kExitUnassociated;
      return st;
    }
    if (!base->ainit && !call(hooks.w3dima)) return st;
  }
  W3IogoWdat& w = *m.wdat;
  adat::Wadat<ExecSpace>* wa = t.x == 0 ? adat::selected(*m.adat, t.ainit) : nullptr;
  W3IogoHostFields<ExecSpace> f;
  if (wa) f.stage(*wa);
  struct Unstage {
    W3IogoHostFields<ExecSpace>& f;
    adat::Wadat<ExecSpace>* wa;
    ~Unstage() {
      if (wa) f.unstage(*wa);
    }
  } unstage_at_return{f, wa};

  const auto nsea = static_cast<std::size_t>(g.nsea > 0 ? g.nsea : 0);
  const std::size_t nk = static_cast<std::size_t>(g.nk > 0 ? g.nk : 0);
  // MAPSF(NSEA,3): the leading extent is the NSEA it was allocated with (W3DIMX), which the
  // descriptor keeps and MAPSF(ISEA,2) strides by, not the current NSEA.
  const std::size_t mapsf_ld = g.mapsf.size() / 3;
  auto shape = [&]() {
    st.exit_code = kExitShape;
    return false;
  };
  auto e3 = [&](int i, int k) { return g.e3df[static_cast<std::size_t>((i - 1) + 3 * (k - 1))]; };

  // ---- Initialization, WRITE (:2853-3015): UNDEF at land and ice points ----------------------
  if (o.write1 != 0) {
    const auto fl = [&](int i, int j) {
      return o.flogrd[static_cast<std::size_t>((i - 1) + kNogrp * (j - 1))] != 0;
    };
    if (mapsf_ld < nsea) return shape(), st;
    bool ok = true;
    std::size_t row = 0;
    auto u1 = [&](const auto& v) {  // X(ISEA) = UNDEF, X(NSEALM)
      if (v.extent(0) <= row) ok = false;
      else v(row) = undef;
    };
    auto u2 = [&](const auto& v) {  // X(ISEA,:) = UNDEF
      if (v.extent(0) <= row) ok = false;
      else
        for (std::size_t c = 0; c < v.extent(1); ++c) v(row, c) = undef;
    };
    auto u2c = [&](const auto& v, std::size_t c) {  // X(ISEA,c) = UNDEF
      if (v.extent(0) <= row || v.extent(1) <= c) ok = false;
      else v(row, c) = undef;
    };
    for (std::size_t isea = 1; isea <= nsea; ++isea) {
      row = isea - 1;
      const auto ixs = g.mapsf[row];
      const auto iys = g.mapsf[row + mapsf_ld];
      if (ixs < 1 || iys < 1 || static_cast<std::size_t>(ixs) > nx ||
          static_cast<std::size_t>(iys) > ny)
        return shape(), st;
      const std::int32_t sta = g.mapsta[static_cast<std::size_t>(iys - 1) +
                                        ny * static_cast<std::size_t>(ixs - 1)];
      if (sta < 0) {  // :2855-2985
        if (fl(2, 2)) u1(f.wlm);
        if (fl(2, 3)) u1(f.t02);
        if (fl(2, 4)) u1(f.t0m1);
        if (fl(2, 5)) u1(f.t01);
        if (fl(2, 6) || fl(2, 18)) u1(f.fp0);
        if (fl(2, 7)) u1(f.thm);
        if (fl(2, 8)) u1(f.ths);
        if (fl(2, 9)) u1(f.thp0);
        if (w.ust.size() <= isea || w.ustdir.size() <= isea) return shape(), st;
        w.ust[isea] = undef;     // :2866, unconditional
        w.ustdir[isea] = undef;  // :2867
        if (fl(2, 10)) u1(f.hsig);
        if (fl(2, 11)) u1(f.stmaxe);
        if (fl(2, 12)) u1(f.stmaxd);
        if (fl(2, 13)) u1(f.hmaxe);
        if (fl(2, 14)) u1(f.hcmaxe);
        if (fl(2, 15)) u1(f.hmaxd);
        if (fl(2, 16)) u1(f.hcmaxd);
        if (fl(2, 17)) u1(f.wbt);
        if (fl(2, 19)) u1(f.wnmean);
        if (fl(3, 1)) u2(f.ef);
        if (fl(3, 2)) u2(f.th1m);
        if (fl(3, 3)) u2(f.sth1m);
        if (fl(3, 4)) u2(f.th2m);
        if (fl(3, 5)) u2(f.sth2m);
        if (fl(4, 1)) u2(f.phs);
        if (fl(4, 2)) u2(f.ptp);
        if (fl(4, 3)) u2(f.plp);
        if (fl(4, 4)) u2(f.pdir);
        if (fl(4, 5)) u2(f.psi);
        if (fl(4, 6)) u2(f.pws);
        if (fl(4, 7)) u2(f.pthp0);
        if (fl(4, 8)) u2(f.pqp);
        if (fl(4, 9)) u2(f.ppe);
        if (fl(4, 10)) u2(f.pgw);
        if (fl(4, 11)) u2(f.psw);
        if (fl(4, 12)) u2(f.ptm1);
        if (fl(4, 13)) u2(f.pt1);
        if (fl(4, 14)) u2(f.pt2);
        if (fl(4, 15)) u2(f.pep);
        if (fl(4, 16)) u1(f.pwst);
        if (fl(4, 17)) u1(f.pnr);
        if (fl(5, 2)) u1(f.charn);
        if (fl(5, 3)) u1(f.cge);
        if (fl(5, 4)) u1(f.phiaw);
        if (fl(5, 5)) {
          u1(f.tauwix);
          u1(f.tauwiy);
        }
        if (fl(5, 6)) {
          u1(f.tauwnx);
          u1(f.tauwny);
        }
        if (fl(5, 7)) u2c(f.whitecap, 0);
        if (fl(5, 8)) u2c(f.whitecap, 1);
        if (fl(5, 9)) u2c(f.whitecap, 2);
        if (fl(5, 10)) u2c(f.whitecap, 3);
        if (fl(6, 1)) {
          u1(f.sxx);
          u1(f.syy);
          u1(f.sxy);
        }
        if (fl(6, 2)) {
          u1(f.tauox);
          u1(f.tauoy);
        }
        if (fl(6, 3)) u1(f.bhd);
        if (fl(6, 4)) u1(f.phioc);
        if (fl(6, 5)) {
          u1(f.tusx);
          u1(f.tusy);
        }
        if (fl(6, 6)) {
          u1(f.ussx);
          u1(f.ussy);
        }
        if (fl(6, 7)) {
          u1(f.prms);
          u1(f.tpms);
        }
        if (fl(6, 8)) u2(f.us3d);
        if (fl(6, 9)) u2(f.p2sms);
        if (fl(6, 10)) u2(f.tauice);
        if (fl(6, 11)) u1(f.phice);
        if (fl(6, 12)) u2(f.ussp);
        if (fl(6, 13)) {
          u1(f.tauocx);
          u1(f.tauocy);
        }
        if (fl(7, 1)) {
          u1(f.aba);
          u1(f.abd);
        }
        if (fl(7, 2)) {
          u1(f.uba);
          u1(f.ubd);
        }
        if (fl(7, 3)) u2(f.bedforms);
        if (fl(7, 4)) u1(f.phibbl);
        if (fl(7, 5)) u2(f.taubbl);
        if (fl(8, 1)) {
          u1(f.mssx);
          u1(f.mssy);
        }
        if (fl(8, 2)) {
          u1(f.mscx);
          u1(f.mscy);
        }
        if (fl(8, 3)) u1(f.mssd);
        if (fl(8, 4)) u1(f.mscd);
        if (fl(8, 5)) u1(f.qp);
        if (fl(8, 6)) u1(f.qkk);
        if (fl(8, 7)) u1(f.skew);
        if (fl(8, 8)) u1(f.embia1);
        if (fl(8, 9)) u1(f.embia2);
        if (fl(9, 1)) u1(f.dtdyn);
        if (fl(9, 2)) u1(f.fcut);
        if (fl(9, 3)) u1(f.cflxymax);
        if (fl(9, 4)) u1(f.cflthmax);
        if (fl(9, 5)) u1(f.cflkmax);
      }
      if (sta == 2) {  // :2987-3013
        if (fl(5, 4)) u1(f.phiaw);
        if (fl(5, 5)) {
          u1(f.tauwix);
          u1(f.tauwiy);
        }
        if (fl(5, 6)) {
          u1(f.tauwnx);
          u1(f.tauwny);
        }
        if (fl(5, 7)) u2c(f.whitecap, 0);
        if (fl(5, 8)) u2c(f.whitecap, 1);
        if (fl(5, 9)) u2c(f.whitecap, 2);
        if (fl(5, 10)) u2c(f.whitecap, 3);
        if (fl(6, 2)) {
          u1(f.tauox);
          u1(f.tauoy);
        }
        if (fl(6, 4)) u1(f.phioc);
        if (fl(7, 3)) u2(f.bedforms);
        if (fl(7, 4)) u1(f.phibbl);
        if (fl(7, 5)) u2(f.taubbl);
      }
      if (!ok) return shape(), st;
    }
  }

  // ---- Field records (:3023-4022) ----------------------------------------------------------
  // Writers and readers of one record. off is the index of Fortran element 1 (1 for the
  // (0:NSEA) arrays, 0 for the (NSEALM) ones); a section that does not fit the array, where the
  // Fortran would run out of bounds, ends with iogr::kExitShape.
  auto w_seq = [&](auto&& at, std::size_t n) {  // one record of n items at(k)
    if (!writer_ok()) return false;
    iogr::RecordWriter rw(ndsog.f.get());
    for (std::size_t k = 0; k < n; ++k) {
      Real x = at(k);
      rw.scalar(x);
    }
    return wr_end(rw);
  };
  auto r_seq = [&](auto&& at, std::size_t n) {
    return read_stmt([&](iogr::RecordReader& rr) {
      for (std::size_t k = 0; k < n; ++k) rr.scalar(at(k));
    });
  };
  auto fits1 = [&](const auto& v, std::size_t off) { return v.extent(0) >= off + nsea; };
  auto cols = [&](const auto& v, long c0, long c1) {  // section c0:c1 of the 0-based columns
    return v.extent(0) >= nsea && (c1 < c0 || (c0 >= 0 && static_cast<std::size_t>(c1) < v.extent(1)));
  };
  auto ncols = [](long c0, long c1) { return c1 < c0 ? std::size_t{0} : static_cast<std::size_t>(c1 - c0 + 1); };
  auto W1 = [&](const auto& v, std::size_t off) {
    if (!fits1(v, off)) return shape();
    return w_seq([&](std::size_t k) { return v(off + k); }, nsea);
  };
  auto WC = [&](const auto& v, long c0, long c1) {  // X(1:NSEA, c0:c1), ISEA fastest
    if (!cols(v, c0, c1)) return shape();
    return w_seq([&](std::size_t k) { return v(k % nsea, static_cast<std::size_t>(c0) + k / nsea); },
                 nsea * ncols(c0, c1));
  };
  auto R1 = [&](const auto& v, std::size_t off) {
    if (!fits1(v, off)) return shape();
    return r_seq([&](std::size_t k) -> Real& { return v(off + k); }, nsea);
  };
  auto RC = [&](const auto& v, long c0, long c1) {
    if (!cols(v, c0, c1)) return shape();
    return r_seq(
        [&](std::size_t k) -> Real& { return v(k % nsea, static_cast<std::size_t>(c0) + k / nsea); },
        nsea * ncols(c0, c1));
  };
  auto WV = [&](const std::vector<Real>& v, std::size_t off) {
    if (v.size() < off + nsea) return shape();
    return w_seq([&](std::size_t k) { return v[off + k]; }, nsea);
  };
  auto RV = [&](std::vector<Real>& v, std::size_t off) {
    if (v.size() < off + nsea) return shape();
    return r_seq([&](std::size_t k) -> Real& { return v[off + k]; }, nsea);
  };
  auto WAUX = [&](const std::vector<Real>& v) {  // WRITE (NDSOG) AUX1: NSEA at entry
    return w_seq([&](std::size_t k) { return v[k]; }, v.size());
  };
  auto WWN = [&]() {  // WN(1:NK,1:NSEA) of WN(0:NK+1,0:NSEA), IK fastest
    if (f.wn.extent(0) < nk + 1 || f.wn.extent(1) < nsea + 1) return shape();
    return w_seq([&](std::size_t k) { return f.wn(1 + k % nk, 1 + k / nk); }, nk * nsea);
  };
  auto RWN = [&]() {
    if (f.wn.extent(0) < nk + 1 || f.wn.extent(1) < nsea + 1) return shape();
    return r_seq([&](std::size_t k) -> Real& { return f.wn(1 + k % nk, 1 + k / nk); }, nk * nsea);
  };
  // The Cartesian components: the loop over ISEA = 1, NSEA writes AUX1(ISEA), which the
  // Fortran does past its bound when NSEA grew since entry.
  auto polar = [&](const auto& a, const auto& d) {  // a, d: (0:NSEA)
    if (!fits1(a, 1) || !fits1(d, 1) || aux1.size() < nsea) return shape();
    for (std::size_t isea = 1; isea <= nsea; ++isea)
      aux_polar(a(isea), d(isea), undef, aux1[isea - 1], aux2[isea - 1]);
    return WAUX(aux1) && WAUX(aux2);
  };
  auto polar1 = [&](const auto& a, const auto& d) {  // a, d: (NSEALM)
    if (!fits1(a, 0) || !fits1(d, 0) || aux1.size() < nsea) return shape();
    for (std::size_t isea = 1; isea <= nsea; ++isea)
      aux_polar(a(isea - 1), d(isea - 1), undef, aux1[isea - 1], aux2[isea - 1]);
    return WAUX(aux1) && WAUX(aux2);
  };
  auto ust_xy = [&]() {  // :3372-3384
    if (w.ust.size() < nsea + 1 || w.ustdir.size() < nsea + 1 || w.asf.size() < nsea ||
        aux1.size() < nsea || mapsf_ld < nsea)
      return shape();
    for (std::size_t isea = 1; isea <= nsea; ++isea) {
      const auto ixs = g.mapsf[isea - 1];
      const auto iys = g.mapsf[isea - 1 + mapsf_ld];
      if (ixs < 1 || iys < 1 || static_cast<std::size_t>(ixs) > nx ||
          static_cast<std::size_t>(iys) > ny)
        return shape();
      const bool active = g.mapsta[static_cast<std::size_t>(iys - 1) +
                                   ny * static_cast<std::size_t>(ixs - 1)] == 1;
      aux_ust(active, w.ust[isea], w.asf[isea - 1], w.ustdir[isea], undef, aux1[isea - 1],
              aux2[isea - 1]);
    }
    return WAUX(aux1) && WAUX(aux2);
  };
  auto e3c0 = [&](int k, int lb) { return static_cast<long>(e3(2, k)) - lb; };
  auto e3c1 = [&](int k, int lb) { return static_cast<long>(e3(3, k)) - lb; };
  const long lnk = static_cast<long>(nk);
  const long us0 = static_cast<long>(g.us3df[1]) - 1, us1 = static_cast<long>(g.us3df[2]) - 1;
  const long up1 = static_cast<long>(g.usspf[1]) - 1;
  const long p0 = static_cast<long>(g.p2msf[1]) - f.p2sms_lb;
  const long p1 = static_cast<long>(g.p2msf[2]) - f.p2sms_lb;
  const long nsw = static_cast<long>(g.noswll);

  auto write_field = [&](int ifi, int ifj) -> bool {
    if (ifi == 1 && ifj == 1) return W1(f.dw, 1);  // :3037
    if (ifi == 1 && ifj == 2) return W1(f.cx, 1) && W1(f.cy, 1);
    if (ifi == 1 && ifj == 3) return polar(f.ua, f.ud);  // :3051-3071
    if (ifi == 1 && ifj == 4) return W1(f.as, 1);
    if (ifi == 1 && ifj == 5) return WV(w.wlv, 0);
    if (ifi == 1 && ifj == 6) return WV(w.ice, 1);
    if (ifi == 1 && ifj == 7) return WV(w.berg, 0);
    if (ifi == 1 && ifj == 8) return polar(f.taua, f.tauadir);  // :3096-3116
    if (ifi == 1 && ifj == 9) return WV(w.rhoair, 0);
    if (ifi == 2 && ifj == 1) return W1(f.hs, 0);  // :3156
    if (ifi == 2 && ifj == 2) return W1(f.wlm, 0);
    if (ifi == 2 && ifj == 3) return W1(f.t02, 0);
    if (ifi == 2 && ifj == 4) return W1(f.t0m1, 0);
    if (ifi == 2 && ifj == 5) return W1(f.t01, 0);
    if ((ifi == 2 && ifj == 6) || (ifi == 2 && ifj == 18)) return W1(f.fp0, 0);
    if (ifi == 2 && ifj == 7) return W1(f.thm, 0);
    if (ifi == 2 && ifj == 8) return W1(f.ths, 0);
    if (ifi == 2 && ifj == 9) return W1(f.thp0, 0);
    if (ifi == 2 && ifj == 10) return W1(f.hsig, 0);
    if (ifi == 2 && ifj == 11) return W1(f.stmaxe, 0);
    if (ifi == 2 && ifj == 12) return W1(f.stmaxd, 0);
    if (ifi == 2 && ifj == 13) return W1(f.hmaxe, 0);
    if (ifi == 2 && ifj == 14) return W1(f.hcmaxe, 0);
    if (ifi == 2 && ifj == 15) return W1(f.hmaxd, 0);
    if (ifi == 2 && ifj == 16) return W1(f.hcmaxd, 0);
    if (ifi == 2 && ifj == 17) return W1(f.wbt, 0);
    if (ifi == 2 && ifj == 19) return W1(f.wnmean, 0);
    if (ifi == 3 && ifj == 1) return WC(f.ef, e3c0(1, f.ef_lb), e3c1(1, f.ef_lb));  // :3251
    if (ifi == 3 && ifj == 2) return WC(f.th1m, e3c0(2, f.th1m_lb), e3c1(2, f.th1m_lb));
    if (ifi == 3 && ifj == 3) return WC(f.sth1m, e3c0(3, f.sth1m_lb), e3c1(3, f.sth1m_lb));
    if (ifi == 3 && ifj == 4) return WC(f.th2m, e3c0(4, f.th2m_lb), e3c1(4, f.th2m_lb));
    if (ifi == 3 && ifj == 5) return WC(f.sth2m, e3c0(5, f.sth2m_lb), e3c1(5, f.sth2m_lb));
    if (ifi == 3 && ifj == 6) return WWN();  // :3276
    if (ifi == 4 && ifj == 1) return WC(f.phs, 0, nsw);  // :3284
    if (ifi == 4 && ifj == 2) return WC(f.ptp, 0, nsw);
    if (ifi == 4 && ifj == 3) return WC(f.plp, 0, nsw);
    if (ifi == 4 && ifj == 4) return WC(f.pdir, 0, nsw);
    if (ifi == 4 && ifj == 5) return WC(f.psi, 0, nsw);
    if (ifi == 4 && ifj == 6) return WC(f.pws, 0, nsw);
    if (ifi == 4 && ifj == 7) return WC(f.pthp0, 0, nsw);
    if (ifi == 4 && ifj == 8) return WC(f.pqp, 0, nsw);
    if (ifi == 4 && ifj == 9) return WC(f.ppe, 0, nsw);
    if (ifi == 4 && ifj == 10) return WC(f.pgw, 0, nsw);
    if (ifi == 4 && ifj == 11) return WC(f.psw, 0, nsw);
    if (ifi == 4 && ifj == 12) return WC(f.ptm1, 0, nsw);
    if (ifi == 4 && ifj == 13) return WC(f.pt1, 0, nsw);
    if (ifi == 4 && ifj == 14) return WC(f.pt2, 0, nsw);
    if (ifi == 4 && ifj == 15) return WC(f.pep, 0, nsw);
    if (ifi == 4 && ifj == 16) return W1(f.pwst, 0);
    if (ifi == 4 && ifj == 17) return W1(f.pnr, 0);
    if (ifi == 5 && ifj == 1) return ust_xy();  // :3371-3391
    if (ifi == 5 && ifj == 2) return W1(f.charn, 0);
    if (ifi == 5 && ifj == 3) return W1(f.cge, 0);
    if (ifi == 5 && ifj == 4) return W1(f.phiaw, 0);
    if (ifi == 5 && ifj == 5) return W1(f.tauwix, 0) && W1(f.tauwiy, 0);
    if (ifi == 5 && ifj == 6) return W1(f.tauwnx, 0) && W1(f.tauwny, 0);
    if (ifi == 5 && ifj == 7) return WC(f.whitecap, 0, 0);
    if (ifi == 5 && ifj == 8) return WC(f.whitecap, 1, 1);
    if (ifi == 5 && ifj == 9) return WC(f.whitecap, 2, 2);
    if (ifi == 5 && ifj == 10) return WC(f.whitecap, 3, 3);
    if (ifi == 5 && ifj == 11) return W1(f.tws, 0);
    if (ifi == 6 && ifj == 1) return W1(f.sxx, 0) && W1(f.syy, 0) && W1(f.sxy, 0);  // :3455
    if (ifi == 6 && ifj == 2) return W1(f.tauox, 0) && W1(f.tauoy, 0);
    if (ifi == 6 && ifj == 3) return W1(f.bhd, 0);
    if (ifi == 6 && ifj == 4) return W1(f.phioc, 0);
    if (ifi == 6 && ifj == 5) return W1(f.tusx, 0) && W1(f.tusy, 0);
    if (ifi == 6 && ifj == 6) return W1(f.ussx, 0) && W1(f.ussy, 0);
    if (ifi == 6 && ifj == 7) return W1(f.prms, 0) && W1(f.tpms, 0);
    if (ifi == 6 && ifj == 8)  // :3514, :3518
      return WC(f.us3d, us0, us1) && WC(f.us3d, lnk + us0, lnk + us1);
    if (ifi == 6 && ifj == 9) return WC(f.p2sms, p0, p1);
    if (ifi == 6 && ifj == 10) return WC(f.tauice, 0, 0) && WC(f.tauice, 1, 1);
    if (ifi == 6 && ifj == 11) return W1(f.phice, 0);
    if (ifi == 6 && ifj == 12)  // :3542, :3546
      return WC(f.ussp, 0, up1) && WC(f.ussp, lnk, lnk + up1);
    if (ifi == 6 && ifj == 13) return W1(f.tauocx, 0) && W1(f.tauocy, 0);
    if (ifi == 7 && ifj == 1) return polar1(f.aba, f.abd);  // :3562-3576
    if (ifi == 7 && ifj == 2) return polar1(f.uba, f.ubd);  // :3582-3596
    if (ifi == 7 && ifj == 3)
      return WC(f.bedforms, 0, 0) && WC(f.bedforms, 1, 1) && WC(f.bedforms, 2, 2);
    if (ifi == 7 && ifj == 4) return W1(f.phibbl, 0);
    if (ifi == 7 && ifj == 5) return WC(f.taubbl, 0, 0) && WC(f.taubbl, 1, 1);
    if (ifi == 8 && ifj == 1) return W1(f.mssx, 0) && W1(f.mssy, 0);  // :3633
    if (ifi == 8 && ifj == 2) return W1(f.mscx, 0) && W1(f.mscy, 0);
    if (ifi == 8 && ifj == 3) return W1(f.mssd, 0);
    if (ifi == 8 && ifj == 4) return W1(f.mscd, 0);
    if (ifi == 8 && ifj == 5) return W1(f.qp, 0);
    if (ifi == 8 && ifj == 6) return W1(f.qkk, 0);
    if (ifi == 8 && ifj == 7) return W1(f.skew, 0);
    if (ifi == 8 && ifj == 8) return W1(f.embia1, 0);
    if (ifi == 8 && ifj == 9) return W1(f.embia2, 0);
    if (ifi == 9 && ifj == 1) return W1(f.dtdyn, 0);  // :3689
    if (ifi == 9 && ifj == 2) return W1(f.fcut, 0);
    if (ifi == 9 && ifj == 3) return W1(f.cflxymax, 0);
    if (ifi == 9 && ifj == 4) return W1(f.cflthmax, 0);
    if (ifi == 9 && ifj == 5) return W1(f.cflkmax, 0);
    if (ifi == 10) return WC(f.usero, ifj - 1, ifj - 1);  // :3717, USERO(NSEALM,NOEXTR)
    return true;  // no branch: nothing written, as the ELSE IF chain (:3722)
  };

  // READ (:3730-4009). A two-record field tests IERR between its READs; every field ends with
  // the test of :4009, which for a flag no branch matches sees the IERR of the last statement.
  auto read_field = [&](int ifi, int ifj) -> bool {
    auto mid = [&]() { return ierr == 0 || extiof(); };
    bool ok = true;
    if (ifi == 1 && ifj == 1) ok = R1(f.dw, 1);
    else if (ifi == 1 && ifj == 2) ok = R1(f.cx, 1) && mid() && R1(f.cy, 1);
    // UA and UD receive the Cartesian components the WRITE stored (:3737-3739), as written.
    else if (ifi == 1 && ifj == 3) ok = R1(f.ua, 1) && mid() && R1(f.ud, 1);
    else if (ifi == 1 && ifj == 4) ok = R1(f.as, 1);
    else if (ifi == 1 && ifj == 5) ok = RV(w.wlv, 0);
    else if (ifi == 1 && ifj == 6) ok = RV(w.ice, 1);
    else if (ifi == 1 && ifj == 7) ok = RV(w.berg, 0);
    else if (ifi == 1 && ifj == 8) ok = R1(f.taua, 1) && mid() && R1(f.tauadir, 1);
    else if (ifi == 1 && ifj == 9) ok = RV(w.rhoair, 0);
    else if (ifi == 2 && ifj == 1) ok = R1(f.hs, 0);
    else if (ifi == 2 && ifj == 2) ok = R1(f.wlm, 0);
    else if (ifi == 2 && ifj == 3) ok = R1(f.t02, 0);
    else if (ifi == 2 && ifj == 4) ok = R1(f.t0m1, 0);
    else if (ifi == 2 && ifj == 5) ok = R1(f.t01, 0);
    else if ((ifi == 2 && ifj == 6) || (ifi == 2 && ifj == 18)) ok = R1(f.fp0, 0);
    else if (ifi == 2 && ifj == 7) ok = R1(f.thm, 0);
    else if (ifi == 2 && ifj == 8) ok = R1(f.ths, 0);
    else if (ifi == 2 && ifj == 9) ok = R1(f.thp0, 0);
    else if (ifi == 2 && ifj == 10) ok = R1(f.hsig, 0);
    else if (ifi == 2 && ifj == 11) ok = R1(f.stmaxe, 0);
    else if (ifi == 2 && ifj == 12) ok = R1(f.stmaxd, 0);
    else if (ifi == 2 && ifj == 13) ok = R1(f.hmaxe, 0);
    else if (ifi == 2 && ifj == 14) ok = R1(f.hcmaxe, 0);
    else if (ifi == 2 && ifj == 15) ok = R1(f.hmaxd, 0);
    else if (ifi == 2 && ifj == 16) ok = R1(f.hcmaxd, 0);
    else if (ifi == 2 && ifj == 17) ok = R1(f.wbt, 0);
    else if (ifi == 2 && ifj == 19) ok = R1(f.wnmean, 0);
    else if (ifi == 3 && ifj == 1) ok = RC(f.ef, e3c0(1, f.ef_lb), e3c1(1, f.ef_lb));
    else if (ifi == 3 && ifj == 2) ok = RC(f.th1m, e3c0(2, f.th1m_lb), e3c1(2, f.th1m_lb));
    else if (ifi == 3 && ifj == 3) ok = RC(f.sth1m, e3c0(3, f.sth1m_lb), e3c1(3, f.sth1m_lb));
    else if (ifi == 3 && ifj == 4) ok = RC(f.th2m, e3c0(4, f.th2m_lb), e3c1(4, f.th2m_lb));
    else if (ifi == 3 && ifj == 5) ok = RC(f.sth2m, e3c0(5, f.sth2m_lb), e3c1(5, f.sth2m_lb));
    else if (ifi == 3 && ifj == 6) ok = RWN();
    else if (ifi == 4 && ifj == 1) ok = RC(f.phs, 0, nsw);
    else if (ifi == 4 && ifj == 2) ok = RC(f.ptp, 0, nsw);
    else if (ifi == 4 && ifj == 3) ok = RC(f.plp, 0, nsw);
    else if (ifi == 4 && ifj == 4) ok = RC(f.pdir, 0, nsw);
    else if (ifi == 4 && ifj == 5) ok = RC(f.psi, 0, nsw);
    else if (ifi == 4 && ifj == 6) ok = RC(f.pws, 0, nsw);
    else if (ifi == 4 && ifj == 7) ok = RC(f.pthp0, 0, nsw);
    else if (ifi == 4 && ifj == 8) ok = RC(f.pqp, 0, nsw);
    else if (ifi == 4 && ifj == 9) ok = RC(f.ppe, 0, nsw);
    else if (ifi == 4 && ifj == 10) ok = RC(f.pgw, 0, nsw);
    else if (ifi == 4 && ifj == 11) ok = RC(f.psw, 0, nsw);
    else if (ifi == 4 && ifj == 12) ok = RC(f.ptm1, 0, nsw);
    else if (ifi == 4 && ifj == 13) ok = RC(f.pt1, 0, nsw);
    else if (ifi == 4 && ifj == 14) ok = RC(f.pt2, 0, nsw);
    else if (ifi == 4 && ifj == 15) ok = RC(f.pep, 0, nsw);
    else if (ifi == 4 && ifj == 16) ok = R1(f.pwst, 0);
    else if (ifi == 4 && ifj == 17) ok = R1(f.pnr, 0);
    // UST and USTDIR receive the components UST*ASF*COS, *SIN the WRITE stored (:3865-3867).
    else if (ifi == 5 && ifj == 1) ok = RV(w.ust, 1) && mid() && RV(w.ustdir, 1);
    else if (ifi == 5 && ifj == 2) ok = R1(f.charn, 0);
    else if (ifi == 5 && ifj == 3) ok = R1(f.cge, 0);
    else if (ifi == 5 && ifj == 4) ok = R1(f.phiaw, 0);
    else if (ifi == 5 && ifj == 5) ok = R1(f.tauwix, 0) && mid() && R1(f.tauwiy, 0);
    else if (ifi == 5 && ifj == 6) ok = R1(f.tauwnx, 0) && mid() && R1(f.tauwny, 0);
    else if (ifi == 5 && ifj == 7) ok = RC(f.whitecap, 0, 0);
    else if (ifi == 5 && ifj == 8) ok = RC(f.whitecap, 1, 1);
    else if (ifi == 5 && ifj == 9) ok = RC(f.whitecap, 2, 2);
    else if (ifi == 5 && ifj == 10) ok = RC(f.whitecap, 3, 3);
    else if (ifi == 5 && ifj == 11) ok = R1(f.tws, 0);
    else if (ifi == 6 && ifj == 1)
      ok = R1(f.sxx, 0) && mid() && R1(f.syy, 0) && mid() && R1(f.sxy, 0);
    else if (ifi == 6 && ifj == 2) ok = R1(f.tauox, 0) && mid() && R1(f.tauoy, 0);
    else if (ifi == 6 && ifj == 3) ok = R1(f.bhd, 0);
    else if (ifi == 6 && ifj == 4) ok = R1(f.phioc, 0);
    else if (ifi == 6 && ifj == 5) ok = R1(f.tusx, 0) && mid() && R1(f.tusy, 0);
    else if (ifi == 6 && ifj == 6) ok = R1(f.ussx, 0) && mid() && R1(f.ussy, 0);
    else if (ifi == 6 && ifj == 7) ok = R1(f.prms, 0) && mid() && R1(f.tpms, 0);
    else if (ifi == 6 && ifj == 8)
      ok = RC(f.us3d, us0, us1) && mid() && RC(f.us3d, lnk + us0, lnk + us1);
    else if (ifi == 6 && ifj == 9) ok = RC(f.p2sms, p0, p1);
    else if (ifi == 6 && ifj == 10) ok = RC(f.tauice, 0, 0) && mid() && RC(f.tauice, 1, 1);
    else if (ifi == 6 && ifj == 11) ok = R1(f.phice, 0);
    else if (ifi == 6 && ifj == 12) ok = RC(f.ussp, 0, up1) && mid() && RC(f.ussp, lnk, lnk + up1);
    else if (ifi == 6 && ifj == 13) ok = R1(f.tauocx, 0) && mid() && R1(f.tauocy, 0);
    else if (ifi == 7 && ifj == 1) ok = R1(f.aba, 0) && mid() && R1(f.abd, 0);
    else if (ifi == 7 && ifj == 2) ok = R1(f.uba, 0) && mid() && R1(f.ubd, 0);
    else if (ifi == 7 && ifj == 3)
      ok = RC(f.bedforms, 0, 0) && mid() && RC(f.bedforms, 1, 1) && mid() &&
           RC(f.bedforms, 2, 2);
    else if (ifi == 7 && ifj == 4) ok = R1(f.phibbl, 0);
    else if (ifi == 7 && ifj == 5) ok = RC(f.taubbl, 0, 0) && mid() && RC(f.taubbl, 1, 1);
    else if (ifi == 8 && ifj == 1) ok = R1(f.mssx, 0) && mid() && R1(f.mssy, 0);
    else if (ifi == 8 && ifj == 2) ok = R1(f.mscx, 0) && mid() && R1(f.mscy, 0);
    else if (ifi == 8 && ifj == 3) ok = R1(f.mssd, 0);
    else if (ifi == 8 && ifj == 4) ok = R1(f.mscd, 0);
    else if (ifi == 8 && ifj == 5) ok = R1(f.qp, 0);
    else if (ifi == 8 && ifj == 6) ok = R1(f.qkk, 0);
    else if (ifi == 8 && ifj == 7) ok = R1(f.skew, 0);
    else if (ifi == 8 && ifj == 8) ok = R1(f.embia1, 0);
    else if (ifi == 8 && ifj == 9) ok = R1(f.embia2, 0);
    else if (ifi == 9 && ifj == 1) ok = R1(f.dtdyn, 0);
    else if (ifi == 9 && ifj == 2) ok = R1(f.fcut, 0);
    else if (ifi == 9 && ifj == 3) ok = R1(f.cflxymax, 0);
    else if (ifi == 9 && ifj == 4) ok = R1(f.cflthmax, 0);
    else if (ifi == 9 && ifj == 5) ok = R1(f.cflkmax, 0);
    else if (ifi == 10) ok = RC(f.usero, ifj - 1, ifj - 1);
    return ok && mid();  // :4009
  };

  for (int ifi = 1; ifi <= kNogrp; ++ifi)  // :3023-4022, IFJ inner
    for (int ifj = 1; ifj <= kNgrpp; ++ifj) {
      if (o.flogrd[static_cast<std::size_t>((ifi - 1) + kNogrp * (ifj - 1))] == 0) continue;
      if (o.write1 != 0) {
        if (!write_field(ifi, ifj)) return st;
      } else {
        if (!read_field(ifi, ifj)) return st;
      }
    }

  // ---- Flush, close, log (:4026-4044) -------------------------------------------------------
  if (o.write1 != 0 && ndsog.f) std::fflush(ndsog.f.get());  // CALL FLUSH(NDSOG), :4026
  if (o.ofiles[0] == 1) {                                     // CLOSE(NDSOG), :4028
    ndsog.f.reset();
    ndsog.readable = ndsog.writable = ndsog.after_endfile = false;
  }
  if (o.write1 != 0 && o.ofiles[0] == 1) {  // :4035-4044, on the same unit number
    const std::string lname = head(fnmpre_local, jlen) + "log." + timetag + ".out_grd." +
                              head(g.filext, ilen) + ".txt";
    st.filename = lname;
    std::unique_ptr<std::FILE, W3IogoUnit::Closer> lf(std::fopen(lname.c_str(), "w"));
    if (!lf) {
      const int err = errno;
      if (err == EACCES || err == EPERM || err == EROFS) {
        const std::unique_ptr<std::FILE, W3IogoUnit::Closer> ro(std::fopen(lname.c_str(), "r"));
        if (ro) {  // connected read-only: the WRITE at :4040 fails, no IOSTAT
          st.exit_code = kExitGfortranRuntime;
          return st;
        }
      }
      st.iostat = err != 0 ? err : 1;  // EXTOPN(NDSE, IERR, 'W3IOGO', '', 41), :4039
      st.exit_code = 41;
      return st;
    }
    std::string tt = timetag;  // TRIM(TIMETAG)
    while (!tt.empty() && tt.back() == ' ') tt.pop_back();
    // List-directed WRITE of one CHARACTER item: a leading blank, the string, a newline, no
    // wrap (v, gfortran 16.2 wrote a 210-character item on one line).
    const std::string line =
        " The " + tt + ".out_grd." + head(g.filext, ilen) + " file has been successfully written!\n";
    if (std::fwrite(line.data(), 1, line.size(), lf.get()) != line.size() ||
        std::fflush(lf.get()) != 0) {
      st.exit_code = kExitGfortranRuntime;
      return st;
    }
  }
  return st;
}

}  // namespace ww::iogo
