// kokkos/src/fortran_iface/flx_shim.cpp
// The bind(C) shim of FLX: the C ABI of ww_kokkos_c.hpp ("FLX") over the
// per-point device functions of ww_kokkos/srce_flx1.hpp .. srce_flx5.hpp
// (W3FLX1..W3FLX5). Marshalling only: the arithmetic is in the kernels.
//
// The rules are st4_shim.cpp's: one context per WW3 grid (IMOD at W3INIT,
// IGRID at W3SRCE); nothing throws across the boundary; Views die inside
// Kokkos::finalize (a finalize hook), not at process exit; inside an OpenMP
// parallel region (W3SRCE under W3_OMPG) the kernels run on Kokkos::Serial.
// Phase 1 copies in, runs, copies out on every call, and the caller serialises
// the calls (!$OMP CRITICAL in the caller patch).
//
// State. Per grid: NITTIN, CINXSI, CD_MAX, CAP_ID, FLX4A0 (W3GDATMD, through
// MPARS(IMOD)%SFLPS). Per process: W3DISPMD's EWN1(0:NAR1D), DSIE, N1MAX, which
// W3IOGR fills once per process with DISTAB (w3iogrmd.F90:2015-2021, guarded by
// FLDISP) from constants alone. The shim registers WW3's own EWN1, DSIE and
// N1MAX: the bits W3FLX2/W3FLX3 read, by construction. The C++ DISTAB
// (disp_distab.hpp) is a translation not yet regtested, so it replaces them only
// with WW_KOKKOS_FLX_TABLES=1, which also reports how many values differ.

#include "fortran_iface/ww_kokkos_c.hpp"

#include <Kokkos_Core.hpp>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <new>
#include <string>
#include <type_traits>

#include "ww_kokkos/disp_distab.hpp"
#include "ww_kokkos/real.hpp"
#include "ww_kokkos/srce_flx1.hpp"
#include "ww_kokkos/srce_flx2.hpp"
#include "ww_kokkos/srce_flx3.hpp"
#include "ww_kokkos/srce_flx4.hpp"
#include "ww_kokkos/srce_flx5.hpp"

// Kokkos::Serial for a caller inside an OpenMP parallel region; host builds
// only, as WW_ST4_SERIAL_NESTED (st4_arrays.hpp).
#if defined(KOKKOS_ENABLE_OPENMP) && defined(KOKKOS_ENABLE_SERIAL) && \
    !defined(KOKKOS_ENABLE_CUDA) && !defined(KOKKOS_ENABLE_HIP) && !defined(KOKKOS_ENABLE_SYCL)
#define WW_FLX_SERIAL_NESTED 1
#include <omp.h>
#endif

namespace ww::flxshim {
namespace {

using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
template <class T>
using Borrowed2D = Kokkos::View<T**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using HostReal1D = Kokkos::View<Real*, Kokkos::HostSpace>;
template <class T>
using Dev2D = Kokkos::View<T**, Kokkos::LayoutLeft, DeviceSpace>;

constexpr int kNEwn1 = ww::disp::kNar1d + 1;  // EWN1(0:NAR1D)

/// The scalars of one grid, as W3GDATMD points at them for IMOD.
struct GridParams {
  int flx = 0;  ///< the compiled W3_FLXn
  int nittin = 0, cap_id = 0;
  Real cinxsi = 0, cd_max = 0, flx4a0 = 0;
};

struct GridCtx {
  GridParams p{};
  std::map<std::string, Dev2D<Real>> rbuf;  ///< batch buffers, grown, never shrunk
  std::map<std::string, Dev2D<int>> ibuf;
  bool configured = false;  ///< ww_flx_init done
  bool ready = false;       ///< ww_flx_commit done
};

/// W3DISPMD's module state: one per process, last writer wins.
struct ModuleCtx {
  HostReal1D ewn1_h;  ///< EWN1(0:NAR1D), index i = Fortran index i
  DevReal1D ewn1_d;
  Real dsie = 0;
  int n1max = 0;
  bool scalars = false;  ///< DSIE, N1MAX given by an FLX2/FLX3 grid
};

std::map<int, GridCtx>& grids() {
  static std::map<int, GridCtx> m;
  return m;
}
ModuleCtx& module_ctx() {
  static ModuleCtx m;
  return m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_hook_pushed = false;
std::atomic<long long> g_count[6] = {};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: FLX: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }

void release() {
  grids().clear();
  module_ctx() = ModuleCtx{};
}

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}

struct Flags {
  bool flx[6];  ///< [1..5] WW_KOKKOS_FLX1..5
  bool tables, count;
};
const Flags& flags() {
  static const Flags f{{false, (ww_kokkos_flag("WW_KOKKOS_FLX1") != 0), (ww_kokkos_flag("WW_KOKKOS_FLX2") != 0),
                        (ww_kokkos_flag("WW_KOKKOS_FLX3") != 0), (ww_kokkos_flag("WW_KOKKOS_FLX4") != 0),
                        (ww_kokkos_flag("WW_KOKKOS_FLX5") != 0)},
                       (ww_kokkos_flag("WW_KOKKOS_FLX_TABLES") != 0),
                       env_is_one("WW_KOKKOS_FLX_COUNT")};
  return f;
}

void print_counts() {
  for (int n = 1; n <= 5; ++n) {
    const long long c = g_count[n].load();
    if (c > 0) std::fprintf(stderr, "ww_kokkos: W3FLX%d points computed: %lld\n", n, c);
  }
}

bool nested_in_openmp() {
#if defined(WW_FLX_SERIAL_NESTED)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

/// WW_KOKKOS_FLX_TABLES=1: EWN1, DSIE, N1MAX from the C++ DISTAB, with the
/// number of values that differ from the Fortran's on stderr.
void rebuild_distab(ModuleCtx& m) {
  using Host = Kokkos::DefaultHostExecutionSpace;
  Kokkos::View<Real*, Host::memory_space> ewn1("ww_flx.ewn1_cpp", kNEwn1);
  Kokkos::View<Real*, Host::memory_space> ecg1("ww_flx.ecg1_cpp", kNEwn1);
  const ww::disp::DistabScalars s = ww::disp::distab(Host(), ewn1, ecg1);
  long long diff = 0;
  for (int i = 0; i < kNEwn1; ++i)
    if (std::memcmp(&ewn1(i), &m.ewn1_h(i), sizeof(Real)) != 0) ++diff;
  if (std::memcmp(&s.dsie, &m.dsie, sizeof(Real)) != 0) ++diff;
  if (s.n1max != m.n1max) ++diff;
  std::fprintf(stderr,
               "ww_kokkos: W3DISPMD tables rebuilt in C++ (DISTAB): %lld of EWN1(0:%d), "
               "DSIE, N1MAX differ from the Fortran's\n",
               diff, ww::disp::kNar1d);
  m.ewn1_h = HostReal1D("ww_flx.ewn1", kNEwn1);
  Kokkos::deep_copy(m.ewn1_h, ewn1);
  m.dsie = s.dsie;
  m.n1max = s.n1max;
}

/// A (rows, >= npts) device buffer named `name`, on Serial when nested.
template <class T>
Dev2D<T>& buffer(std::map<std::string, Dev2D<T>>& pool, bool nested, const std::string& name,
                 int rows, int npts) {
  Dev2D<T>& v = pool[name];
  if (v.extent(0) != static_cast<size_t>(rows) || v.extent(1) < static_cast<size_t>(npts)) {
    v = Dev2D<T>();
#if defined(WW_FLX_SERIAL_NESTED)
    if (nested) {
      v = Dev2D<T>(Kokkos::view_alloc(Kokkos::Serial(), "ww_flx." + name), rows, npts);
      return v;
    }
#endif
    v = Dev2D<T>(Kokkos::view_alloc("ww_flx." + name), rows, npts);
  }
  return v;
}

/// Runs the batch on the right execution space and fences.
struct Exec {
  bool nested;
  template <class F>
  void for_points(const char* name, int npts, const F& f) const {
#if defined(WW_FLX_SERIAL_NESTED)
    if (nested) {
      Kokkos::parallel_for(name, Kokkos::RangePolicy<Kokkos::Serial>(0, npts), f);
      Kokkos::Serial().fence();
      return;
    }
#endif
    Kokkos::parallel_for(name, Kokkos::RangePolicy<>(0, npts), f);
    Kokkos::fence();
  }
  template <class Dst, class Src>
  void copy(const Dst& dst, const Src& src) const {
#if defined(WW_FLX_SERIAL_NESTED)
    if (nested) {
      Kokkos::deep_copy(Kokkos::Serial(), dst, src);
      return;
    }
#endif
    Kokkos::deep_copy(dst, src);
  }
};

/// Caller array (1, npts) -> the first npts columns of a device buffer, and back.
template <class T>
struct Staged {
  Dev2D<std::remove_const_t<T>> all;  ///< (1, capacity)
  Borrowed2D<T> host;
  auto cols(int npts) const { return Kokkos::subview(all, Kokkos::ALL, Kokkos::make_pair(0, npts)); }
};

template <class T>
Staged<T> stage_in(const Exec& ex, std::map<std::string, Dev2D<std::remove_const_t<T>>>& pool,
                   const char* name, T* p, int npts) {
  Staged<T> s{buffer(pool, ex.nested, name, 1, npts), Borrowed2D<T>(p, 1, npts)};
  ex.copy(s.cols(npts), s.host);
  return s;
}

template <class T>
void stage_out(const Exec& ex, const Staged<T>& s, int npts) {
  ex.copy(s.host, s.cols(npts));
}

/// The common checks of a compute entry point: the grid is committed and was
/// configured for this W3FLXn. Returns the grid, or nullptr after recording
/// the error.
GridCtx* compute_ctx(int igrid, int npts, int n, const char* who) {
  const auto it = grids().find(igrid);
  if (it == grids().end() || !it->second.ready || !Kokkos::is_initialized()) {
    set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
              (std::string(who) + " before ww_flx_commit for this grid").c_str());
    return nullptr;
  }
  if (it->second.p.flx != n) {
    set_error(WW_KOKKOS_ERR_UNSUPPORTED,
              (std::string(who) + ": the grid was configured for another W3FLXn").c_str());
    return nullptr;
  }
  if (npts < 0) {
    set_error(WW_KOKKOS_ERR_BAD_SHAPE, (std::string(who) + ": npts is negative").c_str());
    return nullptr;
  }
  return &it->second;
}

template <class F>
void guarded(const char* who, F&& f) {
  try {
    f();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, (std::string(who) + ": unknown exception").c_str());
  }
}

/// W3FLX1 and W3FLX4 share the argument list; `n` picks the kernel.
void flx_wu_hwang(int n, const char* who, int igrid, int npts, const float* zwnd,
                  const float* u10, const float* u10d, float* ust, float* ustd, float* z0,
                  float* cd, int* iext) {
  guarded(who, [&] {
    GridCtx* g = compute_ctx(igrid, npts, n, who);
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    auto& R = g->rbuf;
    const auto zw_s = stage_in(ex, R, "zwnd", zwnd, npts);
    const auto u_s = stage_in(ex, R, "u10", u10, npts);
    const auto ud_s = stage_in(ex, R, "u10d", u10d, npts);
    const auto us_s = stage_in(ex, R, "ust", ust, npts);
    const auto usd_s = stage_in(ex, R, "ustd", ustd, npts);
    const auto z0_s = stage_in(ex, R, "z0", z0, npts);
    const auto cd_s = stage_in(ex, R, "cd", cd, npts);
    const auto ie_s = stage_in(ex, g->ibuf, "iext", iext, npts);
    const auto ZW = zw_s.all, U = u_s.all, UD = ud_s.all, US = us_s.all, USD = usd_s.all,
               Z0 = z0_s.all, CD = cd_s.all;
    const auto IE = ie_s.all;
    if (n == 1) {
      ex.for_points("ww_flx.flx1", npts, KOKKOS_LAMBDA(const int ip) {
        IE(0, ip) = ww::srce::flx::w3flx1_point(ZW(0, ip), U(0, ip), UD(0, ip), US(0, ip),
                                                USD(0, ip), Z0(0, ip), CD(0, ip));
      });
    } else {
      const Real a0 = g->p.flx4a0;
      ex.for_points("ww_flx.flx4", npts, KOKKOS_LAMBDA(const int ip) {
        IE(0, ip) = ww::srce::flx::w3flx4_point(ZW(0, ip), U(0, ip), UD(0, ip), US(0, ip),
                                                USD(0, ip), Z0(0, ip), CD(0, ip), a0);
      });
    }
    stage_out(ex, us_s, npts);
    stage_out(ex, usd_s, npts);
    stage_out(ex, z0_s, npts);
    stage_out(ex, cd_s, npts);
    stage_out(ex, ie_s, npts);
    g_count[n].fetch_add(npts, std::memory_order_relaxed);
    ok();
  });
}

/// W3FLX2 and W3FLX3 share the argument list; `n` picks the kernel.
void flx_tc(int n, const char* who, int igrid, int npts, const float* zwind, const float* depth,
            const float* fp, const float* u, const float* udir, float* ust, float* ustd,
            float* z0, float* cd) {
  guarded(who, [&] {
    GridCtx* g = compute_ctx(igrid, npts, n, who);
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const ModuleCtx& m = module_ctx();
    const Exec ex{nested_in_openmp()};
    auto& R = g->rbuf;
    const auto zw_s = stage_in(ex, R, "zwind", zwind, npts);
    const auto dp_s = stage_in(ex, R, "depth", depth, npts);
    const auto fp_s = stage_in(ex, R, "fp", fp, npts);
    const auto u_s = stage_in(ex, R, "u", u, npts);
    const auto ud_s = stage_in(ex, R, "udir", udir, npts);
    const auto us_s = stage_in(ex, R, "ust", ust, npts);
    const auto usd_s = stage_in(ex, R, "ustd", ustd, npts);
    const auto z0_s = stage_in(ex, R, "z0", z0, npts);
    const auto cd_s = stage_in(ex, R, "cd", cd, npts);
    const auto ZW = zw_s.all, DP = dp_s.all, FP = fp_s.all, U = u_s.all, UD = ud_s.all,
               US = us_s.all, USD = usd_s.all, Z0 = z0_s.all, CD = cd_s.all;
    const DevReal1D E = m.ewn1_d;
    const int nittin = g->p.nittin, cap_id = g->p.cap_id, n1max = m.n1max;
    const Real cinxsi = g->p.cinxsi, cd_max = g->p.cd_max, dsie = m.dsie;
    if (n == 2) {
      ex.for_points("ww_flx.flx2", npts, KOKKOS_LAMBDA(const int ip) {
        ww::srce::flx::w3flx2_point(ZW(0, ip), DP(0, ip), FP(0, ip), U(0, ip), UD(0, ip),
                                    US(0, ip), USD(0, ip), Z0(0, ip), CD(0, ip), nittin, cinxsi,
                                    dsie, n1max, E.data());
      });
    } else {
      ex.for_points("ww_flx.flx3", npts, KOKKOS_LAMBDA(const int ip) {
        ww::srce::flx::w3flx3_point(ZW(0, ip), DP(0, ip), FP(0, ip), U(0, ip), UD(0, ip),
                                    US(0, ip), USD(0, ip), Z0(0, ip), CD(0, ip), nittin, cinxsi,
                                    cd_max, cap_id, dsie, n1max, E.data());
      });
    }
    stage_out(ex, us_s, npts);
    stage_out(ex, usd_s, npts);
    stage_out(ex, z0_s, npts);
    stage_out(ex, cd_s, npts);
    g_count[n].fetch_add(npts, std::memory_order_relaxed);
    ok();
  });
}

}  // namespace
}  // namespace ww::flxshim

using namespace ww::flxshim;
using ww::Real;

extern "C" {

int ww_flx_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_flx_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != WW_FLX_NRPAR || nipar != WW_FLX_NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_flx_init: RPAR/IPAR length differs from ww_kokkos_c.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    GridParams p;
    p.flx = ipar[WW_FLX_I_FLX];
    p.nittin = ipar[WW_FLX_I_NITTIN];
    p.cap_id = ipar[WW_FLX_I_CAP_ID];
    p.cinxsi = rpar[WW_FLX_R_CINXSI];
    p.cd_max = rpar[WW_FLX_R_CD_MAX];
    p.flx4a0 = rpar[WW_FLX_R_FLX4A0];
    if (p.flx < 1 || p.flx > 5) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_flx_init: IPAR(FLX) is not 1..5");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    if (!g_hook_pushed) {
      Kokkos::push_finalize_hook(release);
      g_hook_pushed = true;
    }
    if (flags().count) {
      static bool registered = false;
      if (!registered) {
        std::atexit(print_counts);
        registered = true;
      }
    }
    if (p.flx == 2 || p.flx == 3) {  // W3DISPMD scalars: the last grid to initialise wins
      ModuleCtx& m = module_ctx();
      m.dsie = rpar[WW_FLX_R_DSIE];
      m.n1max = ipar[WW_FLX_I_N1MAX];
      m.scalars = true;
    }
    GridCtx& g = grids()[igrid];
    g.p = p;
    g.configured = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_flx_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_flx_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_flx_set_real before ww_flx_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (id != WW_FLX_A_EWN1 || data == nullptr || lbound == nullptr || extent == nullptr ||
        rank != 1 || lbound[0] != 0 || extent[0] != kNEwn1) {
      set_error(WW_KOKKOS_ERR_TABLE,
                "ww_flx_set_real: expected EWN1(0:NAR1D), rank 1, LBOUND 0, 122 values");
      return WW_KOKKOS_ERR_TABLE;
    }
    ModuleCtx& m = module_ctx();
    m.ewn1_h = HostReal1D("ww_flx.ewn1", kNEwn1);
    std::memcpy(m.ewn1_h.data(), data, sizeof(Real) * kNEwn1);
    it->second.ready = false;
    ok();
    return WW_KOKKOS_OK;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_flx_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_flx_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_flx_commit before ww_flx_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    if (g.p.flx == 2 || g.p.flx == 3) {
      ModuleCtx& m = module_ctx();
      if (m.ewn1_h.data() == nullptr || !m.scalars) {
        set_error(WW_KOKKOS_ERR_TABLE, "ww_flx_commit: EWN1 not registered");
        return WW_KOKKOS_ERR_TABLE;
      }
      if (flags().tables) rebuild_distab(m);
      // The kernels read EWN1(I1+1) for I1 <= N1MAX (w3flx2md.F90:198-200).
      if (m.n1max < 0 || m.n1max + 1 > ww::disp::kNar1d || !(m.dsie > 0)) {
        set_error(WW_KOKKOS_ERR_TABLE, "ww_flx_commit: N1MAX or DSIE out of range");
        return WW_KOKKOS_ERR_TABLE;
      }
      m.ewn1_d = Kokkos::create_mirror_view_and_copy(DeviceSpace(), m.ewn1_h);
    }
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_flx_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_flx1(int igrid, int npts, const float* zwnd, const float* u10, const float* u10d,
             float* ust, float* ustd, float* z0, float* cd, int* iext) {
  flx_wu_hwang(1, "ww_flx1", igrid, npts, zwnd, u10, u10d, ust, ustd, z0, cd, iext);
}

void ww_flx2(int igrid, int npts, const float* zwind, const float* depth, const float* fp,
             const float* u, const float* udir, float* ust, float* ustd, float* z0, float* cd) {
  flx_tc(2, "ww_flx2", igrid, npts, zwind, depth, fp, u, udir, ust, ustd, z0, cd);
}

void ww_flx3(int igrid, int npts, const float* zwind, const float* depth, const float* fp,
             const float* u, const float* udir, float* ust, float* ustd, float* z0, float* cd) {
  flx_tc(3, "ww_flx3", igrid, npts, zwind, depth, fp, u, udir, ust, ustd, z0, cd);
}

void ww_flx4(int igrid, int npts, const float* zwnd, const float* u10, const float* u10d,
             float* ust, float* ustd, float* z0, float* cd, int* iext) {
  flx_wu_hwang(4, "ww_flx4", igrid, npts, zwnd, u10, u10d, ust, ustd, z0, cd, iext);
}

void ww_flx5(int igrid, int npts, const float* zwnd, const float* u10, const float* u10d,
             const float* taua, const float* tauadir, const float* rhoair, float* ust,
             float* ustd, float* z0, float* cd, float* charn, int* iext) {
  guarded("ww_flx5", [&] {
    GridCtx* g = compute_ctx(igrid, npts, 5, "ww_flx5");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    auto& R = g->rbuf;
    const auto zw_s = stage_in(ex, R, "zwnd", zwnd, npts);
    const auto u_s = stage_in(ex, R, "u10", u10, npts);
    const auto ud_s = stage_in(ex, R, "u10d", u10d, npts);
    const auto ta_s = stage_in(ex, R, "taua", taua, npts);
    const auto td_s = stage_in(ex, R, "tauadir", tauadir, npts);
    const auto ra_s = stage_in(ex, R, "rhoair", rhoair, npts);
    const auto us_s = stage_in(ex, R, "ust", ust, npts);
    const auto usd_s = stage_in(ex, R, "ustd", ustd, npts);
    const auto z0_s = stage_in(ex, R, "z0", z0, npts);
    const auto cd_s = stage_in(ex, R, "cd", cd, npts);
    const auto ch_s = stage_in(ex, R, "charn", charn, npts);
    const auto ie_s = stage_in(ex, g->ibuf, "iext", iext, npts);
    const auto ZW = zw_s.all, U = u_s.all, UD = ud_s.all, TA = ta_s.all, TD = td_s.all,
               RA = ra_s.all, US = us_s.all, USD = usd_s.all, Z0 = z0_s.all, CD = cd_s.all,
               CH = ch_s.all;
    const auto IE = ie_s.all;
    ex.for_points("ww_flx.flx5", npts, KOKKOS_LAMBDA(const int ip) {
      IE(0, ip) = ww::srce::flx::w3flx5_point(ZW(0, ip), U(0, ip), UD(0, ip), TA(0, ip),
                                              TD(0, ip), RA(0, ip), US(0, ip), USD(0, ip),
                                              Z0(0, ip), CD(0, ip), CH(0, ip));
    });
    stage_out(ex, us_s, npts);
    stage_out(ex, usd_s, npts);
    stage_out(ex, z0_s, npts);
    stage_out(ex, cd_s, npts);
    stage_out(ex, ch_s, npts);
    stage_out(ex, ie_s, npts);
    g_count[5].fetch_add(npts, std::memory_order_relaxed);
    ok();
  });
}

int ww_flx_enabled(int which) {
  const Flags& f = flags();
  if (which >= 1 && which <= 5) return f.flx[which] ? 1 : 0;
  if (which == 6) return f.tables ? 1 : 0;
  return 0;
}

int ww_flx_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
