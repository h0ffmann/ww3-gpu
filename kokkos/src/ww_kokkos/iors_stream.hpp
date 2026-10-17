// kokkos/src/ww_kokkos/iors_stream.hpp
// The file format of restart.<ext> as W3IORS's OPEN makes it: a gfortran UNFORMATTED file with
// ACCESS='STREAM' and CONVERT='big_endian', read and written at explicit byte positions (POS=).
// Host only. Phase 1, translated, not regtested.
//
// WW3 heritage: the OPEN statements of W3IORS, WW3-noaa/model/src/w3iorsmd.F90:521-523 (WRITE:
// no STATUS, no ACTION) and :525-526 (READ: STATUS='OLD', ACTION='READ'), convert=file_endian;
// FILE_ENDIAN = ENDIANNESS (constants.F90:87), 'big_endian' from WW3's CMake (the -DENDIANNESS
// of flags.make). Not a direct-access file: there are no record markers and no RECL. W3IORS
// lays out fixed records of LRECL bytes itself, by POS = 1 + LRECL*(NREC-1).
//
// What libgfortran 16.2 does with such a unit, as this file reproduces it (v, gcc 16.2.0
// libgfortran/io/unix.c regular_file2, read 2026-10-16, and a standalone gfortran 16.2 program
// run in the pinned ww3 shell, 2026-10-16; see kokkos/port_report/W3IORS_KOKKOS.md):
// - Elements are packed with no padding; INTEGER, LOGICAL and REAL elements are byte-swapped to
//   big-endian, CHARACTER bytes are copied. .TRUE. is written as 00000001.
// - OPEN without STATUS keeps an existing file: nothing is truncated, at the OPEN or after a
//   WRITE (a 100-byte file rewritten from POS=1 with 4 bytes keeps its 100 bytes). A WRITE past
//   the end leaves a hole that reads as zeros.
// - OPEN without ACTION opens read-write, or, refused with EACCES, EPERM or EROFS, read-only,
//   or, that refused with EACCES, EPERM or ENOENT, write-only (regular_file2). The mode is
//   0666 before the umask. A WRITE on a unit it could open read-only only gets IOSTAT 5007
//   (LIBERROR_BAD_ACTION), or the run-time error with exit status 2 without IOSTAT.
// - An OPEN that fails sets IOSTAT to errno (2 for a missing file or directory). A directory
//   opens with ACTION='READ'; its first READ gets IOSTAT 21 (EISDIR).
// - A READ that reaches the end of the file before its last byte gets IOSTAT -1
//   (LIBERROR_END), also when it started inside the file.
// - A transfer without POS= continues where the previous one on the unit stopped.
// - A WRITE with POS= on a unit that is not connected opens fort.<unit> (empty) and gets
//   IOSTAT 5002 (LIBERROR_BAD_OPTION): sequential access refuses POS=.
// ⚠ Not reproduced: the bytes a failed READ leaves in its list items (the Fortran exits at the
// EXTIOF that follows, so no caller sees them); when an OS write error (a full disk) surfaces,
// since libgfortran buffers and this file writes each statement with pwrite; the
// GFORTRAN_CONVERT_UNIT variable, which overrides CONVERT.
#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "iogr_unformatted.hpp"

namespace ww::iors {

/// IOSTAT values of libgfortran (v, gcc/fortran/libgfortran.h, gcc 16.2.0, and the program).
inline constexpr int kIostatEnd = -1;          // LIBERROR_END
inline constexpr int kIostatBadOption = 5002;  // LIBERROR_BAD_OPTION: POS= on a sequential unit
inline constexpr int kIostatBadAction = 5007;  // LIBERROR_BAD_ACTION: WRITE on a read-only unit

/// The bytes of one transfer list, in file order.
class Bytes {
 public:
  template <class T>
  void scalar(const T& v) {  // INTEGER, LOGICAL, REAL: swapped per element
    ww::iogr::detail::put_be(b_, &v, sizeof(T));
  }
  void chars(const char* c, std::size_t n) { b_.insert(b_.end(), c, c + n); }
  void zeros(std::size_t n) { b_.insert(b_.end(), n, static_cast<unsigned char>(0)); }
  const unsigned char* data() const { return b_.data(); }
  std::size_t size() const { return b_.size(); }

 private:
  std::vector<unsigned char> b_;
};

/// A list read from the file, decoded item by item in list order.
class Unpack {
 public:
  explicit Unpack(const std::vector<unsigned char>& b) : b_(b) {}
  template <class T>
  void scalar(T& v) {
    ww::iogr::detail::get_be(b_.data() + at_, &v, sizeof(T));
    at_ += sizeof(T);
  }
  void chars(char* c, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) c[i] = static_cast<char>(b_[at_ + i]);
    at_ += n;
  }

 private:
  const std::vector<unsigned char>& b_;
  std::size_t at_ = 0;
};

/// One Fortran unit connected for stream access. Positions are 1-based, as POS=. Every method
/// returns what IOSTAT= receives.
class StreamUnit {
 public:
  StreamUnit() = default;
  StreamUnit(const StreamUnit&) = delete;
  StreamUnit& operator=(const StreamUnit&) = delete;
  ~StreamUnit() { close(); }

  bool connected() const { return fd_ >= 0; }

  /// OPEN(FILE=path, FORM='UNFORMATTED', ACCESS='STREAM'), STATUS='UNKNOWN', ACTION unset:
  /// the retries of regular_file2 (unix.c), O_CREAT kept off the read-only retry.
  int open_unknown(const std::string& path) {
    constexpr mode_t kMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int fd = open_retry(path, O_RDWR | O_CREAT | O_CLOEXEC, kMode);
    if (fd >= 0) return connect(fd, true, true);
    if (errno != EACCES && errno != EPERM && errno != EROFS) return errno;
    fd = open_retry(path, O_RDONLY | O_CLOEXEC, kMode);
    if (fd >= 0) return connect(fd, true, false);
    if (errno != EACCES && errno != EPERM && errno != ENOENT) return errno;
    fd = open_retry(path, O_WRONLY | O_CREAT | O_CLOEXEC, kMode);
    if (fd >= 0) return connect(fd, false, true);
    return errno;
  }

  /// OPEN(..., STATUS='OLD', ACTION='READ').
  int open_old_read(const std::string& path) {
    const int fd = open_retry(path, O_RDONLY | O_CLOEXEC, 0);
    if (fd >= 0) return connect(fd, true, false);
    return errno;
  }

  /// WRITE(unit, POS=pos) list.
  int write_at(std::int64_t pos, const Bytes& b) {
    pos_ = pos;
    return write_next(b);
  }
  /// WRITE(unit) list, from where the last transfer stopped.
  int write_next(const Bytes& b) {
    if (!can_write_) return kIostatBadAction;
    std::size_t done = 0;
    while (done < b.size()) {
      const ssize_t w = ::pwrite(fd_, b.data() + done, b.size() - done,
                                 static_cast<off_t>(pos_ - 1 + static_cast<std::int64_t>(done)));
      if (w < 0) {
        if (errno == EINTR) continue;
        return errno;
      }
      done += static_cast<std::size_t>(w);
    }
    pos_ += static_cast<std::int64_t>(done);
    return 0;
  }

  /// READ(unit, POS=pos) of n bytes.
  int read_at(std::int64_t pos, std::vector<unsigned char>& out, std::size_t n) {
    pos_ = pos;
    return read_next(out, n);
  }
  /// READ(unit) of n bytes, from where the last transfer stopped.
  int read_next(std::vector<unsigned char>& out, std::size_t n) {
    if (!can_read_) return kIostatBadAction;
    out.assign(n, static_cast<unsigned char>(0));
    std::size_t done = 0;
    while (done < n) {
      const ssize_t r = ::pread(fd_, out.data() + done, n - done,
                                static_cast<off_t>(pos_ - 1 + static_cast<std::int64_t>(done)));
      if (r < 0) {
        if (errno == EINTR) continue;
        return errno;
      }
      if (r == 0) return kIostatEnd;
      done += static_cast<std::size_t>(r);
    }
    pos_ += static_cast<std::int64_t>(done);
    return 0;
  }

  /// CLOSE(unit); a unit not connected is no error (v, the program), a failed close is ignored.
  void close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    can_read_ = can_write_ = false;
  }

 private:
  static int open_retry(const std::string& path, int flags, mode_t mode) {
    int fd;
    do {
      fd = ::open(path.c_str(), flags, mode);
    } while (fd < 0 && errno == EINTR);
    return fd;
  }
  int connect(int fd, bool r, bool w) {
    fd_ = fd;
    can_read_ = r;
    can_write_ = w;
    pos_ = 1;
    return 0;
  }

  int fd_ = -1;
  bool can_read_ = false, can_write_ = false;
  std::int64_t pos_ = 1;
};

}  // namespace ww::iors
