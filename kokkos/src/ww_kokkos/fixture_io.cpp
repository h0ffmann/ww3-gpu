// kokkos/src/ww_kokkos/fixture_io.cpp
// Implementation of the W3SNL1 fixture reader. See fixture_io.hpp.
//
// SPDX-License-Identifier: MIT

#include "ww_kokkos/fixture_io.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <stdexcept>

namespace ww::fixture {
namespace {

/// 'S','N','L','1' read as one big-endian-spelled int32, i.e. the value the
/// Fortran generator writes as INT(z'534E4C31').
constexpr std::int32_t kMagic = 0x534E4C31;

/// A file cursor that refuses to read past the end, so a truncated fixture is a
/// clear error instead of a field full of whatever was on the stack.
class Reader {
 public:
  Reader(std::ifstream& in, const std::string& path) : in_(in), path_(path) {}

  template <typename T>
  T scalar() {
    T v{};
    raw(&v, sizeof(T), 1);
    return v;
  }

  template <typename T>
  void array(T* dst, std::size_t n) {
    raw(dst, sizeof(T), n);
  }

  /// Read `n` 32-bit Fortran indices and store them 0-based.
  void index_array(std::vector<int>& dst, std::size_t n) {
    dst.resize(n);
    raw(dst.data(), sizeof(std::int32_t), n);
    for (int& v : dst) v -= 1;
  }

  void real_array(std::vector<Real>& dst, std::size_t n) {
    dst.resize(n);
    raw(dst.data(), sizeof(float), n);
  }

 private:
  void raw(void* dst, std::size_t size, std::size_t n) {
    in_.read(static_cast<char*>(dst), static_cast<std::streamsize>(size * n));
    if (!in_) throw std::runtime_error("truncated SNL1 fixture: " + path_);
  }

  std::ifstream& in_;
  std::string path_;
};

}  // namespace

Snl1Fixture load(const std::string& path) {
  // Real must be the 32-bit type the generator wrote, or every offset shifts.
  static_assert(sizeof(Real) == 4, "the SNL1 fixture stores 32-bit reals");
  static_assert(sizeof(int) == sizeof(std::int32_t), "the SNL1 fixture stores 32-bit ints");

  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open SNL1 fixture: " + path);
  Reader r(in, path);

  Snl1Fixture f;
  if (r.scalar<std::int32_t>() != kMagic)
    throw std::runtime_error("not an SNL1 fixture (bad magic): " + path);
  f.nk = r.scalar<std::int32_t>();
  f.nth = r.scalar<std::int32_t>();
  f.npts = r.scalar<std::int32_t>();
  f.nspec = f.nk * f.nth;

  f.xfr = r.scalar<float>();
  f.dth = r.scalar<float>();
  f.lam = r.scalar<float>();
  f.snlc1 = r.scalar<float>();
  f.kdcon = r.scalar<float>();
  f.kdmn = r.scalar<float>();
  f.snls1 = r.scalar<float>();
  f.snls2 = r.scalar<float>();
  f.snls3 = r.scalar<float>();
  f.fachfe = r.scalar<float>();
  r.real_array(f.sig, static_cast<std::size_t>(f.nk));

  f.nfr = r.scalar<std::int32_t>();
  f.nfrhgh = r.scalar<std::int32_t>();
  f.nfrchg = r.scalar<std::int32_t>();
  f.nspecx = r.scalar<std::int32_t>();
  f.nspecy = r.scalar<std::int32_t>();

  f.dal1 = r.scalar<float>();
  f.dal2 = r.scalar<float>();
  f.dal3 = r.scalar<float>();
  r.array(f.awg, 8);
  r.array(f.swg, 8);

  const auto nx = static_cast<std::size_t>(f.nspecx);
  const auto ns = static_cast<std::size_t>(f.nspec);

  // The 16 NSPECX tables, in the order INSNL1 assigns them:
  // IP11..IP14, IM11..IM14, IP21..IP24, IM21..IM24.
  for (int j = 0; j < 4; ++j) r.index_array(f.ip[0][j], nx);
  for (int j = 0; j < 4; ++j) r.index_array(f.im[0][j], nx);
  for (int j = 0; j < 4; ++j) r.index_array(f.ip[1][j], nx);
  for (int j = 0; j < 4; ++j) r.index_array(f.im[1][j], nx);

  // The 16 NSPEC tables: IC11,IC21..IC81 first, then IC12,IC22..IC82.
  for (int j = 0; j < 8; ++j) r.index_array(f.ic[j][0], ns);
  for (int j = 0; j < 8; ++j) r.index_array(f.ic[j][1], ns);

  r.real_array(f.af11, nx);

  f.points.resize(static_cast<std::size_t>(f.npts));
  for (Snl1Point& p : f.points) {
    p.kdmean = r.scalar<float>();
    r.real_array(p.cg, static_cast<std::size_t>(f.nk));
    r.real_array(p.a, ns);
    r.real_array(p.s, ns);
    r.real_array(p.d, ns);
  }

  return f;
}

}  // namespace ww::fixture
