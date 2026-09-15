// kokkos/tests/L1_test_snl1_tables.cpp
// L1 parity: ww::snl1::make_tables() against INSNL1 from WAVEWATCH III 7.14.
//
// The reference values are not hand-written here: they are the ones the verbatim
// Fortran INSNL1 in tests/fixtures/snl1_ref.F90 produced, captured in the
// committed binary fixture. This test therefore fails the moment the C++ drifts
// from the Fortran, including for a table whose effect on the source term is too
// small to show up in the kernel-level test.
//
// The 32 index tables are compared exactly (EXPECT_EQ): an address is an integer,
// so "close" is not a category it has. The weights are compared relative, because
// they come out of acosf/asinf/powf and a one-ULP difference is expected.
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "kokkos_env.hpp"
#include "ww_kokkos/fixture_io.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/snl1_config.hpp"
#include "ww_kokkos/snl1_tables.hpp"

namespace {

/// Relative tolerance for everything INSNL1 computes with a libm call.
constexpr double kWeightRelTol = 1e-6;

std::string fixture_path() { return std::string(WW_FIXTURE_DIR) + "/snl1_nk25_nth24.bin"; }

/// The W3GDATMD-side parameters as the fixture recorded them.
ww::snl1::Config config_of(const ww::fixture::Snl1Fixture& f) {
  return ww::snl1::Config{f.nk,    f.nth,  f.xfr,   f.dth,   f.lam,   f.snlc1,
                          f.kdcon, f.kdmn, f.snls1, f.snls2, f.snls3, f.fachfe};
}

/// Copy a device index table to the host so the test can read it element-wise.
std::vector<int> to_host(const ww::snl1::IntView1D& v) {
  auto h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), v);
  std::vector<int> out(h.extent(0));
  for (std::size_t i = 0; i < h.extent(0); ++i) out[i] = h(i);
  return out;
}

std::vector<ww::Real> to_host(const ww::snl1::RealView1D& v) {
  auto h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), v);
  std::vector<ww::Real> out(h.extent(0));
  for (std::size_t i = 0; i < h.extent(0); ++i) out[i] = h(i);
  return out;
}

void expect_index_table(const std::vector<int>& got, const std::vector<int>& want,
                        const char* name) {
  ASSERT_EQ(got.size(), want.size()) << name;
  for (std::size_t i = 0; i < want.size(); ++i) {
    ASSERT_EQ(got[i], want[i]) << name << " at isp=" << i;
  }
}

void expect_rel(double got, double want, const char* name) {
  EXPECT_NEAR(got, want, kWeightRelTol * std::fabs(want)) << name;
}

}  // namespace

TEST(Snl1Tables, MatchesInsnl1Reference) {
  const ww::fixture::Snl1Fixture f = ww::fixture::load(fixture_path());
  ww::snl1::HostRealView1D sig("test.sig", f.nk);
  for (int ik = 0; ik < f.nk; ++ik) sig(ik) = f.sig[static_cast<std::size_t>(ik)];

  const ww::snl1::Tables t = ww::snl1::make_tables(config_of(f), sig);

  // Section 5 of INSNL1: the ranges every scratch allocation is sized from.
  EXPECT_EQ(t.nfr, f.nfr);
  EXPECT_EQ(t.nfrhgh, f.nfrhgh);
  EXPECT_EQ(t.nfrchg, f.nfrchg);
  EXPECT_EQ(t.nspecx, f.nspecx);
  EXPECT_EQ(t.nspecy, f.nspecy);
  EXPECT_EQ(t.nspec, f.nspec);

  // Section 2: the lambda-dependent weights.
  expect_rel(t.dal1, f.dal1, "dal1");
  expect_rel(t.dal2, f.dal2, "dal2");
  expect_rel(t.dal3, f.dal3, "dal3");

  // Section 9: the interpolation weights and their squares.
  for (int j = 0; j < 8; ++j) {
    expect_rel(t.awg[j], f.awg[j], "awg");
    expect_rel(t.swg[j], f.swg[j], "swg");
  }

  // Section 7: all 32 address tables, exactly.
  for (int k = 0; k < 2; ++k) {
    for (int j = 0; j < 4; ++j) {
      expect_index_table(to_host(t.ip[k][j]), f.ip[k][j], "ip");
      expect_index_table(to_host(t.im[k][j]), f.im[k][j], "im");
    }
  }
  for (int j = 0; j < 8; ++j) {
    expect_index_table(to_host(t.ic[j][0]), f.ic[j][0], "ic*1");
    expect_index_table(to_host(t.ic[j][1]), f.ic[j][1], "ic*2");
  }

  // Section 8: the f**11 scaling array. Its values span ten decades, so the
  // comparison has to be relative.
  const std::vector<ww::Real> af11 = to_host(t.af11);
  ASSERT_EQ(af11.size(), f.af11.size());
  for (std::size_t i = 0; i < af11.size(); ++i) {
    const double want = f.af11[i];
    ASSERT_NEAR(af11[i], want, kWeightRelTol * std::fabs(want)) << "af11 at isp=" << i;
  }
}

TEST(Snl1Tables, IndicesStayInsideTheScratchWindow) {
  // The port reads every table at scratch slot `index + nth`, so an index below
  // -nth or above the array it addresses would be an out-of-bounds read that the
  // parity test could still pass by luck. Check the window explicitly.
  const ww::fixture::Snl1Fixture f = ww::fixture::load(fixture_path());
  ww::snl1::HostRealView1D sig("test.sig", f.nk);
  for (int ik = 0; ik < f.nk; ++ik) sig(ik) = f.sig[static_cast<std::size_t>(ik)];

  const ww::snl1::Tables t = ww::snl1::make_tables(config_of(f), sig);
  const int nth = f.nth;

  for (int k = 0; k < 2; ++k) {
    for (int j = 0; j < 4; ++j) {
      for (int idx : to_host(t.ip[k][j])) {  // reads UE(1-NTH:NSPECY)
        ASSERT_GE(idx, -nth);
        ASSERT_LT(idx, t.nspecy);
      }
      for (int idx : to_host(t.im[k][j])) {
        ASSERT_GE(idx, -nth);
        ASSERT_LT(idx, t.nspecy);
      }
    }
  }
  for (int j = 0; j < 8; ++j) {
    for (int c = 0; c < 2; ++c) {
      for (int idx : to_host(t.ic[j][c])) {  // reads SA1/SA2(1-NTH:NSPECX)
        ASSERT_GE(idx, -nth);
        ASSERT_LT(idx, t.nspecx);
      }
    }
  }
}
