// kokkos/tests/bench_snl1.cpp
// ww_bench_snl1 -- the timing harness behind the ms/call column of
// kokkos/PORT_STATUS.md. Not a test: it asserts nothing and CTest does not run it.
//
// It times the DIA on 1 000 sea points, 20 calls, twice over:
//
//   shim   ww_snl1() through the bind(C) boundary -- host copy-in, kernel, fence,
//          host copy-out. This is what a WAVEWATCH III caller pays in phase 1 and
//          it is the number the ledger quotes.
//   kernel ww::snl1::snl1() on views that already live on the device. On a host
//          backend the two are nearly the same; on CUDA the gap between them *is*
//          the phase-1 transfer cost, which is the argument for phase 2.
//
// The spectrum is the fixture's three sea points tiled to 1 000, so the arithmetic
// is the same work the parity tests validate rather than a synthetic load.
//
// Timing caveat: ww_kokkos is built with -ffp-contract=off (and --fmad=false on
// CUDA) to hold bit parity with the Fortran. These numbers are therefore the cost
// of the *parity* build; an FMA-fused build would be faster and would not match
// the fixture. See kokkos/README.md.
//
// SPDX-License-Identifier: MIT

#include <Kokkos_Core.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include "fortran_iface/ww_kokkos_c.hpp"
#include "ww_kokkos/fixture_io.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/snl1_config.hpp"
#include "ww_kokkos/snl1_dia.hpp"
#include "ww_kokkos/snl1_tables.hpp"

namespace {

constexpr int kPoints = 1000;
constexpr int kCalls = 20;
constexpr int kWarmup = 3;

using Clock = std::chrono::steady_clock;

struct Host {
  ww::fixture::Snl1Fixture f;
  std::vector<float> a, cg, kdmean, s, d;
};

/// The fixture's points tiled out to `kPoints` columns of Fortran-ordered memory.
Host make_host(const std::string& path) {
  Host h;
  h.f = ww::fixture::load(path);
  const auto nk = static_cast<std::size_t>(h.f.nk);
  const auto nspec = static_cast<std::size_t>(h.f.nspec);
  const auto npts = static_cast<std::size_t>(kPoints);
  h.a.assign(nspec * npts, 0.0F);
  h.cg.assign(nk * npts, 0.0F);
  h.kdmean.assign(npts, 0.0F);
  h.s.assign(nspec * npts, 0.0F);
  h.d.assign(nspec * npts, 0.0F);
  for (std::size_t ip = 0; ip < npts; ++ip) {
    const ww::fixture::Snl1Point& p =
        h.f.points[ip % static_cast<std::size_t>(h.f.npts)];
    h.kdmean[ip] = p.kdmean;
    for (std::size_t ik = 0; ik < nk; ++ik) h.cg[ik + nk * ip] = p.cg[ik];
    for (std::size_t isp = 0; isp < nspec; ++isp) h.a[isp + nspec * ip] = p.a[isp];
  }
  return h;
}

void report(const char* what, double seconds, int calls) {
  const double ms = 1.0e3 * seconds / calls;
  std::printf("%-7s %8.3f ms/call   %10.3e points/s\n", what, ms,
              static_cast<double>(kPoints) * calls / seconds);
}

/// Time ww_snl1(): the whole phase-1 path a Fortran caller sees.
double time_shim(Host& h) {
  for (int i = 0; i < kWarmup; ++i)
    ww_snl1(kPoints, h.a.data(), h.cg.data(), h.kdmean.data(), h.s.data(), h.d.data());
  const Clock::time_point t0 = Clock::now();
  for (int i = 0; i < kCalls; ++i)
    ww_snl1(kPoints, h.a.data(), h.cg.data(), h.kdmean.data(), h.s.data(), h.d.data());
  return std::chrono::duration<double>(Clock::now() - t0).count();
}

/// Time ww::snl1::snl1() with every argument already resident on the device.
double time_kernel(const Host& h) {
  const ww::snl1::Config c{h.f.nk,    h.f.nth,  h.f.xfr,   h.f.dth,
                           h.f.lam,   h.f.snlc1, h.f.kdcon, h.f.kdmn,
                           h.f.snls1, h.f.snls2, h.f.snls3, h.f.fachfe};
  ww::snl1::HostRealView1D h_sig("bench.sig.host", h.f.nk);
  for (int ik = 0; ik < h.f.nk; ++ik) h_sig(ik) = h.f.sig[static_cast<std::size_t>(ik)];
  const ww::snl1::Tables t = ww::snl1::make_tables(c, h_sig);

  ww::snl1::RealView1D sig("bench.sig", h.f.nk);
  ww::snl1::RealView1D kd("bench.kdmean", kPoints);
  ww::snl1::RealView2D a("bench.a", h.f.nspec, kPoints);
  ww::snl1::RealView2D cg("bench.cg", h.f.nk, kPoints);
  ww::snl1::RealView2D s("bench.s", h.f.nspec, kPoints);
  ww::snl1::RealView2D d("bench.d", h.f.nspec, kPoints);

  using UnmanagedHost2D =
      Kokkos::View<const float**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  using UnmanagedHost1D =
      Kokkos::View<const float*, Kokkos::LayoutLeft, Kokkos::HostSpace,
                   Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
  Kokkos::deep_copy(sig, UnmanagedHost1D(h.f.sig.data(), h.f.nk));
  Kokkos::deep_copy(kd, UnmanagedHost1D(h.kdmean.data(), kPoints));
  Kokkos::deep_copy(a, UnmanagedHost2D(h.a.data(), h.f.nspec, kPoints));
  Kokkos::deep_copy(cg, UnmanagedHost2D(h.cg.data(), h.f.nk, kPoints));
  Kokkos::fence();

  for (int i = 0; i < kWarmup; ++i) ww::snl1::snl1(c, t, sig, a, cg, kd, s, d);
  Kokkos::fence();
  const Clock::time_point t0 = Clock::now();
  for (int i = 0; i < kCalls; ++i) ww::snl1::snl1(c, t, sig, a, cg, kd, s, d);
  Kokkos::fence();
  return std::chrono::duration<double>(Clock::now() - t0).count();
}

int run(const std::string& path) {
  Host h = make_host(path);
  if (ww_kokkos_init(-1) != WW_KOKKOS_OK) {
    std::fprintf(stderr, "ww_bench_snl1: ww_kokkos_init failed\n");
    return 1;
  }
  std::printf("ww_bench_snl1: nk=%d nth=%d nspec=%d points=%d calls=%d backend=%s\n",
              h.f.nk, h.f.nth, h.f.nspec, kPoints, kCalls,
              Kokkos::DefaultExecutionSpace::name());

  if (ww_snl1_init(h.f.nk, h.f.nth, h.f.xfr, h.f.dth, h.f.lam, h.f.snlc1, h.f.kdcon,
                   h.f.kdmn, h.f.snls1, h.f.snls2, h.f.snls3, h.f.fachfe,
                   h.f.sig.data()) != WW_KOKKOS_OK) {
    std::fprintf(stderr, "ww_bench_snl1: ww_snl1_init failed\n");
    ww_kokkos_finalize();
    return 1;
  }
  const double shim_s = time_shim(h);
  if (ww_snl1_last_error() != WW_KOKKOS_OK) {
    std::fprintf(stderr, "ww_bench_snl1: ww_snl1 failed, code %d\n", ww_snl1_last_error());
    ww_kokkos_finalize();
    return 1;
  }
  const double kernel_s = time_kernel(h);
  report("shim", shim_s, kCalls);
  report("kernel", kernel_s, kCalls);
  ww_kokkos_finalize();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string path = (argc > 1)
                               ? std::string(argv[1])
                               : std::string(WW_FIXTURE_DIR) + "/snl1_nk25_nth24.bin";
  try {
    return run(path);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "ww_bench_snl1: %s\n", e.what());
    return 1;
  }
}
