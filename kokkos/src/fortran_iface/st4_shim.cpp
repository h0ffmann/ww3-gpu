// kokkos/src/fortran_iface/st4_shim.cpp
// The bind(C) shim of ST4: the C ABI of ww_kokkos_c.hpp ("ST4") over the
// per-point device functions of ww_kokkos/st4_*.hpp. Marshalling only: the
// arithmetic is in the kernels, the parameters and arrays in st4_params.hpp and
// st4_arrays.hpp.
//
// The rules are snl1_shim.cpp's: one context per WW3 grid (IMOD at W3INIT,
// IGRID at W3SRCE); nothing throws across the boundary; Views die inside
// Kokkos::finalize (a finalize hook), not at process exit; inside an OpenMP
// parallel region (W3SRCE under W3_OMPG) the kernels run on Kokkos::Serial.
// Phase 1 copies in, runs, copies out on every call, and the caller serialises
// the calls (!$OMP CRITICAL in the caller patch).
//

#include "fortran_iface/ww_kokkos_c.hpp"
#include "fortran_iface/shim_state.hpp"
#include "fortran_iface/timers.hpp"

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
#include "ww_kokkos/st4_arrays.hpp"
#include "ww_kokkos/st4_params.hpp"
#include "ww_kokkos/st4_sds4.hpp"
#include "ww_kokkos/st4_sin4.hpp"
#include "ww_kokkos/st4_spr4.hpp"
#include "ww_kokkos/st4_tables_build.hpp"

#if defined(WW_ST4_SERIAL_NESTED)
#include <omp.h>
#endif

namespace ww::st4 {
namespace {

template <class T>
using Borrowed2D = Kokkos::View<T**, Kokkos::LayoutLeft, Kokkos::HostSpace,
                                Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
using DevReal2D = Kokkos::View<Real**, Kokkos::LayoutLeft, DeviceSpace>;
using DevInt2D = Kokkos::View<int**, Kokkos::LayoutLeft, DeviceSpace>;

/// Device batch buffers, by name, each (rows, capacity). Grown, never shrunk.
struct Buffers {
  std::map<std::string, DevReal2D> real;
  std::map<std::string, DevInt2D> ints;
};

struct GridCtx {
  Params p{};
  HostArrays h;          ///< the registered grid arrays, on the host
  Fields d;              ///< the same, on the device (after commit)
  Buffers buf;
  bool configured = false;  ///< ww_st4_init done
  bool ready = false;       ///< ww_st4_commit done
};

/// W3SRC4MD's and CONSTANTS' module state: one per process, last writer wins.
struct ModuleCtx {
  HostArrays h;  ///< taut, tauhft, tauhft2, fwtable
  Fields d;
  Real delust = 0, delalp = 0, deltauw = 0, delu = 0, deltail = 0, delab = 0;
};

// Never destroyed: ww_kokkos_finalize runs from atexit, after the static destructors of
// objects built later, so a static map would be gone when its finalize hook clears it.
std::map<int, GridCtx>& grids() {
  static auto* m = new std::map<int, GridCtx>();
  return *m;
}
// Never destroyed, as grids().
ModuleCtx& module_ctx() {
  static auto* m = new ModuleCtx();
  return *m;
}

std::atomic<int> g_last_error{WW_KOKKOS_OK};
bool g_hook_pushed = false;
std::atomic<long long> g_count_sds4{0}, g_count_sin4{0}, g_count_spr4{0};

void set_error(int code, const char* what) {
  g_last_error.store(code, std::memory_order_relaxed);
  std::fprintf(stderr, "ww_kokkos: ST4: %s (code %d)\n", what, code);
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
  bool sds4, sin4, spr4, tables, count;
};
const Flags& flags() {
  static const Flags f{(ww_kokkos_flag("WW_KOKKOS_SDS4") != 0), (ww_kokkos_flag("WW_KOKKOS_SIN4") != 0),
                       (ww_kokkos_flag("WW_KOKKOS_SPR4") != 0), (ww_kokkos_flag("WW_KOKKOS_ST4_TABLES") != 0),
                       env_is_one("WW_KOKKOS_ST4_COUNT")};
  return f;
}

void print_counts() {
  std::fprintf(stderr, "ww_kokkos: W3SDS4 spectra computed: %lld\n", g_count_sds4.load());
  std::fprintf(stderr, "ww_kokkos: W3SIN4 spectra computed: %lld\n", g_count_sin4.load());
  std::fprintf(stderr, "ww_kokkos: W3SPR4 spectra computed: %lld\n", g_count_spr4.load());
}

bool nested_in_openmp() {
#if defined(WW_ST4_SERIAL_NESTED)
  return omp_get_level() > 0;
#else
  return false;
#endif
}

/// Host View of a registered array, copied from the caller's memory.
template <class T, class V>
V copy_registered(const char* name, const T* data, const int* extent) {
  const std::string label = std::string("ww_st4.") + name;
  if constexpr (V::rank() == 1) {
    V v(label, extent[0]);
    std::memcpy(v.data(), data, sizeof(T) * static_cast<size_t>(extent[0]));
    return v;
  } else if constexpr (V::rank() == 2) {
    V v(label, extent[0], extent[1]);
    std::memcpy(v.data(), data, sizeof(T) * static_cast<size_t>(extent[0]) * extent[1]);
    return v;
  } else {
    V v(label, extent[0], extent[1], extent[2]);
    std::memcpy(v.data(), data,
                sizeof(T) * static_cast<size_t>(extent[0]) * extent[1] * extent[2]);
    return v;
  }
}

/// The member of ArraysT that `id` names, called with that member.
template <class A, class F>
void with_member(A& a, int id, F&& f) {
  switch (id) {
    case A_SIG: f(a.sig); break;
    case A_SIG2: f(a.sig2); break;
    case A_DSIP: f(a.dsip); break;
    case A_DDEN: f(a.dden); break;
    case A_DDEN2: f(a.dden2); break;
    case A_ESIN: f(a.esin); break;
    case A_ECOS: f(a.ecos); break;
    case A_TH: f(a.th); break;
    case A_SATWEIGHTS: f(a.satweights); break;
    case A_CUMULW: f(a.cumulw); break;
    case A_DCKI: f(a.dcki); break;
    case A_QBI: f(a.qbi); break;
    case A_SATINDICES: f(a.satindices); break;
    case A_IKTAB: f(a.iktab); break;
    case A_TAUT: f(a.taut); break;
    case A_TAUHFT: f(a.tauhft); break;
    case A_TAUHFT2: f(a.tauhft2); break;
    case A_FWTABLE: f(a.fwtable); break;
    default: break;
  }
}

template <class T>
int set_array(int igrid, int id, const T* data, int rank, const int* lbound,
              const int* extent) {
  constexpr bool is_int = std::is_same_v<T, int>;
  const auto it = grids().find(igrid);
  if (it == grids().end() || !it->second.configured) {
    set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st4_set_* before ww_st4_init for this grid");
    return WW_KOKKOS_ERR_NOT_INITIALISED;
  }
  if (id < 0 || id >= NARRAYS || data == nullptr || lbound == nullptr || extent == nullptr) {
    set_error(WW_KOKKOS_ERR_TABLE, "ww_st4_set_*: bad array id or null pointer");
    return WW_KOKKOS_ERR_TABLE;
  }
  const ArraySpec& spec = kArraySpecs[id];
  if (spec.is_int != is_int || spec.rank != rank) {
    set_error(WW_KOKKOS_ERR_TABLE, (std::string("ww_st4_set_*: type or rank of ") + spec.name).c_str());
    return WW_KOKKOS_ERR_TABLE;
  }
  for (int r = 0; r < rank; ++r) {
    if (lbound[r] != spec.lbound || extent[r] <= 0) {
      set_error(WW_KOKKOS_ERR_TABLE, (std::string("ww_st4_set_*: LBOUND or extent of ") + spec.name).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
  }
  HostArrays& dst = spec.module ? module_ctx().h : it->second.h;
  with_member(dst, id, [&](auto& member) {
    using V = std::decay_t<decltype(member)>;
    if constexpr (std::is_same_v<typename V::value_type, T>) {
      member = copy_registered<T, V>(spec.name, data, extent);
    }
  });
  it->second.ready = false;
  return WW_KOKKOS_OK;
}

/// The extents ST4 needs of each array, given the grid's scalars. A registered
/// array may be larger (W3DIMX allocates to MK, MTH, MSPEC); never smaller.
bool check_arrays(const Params& p, const HostArrays& g, const HostArrays& m, std::string& why) {
  const int sdsnth = p.mth / 2 - 1;
  auto need = [&](const auto& v, const char* name, size_t e0, size_t e1 = 0, size_t e2 = 0,
                  bool exact = false) {
    if (v.data() == nullptr) {
      why = std::string(name) + " not registered";
      return false;
    }
    const size_t e[3] = {e0, e1, e2};
    for (unsigned r = 0; r < v.rank(); ++r) {
      if (exact ? v.extent(r) != e[r] : v.extent(r) < e[r]) {
        why = std::string(name) + " has the wrong shape";
        return false;
      }
    }
    return true;
  };
  const size_t nk = p.nk, nth = p.nth, nspec = p.nspec;
  bool okk = need(g.sig, "SIG", nk + 2) && need(g.sig2, "SIG2", nspec) &&
             need(g.dsip, "DSIP", nk + 2) && need(g.dden, "DDEN", nk) &&
             need(g.dden2, "DDEN2", nspec) && need(g.esin, "ESIN", nspec + nth) &&
             need(g.ecos, "ECOS", nspec + nth) && need(g.th, "TH", nth) &&
             need(g.satweights, "SATWEIGHTS", 2 * sdsnth + 1, p.mth, 0, true) &&
             need(g.satindices, "SATINDICES", 2 * sdsnth + 1, p.mth, 0, true) &&
             need(g.cumulw, "CUMULW", p.mspec, p.mspec, 0, true) &&
             need(g.dcki, "DCKI", kNkhs, kNkd, 0, true) &&
             need(g.qbi, "QBI", kNkhs, kNkd, 0, true) &&
             need(g.iktab, "IKTAB", p.mk, kNdtab, 0, true) &&
             need(m.fwtable, "FWTABLE", kSizeFwTable + 1, 0, 0, true);
  if (okk && p.sintailpar[0] > static_cast<Real>(0.5)) {
    okk = need(m.taut, "TAUT", kItauMax + 1, kJuMax + 1, 0, true) &&
          need(m.tauhft, "TAUHFT", kIustar + 1, kIalpha + 1, 0, true);
    if (okk && p.ttauwshelter > 0)
      okk = need(m.tauhft2, "TAUHFT2", kIustar + 1, kIalpha + 1, kIlevTail + 1, true);
  }
  return okk;
}

template <class V>
auto to_device(const V& h) {
  return Kokkos::create_mirror_view_and_copy(DeviceSpace(), h);
}

void upload(const HostArrays& h, Fields& d, bool module) {
  if (module) {
    d.taut = to_device(h.taut);
    d.tauhft = to_device(h.tauhft);
    d.tauhft2 = to_device(h.tauhft2);
    d.fwtable = to_device(h.fwtable);
    return;
  }
  d.sig = to_device(h.sig);
  d.sig2 = to_device(h.sig2);
  d.dsip = to_device(h.dsip);
  d.dden = to_device(h.dden);
  d.dden2 = to_device(h.dden2);
  d.esin = to_device(h.esin);
  d.ecos = to_device(h.ecos);
  d.th = to_device(h.th);
  d.satweights = to_device(h.satweights);
  d.cumulw = to_device(h.cumulw);
  d.dcki = to_device(h.dcki);
  d.qbi = to_device(h.qbi);
  d.satindices = to_device(h.satindices);
  d.iktab = to_device(h.iktab);
}

/// Values of `b` that are not bitwise equal to `a`'s (both host, same shape).
template <class V>
long long count_diff(const V& a, const V& b) {
  if (a.data() == nullptr || b.data() == nullptr || a.size() != b.size()) return a.size() + b.size();
  long long n = 0;
  for (size_t i = 0; i < a.size(); ++i)
    if (std::memcmp(a.data() + i, b.data() + i, sizeof(*a.data())) != 0) ++n;
  return n;
}

/// WW_KOKKOS_ST4_TABLES=1: rebuild the grid's and the module's tables with the
/// C++ INSIN4(FLTABS=.TRUE.), report how many values differ from the Fortran's,
/// and use the C++ ones.
void rebuild_tables(GridCtx& g, ModuleCtx& m) {
  Params p = g.p;
  HostArrays built = g.h;  // the grid arrays INSIN4 reads (SIG, DSIP, TH, ECOS, ...)
  allocate_tables(p, built);
  built.fwtable = m.h.fwtable;
  insin4(p, true, built);
  // TAUT/TAUHFT exist only when SINTAILPAR(1) > 0.5, TAUHFT2 only when also
  // TTAUWSHELTER > 0 (w3src4md.F90:1118-1127); otherwise the module tables and
  // DEL* of an earlier grid stay, as the Fortran's ALLOCATED module arrays do.
  const bool tabs = p.sintailpar[0] > static_cast<Real>(0.5);
  const bool tabs2 = tabs && p.ttauwshelter > 0;
  long long diff = count_diff(g.h.satindices, built.satindices) +
                   count_diff(g.h.satweights, built.satweights) +
                   count_diff(g.h.iktab, built.iktab) + count_diff(g.h.dcki, built.dcki) +
                   count_diff(g.h.qbi, built.qbi) + count_diff(g.h.cumulw, built.cumulw);
  if (tabs) diff += count_diff(m.h.taut, built.taut) + count_diff(m.h.tauhft, built.tauhft);
  if (tabs2) diff += count_diff(m.h.tauhft2, built.tauhft2);
  // A difference in TAUHFT/TAUHFT2 can also mean ww3_grid read a stale
  // ST4TABUHF2.bin (w3src4md.F90:1710-1725), which the C++ never reads.
  std::fprintf(stderr,
               "ww_kokkos: ST4 tables rebuilt in C++: %lld values differ from the Fortran's "
               "(mod_def.ww3, ST4TABUHF2.bin)\n",
               diff);
  g.h.satindices = built.satindices;
  g.h.satweights = built.satweights;
  g.h.iktab = built.iktab;
  g.h.dcki = built.dcki;
  g.h.qbi = built.qbi;
  g.h.cumulw = built.cumulw;
  if (tabs) {
    m.h.taut = built.taut;
    m.h.tauhft = built.tauhft;
    m.delu = p.delu;
    m.deltauw = p.deltauw;
    m.delust = p.delust;
    m.delalp = p.delalp;
  }
  if (tabs2) {
    m.h.tauhft2 = built.tauhft2;
    m.deltail = p.deltail;
  }
}

/// The grid's Params and Fields with the module's current state merged in.
void current(const GridCtx& g, Params& p, Fields& f) {
  const ModuleCtx& m = module_ctx();
  p = g.p;
  p.delust = m.delust;
  p.delalp = m.delalp;
  p.deltauw = m.deltauw;
  p.delu = m.delu;
  p.deltail = m.deltail;
  p.delab = m.delab;
  f = g.d;
  f.taut = m.d.taut;
  f.tauhft = m.d.tauhft;
  f.tauhft2 = m.d.tauhft2;
  f.fwtable = m.d.fwtable;
}

/// A (rows, >= npts) device buffer named `name` of this grid; work buffers may
/// have rows == 0. Zero-filled on allocation, on Serial when nested.
template <class V, class... Space>
V& buffer(std::map<std::string, V>& pool, const std::string& name, int rows, int npts,
          const Space&... space) {
  V& v = pool[name];
  if (v.extent(0) != static_cast<size_t>(rows) || v.extent(1) < static_cast<size_t>(npts)) {
    v = V();
    v = V(Kokkos::view_alloc(space..., "ww_st4." + name), rows, npts);
  }
  return v;
}

/// Runs the batch on the right execution space and fences.
struct Exec {
  bool nested;
  template <class F>
  void for_points(const char* name, int npts, const F& f) const {
#if defined(WW_ST4_SERIAL_NESTED)
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
#if defined(WW_ST4_SERIAL_NESTED)
    if (nested) {
      Kokkos::deep_copy(Kokkos::Serial(), dst, src);
      return;
    }
#endif
    Kokkos::deep_copy(dst, src);
  }
  template <class V>
  V& buf(std::map<std::string, V>& pool, const std::string& name, int rows, int npts) const {
#if defined(WW_ST4_SERIAL_NESTED)
    if (nested) return buffer(pool, name, rows, npts, Kokkos::Serial());
#endif
    return buffer(pool, name, rows, npts);
  }
};

/// Caller array (rows, npts) -> the first npts columns of a device buffer, and back.
template <class T>
struct Staged {
  Kokkos::View<std::remove_const_t<T>**, Kokkos::LayoutLeft, DeviceSpace> all;  ///< (rows, capacity)
  Borrowed2D<T> host;

  auto cols(int npts) const { return Kokkos::subview(all, Kokkos::ALL, Kokkos::make_pair(0, npts)); }
};

template <class T, class V>
Staged<T> stage_in(const Exec& ex, std::map<std::string, V>& pool, const char* name, T* p,
                   int rows, int npts) {
  Staged<T> s{ex.buf(pool, name, rows, npts), Borrowed2D<T>(p, rows, npts)};
  ex.copy(s.cols(npts), s.host);
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
              (std::string(who) + " before ww_st4_commit for this grid").c_str());
    return nullptr;
  }
  if (npts < 0) {
    set_error(WW_KOKKOS_ERR_BAD_SHAPE, (std::string(who) + ": npts is negative").c_str());
    return nullptr;
  }
  return &it->second;
}

/// Column `ip` of a LayoutLeft (rows, capacity) buffer, by pointer arithmetic so
/// a zero-row work buffer is legal.
template <class V>
KOKKOS_INLINE_FUNCTION auto col(const V& v, int ip) {
  return v.data() + static_cast<size_t>(ip) * v.extent(0);
}

}  // namespace

bool grid_state(int igrid, Params& p, Fields& f) {
  const auto it = grids().find(igrid);
  if (it == grids().end() || !it->second.ready) return false;
  current(it->second, p, f);
  return true;
}

}  // namespace ww::st4

using namespace ww::st4;
using ww::Real;

extern "C" {

int ww_st4_init(int igrid, const float* rpar, int nrpar, const int* ipar, int nipar) {
  try {
    grids().erase(igrid);
    if (!Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st4_init before ww_kokkos_init");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    if (rpar == nullptr || ipar == nullptr || nrpar != NRPAR || nipar != NIPAR) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st4_init: RPAR/IPAR length differs from st4_params.hpp");
      return WW_KOKKOS_ERR_BAD_SHAPE;
    }
    if (ipar[I_FLX5] != 0) {
      set_error(WW_KOKKOS_ERR_UNSUPPORTED, "ww_st4_init: W3_FLX5 is not supported by the Kokkos ST4");
      return WW_KOKKOS_ERR_UNSUPPORTED;
    }
    const Params p = unpack(rpar, ipar);
    if (p.nk <= 0 || p.nth <= 0 || p.nspec != p.nk * p.nth || p.mk < p.nk || p.mth < p.nth ||
        p.mspec < p.nspec) {
      set_error(WW_KOKKOS_ERR_BAD_SHAPE, "ww_st4_init: NK, NTH, NSPEC, MK, MTH, MSPEC inconsistent");
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
    GridCtx& g = grids()[igrid];
    g.p = p;
    g.configured = true;
    ModuleCtx& m = module_ctx();  // module scalars: the last grid to initialise wins
    m.delust = p.delust;
    m.delalp = p.delalp;
    m.deltauw = p.deltauw;
    m.delu = p.delu;
    m.deltail = p.deltail;
    m.delab = p.delab;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    grids().erase(igrid);
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st4_init: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st4_set_real(int igrid, int id, const float* data, int rank, const int* lbound,
                    const int* extent) {
  try {
    const int rc = set_array<Real>(igrid, id, data, rank, lbound, extent);
    if (rc == WW_KOKKOS_OK) ok();
    return rc;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st4_set_real: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st4_set_int(int igrid, int id, const int* data, int rank, const int* lbound,
                   const int* extent) {
  try {
    const int rc = set_array<int>(igrid, id, data, rank, lbound, extent);
    if (rc == WW_KOKKOS_OK) ok();
    return rc;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st4_set_int: exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

int ww_st4_commit(int igrid) {
  try {
    const auto it = grids().find(igrid);
    if (it == grids().end() || !it->second.configured || !Kokkos::is_initialized()) {
      set_error(WW_KOKKOS_ERR_NOT_INITIALISED, "ww_st4_commit before ww_st4_init for this grid");
      return WW_KOKKOS_ERR_NOT_INITIALISED;
    }
    GridCtx& g = it->second;
    ModuleCtx& m = module_ctx();
    std::string why;
    if (!check_arrays(g.p, g.h, m.h, why)) {
      set_error(WW_KOKKOS_ERR_TABLE, ("ww_st4_commit: " + why).c_str());
      return WW_KOKKOS_ERR_TABLE;
    }
    if (flags().tables) rebuild_tables(g, m);
    upload(g.h, g.d, false);
    upload(m.h, m.d, true);
    g.ready = true;
    ok();
    return WW_KOKKOS_OK;
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
    return WW_KOKKOS_ERR_RUNTIME;
  } catch (...) {
    set_error(WW_KOKKOS_ERR_RUNTIME, "ww_st4_commit: unknown exception");
    return WW_KOKKOS_ERR_RUNTIME;
  }
}

void ww_sds4(int igrid, int npts, const float* a, const float* k, const float* cg,
             const float* ustar, const float* usdir, const float* depth, const float* dair,
             const float* dlwmean, int flogrd57, int flogrd58, float* srhs, float* ddiag,
             float* brlambda, float* whitecap) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sds4");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    ww::timers::Region t_call("st4.sds4.call", !ex.nested);
    ww::timers::Region t_in("st4.sds4.h2d", !ex.nested);
    const int nk = g->p.nk, ns = g->p.nspec;
    auto& R = g->buf.real;
    const auto a_s = stage_in(ex, R, "sds4.a", a, ns, npts);
    const auto k_s = stage_in(ex, R, "sds4.k", k, nk, npts);
    const auto cg_s = stage_in(ex, R, "sds4.cg", cg, nk, npts);
    const auto us_s = stage_in(ex, R, "sds4.ustar", ustar, 1, npts);
    const auto ud_s = stage_in(ex, R, "sds4.usdir", usdir, 1, npts);
    const auto dp_s = stage_in(ex, R, "sds4.depth", depth, 1, npts);
    const auto da_s = stage_in(ex, R, "sds4.dair", dair, 1, npts);
    const auto dl_s = stage_in(ex, R, "sds4.dlwmean", dlwmean, 1, npts);
    const auto sr_s = stage_in(ex, R, "sds4.srhs", srhs, ns, npts);
    const auto dd_s = stage_in(ex, R, "sds4.ddiag", ddiag, ns, npts);
    const auto br_s = stage_in(ex, R, "sds4.brlambda", brlambda, ns, npts);
    const auto wc_s = stage_in(ex, R, "sds4.whitecap", whitecap, 4, npts);
    t_in.stop();
    Params p;
    Fields f;
    current(*g, p, f);
    const auto w = ex.buf(R, "sds4.work", sds4_work_reals(p), npts);
    const auto iw = ex.buf(g->buf.ints, "sds4.iwork", sds4_work_ints(p), npts);
    const auto A = a_s.all, K = k_s.all, CG = cg_s.all, US = us_s.all, UD = ud_s.all,
               DP = dp_s.all, DA = da_s.all, DL = dl_s.all, SR = sr_s.all, DD = dd_s.all,
               BR = br_s.all, WC = wc_s.all;
    ww::timers::Region t_k("st4.sds4.kernel", !ex.nested);
    ex.for_points("ww_st4.sds4", npts, KOKKOS_LAMBDA(const int ip) {
      w3sds4_point(p, f, col(A, ip), col(K, ip), col(CG, ip), US(0, ip), UD(0, ip), DP(0, ip),
                   DA(0, ip), DL(0, ip), flogrd57, flogrd58, col(SR, ip), col(DD, ip),
                   col(BR, ip), col(WC, ip), col(w, ip), col(iw, ip));
    });
    t_k.stop();
    ww::timers::Region t_out("st4.sds4.d2h", !ex.nested);
    stage_out(ex, sr_s, npts);
    stage_out(ex, dd_s, npts);
    stage_out(ex, br_s, npts);
    stage_out(ex, wc_s, npts);
    t_out.stop();
    g_count_sds4.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sds4: unknown exception");
  }
}

void ww_sin4(int igrid, int npts, const float* a, const float* cg, const float* k,
             const float* u, const float* ustar, const float* drat, const float* as,
             const float* usdir, const float* z0, const float* cd, float* tauwx, float* tauwy,
             float* tauwnx, float* tauwny, float* s, float* d, int* llws) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_sin4");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    ww::timers::Region t_call("st4.sin4.call", !ex.nested);
    ww::timers::Region t_in("st4.sin4.h2d", !ex.nested);
    const int nk = g->p.nk, ns = g->p.nspec;
    auto& R = g->buf.real;
    const auto a_s = stage_in(ex, R, "sin4.a", a, ns, npts);
    const auto cg_s = stage_in(ex, R, "sin4.cg", cg, nk, npts);
    const auto k_s = stage_in(ex, R, "sin4.k", k, ns, npts);
    const auto u_s = stage_in(ex, R, "sin4.u", u, 1, npts);
    const auto us_s = stage_in(ex, R, "sin4.ustar", ustar, 1, npts);
    const auto dr_s = stage_in(ex, R, "sin4.drat", drat, 1, npts);
    const auto as_s = stage_in(ex, R, "sin4.as", as, 1, npts);
    const auto ud_s = stage_in(ex, R, "sin4.usdir", usdir, 1, npts);
    const auto z0_s = stage_in(ex, R, "sin4.z0", z0, 1, npts);
    const auto cd_s = stage_in(ex, R, "sin4.cd", cd, 1, npts);
    const auto tx_s = stage_in(ex, R, "sin4.tauwx", tauwx, 1, npts);
    const auto ty_s = stage_in(ex, R, "sin4.tauwy", tauwy, 1, npts);
    const auto tnx_s = stage_in(ex, R, "sin4.tauwnx", tauwnx, 1, npts);
    const auto tny_s = stage_in(ex, R, "sin4.tauwny", tauwny, 1, npts);
    const auto s_s = stage_in(ex, R, "sin4.s", s, ns, npts);
    const auto d_s = stage_in(ex, R, "sin4.d", d, ns, npts);
    const auto l_s = stage_in(ex, g->buf.ints, "sin4.llws", llws, ns, npts);
    t_in.stop();
    Params p;
    Fields f;
    current(*g, p, f);
    const auto w = ex.buf(R, "sin4.work", sin4_work_reals(p), npts);
    const auto iw = ex.buf(g->buf.ints, "sin4.iwork", sin4_work_ints(p), npts);
    const auto A = a_s.all, CG = cg_s.all, K = k_s.all, U = u_s.all, US = us_s.all,
               DR = dr_s.all, AS = as_s.all, UD = ud_s.all, Z0 = z0_s.all, CD = cd_s.all,
               TX = tx_s.all, TY = ty_s.all, TNX = tnx_s.all, TNY = tny_s.all, S = s_s.all,
               D = d_s.all;
    const auto L = l_s.all;
    ww::timers::Region t_k("st4.sin4.kernel", !ex.nested);
    ex.for_points("ww_st4.sin4", npts, KOKKOS_LAMBDA(const int ip) {
      w3sin4_point(p, f, col(A, ip), col(CG, ip), col(K, ip), U(0, ip), US(0, ip), DR(0, ip),
                   AS(0, ip), UD(0, ip), Z0(0, ip), CD(0, ip), TX(0, ip), TY(0, ip), TNX(0, ip),
                   TNY(0, ip), col(S, ip), col(D, ip), col(L, ip), col(w, ip), col(iw, ip));
    });
    t_k.stop();
    ww::timers::Region t_out("st4.sin4.d2h", !ex.nested);
    stage_out(ex, tx_s, npts);
    stage_out(ex, ty_s, npts);
    stage_out(ex, tnx_s, npts);
    stage_out(ex, tny_s, npts);
    stage_out(ex, s_s, npts);
    stage_out(ex, d_s, npts);
    stage_out(ex, l_s, npts);
    t_out.stop();
    g_count_sin4.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_sin4: unknown exception");
  }
}

void ww_spr4(int igrid, int npts, const float* a, const float* cg, const float* wn,
             const float* u, const float* udir, const float* tauwx, const float* tauwy,
             const int* llws, float* ustar, float* usdir, float* emean, float* fmean,
             float* fmean1, float* wnmean, float* amax, float* cd, float* z0, float* charn,
             float* fmeanws, float* dlwmean) {
  try {
    GridCtx* g = compute_ctx(igrid, npts, "ww_spr4");
    if (g == nullptr) return;
    if (npts == 0) return ok();
    const Exec ex{nested_in_openmp()};
    ww::timers::Region t_call("st4.spr4.call", !ex.nested);
    ww::timers::Region t_in("st4.spr4.h2d", !ex.nested);
    const int nk = g->p.nk, ns = g->p.nspec;
    auto& R = g->buf.real;
    const auto a_s = stage_in(ex, R, "spr4.a", a, ns, npts);
    const auto cg_s = stage_in(ex, R, "spr4.cg", cg, nk, npts);
    const auto wn_s = stage_in(ex, R, "spr4.wn", wn, nk, npts);
    const auto u_s = stage_in(ex, R, "spr4.u", u, 1, npts);
    const auto ud_s = stage_in(ex, R, "spr4.udir", udir, 1, npts);
    const auto tx_s = stage_in(ex, R, "spr4.tauwx", tauwx, 1, npts);
    const auto ty_s = stage_in(ex, R, "spr4.tauwy", tauwy, 1, npts);
    const auto l_s = stage_in(ex, g->buf.ints, "spr4.llws", llws, ns, npts);
    // INOUT and OUT scalars, in the order of the ABI: one staged row each.
    float* outs[12] = {ustar, usdir, emean, fmean, fmean1, wnmean,
                       amax,  cd,    z0,    charn, fmeanws, dlwmean};
    static const char* names[12] = {"spr4.ustar", "spr4.usdir", "spr4.emean", "spr4.fmean",
                                    "spr4.fmean1", "spr4.wnmean", "spr4.amax", "spr4.cd",
                                    "spr4.z0", "spr4.charn", "spr4.fmeanws", "spr4.dlwmean"};
    Staged<Real> o[12];
    for (int j = 0; j < 12; ++j) o[j] = stage_in(ex, R, names[j], outs[j], 1, npts);
    t_in.stop();
    Params p;
    Fields f;
    current(*g, p, f);
    const auto w = ex.buf(R, "spr4.work", spr4_work_reals(p), npts);
    const auto iw = ex.buf(g->buf.ints, "spr4.iwork", spr4_work_ints(p), npts);
    const auto A = a_s.all, CG = cg_s.all, WN = wn_s.all, U = u_s.all, UD = ud_s.all,
               TX = tx_s.all, TY = ty_s.all;
    const auto L = l_s.all;
    const auto O0 = o[0].all, O1 = o[1].all, O2 = o[2].all, O3 = o[3].all, O4 = o[4].all,
               O5 = o[5].all, O6 = o[6].all, O7 = o[7].all, O8 = o[8].all, O9 = o[9].all,
               O10 = o[10].all, O11 = o[11].all;
    ww::timers::Region t_k("st4.spr4.kernel", !ex.nested);
    ex.for_points("ww_st4.spr4", npts, KOKKOS_LAMBDA(const int ip) {
      w3spr4_point(p, f, col(A, ip), col(CG, ip), col(WN, ip), U(0, ip), UD(0, ip), TX(0, ip),
                   TY(0, ip), col(L, ip), O0(0, ip), O1(0, ip), O2(0, ip), O3(0, ip), O4(0, ip),
                   O5(0, ip), O6(0, ip), O7(0, ip), O8(0, ip), O9(0, ip), O10(0, ip), O11(0, ip),
                   col(w, ip), col(iw, ip));
    });
    t_k.stop();
    ww::timers::Region t_out("st4.spr4.d2h", !ex.nested);
    for (int j = 0; j < 12; ++j) stage_out(ex, o[j], npts);
    t_out.stop();
    g_count_spr4.fetch_add(npts, std::memory_order_relaxed);
    ok();
  } catch (const std::bad_alloc& e) {
    set_error(WW_KOKKOS_ERR_RUNTIME, e.what());
  } catch (const std::exception& e) {
    set_error(WW_KOKKOS_ERR_KERNEL, e.what());
  } catch (...) {
    set_error(WW_KOKKOS_ERR_KERNEL, "ww_spr4: unknown exception");
  }
}

int ww_st4_enabled(int which) {
  const Flags& f = flags();
  switch (which) {
    case 1: return f.sds4 ? 1 : 0;
    case 2: return f.sin4 ? 1 : 0;
    case 3: return f.spr4 ? 1 : 0;
    case 4: return f.tables ? 1 : 0;
    default: return 0;
  }
}

int ww_st4_last_error(void) { return g_last_error.load(std::memory_order_relaxed); }

}  // extern "C"
