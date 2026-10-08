// kokkos/tests/L1_test_bench_case.cpp
// Byte-equality test for the C++ benchmark-case generator.
//
// The fixture under tests/fixtures/bench_small/ is the output of the retired
// Python generator (`make_bench_case.py --size small`), captured once before it
// was deleted. ww::bench::write_case() has to reproduce every one of those files
// byte for byte -- same CFL arithmetic, same number formatting, same hand-written
// JSON -- so benchmark numbers taken before and after the rewrite stay
// comparable. ww3_ounf.nml is the one file the Python never wrote; its fixture
// copy was produced by this generator and is pinned here so it cannot drift.
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cstdlib>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bench_case.hpp"

namespace {

namespace fs = std::filesystem;

const fs::path kFixture = fs::path(WW_FIXTURE_DIR) / "bench_small";

const std::vector<std::string> kFiles = {"depth.inp",    "mask.inp",     "namelists.nml",
                                         "ww3_grid.nml", "ww3_shel.nml", "ww3_ounf.nml",
                                         "case.json"};

std::string slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) ADD_FAILURE() << "cannot open " << p;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

class BenchCaseSmall : public ::testing::Test {
 protected:
  void SetUp() override {
    out_ = fs::temp_directory_path() / "ww_bench_case_test";
    fs::remove_all(out_);
    const auto p = ww::bench::size_preset("small");
    ASSERT_TRUE(p.has_value());
    summary_ = ww::bench::write_case(out_, *p);
  }
  void TearDown() override { fs::remove_all(out_); }

  fs::path out_;
  ww::bench::Summary summary_{};
};

TEST_F(BenchCaseSmall, EveryFileIsByteIdenticalToThePythonFixture) {
  for (const auto& name : kFiles) {
    ASSERT_TRUE(fs::exists(kFixture / name)) << "missing fixture " << name;
    ASSERT_TRUE(fs::exists(out_ / name)) << "generator did not write " << name;
    EXPECT_EQ(slurp(out_ / name), slurp(kFixture / name)) << "differs: " << name;
  }
}

TEST_F(BenchCaseSmall, WritesNothingElse) {
  const auto n = std::distance(fs::directory_iterator(out_), fs::directory_iterator{});
  EXPECT_EQ(static_cast<std::size_t>(n), kFiles.size());
}

TEST_F(BenchCaseSmall, SummaryMatchesThePythonPrintout) {
  EXPECT_EQ(summary_.sea_points, 9520);
  EXPECT_EQ(summary_.spectral_bins, 576);
  EXPECT_EQ(summary_.state_values, 5483520);
  EXPECT_DOUBLE_EQ(summary_.state_mb, 21.93408);
  EXPECT_EQ(summary_.global_steps, 15);
  const std::string text = ww::bench::summary_text("case_small", summary_);
  EXPECT_NE(text.find("wrote case_small\n"), std::string::npos);
  EXPECT_NE(text.find("  grid          : 120 x 80 at 20 km\n"), std::string::npos);
  EXPECT_NE(text.find("  spectrum      : 24 freq x 24 dir = 576 bins\n"), std::string::npos);
  EXPECT_NE(text.find("  sea points    : 9,520\n"), std::string::npos);
  EXPECT_NE(text.find("  state array   : 22 MB (single precision)\n"), std::string::npos);
  EXPECT_NE(text.find("DTMAX=2820 DTXY=940 DTKTH=1410 DTMIN=10\n"), std::string::npos);
  EXPECT_NE(text.find("  global steps  : 15\n"), std::string::npos);
}

TEST(BenchCaseTimesteps, CflFormulaRoundsDownToTensAndFloorsAtTen) {
  const auto ts = ww::bench::timesteps(20000.0);
  EXPECT_DOUBLE_EQ(ts.dtxy, 940.0);
  EXPECT_DOUBLE_EQ(ts.dtmax, 2820.0);
  EXPECT_DOUBLE_EQ(ts.dtkth, 1410.0);
  EXPECT_DOUBLE_EQ(ts.dtmin, 10.0);
  // dx = 1 km: T_cfl = 52.8 s, 0.9 * T_cfl = 47.5 -> 40.
  EXPECT_DOUBLE_EQ(ww::bench::timesteps(1000.0).dtxy, 40.0);
  // dx = 100 m: the floor of 10 s wins.
  EXPECT_DOUBLE_EQ(ww::bench::timesteps(100.0).dtxy, 10.0);
}

TEST(BenchCasePythonRepr, MatchesPythonFloatRepr) {
  EXPECT_EQ(ww::bench::python_float_repr(2820.0), "2820.0");
  EXPECT_EQ(ww::bench::python_float_repr(10.0), "10.0");
  EXPECT_EQ(ww::bench::python_float_repr(20000.0), "20000.0");
  EXPECT_EQ(ww::bench::python_float_repr(100000.0), "100000.0");
  EXPECT_EQ(ww::bench::python_float_repr(0.04118), "0.04118");
  EXPECT_EQ(ww::bench::python_float_repr(21.93408), "21.93408");
  EXPECT_EQ(ww::bench::python_float_repr(0.1 + 0.2), "0.30000000000000004");
}

TEST(BenchCaseThousands, GroupsLikePythonsCommaFormat) {
  EXPECT_EQ(ww::bench::with_thousands(0), "0");
  EXPECT_EQ(ww::bench::with_thousands(999), "999");
  EXPECT_EQ(ww::bench::with_thousands(9520), "9,520");
  EXPECT_EQ(ww::bench::with_thousands(1234567), "1,234,567");
}

TEST(BenchCaseArgs, SizeThenOverrides) {
  const char* argv[] = {"ww_bench_case", "--size", "small", "--nth", "48", "-o", "here"};
  const auto o = ww::bench::parse_args(7, const_cast<char**>(argv));
  EXPECT_FALSE(o.help);
  EXPECT_EQ(o.params.nx, 120);
  EXPECT_EQ(o.params.ny, 80);
  EXPECT_EQ(o.params.nk, 24);
  EXPECT_EQ(o.params.nth, 48);
  EXPECT_EQ(o.params.hours, 12);
  EXPECT_DOUBLE_EQ(o.params.dx_m, 20000.0);
  EXPECT_EQ(o.out, "here");
}

TEST(BenchCaseArgs, DefaultsAreMediumInCaseMedium) {
  const char* argv[] = {"ww_bench_case"};
  const auto o = ww::bench::parse_args(1, const_cast<char**>(argv));
  EXPECT_EQ(o.params.nx, 300);
  EXPECT_EQ(o.out, "case_medium");
  const char* argv2[] = {"ww_bench_case", "--size", "large", "--dx-km", "10", "--hours", "6"};
  const auto o2 = ww::bench::parse_args(7, const_cast<char**>(argv2));
  EXPECT_EQ(o2.params.nx, 700);
  EXPECT_EQ(o2.params.hours, 6);
  EXPECT_DOUBLE_EQ(o2.params.dx_m, 10000.0);
  EXPECT_EQ(o2.out, "case_large");
}

TEST(BenchCaseArgs, RejectsBadInput) {
  const char* a[] = {"ww_bench_case", "--size", "huge"};
  EXPECT_THROW(ww::bench::parse_args(3, const_cast<char**>(a)), std::invalid_argument);
  const char* b[] = {"ww_bench_case", "--nx"};
  EXPECT_THROW(ww::bench::parse_args(2, const_cast<char**>(b)), std::invalid_argument);
  const char* c[] = {"ww_bench_case", "--nx", "abc"};
  EXPECT_THROW(ww::bench::parse_args(3, const_cast<char**>(c)), std::invalid_argument);
  const char* d[] = {"ww_bench_case", "--bogus"};
  EXPECT_THROW(ww::bench::parse_args(2, const_cast<char**>(d)), std::invalid_argument);
  const char* e[] = {"ww_bench_case", "--help"};
  EXPECT_TRUE(ww::bench::parse_args(2, const_cast<char**>(e)).help);
}

}  // namespace
