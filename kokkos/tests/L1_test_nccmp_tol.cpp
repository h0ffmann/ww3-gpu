// kokkos/tests/L1_test_nccmp_tol.cpp
// The per-field NetCDF comparator, driven in-process. Each case writes real
// NetCDF files into a private temporary directory with netcdf-c, runs
// compare_files() on them and asserts the verdict both ways: identical data
// passes, a 1 % perturbation of one cell fails, fill/NaN cells never count, and
// a variable the tolerance file does not name is reported without being judged.
//
// No Kokkos here: the comparator is plain C++ on top of netcdf-c.
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <netcdf.h>
#include <unistd.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "nccmp-tol/compare.hpp"
#include "nccmp-tol/tolerances.hpp"

namespace {

using ww::nccmp::Stats;
using ww::nccmp::Tolerance;

constexpr std::size_t kTime = 2, kY = 3, kX = 4;
constexpr std::size_t kCells = kTime * kY * kX;
constexpr float kFill = -999.0F;

/// The default row for `hs`, as tolerances.txt states it.
std::vector<Tolerance> hs_only() { return {Tolerance{"hs", 1e-4, 1e-4}}; }

/// 24 distinct, non-zero values so a relative error is well defined everywhere.
std::vector<float> base_values() {
  std::vector<float> v(kCells);
  for (std::size_t i = 0; i < kCells; ++i) v[i] = 0.5F + 0.1F * static_cast<float>(i);
  return v;
}

/// What the writer puts into a file besides hs(time, y, x).
struct FileSpec {
  bool with_fill_attr = false;         ///< hs carries _FillValue = kFill
  const std::vector<float>* extra = nullptr;  ///< also write foo(time, y, x)
  bool only_here = false;              ///< also write a scalar `lone` variable
};

void nc_ok(int status, const char* what) {
  ASSERT_EQ(status, NC_NOERR) << what << ": " << nc_strerror(status);
}

/// Write hs (and optionally foo, lone) into a NetCDF-4 file at `path`.
void write_file(const std::filesystem::path& path, const std::vector<float>& hs,
                const FileSpec& spec = {}) {
  int ncid = -1;
  nc_ok(nc_create(path.c_str(), NC_CLOBBER | NC_NETCDF4, &ncid), "nc_create");
  int dims[3] = {-1, -1, -1};
  nc_ok(nc_def_dim(ncid, "time", kTime, &dims[0]), "def time");
  nc_ok(nc_def_dim(ncid, "y", kY, &dims[1]), "def y");
  nc_ok(nc_def_dim(ncid, "x", kX, &dims[2]), "def x");
  int hs_id = -1;
  nc_ok(nc_def_var(ncid, "hs", NC_FLOAT, 3, dims, &hs_id), "def hs");
  if (spec.with_fill_attr) {
    nc_ok(nc_put_att_float(ncid, hs_id, "_FillValue", NC_FLOAT, 1, &kFill), "put _FillValue");
  }
  int foo_id = -1;
  if (spec.extra != nullptr) {
    nc_ok(nc_def_var(ncid, "foo", NC_DOUBLE, 3, dims, &foo_id), "def foo");
  }
  int lone_id = -1;
  if (spec.only_here) {
    nc_ok(nc_def_var(ncid, "lone", NC_INT, 0, nullptr, &lone_id), "def lone");
  }
  nc_ok(nc_enddef(ncid), "enddef");
  nc_ok(nc_put_var_float(ncid, hs_id, hs.data()), "put hs");
  if (spec.extra != nullptr) {
    std::vector<double> d(spec.extra->begin(), spec.extra->end());
    nc_ok(nc_put_var_double(ncid, foo_id, d.data()), "put foo");
  }
  if (spec.only_here) {
    const int one = 1;
    nc_ok(nc_put_var_int(ncid, lone_id, &one), "put lone");
  }
  nc_ok(nc_close(ncid), "close");
}

/// A fresh directory per test case, removed afterwards.
class NccmpTol : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = std::filesystem::temp_directory_path() /
           ("nccmp_tol_" + std::string(info->name()) + "_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir_);
    std::filesystem::create_directories(dir_);
    ref_ = dir_ / "ref.nc";
    test_ = dir_ / "test.nc";
  }
  void TearDown() override { std::filesystem::remove_all(dir_); }

  std::filesystem::path dir_, ref_, test_;
};

const Stats* find(const std::vector<Stats>& stats, const std::string& name) {
  for (const auto& s : stats) {
    if (s.name == name) return &s;
  }
  return nullptr;
}

}  // namespace

TEST(NccmpTolParser, ParsesTolerances) {
  std::istringstream in(
      "# variable  abs      rel\n"
      "\n"
      "hs          1e-4     1e-4\n"
      "dir         1e-2     1e-4   # trailing comment\n"
      "   \n"
      "t0m1        1e-4     2e-3\n");
  const auto tol = ww::nccmp::parse_tolerances(in);
  ASSERT_EQ(tol.size(), 3U);
  EXPECT_EQ(tol[0].name, "hs");
  EXPECT_DOUBLE_EQ(tol[0].abs, 1e-4);
  EXPECT_DOUBLE_EQ(tol[0].rel, 1e-4);
  EXPECT_EQ(tol[1].name, "dir");
  EXPECT_DOUBLE_EQ(tol[1].abs, 1e-2);
  EXPECT_DOUBLE_EQ(tol[1].rel, 1e-4);
  EXPECT_EQ(tol[2].name, "t0m1");
  EXPECT_DOUBLE_EQ(tol[2].abs, 1e-4);
  EXPECT_DOUBLE_EQ(tol[2].rel, 2e-3);

  std::istringstream bad("hs 1e-4\n");
  EXPECT_THROW(ww::nccmp::parse_tolerances(bad), std::runtime_error);
}

TEST_F(NccmpTol, IdenticalFilesPass) {
  const auto v = base_values();
  write_file(ref_, v);
  write_file(test_, v);

  const auto stats = ww::nccmp::compare_files(ref_.string(), test_.string(), hs_only());
  ASSERT_EQ(stats.size(), 1U);
  const Stats& hs = stats[0];
  EXPECT_EQ(hs.name, "hs");
  EXPECT_EQ(hs.n, kCells);
  EXPECT_EQ(hs.max_abs, 0.0);
  EXPECT_EQ(hs.rms, 0.0);
  EXPECT_EQ(hs.max_rel, 0.0);
  EXPECT_TRUE(hs.judged);
  EXPECT_TRUE(hs.pass);
  EXPECT_TRUE(ww::nccmp::all_pass(stats));

  const std::string table = ww::nccmp::format_table(stats);
  EXPECT_NE(table.find("hs"), std::string::npos);
  EXPECT_NE(table.find("PASS"), std::string::npos);
}

TEST_F(NccmpTol, PerturbationBeyondRelFails) {
  const auto v = base_values();
  auto w = v;
  w[7] *= 1.01F;  // 1 % on one cell: beyond rel = 1e-4 and, at ~1.2, beyond abs = 1e-4
  write_file(ref_, v);
  write_file(test_, w);

  const auto stats = ww::nccmp::compare_files(ref_.string(), test_.string(), hs_only());
  ASSERT_EQ(stats.size(), 1U);
  const Stats& hs = stats[0];
  EXPECT_EQ(hs.n, kCells);
  EXPECT_NEAR(hs.max_abs, 0.01 * static_cast<double>(v[7]), 1e-6);
  EXPECT_NEAR(hs.max_rel, 0.01, 1e-6);
  EXPECT_GT(hs.rms, 0.0);
  EXPECT_LT(hs.rms, hs.max_abs);
  EXPECT_TRUE(hs.judged);
  EXPECT_FALSE(hs.pass);
  EXPECT_FALSE(ww::nccmp::all_pass(stats));
  EXPECT_NE(ww::nccmp::format_table(stats).find("FAIL"), std::string::npos);
}

TEST_F(NccmpTol, FillValuesIgnored) {
  const auto v = base_values();
  auto r = v;
  auto w = v;
  r[0] = kFill;                                     // fill in the reference only
  w[0] = 5.0F;                                      // ...against a real value in test
  w[1] = kFill;                                     // fill in test only
  w[2] = std::numeric_limits<float>::quiet_NaN();   // NaN, no fill attribute involved
  FileSpec spec;
  spec.with_fill_attr = true;
  write_file(ref_, r, spec);
  write_file(test_, w, spec);

  const auto stats = ww::nccmp::compare_files(ref_.string(), test_.string(), hs_only());
  ASSERT_EQ(stats.size(), 1U);
  const Stats& hs = stats[0];
  EXPECT_EQ(hs.n, kCells - 3);
  EXPECT_EQ(hs.dropped, 2U);  // cells 1 and 2: ref valid, test fill / NaN
  EXPECT_EQ(hs.max_abs, 0.0);
  EXPECT_FALSE(std::isnan(hs.rms));
  EXPECT_TRUE(hs.pass);
  EXPECT_TRUE(ww::nccmp::all_pass(stats));
}

TEST_F(NccmpTol, AllNaNTestFieldFails) {
  const auto v = base_values();
  std::vector<float> w(kCells, std::numeric_limits<float>::quiet_NaN());
  write_file(ref_, v);
  write_file(test_, w);

  // Nothing to compare is not agreement: a judged field with no cells fails.
  const auto stats = ww::nccmp::compare_files(ref_.string(), test_.string(), hs_only());
  ASSERT_EQ(stats.size(), 1U);
  const Stats& hs = stats[0];
  EXPECT_EQ(hs.n, 0U);
  EXPECT_EQ(hs.dropped, kCells);
  EXPECT_TRUE(hs.judged);
  EXPECT_FALSE(hs.pass);
  EXPECT_FALSE(ww::nccmp::all_pass(stats));
  EXPECT_NE(ww::nccmp::format_table(stats).find("FAIL"), std::string::npos);
}

TEST_F(NccmpTol, PartialNaNIsReported) {
  const auto v = base_values();
  auto w = v;
  w[3] = std::numeric_limits<float>::quiet_NaN();
  w[9] = std::numeric_limits<float>::quiet_NaN();
  write_file(ref_, v);
  write_file(test_, w);

  const auto stats = ww::nccmp::compare_files(ref_.string(), test_.string(), hs_only());
  ASSERT_EQ(stats.size(), 1U);
  const Stats& hs = stats[0];
  EXPECT_EQ(hs.n, kCells - 2);
  EXPECT_EQ(hs.dropped, 2U);
  EXPECT_TRUE(hs.pass);  // the 22 compared cells agree; the drop is reported, not judged
  EXPECT_TRUE(ww::nccmp::all_pass(stats));
  EXPECT_NE(ww::nccmp::format_table(stats).find("dropped"), std::string::npos);
}

TEST_F(NccmpTol, UnlistedVariableReportedNotJudged) {
  const auto v = base_values();
  auto foo_ref = v;
  auto foo_test = v;
  for (auto& x : foo_test) x *= 3.0F;  // wildly different, but nobody asked
  FileSpec ref_spec;
  ref_spec.extra = &foo_ref;
  ref_spec.only_here = true;           // `lone` exists in ref only: never reported
  FileSpec test_spec;
  test_spec.extra = &foo_test;
  write_file(ref_, v, ref_spec);
  write_file(test_, v, test_spec);

  const auto stats = ww::nccmp::compare_files(ref_.string(), test_.string(), hs_only());
  ASSERT_EQ(stats.size(), 2U);
  EXPECT_EQ(find(stats, "lone"), nullptr);

  const Stats* hs = find(stats, "hs");
  ASSERT_NE(hs, nullptr);
  EXPECT_TRUE(hs->judged);
  EXPECT_TRUE(hs->pass);

  const Stats* foo = find(stats, "foo");
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->n, kCells);
  EXPECT_GT(foo->max_abs, 1.0);
  EXPECT_NEAR(foo->max_rel, 2.0, 1e-6);
  EXPECT_FALSE(foo->judged);
  EXPECT_FALSE(foo->pass);
  EXPECT_TRUE(ww::nccmp::all_pass(stats));

  const std::string table = ww::nccmp::format_table(stats);
  EXPECT_NE(table.find("foo"), std::string::npos);
  EXPECT_EQ(table.find("FAIL"), std::string::npos);
}
