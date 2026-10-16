// kokkos/src/ww_kokkos/iogr_unformatted.hpp
// The file format of mod_def.<ext> as W3IOGR's OPEN makes it: a gfortran sequential
// UNFORMATTED file with CONVERT='big_endian'. Host only. Phase 1, translated, not regtested.
//
// WW3 heritage: the OPEN statements of W3IOGR, WW3-noaa/model/src/w3iogrmd.F90:580-581 (WRITE)
// and :589-590 (READ, STATUS='OLD'), with convert=file_endian; FILE_ENDIAN = ENDIANNESS
// (constants.F90:87), which WW3's CMake defines as 'big_endian' (the -DENDIANNESS of
// flags.make; the -O3 GIMPLE sets open_parm.convert = "big_endian").
//
// Layout, as read in two mod_def.ww3 files that WW3 wrote under switch_lab_shrd (v, 2026-10-16;
// see kokkos/port_report/W3IOGR_KOKKOS.md): each record is a 4-byte length marker, the payload,
// and the same marker again; markers and every INTEGER, LOGICAL and REAL element are
// big-endian; CHARACTER bytes are copied unchanged; elements are packed with no padding.
// Records longer than 2147483639 bytes are split into subrecords whose leading marker is
// negative when another subrecord follows and whose trailing marker is negative when one
// preceded (v, libgfortran io/transfer.c next_record_w_unf and us_write, io/open.c:620-633, gcc
// 16.2.0 source, read 2026-10-16); ⚠ not checked against a file, no lab mod_def reaches that size.
#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ww::iogr {

/// IOSTAT values a READ returns, libgfortran's LIBERROR_* codes (v, gcc/fortran/libgfortran.h:
/// 121-147, gcc 16.2.0). EXTIOF only tests the sign (w3servmd.F90:910-923): > 0 exits with
/// ERRCODE+1, < 0 with ERRCODE. An OS error (LIBERROR_OS) sets IOSTAT to errno instead
/// (runtime/error.c generate_error_common).
inline constexpr int kIostatEnd = -1;            // LIBERROR_END: no marker where a record starts
inline constexpr int kIostatBadAction = 5007;    // LIBERROR_BAD_ACTION: READ on a write-only unit
inline constexpr int kIostatBadUs = 5009;        // LIBERROR_BAD_US: leading marker cut short
inline constexpr int kIostatShortRecord = 5016;  // LIBERROR_SHORT_RECORD: list past record end
inline constexpr int kIostatCorrupt = 5017;      // LIBERROR_CORRUPT_FILE: payload cut short

inline constexpr std::int64_t kMaxSubrecord = 2147483639;  // GFC_MAX_SUBRECORD_LENGTH (v)

namespace detail {
/// Copies n bytes of one element in file (big-endian) order.
inline void put_be(std::vector<unsigned char>& out, const void* p, std::size_t n) {
  const auto* b = static_cast<const unsigned char*>(p);
  if constexpr (std::endian::native == std::endian::little) {
    for (std::size_t i = 0; i < n; ++i) out.push_back(b[n - 1 - i]);
  } else {
    out.insert(out.end(), b, b + n);
  }
}
inline void get_be(const unsigned char* src, void* p, std::size_t n) {
  auto* b = static_cast<unsigned char*>(p);
  if constexpr (std::endian::native == std::endian::little) {
    for (std::size_t i = 0; i < n; ++i) b[i] = src[n - 1 - i];
  } else {
    std::memcpy(b, src, n);
  }
}
}  // namespace detail

/// One WRITE statement: the list items are appended, then end() emits the record. A WRITE in
/// W3IOGR has no IOSTAT, so a failure there is a gfortran run-time error; end() returns false.
class RecordWriter {
 public:
  explicit RecordWriter(std::FILE* f) : f_(f) {}
  template <class T>
  void scalar(const T& v) {  // INTEGER(1,2,4), LOGICAL(4), REAL(4,8): swapped per element
    detail::put_be(buf_, &v, sizeof(T));
  }
  template <class T>
  void array(const T* v, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) scalar(v[i]);
  }
  void chars(const char* c, std::size_t n) { buf_.insert(buf_.end(), c, c + n); }
  bool end() {
    const auto total = static_cast<std::int64_t>(buf_.size());
    std::int64_t done = 0;
    bool first = true;
    do {
      const std::int64_t len = std::min<std::int64_t>(total - done, kMaxSubrecord);
      const bool last = done + len == total;
      const auto m = static_cast<std::int32_t>(len);
      const std::int32_t lead = last ? m : -m;
      const std::int32_t tail = first ? m : -m;
      std::vector<unsigned char> mk;
      detail::put_be(mk, &lead, 4);
      if (std::fwrite(mk.data(), 1, 4, f_) != 4) return false;
      if (len > 0 && std::fwrite(buf_.data() + done, 1, static_cast<std::size_t>(len), f_) !=
                         static_cast<std::size_t>(len))
        return false;
      mk.clear();
      detail::put_be(mk, &tail, 4);
      if (std::fwrite(mk.data(), 1, 4, f_) != 4) return false;
      done += len;
      first = false;
    } while (done < total);
    buf_.clear();
    return true;
  }

 private:
  std::FILE* f_;
  std::vector<unsigned char> buf_;
};

/// One READ statement with IOSTAT, read as libgfortran reads it (io/transfer.c us_read,
/// read_block_direct, next_record_r_unf; v, gcc 16.2.0 source): begin() reads the leading
/// marker, each list item reads its bytes from the file, crossing into the next subrecord when
/// the marker said one follows, and finish() seeks over what the list left and over the trailing
/// marker, which is never read or compared. iostat() is what IOSTAT= receives; after an error no
/// further item is assigned and finish() does nothing, as libgfortran stops the transfer.
class RecordReader {
 public:
  explicit RecordReader(std::FILE* f) : f_(f) {}
  void begin() {
    iostat_ = 0;
    marker();
  }
  template <class T>
  void scalar(T& v) {
    unsigned char b[sizeof(T)];
    if (take(b, sizeof(T))) detail::get_be(b, &v, sizeof(T));
  }
  template <class T>
  void array(T* v, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) scalar(v[i]);
  }
  void chars(char* c, std::size_t n) { take(reinterpret_cast<unsigned char*>(c), n); }
  void finish() {
    if (iostat_ != 0) return;
    skip_tail();
    while (continued_) {
      if (!marker()) return;
      skip_tail();
    }
  }
  int iostat() const { return iostat_; }

 private:
  bool marker() {  // us_read: no byte is the end of file, 1-3 bytes a bad marker
    unsigned char mk[4];
    const std::size_t got = std::fread(mk, 1, 4, f_);
    if (got != 4) {
      iostat_ = got == 0 ? kIostatEnd : kIostatBadUs;
      return false;
    }
    std::int32_t m = 0;
    detail::get_be(mk, &m, 4);
    continued_ = m < 0;
    left_ = m < 0 ? -static_cast<std::int64_t>(m) : m;
    return true;
  }
  void skip_tail() {  // skip_record: an fseek, past the end of the file if need be
    std::fseek(f_, static_cast<long>(left_ + 4), SEEK_CUR);
    left_ = 0;
  }
  bool take(unsigned char* dst, std::size_t n) {
    if (iostat_ != 0) return false;
    std::size_t want = n, have = 0;
    for (;;) {
      const std::size_t sub =
          static_cast<std::int64_t>(want) < left_ ? want : static_cast<std::size_t>(left_);
      want -= sub;
      left_ -= static_cast<std::int64_t>(sub);
      if (sub > 0 && std::fread(dst + have, 1, sub, f_) != sub) {
        iostat_ = kIostatCorrupt;
        return false;
      }
      have += sub;
      if (want == 0) return true;
      if (!continued_) {
        iostat_ = kIostatShortRecord;
        return false;
      }
      skip_tail();
      if (!marker()) return false;
    }
  }

  std::FILE* f_;
  std::int64_t left_ = 0;   // bytes_left_subrecord
  bool continued_ = false;  // the current subrecord's leading marker was negative
  int iostat_ = 0;
};

}  // namespace ww::iogr
