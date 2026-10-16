// kokkos/src/ww_kokkos/flds_w3fldg.hpp
// W3FLDG, ported to Kokkos: read the next input field record (water level, current, wind,
// ice, momentum, air density, ice and mud parameters) of a WW3 field file for the interval
// T0 to TN, or write one record from a preprocessor, through the unit W3FLDO opened. Phase 1,
// translated, not wired into WW3, not regtested.
//
// WW3 heritage: SUBROUTINE W3FLDG, WW3/model/src/w3fldsmd.F90:935-1474 at the WW3/ pin
// 8e433f99 (byte-identical to NOAA-EMC/WW3 03adda91); author H. L. Tolman. Header Purpose
// (:964-965): "Update input fields in the WAVEWATCH III generic shell from a WAVEWATCH III
// shell data file or write from preprocessor." Ported as switches/switch_lab_shrd compiles it:
// no W3_S (STRACE), no W3_T (the WRITE (NDST,90xx) test prints, FORMATs 9000-9042), no
// W3_OASIS/OASACM/OASOCM/OASICM (the coupler branch :1255-1285 and :1369-1371, COUPL_COMM); none ported.
//
// File format. The unit is the one W3FLDO opens, FORM='UNFORMATTED' (:231), default ACCESS
// (sequential), CONVERT=file_endian (:361-373), 'big_endian' from WW3's CMake: gfortran
// sequential unformatted records, as iogr_unformatted.hpp writes and reads them. One call
// transfers a time record TF0 or TFN (two INTEGER) and then one record per component of
// NX*NY REAL, IX fastest (layout table: kokkos/port_report/W3FLDG_KOKKOS.md). In the -O3
// GIMPLE every one of them is one _gfortran_transfer_array(_write) of kind 4 with IOSTAT
// (flags 32); the per-element transfer here puts the same bytes in the file.
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) and the object of
// w3fldsmd.F90 (WW3's CMake Release flags, switch_lab_shrd; object identical to the CMake one):
//  - No floating-point arithmetic. DTTST = DSEC21(T0,TFN) (:1375) is compared with 0.0 by
//    ucomiss (.EQ., :1379) and comiss/ja (.GT., :1383): C++ == 0.0f and > 0.0f, NaN false in
//    both. TICK21(TFN, 1.) (:1301) passes the REAL 1.0 (0x3f800000 in .rodata).
//  - The field loops (:1182-1192, :1401-1411) and FAN(:,:) = FXN(:,:) (:1351, one memcpy of
//    MX REALs per column) are copies.
//  - The WRITE branch tests IERR, not ISTAT (:1210, :1218, :1226, :1233, :1241); IERR is 0
//    there, so a failed WRITE is never reported and codes 3 and 4 are never returned.
//    Reproduced, not corrected.
//  - A READ call after the one that returned IERR -1 returns 5 with IOSTAT 5001, as libgfortran
//    refuses a READ after the end of file (measured with gfortran 16.2): the port tests feof on
//    the stream, which a READ that hit the end of the file at a record start leaves set (and the
//    error paths 5, 6 and 7, after which every caller stops).
//
// Memory contract (phase 1). FX0..FAN live in ExecSpace's memory space. The copies are
// kernels on exec, in the Fortran's statement order; each READ or WRITE of a field stages the
// whole View through a host mirror (no copy when the space is host-accessible), so a READ that
// stops part way leaves the other elements as they were, and aliased arguments (ww3_shel
// passes one scratch array XXX for several of them, ww3_shel.F90:2142-2145) see each statement
// complete before the next. Times and flags are host-side. The function fences exec before
// every return.
#pragma once

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

#include <Kokkos_Core.hpp>

#include "iogr_unformatted.hpp"
#include "real.hpp"

namespace ww::flds {

/// IERR of W3FLDG (header :997-1006). An output of the routine, not a stop: the callers stop
/// on IERR > 0 (ww3_shel.F90:2507-2508) and mark the input finished on IERR < 0.
inline constexpr int kFldgPastLastData = -1;    ///< -1 Past last data, :1296
inline constexpr int kFldgOk = 0;               ///<  0 OK
inline constexpr int kFldgIllegalInxout = 1;    ///<  1 Illegal INXOUT, :1118-1122
inline constexpr int kFldgIllegalIdfld = 2;     ///<  2 Illegal IDFLD, :1123-1134
inline constexpr int kFldgErrWritingTime = 3;   ///<  3 never returned at the pin, :1210-1214
inline constexpr int kFldgErrWritingField = 4;  ///<  4 never returned at the pin
inline constexpr int kFldgErrReadingTime = 5;   ///<  5 Error in reading time, :1289-1293
inline constexpr int kFldgEofReadingField = 6;  ///<  6 Premature EOF reading field
inline constexpr int kFldgErrReadingField = 7;  ///<  7 Error reading field

/// FLAGSC_DEFAULT (:1101): the coupling flag when FLAGSC is absent.
inline constexpr bool kFldgFlagscDefault = false;

/// The calls W3FLDG makes to routines this file does not port, in WW3's argument order.
struct FldgHooks {
  void* ctx = nullptr;
  /// W3TIMEMD DSEC21(T1, T2), seconds from T1 to T2 as the REAL WW3 returns (:1375).
  float (*dsec21)(void* ctx, const int* t1, const int* t2) = nullptr;
  /// W3TIMEMD TICK21(T, DTIME), T advanced by DTIME seconds in place (:1301).
  void (*tick21)(void* ctx, int* t, float dtime) = nullptr;
  /// WRITE (NDSE,fmt): text is what gfortran writes for FORMAT fmt (1001-1007), its records
  /// each ended by '\n' (fldg_message). Called only when NDSE .GE. 0, as in WW3. May be null.
  void (*write_ndse)(void* ctx, int ndse, int fmt, const char* text, std::size_t len) = nullptr;
};

/// FX0, FY0, FA0, FXN, FYN, FAN (MX,MY) -> f(ix-1, iy-1), in ExecSpace's memory space.
template <class ExecSpace>
using FldgField = Kokkos::View<Real**, Kokkos::LayoutLeft, typename ExecSpace::memory_space>;

namespace fldg_detail {

/// Fortran .EQ. of two CHARACTER values: the shorter is padded with blanks
/// (_gfortran_compare_string, :1118, :1138).
inline bool fstr_eq(std::string_view a, std::string_view b) {
  const std::size_t n = a.size() > b.size() ? a.size() : b.size();
  for (std::size_t i = 0; i < n; ++i) {
    const char ca = i < a.size() ? a[i] : ' ';
    const char cb = i < b.size() ? b[i] : ' ';
    if (ca != cb) return false;
  }
  return true;
}

/// IDFLD is CHARACTER(LEN=3) (:1084): the first three characters of the actual argument.
struct Id3 {
  std::array<char, 3> c{' ', ' ', ' '};
  explicit Id3(std::string_view s) {
    for (std::size_t i = 0; i < 3 && i < s.size(); ++i) c[i] = s[i];
  }
  bool is(const char* lit) const {  // __builtin_memcmp_eq (idfld, "XXX", 3) in the GIMPLE
    return c[0] == lit[0] && c[1] == lit[1] && c[2] == lit[2];
  }
  std::string_view view() const { return std::string_view(c.data(), 3); }
};

/// Fortran Iw edit descriptor: right-justified in w columns, all '*' when it does not fit.
inline std::string fortran_iw(int v, int w) {
  const std::string s = std::to_string(v);
  if (static_cast<int>(s.size()) > w) return std::string(static_cast<std::size_t>(w), '*');
  return std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
}

/// IOSTAT of a READ on a unit whose previous READ hit the end of the file: LIBERROR_OPTION_CONFLICT
/// (gcc/fortran/libgfortran.h, gcc 16.2.0), set when compile_options.warn_std is nonzero, as it
/// is for a main program built with WW3's flags (measured with gfortran 16.2); by the source,
/// 5008 (LIBERROR_ENDFILE, io/transfer.c hit_eof) when it is zero.
inline constexpr int kIostatAfterEndfile = 5001;

/// The IOSTAT a failed WRITE would set: LIBERROR_OS reports errno (runtime/error.c).
inline int write_iostat() {
  const int e = errno;
  return e != 0 ? e : 1;
}

/// DO IX=1, NX / DO IY=1, NY ... the shift of :1182-1192 and the fill of :1401-1411. The IX
/// loop is the parallel one; each IX keeps the Fortran's two inner IY loops in order. The
/// bodies are copies, so the order across IX does not change a bit.
template <class ExecSpace>
void shift(const ExecSpace& exec, int nx, int ny, bool fl2d, bool flst,
           const FldgField<ExecSpace>& fx0, const FldgField<ExecSpace>& fy0,
           const FldgField<ExecSpace>& fa0, const FldgField<ExecSpace>& fxn,
           const FldgField<ExecSpace>& fyn, const FldgField<ExecSpace>& fan) {
  if (nx <= 0) return;
  Kokkos::parallel_for(
      "ww::flds::w3fldg::shift", Kokkos::RangePolicy<ExecSpace>(exec, 0, nx),
      KOKKOS_LAMBDA(const int ix) {
        for (int iy = 0; iy < ny; ++iy) {
          fx0(ix, iy) = fxn(ix, iy);
          if (fl2d) fy0(ix, iy) = fyn(ix, iy);
        }
        if (flst || !fl2d) {
          for (int iy = 0; iy < ny; ++iy) fa0(ix, iy) = fan(ix, iy);
        }
      });
}

/// FAN(:,:) = FXN(:,:) over the whole MX x MY arrays (:1351), column by column.
template <class ExecSpace>
void copy_all(const ExecSpace& exec, int mx, int my, const FldgField<ExecSpace>& dst,
              const FldgField<ExecSpace>& src) {
  if (mx <= 0 || my <= 0) return;
  Kokkos::parallel_for(
      "ww::flds::w3fldg::copy_all", Kokkos::RangePolicy<ExecSpace>(exec, 0, my),
      KOKKOS_LAMBDA(const int iy) {
        for (int ix = 0; ix < mx; ++ix) dst(ix, iy) = src(ix, iy);
      });
}

/// WRITE (NDS,IOSTAT=ISTAT) ((F(IX,IY),IX=1,NX),IY=1,NY); returns ISTAT.
template <class ExecSpace>
int write_field(const ExecSpace& exec, std::FILE* nds, const FldgField<ExecSpace>& f, int nx,
                int ny) {
  const auto h = Kokkos::create_mirror_view(f);
  Kokkos::deep_copy(exec, h, f);
  exec.fence("ww::flds::w3fldg::write_field");
  ww::iogr::RecordWriter w(nds);
  for (int iy = 0; iy < ny; ++iy)
    for (int ix = 0; ix < nx; ++ix) w.scalar(h(ix, iy));
  return w.end() ? 0 : write_iostat();
}

/// READ (NDS,IOSTAT=ISTAT) ((F(IX,IY),IX=1,NX),IY=1,NY); returns ISTAT. The mirror is filled
/// from f first, so the elements a failed READ does not reach keep their values.
template <class ExecSpace>
int read_field(const ExecSpace& exec, std::FILE* nds, const FldgField<ExecSpace>& f, int nx,
               int ny) {
  const auto h = Kokkos::create_mirror_view(f);
  Kokkos::deep_copy(exec, h, f);
  exec.fence("ww::flds::w3fldg::read_field in");
  ww::iogr::RecordReader r(nds);
  r.begin();
  for (int iy = 0; iy < ny; ++iy)
    for (int ix = 0; ix < nx; ++ix) r.scalar(h(ix, iy));
  r.finish();
  Kokkos::deep_copy(exec, f, h);
  exec.fence("ww::flds::w3fldg::read_field out");
  return r.iostat();
}

}  // namespace fldg_detail

/// The text gfortran writes to NDSE for FORMAT fmt of W3FLDG (:1431-1444), each record ended
/// by '\n': the leading '/' is an empty record and the trailing '/' another. a is the A item
/// (INXOUT for 1001, IDFLD for 1002), j the I1 item, istat the I6 item. Empty for other fmt.
inline std::string fldg_message(int fmt, std::string_view a, int j, int istat) {
  std::string s = "\n *** WAVEWATCH III ERROR IN W3FLDG : \n";
  const std::string i6 = fldg_detail::fortran_iw(istat, 6);
  const std::string i1 = fldg_detail::fortran_iw(j, 1);
  switch (fmt) {
    case 1001: s += "     ILLEGAL INXOUT STRING : " + std::string(a); break;
    case 1002: s += "     ILLEGAL FIELD ID STRING : " + std::string(a); break;
    case 1003: s += "     ERROR IN WRITING TIME, IOSTAT =" + i6; break;
    case 1004: s += "     ERROR IN WRITING FIELD " + i1 + ", IOSTAT =" + i6; break;
    case 1005: s += "     ERROR IN READING TIME, IOSTAT =" + i6; break;
    case 1006: s += "     PRMATURE EOF READING FIELD " + i1 + ", IOSTAT =" + i6; break;
    case 1007: s += "     ERROR IN READING FIELD " + i1 + ", IOSTAT =" + i6; break;
    default: return std::string();
  }
  s += "\n\n";
  return s;
}

/// @brief W3FLDG(INXOUT, IDFLD, NDS, NDST, NDSE, MX, MY, NX, NY, T0, TN, TF0, FX0, FY0, FA0,
///        TFN, FXN, FYN, FAN, IERR, FLAGSC), w3fldsmd.F90:935-1474.
/// @param exec    execution space instance the copies run on; fenced before every return.
/// @param inxout  'READ' or 'WRITE' (compared blank-padded, as CHARACTER*(*)).
/// @param idfld   field id, its first three characters (CHARACTER(LEN=3)).
/// @param nds     the stream of unit NDS, positioned where W3FLDO or the previous call left it;
///                opened for reading ("rb") or writing ("wb") to match inxout.
/// @param ndst    test output unit; only W3_T writes to it, not compiled.
/// @param ndse    error unit, passed to write_ndse; no output if < 0.
/// @param mx,my   extents of the six Views (the copy of :1351 covers all of them).
/// @param nx,ny   the part of them read or written.
/// @param t0,tn   time interval considered (unused on WRITE).
/// @param tf0,tfn field times (in/out; tfn unused on WRITE).
/// @param fx0..fan fields at TF0 and TFN (in/out; the N fields unused on WRITE).
/// @param flagsc  FLAGSC, absent by default (kFldgFlagscDefault); never written.
/// @return IERR, one of the kFldg* codes.
/// @note A missing dsec21 hook (every completed READ), or tick21 hook (EOF of an interpolated
///       field), aborts: the Fortran always reaches those calls there.
/// @author ww3-gpu; heritage H. L. Tolman (W3FLDSMD)
/// @date 2026-10-16
template <class ExecSpace>
int w3fldg(const ExecSpace& exec, const FldgHooks& hooks, std::string_view inxout,
           std::string_view idfld, std::FILE* nds, int ndst, int ndse, int mx, int my, int nx,
           int ny, const std::array<int, 2>& t0, const std::array<int, 2>& tn,
           std::array<int, 2>& tf0, const FldgField<ExecSpace>& fx0,
           const FldgField<ExecSpace>& fy0, const FldgField<ExecSpace>& fa0,
           std::array<int, 2>& tfn, const FldgField<ExecSpace>& fxn,
           const FldgField<ExecSpace>& fyn, const FldgField<ExecSpace>& fan,
           std::optional<bool> flagsc = std::nullopt) {
  using fldg_detail::fstr_eq;
  (void)ndst;
  const fldg_detail::Id3 id(idfld);
  auto emit = [&](int fmt, std::string_view a, int j, int istat) {
    if (ndse >= 0 && hooks.write_ndse) {
      const std::string s = fldg_message(fmt, a, j, istat);
      hooks.write_ndse(hooks.ctx, ndse, fmt, s.data(), s.size());
    }
  };
  auto done = [&](int code) {
    exec.fence("ww::flds::w3fldg");
    return code;
  };

  int ierr = kFldgOk;  // :1109
  int istat = 0;

  // test input parameters, :1118-1134
  if (!fstr_eq(inxout, "READ") && !fstr_eq(inxout, "WRITE")) {
    emit(1001, inxout, 0, 0);
    return done(kFldgIllegalInxout);
  }
  if (!id.is("IC1") && !id.is("IC2") && !id.is("IC3") && !id.is("IC4") && !id.is("IC5") &&
      !id.is("MDN") && !id.is("MTH") && !id.is("MVS") && !id.is("LEV") && !id.is("CUR") &&
      !id.is("WND") && !id.is("WNS") && !id.is("ICE") && !id.is("ISI") && !id.is("TAU") &&
      !id.is("RHO")) {
    emit(1002, id.view(), 0, 0);
    return done(kFldgIllegalIdfld);
  }

  // Set internal variables, :1138-1160
  const bool write = fstr_eq(inxout, "WRITE");
  const bool fl2d = id.is("CUR") || id.is("WND") || id.is("WNS") || id.is("ISI") || id.is("TAU");
  const bool flbe = id.is("ISI");
  const bool flst = id.is("WNS");
  const bool flcoupl = flagsc ? *flagsc : kFldgFlagscDefault;
  bool flinterp = id.is("CUR") || id.is("WND") || id.is("WNS") || id.is("TAU") || id.is("RHO");
  if (flcoupl) flinterp = false;
  const bool flfrst = tfn[0] == -1;

  // Loop over times / fields, :1168-1387
  for (;;) {
    if (!write && flinterp) {  // :1172-1199
      tf0[0] = tfn[0];
      tf0[1] = tfn[1];
      if (tfn[0] != -1) fldg_detail::shift(exec, nx, ny, fl2d, flst, fx0, fy0, fa0, fxn, fyn, fan);
    }

    if (write) {  // :1204-1249
      {
        ww::iogr::RecordWriter w(nds);  // :1209
        w.array(tf0.data(), 2);
        istat = w.end() ? 0 : fldg_detail::write_iostat();
      }
      if (ierr != 0) {
        emit(1003, {}, 0, istat);
        return done(kFldgErrWritingTime);
      }
      int j = 0;
      if (!fl2d) {
        j = 1;
        istat = fldg_detail::write_field(exec, nds, fa0, nx, ny);  // :1217
        if (ierr != 0) {
          emit(1004, {}, j, istat);
          return done(kFldgErrWritingField);
        }
      } else {
        j = 1;
        istat = fldg_detail::write_field(exec, nds, fx0, nx, ny);  // :1225
        if (ierr != 0) {
          emit(1004, {}, j, istat);
          return done(kFldgErrWritingField);
        }
        j = 2;
        istat = fldg_detail::write_field(exec, nds, fy0, nx, ny);  // :1232
        if (ierr != 0) {
          emit(1004, {}, j, istat);
          return done(kFldgErrWritingField);
        }
        j = 3;
        if (flst) {
          istat = fldg_detail::write_field(exec, nds, fa0, nx, ny);  // :1240
          if (ierr != 0) {
            emit(1004, {}, j, istat);
            return done(kFldgErrWritingField);
          }
        }
      }
      break;  // :1249
    }

    // Process fields, read, :1253-1385
    if (std::feof(nds)) {
      // The previous READ hit the end of the file: libgfortran then refuses any sequential
      // READ of the unit (AFTER_ENDFILE, io/transfer.c:3357-3363), so the call after IERR -1
      // returns 5 (ww3_shel.F90:2507-2508 then stops).
      istat = fldg_detail::kIostatAfterEndfile;
    } else {
      ww::iogr::RecordReader r(nds);  // :1287
      r.begin();
      r.array(tfn.data(), 2);
      r.finish();
      istat = r.iostat();
    }
    if (istat > 0) {  // :1289
      emit(1005, {}, 0, istat);
      return done(kFldgErrReadingTime);
    } else if (istat < 0) {  // :1294, end of file
      ierr = kFldgPastLastData;
      if (flinterp) {
        tfn[0] = tn[0];
        tfn[1] = tn[1];
        if (!hooks.tick21) Kokkos::abort("ww::flds::w3fldg: tick21 hook missing (:1301)");
        hooks.tick21(hooks.ctx, tfn.data(), 1.0f);
      }
      break;  // :1306
    }

    // A field component j of :1311-1368: ISTAT < 0 is a premature EOF, > 0 a read error.
    auto field_status = [&](int j) {
      if (istat < 0) {
        emit(1006, {}, j, istat);
        return kFldgEofReadingField;
      } else if (istat > 0) {
        emit(1007, {}, j, istat);
        return kFldgErrReadingField;
      }
      return kFldgOk;
    };
    int j = 0;
    if (!fl2d) {
      j = 1;
      istat = fldg_detail::read_field(exec, nds, fan, nx, ny);  // :1315
      if (const int e = field_status(j); e != kFldgOk) return done(e);
    } else {
      j = 1;
      istat = fldg_detail::read_field(exec, nds, fxn, nx, ny);  // :1327
      if (const int e = field_status(j); e != kFldgOk) return done(e);
      j = 2;
      istat = fldg_detail::read_field(exec, nds, fyn, nx, ny);  // :1338
      if (const int e = field_status(j); e != kFldgOk) return done(e);

      if (flbe) fldg_detail::copy_all(exec, mx, my, fan, fxn);  // :1351

      j = 3;
      if (flst) {
        istat = fldg_detail::read_field(exec, nds, fan, nx, ny);  // :1357
        if (const int e = field_status(j); e != kFldgOk) return done(e);
      }
    }

    // Check time, branch back if necessary, :1375-1383
    if (!hooks.dsec21) Kokkos::abort("ww::flds::w3fldg: dsec21 hook missing (:1375)");
    const float dttst = hooks.dsec21(hooks.ctx, t0.data(), tfn.data());
    if (!flinterp && flfrst && dttst == 0.0f) break;
    if (dttst > 0.0f) break;
  }

  // Branch point for EOF and interpolated fields, :1393-1413
  if (!write && flinterp && tf0[0] == -1) {
    tf0[0] = t0[0];
    tf0[1] = t0[1];
    fldg_detail::shift(exec, nx, ny, fl2d, flst, fx0, fy0, fa0, fxn, fyn, fan);
  }

  return done(ierr);  // :1427
}

}  // namespace ww::flds
