// kokkos/src/ww_kokkos/iors_w3iors.hpp
// W3IORS, ported: writes and reads the restart file restart.<ext> (the action spectrum of every
// sea point, and for a full restart the water level, ice, map, friction velocity and FPIS
// fields), or sets the cold-start fields when there is no file to read. Phase 1, translated,
// not wired into WW3, not regtested.
//
// WW3 heritage: SUBROUTINE W3IORS, WW3-noaa/model/src/w3iorsmd.F90:113-1600 at NOAA-EMC/WW3
// 03adda91 (identical in the h0ffmann/WW3 fork at the WW3/ pin: git diff 03adda91 HEAD is
// empty), author H. L. Tolman. Only what switches/switch_lab_shrd compiles is ported (SHRD, LN1,
// REF0; gfortran -E with the defines of WW3's flags.make): no W3_MPI (the I/O-server branch
// :701-777, the scattered read :799-857, the MPI_WAITALL :898-905), no W3_PDLIB
// (UNST_PDLIB_WRITE_TO_FILE, UNST_PDLIB_READ_FROM_FILE: with LPDLIB set both branches are
// empty here), no W3_WRST (the wind records :941-978, :1171-1192), no W3_REF1 (SET_UG_IOBP),
// no W3_TIMINGS, W3_T, W3_S. Under LN1 a missing file starts from calm (:608-611).
//
// File layout (kokkos/port_report/W3IORS_KOKKOS.md has the table): a stream file of fixed
// records of LRECL = MAX(4*NSPEC, 84) bytes (:459-462), record n at byte 1 + LRECL*(n-1).
// Record 1 the header, record 2 the time (full restart), records 3 .. NSEA+2 one spectrum
// each, then the level, ice, map, UST, USTDIR, ASF, FPIS records in parts of NSIZE = LRECL/4
// values, then (OARST) the coupling fields written without POS= from where the last part
// stopped. Every record is zero-filled (WRITEBUFF) before its data is written.
//
// Quirks reproduced, not corrected (v, read in the preprocessed source and the -O3 GIMPLE):
// - The header is 878 bytes (IDSTR 26, VERINI 10, GNAME 30, TYPE 4, NSEA, NSPEC, FLOGRR 200
//   LOGICALs), but the 84 of LRECL leaves out FLOGRR. With NSPEC < 220 the header runs into
//   record 2 and on, where the time record's or the spectra's zero fill, or FTCH's FETCH at
//   record 3, overwrites it, so
//   FLOGOA reads back what lies there.
// - The model's VA is clamped by VA = MAX(0.,VA) (:876) after a WRITE of type FULL or COLD,
//   not before: the file holds the unclamped spectra. A WRITE of type WIND, CALM or FTCH
//   returns at :673 without the clamp.
// - OPEN without STATUS keeps a longer old file's tail (iors_stream.hpp).
// - A READ that cannot open the file starts from calm (TYPE 'CALM', RSTYPE 4) without zeroing
//   VA; a READ of a file that opens but is empty or short exits 30 or 31 at EXTIOF.
// - With IFILE4 > 0 the name is 'restartNNN.<ext>', with NNN filled only for a WRITE on
//   process NAPRST (:499-501): a READ, or another process, uses the letters NNN.
// - FTCH is written at record 3 with no test that the unit is connected (:653-657).
// - In the OARST WRITE block a WRITE without its own IERR test is reported only when it is
//   the last before the test at :1131.
// - W3DIMW gets W3IORS's IMOD even when IMOD is absent (:451, :453; the GIMPLE passes imod's
//   pointer, null when absent).
//
// Arithmetic: the one floating-point operation is VA = MAX(0.,VA) (:876), a maxss with VA as
// destination and 0.0 as source (v, the object, `maxss %xmm1,%xmm0`): VA > 0 ? VA : 0, so a NaN
// or -0.0 becomes +0.0; va_clamp() is that statement. MAPTMP = MAPSTA + 8*MAPST2 (:980) and
// MAPSTA = MOD(MAPTMP+2,8)-2, MAPST2 = (MAPTMP-MAPSTA)/8 (:1204-1205) are the integer
// statements of W3IOGR, iogr::encode_maptmp/decode_maptmp. The rest are copies and fills:
// 0.0, 1.0 (0x3f800000), 0.0185 (0x3c978d50) and DUMFPI.
//
// Memory contract (phase 1). The arrays live in ExecSpace's memory space (IorsArrays). The
// whole-array assignments (VA(:,0) = 0, VA = 0, VA = MAX(0.,VA), the cold-start fills) are
// kernels or fills on exec; each READ or WRITE of a field stages its View through a host
// mirror (no copy when the space is host-accessible). Scalars and small arrays are host-side
// (IorsState). After an exit code the arrays are unspecified: the Fortran stops the program
// there.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <Kokkos_Core.hpp>

#include "iogr_state.hpp"
#include "iogr_w3iogr.hpp"
#include "iors_stream.hpp"
#include "real.hpp"

namespace ww::iors {

using ww::iogr::FChar;
using ww::iogr::FLogical;

inline constexpr int kNogrp = 10;  // NOGRP, w3odatmd.F90:330
inline constexpr int kNgrpp = 20;  // NGRPP, w3odatmd.F90:331
inline constexpr int kLrb = 4;     // LRB, :361 (local PARAMETER)
/// The second operand of LRECL's MAX, :459-461: INT of REAL quotients, 6+6+2+7+0 words.
inline constexpr int kLreclMin =
    kLrb * (6 + static_cast<int>(25.0f / kLrb) + static_cast<int>(9.0f / kLrb) +
            static_cast<int>(29.0f / kLrb) + static_cast<int>(3.0f / kLrb));
static_assert(kLreclMin == 84, "MAX_EXPR <_3193, 84> in the GIMPLE");

inline constexpr int kExitGfortranRuntime = ww::iogr::kExitGfortranRuntime;  // 2
inline constexpr int kExitMissingHook = ww::iogr::kExitMissingHook;
inline constexpr int kExitShape = ww::iogr::kExitShape;

/// Host state of W3IORS for grid IGRD, as W3SETO, W3SETG, W3SETW select it (:422-424): the
/// module scalars it reads or writes. LOGICALs are their int32 storage; every test of one in
/// the GIMPLE is `!= 0`. Defaults are WW3's initial values where WW3 gives one.
struct IorsState {
  // W3IORSMD
  FLogical oarst = 0;  // OARST, module LOGICAL (ww3_shel sets it to ANY(FLOGR))
  // W3ODATMD
  int ndse = 0, ndst = 0, screen = 6;
  int iaproc = 1, naproc = 1, naperr = 1, naprst = 1;
  int iostyp = 1;                          // w3odatmd.F90:328
  int ifile4 = 0;                          // OUTPTS(IGRD)%OUT4%IFILE4, INOUT
  FChar<80> fnmpre{std::string("./")};     // w3odatmd.F90:337
  FChar<256> fnmrst{std::string("./")};    // w3odatmd.F90:342
  std::array<FLogical, kNogrp * kNgrpp> flogrr{};  // FLOGRR(NOGRP,NGRPP), (I,J) at (J-1)*10+I-1
  // W3GDATMD
  int nx = 0, ny = 0, nsea = 0, nspec = 0, gtype = 0;
  FChar<30> gname;
  FChar<13> filext;
  int rstype = -1;   // OUT, w3gdatmd.F90:646
  float fetch = 0;   // FETCH, INOUT
  // W3WDATMD
  FLogical dinit = 0;
  std::array<int, 2> time{}, tlev{}, tice{}, trho{}, tic1{}, tic5{};
  // W3ADATMD
  int nsealm = 0;
  // CONSTANTS
  FLogical lpdlib = 0;  // constants.F90:102
};

/// The module arrays W3IORS transfers, in MemSpace, LayoutLeft (Fortran order), with the
/// shapes of their ALLOCATE statements. A(lb:ub) is a(I - lb).
template <class MemSpace>
struct IorsArrays {
  using R1 = Kokkos::View<Real*, Kokkos::LayoutLeft, MemSpace>;
  using R2 = Kokkos::View<Real**, Kokkos::LayoutLeft, MemSpace>;
  using I2 = Kokkos::View<int**, Kokkos::LayoutLeft, MemSpace>;
  // W3WDATMD (w3wdatmd.F90:491, 561-573)
  R2 va;           ///< VA(NSPEC,0:NSEALM): va(ISPEC-1, JSEA)
  R1 wlv;          ///< WLV(NSEA): wlv(ISEA-1)
  R1 ice, icef;    ///< ICE(0:NSEA), ICEF(0:NSEA): x(ISEA)
  R1 ust, ustdir;  ///< UST(0:NSEATM), USTDIR(0:NSEATM): x(ISEA)
  R1 asf, fpis;    ///< ASF(NSEATM), FPIS(NSEATM): x(ISEA-1)
  // W3GDATMD
  I2 mapsta, mapst2;  ///< (NY,NX): x(IY-1, IX-1)
  // W3ADATMD (w3adatmd.F90:1008, 1041-1052, 1161-1252), the OARST fields
  R1 cx, cy;  ///< (0:NSEA): x(ISEA)
  R1 hs, wlm, t0m1, t01, fp0, thm, wnmean, charn, tauwix, tauwiy, tws, tauox, tauoy, bhd,
      phioc, tusx, tusy, ussx, ussy, tauocx, tauocy, uba, ubd, phibbl;  ///< (NSEALM): x(I-1)
  R2 tauice, taubbl;  ///< (NSEALM,2): x(I-1, K-1)
};

/// One message the Fortran writes, by FORMAT label, with its values; the caller prints it,
/// on NDSE when IAPROC equals NAPERR (on SCREEN for 1000), a kernel never does.
struct IorsMessage {
  int fmt = 0;                 // 900-906, 990, 1000; 15 for the '(A,I8)' of :514
  std::string a, b;            // the A items
  std::array<int, 4> i{};      // the I items, in order
};

/// What W3IORS ends with. exit_code 0: it returned. Otherwise the EXTCDE code (EXTIOF: read
/// IERR > 0 gives 31, IERR < 0 gives 30; write gives 31 for any IERR, w3servmd.F90:895-923)
/// with the message; warnings holds the messages of the paths that go on (903, 990, 1000).
struct IorsStatus {
  int exit_code = 0;
  IorsMessage msg;
  int iostat = 0;            // IERR handed to EXTIOF
  bool extiof_write = false; // ISWRITE=.TRUE. (FORMAT 903 of EXTIOF with POS)
  std::int64_t pos = 0;      // POS= of that EXTIOF
  std::vector<IorsMessage> warnings;
  std::string filename;      // FNMPRE_LOCAL(:J)//FNAME, trailing blanks dropped as OPEN does
};

/// W3DIMW(IMOD, NDSE, NDST[, F_ONLY]) at :451 (f_only null) and :453 (f_only -> .FALSE.):
/// allocates the W3WDATMD arrays of the grid and sets DINIT. imod is W3IORS's IMOD pointer,
/// null when IMOD is absent. Returns 0, or the EXTCDE code it ends with.
template <class MemSpace>
struct IorsHooks {
  std::function<int(IorsState&, IorsArrays<MemSpace>&, const int* imod, const FLogical* f_only)>
      w3dimw;
};

// ---- Element statements and launchers ---------------------------------------------------

/// VA = MAX(0.,VA), :876, as the object computes it: maxss, VA destination, 0.0 source.
KOKKOS_INLINE_FUNCTION Real va_clamp(Real v) { return v > Real(0) ? v : Real(0); }

/// VA = MAX(0.,VA) over the whole VA(NSPEC,0:NSEALM). Element-wise, no reduction.
template <class ExecSpace>
void iors_va_clamp(const ExecSpace& space,
                   const Kokkos::View<Real**, Kokkos::LayoutLeft,
                                      typename ExecSpace::memory_space>& va) {
  const auto n0 = static_cast<std::int64_t>(va.extent(0));
  const auto n1 = static_cast<std::int64_t>(va.extent(1));
  Kokkos::parallel_for(
      "ww::iors::va_clamp",
      Kokkos::MDRangePolicy<ExecSpace, Kokkos::Rank<2>>(space, {0, 0}, {n0, n1}),
      KOKKOS_LAMBDA(const std::int64_t i, const std::int64_t j) { va(i, j) = va_clamp(va(i, j)); });
  space.fence("ww::iors::va_clamp");
}

/// VA(:,0) = 0., :457.
template <class ExecSpace>
void iors_va_col0_zero(const ExecSpace& space,
                       const Kokkos::View<Real**, Kokkos::LayoutLeft,
                                          typename ExecSpace::memory_space>& va) {
  Kokkos::parallel_for(
      "ww::iors::va_col0_zero", Kokkos::RangePolicy<ExecSpace>(space, 0, va.extent(0)),
      KOKKOS_LAMBDA(const std::size_t i) { va(i, 0) = Real(0); });
  space.fence("ww::iors::va_col0_zero");
}

namespace detail {

/// Iw.m edit descriptor of formatted output: m digits at least, a minus sign when negative,
/// right-justified in w, w asterisks when it does not fit (v, the gfortran program: I8.8 of -1
/// and of 123456789 give ********, I3.3 of 1000 and of -5 give ***).
inline std::string fmt_iwm(long long v, std::size_t w, std::size_t m) {
  const bool neg = v < 0;
  std::string d = std::to_string(neg ? -v : v);
  if (d.size() < m) d.insert(0, m - d.size(), '0');
  if (neg) d.insert(0, 1, '-');
  if (d.size() > w) return std::string(w, '*');
  return std::string(w - d.size(), ' ') + d;
}

/// The host copy of a View, read from exec.
template <class ExecSpace, class V>
auto host_copy(const ExecSpace& exec, const V& v) {
  auto h = Kokkos::create_mirror_view(v);
  Kokkos::deep_copy(exec, h, v);
  exec.fence("ww::iors::host_copy");
  return h;
}
template <class ExecSpace, class V, class H>
void to_space(const ExecSpace& exec, const V& v, const H& h) {
  Kokkos::deep_copy(exec, v, h);
  exec.fence("ww::iors::to_space");
}

}  // namespace detail

// ---- W3IORS ------------------------------------------------------------------------------

/// W3IORS(INXOUT, NDSR, DUMFPI, IMOD, FLRSTRT), w3iorsmd.F90:113-1600. inxout is 'READ',
/// 'HOT', 'COLD', 'WIND', 'CALM' or 'FTCH'; ndsr the unit number (only compared with NDST
/// here: the unit is this call's own file descriptor); s and a the state of grid IGRD = imod
/// (1 when absent, :416-420), already selected as W3SETO, W3SETG and W3SETW select it.
/// @pre When DINIT is set, or after the W3DIMW hook, the arrays have the extents of their
///      comments in IorsArrays (checked where they are indexed; kExitShape otherwise).
template <class ExecSpace>
IorsStatus w3iors(const ExecSpace& exec, const std::string& inxout, int ndsr, float& dumfpi,
                  IorsState& s, IorsArrays<typename ExecSpace::memory_space>& a,
                  const IorsHooks<typename ExecSpace::memory_space>& hooks,
                  std::optional<int> imod = std::nullopt,
                  std::optional<FLogical> flrstrt = std::nullopt) {
  using ww::iogr::detail::fstr_eq;
  IorsStatus st;
  const bool iosflg = s.iostyp > 0;  // :412

  // ---- Parameter list, :416-446 --------------------------------------------------------
  if (!fstr_eq(inxout, "READ") && !fstr_eq(inxout, "HOT") && !fstr_eq(inxout, "COLD") &&
      !fstr_eq(inxout, "WIND") && !fstr_eq(inxout, "CALM") && !fstr_eq(inxout, "FTCH")) {
    st.exit_code = 1;  // :429-434
    st.msg.fmt = 900;
    st.msg.a = inxout;
    return st;
  }
  const bool write = !fstr_eq(inxout, "READ");  // :436
  FChar<4> type{fstr_eq(inxout, "HOT") ? std::string("FULL") : inxout};  // :437-441

  // ---- Initialisations, :449-462 -------------------------------------------------------
  if (s.dinit == 0) {
    if (!hooks.w3dimw) {
      st.exit_code = kExitMissingHook;
      return st;
    }
    const int imod_v = imod ? *imod : 0;
    const FLogical f_false = 0;
    const int code = hooks.w3dimw(s, a, imod ? &imod_v : nullptr,
                                  s.iaproc <= s.naproc ? nullptr : &f_false);
    if (code != 0) {
      st.exit_code = code;
      return st;
    }
  }
  {
    const auto nsea = static_cast<std::size_t>(s.nsea);
    const bool ok =
        a.va.extent(0) == static_cast<std::size_t>(s.nspec) && a.va.extent(1) >= nsea + 1 &&
        a.wlv.extent(0) >= nsea && a.ice.extent(0) >= nsea + 1 && a.ust.extent(0) >= nsea + 1 &&
        a.ustdir.extent(0) >= nsea + 1 && a.asf.extent(0) >= nsea && a.fpis.extent(0) >= nsea &&
        a.mapsta.extent(0) == static_cast<std::size_t>(s.ny) &&
        a.mapsta.extent(1) == static_cast<std::size_t>(s.nx) &&
        a.mapst2.extent(0) == a.mapsta.extent(0) && a.mapst2.extent(1) == a.mapsta.extent(1);
    if (!ok) {
      st.exit_code = kExitShape;
      return st;
    }
  }
  if (s.iaproc <= s.naproc) iors_va_col0_zero(exec, a.va);  // :457

  const int lrecl = std::max(kLrb * s.nspec, kLreclMin);  // :459-461
  const int nsize = lrecl / kLrb;                          // :462 (>> 2 in the GIMPLE)
  auto rpos_of = [lrecl](int nrec) {                       // 1_8 + LRECL*(NREC-1_8)
    return std::int64_t{1} + static_cast<std::int64_t>(lrecl) *
                                 (static_cast<std::int64_t>(nrec) - std::int64_t{1});
  };
  Bytes writebuff;  // WRITEBUFF(NSIZE) = 0., :465-466
  writebuff.zeros(static_cast<std::size_t>(lrecl));

  // ---- File name, :476-505 -------------------------------------------------------------
  FChar<256> fnmpre_local;
  if (s.fnmrst.len_trim() == 0) {
    fnmpre_local.assign(s.fnmpre.trimmed());
  } else {
    fnmpre_local.assign(s.fnmrst.trimmed());
  }
  const std::string ext = s.filext.trimmed();  // FILEXT(:I)
  const std::size_t j = fnmpre_local.len_trim();
  FChar<40> fname;
  int itmp = 0;
  if (flrstrt && *flrstrt != 0) {  // :488-494
    const std::string timetag = detail::fmt_iwm(s.time[0], 8, 8) + "." +
                                detail::fmt_iwm(s.time[1], 6, 6);  // (i8.8,'.'i6.6)
    fname.assign(timetag + ".restart." + ext);
    itmp = 1;
  }
  if (itmp != 1) {  // :495-503
    if (s.ifile4 == 0) {
      fname.assign("restart." + ext);
    } else {
      fname.assign("restartNNN." + ext);
      if (write && s.iaproc == s.naprst) {
        const std::string nnn = detail::fmt_iwm(s.ifile4, 3, 3);  // (I3.3) into FNAME(8:10)
        for (std::size_t k = 0; k < 3; ++k) fname.c[7 + k] = nnn[k];
      }
    }
  }
  s.ifile4 = s.ifile4 + 1;  // :505

  if (s.ndst == ndsr) {  // :512-517
    st.exit_code = 15;
    st.msg.fmt = 15;
    st.msg.a = "UNIT NUMBERS OF RESTART FILE AND TEST OUTPUT ARE THE SAME : ";
    st.msg.i[0] = s.ndst;
    return st;
  }

  // FILE= is FNMPRE_LOCAL(:J)//FNAME; OPEN drops the trailing blanks (v, the program).
  st.filename = std::string(fnmpre_local.c.data(), j) + fname.trimmed();
  StreamUnit u;
  int ierr = 0;
  if (write) {  // :519-527
    ierr = 0;
    if (!iosflg || s.iaproc == s.naprst) ierr = u.open_unknown(st.filename);
  } else {
    ierr = u.open_old_read(st.filename);
  }

  auto extiof_read = [&](int e) {  // EXTIOF(NDSE,IERR,'W3IORS','',30)
    st.iostat = e;
    st.exit_code = e > 0 ? 31 : 30;
    return st;
  };
  auto extiof_write = [&](int e, std::int64_t pos) {  // EXTIOF(...,31,ISWRITE=.TRUE.,POS=RPOS)
    st.iostat = e;
    st.extiof_write = true;
    st.pos = pos;
    st.exit_code = 31;
    return st;
  };
  auto runtime_write = [&](int e) {  // a WRITE without IOSTAT: gfortran's exit_error(2)
    st.iostat = e;
    st.exit_code = kExitGfortranRuntime;
    return st;
  };

  // ---- Header, :530-614 ----------------------------------------------------------------
  std::array<FLogical, kNogrp * kNgrpp> flogoa{};
  const FChar<26> idstr{std::string("WAVEWATCH III RESTART FILE")};  // IDSTR, :74-75
  const FChar<10> verini{std::string("2024-04-26")};                 // VERINI, :73
  if (ierr == 0) {
    if (write) {
      if (s.iaproc == s.naprst) {
        int e = u.write_at(1, writebuff);  // :540-541
        if (e != 0) return runtime_write(e);
        Bytes h;  // :543-544
        h.chars(idstr.c.data(), 26);
        h.chars(verini.c.data(), 10);
        h.chars(s.gname.c.data(), 30);
        h.chars(type.c.data(), 4);
        h.scalar(s.nsea);
        h.scalar(s.nspec);
        for (const FLogical l : s.flogrr) h.scalar(l);
        e = u.write_at(1, h);
        if (e != 0) return runtime_write(e);
      }
      s.rstype = 3;  // :546
    } else {
      std::vector<unsigned char> raw;  // :549-551
      const int e = u.read_at(1, raw, 26 + 10 + 30 + 4 + 4 + 4 + 4 * kNogrp * kNgrpp);
      if (e != 0) return extiof_read(e);
      FChar<26> idtst;
      FChar<10> vertst;
      FChar<30> tname;
      int nseat = 0, mspec = 0;
      Unpack p(raw);
      p.chars(idtst.c.data(), 26);
      p.chars(vertst.c.data(), 10);
      p.chars(tname.c.data(), 30);
      p.chars(type.c.data(), 4);
      p.scalar(nseat);
      p.scalar(mspec);
      for (FLogical& l : flogoa) p.scalar(l);
      if (!(idtst == idstr)) {  // :553-557
        st.exit_code = 10;
        st.msg = {901, std::string(idtst.c.data(), 26), std::string(idstr.c.data(), 26), {}};
        return st;
      }
      if (!(vertst == verini)) {  // :558-562
        st.exit_code = 11;
        st.msg = {902, std::string(vertst.c.data(), 10), std::string(verini.c.data(), 10), {}};
        return st;
      }
      if (!(tname == s.gname)) {  // :563-566, a warning
        st.warnings.push_back(
            {903, std::string(tname.c.data(), 30), std::string(s.gname.c.data(), 30), {}});
      }
      const std::string ty(type.c.data(), 4);
      if (ty != "FULL" && ty != "COLD" && ty != "WIND" && ty != "CALM" && ty != "FTCH") {
        st.exit_code = 12;  // :567-572
        st.msg = {904, ty, {}, {}};
        return st;
      }
      if (nseat != s.nsea || s.nspec != mspec) {  // :573-577
        st.exit_code = 13;
        st.msg = {905, {}, {}, {mspec, nseat, s.nspec, s.nsea}};
        return st;
      }
      if (ty == "FULL") {  // :578-586
        s.rstype = 2;
      } else if (ty == "WIND" || ty == "FTCH") {
        s.rstype = 1;
      } else if (ty == "CALM") {
        s.rstype = 4;
      } else {
        s.rstype = 0;
      }
      if (!write && s.oarst != 0 && s.iaproc == s.naproc) {  // :588-596
        for (int i = 1; i <= kNogrp; ++i) {
          for (int jj = 1; jj <= kNgrpp; ++jj) {
            const std::size_t k = static_cast<std::size_t>((jj - 1) * kNogrp + i - 1);
            if (s.flogrr[k] != 0 && !(flogoa[k] != 0)) st.warnings.push_back({1000, {}, {}, {i, jj}});
          }
        }
      }
    }
  } else {
    type.assign("CALM");  // LN1, :608-611
    s.rstype = 4;
    st.warnings.push_back({990, "CALM", {}, {ierr}});  // :612
  }
  const std::string ty(type.c.data(), 4);
  const bool full = ty == "FULL";

  // ---- Time, :623-640 ------------------------------------------------------------------
  if (full) {
    const std::int64_t rpos = rpos_of(2);  // :624
    if (write) {
      if (s.iaproc == s.naprst) {  // :626-630, no IOSTAT
        int e = u.write_at(rpos, writebuff);
        if (e != 0) return runtime_write(e);
        Bytes t;
        t.scalar(s.time[0]);
        t.scalar(s.time[1]);
        e = u.write_at(rpos, t);
        if (e != 0) return runtime_write(e);
      }
    } else {
      std::vector<unsigned char> raw;  // :632-638
      const int e = u.read_at(rpos, raw, 8);
      if (e != 0) return extiof_read(e);
      std::array<int, 2> ttime{};
      Unpack p(raw);
      p.scalar(ttime[0]);
      p.scalar(ttime[1]);
      if (s.time[0] != ttime[0] || s.time[1] != ttime[1]) {
        st.exit_code = 20;
        st.msg = {906, {}, {}, {ttime[0], ttime[1], s.time[0], s.time[1]}};
        return st;
      }
    }
  }

  // ---- Spectra, :652-876 ---------------------------------------------------------------
  if (write) {
    if (ty == "FTCH") {  // :653-658
      const std::int64_t rpos = rpos_of(3);
      Bytes f;
      f.scalar(s.fetch);
      int e;
      if (u.connected()) {
        e = u.write_at(rpos, f);
      } else {
        // An unconnected unit: gfortran opens fort.<NDSR> for sequential access, which refuses
        // POS= (v, the program: an empty fort.61 and IOSTAT 5002).
        const std::string fort = "fort." + std::to_string(ndsr);
        const int fd = ::open(fort.c_str(), O_RDWR | O_CREAT | O_CLOEXEC,
                              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd >= 0) ::close(fd);
        e = kIostatBadOption;
      }
      if (e != 0) return extiof_write(e, rpos);
    }
    if (ty == "WIND" || ty == "CALM" || ty == "FTCH") {  // :659-673
      u.close();
      return st;
    } else if (s.iaproc <= s.naproc || s.iaproc == s.naprst) {  // :674
      if (!iosflg || (s.naproc == 1 && s.naprst == 1)) {         // :678, the SHRD loop :691-700
        const auto h_va = detail::host_copy(exec, a.va);
        for (int jsea = 1; jsea <= s.nsea; ++jsea) {
          const int isea = jsea;
          const int nrec = isea + 2;
          const std::int64_t rpos = rpos_of(nrec);
          Bytes w;  // WRITEBUFF(:) = 0.; WRITEBUFF(1:NSPEC) = VA(1:NSPEC,JSEA)
          for (int i = 0; i < s.nspec; ++i) w.scalar(h_va(static_cast<std::size_t>(i), jsea));
          w.zeros(static_cast<std::size_t>(nsize - s.nspec) * 4);
          const int e = u.write_at(rpos, w);
          if (e != 0) return extiof_write(e, rpos);
        }
      }
    }
  } else {
    if (ty == "FTCH") {  // :786-792
      std::vector<unsigned char> raw;
      const int e = u.read_at(rpos_of(3), raw, 4);
      if (e != 0) return extiof_read(e);
      Unpack p(raw);
      p.scalar(s.fetch);
    } else if (ty == "WIND") {
      s.fetch = 0.0f;
    }
    if (ty == "WIND" || ty == "CALM" || ty == "FTCH") {  // :793
    } else {
      if (s.lpdlib != 0) {  // :798, W3_PDLIB not compiled: empty
      } else {
        auto h_va = Kokkos::create_mirror_view(a.va);  // :860, VA = 0.
        Kokkos::deep_copy(h_va, Real(0));
        for (int jsea = 1; jsea <= s.nsea; ++jsea) {  // :861-868
          const int isea = jsea;  // INIT_GET_ISEA under SHRD, w3parall.F90:1484
          const int nrec = isea + 2;
          std::vector<unsigned char> raw;
          const int e = u.read_at(rpos_of(nrec), raw, static_cast<std::size_t>(s.nspec) * 4);
          if (e != 0) return extiof_read(e);
          Unpack p(raw);
          for (int i = 0; i < s.nspec; ++i) p.scalar(h_va(static_cast<std::size_t>(i), jsea));
        }
        detail::to_space(exec, a.va, h_va);
      }
    }
  }

  iors_va_clamp(exec, a.va);  // :876

  // ---- Water level etc., :887-1521 -----------------------------------------------------
  int nrec = s.nsea + 3;                       // :887
  const int npart = 1 + (s.nsea - 1) / nsize;  // :888
  const int nprtx2 = 1 + (s.nx - 1) / nsize;   // :889; NPRTY2 (:890) is used by W3_WRST only

  // The NPART parts of one per-point field, :916-927 (WRITE) / :1155-1162 (READ).
  auto parts_write = [&](const auto& h, int lb) -> bool {
    for (int ipart = 1; ipart <= npart; ++ipart) {
      nrec = nrec + 1;
      const std::int64_t rpos = rpos_of(nrec);
      int e = u.write_at(rpos, writebuff);
      if (e != 0) {
        extiof_write(e, rpos);
        return false;
      }
      Bytes w;
      const int last = std::min(s.nsea, ipart * nsize);
      for (int isea = 1 + (ipart - 1) * nsize; isea <= last; ++isea)
        w.scalar(h(static_cast<std::size_t>(isea - lb)));
      e = u.write_at(rpos, w);
      if (e != 0) {
        extiof_write(e, rpos);
        return false;
      }
    }
    return true;
  };
  auto parts_read = [&](const auto& h, int lb) -> bool {
    for (int ipart = 1; ipart <= npart; ++ipart) {
      nrec = nrec + 1;
      const std::int64_t rpos = rpos_of(nrec);
      const int first = 1 + (ipart - 1) * nsize;
      const int last = std::min(s.nsea, ipart * nsize);
      std::vector<unsigned char> raw;
      const int e = u.read_at(rpos, raw, static_cast<std::size_t>(last >= first ? last - first + 1 : 0) * 4);
      if (e != 0) {
        extiof_read(e);
        return false;
      }
      Unpack p(raw);
      for (int isea = first; isea <= last; ++isea) p.scalar(h(static_cast<std::size_t>(isea - lb)));
    }
    return true;
  };

  // The OARST fields: one WRITE or READ without POS= of X(1:NSEA), :1044-1133 / :1250-1461.
  auto seq_write = [&](const auto& h, int lb, int col) {
    Bytes w;
    for (int isea = 1; isea <= s.nsea; ++isea) {
      if constexpr (std::remove_cvref_t<decltype(h)>::rank == 1) {
        w.scalar(h(static_cast<std::size_t>(isea - lb)));
      } else {
        w.scalar(h(static_cast<std::size_t>(isea - lb), static_cast<std::size_t>(col)));
      }
    }
    return u.write_next(w);
  };
  std::vector<float> tmp, tmp2;  // TMP(NSEA), TMP2(NSEA), allocated when OARST (:469-472)
  if (s.oarst != 0) {
    tmp.assign(static_cast<std::size_t>(s.nsea), 0.0f);
    tmp2.assign(static_cast<std::size_t>(s.nsea), 0.0f);
  }
  auto seq_read = [&](std::vector<float>& t) {
    std::vector<unsigned char> raw;
    const int e = u.read_next(raw, static_cast<std::size_t>(s.nsea) * 4);
    if (e == 0) {
      Unpack p(raw);
      for (int isea = 0; isea < s.nsea; ++isea) p.scalar(t[static_cast<std::size_t>(isea)]);
    }
    return e;
  };
  auto oarst_shapes_ok = [&]() {
    const auto ns = static_cast<std::size_t>(s.nsea);
    const auto nl = static_cast<std::size_t>(write ? s.nsea : s.nsealm);
    bool ok = a.cx.extent(0) >= ns + 1 && a.cy.extent(0) >= ns + 1 && a.icef.extent(0) >= ns + 1;
    for (const auto* v : {&a.hs, &a.wlm, &a.t0m1, &a.t01, &a.fp0, &a.thm, &a.wnmean, &a.charn,
                          &a.tauwix, &a.tauwiy, &a.tws, &a.tauox, &a.tauoy, &a.bhd, &a.phioc,
                          &a.tusx, &a.tusy, &a.ussx, &a.ussy, &a.tauocx, &a.tauocy, &a.uba,
                          &a.ubd, &a.phibbl})
      ok = ok && v->extent(0) >= nl;
    ok = ok && a.tauice.extent(0) >= nl && a.tauice.extent(1) == 2 &&
         a.taubbl.extent(0) >= nl && a.taubbl.extent(1) == 2;
    return ok;
  };

  if (write) {
    if (full && s.iaproc == s.naprst) {  // :894-896
      std::int64_t rpos = rpos_of(nrec);  // :907-915
      int e = u.write_at(rpos, writebuff);
      if (e != 0) return extiof_write(e, rpos);
      Bytes lv;
      for (const auto* t : {&s.tlev, &s.tice, &s.trho, &s.tic1, &s.tic5}) {
        lv.scalar((*t)[0]);
        lv.scalar((*t)[1]);
      }
      e = u.write_at(rpos, lv);
      if (e != 0) return extiof_write(e, rpos);
      if (!parts_write(detail::host_copy(exec, a.wlv), 1)) return st;  // :916-927
      if (!parts_write(detail::host_copy(exec, a.ice), 0)) return st;  // :928-939
      {  // :979-995, MAPTMP = MAPSTA + 8*MAPST2, one record per row and part
        const auto hs = detail::host_copy(exec, a.mapsta);
        const auto h2 = detail::host_copy(exec, a.mapst2);
        for (int iy = 1; iy <= s.ny; ++iy) {
          for (int ipart = 1; ipart <= nprtx2; ++ipart) {
            nrec = nrec + 1;
            rpos = rpos_of(nrec);
            e = u.write_at(rpos, writebuff);
            if (e != 0) return extiof_write(e, rpos);
            Bytes w;
            const int last = std::min(s.nx, ipart * nsize);
            for (int ixl = 1 + (ipart - 1) * nsize; ixl <= last; ++ixl) {
              const auto yi = static_cast<std::size_t>(iy - 1), xi = static_cast<std::size_t>(ixl - 1);
              const std::int32_t m = ww::iogr::encode_maptmp(hs(yi, xi), h2(yi, xi));
              w.scalar(m);
            }
            e = u.write_at(rpos, w);
            if (e != 0) return extiof_write(e, rpos);
          }
        }
      }
      if (!parts_write(detail::host_copy(exec, a.ust), 0)) return st;     // :996-1007
      if (!parts_write(detail::host_copy(exec, a.ustdir), 0)) return st;  // :1008-1019
      if (!parts_write(detail::host_copy(exec, a.asf), 1)) return st;     // :1020-1031
      if (!parts_write(detail::host_copy(exec, a.fpis), 1)) return st;    // :1032-1043
      if (s.oarst != 0) {                                                 // :1044-1133
        if (!oarst_shapes_ok()) {
          st.exit_code = kExitShape;
          return st;
        }
        rpos = rpos_of(nrec);  // RPOS of the last FPIS part (:1034): the POS= EXTIOF reports below
        auto fl = [&](int i, int jj) {
          return s.flogrr[static_cast<std::size_t>((jj - 1) * kNogrp + i - 1)] != 0;
        };
        // Each X(1:NSEA) below is one WRITE; IERR is tested where the Fortran tests it.
        auto w1 = [&](const auto& v, int lb) { return seq_write(detail::host_copy(exec, v), lb, 0); };
        auto w2 = [&](const auto& v, int col) { return seq_write(detail::host_copy(exec, v), 1, col); };
        if (fl(1, 2)) {  // :1049-1056
          ierr = w1(a.cx, 0);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w1(a.cy, 0);
          if (ierr != 0) return extiof_write(ierr, rpos);
        }
        if (fl(1, 12)) ierr = w1(a.icef, 0);   // :1057-1058
        if (fl(2, 1)) ierr = w1(a.hs, 1);      // :1059-1060
        if (fl(2, 2)) ierr = w1(a.wlm, 1);     // :1061-1062
        if (fl(2, 4)) ierr = w1(a.t0m1, 1);    // :1063-1064
        if (fl(2, 5)) ierr = w1(a.t01, 1);     // :1065-1066
        if (fl(2, 6)) ierr = w1(a.fp0, 1);     // :1067-1068
        if (fl(2, 7)) ierr = w1(a.thm, 1);     // :1069-1070
        if (fl(2, 19)) ierr = w1(a.wnmean, 1); // :1071-1072
        if (fl(5, 2)) ierr = w1(a.charn, 1);   // :1073-1074
        if (fl(5, 5)) {                        // :1075-1080
          ierr = w1(a.tauwix, 1);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w1(a.tauwiy, 1);
        }
        if (fl(5, 11)) ierr = w1(a.tws, 1);    // :1081-1082
        if (fl(6, 2)) {                        // :1083-1088
          ierr = w1(a.tauox, 1);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w1(a.tauoy, 1);
        }
        if (fl(6, 3)) ierr = w1(a.bhd, 1);     // :1089-1090
        if (fl(6, 4)) ierr = w1(a.phioc, 1);   // :1091-1092
        if (fl(6, 5)) {                        // :1093-1098
          ierr = w1(a.tusx, 1);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w1(a.tusy, 1);
        }
        if (fl(6, 6)) {                        // :1099-1104
          ierr = w1(a.ussx, 1);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w1(a.ussy, 1);
        }
        if (fl(6, 10)) {                       // :1105-1110
          ierr = w2(a.tauice, 0);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w2(a.tauice, 1);
        }
        if (fl(6, 13)) {                       // :1111-1116
          ierr = w1(a.tauocx, 1);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w1(a.tauocy, 1);
        }
        if (fl(7, 2)) {                        // :1117-1122
          ierr = w1(a.uba, 1);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w1(a.ubd, 1);
        }
        if (fl(7, 4)) ierr = w1(a.phibbl, 1);  // :1123-1124
        if (fl(7, 5)) {                        // :1125-1130
          ierr = w2(a.taubbl, 0);
          if (ierr != 0) return extiof_write(ierr, rpos);
          ierr = w2(a.taubbl, 1);
        }
        if (ierr != 0) return extiof_write(ierr, rpos);  // :1131-1132
      }
    }
  } else {
    if (full) {  // :1150-1461
      std::int64_t rpos = rpos_of(nrec);
      {
        std::vector<unsigned char> raw;  // :1151-1154
        const int e = u.read_at(rpos, raw, 40);
        if (e != 0) return extiof_read(e);
        Unpack p(raw);
        for (auto* t : {&s.tlev, &s.tice, &s.trho, &s.tic1, &s.tic5}) {
          p.scalar((*t)[0]);
          p.scalar((*t)[1]);
        }
      }
      {
        auto h = detail::host_copy(exec, a.wlv);  // :1155-1162
        if (!parts_read(h, 1)) return st;
        detail::to_space(exec, a.wlv, h);
      }
      {
        auto h = detail::host_copy(exec, a.ice);  // :1163-1170
        if (!parts_read(h, 0)) return st;
        detail::to_space(exec, a.ice, h);
      }
      {  // :1193-1206
        std::vector<std::int32_t> maptmp(static_cast<std::size_t>(s.ny) * static_cast<std::size_t>(s.nx), 0);
        for (int iy = 1; iy <= s.ny; ++iy) {
          for (int ipart = 1; ipart <= nprtx2; ++ipart) {
            nrec = nrec + 1;
            rpos = rpos_of(nrec);
            const int first = 1 + (ipart - 1) * nsize;
            const int last = std::min(s.nx, ipart * nsize);
            std::vector<unsigned char> raw;
            const int e = u.read_at(rpos, raw, static_cast<std::size_t>(last >= first ? last - first + 1 : 0) * 4);
            if (e != 0) return extiof_read(e);
            Unpack p(raw);
            for (int ixl = first; ixl <= last; ++ixl)
              p.scalar(maptmp[static_cast<std::size_t>(iy - 1) +
                              static_cast<std::size_t>(s.ny) * static_cast<std::size_t>(ixl - 1)]);
          }
        }
        auto hs = Kokkos::create_mirror_view(a.mapsta);  // MAPSTA, MAPST2 = whole arrays
        auto h2 = Kokkos::create_mirror_view(a.mapst2);
        for (int ix = 0; ix < s.nx; ++ix) {
          for (int iy = 0; iy < s.ny; ++iy) {
            std::int32_t ms = 0, m2 = 0;
            ww::iogr::decode_maptmp(
                maptmp[static_cast<std::size_t>(iy) + static_cast<std::size_t>(s.ny) * static_cast<std::size_t>(ix)], ms, m2);
            hs(static_cast<std::size_t>(iy), static_cast<std::size_t>(ix)) = ms;
            h2(static_cast<std::size_t>(iy), static_cast<std::size_t>(ix)) = m2;
          }
        }
        detail::to_space(exec, a.mapsta, hs);
        detail::to_space(exec, a.mapst2, h2);
        // :1210-1216: GTYPE = UNGTYPE does nothing (SET_UG_IOBP is commented out; REF1 off).
      }
      {
        auto h = detail::host_copy(exec, a.ust);  // :1218-1225
        if (!parts_read(h, 0)) return st;
        detail::to_space(exec, a.ust, h);
      }
      {
        auto h = detail::host_copy(exec, a.ustdir);  // :1226-1233
        if (!parts_read(h, 0)) return st;
        detail::to_space(exec, a.ustdir, h);
      }
      {
        auto h = detail::host_copy(exec, a.asf);  // :1234-1241
        if (!parts_read(h, 1)) return st;
        detail::to_space(exec, a.asf, h);
      }
      {
        auto h = detail::host_copy(exec, a.fpis);  // :1242-1249
        if (!parts_read(h, 1)) return st;
        detail::to_space(exec, a.fpis, h);
      }
      if (s.oarst != 0) {  // :1250-1461
        if (!oarst_shapes_ok()) {
          st.exit_code = kExitShape;
          return st;
        }
        auto fa = [&](int i, int jj) {
          return flogoa[static_cast<std::size_t>((jj - 1) * kNogrp + i - 1)] != 0;
        };
        // READ X(1:NSEA) straight into a field of (0:NSEA), :1252-1259.
        auto r0 = [&](const auto& v) {
          auto h = detail::host_copy(exec, v);
          std::vector<unsigned char> raw;
          const int e = u.read_next(raw, static_cast<std::size_t>(s.nsea) * 4);
          if (e != 0) return e;
          Unpack p(raw);
          for (int isea = 1; isea <= s.nsea; ++isea) p.scalar(h(static_cast<std::size_t>(isea)));
          detail::to_space(exec, v, h);
          return 0;
        };
        // DO I=1,NSEALM; J = IAPROC + (I-1)*NAPROC; IF (J.LE.NSEA) X(I) = T(J), e.g. :1264-1267.
        auto scatter = [&](const auto& v, int col, const std::vector<float>& t) {
          auto h = detail::host_copy(exec, v);
          for (int i = 1; i <= s.nsealm; ++i) {
            const int jj = s.iaproc + (i - 1) * s.naproc;
            if (jj <= s.nsea) {
              if constexpr (std::remove_cvref_t<decltype(h)>::rank == 1) {
                h(static_cast<std::size_t>(i - 1)) = t[static_cast<std::size_t>(jj - 1)];
              } else {
                h(static_cast<std::size_t>(i - 1), static_cast<std::size_t>(col)) =
                    t[static_cast<std::size_t>(jj - 1)];
              }
            }
          }
          detail::to_space(exec, v, h);
        };
        // The two-field groups assign X(I) and Y(I) in one loop; two passes give the same
        // values, since each element is written once from its own TMP/TMP2 entry.
        auto one = [&](const auto& v) -> bool {
          const int e = seq_read(tmp);
          if (e != 0) {
            extiof_read(e);
            return false;
          }
          scatter(v, 0, tmp);
          return true;
        };
        auto two = [&](const auto& vx, int cx, const auto& vy, int cy) -> bool {
          int e = seq_read(tmp);
          if (e != 0) {
            extiof_read(e);
            return false;
          }
          e = seq_read(tmp2);
          if (e != 0) {
            extiof_read(e);
            return false;
          }
          scatter(vx, cx, tmp);
          scatter(vy, cy, tmp2);
          return true;
        };
        if (fa(1, 2)) {  // :1251-1256
          int e = r0(a.cx);
          if (e != 0) return extiof_read(e);
          e = r0(a.cy);
          if (e != 0) return extiof_read(e);
        }
        if (fa(1, 12)) {  // :1257-1260
          const int e = r0(a.icef);
          if (e != 0) return extiof_read(e);
        }
        if (fa(2, 1) && !one(a.hs)) return st;                                 // :1261-1268
        if (fa(2, 2) && !one(a.wlm)) return st;                                // :1269-1276
        if (fa(2, 4) && !one(a.t0m1)) return st;                               // :1277-1284
        if (fa(2, 5) && !one(a.t01)) return st;                                // :1285-1292
        if (fa(2, 6) && !one(a.fp0)) return st;                                // :1293-1300
        if (fa(2, 7) && !one(a.thm)) return st;                                // :1301-1308
        if (fa(2, 19) && !one(a.wnmean)) return st;                            // :1309-1316
        if (fa(5, 2) && !one(a.charn)) return st;                              // :1317-1324
        if (fa(5, 5) && !two(a.tauwix, 0, a.tauwiy, 0)) return st;             // :1325-1337
        if (fa(5, 11) && !one(a.tws)) return st;                               // :1338-1345
        if (fa(6, 2) && !two(a.tauox, 0, a.tauoy, 0)) return st;               // :1346-1358
        if (fa(6, 3) && !one(a.bhd)) return st;                                // :1359-1366
        if (fa(6, 4) && !one(a.phioc)) return st;                              // :1367-1374
        if (fa(6, 5) && !two(a.tusx, 0, a.tusy, 0)) return st;                 // :1375-1387
        if (fa(6, 6) && !two(a.ussx, 0, a.ussy, 0)) return st;                 // :1388-1400
        if (fa(6, 10) && !two(a.tauice, 0, a.tauice, 1)) return st;            // :1401-1413
        if (fa(6, 13) && !two(a.tauocx, 0, a.tauocy, 0)) return st;            // :1414-1426
        if (fa(7, 2) && !two(a.uba, 0, a.ubd, 0)) return st;                   // :1427-1439
        if (fa(7, 4) && !one(a.phibbl)) return st;                             // :1440-1447
        if (fa(7, 5) && !two(a.taubbl, 0, a.taubbl, 1)) return st;             // :1448-1460
      }
    } else {  // cold start, :1465-1516
      s.tlev = {-1, 0};
      s.tice = {-1, 0};
      s.trho = {-1, 0};
      s.tic1 = {-1, 0};
      s.tic5 = {-1, 0};
      Kokkos::deep_copy(exec, a.wlv, Real(0));
      Kokkos::deep_copy(exec, a.ice, Real(0));
      Kokkos::deep_copy(exec, a.asf, Real(1));
      Kokkos::deep_copy(exec, a.fpis, static_cast<Real>(dumfpi));
      if (s.oarst != 0) {  // :1486-1516, whole arrays as allocated
        for (const auto* v : {&a.cx, &a.cy, &a.icef, &a.hs, &a.wlm, &a.t0m1, &a.t01})
          Kokkos::deep_copy(exec, *v, Real(0));
        Kokkos::deep_copy(exec, a.fp0, Real(1));
        Kokkos::deep_copy(exec, a.thm, Real(0));
        Kokkos::deep_copy(exec, a.wnmean, Real(0));
        Kokkos::deep_copy(exec, a.charn, 0.0185f);  // 0x3c978d50, as the object stores it
        for (const auto* v : {&a.tauwix, &a.tauwiy, &a.tws, &a.tauox, &a.tauoy, &a.bhd, &a.phioc,
                              &a.tusx, &a.tusy, &a.ussx, &a.ussy, &a.tauocx, &a.tauocy})
          Kokkos::deep_copy(exec, *v, Real(0));
        Kokkos::deep_copy(exec, a.tauice, Real(0));
        for (const auto* v : {&a.uba, &a.ubd, &a.phibbl}) Kokkos::deep_copy(exec, *v, Real(0));
        Kokkos::deep_copy(exec, a.taubbl, Real(0));
      }
      exec.fence("ww::iors::cold_start");
    }
  }

  // ---- Close, :1525-1535 -----------------------------------------------------------------
  u.close();
  return st;
}

}  // namespace ww::iors
