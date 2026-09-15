// kokkos/src/fortran_iface/snl1_shim.cpp
// The bind(C) shim: the only place where WAVEWATCH III's Fortran memory becomes
// Kokkos Views. Implements kokkos/src/fortran_iface/ww_kokkos_c.hpp; the Fortran
// side of the same contract is w3kokkosmd.F90.
//
// The whole file is one responsibility -- marshalling -- and it earns its keep in
// three places a naive `extern "C"` wrapper gets wrong:
//
//  * Ownership. Kokkos may be started by us, by another shim, or by a test's
//    ::testing::Environment. We track who started it and finalize only what we
//    started; an over-eager Kokkos::finalize() here would take down the caller's
//    runtime.
//  * Lifetime. Every Kokkos View must be destroyed *before* Kokkos::finalize().
//    The context is file-static, so its destructor would otherwise run at exit,
//    long after finalize. A Kokkos::push_finalize_hook() releases it whoever calls
//    finalize, which makes the shim safe inside a test binary that owns the
//    runtime.
//  * Errors. ww::snl1::snl1() throws std::runtime_error when the spectral grid
//    does not fit in level-0 team scratch. An exception unwinding into Fortran is
//    undefined behaviour, so nothing may leave this file: every entry point is a
//    try/catch that prints one line and records a code.
//
// Phase 1 is copy-in / kernel / copy-out, with the device buffers kept across
// calls and grown on demand. Phase 2 is where WW3 keeps its state on the device
// and this copy disappears.
//
// SPDX-License-Identifier: MIT

#include "fortran_iface/ww_kokkos_c.hpp"

#include <Kokkos_Core.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <utility>

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/snl1_config.hpp"
#include "ww_kokkos/snl1_dia.hpp"
#include "ww_kokkos/snl1_tables.hpp"

namespace {

namespace snl1 = ww::snl1;
using ww::Real;

/// The caller's arrays, borrowed. LayoutLeft because the pointer came from
/// Fortran; Unmanaged because the shim must not free it; HostSpace because that
/// is where a WW3 array lives in phase 1.
template <class T>
using Borrowed2D = Kokkos::View<T**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
template <class T>
using Borrowed1D = Kokkos::View<T*, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;

/// Everything the shim keeps between calls: one grid's tables and the device
/// buffers of the largest batch seen so far.
struct Ctx {
  snl1::Config cfg{};
  snl1::Tables tables{};
  snl1::RealView1D sig_d;
  snl1::RealView1D kdmean_d;
  snl1::RealView2D a_d, cg_d, s_d, d_d;
  int capacity = 0;  ///< columns the device buffers currently hold
  bool ready = false;
};

/// Function-local so its construction order is defined; never destroyed through
/// this reference -- release() empties it before Kokkos::finalize().
Ctx& ctx() {
  static Ctx c;
  return c;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_we_started_kokkos = false;  ///< only then may we finalize it
bool g_runtime_ready = false;      ///< ww_kokkos_init() completed
bool g_hook_pushed = false;
bool g_enabled = false;  ///< WW_KOKKOS_SNL1 as read at ww_kokkos_init()

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: %s (code %d)\n", what, code);
}

/// Drop every View the shim holds. Idempotent, and callable from a finalize hook.
void release() {
  Ctx& c = ctx();
  c.tables = snl1::Tables{};
  c.sig_d = snl1::RealView1D();
  c.kdmean_d = snl1::RealView1D();
  c.a_d = snl1::RealView2D();
  c.cg_d = snl1::RealView2D();
  c.s_d = snl1::RealView2D();
  c.d_d = snl1::RealView2D();
  c.cfg = snl1::Config{};
  c.capacity = 0;
  c.ready = false;
}

/// Registered with Kokkos so the Views die inside finalize, not at process exit.
void release_on_finalize() {
  release();
  g_runtime_ready = false;
  g_we_started_kokkos = false;
  g_hook_pushed = false;
}

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}

#if defined(KOKKOS_ENABLE_CUDA) || defined(KOKKOS_ENABLE_HIP) || defined(KOKKOS_ENABLE_SYCL)
/// WW_KOKKOS_DEVICE_ID, or 0 if unset or unparsable. See ww_kokkos_c.hpp for why
/// this is an environment variable and not MPI_Comm_rank(). Only compiled on a
/// backend that has a device to select; on Serial/OpenMP there is nothing to pick.
int device_id_from_env() {
  const char* v = std::getenv("WW_KOKKOS_DEVICE_ID");
  if (v == nullptr || *v == '\0') return 0;
  char* end = nullptr;
  const long id = std::strtol(v, &end, 10);
  if (end == v || *end != '\0' || id < 0 || id > 65535) {
    std::fprintf(stderr, "ww_kokkos: WW_KOKKOS_DEVICE_ID='%s' is not a device id; using 0\n",
                 v);
    return 0;
  }
  return static_cast<int>(id);
}
#endif

/// (Re)allocate the device batch buffers for at least `npts` columns.
void grow(Ctx& c, int npts) {
  const int nspec = c.cfg.nspec();
  // Release before allocating: on a GPU the old and the new buffers are the same
  // order of magnitude, and holding both would double the peak footprint.
  c.kdmean_d = snl1::RealView1D();
  c.a_d = snl1::RealView2D();
  c.cg_d = snl1::RealView2D();
  c.s_d = snl1::RealView2D();
  c.d_d = snl1::RealView2D();
  c.capacity = 0;

  c.kdmean_d = snl1::RealView1D("ww_snl1.kdmean", npts);
  c.a_d = snl1::RealView2D("ww_snl1.a", nspec, npts);
  c.cg_d = snl1::RealView2D("ww_snl1.cg", c.cfg.nk, npts);
  c.s_d = snl1::RealView2D("ww_snl1.s", nspec, npts);
  c.d_d = snl1::RealView2D("ww_snl1.d", nspec, npts);
  c.capacity = npts;
}

}  // namespace

extern "C" {

int ww_kokkos_init(int comm_f) {
  // Phase 1 links no MPI; the communicator is accepted and ignored on purpose,
  // so that phase 2 can honour it without changing this ABI.
  (void)comm_f;
  try {
    if (Kokkos::is_finalized()) {
      set_error(WW_KOKKOS_ERR_RUNTIME, "Kokkos has already been finalized");
      return WW_KOKKOS_ERR_RUNTIME;
    }
    if (!Kokkos::is_initialized()) {
      Kokkos::InitializationSettings settings;
#if defined(KOKKOS_ENABLE_CUDA) || defined(KOKKOS_ENABLE_HIP) || defined(KOKKOS_ENABLE_SYCL)
      settings.set_device_id(device_id_from_env());
#endif
      Kokkos::initialize(settings);
      g_we_started_kokkos = true;
    }
    if (!g_hook_pushed) {
      Kokkos::push_finalize_hook(release_on_finalize);
      g_hook_pushed = true;
    }
    g_enabled = env_is_one("WW_KOKKOS_SNL1");
    g_runtime_ready = true;
    g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed);
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_kokkos_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_kokkos_finalize(void) {
  try {
    const bool ours = g_we_started_kokkos;
    release();
    g_runtime_ready = false;
    g_enabled = false;
    g_we_started_kokkos = false;
    if (ours && Kokkos::is_initialized()) Kokkos::finalize();
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_kokkos_finalize: unknown exception");
  }
}

int ww_snl1_init(int nk, int nth, float xfr, float dth, float lam, float snlc1,
                 float kdcon, float kdmn, float snls1, float snls2, float snls3,
                 float fachfe, const float* sig) {
  Ctx& c = ctx();
  try {
    // Anything that goes wrong below leaves the shim unconfigured rather than
    // half-configured: a later ww_snl1() must say NOT_INITIALISED, not crash.
    // Inside the try, because nothing at all may escape this function.
    release();
    if (!g_runtime_ready || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_snl1_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (nk <= 0 || nth <= 0 || sig == nullptr) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_snl1_init: nk and nth must be > 0 and sig non-null");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    const snl1::Config cfg{nk,    nth,   xfr,   dth,   lam,   snlc1,
                           kdcon, kdmn,  snls1, snls2, snls3, fachfe};

    // SIG is copied, not borrowed: INSNL1 wants it on the host and W3SNL1 on the
    // device, and the caller's array may be gone by the next ww_snl1().
    snl1::HostRealView1D h_sig("ww_snl1.sig.host", nk);
    for (int ik = 0; ik < nk; ++ik) h_sig(ik) = sig[ik];

    snl1::Tables tables = snl1::make_tables(cfg, h_sig);
    if (tables.nspec != cfg.nspec()) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_snl1_init: tables disagree with nk*nth");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    snl1::RealView1D sig_d("ww_snl1.sig", nk);
    Kokkos::deep_copy(sig_d, h_sig);

    c.cfg = cfg;
    c.tables = std::move(tables);
    c.sig_d = sig_d;
    c.ready = true;
    g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed);
    return WW_KOKKOS_OK;
  } catch (const std::bad_alloc& e) {
    release();
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (const std::exception& e) {
    release();
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
    return WW_KOKKOS_ERR_KERNEL;
  } catch (...) {
    release();
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_snl1_init: unknown exception");
    return WW_KOKKOS_ERR_KERNEL;
  }
}

void ww_snl1(int npts, const float* a, const float* cg, const float* kdmean, float* s,
             float* d) {
  Ctx& c = ctx();
  try {
    if (!c.ready || !g_runtime_ready || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_snl1 before ww_snl1_init");
      return;
    }
    if (npts < 0) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_snl1: npts is negative");
      return;
    }
    // The one shape invariant the C API cannot carry in its arguments: the
    // batch's leading extent is nspec, and nspec is nk*nth from ww_snl1_init().
    if (c.tables.nspec != c.cfg.nspec() || c.cfg.nspec() <= 0) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_snl1: nspec does not match nk*nth");
      return;
    }
    if (npts == 0) {  // a rank may own no sea points; that is not an error
      g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed);
      return;
    }
    if (a == nullptr || cg == nullptr || kdmean == nullptr || s == nullptr ||
        d == nullptr) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_snl1: null array pointer");
      return;
    }

    const int nk = c.cfg.nk;
    const int nspec = c.cfg.nspec();
    if (npts > c.capacity) grow(c, npts);

    const Borrowed2D<const Real> h_a(a, nspec, npts);
    const Borrowed2D<const Real> h_cg(cg, nk, npts);
    const Borrowed1D<const Real> h_kd(kdmean, npts);
    const Borrowed2D<Real> h_s(s, nspec, npts);
    const Borrowed2D<Real> h_d(d, nspec, npts);

    // The buffers may be wider than this batch; work on the first npts columns.
    // A LayoutLeft subview of (ALL, range) is still LayoutLeft and contiguous.
    const auto cols = Kokkos::make_pair(0, npts);
    auto a_d = Kokkos::subview(c.a_d, Kokkos::ALL, cols);
    auto cg_d = Kokkos::subview(c.cg_d, Kokkos::ALL, cols);
    auto kd_d = Kokkos::subview(c.kdmean_d, cols);
    auto s_d = Kokkos::subview(c.s_d, Kokkos::ALL, cols);
    auto d_d = Kokkos::subview(c.d_d, Kokkos::ALL, cols);

    Kokkos::deep_copy(a_d, h_a);
    Kokkos::deep_copy(cg_d, h_cg);
    Kokkos::deep_copy(kd_d, h_kd);

    snl1::snl1(c.cfg, c.tables, c.sig_d, a_d, cg_d, kd_d, s_d, d_d);
    Kokkos::fence();  // the caller reads s and d the instant we return

    Kokkos::deep_copy(h_s, s_d);
    Kokkos::deep_copy(h_d, d_d);
    g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed);
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_snl1: unknown exception");
  }
}

int ww_snl1_enabled(void) { return g_enabled ? 1 : 0; }

int ww_snl1_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
