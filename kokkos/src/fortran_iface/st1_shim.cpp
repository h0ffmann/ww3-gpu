// kokkos/src/fortran_iface/st1_shim.cpp
// The bind(C) shim of ST1: the C ABI of ww_kokkos_c.hpp ("ST1") over the
// per-point device functions of ww_kokkos/srce_st1_*.hpp (W3SPR1, W3SIN1,
// W3SDS1 of w3src1md.F90). Marshalling only: the arithmetic is in the kernels.
//
// The rules are st4_shim.cpp's: one context per WW3 grid (IMOD at W3INIT,
// IGRID at W3SRCE); nothing throws across the boundary; Views die inside
// Kokkos::finalize (a finalize hook), not at process exit; inside an OpenMP
// parallel region (W3SRCE under W3_OMPG) the kernels run on Kokkos::Serial.
// Phase 1 copies in, runs, copies out on every call, and the caller serialises
// the calls (!$OMP CRITICAL in the caller patch).
//
// ST1 has no tables of its own: the kernels read W3GDATMD's SIG, DDEN, SIG2,
// ECOS, ESIN and the scalars FTE, FTF, FTWN, SINC1, SDSC1, registered once per
// grid. Each array is registered whole, with its LBOUND, and the shim keeps
// elements 1..n (SIG is SIG(0:MK+1), w3gdatmd.F90:2066; ECOS and ESIN hold
// MSPEC+MTH).

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

#include "ww_kokkos/real.hpp"
#include "ww_kokkos/srce_params.hpp"
#include "ww_kokkos/srce_st1_sds1.hpp"
#include "ww_kokkos/srce_st1_sin1.hpp"
#include "ww_kokkos/srce_st1_spr1.hpp"

#if defined(WW_SRCE_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::srce::st1 {
namespace {

using DeviceSpace = Kokkos::DefaultExecutionSpace::memory_space;
template <class T>
using Borrowed2D = Kokkos::View<T**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
using DevReal1D = Kokkos::View<Real*, DeviceSpace>;
using DevReal2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
using HostReal1D = Kokkos::View<Real*, Kokkos::HostSpace>;

struct GridCtx {
  Params p{};  ///< nk, nth, nspec; the kernels read nothing else of it
  Real fte = 0, ftf = 0, ftwn = 0, sinc1 = 0, sdsc1 = 0;
  HostReal1D h[WW_ST1_NARRAYS];  ///< registered arrays, elements 1..n
  DevReal1D d[WW_ST1_NARRAYS];   ///< the same, on the device (after commit)
  std::map<std::string, DevReal2D> buf;  ///< batch buffers, grown, never shrunk
  bool configured = false;  ///< ww_st1_init done
  bool ready = false;       ///< ww_st1_commit done
};

std::map<int, GridCtx>& grids() {
  static std::map<int, GridCtx> m;
  return m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_hook_pushed = false;
std::atomic<long long> g_count_spr1{0}, g_count_sin1{0}, g_count_sds1{0};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: ST1: %s (code %d)\n", what, code);
}
void ok() { g_last_error.store(WW_KOKKOS_OK, std::memory_order_relaxed); }

void release() { grids().clear(); }

bool env_is_one(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && std::strcmp(v, "1") == 0;
}

struct Flags {
  bool spr1, sin1, sds1, count;
};
const Flags& flags() {
  static const Flags f{(ww_kokkos_flag("WW_KOKKOS_SPR1") != 0), (ww_kokkos_flag("WW_KOKKOS_SIN1") != 0),
                       (ww_kokkos_flag("WW_KOKKOS_SDS1") != 0), env_is_one("WW_KOKKOS_ST1_COUNT")};
  return f;
}

void print_counts() {
  std::fprintf(stderr, "ww_kokkos: W3SPR1 spectra computed: %lld\n", g_count_spr1.load());
  std::fprintf(stderr, "ww_kokkos: W3SIN1 spectra computed: %lld\n", g_count_sin1.load());
  std::fprintf(stderr, "ww_kokkos: W3SDS1 spectra computed: %lld\n", g_count_sds1.load());
}

bool nested_in_openmp() {
#if defined(WW_SRCE_SERIAL_NESTED)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

const char* array_name(int id) {
  switch (id) {
    case WW_ST1_A_SIG: return "SIG";
    case WW_ST1_A_DDEN: return "DDEN";
    case WW_ST1_A_SIG2: return "SIG2";
    case WW_ST1_A_ECOS: return "ECOS";
    case WW_ST1_A_ESIN: return "ESIN";
    default: return "?";
  }
}

/// Elements 1..n of array `id` are what the kernels read: NK of SIG(1:NK) and
/// DDEN(1:NK), NSPEC of SIG2, ECOS, ESIN (w3src1md.F90:158, :329).
int needed(const Params& p, int id) {
  return (id == WW_ST1_A_SIG || id == WW_ST1_A_DDEN) ? p.nk : p.nspec;
}

/// A (rows, >= npts) device buffer named `name`, on Serial when nested.
DevReal2D& buffer(GridCtx& g, bool nested, const std::string& name, int rows, int npts) {
  DevReal2D& v = g.buf[name];
  if (v.extent(0) != static_cast<size_t>(rows) || v.extent(1) < static_cast<size_t>(npts)) {
    v = DevReal2D();
#if defined(WW_SRCE_SERIAL_NESTED)
    if (nested) {
      v = DevReal2D(Kokkos::view_alloc(Kokkos::Serial(), "ww_st1." + name), rows, npts);
      return v;
    }
#endif
    v = DevReal2D(Kokkos::view_alloc("ww_st1." + name), rows, npts);
  }
  return v;
}

/// Runs the batch on the right execution space and fences.
struct Exec {
  bool nested;
  template <class F>
  void for_points(const char* name, int npts, const F& f) const {
#if defined(WW_SRCE_SERIAL_NESTED)
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
#if defined(WW_SRCE_SERIAL_NESTED)
    if (nested) {
      Kokkos::deep_copy(Kokkos::Serial(), dst, src);
      return;
    }
#endif
    Kokkos::deep_copy(dst, src);
  }
};

/// Caller array (rows, npts) -> the first npts columns of a device buffer, and back.
template <class T>
struct Staged {
  DevReal2D all;  ///< (rows, capacity)
  Borrowed2D<T> host;
  auto cols(int npts) const { return Kokkos::subview(all, Kokkos::ALL, Kokkos::make_pair(0, npts)); }
};

template <class T>
Staged<T> stage(const Exec& ex, GridCtx& g, const char* name, T* p, int rows, int npts,
                bool copy_in) {
  Staged<T> s{buffer(g, ex.nested, name, rows, npts), Borrowed2D<T>(p, rows, npts)};
  if (copy_in) ex.copy(s.cols(npts), s.host);
  return s;
}

template <class T>
void stage_out(const Exec& ex, const Staged<T>& s, int npts) {
  ex.copy(s.host, s.cols(npts));
}

/// The common checks of a compute entry point. Returns the grid, or nullptr
/// after recording the error.
GridCtx* compute_ctx(int igrid, int npts, const char* who) {
  const auto it = grids().find(igrid);
  if (it == grids().end() || !it->second.ready || !Kokkos::is_initialized()) {
    set_error(WW_KOKKOS_ERR_NOT_INITIALISED,
              (std::string(who) + " before ww_st1_commit for this grid").c_str());
    return nullptr;
  }
  if (npts < 0) {
    set_error(WW_KOKKOS_ERR_BAD_SHAPE, (std::string(who) + ": npts is negative").c_str());
    return nullptr;
  }
  return &it->second;
}

/// Column `ip` of a LayoutLeft (rows, capacity) buffer.
KOKKOS_INLINE_FUNCTION Real* col(const DevReal2D& v, int ip) {
  return v.data() + static_cast<size_t>(ip) * v.extent(0);
}

}  // namespace
}  // namespace ww::srce::st1

using namespace ww::srce::st1;
using ww::srce::Params;
using ww::Real;

extern "C" {

int ww_st1_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st1_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != WW_ST1_NRPAR || nipar != WW_ST1_NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st1_init: RPAR/IPAR length differs from ww_kokkos_c.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    GridCtx g;
    g.p.nk = ipar[WW_ST1_I_NK];
    g.p.nth = ipar[WW_ST1_I_NTH];
    g.p.nspec = ipar[WW_ST1_I_NSPEC];
    if (g.p.nk <= 0 || g.p.nth <= 0 || g.p.nspec != g.p.nk * g.p.nth) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st1_init: NK, NTH, NSPEC inconsistent");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    g.fte = rpar[WW_ST1_R_FTE];
    g.ftf = rpar[WW_ST1_R_FTF];
    g.ftwn = rpar[WW_ST1_R_FTWN];
    g.sinc1 = rpar[WW_ST1_R_SINC1];
    g.sdsc1 = rpar[WW_ST1_R_SDSC1];
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
    g.configured = true;
    grids()[igrid] = std::move(g);
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st1_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st1_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st1_set_real before ww_st1_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (id < 0 || id >= WW_ST1_NARRAYS || data == nullptr || lbound == nullptr ||
        extent == nullptr || rank != 1) {
      set_error(WW_KOKKOS_ERR_TABLE, "ww_st1_set_real: bad array id, rank or null pointer");
      return WW_KOKKOS_ERR_TABLE;
    }
    GridCtx& g = it->second;
    const int n = needed(g.p, id);
    const long first = 1L - lbound[0];  // offset of element 1 in the caller's array
    if (first < 0 || static_cast<long>(extent[0]) - first < n) {
      set_error(WW_KOKKOS_ERR_TABLE,
                (std::string("ww_st1_set_real: LBOUND or extent of ") + array_name(id)).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
    HostReal1D h(std::string("ww_st1.") + array_name(id), static_cast<size_t>(n));
    std::memcpy(h.data(), data + first, sizeof(Real) * static_cast<size_t>(n));
    g.h[id] = h;
    g.ready = false;
    ok();
    return WW_KOKKOS_OK;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st1_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st1_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st1_commit before ww_st1_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    for (int id = 0; id < WW_ST1_NARRAYS; ++id) {
      if (g.h[id].data() == nullptr) {
        set_error(WW_KOKKOS_ERR_TABLE,
                  (std::string("ww_st1_commit: ") + array_name(id) + " not registered").c_str());
        return WW_KOKKOS_ERR_TABLE;
      }
    }
    for (int id = 0; id < WW_ST1_NARRAYS; ++id)
      g.d[id] = Kokkos::create_mirror_view_and_copy(DeviceSpace(), g.h[id]);
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st1_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_spr1(int igrid, int npts, const float* a, const float* cg, const float* wn,
             float* emean, float* fmean, float* wnmean, float* amax) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_spr1");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int nk = g->p.nk, ns = g->p.nspec;
    const auto a_s = stage(ex, *g, "spr1.a", a, ns, npts, true);
    const auto cg_s = stage(ex, *g, "spr1.cg", cg, nk, npts, true);
    const auto wn_s = stage(ex, *g, "spr1.wn", wn, nk, npts, true);
    // OUT in the Fortran and assigned on every path (w3src1md.F90:189-224).
    const auto em_s = stage(ex, *g, "spr1.emean", emean, 1, npts, false);
    const auto fm_s = stage(ex, *g, "spr1.fmean", fmean, 1, npts, false);
    const auto wm_s = stage(ex, *g, "spr1.wnmean", wnmean, 1, npts, false);
    const auto am_s = stage(ex, *g, "spr1.amax", amax, 1, npts, false);
    const DevReal2D eb = buffer(*g, ex.nested, "spr1.eb", nk, npts);
    const Params p = g->p;
    const Real fte = g->fte, ftf = g->ftf, ftwn = g->ftwn;
    const DevReal1D sig = g->d[WW_ST1_A_SIG], dden = g->d[WW_ST1_A_DDEN];
    const DevReal2D A = a_s.all, CG = cg_s.all, WN = wn_s.all, EM = em_s.all, FM = fm_s.all,
                    WM = wm_s.all, AM = am_s.all;
    ex.for_points("ww_st1.spr1", npts, KOKKOS_LAMBDA(const int ip) {
      w3spr1_point(p, col(A, ip), col(CG, ip), col(WN, ip), EM(0, ip), FM(0, ip), WM(0, ip),
                   AM(0, ip), sig.data(), dden.data(), fte, ftf, ftwn, col(eb, ip));
    });
    stage_out(ex, em_s, npts);
    stage_out(ex, fm_s, npts);
    stage_out(ex, wm_s, npts);
    stage_out(ex, am_s, npts);
    g_count_spr1.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_spr1: unknown exception");
  }
}

void ww_sin1(int igrid, int npts, const float* a, const float* k, const float* ustar,
             const float* usdir, float* s, float* d) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sin1");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int ns = g->p.nspec;
    const auto a_s = stage(ex, *g, "sin1.a", a, ns, npts, true);
    const auto k_s = stage(ex, *g, "sin1.k", k, ns, npts, true);
    const auto us_s = stage(ex, *g, "sin1.ustar", ustar, 1, npts, true);
    const auto ud_s = stage(ex, *g, "sin1.usdir", usdir, 1, npts, true);
    // OUT, every element assigned (w3src1md.F90:383-389).
    const auto s_s = stage(ex, *g, "sin1.s", s, ns, npts, false);
    const auto d_s = stage(ex, *g, "sin1.d", d, ns, npts, false);
    const Params p = g->p;
    const Real sinc1 = g->sinc1;
    const DevReal1D sig2 = g->d[WW_ST1_A_SIG2], ecos = g->d[WW_ST1_A_ECOS],
                    esin = g->d[WW_ST1_A_ESIN];
    const DevReal2D A = a_s.all, K = k_s.all, US = us_s.all, UD = ud_s.all, S = s_s.all,
                    D = d_s.all;
    ex.for_points("ww_st1.sin1", npts, KOKKOS_LAMBDA(const int ip) {
      w3sin1_point(col(A, ip), col(K, ip), US(0, ip), UD(0, ip), col(S, ip), col(D, ip), p,
                   sinc1, sig2.data(), ecos.data(), esin.data());
    });
    stage_out(ex, s_s, npts);
    stage_out(ex, d_s, npts);
    g_count_sin1.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sin1: unknown exception");
  }
}

void ww_sds1(int igrid, int npts, const float* a, const float* k, const float* emean,
             const float* fmean, const float* wnmean, float* s, float* d) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sds1");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    const int ns = g->p.nspec;
    const auto a_s = stage(ex, *g, "sds1.a", a, ns, npts, true);
    const auto k_s = stage(ex, *g, "sds1.k", k, ns, npts, true);
    const auto em_s = stage(ex, *g, "sds1.emean", emean, 1, npts, true);
    const auto fm_s = stage(ex, *g, "sds1.fmean", fmean, 1, npts, true);
    const auto wm_s = stage(ex, *g, "sds1.wnmean", wnmean, 1, npts, true);
    // OUT, every element assigned (w3src1md.F90:557-558).
    const auto s_s = stage(ex, *g, "sds1.s", s, ns, npts, false);
    const auto d_s = stage(ex, *g, "sds1.d", d, ns, npts, false);
    const Params p = g->p;
    const Real sdsc1 = g->sdsc1;
    const DevReal2D A = a_s.all, K = k_s.all, EM = em_s.all, FM = fm_s.all, WM = wm_s.all,
                    S = s_s.all, D = d_s.all;
    ex.for_points("ww_st1.sds1", npts, KOKKOS_LAMBDA(const int ip) {
      w3sds1_point(col(A, ip), col(K, ip), EM(0, ip), FM(0, ip), WM(0, ip), col(S, ip),
                   col(D, ip), p, sdsc1);
    });
    stage_out(ex, s_s, npts);
    stage_out(ex, d_s, npts);
    g_count_sds1.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sds1: unknown exception");
  }
}

int ww_st1_enabled(int which) {
  const Flags& f = flags();
  switch (which) {
    case 1: return f.spr1 ? 1 : 0;
    case 2: return f.sin1 ? 1 : 0;
    case 3: return f.sds1 ? 1 : 0;
    default: return 0;
  }
}

int ww_st1_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
