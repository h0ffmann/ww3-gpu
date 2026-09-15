// kokkos/tests/L1_test_snl1_dia.cpp
// L1 parity: ww::snl1::snl1() against W3SNL1 from WAVEWATCH III 7.14.
//
// The reference S and D come from the verbatim Fortran in
// tests/fixtures/snl1_ref.F90, captured in the committed binary fixture for three
// sea points (deep, 50 m, 10 m) of one JONSWAP x cos^2 sea state -- the depth
// sweep is what exercises the KDMEAN branch of section 1.
//
// Besides parity the suite pins down two properties that a wrong-but-plausible
// port can still get wrong: Snl is exactly cubic in the spectrum, and it is zero
// for a zero spectrum. Both are checked on the port, not on the fixture.
//
// The kernels live in free functions rather than in the TEST bodies because nvcc
// rejects an extended lambda inside a private member function, and a TEST body is
// one. Same reason as tests/L1_test_intro.cpp.
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "kokkos_env.hpp"
#include "ww_kokkos/fixture_io.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/snl1_config.hpp"
#include "ww_kokkos/snl1_dia.hpp"
#include "ww_kokkos/snl1_tables.hpp"

namespace {

/// Parity tolerance from the task brief: 1e-5 relative, with an absolute floor so
/// a bin whose reference value is a denormal cannot demand infinite precision.
constexpr double kRelTol = 1e-5;
constexpr double kAbsFloor = 1e-30;

std::string fixture_path() { return std::string(WW_FIXTURE_DIR) + "/snl1_nk25_nth24.bin"; }

ww::snl1::Config config_of(const ww::fixture::Snl1Fixture& f) {
  return ww::snl1::Config{f.nk,    f.nth,  f.xfr,   f.dth,   f.lam,   f.snlc1,
                          f.kdcon, f.kdmn, f.snls1, f.snls2, f.snls3, f.fachfe};
}

/// Everything the kernel needs, on the device, built once per test.
struct Case {
  ww::fixture::Snl1Fixture f;
  ww::snl1::Config c{};
  ww::snl1::Tables t;
  ww::snl1::RealView1D sig;
  ww::snl1::RealView1D kdmean;
  ww::snl1::RealView2D a, cg, s, d;
};

/// Load the fixture and stage its inputs on the device. `a_scale` multiplies the
/// action spectrum, which the cubic-scaling test uses.
Case make_case(ww::Real a_scale = static_cast<ww::Real>(1)) {
  Case k;
  k.f = ww::fixture::load(fixture_path());
  k.c = config_of(k.f);

  const int nk = k.f.nk;
  const int nspec = k.f.nspec;
  const int npts = k.f.npts;

  ww::snl1::HostRealView1D h_sig("test.sig", nk);
  for (int ik = 0; ik < nk; ++ik) h_sig(ik) = k.f.sig[static_cast<std::size_t>(ik)];
  k.t = ww::snl1::make_tables(k.c, h_sig);

  k.sig = ww::snl1::RealView1D("test.snl1.sig", nk);
  k.kdmean = ww::snl1::RealView1D("test.snl1.kdmean", npts);
  k.a = ww::snl1::RealView2D("test.snl1.a", nspec, npts);
  k.cg = ww::snl1::RealView2D("test.snl1.cg", nk, npts);
  k.s = ww::snl1::RealView2D("test.snl1.s", nspec, npts);
  k.d = ww::snl1::RealView2D("test.snl1.d", nspec, npts);

  auto h_sigd = Kokkos::create_mirror_view(k.sig);
  auto h_kd = Kokkos::create_mirror_view(k.kdmean);
  auto h_a = Kokkos::create_mirror_view(k.a);
  auto h_cg = Kokkos::create_mirror_view(k.cg);
  for (int ik = 0; ik < nk; ++ik) h_sigd(ik) = k.f.sig[static_cast<std::size_t>(ik)];
  for (int ip = 0; ip < npts; ++ip) {
    const ww::fixture::Snl1Point& p = k.f.points[static_cast<std::size_t>(ip)];
    h_kd(ip) = p.kdmean;
    for (int ik = 0; ik < nk; ++ik) h_cg(ik, ip) = p.cg[static_cast<std::size_t>(ik)];
    for (int isp = 0; isp < nspec; ++isp)
      h_a(isp, ip) = a_scale * p.a[static_cast<std::size_t>(isp)];
  }
  Kokkos::deep_copy(k.sig, h_sigd);
  Kokkos::deep_copy(k.kdmean, h_kd);
  Kokkos::deep_copy(k.a, h_a);
  Kokkos::deep_copy(k.cg, h_cg);
  return k;
}

void run(Case& k) {
  ww::snl1::snl1(k.c, k.t, k.sig, k.a, k.cg, k.kdmean, k.s, k.d);
  Kokkos::fence();  // the host reads s and d below
}

std::vector<ww::Real> column(const ww::snl1::RealView2D& v, int ip) {
  auto h = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), v);
  std::vector<ww::Real> out(h.extent(0));
  for (std::size_t i = 0; i < h.extent(0); ++i) out[i] = h(i, static_cast<std::size_t>(ip));
  return out;
}

/// Largest |got-want| / max(|want|, floor) over a column, with where it happened.
struct MaxErr {
  double value = 0.0;
  int at = -1;
};

MaxErr max_rel_error(const std::vector<ww::Real>& got, const std::vector<ww::Real>& want) {
  MaxErr m;
  for (std::size_t i = 0; i < want.size(); ++i) {
    const double w = want[i];
    const double rel = std::fabs(got[i] - w) / std::fmax(std::fabs(w), kAbsFloor);
    if (rel > m.value) {
      m.value = rel;
      m.at = static_cast<int>(i);
    }
  }
  return m;
}

}  // namespace

TEST(Snl1Dia, MatchesFortranReference) {
  Case k = make_case();
  run(k);

  double worst_s = 0.0;
  double worst_d = 0.0;
  for (int ip = 0; ip < k.f.npts; ++ip) {
    const ww::fixture::Snl1Point& p = k.f.points[static_cast<std::size_t>(ip)];
    const MaxErr es = max_rel_error(column(k.s, ip), p.s);
    const MaxErr ed = max_rel_error(column(k.d, ip), p.d);
    worst_s = std::fmax(worst_s, es.value);
    worst_d = std::fmax(worst_d, ed.value);
    EXPECT_LT(es.value, kRelTol)
        << "point " << ip << " (kdmean " << p.kdmean << "): S worst at isp=" << es.at;
    EXPECT_LT(ed.value, kRelTol)
        << "point " << ip << " (kdmean " << p.kdmean << "): D worst at isp=" << ed.at;
  }
  // Printed so the report can quote the number rather than "it passed".
  std::printf("[  PARITY  ] max relative error: S %.3e, D %.3e (tolerance %.0e)\n", worst_s,
              worst_d, kRelTol);
}

TEST(Snl1Dia, ZeroSpectrumGivesZeroSource) {
  Case k = make_case(static_cast<ww::Real>(0));
  run(k);
  for (int ip = 0; ip < k.f.npts; ++ip) {
    for (ww::Real v : column(k.s, ip)) EXPECT_EQ(v, static_cast<ww::Real>(0));
    for (ww::Real v : column(k.d, ip)) EXPECT_EQ(v, static_cast<ww::Real>(0));
  }
}

TEST(Snl1Dia, ScalesCubicallyWithTheSpectrum) {
  // Snl is a cubic form in the action density and its diagonal is quadratic, so
  // doubling A must multiply S by 8 and D by 4. That is a property of the DIA,
  // not of the fixture, and it catches a stray linear term or a missing factor.
  Case base = make_case(static_cast<ww::Real>(1));
  Case doubled = make_case(static_cast<ww::Real>(2));
  run(base);
  run(doubled);

  for (int ip = 0; ip < base.f.npts; ++ip) {
    const std::vector<ww::Real> s1 = column(base.s, ip);
    const std::vector<ww::Real> s2 = column(doubled.s, ip);
    const std::vector<ww::Real> d1 = column(base.d, ip);
    const std::vector<ww::Real> d2 = column(doubled.d, ip);
    for (std::size_t i = 0; i < s1.size(); ++i) {
      ASSERT_NEAR(s2[i], 8.0 * s1[i], 1e-4 * std::fabs(8.0 * s1[i]) + 1e-30)
          << "S at point " << ip << " isp=" << i;
      ASSERT_NEAR(d2[i], 4.0 * d1[i], 1e-4 * std::fabs(4.0 * d1[i]) + 1e-30)
          << "D at point " << ip << " isp=" << i;
    }
  }
}

TEST(Snl1Dia, IsReproducibleAcrossLaunches) {
  // Under the openmp-release preset this is the OpenMP-vs-Serial check the brief
  // asks for: the same kernel run twice on two teams must agree bit for bit. The
  // DIA has no reduction, so there is no summation order to vary and the equality
  // is exact -- if this ever fails, the kernel has a race, not a rounding issue.
  Case k = make_case();
  run(k);
  const std::vector<ww::Real> s0 = column(k.s, 0);
  const std::vector<ww::Real> d0 = column(k.d, 0);
  for (int rep = 0; rep < 4; ++rep) {
    run(k);
    const std::vector<ww::Real> s1 = column(k.s, 0);
    const std::vector<ww::Real> d1 = column(k.d, 0);
    for (std::size_t i = 0; i < s0.size(); ++i) {
      ASSERT_EQ(s0[i], s1[i]) << "repeat " << rep << " differs in S at isp=" << i;
      ASSERT_EQ(d0[i], d1[i]) << "repeat " << rep << " differs in D at isp=" << i;
    }
  }
}
