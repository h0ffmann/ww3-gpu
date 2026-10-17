// kokkos/src/ww_kokkos/flds_w3fldo.hpp
// W3FLDO, ported: open a WW3 input field file (level.ww3, current.ww3, wind.ww3, ice.ww3,
// momentum.ww3, density.ww3, the ice and mud parameter files, data0-2.ww3) on unit NDS and
// write its header record, or read it and check the file id, the field id and the grid
// dimensions. Host only. Phase 1, translated, not wired into WW3, not regtested.
//
// WW3 heritage: SUBROUTINE W3FLDO, WW3/model/src/w3fldsmd.F90:89-514 at the WW3/ pin 8e433f99
// (byte-identical to NOAA-EMC/WW3 03adda91); authors H. L. Tolman, A. Chawla. Header Purpose
// (:116-117): "Open and prepare WAVEWATCH III field files as used by the generic shell and the
// field preprocessor." Ported as switches/switch_lab_shrd compiles it: no W3_S (STRACE,
// :200-202, :226-228, :243-245), no W3_T (the WRITE (NDST,900x) test prints :247-250, :353-355,
// :436-438, FORMATs 9000-9002), no W3_TIDE (TIDEOK = .TRUE., :254-256; without it TIDEOK keeps
// its initial .FALSE. and the -O3 GIMPLE folds it away). None of them is ported.
//
// File format. OPEN (NDS, FILE=..., FORM=FORM, CONVERT=file_endian, IOSTAT=IERR) with FORM =
// 'UNFORMATTED' (:231), sequential, 'big_endian' from WW3's CMake (:361-373): the records of
// iogr_unformatted.hpp. The header is one record of 40 bytes, :391-392 / :405-406:
//   IDSTR or TSSTR  c13  'WAVEWATCH III'
//   IDFLD or TSFLD  c3   field id
//   NX, NY, GTYPE   i4 x 3 (on READ NXT, NYT, GTYPET)
//   FILLER(1:2)     i4 x 2, 0 on WRITE
//   TIDEFLAG        i4
// FORM is a SAVE local assigned only by its initialiser (:231), so FORM .EQ. 'UNFORMATTED'
// (:386, :404) is always true and the formatted branches (FORMAT 900, :393-396, :407-410) are
// compiled but never taken: not ported.
//
// The unit. NDS is a Fortran unit number; FldoUnits keeps what libgfortran keeps for it (the
// stream, which transfers its OPEN allowed, the file's device and inode). W3FLDG then takes
// FldoUnits::stream(nds), positioned after the header. The OPEN is libgfortran's st_open as
// gcc 16.2.0 writes it (io/open.c st_open, already_open, new_unit; io/unix.c regular_file2,
// compare_file_filename; runtime/string.c fc_strdup):
//  - the file name loses its trailing blanks, then stops at a NUL (fstrlen, then strndup);
//  - a negative unit number not made by NEWUNIT= gives IOSTAT 5002 (LIBERROR_BAD_OPTION);
//  - a unit connected to the same file (same st_dev and st_ino) keeps its stream and position;
//    connected to another file, it is closed and opened again;
//  - no ACTION: read-write first, then, refused with EACCES, EPERM or EROFS, read-only (O_CREAT
//    dropped for STATUS='UNKNOWN'), then, refused with EACCES, EPERM or ENOENT, write-only;
//    the mode is 0666 before the umask; a failure sets IOSTAT to errno (LIBERROR_OS);
//  - a READ on a write-only unit or a WRITE on a read-only one gets IOSTAT 5007
//    (LIBERROR_BAD_ACTION, io/transfer.c data_transfer_init).
// The "file already open on another unit" check of new_unit is skipped for programs built with
// gfortran's default -std=gnu (it is gated by !(allow_std & GFC_STD_F2018)); WW3's flags give no
// -std, so two units may hold the same file, as here.
//
// OPEN without STATUS keeps an existing file; libgfortran truncates it where the first WRITE
// ends (io/transfer.c st_write_done, NO_ENDFILE; io/unit.c unit_truncate). The port does that
// after the header record. ⚠ With FDHDR .FALSE. the first WRITE is W3FLDG's, whose writer does
// not truncate, so the port truncates a newly connected unit at the OPEN instead: a file opened
// that way and never written ends empty here and unchanged in the Fortran (every caller at the
// pin writes a record with W3FLDG next). ⚠ Units 0, 5 and 6 are preconnected in libgfortran;
// the port does not close the process's stderr, stdin or stdout when one is reconnected.
// GFORTRAN_CONVERT_UNIT is ignored.
//
// Checked against gfortran 16.2 -O3 GIMPLE (-fdump-tree-optimized-lineno) and the object of
// w3fldsmd.F90 (WW3's CMake Release flags, switch_lab_shrd; .text identical to the CMake one):
// no floating-point arithmetic; the order of every test, assignment and transfer is the
// Fortran's (port report, kokkos/port_report/W3FLDO_KOKKOS.md).
#pragma once

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "flds_w3fldg.hpp"
#include "iogr_state.hpp"
#include "iogr_unformatted.hpp"
#include "iogr_w3iogr.hpp"

namespace ww::flds {

/// IERR of W3FLDO (header :143-154). An output of the routine, not a stop: ww3_shel stops on
/// IERR .NE. 0 (ww3_shel.F90:1735-1736), ww3_multi exits with 2080 (wminitmd.F90:2564-2567).
inline constexpr int kFldoOk = 0;               ///<  0 No errors
inline constexpr int kFldoIllegalInxout = 1;    ///<  1 Illegal INXOUT, :264-268
inline constexpr int kFldoIllegalIdfld = 2;     ///<  2 Illegal ID, :269-282
inline constexpr int kFldoErrOpening = 3;       ///<  3 Error in opening file, :376-380
inline constexpr int kFldoErrWriting = 4;       ///<  4 Write error in file, :397-401
inline constexpr int kFldoErrReading = 5;       ///<  5 Read error in file, :415-419
inline constexpr int kFldoPrematureEof = 6;     ///<  6 Premature EOF in read, :411-414
inline constexpr int kFldoBadFileId = 7;        ///<  7 Unexpected file identifier, :428-432
inline constexpr int kFldoBadFieldId = 8;       ///<  8 Unexpected field identifier, :440-444
inline constexpr int kFldoBadGrid = 9;          ///<  9 Unexpected grid dimensions, :446-451
inline constexpr int kFldoBadDataInfo = 10;     ///< 10 Unexpected data info (tide), :421-426

/// Not a code of the Fortran: LEN_TRIM(FEXT) > 20 makes TEMPXT(:I) (:299-348) read past the
/// 20 bytes of TEMPXT (:233), which the -O3 object does (a concat_string of I bytes from the
/// stack); what it reads is undefined, so the port stops there. ww3_prtide passes a FILENAMEXT
/// of 256 characters read from its input (ww3_prtide.F90:185, :335, :340).
inline constexpr int kFldoExitFextTooLong = -120;

/// IOSTAT of an OPEN with a negative unit not made by NEWUNIT= (LIBERROR_BAD_OPTION).
inline constexpr int kIostatBadOption = 5002;

/// The call W3FLDO makes to a routine this file does not port.
struct FldoHooks {
  void* ctx = nullptr;
  /// WRITE (NDSE,fmt): text is what gfortran writes for FORMAT fmt (1001-1010), its records
  /// each ended by '\n' (fldo_message). Called only when NDSE .GE. 0, as in WW3. May be null.
  void (*write_ndse)(void* ctx, int ndse, int fmt, const char* text, std::size_t len) = nullptr;
};

/// The SAVE local of W3FLDO that outlives a call: FDHDR, initialised .TRUE. (:235), set only
/// when FHDR is present (:292-294). One per process. FORM and IDSTR are SAVE too but never
/// assigned; TIDEFLAG (:236) is assigned at every entry (:258-262) before it is read, and
/// TIDEOK (:237) is .FALSE. without W3_TIDE.
struct FldoSaved {
  bool fdhdr = true;
};

/// The Fortran units W3FLDO connects, kept by the caller between calls and shared with W3FLDG.
class FldoUnits {
 public:
  struct Unit {
    std::unique_ptr<std::FILE, ww::iogr::detail::FileCloser> f;
    bool can_read = false;
    bool can_write = false;
    dev_t dev = 0;
    ino_t ino = 0;
  };
  /// The unit's record, or nullptr when it is not connected.
  Unit* get(int unit) {
    const auto it = u_.find(unit);
    return it == u_.end() ? nullptr : &it->second;
  }
  /// The stream W3FLDG reads or writes, or nullptr when the unit is not connected.
  std::FILE* stream(int unit) {
    Unit* u = get(unit);
    return u ? u->f.get() : nullptr;
  }
  void close(int unit) { u_.erase(unit); }

  /// OPEN(unit, FILE=file, FORM='UNFORMATTED', CONVERT=..., IOSTAT=)[, STATUS='OLD'], no
  /// ACTION, no POSITION. Returns IOSTAT. truncate: the port's stand-in for the truncation of
  /// W3FLDG's first WRITE when no header is written (file comment); a unit kept connected to
  /// the same file is not truncated.
  int open(int unit, std::string_view file, bool status_old, bool truncate) {
    if (unit < 0) return kIostatBadOption;  // st_open, io/open.c
    std::size_t n = file.size();  // fc_strdup: fstrlen, then strndup of those bytes
    while (n > 0 && file[n - 1] == ' ') --n;
    std::string_view trimmed = file.substr(0, n);
    const std::string path(trimmed.substr(0, trimmed.find('\0')));

    if (Unit* cur = get(unit)) {  // already_open
      struct stat st {};
      if (::stat(path.c_str(), &st) == 0 && st.st_dev == cur->dev && st.st_ino == cur->ino)
        return 0;  // edit_modes: same file, the connection and its position are kept
      u_.erase(unit);
    }

    constexpr mode_t kMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    const int crflag = (status_old ? 0 : O_CREAT) | O_CLOEXEC;
    Unit nu;
    int fd = open_retry(path, O_RDWR | crflag, kMode);  // regular_file2
    if (fd >= 0) {
      nu.can_read = nu.can_write = true;
    } else {
      if (errno != EACCES && errno != EPERM && errno != EROFS) return errno;
      const int crflag2 = status_old ? crflag : (crflag & ~O_CREAT);
      fd = open_retry(path, O_RDONLY | crflag2, kMode);
      if (fd >= 0) {
        nu.can_read = true;
      } else {
        if (errno != EACCES && errno != EPERM && errno != ENOENT) return errno;
        fd = open_retry(path, O_WRONLY | crflag, kMode);
        if (fd < 0) return errno;
        nu.can_write = true;
      }
    }
    struct stat st {};
    if (::fstat(fd, &st) == 0) {
      nu.dev = st.st_dev;
      nu.ino = st.st_ino;
    }
    if (truncate && nu.can_write && ::ftruncate(fd, 0) != 0) {
      const int e = errno;
      ::close(fd);
      return e;
    }
    const char* mode = nu.can_read && nu.can_write ? "r+b" : (nu.can_read ? "rb" : "wb");
    nu.f.reset(::fdopen(fd, mode));
    if (!nu.f) {
      const int e = errno;
      ::close(fd);
      return e != 0 ? e : 1;
    }
    u_.emplace(unit, std::move(nu));
    return 0;
  }

 private:
  static int open_retry(const std::string& path, int flags, mode_t mode) {
    int fd;
    do {
      fd = ::open(path.c_str(), flags, mode);  // TEMP_FAILURE_RETRY
    } while (fd < 0 && errno == EINTR);
    return fd;
  }
  std::map<int, Unit> u_;
};

/// The text gfortran writes to NDSE for FORMAT fmt of W3FLDO (:472-496), each record ended by
/// '\n'. a and b are the A items (1001: INXOUT at its full length; 1002-1006: IDFLD; 1007:
/// TSSTR, IDSTR; 1008: TSFLD, IDFLD), i the integer items in list order (1003-1005: IOSTAT;
/// 1009: NXT, NYT, GTYPET, NX, NY, GTYPE; 1010: FILLER(1:2), TIDEFLAG). Empty for other fmt.
inline std::string fldo_message(int fmt, std::string_view a, std::string_view b,
                                const std::array<int, 6>& i) {
  using fldg_detail::fortran_iw;
  std::string s = "\n *** WAVEWATCH III ERROR IN W3FLDO : \n";
  const std::string as(a), bs(b);
  switch (fmt) {
    case 1001: s += "     ILLEGAL INXOUT STRING : " + as + "\n"; break;
    case 1002: s += "     ILLEGAL FIELD ID STRING : " + as + "\n"; break;
    case 1003:
      s += "     ERROR IN OPENING " + as + " FILE, IOSTAT =" + fortran_iw(i[0], 6) + "\n";
      break;
    case 1004:
      s += "     ERROR IN WRITING TO " + as + " FILE, IOSTAT =" + fortran_iw(i[0], 6) + "\n";
      break;
    case 1005:
      s += "     ERROR IN READING " + as + " FILE, IOSTAT =" + fortran_iw(i[0], 6) + "\n";
      break;
    case 1006: s += "     PREMATURE END OF " + as + " FILE\n"; break;
    case 1007:
      s += "     ILLEGAL FILE ID STRING >" + as + "<\n";
      s += "                  SHOULD BE >" + bs + "<\n";
      break;
    case 1008:
      s += "     ILLEGAL FIELD ID STRING >" + as + "<\n";
      s += "                   SHOULD BE >" + bs + "<\n";
      break;
    case 1009:
      s += "     INCOMPATIBLE GRID DATA : ";
      for (std::size_t k = 0; k < 3; ++k) s += " " + fortran_iw(i[k], 10);
      s += "\n                  SHOULD BE : ";
      for (std::size_t k = 3; k < 6; ++k) s += " " + fortran_iw(i[k], 10);
      s += "\n";
      break;
    case 1010:
      s += "     FILLER indicates use of tidal constituents";
      for (std::size_t k = 0; k < 3; ++k) s += fortran_iw(i[k], 4);
      s += "\n     For this the code should be compiled with TIDE switch\n";
      break;
    default: return std::string();
  }
  s += "\n";
  return s;
}

/// @brief W3FLDO(INXOUT, IDFLD, NDS, NDST, NDSE, NX, NY, GTYPE, IERR, FEXT, FPRE, FHDR,
///        TIDEFLAGIN), w3fldsmd.F90:89-514. Host only.
/// @param hooks      write_ndse for the error messages.
/// @param saved      FDHDR, the SAVE local (one per process).
/// @param units      the connected units; NDS is opened in it.
/// @param inxout     'READ' or 'WRITE' (compared blank-padded, as CHARACTER*(*)).
/// @param idfld      IDFLD, CHARACTER(LEN=3), in/out: 'WND' may become 'WNS', 'ICE' 'ISI'.
/// @param nds        the unit of the field file.
/// @param ndst       test output unit; only W3_T writes to it, not compiled.
/// @param ndse       error unit, passed to write_ndse; no output if < 0.
/// @param nx         in/out: NX, set to NXT on a READ that passes the check (:453).
/// @param ny         NY.
/// @param gtype      in/out: GTYPE, set to GTYPET on that READ when GTYPE .LE. 4 (:454).
/// @param fext       FEXT, OPTIONAL; 'ww3' when absent (:284-290).
/// @param fpre       FPRE, OPTIONAL, prepended at its full length (:361, :369).
/// @param fhdr       FHDR, OPTIONAL; sets saved.fdhdr when present (:292-294).
/// @param tideflagin TIDEFLAGIN, OPTIONAL (nullptr when absent), in/out (:258-262, :462-464).
/// @return IERR, one of the kFldo* codes, or kFldoExitFextTooLong where the Fortran's
///         behaviour is undefined.
/// @author ww3-gpu; heritage H. L. Tolman, A. Chawla (W3FLDSMD)
/// @date 2026-10-16
inline int w3fldo(const FldoHooks& hooks, FldoSaved& saved, FldoUnits& units,
                  std::string_view inxout, std::array<char, 3>& idfld, int nds, int ndst,
                  int ndse, int& nx, int ny, int& gtype,
                  std::optional<std::string_view> fext = std::nullopt,
                  std::optional<std::string_view> fpre = std::nullopt,
                  std::optional<bool> fhdr = std::nullopt, int* tideflagin = nullptr) {
  using fldg_detail::fstr_eq;
  (void)ndst;
  const std::array<int, 6> none{};
  auto emit = [&](int fmt, std::string_view a, std::string_view b,
                  const std::array<int, 6>& i) {
    if (ndse >= 0 && hooks.write_ndse) {
      const std::string s = fldo_message(fmt, a, b, i);
      hooks.write_ndse(hooks.ctx, ndse, fmt, s.data(), s.size());
    }
  };
  auto is = [&](const char* lit) {  // __builtin_memcmp_eq (idfld, "XXX", 3)
    return idfld[0] == lit[0] && idfld[1] == lit[1] && idfld[2] == lit[2];
  };
  const std::string_view id(idfld.data(), 3);

  // test input parameters, :257-282
  std::array<std::int32_t, 3> filler{0, 0, 0};                 // :257
  std::int32_t tideflag = tideflagin ? *tideflagin : 0;        // :258-262
  constexpr bool tideok = false;                               // :237, no W3_TIDE

  if (!fstr_eq(inxout, "READ") && !fstr_eq(inxout, "WRITE")) {  // :264-268
    emit(1001, inxout, {}, none);
    return kFldoIllegalInxout;
  }
  if (!is("IC1") && !is("IC2") && !is("IC3") && !is("IC4") && !is("IC5") && !is("MDN") &&
      !is("MTH") && !is("MVS") && !is("LEV") && !is("CUR") && !is("WND") && !is("WNS") &&
      !is("ICE") && !is("TAU") && !is("RHO") && !is("DT0") && !is("DT1") && !is("DT2") &&
      !is("ISI")) {  // :269-282
    emit(1002, id, {}, none);
    return kFldoIllegalIdfld;
  }

  // :284-290. TEMPXT = FEXT pads or truncates to 20; I = LEN_TRIM(FEXT).
  ww::iogr::FChar<20> tempxt{std::string(fext ? *fext : std::string_view("ww3"))};
  std::size_t i = 3;
  if (fext) {
    i = fext->size();
    while (i > 0 && (*fext)[i - 1] == ' ') --i;
  }
  if (fhdr) saved.fdhdr = *fhdr;  // :292-294

  // Set internal variables, :298-349. Every valid id has a branch, so FNAME is always set.
  const char* stem = "";
  if (is("LEV")) stem = "level.";
  else if (is("CUR")) stem = "current.";
  else if (is("WND") || is("WNS")) stem = "wind.";
  else if (is("ICE") || is("ISI")) stem = "ice.";
  else if (is("TAU")) stem = "momentum.";
  else if (is("RHO")) stem = "density.";
  else if (is("DT0")) stem = "data0.";
  else if (is("DT1")) stem = "data1.";
  else if (is("DT2")) stem = "data2.";
  else if (is("MDN")) stem = "muddens.";
  else if (is("MTH")) stem = "mudthk.";
  else if (is("MVS")) stem = "mudvisc.";
  else if (is("IC1")) stem = "ice1.";
  else if (is("IC2")) stem = "ice2.";
  else if (is("IC3")) stem = "ice3.";
  else if (is("IC4")) stem = "ice4.";
  else if (is("IC5")) stem = "ice5.";
  if (i > 20) return kFldoExitFextTooLong;
  // FNAME(:I) = stem // TEMPXT(:I); at most 9 + 20 characters, within FNAME's 30 (:234).
  const std::string fname = std::string(stem) + std::string(tempxt.c.data(), i);

  const bool write = fstr_eq(inxout, "WRITE");  // :351

  // Open file, :359-380
  const std::string file = fpre ? std::string(*fpre) + fname : fname;
  int ierr = units.open(nds, file, !write, write && !saved.fdhdr);
  if (ierr != 0) {
    emit(1003, id, {}, {ierr, 0, 0, 0, 0, 0});
    return kFldoErrOpening;
  }
  FldoUnits::Unit& u = *units.get(nds);

  if (write) {  // :384-402
    if (saved.fdhdr) {
      if (!u.can_write) {
        ierr = ww::iogr::kIostatBadAction;
      } else if (std::feof(u.f.get())) {
        // A unit kept connected to the same file whose READ hit the end of the file: the WRITE
        // is refused too (io/transfer.c data_transfer_init, AFTER_ENDFILE).
        ierr = fldg_detail::kIostatAfterEndfile;
      } else {
        std::fseek(u.f.get(), 0, SEEK_CUR);  // C needs it between a READ and a WRITE
        ww::iogr::RecordWriter w(u.f.get());  // :391-392
        w.chars("WAVEWATCH III", 13);
        w.chars(idfld.data(), 3);
        w.scalar(static_cast<std::int32_t>(nx));
        w.scalar(static_cast<std::int32_t>(ny));
        w.scalar(static_cast<std::int32_t>(gtype));
        w.array(filler.data(), 2);
        w.scalar(tideflag);
        ierr = w.end() ? 0 : fldg_detail::write_iostat();
        if (ierr == 0) {  // st_write_done: unit_truncate at the position after the record
          const off_t pos = ::ftello(u.f.get());
          if (pos < 0 || std::fflush(u.f.get()) != 0 || ::ftruncate(::fileno(u.f.get()), pos) != 0)
            ierr = fldg_detail::write_iostat();
        }
      }
      if (ierr != 0) {
        emit(1004, id, {}, {ierr, 0, 0, 0, 0, 0});
        return kFldoErrWriting;
      }
    }
  } else {  // :404-457
    std::array<char, 13> tsstr{};
    std::array<char, 3> tsfld{};
    std::int32_t nxt = 0, nyt = 0, gtypet = 0;
    if (!u.can_read) {
      ierr = ww::iogr::kIostatBadAction;
    } else if (std::feof(u.f.get())) {
      // A unit kept from an earlier OPEN of the same file whose READ hit the end of the file:
      // libgfortran refuses the READ (AFTER_ENDFILE), as in W3FLDG.
      ierr = fldg_detail::kIostatAfterEndfile;
    } else {
      std::fseek(u.f.get(), 0, SEEK_CUR);  // C needs it between a WRITE and a READ
      ww::iogr::RecordReader r(u.f.get());  // :405-406
      r.begin();
      r.chars(tsstr.data(), 13);
      r.chars(tsfld.data(), 3);
      r.scalar(nxt);
      r.scalar(nyt);
      r.scalar(gtypet);
      r.array(filler.data(), 2);
      r.scalar(tideflag);
      r.finish();
      ierr = r.iostat();
    }
    if (ierr < 0) {  // :411-414
      emit(1006, id, {}, none);
      return kFldoPrematureEof;
    } else if (ierr > 0) {  // :415-419
      emit(1005, id, {}, {ierr, 0, 0, 0, 0, 0});
      return kFldoErrReading;
    }
    if ((filler[0] != 0 || filler[1] != 0) && tideflag >= 0) tideflag = 0;  // :420
    if (tideflag != 0 && !tideok) {                                        // :421-426
      emit(1010, {}, {}, {filler[0], filler[1], tideflag, 0, 0, 0});
      return kFldoBadDataInfo;
    }

    const std::string_view ts(tsstr.data(), 13);
    if (ts != std::string_view("WAVEWATCH III", 13)) {  // :428-432
      emit(1007, ts, "WAVEWATCH III", none);
      return kFldoBadFileId;
    }
    const bool ts_wns = tsfld[0] == 'W' && tsfld[1] == 'N' && tsfld[2] == 'S';
    const bool ts_isi = tsfld[0] == 'I' && tsfld[1] == 'S' && tsfld[2] == 'I';
    if ((is("WND") && ts_wns) || (is("ICE") && ts_isi)) idfld = tsfld;  // :433-439
    if (idfld != tsfld) {                                               // :440-444
      emit(1008, std::string_view(tsfld.data(), 3), id, none);
      return kFldoBadFieldId;
    }
    if (!(idfld[0] == 'D' && idfld[1] == 'T')) {  // :445-456
      if (nx != nxt || ny != nyt) {
        emit(1009, {}, {}, {nxt, nyt, gtypet, nx, ny, gtype});
        return kFldoBadGrid;
      }
      nx = nxt;
      if (gtype <= 4) gtype = gtypet;
    }
  }

  // File OK, :461-464
  if (tideflagin) *tideflagin = tideflag;
  return kFldoOk;
}

}  // namespace ww::flds
