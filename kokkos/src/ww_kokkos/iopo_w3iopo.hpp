// kokkos/src/ww_kokkos/iopo_w3iopo.hpp
// W3IOPO, ported: writes the point output file out_pnt.<ext> (or, with OFILES(2) = 1, one
// <YYYYMMDD>.<HHMMSS>.out_pnt.<ext> per output time), or reads it back one time per call: the
// header and the point table on the first pass, then per call the time and, per point, the
// depth, wind, current, ice and grid ID with the spectrum SPCO(:,I). Host only, no kernel and
// no floating-point arithmetic: every REAL is copied. Phase 1, translated, not wired into WW3,
// not regtested.
//
// WW3 heritage: SUBROUTINE W3IOPO, WW3-noaa/model/src/w3iopomd.F90:2131-2626 at NOAA-EMC/WW3
// 03adda91 (identical in the h0ffmann/WW3 fork at the WW3/ pin: the two files compare equal),
// author H. L. Tolman. Only what switches/switch_lab_shrd compiles is ported (read from
// `gfortran -E` with the defines of WW3's flags.make). Not compiled and not ported: W3_ASCII (the
// NDSOA argument, the out_pnt.<ext>.txt files and their list-directed copies, :2132-2134,
// :2242-2244, :2322-2326, :2341-2345, :2377-2381, :2425-2429, :2441-2445, :2477-2481,
// :2502-2504, :2543-2559), W3_FLX5 (TAUAO, TAUDO, DAIRO in the point record, :2220-2222,
// :2535-2537, :2563-2565), W3_SETUP (ZET_SETO in the point record, :2225-2227, :2538-2540,
// :2566-2568), W3_T test output, W3_S STRACE. W3_BIN2NC, which makes W3WAVE call W3IOPON instead (w3wavemd.F90:2691-2699), is not
// defined either.
//
// Quirks of the Fortran kept, each read in the -O3 GIMPLE (gfortran 16.2.0, WW3's Release flags):
// - IPASS => IPASS2 is incremented through the pointer W3SETO left at entry, before
//   W3SETO(IGRD) moves it (:2271 before :2282): with ww3_multi it counts the pass of the grid
//   selected at entry, and every later test reads IGRD's counter. The caller passes both.
// - WRITE is one SAVE LOGICAL for every grid (:2256), zero in .bss before the first pass. Only
//   the first pass with OFILES(2) = 0, or every pass with OFILES(2) = 1, sets it; on a later
//   pass a 'WRITE' request after a reading first pass is not refused (FORMAT 901 tests only
//   WRITE .AND. 'READ') and reads.
// - With OFILES(2) = 1 only a WRITE opens the file (:2422-2424); a READ rewinds and reads the
//   unit the caller connected, and the CLOSE at :2575 closes it. With OFILES(2) neither 0 nor 1,
//   no block opens anything and the passes transfer on the unit as it stands.
// - Writing zeroes IW(I), II(I), IL(I) in the state before the record (:2530-2532).
// - The DO I=1,NOPTS trip count is read once, after the TIME record (:2525).
//
// File format: gfortran sequential unformatted, CONVERT=file_endian, 'big_endian' from WW3's
// CMake (open_parm.convert = "big_endian" for the OPENs at :2319, :2328, :2422), the records of
// iogr_unformatted.hpp. The layout, checked against out_pnt.ww3 files WW3 wrote on this machine
// (kokkos/port_report/W3IOPO_KOKKOS.md), with NSPEC = NK*NTH:
//   1  IDSTR C*31, VEROPT C*10, NK, NTH, NOPTS                         53 bytes
//   2  PTLOC(1:2,1:NOPTS) REAL, PTNME(1:NOPTS) C*40                    48*NOPTS bytes
//   then per time: TIME(2)                                              8 bytes
//   and per point: IW, II, IL, DPO, WAO, WDO, ASO, CAO, CDO, ICEO,
//                  ICEHO, ICEFO, GRDID C*13, SPCO(1:NSPEC,I)            61 + 4*NSPEC bytes
//
// Units: ww::iobc::IobcUnits, the libgfortran OPEN model of the W3IOBC port (its open(2) calls
// for an OPEN without ACTION, its AFTER_ENDFILE flag), is reused; REWIND and CLOSE are here.
// Routines W3IOPO calls and this file does not port are hooks (IopoioHooks): W3SETO, W3SETG and
// W3SETW, W3DMO2 (iopo_shape_w3dmo2 of iopo_state.hpp reproduces its arrays). EXTOPN, EXTIOF and
// EXTCDE are reported in IopoioStatus, not called. A path that reaches an empty hook ends with
// kExitMissingHook.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>

#include "iobc_w3iobc.hpp"
#include "iogr_state.hpp"
#include "iogr_unformatted.hpp"
#include "iogr_w3iogr.hpp"
#include "iopo_state.hpp"

namespace ww::iopo {

/// The module data W3IOPO reads after W3SETO, W3SETG and W3SETW have selected grid IGRD
/// (w3iopomd.F90:2282-2284). The pointers are the caller's storage: W3IOPO writes OUT2 and, on
/// a READ, TIME.
struct IopoioGrid {
  IopoState* out2 = nullptr;                    // OUTPTS(IGRD)%OUT2 (NOPTS, IPASS2, PTLOC, ...)
  const std::array<std::int32_t, 8>* ofiles = nullptr;  // OUTPTS(IGRD)%OFILES(8), w3odatmd.F90:452
  std::array<std::int32_t, 2>* time = nullptr;  // TIME of WDATAS(IGRD), W3WDATMD
  std::int32_t ndse = 6, ndst = 6;              // OUTPTS(IGRD)%NDSE, %NDST
  std::int32_t nk = 0, nth = 0, nspec = 0;      // SGRDS(IGRD), W3GDATMD
  FChar<13> filext;                             // GRIDS(IGRD)%FILEXT, CHARACTER(LEN=13)
  // Module scalars of W3ODATMD, the same for every grid (w3odatmd.F90:337, :341).
  FChar<80> fnmpre{std::string("./")};          // FNMPRE = './'
  FChar<256> fnmpnt{std::string("./")};         // FNMPNT = './'
};

/// W3IOPO's SAVE variable (w3iopomd.F90:2256). One per process, shared by all grids.
struct IopoioSaved {
  FLogical write = 0;
};

/// The calls of W3IOPO, in its order. Each returns 0, or the EXTCDE code it ends with.
struct IopoioHooks {
  /// CALL W3SETO, W3SETG, W3SETW (IGRD, NDSE, NDST), :2282-2284, with the NDSE, NDST of the grid
  /// selected at entry: fills g with grid IGRD's data.
  std::function<int(int igrd, IopoioGrid& g)> select;
  /// W3DMO2(IGRD, NDSE, NDST, NOPTS), :2364 and :2464, READ only, when .NOT. O2INIT: allocates
  /// OUT2 with NOPTS points and sets NOPTS = NPT.
  std::function<int(IopoState& o, int igrd, int ndse, int ndst, int npt)> w3dmo2;
};

/// Hooks with W3DMO2 replaced by iopo_shape_w3dmo2 (SPCO with the grid's NSPEC) and W3SETO..W3SETW
/// by a fixed grid, for a caller with one grid. The fixed select skips W3SETO's tests of IMOD
/// (w3odatmd.F90:1631-1640, EXTCDE 1 and 2).
inline IopoioHooks iopoio_single_grid_hooks(const IopoioGrid& grid) {
  IopoioHooks h;
  h.select = [grid](int, IopoioGrid& g) {
    g = grid;
    return 0;
  };
  const int nspec = grid.nspec;
  h.w3dmo2 = [nspec](IopoState& o, int, int, int, int npt) {
    iopo_shape_w3dmo2(o, npt, nspec);
    return 0;
  };
  return h;
}

/// Codes outside the Fortran's EXTCDE codes (the W3IOGR and W3IOBC ones are reused).
inline constexpr int kExitGfortranRuntime = ww::iogr::kExitGfortranRuntime;  // 2
inline constexpr int kExitUnitNotConnected = ww::iobc::kExitUnitNotConnected;  // -105
inline constexpr int kExitShapeIo = ww::iogr::kExitShape;  // -102: OUT2 shorter than NOPTS

/// What W3IOPO ends with. exit_code 0: it returned, IOTST says how. Otherwise the code the run
/// ends with: EXTCDE(1), (2), (10), (11), (12) after one of W3IOPO's FORMATs 900-904, whose
/// bytes are ndse_text (written to NDSE unconditionally, :2287, :2296, :2402, :2352, :2356, :2360, :2452, :2456,
/// :2460);
/// EXTOPN(NDSE, IERR, 'W3IOPO', '', 20), exit 20 (w3servmd.F90:974-1062, FORMAT 1000); EXTIOF
/// (NDSE, IERR, 'W3IOPO', '', 21), exit 22 for IERR > 0 and 21 for IERR < 0 (w3servmd.F90:
/// 822-972, FORMATs 1002 and 1001), the messages being the caller's; a hook's code; or a kExit*.
struct IopoioStatus {
  int exit_code = 0;
  int ext = 0;            // 0, or 20 for EXTOPN, 21 for EXTIOF, the ERRCODE argument
  int iostat = 0;         // IERR handed to EXTOPN or EXTIOF
  int line = 0;           // the first source line of the failing statement at 03adda91
                          // (gfortran's run-time message names its last: 2542 for :2533)
  std::int32_t unit = 0;  // NDSE of ndse_text
  std::string ndse_text;  // bytes of FORMAT 900-904
  std::string filename;   // the FILE= of the last OPEN
  const char* missing = nullptr;  // the empty hook, with kExitMissingHook
};

namespace detail {

/// Iw.m (m >= 1) of a default INTEGER, gfortran: at least m digits, '-' for a negative value,
/// right-justified in w; w asterisks when it does not fit (io/write.c write_decimal).
inline std::string fmt_iwm(std::int32_t v, int w, int m) {
  const long long a = v < 0 ? -static_cast<long long>(v) : static_cast<long long>(v);
  std::string d = std::to_string(a);
  if (static_cast<int>(d.size()) < m) d.insert(0, static_cast<std::size_t>(m) - d.size(), '0');
  if (v < 0) d.insert(0, 1, '-');
  if (static_cast<int>(d.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - d.size(), ' ') + d;
}

/// The bytes of FORMATs 900-904 (w3iopomd.F90:2581-2593): each starts with '/' (an empty
/// record) and ends with '/' before the statement ends (another).
inline std::string fmt900(const std::string& inxout) {
  return "\n *** WAVEWATCH III ERROR IN W3IOPO :\n     ILEGAL INXOUT VALUE: " + inxout + "\n\n";
}
inline std::string fmt901(const std::string& inxout) {
  return "\n *** WAVEWATCH III ERROR IN W3IOPO :\n     MIXED READ/WRITE, LAST REQUEST: " +
         inxout + "\n\n";
}
inline std::string fmt902(const std::string& read, const std::string& check) {
  return "\n *** WAVEWATCH III ERROR IN W3IOPO :\n     ILEGAL IDSTR, READ : " + read +
         "\n                  CHECK : " + check + "\n\n";
}
inline std::string fmt903(const std::string& read, const std::string& check) {
  return "\n *** WAVEWATCH III ERROR IN W3IOPO :\n     ILEGAL VEROPT, READ : " + read +
         "\n                   CHECK : " + check + "\n\n";
}
inline std::string fmt904(std::int32_t mk, std::int32_t mth, std::int32_t nk, std::int32_t nth) {
  return "\n *** WAVEWATCH III ERROR IN W3IOPO :\n     ERROR IN SPECTRA, MK, MTH : " +
         fmt_iwm(mk, 8, 1) + fmt_iwm(mth, 8, 1) + "\n              ARRAY DIMENSIONS : " +
         fmt_iwm(nk, 8, 1) + fmt_iwm(nth, 8, 1) + "\n\n";
}

}  // namespace detail

/// REWIND(unit) without IOSTAT on a sequential unit, libgfortran's st_rewind (io/file_pos.c:
/// 427-490, gcc 16.2.0): nothing on an unconnected unit; otherwise flush, seek to 0 and leave
/// AFTER_ENDFILE. Returns false where libgfortran raises LIBERROR_OS (a run-time error, exit 2).
inline bool rewind_unit(ww::iobc::IobcUnits& units, int unit) {
  auto* u = units.get(unit);
  if (u == nullptr) return true;
  if (std::fflush(u->f.get()) != 0 || std::fseek(u->f.get(), 0, SEEK_SET) != 0) return false;
  u->after_endfile = false;
  return true;
}

/// W3IOPO(INXOUT, NDSOP, IOTST, IMOD), w3iopomd.F90:2131-2626, host driver. inxout as the caller
/// passes it (any length, compared blank padded); entry is OUTPTS(IOUTP)%OUT2 of the grid W3SETO
/// selected before the call (its IPASS2 is incremented, :2271); when that grid is IGRD, entry
/// must be the object the select hook points g.out2 at, or the increment is lost (a shim that
/// copies OUT2 in and out copies it once); units keeps the connected files between calls. IOTST: 0 data read or written, -1 past the end of the file (:2166-2167).
inline IopoioStatus w3iopo(const std::string& inxout, int ndsop, int& iotst, IopoState& entry,
                           IopoioSaved& saved, ww::iobc::IobcUnits& units,
                           const IopoioHooks& hooks, std::optional<int> imod = std::nullopt) {
  using ww::iogr::RecordReader;
  using ww::iogr::RecordWriter;
  using ww::iogr::detail::fstr_eq;
  IopoioStatus st;

  entry.ipass2 = static_cast<std::int32_t>(static_cast<std::uint32_t>(entry.ipass2) + 1u);  // :2271
  iotst = 0;                              // :2272
  const int igrd = imod ? *imod : 1;      // :2276-2280

  if (!hooks.select) {
    st.exit_code = ww::iogr::kExitMissingHook;
    st.missing = "select";
    return st;
  }
  IopoioGrid g;
  if (const int rc = hooks.select(igrd, g); rc != 0) {  // :2282-2284
    st.exit_code = rc;
    return st;
  }
  if (g.out2 == nullptr || g.ofiles == nullptr || g.time == nullptr) {
    st.exit_code = ww::iogr::kExitMissingHook;
    st.missing = "select";
    return st;
  }
  IopoState& o = *g.out2;
  const std::int32_t ofiles2 = (*g.ofiles)[1];  // OFILES(2); nothing in W3IOPO changes it
  st.unit = g.ndse;

  if (!fstr_eq(inxout, "READ") && !fstr_eq(inxout, "WRITE")) {  // :2286-2289
    st.ndse_text = detail::fmt900(inxout);
    st.exit_code = 1;
    return st;
  }
  auto check_mixed = [&]() {  // :2295-2298, :2401-2404
    if (saved.write != 0 && fstr_eq(inxout, "READ")) {
      st.ndse_text = detail::fmt901(inxout);
      st.exit_code = 2;
      return false;
    }
    return true;
  };
  if (o.ipass2 == 1 && ofiles2 == 0) {  // :2292-2299
    saved.write = fstr_eq(inxout, "WRITE") ? 1 : 0;
  } else if (!check_mixed()) {
    return st;
  }

  // FNMPRE_LOCAL, CHARACTER(LEN=256) (:2303-2307): FNMPRE is padded from 80 characters.
  FChar<256> fnmpre_local;
  if (g.fnmpnt.len_trim() == 0)
    fnmpre_local.assign(std::string(g.fnmpre.c.data(), g.fnmpre.c.size()));
  else
    fnmpre_local = g.fnmpnt;
  const std::string pre = fnmpre_local.trimmed();  // FNMPRE_LOCAL(:J)
  const std::string ext = g.filext.trimmed();      // FILEXT(:I)

  const auto nopts_fits = [&](std::int32_t n) {
    const auto nn = static_cast<std::size_t>(n > 0 ? n : 0);
    const auto ns = static_cast<std::size_t>(g.nspec > 0 ? g.nspec : 0);
    return o.ptloc.size() >= 2 * nn && o.ptnme.size() >= nn && o.iw.size() >= nn &&
           o.ii.size() >= nn && o.il.size() >= nn && o.dpo.size() >= nn && o.wao.size() >= nn &&
           o.wdo.size() >= nn && o.aso.size() >= nn && o.cao.size() >= nn &&
           o.cdo.size() >= nn && o.iceo.size() >= nn && o.iceho.size() >= nn &&
           o.icefo.size() >= nn && o.grdid.size() >= nn && o.spco.size() >= ns * nn;
  };

  // Transfers. A WRITE has no IOSTAT: a failure is a gfortran run-time error, exit status 2,
  // and so is a WRITE after a READ hit the end of the file (LIBERROR_OPTION_CONFLICT,
  // io/transfer.c:3357-3364).
  auto write_rec = [&](int line, const auto& fill) {
    auto* u = units.get(ndsop);
    st.line = line;
    if (u == nullptr) {  // ⚠ libgfortran would open fort.<NDSOP> implicitly
      st.exit_code = kExitUnitNotConnected;
      return false;
    }
    if (!u->can_write || u->after_endfile) {
      st.exit_code = kExitGfortranRuntime;
      return false;
    }
    RecordWriter w(u->f.get());
    fill(w);
    if (!w.end()) {
      st.exit_code = kExitGfortranRuntime;
      return false;
    }
    return true;
  };
  // READ (NDSOP, IOSTAT=IERR) list: returns IERR, or sets exit_code for an unconnected unit.
  auto read_rec = [&](int line, const auto& fill, int& ierr) {
    auto* u = units.get(ndsop);
    st.line = line;
    if (u == nullptr) {  // ⚠ libgfortran would open fort.<NDSOP> implicitly
      st.exit_code = kExitUnitNotConnected;
      return false;
    }
    if (!u->can_read) {
      ierr = ww::iogr::kIostatBadAction;
      return true;
    }
    if (u->after_endfile) {
      ierr = ww::iobc::kIostatOptionConflict;
      return true;
    }
    RecordReader r(u->f.get());
    r.begin();
    fill(r);
    r.finish();
    ierr = r.iostat();
    if (ierr == ww::iogr::kIostatEnd) u->after_endfile = true;
    return true;
  };
  auto extiof = [&](int ierr) {  // EXTIOF(NDSE, IERR, 'W3IOPO', '', 21)
    st.ext = 21;
    st.iostat = ierr;
    st.exit_code = ierr > 0 ? 22 : 21;
  };
  auto do_open = [&](int line, const std::string& path, bool status_old) {
    st.filename = path;
    st.line = line;
    const int ierr = units.open(ndsop, path, status_old);
    if (ierr != 0) {  // EXTOPN(NDSE, IERR, 'W3IOPO', '', 20)
      st.ext = 20;
      st.iostat = ierr;
      st.exit_code = 20;
      return false;
    }
    return true;
  };

  // The header and the point table, both blocks (:2338-2386, :2438-2486).
  auto header_and_points = [&](int l_write_hdr, int l_read_hdr, int l_write_pts,
                               int l_read_pts) {
    if (saved.write != 0) {
      if (!write_rec(l_write_hdr, [&](RecordWriter& w) {
            w.chars(kIdstr, 31);
            w.chars(kVeropt, 10);
            w.scalar(g.nk);
            w.scalar(g.nth);
            w.scalar(o.nopts);
          }))
        return false;
    } else {
      FChar<31> idtst;
      FChar<10> vertst;
      std::int32_t mk = 0, mth = 0;  // undefined in the Fortran until read
      int ierr = 0;
      if (!read_rec(l_read_hdr, [&](RecordReader& r) {
            r.chars(idtst.c.data(), 31);
            r.chars(vertst.c.data(), 10);
            r.scalar(mk);
            r.scalar(mth);
            r.scalar(o.nopts);
          }, ierr))
        return false;
      if (ierr != 0) {
        extiof(ierr);
        return false;
      }
      const std::string sid(idtst.c.data(), 31), sver(vertst.c.data(), 10);
      if (sid != kIdstr) {
        st.ndse_text = detail::fmt902(sid, kIdstr);
        st.exit_code = 10;
        return false;
      }
      if (sver != kVeropt) {
        st.ndse_text = detail::fmt903(sver, kVeropt);
        st.exit_code = 11;
        return false;
      }
      if (g.nk != mk || g.nth != mth) {
        st.ndse_text = detail::fmt904(mk, mth, g.nk, g.nth);
        st.exit_code = 12;
        return false;
      }
      if (o.o2init == 0) {
        if (!hooks.w3dmo2) {
          st.exit_code = ww::iogr::kExitMissingHook;
          st.missing = "w3dmo2";
          return false;
        }
        if (const int rc = hooks.w3dmo2(o, igrd, g.ndse, g.ndst, o.nopts); rc != 0) {
          st.exit_code = rc;
          return false;
        }
      }
    }
    if (!nopts_fits(o.nopts)) {  // the Fortran would transfer out of bounds
      st.exit_code = kExitShapeIo;
      return false;
    }
    const auto n = static_cast<std::size_t>(o.nopts > 0 ? o.nopts : 0);
    if (saved.write != 0) {
      return write_rec(l_write_pts, [&](RecordWriter& w) {
        w.array(o.ptloc.data(), 2 * n);
        for (std::size_t i = 0; i < n; ++i) w.chars(o.ptnme[i].c.data(), 40);
      });
    }
    int ierr = 0;
    if (!read_rec(l_read_pts, [&](RecordReader& r) {
          r.array(o.ptloc.data(), 2 * n);
          for (std::size_t i = 0; i < n; ++i) r.chars(o.ptnme[i].c.data(), 40);
        }, ierr))
      return false;
    if (ierr != 0) {
      extiof(ierr);
      return false;
    }
    return true;
  };

  // ---- one file for all times, first pass (:2310-2396) ----------------------------------
  if (o.ipass2 == 1 && ofiles2 == 0) {
    const std::string path = pre + "out_pnt." + ext;
    if (!do_open(saved.write != 0 ? 2319 : 2328, path, saved.write == 0)) return st;
    if (!rewind_unit(units, ndsop)) {  // :2333
      st.exit_code = kExitGfortranRuntime;
      return st;
    }
    if (!header_and_points(2339, 2347, 2375, 2383)) return st;
  }

  // ---- one file per time (:2398-2496) -----------------------------------------------------
  if (o.ipass2 >= 1 && ofiles2 == 1) {
    saved.write = fstr_eq(inxout, "WRITE") ? 1 : 0;  // :2399
  } else if (!check_mixed()) {
    return st;
  }
  if (o.ipass2 >= 1 && ofiles2 == 1) {
    // WRITE(TIMETAG,"(i8.8,'.'i6.6)") TIME(1), TIME(2), CHARACTER(LEN=15): 8+1+6 characters,
    // so the internal record never overflows (:2415).
    const std::string timetag =
        detail::fmt_iwm((*g.time)[0], 8, 8) + "." + detail::fmt_iwm((*g.time)[1], 6, 6);
    if (saved.write != 0 && !do_open(2422, pre + timetag + ".out_pnt." + ext, false)) return st;
    if (!rewind_unit(units, ndsop)) {  // :2432
      st.exit_code = kExitGfortranRuntime;
      return st;
    }
    if (!header_and_points(2439, 2447, 2475, 2483)) return st;
  }

  // ---- TIME (:2500-2516) ------------------------------------------------------------------
  if (saved.write != 0) {
    if (!write_rec(2501, [&](RecordWriter& w) { w.array(g.time->data(), 2); })) return st;
  } else {
    int ierr = 0;
    if (!read_rec(2506, [&](RecordReader& r) { r.array(g.time->data(), 2); }, ierr)) return st;
    if (ierr < 0) {
      iotst = -1;
      return st;
    }
    if (ierr > 0) {
      extiof(ierr);
      return st;
    }
  }

  // ---- loop over spectra (:2525-2573) -----------------------------------------------------
  const std::int32_t nopts = o.nopts;
  if (!nopts_fits(nopts)) {
    st.exit_code = kExitShapeIo;
    return st;
  }
  // SPCO(J,I) is addressed with the descriptor's column stride, the NSPEC W3DMO2 allocated with
  // (w3odatmd.F90:1108) after W3SETG(IMOD) (:1081), so grid IGRD's NSPEC; the C++ takes the
  // grid's NSPEC, the same value unless a hook allocated SPCO with another first extent.
  const auto nspec = static_cast<std::size_t>(g.nspec > 0 ? g.nspec : 0);
  for (std::int32_t i = 1; i <= nopts; ++i) {
    const auto k = static_cast<std::size_t>(i - 1);
    if (saved.write != 0) {
      o.iw[k] = 0;  // :2530-2532
      o.ii[k] = 0;
      o.il[k] = 0;
      if (!write_rec(2533, [&](RecordWriter& w) {
            w.scalar(o.iw[k]);
            w.scalar(o.ii[k]);
            w.scalar(o.il[k]);
            w.scalar(o.dpo[k]);
            w.scalar(o.wao[k]);
            w.scalar(o.wdo[k]);
            w.scalar(o.aso[k]);
            w.scalar(o.cao[k]);
            w.scalar(o.cdo[k]);
            w.scalar(o.iceo[k]);
            w.scalar(o.iceho[k]);
            w.scalar(o.icefo[k]);
            w.chars(o.grdid[k].c.data(), 13);
            w.array(o.spco.data() + nspec * k, nspec);
          }))
        return st;
    } else {
      int ierr = 0;
      if (!read_rec(2561, [&](RecordReader& r) {
            r.scalar(o.iw[k]);
            r.scalar(o.ii[k]);
            r.scalar(o.il[k]);
            r.scalar(o.dpo[k]);
            r.scalar(o.wao[k]);
            r.scalar(o.wdo[k]);
            r.scalar(o.aso[k]);
            r.scalar(o.cao[k]);
            r.scalar(o.cdo[k]);
            r.scalar(o.iceo[k]);
            r.scalar(o.iceho[k]);
            r.scalar(o.icefo[k]);
            r.chars(o.grdid[k].c.data(), 13);
            r.array(o.spco.data() + nspec * k, nspec);
          }, ierr))
        return st;
      if (ierr != 0) {
        extiof(ierr);
        return st;
      }
    }
  }
  if (ofiles2 == 1) units.close(ndsop);  // :2575
  return st;
}

}  // namespace ww::iopo
