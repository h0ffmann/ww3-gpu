// kokkos/tests/L1_test_snl1_shim.cpp
// L1 parity and error handling of the bind(C) shim, exercised through the raw C
// API exactly as WAVEWATCH III's Fortran will call it.
//
// L1_test_snl1_dia.cpp already proves the kernel itself; what is on trial here is
// the boundary around it: the Fortran-ordered pointers arriving as LayoutLeft
// unmanaged views, the copy-in/copy-out, the persistent device buffers, and the
// promise that no exception and no crash can cross `extern "C"` -- a Fortran
// caller has no catch clause, so the shim must always come back with a code.
//
// The Kokkos runtime is started once by kokkos_env.hpp, so ww_kokkos_init() here
// always finds a live runtime and never takes ownership of it. Every test resets
// the shim's state with ww_kokkos_finalize() first, so the cases are order
// independent -- including the "called before init" one.
//
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

#include "fortran_iface/ww_kokkos_c.hpp"
#include "kokkos_env.hpp"
#include "ww_kokkos/fixture_io.hpp"
#include "ww_kokkos/real.hpp"

namespace {

/// Parity tolerance of the task brief: 1e-5 relative, with an absolute floor so a
/// bin whose reference value is a denormal cannot demand infinite precision.
constexpr double kRelTol = 1e-5;
constexpr double kAbsFloor = 1e-30;

std::string fixture_path() { return std::string(WW_FIXTURE_DIR) + "/snl1_nk25_nth24.bin"; }

/// The fixture repacked the way Fortran hands it over: one contiguous
/// column-major block per array, `a`/`s`/`d` of (nspec, npts) and `cg` of
/// (nk, npts). These are plain std::vectors -- the point of the test is that the
/// shim accepts ordinary caller-owned memory.
struct Batch {
  ww::fixture::Snl1Fixture f;
  std::vector<float> a, cg, kdmean, s, d;
};

Batch make_batch() {
  Batch b;
  b.f = ww::fixture::load(fixture_path());
  const std::size_t nk = static_cast<std::size_t>(b.f.nk);
  const std::size_t nspec = static_cast<std::size_t>(b.f.nspec);
  const std::size_t npts = static_cast<std::size_t>(b.f.npts);

  b.a.assign(nspec * npts, 0.0F);
  b.cg.assign(nk * npts, 0.0F);
  b.kdmean.assign(npts, 0.0F);
  // Poisoned rather than zeroed: if the shim forgets to copy the results back,
  // the comparison must fail loudly instead of comparing zeros to zeros.
  b.s.assign(nspec * npts, -1.0F);
  b.d.assign(nspec * npts, -1.0F);

  for (std::size_t ip = 0; ip < npts; ++ip) {
    const ww::fixture::Snl1Point& p = b.f.points[ip];
    b.kdmean[ip] = p.kdmean;
    for (std::size_t ik = 0; ik < nk; ++ik) b.cg[ik + nk * ip] = p.cg[ik];
    for (std::size_t isp = 0; isp < nspec; ++isp) b.a[isp + nspec * ip] = p.a[isp];
  }
  return b;
}

/// Hand the fixture header to ww_snl1_init, field for field.
int init_from(const ww::fixture::Snl1Fixture& f) {
  return ww_snl1_init(f.nk, f.nth, f.xfr, f.dth, f.lam, f.snlc1, f.kdcon, f.kdmn, f.snls1,
                      f.snls2, f.snls3, f.fachfe, f.sig.data());
}

/// Largest |got-want| / max(|want|, floor) over one point's column.
double max_rel_error(const std::vector<float>& got, std::size_t offset,
                     const std::vector<ww::Real>& want) {
  double worst = 0.0;
  for (std::size_t i = 0; i < want.size(); ++i) {
    const double w = want[i];
    const double rel = std::fabs(static_cast<double>(got[offset + i]) - w) /
                       std::fmax(std::fabs(w), kAbsFloor);
    worst = std::fmax(worst, rel);
  }
  return worst;
}

}  // namespace

TEST(Snl1Shim, ReportsAnErrorWhenCalledBeforeInit) {
  // A Fortran caller cannot catch anything, so calling out of order must produce
  // a code, not a crash and not an exception.
  ww_kokkos_finalize();
  float a = 0.0F, cg = 0.0F, kd = 0.0F, s = -1.0F, d = -1.0F;
  ww_snl1(1, &a, &cg, &kd, &s, &d);
  EXPECT_NE(ww_snl1_last_error(), WW_KOKKOS_OK);
  EXPECT_EQ(ww_snl1_last_error(), WW_KOKKOS_ERR_NOT_INITIALISED);
  // The outputs were left alone; nothing was written through the pointers.
  EXPECT_EQ(s, -1.0F);
  EXPECT_EQ(d, -1.0F);
}

TEST(Snl1Shim, RejectsBadShapesAndNullPointers) {
  ww_kokkos_finalize();
  ASSERT_EQ(ww_kokkos_init(-1), WW_KOKKOS_OK);
  const ww::fixture::Snl1Fixture f = ww::fixture::load(fixture_path());
  ASSERT_EQ(init_from(f), WW_KOKKOS_OK);

  float a = 0.0F, cg = 0.0F, kd = 0.0F, s = 0.0F, d = 0.0F;
  ww_snl1(-1, &a, &cg, &kd, &s, &d);
  EXPECT_EQ(ww_snl1_last_error(), WW_KOKKOS_ERR_BAD_SHAPE);
  ww_snl1(1, nullptr, &cg, &kd, &s, &d);
  EXPECT_EQ(ww_snl1_last_error(), WW_KOKKOS_ERR_BAD_SHAPE);
  // Zero points is a legal no-op, not an error: a WW3 rank may own no sea points.
  ww_snl1(0, &a, &cg, &kd, &s, &d);
  EXPECT_EQ(ww_snl1_last_error(), WW_KOKKOS_OK);

  // A bad grid is refused at set-up, and leaves the shim unusable rather than
  // half-configured.
  EXPECT_NE(ww_snl1_init(0, f.nth, f.xfr, f.dth, f.lam, f.snlc1, f.kdcon, f.kdmn, f.snls1,
                         f.snls2, f.snls3, f.fachfe, f.sig.data()),
            WW_KOKKOS_OK);
  ww_snl1(1, &a, &cg, &kd, &s, &d);
  EXPECT_EQ(ww_snl1_last_error(), WW_KOKKOS_ERR_NOT_INITIALISED);
  ww_kokkos_finalize();
}

TEST(Snl1Shim, MatchesFortranReferenceThroughTheCApi) {
  ww_kokkos_finalize();
  ASSERT_EQ(ww_kokkos_init(-1), WW_KOKKOS_OK);
  Batch b = make_batch();
  ASSERT_EQ(init_from(b.f), WW_KOKKOS_OK);

  ww_snl1(b.f.npts, b.a.data(), b.cg.data(), b.kdmean.data(), b.s.data(), b.d.data());
  ASSERT_EQ(ww_snl1_last_error(), WW_KOKKOS_OK);

  const std::size_t nspec = static_cast<std::size_t>(b.f.nspec);
  double worst_s = 0.0;
  double worst_d = 0.0;
  for (int ip = 0; ip < b.f.npts; ++ip) {
    const std::size_t off = nspec * static_cast<std::size_t>(ip);
    const ww::fixture::Snl1Point& p = b.f.points[static_cast<std::size_t>(ip)];
    const double es = max_rel_error(b.s, off, p.s);
    const double ed = max_rel_error(b.d, off, p.d);
    worst_s = std::fmax(worst_s, es);
    worst_d = std::fmax(worst_d, ed);
    EXPECT_LT(es, kRelTol) << "point " << ip << " (kdmean " << p.kdmean << "): S";
    EXPECT_LT(ed, kRelTol) << "point " << ip << " (kdmean " << p.kdmean << "): D";
  }
  std::printf("[  PARITY  ] shim max relative error: S %.3e, D %.3e (tolerance %.0e)\n",
              worst_s, worst_d, kRelTol);
  ww_kokkos_finalize();
}

TEST(Snl1Shim, ReusesAndGrowsItsDeviceBuffers) {
  // Two calls of different sizes through one set-up: the second must grow the
  // persistent buffers without disturbing the first one's answer, and a smaller
  // third call must not read the stale tail of the grown buffer.
  ww_kokkos_finalize();
  ASSERT_EQ(ww_kokkos_init(-1), WW_KOKKOS_OK);
  Batch b = make_batch();
  ASSERT_EQ(init_from(b.f), WW_KOKKOS_OK);
  const std::size_t nspec = static_cast<std::size_t>(b.f.nspec);

  std::vector<float> s1(nspec, -1.0F), d1(nspec, -1.0F);
  ww_snl1(1, b.a.data(), b.cg.data(), b.kdmean.data(), s1.data(), d1.data());
  ASSERT_EQ(ww_snl1_last_error(), WW_KOKKOS_OK);

  ww_snl1(b.f.npts, b.a.data(), b.cg.data(), b.kdmean.data(), b.s.data(), b.d.data());
  ASSERT_EQ(ww_snl1_last_error(), WW_KOKKOS_OK);

  std::vector<float> s3(nspec, -1.0F), d3(nspec, -1.0F);
  ww_snl1(1, b.a.data(), b.cg.data(), b.kdmean.data(), s3.data(), d3.data());
  ASSERT_EQ(ww_snl1_last_error(), WW_KOKKOS_OK);

  for (std::size_t i = 0; i < nspec; ++i) {
    ASSERT_EQ(s1[i], b.s[i]) << "grown call disagrees with the first at isp=" << i;
    ASSERT_EQ(d1[i], b.d[i]) << "grown call disagrees with the first at isp=" << i;
    ASSERT_EQ(s1[i], s3[i]) << "shrunk call disagrees with the first at isp=" << i;
    ASSERT_EQ(d1[i], d3[i]) << "shrunk call disagrees with the first at isp=" << i;
  }
  ww_kokkos_finalize();
}

TEST(Snl1Shim, InitIsIdempotentAndReadsTheSwitchFromTheEnvironment) {
  ww_kokkos_finalize();
  ASSERT_EQ(setenv("WW_KOKKOS_SNL1", "1", 1), 0);
  ASSERT_EQ(ww_kokkos_init(-1), WW_KOKKOS_OK);
  EXPECT_EQ(ww_snl1_enabled(), 1);
  // Calling it again must neither fail nor disturb the runtime: W3INIT runs once
  // per grid, and a multi-grid run calls it once per grid.
  ASSERT_EQ(ww_kokkos_init(-1), WW_KOKKOS_OK);
  EXPECT_EQ(ww_snl1_enabled(), 1);
  EXPECT_TRUE(Kokkos::is_initialized());

  ww_kokkos_finalize();
  ASSERT_EQ(unsetenv("WW_KOKKOS_SNL1"), 0);
  ASSERT_EQ(ww_kokkos_init(-1), WW_KOKKOS_OK);
  EXPECT_EQ(ww_snl1_enabled(), 0);
  // ww_kokkos_finalize() must not take down a runtime the shim did not start:
  // kokkos_env.hpp owns this one and will finalize it after the last test.
  ww_kokkos_finalize();
  EXPECT_TRUE(Kokkos::is_initialized());
}
